/*
 * Copyright © 2010 Codethink Limited
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
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Ryan Lortie <desrt@desrt.ca>
 */

#include "config.h"

#include "gapplicationimpl.h"

#include "gactiongroup.h"
#include "gactiongroupexporter.h"
#include "gremoteactiongroup.h"
#include "gdbusactiongroup-private.h"
#include "gapplication.h"
#include "gfile.h"
#include "gdbusconnection.h"
#include "gdbusintrospection.h"
#include "gdbuserror.h"
#include "glib/gstdio.h"

#include <string.h>
#include <stdio.h>

#include "gapplicationcommandline.h"
#include "gdbusmethodinvocation.h"

#ifdef G_OS_UNIX
#include "gunixinputstream.h"
#include "gunixfdlist.h"
#endif

/* DBus Interface definition {{{1 */

/* For documentation of these interfaces, see
 * https://wiki.gnome.org/Projects/GLib/GApplication/DBusAPI
 */
static const gchar org_gtk_Application_xml[] =
  "<node>"
    "<interface name='org.gtk.Application'>"
      "<method name='Activate'>"
        "<arg type='a{sv}' name='platform-data' direction='in'/>"
      "</method>"
      "<method name='Open'>"
        "<arg type='as' name='uris' direction='in'/>"
        "<arg type='s' name='hint' direction='in'/>"
        "<arg type='a{sv}' name='platform-data' direction='in'/>"
      "</method>"
      "<method name='CommandLine'>"
        "<arg type='o' name='path' direction='in'/>"
        "<arg type='aay' name='arguments' direction='in'/>"
        "<arg type='a{sv}' name='platform-data' direction='in'/>"
        "<arg type='i' name='exit-status' direction='out'/>"
      "</method>"
    "<property name='Busy' type='b' access='read'/>"
    "</interface>"
  "</node>";

static GDBusInterfaceInfo *org_gtk_Application;

static const gchar org_freedesktop_Application_xml[] =
  "<node>"
    "<interface name='org.freedesktop.Application'>"
      "<method name='Activate'>"
        "<arg type='a{sv}' name='platform-data' direction='in'/>"
      "</method>"
      "<method name='Open'>"
        "<arg type='as' name='uris' direction='in'/>"
        "<arg type='a{sv}' name='platform-data' direction='in'/>"
      "</method>"
      "<method name='ActivateAction'>"
        "<arg type='s' name='action-name' direction='in'/>"
        "<arg type='av' name='parameter' direction='in'/>"
        "<arg type='a{sv}' name='platform-data' direction='in'/>"
      "</method>"
    "</interface>"
  "</node>";

static GDBusInterfaceInfo *org_freedesktop_Application;

static const gchar org_gtk_private_CommandLine_xml[] =
  "<node>"
    "<interface name='org.gtk.private.CommandLine'>"
      "<method name='Print'>"
        "<arg type='s' name='message' direction='in'/>"
      "</method>"
      "<method name='PrintError'>"
        "<arg type='s' name='message' direction='in'/>"
      "</method>"
    "</interface>"
  "</node>";

static GDBusInterfaceInfo *org_gtk_private_CommandLine;

/* GApplication implementation {{{1 */
struct _GApplicationImpl
{
  GDBusConnection *session_bus;
  GActionGroup    *exported_actions;
  const gchar     *bus_name;

  gchar           *object_path;
  guint            object_id;
  guint            fdo_object_id;
  guint            actions_id;

  gboolean         properties_live;
  gboolean         primary;
  gboolean         busy;
  gboolean         registered;
  GApplication    *app;
};


static GApplicationCommandLine *
g_dbus_command_line_new (GDBusMethodInvocation *invocation);

static GVariant *
g_application_impl_get_property (GDBusConnection *connection,
                                 const gchar  *sender,
                                 const gchar  *object_path,
                                 const gchar  *interface_name,
                                 const gchar  *property_name,
                                 GError      **error,
                                 gpointer      user_data)
{
  GApplicationImpl *impl = user_data;

  if (strcmp (property_name, "Busy") == 0)
    return g_variant_new_boolean (impl->busy);

  g_assert_not_reached ();

  return NULL;
}

