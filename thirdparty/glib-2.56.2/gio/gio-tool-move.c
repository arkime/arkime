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
#include <stdio.h>

#include "gio-tool.h"


/* statics {{{1 */

static gboolean no_target_directory = FALSE;
static gboolean progress = FALSE;
static gboolean interactive = FALSE;
static gboolean backup = FALSE;
static gboolean no_copy_fallback = FALSE;

static const GOptionEntry entries[] = {
  { "no-target-directory", 'T', 0, G_OPTION_ARG_NONE, &no_target_directory, N_("No target directory"), NULL },
  { "progress", 'p', 0, G_OPTION_ARG_NONE, &progress, N_("Show progress"), NULL },
  { "interactive", 'i', 0, G_OPTION_ARG_NONE, &interactive, N_("Prompt before overwrite"), NULL },
  { "backup", 'b', 0, G_OPTION_ARG_NONE, &backup, N_("Backup existing destination files"), NULL },
  { "no-copy-fallback", 'C', 0, G_OPTION_ARG_NONE, &no_copy_fallback, N_("Don’t use copy and delete fallback"), NULL },
  { NULL }
};

static gint64 start_time;
static gint64 previous_time;

static void
show_progress (goffset current_num_bytes,
               goffset total_num_bytes,
               gpointer user_data)
{
  gint64 tv;
  char *current_size, *total_size, *rate;

  tv = g_get_monotonic_time ();
  if (tv - previous_time < (G_USEC_PER_SEC / 5) &&
      current_num_bytes != total_num_bytes)
    return;

  current_size = g_format_size (current_num_bytes);
  total_size = g_format_size (total_num_bytes);
  rate = g_format_size (current_num_bytes /
                        MAX ((tv - start_time) / G_USEC_PER_SEC, 1));
  g_print ("\r\033[K");
  g_print (_("Transferred %s out of %s (%s/s)"),
           current_size, total_size, rate);

  previous_time = tv;

  g_free (current_size);
  g_free (total_size);
  g_free (rate);
}

int
handle_move (int argc, char *argv[], gboolean do_help)
{
  GOptionContext *context;
  gchar *param;
  GError *error = NULL;
  GFile *source, *dest, *target;
  gboolean dest_is_dir;
  char *basename;
  char *uri;
  int i;
  GFileCopyFlags flags;
  int retval = 0;

  g_set_prgname ("gio move");

  /* Translators: commandline placeholder */
  param = g_strdup_printf ("%s... %s", _("SOURCE"), _("DESTINATION"));
  context = g_option_context_new (param);
  g_free (param);
  g_option_context_set_help_enabled (context, FALSE);
  g_option_context_set_summary (context,
      _("Move one or more files from SOURCE to DEST."));
  g_option_context_set_description (context,
      _("gio move is similar to the traditional mv utility, but using GIO\n"
        "locations instead of local files: for example, you can use something\n"
        "like smb://server/resource/file.txt as location"));
  g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);

  if (do_help)
    {
      show_help (context, NULL);
      return 0;
    }

  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      show_help (context, error->message);
      g_error_free (error);
      g_option_context_free (context);
      return 1;
    }

  if (argc < 3)
    {
      show_help (context, NULL);
      g_option_context_free (context);
      return 1;
    }

  dest = g_file_new_for_commandline_arg (argv[argc - 1]);

  if (no_target_directory && argc > 3)
    {
      show_help (context, NULL);
      g_object_unref (dest);
      g_option_context_free (context);
      return 1;
    }

  dest_is_dir = file_is_dir (dest);

  if (!dest_is_dir && argc > 3)
    {
      char *message;
      message = g_strdup_printf (_("Target %s is not a directory"), argv[argc - 1]);
      show_help (context, message);
      g_free (message);
      g_object_unref (dest);
      g_option_context_free (context);
      return 1;
    }

  g_option_context_free (context);

  for (i = 1; i < argc - 1; i++)
    {
      source = g_file_new_for_commandline_arg (argv[i]);

      if (dest_is_dir && !no_target_directory)
        {
          basename = g_file_get_basename (source);
          target = g_file_get_child (dest, basename);
          g_free (basename);
        }
      else
        target = g_object_ref (dest);

      flags = 0;
      if (backup)
        flags |= G_FILE_COPY_BACKUP;
      if (!interactive)
        flags |= G_FILE_COPY_OVERWRITE;
      if (no_copy_fallback)
        flags |= G_FILE_COPY_NO_FALLBACK_FOR_MOVE;

      error = NULL;
      start_time = g_get_monotonic_time ();
      if (!g_file_move (source, target, flags, NULL, progress ? show_progress : NULL, NULL, &error))
        {
          if (interactive && g_error_matches (error, G_IO_ERROR, G_IO_ERROR_EXISTS))
            {
              char line[16];

              g_error_free (error);
              error = NULL;

              uri = g_file_get_uri (target);
              g_print (_("%s: overwrite “%s”? "), argv[0], uri);
              g_free (uri);
              if (fgets (line, sizeof (line), stdin) &&
                  (line[0] == 'y' || line[0] == 'Y'))
                {
                  flags |= G_FILE_COPY_OVERWRITE;
                  start_time = g_get_monotonic_time ();
                  if (!g_file_move (source, target, flags, NULL, progress ? show_progress : NULL, NULL, &error))
                    goto move_failed;
                }
            }
          else
            {
            move_failed:
              print_file_error (source, error->message);
              g_error_free (error);
              retval = 1;
            }
        }

      if (progress && retval == 0)
        g_print("\n");

      g_object_unref (source);
      g_object_unref (target);
    }

  g_object_unref (dest);

  return retval;
}
