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
#include <locale.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "gio-tool.h"


void
print_error (const gchar *format, ...)
{
  gchar *message;
  va_list args;

  va_start (args, format);
  message = g_strdup_vprintf (format, args);
  va_end (args);

  g_printerr ("gio: %s\n", message);
  g_free (message);
}

void
print_file_error (GFile *file, const gchar *message)
{
  gchar *uri;

  uri = g_file_get_uri (file);
  print_error ("%s: %s", uri, message);
  g_free (uri);
}

void
show_help (GOptionContext *context, const char *message)
{
  char *help;

  if (message)
    g_printerr ("gio: %s\n\n", message);

  help = g_option_context_get_help (context, TRUE, NULL);
  g_printerr ("%s", help);
  g_free (help);
}

const char *
file_type_to_string (GFileType type)
{
  switch (type)
    {
    case G_FILE_TYPE_UNKNOWN:
      return "unknown";
    case G_FILE_TYPE_REGULAR:
      return "regular";
    case G_FILE_TYPE_DIRECTORY:
      return "directory";
    case G_FILE_TYPE_SYMBOLIC_LINK:
      return "symlink";
    case G_FILE_TYPE_SPECIAL:
      return "special";
    case G_FILE_TYPE_SHORTCUT:
      return "shortcut";
    case G_FILE_TYPE_MOUNTABLE:
      return "mountable";
    default:
      return "invalid type";
    }
}

const char *
attribute_type_to_string (GFileAttributeType type)
{
  switch (type)
    {
    case G_FILE_ATTRIBUTE_TYPE_INVALID:
      return "invalid";
    case G_FILE_ATTRIBUTE_TYPE_STRING:
      return "string";
    case G_FILE_ATTRIBUTE_TYPE_BYTE_STRING:
      return "bytestring";
    case G_FILE_ATTRIBUTE_TYPE_BOOLEAN:
      return "boolean";
    case G_FILE_ATTRIBUTE_TYPE_UINT32:
      return "uint32";
    case G_FILE_ATTRIBUTE_TYPE_INT32:
      return "int32";
    case G_FILE_ATTRIBUTE_TYPE_UINT64:
      return "uint64";
    case G_FILE_ATTRIBUTE_TYPE_INT64:
      return "int64";
    case G_FILE_ATTRIBUTE_TYPE_OBJECT:
      return "object";
    default:
      return "uknown type";
    }
}

GFileAttributeType
attribute_type_from_string (const char *str)
{
  if (strcmp (str, "string") == 0)
    return G_FILE_ATTRIBUTE_TYPE_STRING;
  if (strcmp (str, "stringv") == 0)
    return G_FILE_ATTRIBUTE_TYPE_STRINGV;
  if (strcmp (str, "bytestring") == 0)
    return G_FILE_ATTRIBUTE_TYPE_BYTE_STRING;
  if (strcmp (str, "boolean") == 0)
    return G_FILE_ATTRIBUTE_TYPE_BOOLEAN;
  if (strcmp (str, "uint32") == 0)
    return G_FILE_ATTRIBUTE_TYPE_UINT32;
  if (strcmp (str, "int32") == 0)
    return G_FILE_ATTRIBUTE_TYPE_INT32;
  if (strcmp (str, "uint64") == 0)
    return G_FILE_ATTRIBUTE_TYPE_UINT64;
  if (strcmp (str, "int64") == 0)
    return G_FILE_ATTRIBUTE_TYPE_INT64;
  if (strcmp (str, "object") == 0)
    return G_FILE_ATTRIBUTE_TYPE_OBJECT;
  if (strcmp (str, "unset") == 0)
    return G_FILE_ATTRIBUTE_TYPE_INVALID;
  return -1;
}

char *
attribute_flags_to_string (GFileAttributeInfoFlags flags)
{
  GString *s;
  int i;
  gboolean first;
  struct {
    guint32 mask;
    char *descr;
  } flag_descr[] = {
    {
      G_FILE_ATTRIBUTE_INFO_COPY_WITH_FILE,
      N_("Copy with file")
    },
    {
      G_FILE_ATTRIBUTE_INFO_COPY_WHEN_MOVED,
      N_("Keep with file when moved")
    }
  };

  first = TRUE;

  s = g_string_new ("");
  for (i = 0; i < G_N_ELEMENTS (flag_descr); i++)
    {
      if (flags & flag_descr[i].mask)
        {
          if (!first)
            g_string_append (s, ", ");
          g_string_append (s, gettext (flag_descr[i].descr));
          first = FALSE;
        }
    }

  return g_string_free (s, FALSE);
}

gboolean
file_is_dir (GFile *file)
{
  GFileInfo *info;
  gboolean res;

  info = g_file_query_info (file, G_FILE_ATTRIBUTE_STANDARD_TYPE, 0, NULL, NULL);
  res = info && g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY;
  if (info)
    g_object_unref (info);
  return res;
}