static void
send_property_change (GApplicationImpl *impl)
{
  GVariantBuilder builder;

  g_variant_builder_init (&builder, G_VARIANT_TYPE_ARRAY);
  g_variant_builder_add (&builder,
                         "{sv}",
                         "Busy", g_variant_new_boolean (impl->busy));

  g_dbus_connection_emit_signal (impl->session_bus,
                                 NULL,
                                 impl->object_path,
                                 "org.freedesktop.DBus.Properties",
                                 "PropertiesChanged",
                                 g_variant_new ("(sa{sv}as)",
                                                "org.gtk.Application",
                                                &builder,
                                                NULL),
                                 NULL);
}

static void
g_application_impl_method_call (GDBusConnection       *connection,
                                const gchar           *sender,
                                const gchar           *object_path,
                                const gchar           *interface_name,
                                const gchar           *method_name,
                                GVariant              *parameters,
                                GDBusMethodInvocation *invocation,
                                gpointer               user_data)
{
  GApplicationImpl *impl = user_data;
  GApplicationClass *class;

  class = G_APPLICATION_GET_CLASS (impl->app);

  if (strcmp (method_name, "Activate") == 0)
    {
      GVariant *platform_data;

      /* Completely the same for both freedesktop and gtk interfaces */

      g_variant_get (parameters, "(@a{sv})", &platform_data);

      class->before_emit (impl->app, platform_data);
      g_signal_emit_by_name (impl->app, "activate");
      class->after_emit (impl->app, platform_data);
      g_variant_unref (platform_data);

      g_dbus_method_invocation_return_value (invocation, NULL);
    }

  else if (strcmp (method_name, "Open") == 0)
    {
      GApplicationFlags flags;
      GVariant *platform_data;
      const gchar *hint;
      GVariant *array;
      GFile **files;
      gint n, i;

      flags = g_application_get_flags (impl->app);
      if ((flags & G_APPLICATION_HANDLES_OPEN) == 0)
        {
          g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR, G_DBUS_ERROR_NOT_SUPPORTED, "Application does not open files");
          return;
        }

      /* freedesktop interface has no hint parameter */
      if (g_str_equal (interface_name, "org.freedesktop.Application"))
        {
          g_variant_get (parameters, "(@as@a{sv})", &array, &platform_data);
          hint = "";
        }
      else
        g_variant_get (parameters, "(@as&s@a{sv})", &array, &hint, &platform_data);

      n = g_variant_n_children (array);
      files = g_new (GFile *, n + 1);

      for (i = 0; i < n; i++)
        {
          const gchar *uri;

          g_variant_get_child (array, i, "&s", &uri);
          files[i] = g_file_new_for_uri (uri);
        }
      g_variant_unref (array);
      files[n] = NULL;

      class->before_emit (impl->app, platform_data);
      g_signal_emit_by_name (impl->app, "open", files, n, hint);
      class->after_emit (impl->app, platform_data);

      g_variant_unref (platform_data);

      for (i = 0; i < n; i++)
        g_object_unref (files[i]);
      g_free (files);

      g_dbus_method_invocation_return_value (invocation, NULL);
    }

  else if (strcmp (method_name, "CommandLine") == 0)
    {
      GApplicationFlags flags;
      GApplicationCommandLine *cmdline;
      GVariant *platform_data;
      int status;

      flags = g_application_get_flags (impl->app);
      if ((flags & G_APPLICATION_HANDLES_COMMAND_LINE) == 0)
        {
          g_dbus_method_invocation_return_error (invocation, G_DBUS_ERROR, G_DBUS_ERROR_NOT_SUPPORTED,
                                                 "Application does not handle command line arguments");
          return;
        }

      /* Only on the GtkApplication interface */

      cmdline = g_dbus_command_line_new (invocation);
      platform_data = g_variant_get_child_value (parameters, 2);
      class->before_emit (impl->app, platform_data);
      g_signal_emit_by_name (impl->app, "command-line", cmdline, &status);
      g_application_command_line_set_exit_status (cmdline, status);
      class->after_emit (impl->app, platform_data);
      g_variant_unref (platform_data);
      g_object_unref (cmdline);
    }
  else if (g_str_equal (method_name, "ActivateAction"))
    {
      GVariant *parameter = NULL;
      GVariant *platform_data;
      GVariantIter *iter;
      const gchar *name;

      /* Only on the freedesktop interface */

      g_variant_get (parameters, "(&sav@a{sv})", &name, &iter, &platform_data);
      g_variant_iter_next (iter, "v", &parameter);
      g_variant_iter_free (iter);

      class->before_emit (impl->app, platform_data);
      g_action_group_activate_action (impl->exported_actions, name, parameter);
      class->after_emit (impl->app, platform_data);

      if (parameter)
        g_variant_unref (parameter);

      g_variant_unref (platform_data);

      g_dbus_method_invocation_return_value (invocation, NULL);
    }
  else
    g_assert_not_reached ();
}

