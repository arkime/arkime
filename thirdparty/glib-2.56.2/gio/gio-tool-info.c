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


static gboolean writable = FALSE;
static gboolean filesystem = FALSE;
static char *attributes = NULL;
static gboolean nofollow_symlinks = FALSE;

static const GOptionEntry entries[] = {
  { "query-writable", 'w', 0, G_OPTION_ARG_NONE, &writable, N_("List writable attributes"), NULL },
  { "filesystem", 'f', 0, G_OPTION_ARG_NONE, &filesystem, N_("Get file system info"), NULL },
  { "attributes", 'a', 0, G_OPTION_ARG_STRING, &attributes, N_("The attributes to get"), N_("ATTRIBUTES") },
  { "nofollow-symlinks", 'n', 0, G_OPTION_ARG_NONE, &nofollow_symlinks, N_("Don’t follow symbolic links"), NULL },
  { NULL }
};

static char *
escape_string (const char *in)
{
  GString *str;
  static char *hex_digits = "0123456789abcdef";
  unsigned char c;


  str = g_string_new ("");

  while ((c = *in++) != 0)
    {
      if (c >= 32 && c <= 126 && c != '\\')
        g_string_append_c (str, c);
      else
        {
          g_string_append (str, "\\x");
          g_string_append_c (str, hex_digits[(c >> 4) & 0xf]);
          g_string_append_c (str, hex_digits[c & 0xf]);
        }
    }

  return g_string_free (str, FALSE);
}

static void
show_attributes (GFileInfo *info)
{
  char **attributes;
  char *s;
  int i;

  attributes = g_file_info_list_attributes (info, NULL);

  g_print (_("attributes:\n"));
  for (i = 0; attributes[i] != NULL; i++)
    {
      /* list the icons in order rather than displaying "GThemedIcon:0x8df7200" */
      if (strcmp (attributes[i], "standard::icon") == 0 ||
          strcmp (attributes[i], "standard::symbolic-icon") == 0)
        {
          GIcon *icon;
          int j;
          const char * const *names = NULL;

          if (strcmp (attributes[i], "standard::symbolic-icon") == 0)
            icon = g_file_info_get_symbolic_icon (info);
          else
            icon = g_file_info_get_icon (info);

          /* only look up names if GThemedIcon */
          if (G_IS_THEMED_ICON(icon))
            {
              names = g_themed_icon_get_names (G_THEMED_ICON (icon));
              g_print ("  %s: ", attributes[i]);
              for (j = 0; names[j] != NULL; j++)
                g_print ("%s%s", names[j], (names[j+1] == NULL)?"":", ");
              g_print ("\n");
            }
          else
            {
              s = g_file_info_get_attribute_as_string (info, attributes[i]);
              g_print ("  %s: %s\n", attributes[i], s);
              g_free (s);
            }
        }
      else
        {
          s = g_file_info_get_attribute_as_string (info, attributes[i]);
          g_print ("  %s: %s\n", attributes[i], s);
          g_free (s);
        }
    }
  g_strfreev (attributes);
}

static void
show_info (GFile *file, GFileInfo *info)
{
  const char *name, *type;
  char *escaped, *uri;
  goffset size;

  name = g_file_info_get_display_name (info);
  if (name)
    /* Translators: This is a noun and represents and attribute of a file */
    g_print (_("display name: %s\n"), name);

  name = g_file_info_get_edit_name (info);
  if (name)
    /* Translators: This is a noun and represents and attribute of a file */
    g_print (_("edit name: %s\n"), name);

  name = g_file_info_get_name (info);
  if (name)
    {
      escaped = escape_string (name);
      g_print (_("name: %s\n"), escaped);
      g_free (escaped);
    }

  if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_STANDARD_TYPE))
    {
      type = file_type_to_string (g_file_info_get_file_type (info));
      g_print (_("type: %s\n"), type);
    }

  if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_STANDARD_SIZE))
    {
      size = g_file_info_get_size (info);
      g_print (_("size: "));
      g_print (" %"G_GUINT64_FORMAT"\n", (guint64)size);
    }

  if (g_file_info_get_is_hidden (info))
    g_print (_("hidden\n"));

  uri = g_file_get_uri (file);
  g_print (_("uri: %s\n"), uri);
  g_free (uri);

  show_attributes (info);
}