static int
handle_version (int argc, char *argv[], gboolean do_help)
{
  if (do_help || argc > 1)
    {
      if (!do_help)
        g_printerr ("gio: %s\n\n", _("“version” takes no arguments"));

      g_printerr ("%s\n", _("Usage:"));
      g_printerr ("  gio version\n");
      g_printerr ("\n");
      g_printerr ("%s\n", _("Print version information and exit."));

      return do_help ? 0 : 2;
    }

  g_print ("%d.%d.%d\n", glib_major_version, glib_minor_version, glib_micro_version);

  return 0;
}

static void
usage (void)
{
  g_printerr ("%s\n", _("Usage:"));
  g_printerr ("  gio %s %s\n", _("COMMAND"), _("[ARGS...]"));
  g_printerr ("\n");
  g_printerr ("%s\n", _("Commands:"));
  g_printerr ("  help     %s\n", _("Print help"));
  g_printerr ("  version  %s\n", _("Print version"));
  g_printerr ("  cat      %s\n", _("Concatenate files to standard output"));
  g_printerr ("  copy     %s\n", _("Copy one or more files"));
  g_printerr ("  info     %s\n", _("Show information about locations"));
  g_printerr ("  list     %s\n", _("List the contents of locations"));
  g_printerr ("  mime     %s\n", _("Get or set the handler for a mimetype"));
  g_printerr ("  mkdir    %s\n", _("Create directories"));
  g_printerr ("  monitor  %s\n", _("Monitor files and directories for changes"));
  g_printerr ("  mount    %s\n", _("Mount or unmount the locations"));
  g_printerr ("  move     %s\n", _("Move one or more files"));
  g_printerr ("  open     %s\n", _("Open files with the default application"));
  g_printerr ("  rename   %s\n", _("Rename a file"));
  g_printerr ("  remove   %s\n", _("Delete one or more files"));
  g_printerr ("  save     %s\n", _("Read from standard input and save"));
  g_printerr ("  set      %s\n", _("Set a file attribute"));
  g_printerr ("  trash    %s\n", _("Move files or directories to the trash"));
  g_printerr ("  tree     %s\n", _("Lists the contents of locations in a tree"));
  g_printerr ("\n");
  g_printerr (_("Use %s to get detailed help.\n"), "“gio help COMMAND”");
  exit (1);
}

int
main (int argc, char **argv)
{
  const char *command;
  gboolean do_help;

  setlocale (LC_ALL, "");
  textdomain (GETTEXT_PACKAGE);
  bindtextdomain (GETTEXT_PACKAGE, GLIB_LOCALE_DIR);

#ifdef HAVE_BIND_TEXTDOMAIN_CODESET
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
#endif

  if (argc < 2)
    {
      usage ();
      return 1;
    }

  command = argv[1];
  argc -= 1;
  argv += 1;

  do_help = FALSE;
  if (g_str_equal (command, "help"))
    {
      if (argc == 1)
        {
          usage ();
          return 0;
        }
      else
        {
          command = argv[1];
          do_help = TRUE;
        }
    }
  else if (g_str_equal (command, "--help"))
    {
      usage ();
      return 0;
    }
  else if (g_str_equal (command, "--version"))
    command = "version";

  if (g_str_equal (command, "version"))
    return handle_version (argc, argv, do_help);
  else if (g_str_equal (command, "cat"))
    return handle_cat (argc, argv, do_help);
  else if (g_str_equal (command, "copy"))
    return handle_copy (argc, argv, do_help);
  else if (g_str_equal (command, "info"))
    return handle_info (argc, argv, do_help);
  else if (g_str_equal (command, "list"))
    return handle_list (argc, argv, do_help);
  else if (g_str_equal (command, "mime"))
    return handle_mime (argc, argv, do_help);
  else if (g_str_equal (command, "mkdir"))
    return handle_mkdir (argc, argv, do_help);
  else if (g_str_equal (command, "monitor"))
    return handle_monitor (argc, argv, do_help);
  else if (g_str_equal (command, "mount"))
    return handle_mount (argc, argv, do_help);
  else if (g_str_equal (command, "move"))
    return handle_move (argc, argv, do_help);
  else if (g_str_equal (command, "open"))
    return handle_open (argc, argv, do_help);
  else if (g_str_equal (command, "rename"))
    return handle_rename (argc, argv, do_help);
  else if (g_str_equal (command, "remove"))
    return handle_remove (argc, argv, do_help);
  else if (g_str_equal (command, "save"))
    return handle_save (argc, argv, do_help);
  else if (g_str_equal (command, "set"))
    return handle_set (argc, argv, do_help);
  else if (g_str_equal (command, "trash"))
    return handle_trash (argc, argv, do_help);
  else if (g_str_equal (command, "tree"))
    return handle_tree (argc, argv, do_help);
  else
    usage ();

  return 1;
}
