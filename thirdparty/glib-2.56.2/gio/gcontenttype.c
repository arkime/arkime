/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright (C) 2006-2007 Red Hat, Inc.
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
 * Author: Alexander Larsson <alexl@redhat.com>
 */

#include "config.h"
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "gcontenttypeprivate.h"
#include "gthemedicon.h"
#include "gicon.h"
#include "gfile.h"
#include "gfileenumerator.h"
#include "gfileinfo.h"
#include "glibintl.h"


/**
 * SECTION:gcontenttype
 * @short_description: Platform-specific content typing
 * @include: gio/gio.h
 *
 * A content type is a platform specific string that defines the type
 * of a file. On UNIX it is a
 * [mime type](http://www.wikipedia.org/wiki/Internet_media_type)
 * like "text/plain" or "image/png".
 * On Win32 it is an extension string like ".doc", ".txt" or a perceived
 * string like "audio". Such strings can be looked up in the registry at
 * HKEY_CLASSES_ROOT.
 * On OSX it is a [Uniform Type Identifier](https://en.wikipedia.org/wiki/Uniform_Type_Identifier)
 * such as "com.apple.application".
 **/

#include <dirent.h>

#define XDG_PREFIX _gio_xdg
#include "xdgmime/xdgmime.h"

/* We lock this mutex whenever we modify global state in this module.  */
G_LOCK_DEFINE_STATIC (gio_xdgmime);

gsize
_g_unix_content_type_get_sniff_len (void)
{
  gsize size;

  G_LOCK (gio_xdgmime);
  size = xdg_mime_get_max_buffer_extents ();
  G_UNLOCK (gio_xdgmime);

  return size;
}

gchar *
_g_unix_content_type_unalias (const gchar *type)
{
  gchar *res;

  G_LOCK (gio_xdgmime);
  res = g_strdup (xdg_mime_unalias_mime_type (type));
  G_UNLOCK (gio_xdgmime);

  return res;
}

gchar **
_g_unix_content_type_get_parents (const gchar *type)
{
  const gchar *umime;
  gchar **parents;
  GPtrArray *array;
  int i;

  array = g_ptr_array_new ();

  G_LOCK (gio_xdgmime);

  umime = xdg_mime_unalias_mime_type (type);

  g_ptr_array_add (array, g_strdup (umime));

  parents = xdg_mime_list_mime_parents (umime);
  for (i = 0; parents && parents[i] != NULL; i++)
    g_ptr_array_add (array, g_strdup (parents[i]));

  free (parents);

  G_UNLOCK (gio_xdgmime);

  g_ptr_array_add (array, NULL);

  return (gchar **)g_ptr_array_free (array, FALSE);
}

/**
 * g_content_type_equals:
 * @type1: a content type string
 * @type2: a content type string
 *
 * Compares two content types for equality.
 *
 * Returns: %TRUE if the two strings are identical or equivalent,
 *     %FALSE otherwise.
 */
gboolean
g_content_type_equals (const gchar *type1,
                       const gchar *type2)
{
  gboolean res;

  g_return_val_if_fail (type1 != NULL, FALSE);
  g_return_val_if_fail (type2 != NULL, FALSE);

  G_LOCK (gio_xdgmime);
  res = xdg_mime_mime_type_equal (type1, type2);
  G_UNLOCK (gio_xdgmime);

  return res;
}

/**
 * g_content_type_is_a:
 * @type: a content type string
 * @supertype: a content type string
 *
 * Determines if @type is a subset of @supertype.
 *
 * Returns: %TRUE if @type is a kind of @supertype,
 *     %FALSE otherwise.
 */
gboolean
g_content_type_is_a (const gchar *type,
                     const gchar *supertype)
{
  gboolean res;

  g_return_val_if_fail (type != NULL, FALSE);
  g_return_val_if_fail (supertype != NULL, FALSE);

  G_LOCK (gio_xdgmime);
  res = xdg_mime_mime_type_subclass (type, supertype);
  G_UNLOCK (gio_xdgmime);

  return res;
}

/**
 * g_content_type_is_mime_type:
 * @type: a content type string
 * @mime_type: a mime type string
 *
 * Determines if @type is a subset of @mime_type.
 * Convenience wrapper around g_content_type_is_a().
 *
 * Returns: %TRUE if @type is a kind of @mime_type,
 *     %FALSE otherwise.
 *
 * Since: 2.52
 */
gboolean
g_content_type_is_mime_type (const gchar *type,
                             const gchar *mime_type)
{
  return g_content_type_is_a (type, mime_type);
}

