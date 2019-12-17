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


static gboolean force = FALSE;
static gboolean empty = FALSE;
static const GOptionEntry entries[] = {
  { "force", 'f', 0, G_OPTION_ARG_NONE, &force, N_("Ignore nonexistent files, never prompt"), NULL },
  { "empty", 0, 0, G_OPTION_ARG_NONE, &empty, N_("Empty the trash"), NULL },
  { NULL }
};

static void
delete_trash_file (GFile *file, gboolean del_file, gboolean del_children)
{
  GFileInfo *info;
  GFile *child;
  GFileEnumerator *enumerator;

  if (del_children)
    {
      enumerator = g_file_enumerate_children (file,
                                              G_FILE_ATTRIBUTE_STANDARD_NAME ","
                                              G_FILE_ATTRIBUTE_STANDARD_TYPE,
                                              G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                              NULL,
                                              NULL);
      if (enumerator)
        {
          while ((info = g_file_enumerator_next_file (enumerator, NULL, NULL)) != NULL)
            {
              child = g_file_get_child (file, g_file_info_get_name (info));
              delete_trash_file (child, TRUE, g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY);
              g_object_unref (child);
              g_object_unref (info);
            }
          g_file_enumerator_close (enumerator, NULL, NULL);
          g_object_unref (enumerator);
        }
    }

  if (del_file)
    g_file_delete (file, NULL, NULL);
}

int
handle_trash (int argc, char *argv[], gboolean do_help)
{
  GOptionContext *context;
  gchar *param;
  GError *error = NULL;
  int retval = 0;
  GFile *file;

  g_set_prgname ("gio trash");

  /* Translators: commandline placeholder */
  param = g_strdup_printf ("[%s...]", _("LOCATION"));
  context = g_option_context_new (param);
  g_free (param);
  g_option_context_set_help_enabled (context, FALSE);
  g_option_context_set_summary (context,
      _("Move files or directories to the trash."));
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

  g_option_context_free (context);

  if (argc > 1)
    {
      int i;

      for (i = 1; i < argc; i++)
        {
          file = g_file_new_for_commandline_arg (argv[i]);
          error = NULL;
          if (!g_file_trash (file, NULL, &error))
            {
              if (!force ||
                  !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
                {
                  print_file_error (file, error->message);
                  retval = 1;
                }
              g_error_free (error);
            }
          g_object_unref (file);
        }
    }

  if (empty)
    {
      GFile *file;
      file = g_file_new_for_uri ("trash:");
      delete_trash_file (file, FALSE, TRUE);
      g_object_unref (file);
    }

  return retval;
}
