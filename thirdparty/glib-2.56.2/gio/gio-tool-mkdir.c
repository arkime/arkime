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
#include <gi18n.h>

#include "gio-tool.h"


static gboolean parent = FALSE;

static const GOptionEntry entries[] = {
  { "parent", 'p', 0, G_OPTION_ARG_NONE, &parent, N_("Create parent directories"), NULL },
  { NULL }
};

int
handle_mkdir (int argc, char *argv[], gboolean do_help)
{
  GOptionContext *context;
  gchar *param;
  GError *error = NULL;
  GFile *file;
  int retval = 0;
  int i;

  g_set_prgname ("gio mkdir");

  /* Translators: commandline placeholder */
  param = g_strdup_printf ("%s...", _("LOCATION"));
  context = g_option_context_new (param);
  g_free (param);
  g_option_context_set_help_enabled (context, FALSE);
  g_option_context_set_summary (context, _("Create directories."));
  g_option_context_set_description (context,
      _("gio mkdir is similar to the traditional mkdir utility, but using GIO\n"
        "locations instead of local files: for example, you can use something\n"
        "like smb://server/resource/mydir as location."));
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

  for (i = 1; i < argc; i++)
    {
      file = g_file_new_for_commandline_arg (argv[i]);
      error = NULL;
      if (parent)
        {
          if (!g_file_make_directory_with_parents (file, NULL, &error))
            {
              print_file_error (file, error->message);
              g_error_free (error);
              retval = 1;
            }
        }
      else
        {
          if (!g_file_make_directory (file, NULL, &error))
            {
              print_file_error (file, error->message);
              g_error_free (error);
              retval = 1;
            }
          g_object_unref (file);
        }
    }

  return retval;

}