static gchar *
application_path_from_appid (const gchar *appid)
{
  gchar *appid_path, *iter;

  if (appid == NULL)
    /* this is a private implementation detail */
    return g_strdup ("/org/gtk/Application/anonymous");

  appid_path = g_strconcat ("/", appid, NULL);
  for (iter = appid_path; *iter; iter++)
    {
      if (*iter == '.')
        *iter = '/';

      if (*iter == '-')
        *iter = '_';
    }

  return appid_path;
}

/* Attempt to become the primary instance.
 *
 * Returns %TRUE if everything went OK, regardless of if we became the
 * primary instance or not.  %FALSE is reserved for when something went
 * seriously wrong (and @error will be set too, in that case).
 *
 * After a %TRUE return, impl->primary will be TRUE if we were
 * successful.
 */
static gboolean
g_application_impl_attempt_primary (GApplicationImpl  *impl,
                                    GCancellable      *cancellable,
                                    GError           **error)
{
  const static GDBusInterfaceVTable vtable = {
    g_application_impl_method_call,
    g_application_impl_get_property,
    NULL /* set_property */
  };
  GApplicationClass *app_class = G_APPLICATION_GET_CLASS (impl->app);
  GVariant *reply;
  guint32 rval;

  if (org_gtk_Application == NULL)
    {
      GError *error = NULL;
      GDBusNodeInfo *info;

      info = g_dbus_node_info_new_for_xml (org_gtk_Application_xml, &error);
      if G_UNLIKELY (info == NULL)
        g_error ("%s", error->message);
      org_gtk_Application = g_dbus_node_info_lookup_interface (info, "org.gtk.Application");
      g_assert (org_gtk_Application != NULL);
      g_dbus_interface_info_ref (org_gtk_Application);
      g_dbus_node_info_unref (info);

      info = g_dbus_node_info_new_for_xml (org_freedesktop_Application_xml, &error);
      if G_UNLIKELY (info == NULL)
        g_error ("%s", error->message);
      org_freedesktop_Application = g_dbus_node_info_lookup_interface (info, "org.freedesktop.Application");
      g_assert (org_freedesktop_Application != NULL);
      g_dbus_interface_info_ref (org_freedesktop_Application);
      g_dbus_node_info_unref (info);
    }

  /* We could possibly have been D-Bus activated as a result of incoming
   * requests on either the application or actiongroup interfaces.
   * Because of how GDBus dispatches messages, we need to ensure that
   * both of those things are registered before we attempt to request
   * our name.
   *
   * The action group need not be populated yet, as long as it happens
   * before we return to the mainloop.  The reason for that is because
   * GDBus does the check to make sure the object exists from the worker
   * thread but doesn't actually dispatch the action invocation until we
   * hit the mainloop in this thread.  There is also no danger of
   * receiving 'activate' or 'open' signals until after 'startup' runs,
   * for the same reason.
   */
  impl->object_id = g_dbus_connection_register_object (impl->session_bus, impl->object_path,
                                                       org_gtk_Application, &vtable, impl, NULL, error);

  if (impl->object_id == 0)
    return FALSE;

  impl->fdo_object_id = g_dbus_connection_register_object (impl->session_bus, impl->object_path,
                                                           org_freedesktop_Application, &vtable, impl, NULL, error);

  if (impl->fdo_object_id == 0)
    return FALSE;

  impl->actions_id = g_dbus_connection_export_action_group (impl->session_bus, impl->object_path,
                                                            impl->exported_actions, error);

  if (impl->actions_id == 0)
    return FALSE;

  impl->registered = TRUE;
  if (!app_class->dbus_register (impl->app,
                                 impl->session_bus,
                                 impl->object_path,
                                 error))
    return FALSE;

  if (impl->bus_name == NULL)
    {
      /* If this is a non-unique application then it is sufficient to
       * have our object paths registered. We can return now.
       *
       * Note: non-unique applications always act as primary-instance.
       */
      impl->primary = TRUE;
      return TRUE;
    }

  /* If this is a unique application then we need to attempt to own
   * the well-known name and fall back to remote mode (!is_primary)
   * in the case that we can't do that.
   */
  /* DBUS_NAME_FLAG_DO_NOT_QUEUE: 0x4 */
  reply = g_dbus_connection_call_sync (impl->session_bus, "org.freedesktop.DBus", "/org/freedesktop/DBus",
                                       "org.freedesktop.DBus", "RequestName",
                                       g_variant_new ("(su)", impl->bus_name, 0x4), G_VARIANT_TYPE ("(u)"),
                                       0, -1, cancellable, error);

  if (reply == NULL)
    return FALSE;

  g_variant_get (reply, "(u)", &rval);
  g_variant_unref (reply);

  /* DBUS_REQUEST_NAME_REPLY_EXISTS: 3 */
  impl->primary = (rval != 3);

  return TRUE;
}