/**
 * g_content_type_is_unknown:
 * @type: a content type string
 *
 * Checks if the content type is the generic "unknown" type.
 * On UNIX this is the "application/octet-stream" mimetype,
 * while on win32 it is "*" and on OSX it is a dynamic type
 * or octet-stream.
 *
 * Returns: %TRUE if the type is the unknown type.
 */
gboolean
g_content_type_is_unknown (const gchar *type)
{
  g_return_val_if_fail (type != NULL, FALSE);

  return strcmp (XDG_MIME_TYPE_UNKNOWN, type) == 0;
}


typedef enum {
  MIME_TAG_TYPE_OTHER,
  MIME_TAG_TYPE_COMMENT
} MimeTagType;

typedef struct {
  int current_type;
  int current_lang_level;
  int comment_lang_level;
  char *comment;
} MimeParser;


static int
language_level (const char *lang)
{
  const char * const *lang_list;
  int i;

  /* The returned list is sorted from most desirable to least
     desirable and always contains the default locale "C". */
  lang_list = g_get_language_names ();

  for (i = 0; lang_list[i]; i++)
    if (strcmp (lang_list[i], lang) == 0)
      return 1000-i;

  return 0;
}

static void
mime_info_start_element (GMarkupParseContext  *context,
                         const gchar          *element_name,
                         const gchar         **attribute_names,
                         const gchar         **attribute_values,
                         gpointer              user_data,
                         GError              **error)
{
  int i;
  const char *lang;
  MimeParser *parser = user_data;

  if (strcmp (element_name, "comment") == 0)
    {
      lang = "C";
      for (i = 0; attribute_names[i]; i++)
        if (strcmp (attribute_names[i], "xml:lang") == 0)
          {
            lang = attribute_values[i];
            break;
          }

      parser->current_lang_level = language_level (lang);
      parser->current_type = MIME_TAG_TYPE_COMMENT;
    }
  else
    parser->current_type = MIME_TAG_TYPE_OTHER;
}

static void
mime_info_end_element (GMarkupParseContext  *context,
                       const gchar          *element_name,
                       gpointer              user_data,
                       GError              **error)
{
  MimeParser *parser = user_data;

  parser->current_type = MIME_TAG_TYPE_OTHER;
}

static void
mime_info_text (GMarkupParseContext  *context,
                const gchar          *text,
                gsize                 text_len,
                gpointer              user_data,
                GError              **error)
{
  MimeParser *parser = user_data;

  if (parser->current_type == MIME_TAG_TYPE_COMMENT &&
      parser->current_lang_level > parser->comment_lang_level)
    {
      g_free (parser->comment);
      parser->comment = g_strndup (text, text_len);
      parser->comment_lang_level = parser->current_lang_level;
    }
}

static char *
load_comment_for_mime_helper (const char *dir,
                              const char *basename)
{
  GMarkupParseContext *context;
  char *filename, *data;
  gsize len;
  gboolean res;
  MimeParser parse_data = {0};
  GMarkupParser parser = {
    mime_info_start_element,
    mime_info_end_element,
    mime_info_text
  };

  filename = g_build_filename (dir, "mime", basename, NULL);

  res = g_file_get_contents (filename,  &data,  &len,  NULL);
  g_free (filename);
  if (!res)
    return NULL;

  context = g_markup_parse_context_new   (&parser, 0, &parse_data, NULL);
  res = g_markup_parse_context_parse (context, data, len, NULL);
  g_free (data);
  g_markup_parse_context_free (context);

  if (!res)
    return NULL;

  return parse_data.comment;
}


static char *
load_comment_for_mime (const char *mimetype)
{
  const char * const* dirs;
  char *basename;
  char *comment;
  int i;

  basename = g_strdup_printf ("%s.xml", mimetype);

  comment = load_comment_for_mime_helper (g_get_user_data_dir (), basename);
  if (comment)
    {
      g_free (basename);
      return comment;
    }

  dirs = g_get_system_data_dirs ();

  for (i = 0; dirs[i] != NULL; i++)
    {
      comment = load_comment_for_mime_helper (dirs[i], basename);
      if (comment)
        {
          g_free (basename);
          return comment;
        }
    }
  g_free (basename);

  return g_strdup_printf (_("%s type"), mimetype);
}

/**
 * g_content_type_get_description:
 * @type: a content type string
 *
 * Gets the human readable description of the content type.
 *
 * Returns: a short description of the content type @type. Free the
 *     returned string with g_free()
 */
