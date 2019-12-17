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
#include <stdlib.h>

#include "gio-tool.h"


static char *attr_type = "string";
static gboolean nofollow_symlinks = FALSE;

static const GOptionEntry entries[] = {
  { "type", 't', 0, G_OPTION_ARG_STRING, &attr_type, N_("Type of the attribute"), N_("TYPE") },
  { "nofollow-symlinks", 'n', 0, G_OPTION_ARG_NONE, &nofollow_symlinks, N_("Don’t follow symbolic links"), NULL },
  { NULL }
};

static char *
hex_unescape (const char *str)
{
  int i;
  char *unescaped_str, *p;
  unsigned char c;
  int len;

  len = strlen (str);
  unescaped_str = g_malloc (len + 1);

  p = unescaped_str;
  for (i = 0; i < len; i++)
    {
      if (str[i] == '\\' &&
	  str[i+1] == 'x' &&
	  len - i >= 4)
	{
	  c =
	    (g_ascii_xdigit_value (str[i+2]) << 4) |
	    g_ascii_xdigit_value (str[i+3]);
	  *p++ = c;
	  i += 3;
	}
      else
	*p++ = str[i];
    }
  *p++ = 0;

  return unescaped_str;
}

int
handle_set (int argc, char *argv[], gboolean do_help)
{
  GOptionContext *context;
  GError *error = NULL;
  GFile *file;
  const char *attribute;
  GFileAttributeType type;
  gpointer value;
  gboolean b;
  guint32 uint32;
  gint32 int32;
  guint64 uint64;
  gint64 int64;
  gchar *param;

  g_set_prgname ("gio set");

  /* Translators: commandline placeholder */
  param = g_strdup_printf ("%s %s %s...", _("LOCATION"), _("ATTRIBUTE"), _("VALUE"));
  context = g_option_context_new (param);
  g_free (param);
  g_option_context_set_help_enabled (context, FALSE);
  g_option_context_set_summary (context, _("Set a file attribute of LOCATION."));
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
      show_help (context, _("Location not specified"));
      g_option_context_free (context);
      return 1;
    }

  if (argc < 3)
    {
      show_help (context, _("Attribute not specified"));
      g_option_context_free (context);
      return 1;
    }

  attribute = argv[2];

  type = attribute_type_from_string (attr_type);
  if ((argc < 4) && (type != G_FILE_ATTRIBUTE_TYPE_INVALID))
    {
      show_help (context, _("Value not specified"));
      g_option_context_free (context);
      return 1;
    }

  if ((argc > 4) && (type != G_FILE_ATTRIBUTE_TYPE_STRINGV))
    {
      show_help (context, _("Too many arguments"));
      g_option_context_free (context);
      return 1;
    }

  g_option_context_free (context);

  switch (type)
    {
    case G_FILE_ATTRIBUTE_TYPE_STRING:
      value = argv[3];
      break;
    case G_FILE_ATTRIBUTE_TYPE_BYTE_STRING:
      value = hex_unescape (argv[3]);
      break;
    case G_FILE_ATTRIBUTE_TYPE_BOOLEAN:
      b = g_ascii_strcasecmp (argv[3], "true") == 0;
      value = &b;
      break;
    case G_FILE_ATTRIBUTE_TYPE_UINT32:
      uint32 = atol (argv[3]);
      value = &uint32;
      break;
    case G_FILE_ATTRIBUTE_TYPE_INT32:
      int32 = atol (argv[3]);
      value = &int32;
      break;
    case G_FILE_ATTRIBUTE_TYPE_UINT64:
      uint64 = g_ascii_strtoull (argv[3], NULL, 10);
      value = &uint64;
      break;
    case G_FILE_ATTRIBUTE_TYPE_INT64:
      int64 = g_ascii_strtoll (argv[3], NULL, 10);
      value = &int64;
      break;
    case G_FILE_ATTRIBUTE_TYPE_STRINGV:
      value = &argv[3];
      break;
    case G_FILE_ATTRIBUTE_TYPE_INVALID:
      value = NULL;
      break;
    case G_FILE_ATTRIBUTE_TYPE_OBJECT:
    default:
      print_error (_("Invalid attribute type “%s”"), attr_type);
      return 1;
    }

  file = g_file_new_for_commandline_arg (argv[1]);

  if (!g_file_set_attribute (file,
			     attribute,
			     type,
			     value,
                             nofollow_symlinks ?
                               G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS :
                               G_FILE_QUERY_INFO_NONE,
                             NULL, &error))
    {
      print_error ("%s", error->message);
      g_error_free (error);
      g_object_unref (file);
      return 1;
    }

  g_object_unref (file);

  return 0;
}
