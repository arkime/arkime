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
#include <stdlib.h>
#include <string.h>

#include "gicon.h"
#include "gthemedicon.h"
#include "gfileicon.h"
#include "gemblemedicon.h"
#include "gbytesicon.h"
#include "gfile.h"
#include "gioerror.h"
#include "gioenumtypes.h"
#include "gvfs.h"

#include "glibintl.h"


/* There versioning of this is implicit, version 1 would be ".1 " */
#define G_ICON_SERIALIZATION_MAGIC0 ". "

/**
 * SECTION:gicon
 * @short_description: Interface for icons
 * @include: gio/gio.h
 *
 * #GIcon is a very minimal interface for icons. It provides functions
 * for checking the equality of two icons, hashing of icons and
 * serializing an icon to and from strings.
 *
 * #GIcon does not provide the actual pixmap for the icon as this is out 
 * of GIO's scope, however implementations of #GIcon may contain the name 
 * of an icon (see #GThemedIcon), or the path to an icon (see #GLoadableIcon). 
 *
 * To obtain a hash of a #GIcon, see g_icon_hash().
 *
 * To check if two #GIcons are equal, see g_icon_equal().
 *
 * For serializing a #GIcon, use g_icon_serialize() and
 * g_icon_deserialize().
 *
 * If you want to consume #GIcon (for example, in a toolkit) you must
 * be prepared to handle at least the three following cases:
 * #GLoadableIcon, #GThemedIcon and #GEmblemedIcon.  It may also make
 * sense to have fast-paths for other cases (like handling #GdkPixbuf
 * directly, for example) but all compliant #GIcon implementations
 * outside of GIO must implement #GLoadableIcon.
 *
 * If your application or library provides one or more #GIcon
 * implementations you need to ensure that your new implementation also
 * implements #GLoadableIcon.  Additionally, you must provide an
 * implementation of g_icon_serialize() that gives a result that is
 * understood by g_icon_deserialize(), yielding one of the built-in icon
 * types.
 **/

typedef GIconIface GIconInterface;
G_DEFINE_INTERFACE(GIcon, g_icon, G_TYPE_OBJECT)

static void
g_icon_default_init (GIconInterface *iface)
{
}

/**
 * g_icon_hash:
 * @icon: (not nullable): #gconstpointer to an icon object.
 * 
 * Gets a hash for an icon.
 *
 * Virtual: hash
 * Returns: a #guint containing a hash for the @icon, suitable for 
 * use in a #GHashTable or similar data structure.
 **/
guint
g_icon_hash (gconstpointer icon)
{
  GIconIface *iface;

  g_return_val_if_fail (G_IS_ICON (icon), 0);

  iface = G_ICON_GET_IFACE (icon);

  return (* iface->hash) ((GIcon *)icon);
}

/**
 * g_icon_equal:
 * @icon1: (nullable): pointer to the first #GIcon.
 * @icon2: (nullable): pointer to the second #GIcon.
 * 
 * Checks if two icons are equal.
 * 
 * Returns: %TRUE if @icon1 is equal to @icon2. %FALSE otherwise.
 **/
gboolean
g_icon_equal (GIcon *icon1,
	      GIcon *icon2)
{
  GIconIface *iface;

  if (icon1 == NULL && icon2 == NULL)
    return TRUE;

  if (icon1 == NULL || icon2 == NULL)
    return FALSE;
  
  if (G_TYPE_FROM_INSTANCE (icon1) != G_TYPE_FROM_INSTANCE (icon2))
    return FALSE;

  iface = G_ICON_GET_IFACE (icon1);
  
  return (* iface->equal) (icon1, icon2);
}