gchar *
g_content_type_get_description (const gchar *type)
{
  static GHashTable *type_comment_cache = NULL;
  gchar *comment;

  g_return_val_if_fail (type != NULL, NULL);

  G_LOCK (gio_xdgmime);
  type = xdg_mime_unalias_mime_type (type);

  if (type_comment_cache == NULL)
    type_comment_cache = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

  comment = g_hash_table_lookup (type_comment_cache, type);
  comment = g_strdup (comment);
  G_UNLOCK (gio_xdgmime);

  if (comment != NULL)
    return comment;

  comment = load_comment_for_mime (type);

  G_LOCK (gio_xdgmime);
  g_hash_table_insert (type_comment_cache,
                       g_strdup (type),
                       g_strdup (comment));
  G_UNLOCK (gio_xdgmime);

  return comment;
}

/**
 * g_content_type_get_mime_type:
 * @type: a content type string
 *
 * Gets the mime type for the content type, if one is registered.
 *
 * Returns: (nullable): the registered mime type for the given @type,
 *     or %NULL if unknown.
 */
char *
g_content_type_get_mime_type (const char *type)
{
  g_return_val_if_fail (type != NULL, NULL);

  return g_strdup (type);
}

static GIcon *
g_content_type_get_icon_internal (const gchar *type,
                                  gboolean     symbolic)
{
  char *mimetype_icon;
  char *generic_mimetype_icon = NULL;
  char *q;
  char *icon_names[6];
  int n = 0;
  GIcon *themed_icon;
  const char  *xdg_icon;
  int i;

  g_return_val_if_fail (type != NULL, NULL);

  G_LOCK (gio_xdgmime);
  xdg_icon = xdg_mime_get_icon (type);
  G_UNLOCK (gio_xdgmime);

  if (xdg_icon)
    icon_names[n++] = g_strdup (xdg_icon);

  mimetype_icon = g_strdup (type);
  while ((q = strchr (mimetype_icon, '/')) != NULL)
    *q = '-';

  icon_names[n++] = mimetype_icon;

  generic_mimetype_icon = g_content_type_get_generic_icon_name (type);
  if (generic_mimetype_icon)
    icon_names[n++] = generic_mimetype_icon;

  if (symbolic)
    {
      for (i = 0; i < n; i++)
        {
          icon_names[n + i] = icon_names[i];
          icon_names[i] = g_strconcat (icon_names[i], "-symbolic", NULL);
        }

      n += n;
    }

  themed_icon = g_themed_icon_new_from_names (icon_names, n);

  for (i = 0; i < n; i++)
    g_free (icon_names[i]);

  return themed_icon;
}

/**
 * g_content_type_get_icon:
 * @type: a content type string
 *
 * Gets the icon for a content type.
 *
 * Returns: (transfer full): #GIcon corresponding to the content type. Free the returned
 *     object with g_object_unref()
 */
GIcon *
g_content_type_get_icon (const gchar *type)
{
  return g_content_type_get_icon_internal (type, FALSE);
}

/**
 * g_content_type_get_symbolic_icon:
 * @type: a content type string
 *
 * Gets the symbolic icon for a content type.
 *
 * Returns: (transfer full): symbolic #GIcon corresponding to the content type.
 *     Free the returned object with g_object_unref()
 *
 * Since: 2.34
 */
GIcon *
g_content_type_get_symbolic_icon (const gchar *type)
{
  return g_content_type_get_icon_internal (type, TRUE);
}

/**
 * g_content_type_get_generic_icon_name:
 * @type: a content type string
 *
 * Gets the generic icon name for a content type.
 *
 * See the
 * [shared-mime-info](http://www.freedesktop.org/wiki/Specifications/shared-mime-info-spec)
 * specification for more on the generic icon name.
 *
 * Returns: (nullable): the registered generic icon name for the given @type,
 *     or %NULL if unknown. Free with g_free()
 *
 * Since: 2.34
 */
gchar *
g_content_type_get_generic_icon_name (const gchar *type)
{
  const gchar *xdg_icon_name;
  gchar *icon_name;

  G_LOCK (gio_xdgmime);
  xdg_icon_name = xdg_mime_get_generic_icon (type);
  G_UNLOCK (gio_xdgmime);

  if (!xdg_icon_name)
    {
      const char *p;
      const char *suffix = "-x-generic";

      p = strchr (type, '/');
      if (p == NULL)
        p = type + strlen (type);

      icon_name = g_malloc (p - type + strlen (suffix) + 1);
      memcpy (icon_name, type, p - type);
      memcpy (icon_name + (p - type), suffix, strlen (suffix));
      icon_name[(p - type) + strlen (suffix)] = 0;
    }
  else
    {
      icon_name = g_strdup (xdg_icon_name);
    }

  return icon_name;
}

