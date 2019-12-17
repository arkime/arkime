/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright 2017 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#include "gopenuriportal.h"
#include "xdp-dbus.h"
#include "gstdio.h"

#ifdef G_OS_UNIX
#include "gunixfdlist.h"
#endif

#ifndef O_PATH
#define O_PATH 0
#endif
#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#else
#define HAVE_O_CLOEXEC 1
#endif


static GXdpOpenURI *openuri;

static gboolean
init_openuri_portal (void)
{
  static gsize openuri_inited = 0;

  if (g_once_init_enter (&openuri_inited))
    {
      GError *error = NULL;
      GDBusConnection *connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);

      if (connection != NULL)
        {
          openuri = gxdp_open_uri_proxy_new_sync (connection, 0,
                                                  "org.freedesktop.portal.Desktop",
                                                  "/org/freedesktop/portal/desktop",
                                                  NULL, &error);
          if (openuri == NULL)
            {
              g_warning ("Cannot create document portal proxy: %s", error->message);
              g_error_free (error);
            }

          g_object_unref (connection);
        }
      else
        {
          g_warning ("Cannot connect to session bus when initializing document portal: %s",
                     error->message);
          g_error_free (error);
        }

      g_once_init_leave (&openuri_inited, 1);
    }

  return openuri != NULL;
}

gboolean
g_openuri_portal_open_uri (const char  *uri,
                           const char  *parent_window,
                           GError     **error)
{
  GFile *file = NULL;
  GVariantBuilder opt_builder;
  gboolean res;

  if (!init_openuri_portal ())
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_INITIALIZED,
                   "OpenURI portal is not available");
      return FALSE;
    }

  g_variant_builder_init (&opt_builder, G_VARIANT_TYPE_VARDICT);

  file = g_file_new_for_uri (uri);
  if (g_file_is_native (file))
    {
      char *path = NULL;
      GUnixFDList *fd_list = NULL;
      int fd, fd_id, errsv;

      path = g_file_get_path (file);

      fd = g_open (path, O_PATH | O_CLOEXEC);
      errsv = errno;
      if (fd == -1)
        {
          g_set_error (error, G_IO_ERROR, g_io_error_from_errno (errsv),
                       "Failed to open '%s'", path);
          return FALSE;
        }

#ifndef HAVE_O_CLOEXEC
      fcntl (fd, F_SETFD, FD_CLOEXEC);
#endif
      fd_list = g_unix_fd_list_new_from_array (&fd, 1);
      fd = -1;
      fd_id = 0;

      res = gxdp_open_uri_call_open_file_sync (openuri,
                                               parent_window ? parent_window : "",
                                               g_variant_new ("h", fd_id),
                                               g_variant_builder_end (&opt_builder),
                                               fd_list,
                                               NULL,
                                               NULL,
                                               NULL,
                                               error);
      g_free (path);
      g_object_unref (fd_list);
    }
  else
    {
      res = gxdp_open_uri_call_open_uri_sync (openuri,
                                              parent_window ? parent_window : "",
                                              uri,
                                              g_variant_builder_end (&opt_builder),
                                              NULL,
                                              NULL,
                                              error);
    }

  g_object_unref (file);

  return res;
}

enum {
  XDG_DESKTOP_PORTAL_SUCCESS   = 0,
  XDG_DESKTOP_PORTAL_CANCELLED = 1,
  XDG_DESKTOP_PORTAL_FAILED    = 2
};

static void
response_received (GDBusConnection *connection,
                   const char      *sender_name,
                   const char      *object_path,
                   const char      *interface_name,
                   const char      *signal_name,
                   GVariant        *parameters,
                   gpointer         user_data)
{
  GTask *task = user_data;
  guint32 response;
  guint signal_id;

  signal_id = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (task), "signal-id"));
  g_dbus_connection_signal_unsubscribe (connection, signal_id);

  g_variant_get (parameters, "(u@a{sv})", &response, NULL);

  switch (response)
    {
    case XDG_DESKTOP_PORTAL_SUCCESS:
      g_task_return_boolean (task, TRUE);
      break;
    case XDG_DESKTOP_PORTAL_CANCELLED:
      g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_CANCELLED, "Launch cancelled");
      break;
    case XDG_DESKTOP_PORTAL_FAILED:
    default:
      g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_FAILED, "Launch failed");
      break;
    }

  g_object_unref (task);
}