static gboolean
g_icon_to_string_tokenized (GIcon *icon, GString *s)
{
  GPtrArray *tokens;
  gint version;
  GIconIface *icon_iface;
  int i;

  g_return_val_if_fail (icon != NULL, FALSE);
  g_return_val_if_fail (G_IS_ICON (icon), FALSE);

  icon_iface = G_ICON_GET_IFACE (icon);
  if (icon_iface->to_tokens == NULL)
    return FALSE;

  tokens = g_ptr_array_new ();
  if (!icon_iface->to_tokens (icon, tokens, &version))
    {
      g_ptr_array_free (tokens, TRUE);
      return FALSE;
    }

  /* format: TypeName[.Version] <token_0> .. <token_N-1>
     version 0 is implicit and can be omitted
     all the tokens are url escaped to ensure they have no spaces in them */
  
  g_string_append (s, g_type_name_from_instance ((GTypeInstance *)icon));
  if (version != 0)
    g_string_append_printf (s, ".%d", version);
  
  for (i = 0; i < tokens->len; i++)
    {
      char *token;

      token = g_ptr_array_index (tokens, i);

      g_string_append_c (s, ' ');
      /* We really only need to escape spaces here, so allow lots of otherwise reserved chars */
      g_string_append_uri_escaped (s, token,
				   G_URI_RESERVED_CHARS_ALLOWED_IN_PATH, TRUE);

      g_free (token);
    }
  
  g_ptr_array_free (tokens, TRUE);
  
  return TRUE;
}

/**
 * g_icon_to_string:
 * @icon: a #GIcon.
 *
 * Generates a textual representation of @icon that can be used for
 * serialization such as when passing @icon to a different process or
 * saving it to persistent storage. Use g_icon_new_for_string() to
 * get @icon back from the returned string.
 *
 * The encoding of the returned string is proprietary to #GIcon except
 * in the following two cases
 *
 * - If @icon is a #GFileIcon, the returned string is a native path
 *   (such as `/path/to/my icon.png`) without escaping
 *   if the #GFile for @icon is a native file.  If the file is not
 *   native, the returned string is the result of g_file_get_uri()
 *   (such as `sftp://path/to/my%20icon.png`).
 * 
 * - If @icon is a #GThemedIcon with exactly one name, the encoding is
 *    simply the name (such as `network-server`).
 *
 * Virtual: to_tokens
 * Returns: (nullable): An allocated NUL-terminated UTF8 string or
 * %NULL if @icon can't be serialized. Use g_free() to free.
 *
 * Since: 2.20
 */
gchar *
g_icon_to_string (GIcon *icon)
{
  gchar *ret;

  g_return_val_if_fail (icon != NULL, NULL);
  g_return_val_if_fail (G_IS_ICON (icon), NULL);

  ret = NULL;

  if (G_IS_FILE_ICON (icon))
    {
      GFile *file;

      file = g_file_icon_get_file (G_FILE_ICON (icon));
      if (g_file_is_native (file))
	{
	  ret = g_file_get_path (file);
	  if (!g_utf8_validate (ret, -1, NULL))
	    {
	      g_free (ret);
	      ret = NULL;
	    }
	}
      else
        ret = g_file_get_uri (file);
    }
  else if (G_IS_THEMED_ICON (icon))
    {
      const char * const *names;

      names = g_themed_icon_get_names (G_THEMED_ICON (icon));
      if (names != NULL &&
	  names[0] != NULL &&
	  names[0][0] != '.' && /* Allowing icons starting with dot would break G_ICON_SERIALIZATION_MAGIC0 */
	  g_utf8_validate (names[0], -1, NULL) && /* Only return utf8 strings */
	  names[1] == NULL)
	ret = g_strdup (names[0]);
    }

  if (ret == NULL)
    {
      GString *s;

      s = g_string_new (G_ICON_SERIALIZATION_MAGIC0);

      if (g_icon_to_string_tokenized (icon, s))
	ret = g_string_free (s, FALSE);
      else
	g_string_free (s, TRUE);
    }

  return ret;
}