/**
 * g_content_type_can_be_executable:
 * @type: a content type string
 *
 * Checks if a content type can be executable. Note that for instance
 * things like text files can be executables (i.e. scripts and batch files).
 *
 * Returns: %TRUE if the file type corresponds to a type that
 *     can be executable, %FALSE otherwise.
 */
gboolean
g_content_type_can_be_executable (const gchar *type)
{
  g_return_val_if_fail (type != NULL, FALSE);

  if (g_content_type_is_a (type, "application/x-executable")  ||
      g_content_type_is_a (type, "text/plain"))
    return TRUE;

  return FALSE;
}

static gboolean
looks_like_text (const guchar *data, gsize data_size)
{
  gsize i;
  char c;

  for (i = 0; i < data_size; i++)
    {
      c = data[i];

      if (g_ascii_iscntrl (c) &&
          !g_ascii_isspace (c) &&
          c != '\b')
        return FALSE;
    }
  return TRUE;
}

/**
 * g_content_type_from_mime_type:
 * @mime_type: a mime type string
 *
 * Tries to find a content type based on the mime type name.
 *
 * Returns: (nullable): Newly allocated string with content type or
 *     %NULL. Free with g_free()
 *
 * Since: 2.18
 **/
gchar *
g_content_type_from_mime_type (const gchar *mime_type)
{
  char *umime;

  g_return_val_if_fail (mime_type != NULL, NULL);

  G_LOCK (gio_xdgmime);
  /* mime type and content type are same on unixes */
  umime = g_strdup (xdg_mime_unalias_mime_type (mime_type));
  G_UNLOCK (gio_xdgmime);

  return umime;
}

/**
 * g_content_type_guess:
 * @filename: (nullable): a string, or %NULL
 * @data: (nullable) (array length=data_size): a stream of data, or %NULL
 * @data_size: the size of @data
 * @result_uncertain: (out) (optional): return location for the certainty
 *     of the result, or %NULL
 *
 * Guesses the content type based on example data. If the function is
 * uncertain, @result_uncertain will be set to %TRUE. Either @filename
 * or @data may be %NULL, in which case the guess will be based solely
 * on the other argument.
 *
 * Returns: a string indicating a guessed content type for the
 *     given data. Free with g_free()
 */
gchar *
g_content_type_guess (const gchar  *filename,
                      const guchar *data,
                      gsize         data_size,
                      gboolean     *result_uncertain)
{
  char *basename;
  const char *name_mimetypes[10], *sniffed_mimetype;
  char *mimetype;
  int i;
  int n_name_mimetypes;
  int sniffed_prio;

  sniffed_prio = 0;
  n_name_mimetypes = 0;
  sniffed_mimetype = XDG_MIME_TYPE_UNKNOWN;

  if (result_uncertain)
    *result_uncertain = FALSE;

  /* our test suite and potentially other code used -1 in the past, which is
   * not documented and not allowed; guard against that */
  g_return_val_if_fail (data_size != (gsize) -1, g_strdup (XDG_MIME_TYPE_UNKNOWN));

  G_LOCK (gio_xdgmime);

  if (filename)
    {
      i = strlen (filename);
      if (filename[i - 1] == '/')
        {
          name_mimetypes[0] = "inode/directory";
          name_mimetypes[1] = NULL;
          n_name_mimetypes = 1;
          if (result_uncertain)
            *result_uncertain = TRUE;
        }
      else
        {
          basename = g_path_get_basename (filename);
          n_name_mimetypes = xdg_mime_get_mime_types_from_file_name (basename, name_mimetypes, 10);
          g_free (basename);
        }
    }

  /* Got an extension match, and no conflicts. This is it. */
  if (n_name_mimetypes == 1)
    {
      gchar *s = g_strdup (name_mimetypes[0]);
      G_UNLOCK (gio_xdgmime);
      return s;
    }

  if (data)
    {
      sniffed_mimetype = xdg_mime_get_mime_type_for_data (data, data_size, &sniffed_prio);
      if (sniffed_mimetype == XDG_MIME_TYPE_UNKNOWN &&
          data &&
          looks_like_text (data, data_size))
        sniffed_mimetype = "text/plain";

      /* For security reasons we don't ever want to sniff desktop files
       * where we know the filename and it doesn't have a .desktop extension.
       * This is because desktop files allow executing any application and
       * we don't want to make it possible to hide them looking like something
       * else.
       */
      if (filename != NULL &&
          strcmp (sniffed_mimetype, "application/x-desktop") == 0)
        sniffed_mimetype = "text/plain";
    }

  if (n_name_mimetypes == 0)
    {
      if (sniffed_mimetype == XDG_MIME_TYPE_UNKNOWN &&
          result_uncertain)
        *result_uncertain = TRUE;

      mimetype = g_strdup (sniffed_mimetype);
    }
  else
    {
      mimetype = NULL;
      if (sniffed_mimetype != XDG_MIME_TYPE_UNKNOWN)
        {
          if (sniffed_prio >= 80) /* High priority sniffing match, use that */
            mimetype = g_strdup (sniffed_mimetype);
          else
            {
              /* There are conflicts between the name matches and we
               * have a sniffed type, use that as a tie breaker.
               */
              for (i = 0; i < n_name_mimetypes; i++)
                {
                  if ( xdg_mime_mime_type_subclass (name_mimetypes[i], sniffed_mimetype))
                    {
                      /* This nametype match is derived from (or the same as)
                       * the sniffed type). This is probably it.
                       */
                      mimetype = g_strdup (name_mimetypes[i]);
                      break;
                    }
                }
            }
        }

      if (mimetype == NULL)
        {
          /* Conflicts, and sniffed type was no help or not there.
           * Guess on the first one
           */
          mimetype = g_strdup (name_mimetypes[0]);
          if (result_uncertain)
            *result_uncertain = TRUE;
        }
    }

  G_UNLOCK (gio_xdgmime);

  return mimetype;
}