/* Stop doing the things that the primary instance does.
 *
 * This should be called if attempting to become the primary instance
 * failed (in order to clean up any partial success) and should also
 * be called when freeing the GApplication.
 *
 * It is safe to call this multiple times.
 */
static void
g_application_impl_stop_primary (GApplicationImpl *impl)
{
  GApplicationClass *app_class = G_APPLICATION_GET_CLASS (impl->app);

  if (impl->registered)
    {
      app_class->dbus_unregister (impl->app,
                                  impl->session_bus,
                                  impl->object_path);
      impl->registered = FALSE;
    }

  if (impl->object_id)
    {
      g_dbus_connection_unregister_object (impl->session_bus, impl->object_id);
      impl->object_id = 0;
    }

  if (impl->fdo_object_id)
    {
      g_dbus_connection_unregister_object (impl->session_bus, impl->fdo_object_id);
      impl->fdo_object_id = 0;
    }

  if (impl->actions_id)
    {
      g_dbus_connection_unexport_action_group (impl->session_bus, impl->actions_id);
      impl->actions_id = 0;
    }

  if (impl->primary && impl->bus_name)
    {
      g_dbus_connection_call (impl->session_bus, "org.freedesktop.DBus",
                              "/org/freedesktop/DBus", "org.freedesktop.DBus",
                              "ReleaseName", g_variant_new ("(s)", impl->bus_name),
                              NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);
      impl->primary = FALSE;
    }
}

void
g_application_impl_set_busy_state (GApplicationImpl *impl,
                                   gboolean          busy)
{
  if (impl->busy != busy)
    {
      impl->busy = busy;
      send_property_change (impl);
    }
}

void
g_application_impl_destroy (GApplicationImpl *impl)
{
  g_application_impl_stop_primary (impl);

  if (impl->session_bus)
    g_object_unref (impl->session_bus);

  g_free (impl->object_path);

  g_slice_free (GApplicationImpl, impl);
}