static GIcon *
g_icon_new_from_tokens (char   **tokens,
			GError **error)
{
  GIcon *icon;
  char *typename, *version_str;
  GType type;
  gpointer klass;
  GIconIface *icon_iface;
  gint version;
  char *endp;
  int num_tokens;
  int i;

  icon = NULL;
  klass = NULL;

  num_tokens = g_strv_length (tokens);

  if (num_tokens < 1)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_ARGUMENT,
                   _("Wrong number of tokens (%d)"),
                   num_tokens);
      goto out;
    }
  
  typename = tokens[0];
  version_str = strchr (typename, '.');
  if (version_str)
    {
      *version_str = 0;
      version_str += 1;
    }
  
  
  type = g_type_from_name (tokens[0]);
  if (type == 0)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_ARGUMENT,
                   _("No type for class name %s"),
                   tokens[0]);
      goto out;
    }

  if (!g_type_is_a (type, G_TYPE_ICON))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_ARGUMENT,
                   _("Type %s does not implement the GIcon interface"),
                   tokens[0]);
      goto out;
    }

  klass = g_type_class_ref (type);
  if (klass == NULL)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_ARGUMENT,
                   _("Type %s is not classed"),
                   tokens[0]);
      goto out;
    }

  version = 0;
  if (version_str)
    {
      version = strtol (version_str, &endp, 10);
      if (endp == NULL || *endp != '\0')
	{
	  g_set_error (error,
		       G_IO_ERROR,
		       G_IO_ERROR_INVALID_ARGUMENT,
		       _("Malformed version number: %s"),
		       version_str);
	  goto out;
	}
    }

  icon_iface = g_type_interface_peek (klass, G_TYPE_ICON);
  g_assert (icon_iface != NULL);

  if (icon_iface->from_tokens == NULL)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_ARGUMENT,
                   _("Type %s does not implement from_tokens() on the GIcon interface"),
                   tokens[0]);
      goto out;
    }

  for (i = 1;  i < num_tokens; i++)
    {
      char *escaped;

      escaped = tokens[i];
      tokens[i] = g_uri_unescape_string (escaped, NULL);
      g_free (escaped);
    }
  
  icon = icon_iface->from_tokens (tokens + 1, num_tokens - 1, version, error);

 out:
  if (klass != NULL)
    g_type_class_unref (klass);
  return icon;
}

static void
ensure_builtin_icon_types (void)
{
  g_type_ensure (G_TYPE_THEMED_ICON);
  g_type_ensure (G_TYPE_FILE_ICON);
  g_type_ensure (G_TYPE_EMBLEMED_ICON);
  g_type_ensure (G_TYPE_EMBLEM);
}

/* handles the 'simple' cases: GFileIcon and GThemedIcon */
static GIcon *
g_icon_new_for_string_simple (const gchar *str)
{
  gchar *scheme;
  GIcon *icon;

  if (str[0] == '.')
    return NULL;

  /* handle special GFileIcon and GThemedIcon cases */
  scheme = g_uri_parse_scheme (str);
  if (scheme != NULL || str[0] == '/' || str[0] == G_DIR_SEPARATOR)
    {
      GFile *location;
      location = g_file_new_for_commandline_arg (str);
      icon = g_file_icon_new (location);
      g_object_unref (location);
    }
  else
    icon = g_themed_icon_new (str);

  g_free (scheme);

  return icon;
}

/**
 * g_icon_new_for_string:
 * @str: A string obtained via g_icon_to_string().
 * @error: Return location for error.
 *
 * Generate a #GIcon instance from @str. This function can fail if
 * @str is not valid - see g_icon_to_string() for discussion.
 *
 * If your application or library provides one or more #GIcon
 * implementations you need to ensure that each #GType is registered
 * with the type system prior to calling g_icon_new_for_string().
 *
 * Returns: (transfer full): An object implementing the #GIcon
 *          interface or %NULL if @error is set.
 *
 * Since: 2.20
 **/