static gboolean
query_info (GFile *file)
{
  GFileQueryInfoFlags flags;
  GFileInfo *info;
  GError *error;

  if (file == NULL)
    return FALSE;

  if (attributes == NULL)
    attributes = "*";

  flags = 0;
  if (nofollow_symlinks)
    flags |= G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS;

  error = NULL;
  if (filesystem)
    info = g_file_query_filesystem_info (file, attributes, NULL, &error);
  else
    info = g_file_query_info (file, attributes, flags, NULL, &error);

  if (info == NULL)
    {
      print_file_error (file, error->message);
      g_error_free (error);
      return FALSE;
    }

  if (filesystem)
    show_attributes (info);
  else
    show_info (file, info);

  g_object_unref (info);

  return TRUE;
}

static gboolean
get_writable_info (GFile *file)
{
  GFileAttributeInfoList *list;
  GError *error;
  int i;
  char *flags;

  if (file == NULL)
    return FALSE;

  error = NULL;

  list = g_file_query_settable_attributes (file, NULL, &error);
  if (list == NULL)
    {
      print_file_error (file, error->message);
      g_error_free (error);
      return FALSE;
    }

  if (list->n_infos > 0)
    {
      g_print (_("Settable attributes:\n"));
      for (i = 0; i < list->n_infos; i++)
        {
          flags = attribute_flags_to_string (list->infos[i].flags);
          g_print (" %s (%s%s%s)\n",
                   list->infos[i].name,
                   attribute_type_to_string (list->infos[i].type),
                   (*flags != 0)?", ":"", flags);
          g_free (flags);
        }
    }

  g_file_attribute_info_list_unref (list);

  list = g_file_query_writable_namespaces (file, NULL, &error);
  if (list == NULL)
    {
      print_file_error (file, error->message);
      g_error_free (error);
      return FALSE;
    }

  if (list->n_infos > 0)
    {
      g_print (_("Writable attribute namespaces:\n"));
      for (i = 0; i < list->n_infos; i++)
        {
          flags = attribute_flags_to_string (list->infos[i].flags);
          g_print (" %s (%s%s%s)\n",
                   list->infos[i].name,
                   attribute_type_to_string (list->infos[i].type),
                   (*flags != 0)?", ":"", flags);
          g_free (flags);
        }
    }

  g_file_attribute_info_list_unref (list);

  return TRUE;
}

int
handle_info (int argc, char *argv[], gboolean do_help)
{
  GOptionContext *context;
  gchar *param;
  GError *error = NULL;
  gboolean res;
  gint i;
  GFile *file;

  g_set_prgname ("gio info");

  /* Translators: commandline placeholder */
  param = g_strdup_printf ("%s...", _("LOCATION"));
  context = g_option_context_new (param);
  g_free (param);
  g_option_context_set_help_enabled (context, FALSE);
  g_option_context_set_summary (context,
      _("Show information about locations."));
  g_option_context_set_description (context,
      _("gio info is similar to the traditional ls utility, but using GIO\n"
        "locations instead of local files: for example, you can use something\n"
        "like smb://server/resource/file.txt as location. File attributes can\n"
        "be specified with their GIO name, e.g. standard::icon, or just by\n"
        "namespace, e.g. unix, or by “*”, which matches all attributes"));
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

  res = TRUE;
  for (i = 1; i < argc; i++)
    {
      file = g_file_new_for_commandline_arg (argv[i]);
      if (writable)
        res &= get_writable_info (file);
      else
        res &= query_info (file);
      g_object_unref (file);
    }

  return res ? 0 : 2;
}