static void
enumerate_mimetypes_subdir (const char *dir,
                            const char *prefix,
                            GHashTable *mimetypes)
{
  DIR *d;
  struct dirent *ent;
  char *mimetype;

  d = opendir (dir);
  if (d)
    {
      while ((ent = readdir (d)) != NULL)
        {
          if (g_str_has_suffix (ent->d_name, ".xml"))
            {
              mimetype = g_strdup_printf ("%s/%.*s", prefix, (int) strlen (ent->d_name) - 4, ent->d_name);
              g_hash_table_replace (mimetypes, mimetype, NULL);
            }
        }
      closedir (d);
    }
}

static void
enumerate_mimetypes_dir (const char *dir,
                         GHashTable *mimetypes)
{
  DIR *d;
  struct dirent *ent;
  char *mimedir;
  char *name;

  mimedir = g_build_filename (dir, "mime", NULL);

  d = opendir (mimedir);
  if (d)
    {
      while ((ent = readdir (d)) != NULL)
        {
          if (strcmp (ent->d_name, "packages") != 0)
            {
              name = g_build_filename (mimedir, ent->d_name, NULL);
              if (g_file_test (name, G_FILE_TEST_IS_DIR))
                enumerate_mimetypes_subdir (name, ent->d_name, mimetypes);
              g_free (name);
            }
        }
      closedir (d);
    }

  g_free (mimedir);
}

/**
 * g_content_types_get_registered:
 *
 * Gets a list of strings containing all the registered content types
 * known to the system. The list and its data should be freed using
 * g_list_free_full (list, g_free).
 *
 * Returns: (element-type utf8) (transfer full): list of the registered
 *     content types
 */
GList *
g_content_types_get_registered (void)
{
  const char * const* dirs;
  GHashTable *mimetypes;
  GHashTableIter iter;
  gpointer key;
  int i;
  GList *l;

  mimetypes = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  enumerate_mimetypes_dir (g_get_user_data_dir (), mimetypes);
  dirs = g_get_system_data_dirs ();

  for (i = 0; dirs[i] != NULL; i++)
    enumerate_mimetypes_dir (dirs[i], mimetypes);

  l = NULL;
  g_hash_table_iter_init (&iter, mimetypes);
  while (g_hash_table_iter_next (&iter, &key, NULL))
    {
      l = g_list_prepend (l, key);
      g_hash_table_iter_steal (&iter);
    }

  g_hash_table_destroy (mimetypes);

  return l;
}


/* tree magic data */
static GList *tree_matches = NULL;
static gboolean need_reload = FALSE;

G_LOCK_DEFINE_STATIC (gio_treemagic);

typedef struct
{
  gchar *path;
  GFileType type;
  guint match_case : 1;
  guint executable : 1;
  guint non_empty  : 1;
  guint on_disc    : 1;
  gchar *mimetype;
  GList *matches;
} TreeMatchlet;

typedef struct
{
  gchar *contenttype;
  gint priority;
  GList *matches;
} TreeMatch;


static void
tree_matchlet_free (TreeMatchlet *matchlet)
{
  g_list_free_full (matchlet->matches, (GDestroyNotify) tree_matchlet_free);
  g_free (matchlet->path);
  g_free (matchlet->mimetype);
  g_slice_free (TreeMatchlet, matchlet);
}