GIcon *
g_icon_new_for_string (const gchar   *str,
                       GError       **error)
{
  GIcon *icon = NULL;

  g_return_val_if_fail (str != NULL, NULL);

  icon = g_icon_new_for_string_simple (str);
  if (icon)
    return icon;

  ensure_builtin_icon_types ();

  if (g_str_has_prefix (str, G_ICON_SERIALIZATION_MAGIC0))
    {
      gchar **tokens;

      /* handle tokenized encoding */
      tokens = g_strsplit (str + sizeof (G_ICON_SERIALIZATION_MAGIC0) - 1, " ", 0);
      icon = g_icon_new_from_tokens (tokens, error);
      g_strfreev (tokens);
    }
  else
    g_set_error_literal (error,
                         G_IO_ERROR,
                         G_IO_ERROR_INVALID_ARGUMENT,
                         _("Can’t handle the supplied version of the icon encoding"));

  return icon;
}

static GEmblem *
g_icon_deserialize_emblem (GVariant *value)
{
  GVariant *emblem_metadata;
  GVariant *emblem_data;
  const gchar *origin_nick;
  GIcon *emblem_icon;
  GEmblem *emblem;

  g_variant_get (value, "(v@a{sv})", &emblem_data, &emblem_metadata);

  emblem = NULL;

  emblem_icon = g_icon_deserialize (emblem_data);
  if (emblem_icon != NULL)
    {
      /* Check if we should create it with an origin. */
      if (g_variant_lookup (emblem_metadata, "origin", "&s", &origin_nick))
        {
          GEnumClass *origin_class;
          GEnumValue *origin_value;

          origin_class = g_type_class_ref (G_TYPE_EMBLEM_ORIGIN);
          origin_value = g_enum_get_value_by_nick (origin_class, origin_nick);
          if (origin_value)
            emblem = g_emblem_new_with_origin (emblem_icon, origin_value->value);
          g_type_class_unref (origin_class);
        }

      /* We didn't create it with an origin, so do it without. */
      if (emblem == NULL)
        emblem = g_emblem_new (emblem_icon);

      g_object_unref (emblem_icon);
    }

  g_variant_unref (emblem_metadata);
  g_variant_unref (emblem_data);

  return emblem;
}

static GIcon *
g_icon_deserialize_emblemed (GVariant *value)
{
  GVariantIter *emblems;
  GVariant *icon_data;
  GIcon *main_icon;
  GIcon *icon;

  g_variant_get (value, "(va(va{sv}))", &icon_data, &emblems);
  main_icon = g_icon_deserialize (icon_data);

  if (main_icon)
    {
      GVariant *emblem_data;

      icon = g_emblemed_icon_new (main_icon, NULL);

      while ((emblem_data = g_variant_iter_next_value (emblems)))
        {
          GEmblem *emblem;

          emblem = g_icon_deserialize_emblem (emblem_data);

          if (emblem)
            {
              g_emblemed_icon_add_emblem (G_EMBLEMED_ICON (icon), emblem);
              g_object_unref (emblem);
            }

          g_variant_unref (emblem_data);
        }

      g_object_unref (main_icon);
    }
  else
    icon = NULL;

  g_variant_iter_free (emblems);
  g_variant_unref (icon_data);

  return icon;
}

/**
 * g_icon_deserialize:
 * @value: a #GVariant created with g_icon_serialize()
 *
 * Deserializes a #GIcon previously serialized using g_icon_serialize().
 *
 * Returns: (transfer full): a #GIcon, or %NULL when deserialization fails.
 *
 * Since: 2.38
 */
