/*
 * Copyright 2015 Red Hat, Inc.
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
 *
 * Author: Matthias Clasen <mclasen@redhat.com>
 */

#include "config.h"

#include <gio/gio.h>

#if defined(G_OS_UNIX) && !defined(HAVE_COCOA)
#include <gio/gdesktopappinfo.h>
#endif

#include <gi18n.h>

#include "gio-tool.h"


static const GOptionEntry entries[] = {
  { NULL }
};

#if defined(G_OS_UNIX) && !defined(HAVE_COCOA)
static gboolean
get_bus_name_and_path_from_uri (const char *uri,
                                char **bus_name_out,
                                char **object_path_out)
{
  GAppInfo *app_info = NULL;
  char *bus_name = NULL;
  char *object_path = NULL;
  char *uri_scheme;
  const char *filename;
  char *basename = NULL;
  char *p;
  gboolean got_name = FALSE;

  uri_scheme = g_uri_parse_scheme (uri);
  if (uri_scheme && uri_scheme[0] != '\0')
    app_info = g_app_info_get_default_for_uri_scheme (uri_scheme);
  g_free (uri_scheme);

  if (app_info == NULL)
    {
      GFile *file;

      file = g_file_new_for_uri (uri);
      app_info = g_file_query_default_handler (file, NULL, NULL);
      g_object_unref (file);
    }

  if (app_info == NULL || !G_IS_DESKTOP_APP_INFO (app_info) ||
      !g_desktop_app_info_get_boolean (G_DESKTOP_APP_INFO (app_info), "DBusActivatable"))
    goto out;

  filename = g_desktop_app_info_get_filename (G_DESKTOP_APP_INFO (app_info));
  if (filename == NULL)
    goto out;

  basename = g_path_get_basename (filename);
  if (!g_str_has_suffix (basename, ".desktop"))
    goto out;

  basename[strlen (basename) - strlen (".desktop")] = '\0';
  if (!g_dbus_is_name (basename))
    goto out;

  bus_name = g_strdup (basename);
  object_path = g_strdup_printf ("/%s", bus_name);
  for (p = object_path; *p != '\0'; p++)
    if (*p == '.')
      *p = '/';

  *bus_name_out = g_steal_pointer (&bus_name);
  *object_path_out = g_steal_pointer (&object_path);
  got_name = TRUE;

out:
  g_clear_object (&app_info);
  g_clear_pointer (&basename, g_free);

  return got_name;
}
#endif

int
handle_open (int argc, char *argv[], gboolean do_help)
{
  GOptionContext *context;
  gchar *param;
  GError *error = NULL;
  int i;
  gboolean success;
  gboolean res;

  g_set_prgname ("gio open");

  /* Translators: commandline placeholder */
  param = g_strdup_printf ("%s...", _("LOCATION"));
  context = g_option_context_new (param);
  g_free (param);
  g_option_context_set_help_enabled (context, FALSE);
  g_option_context_set_summary (context,
      _("Open files with the default application that\n"
        "is registered to handle files of this type."));
  g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);

  if (do_help)
    {
      show_help (context, NULL);
      g_option_context_free (context);
      return 0;
    }

  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      show_help (context, error->message);
      g_error_free (error);
      g_option_context_free (context);
      return 1;
    }

  if (argc < 2)
    {
      show_help (context, _("No locations given"));
      g_option_context_free (context);
      return 1;
    }

  g_option_context_free (context);

  success = TRUE;
  for (i = 1; i < argc; i++)
    {
      char *uri = NULL;
      char *uri_scheme;

      /* Workaround to handle non-URI locations. We still use the original
       * location for other cases, because GFile might modify the URI in ways
       * we don't want. See:
       * https://bugzilla.gnome.org/show_bug.cgi?id=779182 */
      uri_scheme = g_uri_parse_scheme (argv[i]);
      if (!uri_scheme || uri_scheme[0] == '\0')
        {
          GFile *file;

          file = g_file_new_for_commandline_arg (argv[i]);
          uri = g_file_get_uri (file);
          g_object_unref (file);
        }
      g_free (uri_scheme);

      res = g_app_info_launch_default_for_uri (uri ? uri : argv[i], NULL, &error);
      if (!res)
	{
          print_error ("%s: %s", uri ? uri : argv[i], error->message);
	  g_clear_error (&error);
	  success = FALSE;
	}

#if defined(G_OS_UNIX) && !defined(HAVE_COCOA)
      /* FIXME: This chunk of madness is a workaround for a dbus-daemon bug.
       * See https://bugzilla.gnome.org/show_bug.cgi?id=780296
       */
      if (res)
        {
          char *bus_name = NULL;
          char *object_path = NULL;

          if (get_bus_name_and_path_from_uri (uri ? uri : argv[i], &bus_name, &object_path))
            {
              GDBusConnection *connection;
              connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);

              if (connection)
                g_dbus_connection_call_sync (connection,
                                             bus_name,
                                             object_path,
                                             "org.freedesktop.DBus.Peer",
                                             "Ping",
                                             NULL, NULL,
                                             G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL);
              g_clear_object (&connection);
              g_free (bus_name);
              g_free (object_path);
            }
        }
#endif

      g_free (uri);
    }

  return success ? 0 : 2;
}