static void
tree_match_free (TreeMatch *match)
{
  g_list_free_full (match->matches, (GDestroyNotify) tree_matchlet_free);
  g_free (match->contenttype);
  g_slice_free (TreeMatch, match);
}

static TreeMatch *
parse_header (gchar *line)
{
  gint len;
  gchar *s;
  TreeMatch *match;

  len = strlen (line);

  if (line[0] != '[' || line[len - 1] != ']')
    return NULL;

  line[len - 1] = 0;
  s = strchr (line, ':');

  match = g_slice_new0 (TreeMatch);
  match->priority = atoi (line + 1);
  match->contenttype = g_strdup (s + 1);

  return match;
}

static TreeMatchlet *
parse_match_line (gchar *line,
                  gint  *depth)
{
  gchar *s, *p;
  TreeMatchlet *matchlet;
  gchar **parts;
  gint i;

  matchlet = g_slice_new0 (TreeMatchlet);

  if (line[0] == '>')
    {
      *depth = 0;
      s = line;
    }
  else
    {
      *depth = atoi (line);
      s = strchr (line, '>');
    }
  s += 2;
  p = strchr (s, '"');
  *p = 0;

  matchlet->path = g_strdup (s);
  s = p + 1;
  parts = g_strsplit (s, ",", 0);
  if (strcmp (parts[0], "=file") == 0)
    matchlet->type = G_FILE_TYPE_REGULAR;
  else if (strcmp (parts[0], "=directory") == 0)
    matchlet->type = G_FILE_TYPE_DIRECTORY;
  else if (strcmp (parts[0], "=link") == 0)
    matchlet->type = G_FILE_TYPE_SYMBOLIC_LINK;
  else
    matchlet->type = G_FILE_TYPE_UNKNOWN;
  for (i = 1; parts[i]; i++)
    {
      if (strcmp (parts[i], "executable") == 0)
        matchlet->executable = 1;
      else if (strcmp (parts[i], "match-case") == 0)
        matchlet->match_case = 1;
      else if (strcmp (parts[i], "non-empty") == 0)
        matchlet->non_empty = 1;
      else if (strcmp (parts[i], "on-disc") == 0)
        matchlet->on_disc = 1;
      else
        matchlet->mimetype = g_strdup (parts[i]);
    }

  g_strfreev (parts);

  return matchlet;
}

static gint
cmp_match (gconstpointer a, gconstpointer b)
{
  const TreeMatch *aa = (const TreeMatch *)a;
  const TreeMatch *bb = (const TreeMatch *)b;

  return bb->priority - aa->priority;
}

static void
insert_match (TreeMatch *match)
{
  tree_matches = g_list_insert_sorted (tree_matches, match, cmp_match);
}

static void
insert_matchlet (TreeMatch    *match,
                 TreeMatchlet *matchlet,
                 gint          depth)
{
  if (depth == 0)
    match->matches = g_list_append (match->matches, matchlet);
  else
    {
      GList *last;
      TreeMatchlet *m;

      last = g_list_last (match->matches);
      if (!last)
        {
          tree_matchlet_free (matchlet);
          g_warning ("can't insert tree matchlet at depth %d", depth);
          return;
        }

      m = (TreeMatchlet *) last->data;
      while (--depth > 0)
        {
          last = g_list_last (m->matches);
          if (!last)
            {
              tree_matchlet_free (matchlet);
              g_warning ("can't insert tree matchlet at depth %d", depth);
              return;
            }

          m = (TreeMatchlet *) last->data;
        }
      m->matches = g_list_append (m->matches, matchlet);
    }
}

static void
read_tree_magic_from_directory (const gchar *prefix)
{
  gchar *filename;
  gchar *text;
  gsize len;
  gchar **lines;
  gint i;
  TreeMatch *match;
  TreeMatchlet *matchlet;
  gint depth;

  filename = g_build_filename (prefix, "mime", "treemagic", NULL);

  if (g_file_get_contents (filename, &text, &len, NULL))
    {
      if (strcmp (text, "MIME-TreeMagic") == 0)
        {
          lines = g_strsplit (text + strlen ("MIME-TreeMagic") + 2, "\n", 0);
          match = NULL;
          for (i = 0; lines[i] && lines[i][0]; i++)
            {
              if (lines[i][0] == '[')
                {
                  match = parse_header (lines[i]);
                  insert_match (match);
                }
              else if (match != NULL)
                {
                  matchlet = parse_match_line (lines[i], &depth);
                  insert_matchlet (match, matchlet, depth);
                }
              else
                {
                  g_warning ("%s: header corrupt; skipping\n", filename);
                  break;
                }
            }

          g_strfreev (lines);
        }
      else
        g_warning ("%s: header not found, skipping\n", filename);

      g_free (text);
    }

  g_free (filename);
}