GApplicationImpl *
g_application_impl_register (GApplication        *application,
                             const gchar         *appid,
                             GApplicationFlags    flags,
                             GActionGroup        *exported_actions,
                             GRemoteActionGroup **remote_actions,
                             GCancellable        *cancellable,
                             GError             **error)
{
  GDBusActionGroup *actions;
  GApplicationImpl *impl;

  g_assert ((flags & G_APPLICATION_NON_UNIQUE) || appid != NULL);

  impl = g_slice_new0 (GApplicationImpl);

  impl->app = application;
  impl->exported_actions = exported_actions;

  /* non-unique applications do not attempt to acquire a bus name */
  if (~flags & G_APPLICATION_NON_UNIQUE)
    impl->bus_name = appid;

  impl->session_bus = g_bus_get_sync (G_BUS_TYPE_SESSION, cancellable, NULL);

  if (impl->session_bus == NULL)
    {
      /* If we can't connect to the session bus, proceed as a normal
       * non-unique application.
       */
      *remote_actions = NULL;
      return impl;
    }

  impl->object_path = application_path_from_appid (appid);

  /* Only try to be the primary instance if
   * G_APPLICATION_IS_LAUNCHER was not specified.
   */
  if (~flags & G_APPLICATION_IS_LAUNCHER)
    {
      if (!g_application_impl_attempt_primary (impl, cancellable, error))
        {
          g_application_impl_destroy (impl);
          return NULL;
        }

      if (impl->primary)
        return impl;

      /* We didn't make it.  Drop our service-side stuff. */
      g_application_impl_stop_primary (impl);

      if (flags & G_APPLICATION_IS_SERVICE)
        {
          g_set_error (error, G_DBUS_ERROR, G_DBUS_ERROR_FAILED,
                       "Unable to acquire bus name '%s'", appid);
          g_application_impl_destroy (impl);

          return NULL;
        }
    }

  /* We are non-primary.  Try to get the primary's list of actions.
   * This also serves as a mechanism to ensure that the primary exists
   * (ie: DBus service files installed correctly, etc).
   */
  actions = g_dbus_action_group_get (impl->session_bus, impl->bus_name, impl->object_path);
  if (!g_dbus_action_group_sync (actions, cancellable, error))
    {
      /* The primary appears not to exist.  Fail the registration. */
      g_application_impl_destroy (impl);
      g_object_unref (actions);

      return NULL;
    }

  *remote_actions = G_REMOTE_ACTION_GROUP (actions);

  return impl;
}

void
g_application_impl_activate (GApplicationImpl *impl,
                             GVariant         *platform_data)
{
  g_dbus_connection_call (impl->session_bus,
                          impl->bus_name,
                          impl->object_path,
                          "org.gtk.Application",
                          "Activate",
                          g_variant_new ("(@a{sv})", platform_data),
                          NULL, 0, -1, NULL, NULL, NULL);
}

void
g_application_impl_open (GApplicationImpl  *impl,
                         GFile            **files,
                         gint               n_files,
                         const gchar       *hint,
                         GVariant          *platform_data)
{
  GVariantBuilder builder;
  gint i;

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("(assa{sv})"));
  g_variant_builder_open (&builder, G_VARIANT_TYPE_STRING_ARRAY);
  for (i = 0; i < n_files; i++)
    {
      gchar *uri = g_file_get_uri (files[i]);
      g_variant_builder_add (&builder, "s", uri);
      g_free (uri);
    }
  g_variant_builder_close (&builder);
  g_variant_builder_add (&builder, "s", hint);
  g_variant_builder_add_value (&builder, platform_data);

  g_dbus_connection_call (impl->session_bus,
                          impl->bus_name,
                          impl->object_path,
                          "org.gtk.Application",
                          "Open",
                          g_variant_builder_end (&builder),
                          NULL, 0, -1, NULL, NULL, NULL);
}

static void
g_application_impl_cmdline_method_call (GDBusConnection       *connection,
                                        const gchar           *sender,
                                        const gchar           *object_path,
                                        const gchar           *interface_name,
                                        const gchar           *method_name,
                                        GVariant              *parameters,
                                        GDBusMethodInvocation *invocation,
                                        gpointer               user_data)
{
  const gchar *message;

  g_variant_get_child (parameters, 0, "&s", &message);

  if (strcmp (method_name, "Print") == 0)
    g_print ("%s", message);
  else if (strcmp (method_name, "PrintError") == 0)
    g_printerr ("%s", message);
  else
    g_assert_not_reached ();

  g_dbus_method_invocation_return_value (invocation, NULL);
}

typedef struct
{
  GMainLoop *loop;
  int status;
} CommandLineData;

static void
g_application_impl_cmdline_done (GObject      *source,
                                 GAsyncResult *result,
                                 gpointer      user_data)
{
  CommandLineData *data = user_data;
  GError *error = NULL;
  GVariant *reply;

#ifdef G_OS_UNIX
  reply = g_dbus_connection_call_with_unix_fd_list_finish (G_DBUS_CONNECTION (source), NULL, result, &error);
#else
  reply = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source), result, &error);