static void
open_call_done (GObject      *source,
                GAsyncResult *result,
                gpointer      user_data)
{
  GXdpOpenURI *openuri = GXDP_OPEN_URI (source);
  GDBusConnection *connection;
  GTask *task = user_data;
  GError *error = NULL;
  gboolean open_file;
  gboolean res;
  char *path;
  const char *handle;
  guint signal_id;

  connection = g_dbus_proxy_get_connection (G_DBUS_PROXY (openuri));
  open_file = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (task), "open-file"));

  if (open_file)
    res = gxdp_open_uri_call_open_file_finish (openuri, &path, NULL, result, &error);
  else
    res = gxdp_open_uri_call_open_uri_finish (openuri, &path, result, &error);

  if (!res)
    {
      g_task_return_error (task, error);
      g_object_unref (task);
      g_free (path);
      return;
    }

  handle = (const char *)g_object_get_data (G_OBJECT (task), "handle");
  if (strcmp (handle, path) != 0)
    {
      signal_id = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (task), "signal-id"));
      g_dbus_connection_signal_unsubscribe (connection, signal_id);

      signal_id = g_dbus_connection_signal_subscribe (connection,
                                                      "org.freedesktop.portal.Desktop",
                                                      "org.freedesktop.portal.Request",
                                                      "Response",
                                                      path,
                                                      NULL,
                                                      G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
                                                      response_received,
                                                      task,
                                                      NULL);
      g_object_set_data (G_OBJECT (task), "signal-id", GINT_TO_POINTER (signal_id));
    }
}

void
g_openuri_portal_open_uri_async (const char          *uri,
                                 const char          *parent_window,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data)
{
  GDBusConnection *connection;
  GTask *task;
  GFile *file;
  GVariant *opts = NULL;
  int i;
  guint signal_id;

  if (!init_openuri_portal ())
    {
      g_task_report_new_error (NULL, callback, user_data, NULL,
                               G_IO_ERROR, G_IO_ERROR_NOT_INITIALIZED,
                               "OpenURI portal is not available");
      return;
    }

  connection = g_dbus_proxy_get_connection (G_DBUS_PROXY (openuri));

  if (callback)
    {
      GVariantBuilder opt_builder;
      char *token;
      char *sender;
      char *handle;

      task = g_task_new (NULL, cancellable, callback, user_data);

      token = g_strdup_printf ("gio%d", g_random_int_range (0, G_MAXINT));
      sender = g_strdup (g_dbus_connection_get_unique_name (connection) + 1);
      for (i = 0; sender[i]; i++)
        if (sender[i] == '.')
          sender[i] = '_';

      handle = g_strdup_printf ("/org/fredesktop/portal/desktop/request/%s/%s", sender, token);
      g_object_set_data_full (G_OBJECT (task), "handle", handle, g_free);
      g_free (sender);

      signal_id = g_dbus_connection_signal_subscribe (connection,
                                                      "org.freedesktop.portal.Desktop",
                                                      "org.freedesktop.portal.Request",
                                                      "Response",
                                                      handle,
                                                      NULL,
                                                      G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
                                                      response_received,
                                                      task,
                                                      NULL);
      g_object_set_data (G_OBJECT (task), "signal-id", GINT_TO_POINTER (signal_id));

      g_variant_builder_init (&opt_builder, G_VARIANT_TYPE_VARDICT);
      g_variant_builder_add (&opt_builder, "{sv}", "handle_token", g_variant_new_string (token));
      g_free (token);

      opts = g_variant_builder_end (&opt_builder);
    }
  else
    task = NULL;

  file = g_file_new_for_uri (uri);
  if (g_file_is_native (file))
    {
      char *path = NULL;
      GUnixFDList *fd_list = NULL;
      int fd, fd_id, errsv;

      if (task)
        g_object_set_data (G_OBJECT (task), "open-file", GINT_TO_POINTER (TRUE));

      path = g_file_get_path (file);
      fd = g_open (path, O_PATH | O_CLOEXEC);
      errsv = errno;
      if (fd == -1)
        {
          g_task_report_new_error (NULL, callback, user_data, NULL,
                                   G_IO_ERROR, g_io_error_from_errno (errsv),
                                   "OpenURI portal is not available");
          return;
        }

#ifndef HAVE_O_CLOEXEC
      fcntl (fd, F_SETFD, FD_CLOEXEC);
#endif
      fd_list = g_unix_fd_list_new_from_array (&fd, 1);
      fd = -1;
      fd_id = 0;

      gxdp_open_uri_call_open_file (openuri,
                                    parent_window ? parent_window : "",
                                    g_variant_new ("h", fd_id),
                                    opts,
                                    fd_list,
                                    cancellable,
                                    task ? open_call_done : NULL,
                                    task);
      g_object_unref (fd_list);
      g_free (path);
    }
  else
    {
      gxdp_open_uri_call_open_uri (openuri,
                                   parent_window ? parent_window : "",
                                   uri,
                                   opts,
                                   cancellable,
                                   task ? open_call_done : NULL,
                                   task);
    }

  g_object_unref (file);
}

gboolean
g_openuri_portal_open_uri_finish (GAsyncResult  *result,
                                  GError       **error)
{
  return g_task_propagate_boolean (G_TASK (result), error);
}