static void
xdg_mime_reload (void *user_data)
{
  need_reload = TRUE;
}

static void
tree_magic_shutdown (void)
{
  g_list_free_full (tree_matches, (GDestroyNotify) tree_match_free);
  tree_matches = NULL;
}

static void
tree_magic_init (void)
{
  static gboolean initialized = FALSE;
  const gchar *dir;
  const gchar * const * dirs;
  int i;

  if (!initialized)
    {
      initialized = TRUE;

      xdg_mime_register_reload_callback (xdg_mime_reload, NULL, NULL);
      need_reload = TRUE;
    }

  if (need_reload)
    {
      need_reload = FALSE;

      tree_magic_shutdown ();

      dir = g_get_user_data_dir ();
      read_tree_magic_from_directory (dir);
      dirs = g_get_system_data_dirs ();
      for (i = 0; dirs[i]; i++)
        read_tree_magic_from_directory (dirs[i]);
    }
}

/* a filtering enumerator */

typedef struct
{
  gchar *path;
  gint depth;
  gboolean ignore_case;
  gchar **components;
  gchar **case_components;
  GFileEnumerator **enumerators;
  GFile **children;
} Enumerator;

static gboolean
component_match (Enumerator  *e,
                 gint         depth,
                 const gchar *name)
{
  gchar *case_folded, *key;
  gboolean found;

  if (strcmp (name, e->components[depth]) == 0)
    return TRUE;

  if (!e->ignore_case)
    return FALSE;

  case_folded = g_utf8_casefold (name, -1);
  key = g_utf8_collate_key (case_folded, -1);

  found = strcmp (key, e->case_components[depth]) == 0;

  g_free (case_folded);
  g_free (key);

  return found;
}

static GFile *
next_match_recurse (Enumerator *e,
                    gint        depth)
{
  GFile *file;
  GFileInfo *info;
  const gchar *name;

  while (TRUE)
    {
      if (e->enumerators[depth] == NULL)
        {
          if (depth > 0)
            {
              file = next_match_recurse (e, depth - 1);
              if (file)
                {
                  e->children[depth] = file;
                  e->enumerators[depth] = g_file_enumerate_children (file,
                                                                     G_FILE_ATTRIBUTE_STANDARD_NAME,
                                                                     G_FILE_QUERY_INFO_NONE,
                                                                     NULL,
                                                                     NULL);
                }
            }
          if (e->enumerators[depth] == NULL)
            return NULL;
        }

      while ((info = g_file_enumerator_next_file (e->enumerators[depth], NULL, NULL)))
        {
          name = g_file_info_get_name (info);
          if (component_match (e, depth, name))
            {
              file = g_file_get_child (e->children[depth], name);
              g_object_unref (info);
              return file;
            }
          g_object_unref (info);
        }

      g_object_unref (e->enumerators[depth]);
      e->enumerators[depth] = NULL;
      g_object_unref (e->children[depth]);
      e->children[depth] = NULL;
    }
}

static GFile *
enumerator_next (Enumerator *e)
{
  return next_match_recurse (e, e->depth - 1);
}

static Enumerator *
enumerator_new (GFile      *root,
                const char *path,
                gboolean    ignore_case)
{
  Enumerator *e;
  gint i;
  gchar *case_folded;

  e = g_new0 (Enumerator, 1);
  e->path = g_strdup (path);
  e->ignore_case = ignore_case;

  e->components = g_strsplit (e->path, G_DIR_SEPARATOR_S, -1);
  e->depth = g_strv_length (e->components);
  if (e->ignore_case)
    {
      e->case_components = g_new0 (char *, e->depth + 1);
      for (i = 0; e->components[i]; i++)
        {
          case_folded = g_utf8_casefold (e->components[i], -1);
          e->case_components[i] = g_utf8_collate_key (case_folded, -1);
          g_free (case_folded);
        }
    }

  e->children = g_new0 (GFile *, e->depth);
  e->children[0] = g_object_ref (root);
  e->enumerators = g_new0 (GFileEnumerator *, e->depth);
  e->enumerators[0] = g_file_enumerate_children (root,
                                                 G_FILE_ATTRIBUTE_STANDARD_NAME,
                                                 G_FILE_QUERY_INFO_NONE,
                                                 NULL,
                                                 NULL);

  return e;
}