#endif


  if (reply != NULL)
    {
      g_variant_get (reply, "(i)", &data->status);
      g_variant_unref (reply);
    }

  else
    {
      g_printerr ("%s\n", error->message);
      g_error_free (error);
      data->status = 1;
    }

  g_main_loop_quit (data->loop);
}

int
g_application_impl_command_line (GApplicationImpl    *impl,
                                 const gchar * const *arguments,
                                 GVariant            *platform_data)
{
  const static GDBusInterfaceVTable vtable = {
    g_application_impl_cmdline_method_call
  };
  const gchar *object_path = "/org/gtk/Application/CommandLine";
  GMainContext *context;
  CommandLineData data;
  guint object_id;

  context = g_main_context_new ();
  data.loop = g_main_loop_new (context, FALSE);
  g_main_context_push_thread_default (context);

  if (org_gtk_private_CommandLine == NULL)
    {
      GError *error = NULL;
      GDBusNodeInfo *info;

      info = g_dbus_node_info_new_for_xml (org_gtk_private_CommandLine_xml, &error);
      if G_UNLIKELY (info == NULL)
        g_error ("%s", error->message);
      org_gtk_private_CommandLine = g_dbus_node_info_lookup_interface (info, "org.gtk.private.CommandLine");
      g_assert (org_gtk_private_CommandLine != NULL);
      g_dbus_interface_info_ref (org_gtk_private_CommandLine);
      g_dbus_node_info_unref (info);
    }

  object_id = g_dbus_connection_register_object (impl->session_bus, object_path,
                                                 org_gtk_private_CommandLine,
                                                 &vtable, &data, NULL, NULL);
  /* In theory we should try other paths... */
  g_assert (object_id != 0);

#ifdef G_OS_UNIX
  {
    GError *error = NULL;
    GUnixFDList *fd_list;

    /* send along the stdin in case
     * g_application_command_line_get_stdin_data() is called
     */
    fd_list = g_unix_fd_list_new ();
    g_unix_fd_list_append (fd_list, 0, &error);
    g_assert_no_error (error);

    g_dbus_connection_call_with_unix_fd_list (impl->session_bus, impl->bus_name, impl->object_path,
                                              "org.gtk.Application", "CommandLine",
                                              g_variant_new ("(o^aay@a{sv})", object_path, arguments, platform_data),
                                              G_VARIANT_TYPE ("(i)"), 0, G_MAXINT, fd_list, NULL,
                                              g_application_impl_cmdline_done, &data);
    g_object_unref (fd_list);
  }
#else
  g_dbus_connection_call (impl->session_bus, impl->bus_name, impl->object_path,
                          "org.gtk.Application", "CommandLine",
                          g_variant_new ("(o^aay@a{sv})", object_path, arguments, platform_data),
                          G_VARIANT_TYPE ("(i)"), 0, G_MAXINT, NULL,
                          g_application_impl_cmdline_done, &data);
#endif

  g_main_loop_run (data.loop);

  g_main_context_pop_thread_default (context);
  g_main_context_unref (context);
  g_main_loop_unref (data.loop);

  return data.status;
}

void
g_application_impl_flush (GApplicationImpl *impl)
{
  if (impl->session_bus)
    g_dbus_connection_flush_sync (impl->session_bus, NULL, NULL);
}

GDBusConnection *
g_application_impl_get_dbus_connection (GApplicationImpl *impl)
{
  return impl->session_bus;
}

const gchar *
g_application_impl_get_dbus_object_path (GApplicationImpl *impl)
{
  return impl->object_path;
}

/* GDBusCommandLine implementation {{{1 */

typedef GApplicationCommandLineClass GDBusCommandLineClass;
static GType g_dbus_command_line_get_type (void);
typedef struct
{
  GApplicationCommandLine  parent_instance;
  GDBusMethodInvocation   *invocation;

  GDBusConnection *connection;
  const gchar     *bus_name;
  const gchar     *object_path;
} GDBusCommandLine;