GIcon *
g_icon_deserialize (GVariant *value)
{
  const gchar *tag;
  GVariant *val;
  GIcon *icon;

  g_return_val_if_fail (value != NULL, NULL);
  g_return_val_if_fail (g_variant_is_of_type (value, G_VARIANT_TYPE_STRING) ||
                        g_variant_is_of_type (value, G_VARIANT_TYPE ("(sv)")), NULL);

  /* Handle some special cases directly so that people can hard-code
   * stuff into GMenuModel xml files without resorting to using GVariant
   * text format to describe one of the explicitly-tagged possibilities
   * below.
   */
  if (g_variant_is_of_type (value, G_VARIANT_TYPE_STRING))
    return g_icon_new_for_string_simple (g_variant_get_string (value, NULL));

  /* Otherwise, use the tagged union format */
  g_variant_get (value, "(&sv)", &tag, &val);

  icon = NULL;

  if (g_str_equal (tag, "file") && g_variant_is_of_type (val, G_VARIANT_TYPE_STRING))
    {
      GFile *file;

      file = g_file_new_for_commandline_arg (g_variant_get_string (val, NULL));
      icon = g_file_icon_new (file);
      g_object_unref (file);
    }
  else if (g_str_equal (tag, "themed") && g_variant_is_of_type (val, G_VARIANT_TYPE_STRING_ARRAY))
    {
      const gchar **names;
      gsize size;

      names = g_variant_get_strv (val, &size);
      icon = g_themed_icon_new_from_names ((gchar **) names, size);
      g_free (names);
    }
  else if (g_str_equal (tag, "bytes") && g_variant_is_of_type (val, G_VARIANT_TYPE_BYTESTRING))
    {
      GBytes *bytes;

      bytes = g_variant_get_data_as_bytes (val);
      icon = g_bytes_icon_new (bytes);
      g_bytes_unref (bytes);
    }
  else if (g_str_equal (tag, "emblem") && g_variant_is_of_type (val, G_VARIANT_TYPE ("(va{sv})")))
    {
      GEmblem *emblem;

      emblem = g_icon_deserialize_emblem (val);
      if (emblem)
        icon = G_ICON (emblem);
    }
  else if (g_str_equal (tag, "emblemed") && g_variant_is_of_type (val, G_VARIANT_TYPE ("(va(va{sv}))")))
    {
      icon = g_icon_deserialize_emblemed (val);
    }
  else if (g_str_equal (tag, "gvfs"))
    {
      GVfsClass *class;
      GVfs *vfs;

      vfs = g_vfs_get_default ();
      class = G_VFS_GET_CLASS (vfs);
      if (class->deserialize_icon)
        icon = (* class->deserialize_icon) (vfs, val);
    }

  g_variant_unref (val);

  return icon;
}

/**
 * g_icon_serialize:
 * @icon: a #GIcon
 *
 * Serializes a #GIcon into a #GVariant. An equivalent #GIcon can be retrieved
 * back by calling g_icon_deserialize() on the returned value.
 * As serialization will avoid using raw icon data when possible, it only
 * makes sense to transfer the #GVariant between processes on the same machine,
 * (as opposed to over the network), and within the same file system namespace.
 *
 * Returns: (transfer full): a #GVariant, or %NULL when serialization fails.
 *
 * Since: 2.38
 */
GVariant *
g_icon_serialize (GIcon *icon)
{
  GIconInterface *iface;
  GVariant *result;

  iface = G_ICON_GET_IFACE (icon);

  if (!iface->serialize)
    {
      g_critical ("g_icon_serialize() on icon type '%s' is not implemented", G_OBJECT_TYPE_NAME (icon));
      return NULL;
    }

  result = (* iface->serialize) (icon);

  if (result)
    {
      g_variant_take_ref (result);

      if (!g_variant_is_of_type (result, G_VARIANT_TYPE ("(sv)")))
        {
          g_critical ("g_icon_serialize() on icon type '%s' returned GVariant of type '%s' but it must return "
                      "one with type '(sv)'", G_OBJECT_TYPE_NAME (icon), g_variant_get_type_string (result));
          g_variant_unref (result);
          result = NULL;
        }
    }

  return result;
}