static void
enumerator_free (Enumerator *e)
{
  gint i;

  for (i = 0; i < e->depth; i++)
    {
      if (e->enumerators[i])
        g_object_unref (e->enumerators[i]);
      if (e->children[i])
        g_object_unref (e->children[i]);
    }

  g_free (e->enumerators);
  g_free (e->children);
  g_strfreev (e->components);
  if (e->case_components)
    g_strfreev (e->case_components);
  g_free (e->path);
  g_free (e);
}

static gboolean
matchlet_match (TreeMatchlet *matchlet,
                GFile        *root)
{
  GFile *file;
  GFileInfo *info;
  gboolean result;
  const gchar *attrs;
  Enumerator *e;
  GList *l;

  e = enumerator_new (root, matchlet->path, !matchlet->match_case);

  do
    {
      file = enumerator_next (e);
      if (!file)
        {
          enumerator_free (e);
          return FALSE;
        }

      if (matchlet->mimetype)
        attrs = G_FILE_ATTRIBUTE_STANDARD_TYPE ","
                G_FILE_ATTRIBUTE_ACCESS_CAN_EXECUTE ","
                G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE;
      else
        attrs = G_FILE_ATTRIBUTE_STANDARD_TYPE ","
                G_FILE_ATTRIBUTE_ACCESS_CAN_EXECUTE;
      info = g_file_query_info (file,
                                attrs,
                                G_FILE_QUERY_INFO_NONE,
                                NULL,
                                NULL);
      if (info)
        {
          result = TRUE;

          if (matchlet->type != G_FILE_TYPE_UNKNOWN &&
              g_file_info_get_file_type (info) != matchlet->type)
            result = FALSE;

          if (matchlet->executable &&
              !g_file_info_get_attribute_boolean (info, G_FILE_ATTRIBUTE_ACCESS_CAN_EXECUTE))
            result = FALSE;
        }
      else
        result = FALSE;

      if (result && matchlet->non_empty)
        {
          GFileEnumerator *child_enum;
          GFileInfo *child_info;

          child_enum = g_file_enumerate_children (file,
                                                  G_FILE_ATTRIBUTE_STANDARD_NAME,
                                                  G_FILE_QUERY_INFO_NONE,
                                                  NULL,
                                                  NULL);

          if (child_enum)
            {
              child_info = g_file_enumerator_next_file (child_enum, NULL, NULL);
              if (child_info)
                g_object_unref (child_info);
              else
                result = FALSE;
              g_object_unref (child_enum);
            }
          else
            result = FALSE;
        }

      if (result && matchlet->mimetype)
        {
          if (strcmp (matchlet->mimetype, g_file_info_get_content_type (info)) != 0)
            result = FALSE;
        }

      if (info)
        g_object_unref (info);
      g_object_unref (file);
    }
  while (!result);

  enumerator_free (e);

  if (!matchlet->matches)
    return TRUE;

  for (l = matchlet->matches; l; l = l->next)
    {
      TreeMatchlet *submatchlet;

      submatchlet = l->data;
      if (matchlet_match (submatchlet, root))
        return TRUE;
    }

  return FALSE;
}

static void
match_match (TreeMatch    *match,
             GFile        *root,
             GPtrArray    *types)
{
  GList *l;

  for (l = match->matches; l; l = l->next)
    {
      TreeMatchlet *matchlet = l->data;
      if (matchlet_match (matchlet, root))
        {
          g_ptr_array_add (types, g_strdup (match->contenttype));
          break;
        }
    }
}

/**
 * g_content_type_guess_for_tree:
 * @root: the root of the tree to guess a type for
 *
 * Tries to guess the type of the tree with root @root, by
 * looking at the files it contains. The result is an array
 * of content types, with the best guess coming first.
 *
 * The types returned all have the form x-content/foo, e.g.
 * x-content/audio-cdda (for audio CDs) or x-content/image-dcf
 * (for a camera memory card). See the
 * [shared-mime-info](http://www.freedesktop.org/wiki/Specifications/shared-mime-info-spec)
 * specification for more on x-content types.
 *
 * This function is useful in the implementation of
 * g_mount_guess_content_type().
 *
 * Returns: (transfer full) (array zero-terminated=1): an %NULL-terminated
 *     array of zero or more content types. Free with g_strfreev()
 *
 * Since: 2.18
 */
gchar **
g_content_type_guess_for_tree (GFile *root)
{
  GPtrArray *types;
  GList *l;

  types = g_ptr_array_new ();

  G_LOCK (gio_treemagic);

  tree_magic_init ();
  for (l = tree_matches; l; l = l->next)
    {
      TreeMatch *match = l->data;
      match_match (match, root, types);
    }

  G_UNLOCK (gio_treemagic);

  g_ptr_array_add (types, NULL);

  return (gchar **)g_ptr_array_free (types, FALSE);
}