G_DEFINE_TYPE (GDBusCommandLine,
               g_dbus_command_line,
               G_TYPE_APPLICATION_COMMAND_LINE)

static void
g_dbus_command_line_print_literal (GApplicationCommandLine *cmdline,
                                   const gchar             *message)
{
  GDBusCommandLine *gdbcl = (GDBusCommandLine *) cmdline;

  g_dbus_connection_call (gdbcl->connection,
                          gdbcl->bus_name,
                          gdbcl->object_path,
                          "org.gtk.private.CommandLine", "Print",
                          g_variant_new ("(s)", message),
                          NULL, 0, -1, NULL, NULL, NULL);
}

static void
g_dbus_command_line_printerr_literal (GApplicationCommandLine *cmdline,
                                      const gchar             *message)
{
  GDBusCommandLine *gdbcl = (GDBusCommandLine *) cmdline;

  g_dbus_connection_call (gdbcl->connection,
                          gdbcl->bus_name,
                          gdbcl->object_path,
                          "org.gtk.private.CommandLine", "PrintError",
                          g_variant_new ("(s)", message),
                          NULL, 0, -1, NULL, NULL, NULL);
}

static GInputStream *
g_dbus_command_line_get_stdin (GApplicationCommandLine *cmdline)
{
#ifdef G_OS_UNIX
  GDBusCommandLine *gdbcl = (GDBusCommandLine *) cmdline;
  GInputStream *result = NULL;
  GDBusMessage *message;
  GUnixFDList *fd_list;

  message = g_dbus_method_invocation_get_message (gdbcl->invocation);
  fd_list = g_dbus_message_get_unix_fd_list (message);

  if (fd_list && g_unix_fd_list_get_length (fd_list))
    {
      gint *fds, n_fds, i;

      fds = g_unix_fd_list_steal_fds (fd_list, &n_fds);
      result = g_unix_input_stream_new (fds[0], TRUE);
      for (i = 1; i < n_fds; i++)
        (void) g_close (fds[i], NULL);
      g_free (fds);
    }

  return result;
#else
  return NULL;
#endif
}

static void
g_dbus_command_line_finalize (GObject *object)
{
  GApplicationCommandLine *cmdline = G_APPLICATION_COMMAND_LINE (object);
  GDBusCommandLine *gdbcl = (GDBusCommandLine *) object;
  gint status;

  status = g_application_command_line_get_exit_status (cmdline);

  g_dbus_method_invocation_return_value (gdbcl->invocation,
                                         g_variant_new ("(i)", status));
  g_object_unref (gdbcl->invocation);

  G_OBJECT_CLASS (g_dbus_command_line_parent_class)
    ->finalize (object);
}

static void
g_dbus_command_line_init (GDBusCommandLine *gdbcl)
{
}

static void
g_dbus_command_line_class_init (GApplicationCommandLineClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = g_dbus_command_line_finalize;
  class->printerr_literal = g_dbus_command_line_printerr_literal;
  class->print_literal = g_dbus_command_line_print_literal;
  class->get_stdin = g_dbus_command_line_get_stdin;
}

static GApplicationCommandLine *
g_dbus_command_line_new (GDBusMethodInvocation *invocation)
{
  GDBusCommandLine *gdbcl;
  GVariant *args;
  GVariant *arguments, *platform_data;

  args = g_dbus_method_invocation_get_parameters (invocation);

  arguments = g_variant_get_child_value (args, 1);
  platform_data = g_variant_get_child_value (args, 2);
  gdbcl = g_object_new (g_dbus_command_line_get_type (),
                        "arguments", arguments,
                        "platform-data", platform_data,
                        NULL);
  g_variant_unref (arguments);
  g_variant_unref (platform_data);

  gdbcl->connection = g_dbus_method_invocation_get_connection (invocation);
  gdbcl->bus_name = g_dbus_method_invocation_get_sender (invocation);
  g_variant_get_child (args, 0, "&o", &gdbcl->object_path);
  gdbcl->invocation = g_object_ref (invocation);

  return G_APPLICATION_COMMAND_LINE (gdbcl);
}

/* Epilogue {{{1 */

/* vim:set foldmethod=marker: */
