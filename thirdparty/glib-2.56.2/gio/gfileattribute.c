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

#include <string.h>

#include "gfileattribute.h"
#include "gfileattribute-priv.h"
#include <glib-object.h>
#include "glibintl.h"


/**
 * SECTION:gfileattribute
 * @short_description: Key-Value Paired File Attributes
 * @include: gio/gio.h
 * @see_also: #GFile, #GFileInfo
 *
 * File attributes in GIO consist of a list of key-value pairs.
 *
 * Keys are strings that contain a key namespace and a key name, separated
 * by a colon, e.g. "namespace::keyname". Namespaces are included to sort
 * key-value pairs by namespaces for relevance. Keys can be retrived
 * using wildcards, e.g. "standard::*" will return all of the keys in the
 * "standard" namespace.
 *
 * The list of possible attributes for a filesystem (pointed to by a #GFile) is
 * available as a #GFileAttributeInfoList. This list is queryable by key names
 * as indicated earlier.
 *
 * Information is stored within the list in #GFileAttributeInfo structures.
 * The info structure can store different types, listed in the enum
 * #GFileAttributeType. Upon creation of a #GFileAttributeInfo, the type will
 * be set to %G_FILE_ATTRIBUTE_TYPE_INVALID.
 *
 * Classes that implement #GFileIface will create a #GFileAttributeInfoList and
 * install default keys and values for their given file system, architecture,
 * and other possible implementation details (e.g., on a UNIX system, a file
 * attribute key will be registered for the user id for a given file).
 *
 * ## Default Namespaces
 *
 * - `"standard"`: The "Standard" namespace. General file information that
 *   any application may need should be put in this namespace. Examples
 *   include the file's name, type, and size.
 * - `"etag`: The [Entity Tag][gfile-etag] namespace. Currently, the only key
 *   in this namespace is "value", which contains the value of the current
 *   entity tag.
 * - `"id"`: The "Identification" namespace. This namespace is used by file
 *   managers and applications that list directories to check for loops and
 *   to uniquely identify files.
 * - `"access"`: The "Access" namespace. Used to check if a user has the
 *   proper privileges to access files and perform file operations. Keys in
 *   this namespace are made to be generic and easily understood, e.g. the
 *   "can_read" key is %TRUE if the current user has permission to read the
 *   file. UNIX permissions and NTFS ACLs in Windows should be mapped to
 *   these values.
 * - `"mountable"`: The "Mountable" namespace. Includes simple boolean keys
 *   for checking if a file or path supports mount operations, e.g. mount,
 *   unmount, eject. These are used for files of type %G_FILE_TYPE_MOUNTABLE.
 * - `"time"`: The "Time" namespace. Includes file access, changed, created
 *   times.
 * - `"unix"`: The "Unix" namespace. Includes UNIX-specific information and
 *   may not be available for all files. Examples include the UNIX "UID",
 *   "GID", etc.
 * - `"dos"`: The "DOS" namespace. Includes DOS-specific information and may
 *   not be available for all files. Examples include "is_system" for checking
 *   if a file is marked as a system file, and "is_archive" for checking if a
 *   file is marked as an archive file.
 * - `"owner"`: The "Owner" namespace. Includes information about who owns a
 *   file. May not be available for all file systems. Examples include "user"
 *   for getting the user name of the file owner. This information is often
 *   mapped from some backend specific data such as a UNIX UID.
 * - `"thumbnail"`: The "Thumbnail" namespace. Includes information about file
 *   thumbnails and their location within the file system. Examples of keys in
 *   this namespace include "path" to get the location of a thumbnail, "failed"
 *   to check if thumbnailing of the file failed, and "is-valid" to check if
 *   the thumbnail is outdated.
 * - `"filesystem"`: The "Filesystem" namespace. Gets information about the
 *   file system where a file is located, such as its type, how much space is
 *   left available, and the overall size of the file system.
 * - `"gvfs"`: The "GVFS" namespace. Keys in this namespace contain information
 *   about the current GVFS backend in use.
 * - `"xattr"`: The "xattr" namespace. Gets information about extended
 *   user attributes. See attr(5). The "user." prefix of the extended user
 *   attribute name is stripped away when constructing keys in this namespace,
 *   e.g. "xattr::mime_type" for the extended attribute with the name
 *   "user.mime_type". Note that this information is only available if
 *   GLib has been built with extended attribute support.
 * - `"xattr-sys"`: The "xattr-sys" namespace. Gets information about
 *   extended attributes which are not user-specific. See attr(5). Note
 *   that this information is only available if GLib has been built with
 *   extended attribute support.
 * - `"selinux"`: The "SELinux" namespace. Includes information about the
 *   SELinux context of files. Note that this information is only available
 *   if GLib has been built with SELinux support.
 *
 * Please note that these are not all of the possible namespaces.
 * More namespaces can be added from GIO modules or by individual applications.
 * For more information about writing GIO modules, see #GIOModule.
 *
 * <!-- TODO: Implementation note about using extended attributes on supported
 * file systems -->
 *
 * ## Default Keys
 *
 * For a list of the built-in keys and their types, see the
 * [GFileInfo][GFileInfo] documentation.
 *
 * Note that there are no predefined keys in the "xattr" and "xattr-sys"
 * namespaces. Keys for the "xattr" namespace are constructed by stripping
 * away the "user." prefix from the extended user attribute, and prepending
 * "xattr::". Keys for the "xattr-sys" namespace are constructed by
 * concatenating "xattr-sys::" with the extended attribute name. All extended
 * attribute values are returned as hex-encoded strings in which bytes outside
 * the ASCII range are encoded as escape sequences of the form \x`nn`
 * where `nn` is a 2-digit hexadecimal number.
 */

/**
 * _g_file_attribute_value_free:
 * @attr: a #GFileAttributeValue.
 *
 * Frees the memory used by @attr.
 *
 **/
void
_g_file_attribute_value_free (GFileAttributeValue *attr)
{
  g_return_if_fail (attr != NULL);

  _g_file_attribute_value_clear (attr);
  g_free (attr);
}

/**
 * _g_file_attribute_value_clear:
 * @attr: a #GFileAttributeValue.
 *
 * Clears the value of @attr and sets its type to
 * %G_FILE_ATTRIBUTE_TYPE_INVALID.
 *
 **/
void
_g_file_attribute_value_clear (GFileAttributeValue *attr)
{
  g_return_if_fail (attr != NULL);

  if (attr->type == G_FILE_ATTRIBUTE_TYPE_STRING ||
      attr->type == G_FILE_ATTRIBUTE_TYPE_BYTE_STRING)
    g_free (attr->u.string);

  if (attr->type == G_FILE_ATTRIBUTE_TYPE_STRINGV)
    g_strfreev (attr->u.stringv);

  if (attr->type == G_FILE_ATTRIBUTE_TYPE_OBJECT &&
      attr->u.obj != NULL)
    g_object_unref (attr->u.obj);

  attr->type = G_FILE_ATTRIBUTE_TYPE_INVALID;
}

/**
 * g_file_attribute_value_set:
 * @attr: a #GFileAttributeValue to set the value in.
 * @new_value: a #GFileAttributeValue to get the value from.
 *
 * Sets an attribute's value from another attribute.
 **/
void
_g_file_attribute_value_set (GFileAttributeValue        *attr,
			     const GFileAttributeValue *new_value)
{
  g_return_if_fail (attr != NULL);
  g_return_if_fail (new_value != NULL);

  _g_file_attribute_value_clear (attr);
  *attr = *new_value;

  if (attr->type == G_FILE_ATTRIBUTE_TYPE_STRING ||
      attr->type == G_FILE_ATTRIBUTE_TYPE_BYTE_STRING)
    attr->u.string = g_strdup (attr->u.string);

  if (attr->type == G_FILE_ATTRIBUTE_TYPE_STRINGV)
    attr->u.stringv = g_strdupv (attr->u.stringv);

  if (attr->type == G_FILE_ATTRIBUTE_TYPE_OBJECT &&
      attr->u.obj != NULL)
    g_object_ref (attr->u.obj);
}

/**
 * _g_file_attribute_value_new:
 *
 * Creates a new file attribute.
 *
 * Returns: a #GFileAttributeValue.
 **/
GFileAttributeValue *
_g_file_attribute_value_new (void)
{
  GFileAttributeValue *attr;

  attr = g_new (GFileAttributeValue, 1);
  attr->type = G_FILE_ATTRIBUTE_TYPE_INVALID;
  return attr;
}

gpointer
_g_file_attribute_value_peek_as_pointer (GFileAttributeValue *attr)
{
  switch (attr->type) {
  case G_FILE_ATTRIBUTE_TYPE_STRING:
  case G_FILE_ATTRIBUTE_TYPE_BYTE_STRING:
    return attr->u.string;
  case G_FILE_ATTRIBUTE_TYPE_STRINGV:
    return attr->u.stringv;
  case G_FILE_ATTRIBUTE_TYPE_OBJECT:
    return attr->u.obj;
  default:
    return (gpointer) &attr->u;
  }
}

/**
 * g_file_attribute_value_dup:
 * @other: a #GFileAttributeValue to duplicate.
 *
 * Duplicates a file attribute.
 *
 * Returns: a duplicate of the @other.
 **/
GFileAttributeValue *
_g_file_attribute_value_dup (const GFileAttributeValue *other)
{
  GFileAttributeValue *attr;

  g_return_val_if_fail (other != NULL, NULL);

  attr = g_new (GFileAttributeValue, 1);
  attr->type = G_FILE_ATTRIBUTE_TYPE_INVALID;
  _g_file_attribute_value_set (attr, other);
  return attr;
}

G_DEFINE_BOXED_TYPE (GFileAttributeInfoList, g_file_attribute_info_list,
                     g_file_attribute_info_list_dup,
                     g_file_attribute_info_list_unref)

static gboolean
valid_char (char c)
{
  return c >= 32 && c <= 126 && c != '\\';
}

static char *
escape_byte_string (const char *str)
{
  size_t len;
  int num_invalid, i;
  char *escaped_val, *p;
  unsigned char c;
  const char hex_digits[] = "0123456789abcdef";

  len = strlen (str);

  num_invalid = 0;
  for (i = 0; i < len; i++)
    {
      if (!valid_char (str[i]))
	num_invalid++;
    }

  if (num_invalid == 0)
    return g_strdup (str);
  else
    {
      escaped_val = g_malloc (len + num_invalid*3 + 1);

      p = escaped_val;
      for (i = 0; i < len; i++)
	{
	  c = str[i];
	  if (valid_char (c))
	    *p++ = c;
	  else
	    {
	      *p++ = '\\';
	      *p++ = 'x';
	      *p++ = hex_digits[(c >> 4) & 0xf];
	      *p++ = hex_digits[c & 0xf];
	    }
	}
      *p++ = 0;
      return escaped_val;
    }
}

/**
 * _g_file_attribute_value_as_string:
 * @attr: a #GFileAttributeValue.
 *
 * Converts a #GFileAttributeValue to a string for display.
 * The returned string should be freed when no longer needed.
 *
 * Returns: a string from the @attr, %NULL on error, or "<invalid>"
 * if @attr is of type %G_FILE_ATTRIBUTE_TYPE_INVALID.
 */
char *
_g_file_attribute_value_as_string (const GFileAttributeValue *attr)
{
  GString *s;
  int i;
  char *str;

  g_return_val_if_fail (attr != NULL, NULL);

  switch (attr->type)
    {
    case G_FILE_ATTRIBUTE_TYPE_STRING:
      str = g_strdup (attr->u.string);
      break;
    case G_FILE_ATTRIBUTE_TYPE_STRINGV:
      s = g_string_new ("[");
      for (i = 0; attr->u.stringv[i] != NULL; i++)
	{
	  g_string_append (s, attr->u.stringv[i]);
	  if (attr->u.stringv[i+1] != NULL)
	    g_string_append (s, ", ");
	}
      g_string_append (s, "]");
      str = g_string_free (s, FALSE);
      break;
    case G_FILE_ATTRIBUTE_TYPE_BYTE_STRING:
      str = escape_byte_string (attr->u.string);
      break;
    case G_FILE_ATTRIBUTE_TYPE_BOOLEAN:
      str = g_strdup_printf ("%s", attr->u.boolean?"TRUE":"FALSE");
      break;
    case G_FILE_ATTRIBUTE_TYPE_UINT32:
      str = g_strdup_printf ("%u", (unsigned int)attr->u.uint32);
      break;
    case G_FILE_ATTRIBUTE_TYPE_INT32:
      str = g_strdup_printf ("%i", (int)attr->u.int32);
      break;
    case G_FILE_ATTRIBUTE_TYPE_UINT64:
      str = g_strdup_printf ("%"G_GUINT64_FORMAT, attr->u.uint64);
      break;
    case G_FILE_ATTRIBUTE_TYPE_INT64:
      str = g_strdup_printf ("%"G_GINT64_FORMAT, attr->u.int64);
      break;
    case G_FILE_ATTRIBUTE_TYPE_OBJECT:
      str = g_strdup_printf ("%s:%p", g_type_name_from_instance
                                          ((GTypeInstance *) attr->u.obj),
                                      attr->u.obj);
      break;
    case G_FILE_ATTRIBUTE_TYPE_INVALID:
      str = g_strdup ("<unset>");
      break;
    default:
      g_warning ("Invalid type in GFileInfo attribute");
      str = g_strdup ("<invalid>");
      break;
    }

  return str;
}

/**
 * _g_file_attribute_value_get_string:
 * @attr: a #GFileAttributeValue.
 *
 * Gets the string from a file attribute value. If the value is not the
 * right type then %NULL will be returned.
 *
 * Returns: the UTF-8 string value contained within the attribute, or %NULL.
 */
const char *
_g_file_attribute_value_get_string (const GFileAttributeValue *attr)
{
  if (attr == NULL)
    return NULL;

  g_return_val_if_fail (attr->type == G_FILE_ATTRIBUTE_TYPE_STRING, NULL);

  return attr->u.string;
}

/**
 * _g_file_attribute_value_get_byte_string:
 * @attr: a #GFileAttributeValue.
 *
 * Gets the byte string from a file attribute value. If the value is not the
 * right type then %NULL will be returned.
 *
 * Returns: the byte string contained within the attribute or %NULL.
 */
const char *
_g_file_attribute_value_get_byte_string (const GFileAttributeValue *attr)
{
  if (attr == NULL)
    return NULL;

  g_return_val_if_fail (attr->type == G_FILE_ATTRIBUTE_TYPE_BYTE_STRING, NULL);

  return attr->u.string;
}

char **
_g_file_attribute_value_get_stringv (const GFileAttributeValue *attr)
{
  if (attr == NULL)
    return NULL;

  g_return_val_if_fail (attr->type == G_FILE_ATTRIBUTE_TYPE_STRINGV, NULL);

  return attr->u.stringv;
}

/**
 * _g_file_attribute_value_get_boolean:
 * @attr: a #GFileAttributeValue.
 *
 * Gets the boolean value from a file attribute value. If the value is not the
 * right type then %FALSE will be returned.
 *
 * Returns: the boolean value contained within the attribute, or %FALSE.
 */
gboolean
_g_file_attribute_value_get_boolean (const GFileAttributeValue *attr)
{
  if (attr == NULL)
    return FALSE;

  g_return_val_if_fail (attr->type == G_FILE_ATTRIBUTE_TYPE_BOOLEAN, FALSE);

  return attr->u.boolean;
}

/**
 * _g_file_attribute_value_get_uint32:
 * @attr: a #GFileAttributeValue.
 *
 * Gets the unsigned 32-bit integer from a file attribute value. If the value
 * is not the right type then 0 will be returned.
 *
 * Returns: the unsigned 32-bit integer from the attribute, or 0.
 */
guint32
_g_file_attribute_value_get_uint32 (const GFileAttributeValue *attr)
{
  if (attr == NULL)
    return 0;

  g_return_val_if_fail (attr->type == G_FILE_ATTRIBUTE_TYPE_UINT32, 0);

  return attr->u.uint32;
}

/**
 * _g_file_attribute_value_get_int32:
 * @attr: a #GFileAttributeValue.
 *
 * Gets the signed 32-bit integer from a file attribute value. If the value
 * is not the right type then 0 will be returned.
 *
 * Returns: the signed 32-bit integer from the attribute, or 0.
 */
gint32
_g_file_attribute_value_get_int32 (const GFileAttributeValue *attr)
{
  if (attr == NULL)
    return 0;

  g_return_val_if_fail (attr->type == G_FILE_ATTRIBUTE_TYPE_INT32, 0);

  return attr->u.int32;
}

/**
 * _g_file_attribute_value_get_uint64:
 * @attr: a #GFileAttributeValue.
 *
 * Gets the unsigned 64-bit integer from a file attribute value. If the value
 * is not the right type then 0 will be returned.
 *
 * Returns: the unsigned 64-bit integer from the attribute, or 0.
 */
guint64
_g_file_attribute_value_get_uint64 (const GFileAttributeValue *attr)
{
  if (attr == NULL)
    return 0;

  g_return_val_if_fail (attr->type == G_FILE_ATTRIBUTE_TYPE_UINT64, 0);

  return attr->u.uint64;
}

/**
 * _g_file_attribute_value_get_int64:
 * @attr: a #GFileAttributeValue.
 *
 * Gets the signed 64-bit integer from a file attribute value. If the value
 * is not the right type then 0 will be returned.
 *
 * Returns: the signed 64-bit integer from the attribute, or 0.
 */
gint64
_g_file_attribute_value_get_int64 (const GFileAttributeValue *attr)
{
  if (attr == NULL)
    return 0;

  g_return_val_if_fail (attr->type == G_FILE_ATTRIBUTE_TYPE_INT64, 0);

  return attr->u.int64;
}

/**
 * _g_file_attribute_value_get_object:
 * @attr: a #GFileAttributeValue.
 *
 * Gets the GObject from a file attribute value. If the value
 * is not the right type then %NULL will be returned.
 *
 * Returns: the GObject from the attribute, or %NULL.
 **/
GObject *
_g_file_attribute_value_get_object (const GFileAttributeValue *attr)
{
  if (attr == NULL)
    return NULL;

  g_return_val_if_fail (attr->type == G_FILE_ATTRIBUTE_TYPE_OBJECT, NULL);

  return attr->u.obj;
}


void
_g_file_attribute_value_set_from_pointer (GFileAttributeValue *value,
					  GFileAttributeType   type,
					  gpointer             value_p,
					  gboolean             dup)
{
  _g_file_attribute_value_clear (value);
  value->type = type;
  switch (type)
    {
    case G_FILE_ATTRIBUTE_TYPE_STRING:
    case G_FILE_ATTRIBUTE_TYPE_BYTE_STRING:
      if (dup)
	value->u.string = g_strdup (value_p);
      else
	value->u.string = value_p;
      break;

    case G_FILE_ATTRIBUTE_TYPE_STRINGV:
      if (dup)
	value->u.stringv = g_strdupv (value_p);
      else
	value->u.stringv = value_p;
      break;

    case G_FILE_ATTRIBUTE_TYPE_OBJECT:
      if (dup)
	value->u.obj = g_object_ref (value_p);
      else
	value->u.obj = value_p;
      break;

    case G_FILE_ATTRIBUTE_TYPE_BOOLEAN:
      value->u.boolean = *(gboolean *)value_p;
      break;

    case G_FILE_ATTRIBUTE_TYPE_UINT32:
      value->u.uint32 = *(guint32 *)value_p;
      break;

    case G_FILE_ATTRIBUTE_TYPE_INT32:
      value->u.int32 = *(gint32 *)value_p;
      break;

    case G_FILE_ATTRIBUTE_TYPE_UINT64:
      value->u.uint64 = *(guint64 *)value_p;
      break;

    case G_FILE_ATTRIBUTE_TYPE_INT64:
      value->u.int64 = *(gint64 *)value_p;
      break;

    case G_FILE_ATTRIBUTE_TYPE_INVALID:
      break;

    default:
      g_warning ("Unknown type specified in g_file_info_set_attribute\n");
      break;
    }
}

/**
 * _g_file_attribute_value_set_string:
 * @attr: a #GFileAttributeValue.
 * @string: a UTF-8 string to set within the type.
 *
 * Sets the attribute value to a given UTF-8 string.
 */
void
_g_file_attribute_value_set_string (GFileAttributeValue *attr,
				    const char          *string)
{
  g_return_if_fail (attr != NULL);
  g_return_if_fail (string != NULL);

  _g_file_attribute_value_clear (attr);
  attr->type = G_FILE_ATTRIBUTE_TYPE_STRING;
  attr->u.string = g_strdup (string);
}

/**
 * _g_file_attribute_value_set_byte_string:
 * @attr: a #GFileAttributeValue.
 * @string: a byte string to set within the type.
 *
 * Sets the attribute value to a given byte string.
 */
void
_g_file_attribute_value_set_byte_string (GFileAttributeValue *attr,
					 const char          *string)
{
  g_return_if_fail (attr != NULL);
  g_return_if_fail (string != NULL);

  _g_file_attribute_value_clear (attr);
  attr->type = G_FILE_ATTRIBUTE_TYPE_BYTE_STRING;
  attr->u.string = g_strdup (string);
}

void
_g_file_attribute_value_set_stringv (GFileAttributeValue *attr,
				     char               **value)
{
  g_return_if_fail (attr != NULL);
  g_return_if_fail (value != NULL);

  _g_file_attribute_value_clear (attr);
  attr->type = G_FILE_ATTRIBUTE_TYPE_STRINGV;
  attr->u.stringv = g_strdupv (value);
}


/**
 * _g_file_attribute_value_set_boolean:
 * @attr: a #GFileAttributeValue.
 * @value: a #gboolean to set within the type.
 *
 * Sets the attribute value to the given boolean value.
 */
void
_g_file_attribute_value_set_boolean (GFileAttributeValue *attr,
				     gboolean             value)
{
  g_return_if_fail (attr != NULL);

  _g_file_attribute_value_clear (attr);
  attr->type = G_FILE_ATTRIBUTE_TYPE_BOOLEAN;
  attr->u.boolean = !!value;
}

/**
 * _g_file_attribute_value_set_uint32:
 * @attr: a #GFileAttributeValue.
 * @value: a #guint32 to set within the type.
 *
 * Sets the attribute value to the given unsigned 32-bit integer.
 */
void
_g_file_attribute_value_set_uint32 (GFileAttributeValue *attr,
				    guint32              value)
{
  g_return_if_fail (attr != NULL);

  _g_file_attribute_value_clear (attr);
  attr->type = G_FILE_ATTRIBUTE_TYPE_UINT32;
  attr->u.uint32 = value;
}

/**
 * _g_file_attribute_value_set_int32:
 * @attr: a #GFileAttributeValue.
 * @value: a #gint32 to set within the type.
 *
 * Sets the attribute value to the given signed 32-bit integer.
 */
void
_g_file_attribute_value_set_int32 (GFileAttributeValue *attr,
				   gint32               value)
{
  g_return_if_fail (attr != NULL);

  _g_file_attribute_value_clear (attr);
  attr->type = G_FILE_ATTRIBUTE_TYPE_INT32;
  attr->u.int32 = value;
}

/**
 * _g_file_attribute_value_set_uint64:
 * @attr: a #GFileAttributeValue.
 * @value: a #guint64 to set within the type.
 *
 * Sets the attribute value to a given unsigned 64-bit integer.
 */
void
_g_file_attribute_value_set_uint64 (GFileAttributeValue *attr,
				    guint64              value)
{
  g_return_if_fail (attr != NULL);

  _g_file_attribute_value_clear (attr);
  attr->type = G_FILE_ATTRIBUTE_TYPE_UINT64;
  attr->u.uint64 = value;
}

/**
 * _g_file_attribute_value_set_int64:
 * @attr: a #GFileAttributeValue.
 * @value: a #gint64 to set within the type.
 *
 * Sets the attribute value to a given signed 64-bit integer.
 */
void
_g_file_attribute_value_set_int64 (GFileAttributeValue *attr,
				   gint64               value)
{
  g_return_if_fail (attr != NULL);

  _g_file_attribute_value_clear (attr);
  attr->type = G_FILE_ATTRIBUTE_TYPE_INT64;
  attr->u.int64 = value;
}

/**
 * _g_file_attribute_value_set_object:
 * @attr: a #GFileAttributeValue.
 * @obj: a #GObject.
 *
 * Sets the attribute to contain the value @obj.
 * The @attr references the GObject internally.
 */
void
_g_file_attribute_value_set_object (GFileAttributeValue *attr,
				    GObject             *obj)
{
  g_return_if_fail (attr != NULL);
  g_return_if_fail (obj != NULL);

  _g_file_attribute_value_clear (attr);
  attr->type = G_FILE_ATTRIBUTE_TYPE_OBJECT;
  attr->u.obj = g_object_ref (obj);
}

typedef struct {
  GFileAttributeInfoList public;
  GArray *array;
  int ref_count;
} GFileAttributeInfoListPriv;

static void
list_update_public (GFileAttributeInfoListPriv *priv)
{
  priv->public.infos = (GFileAttributeInfo *)priv->array->data;
  priv->public.n_infos = priv->array->len;
}

/**
 * g_file_attribute_info_list_new:
 *
 * Creates a new file attribute info list.
 *
 * Returns: a #GFileAttributeInfoList.
 */
GFileAttributeInfoList *
g_file_attribute_info_list_new (void)
{
  GFileAttributeInfoListPriv *priv;

  priv = g_new0 (GFileAttributeInfoListPriv, 1);

  priv->ref_count = 1;
  priv->array = g_array_new (TRUE, FALSE, sizeof (GFileAttributeInfo));

  list_update_public (priv);

  return (GFileAttributeInfoList *)priv;
}

/**
 * g_file_attribute_info_list_dup:
 * @list: a #GFileAttributeInfoList to duplicate.
 *
 * Makes a duplicate of a file attribute info list.
 *
 * Returns: a copy of the given @list.
 */
GFileAttributeInfoList *
g_file_attribute_info_list_dup (GFileAttributeInfoList *list)
{
  GFileAttributeInfoListPriv *new;
  int i;

  g_return_val_if_fail (list != NULL, NULL);

  new = g_new0 (GFileAttributeInfoListPriv, 1);
  new->ref_count = 1;
  new->array = g_array_new (TRUE, FALSE, sizeof (GFileAttributeInfo));

  g_array_set_size (new->array, list->n_infos);
  list_update_public (new);
  for (i = 0; i < list->n_infos; i++)
    {
      new->public.infos[i].name = g_strdup (list->infos[i].name);
      new->public.infos[i].type = list->infos[i].type;
      new->public.infos[i].flags = list->infos[i].flags;
    }

  return (GFileAttributeInfoList *)new;
}

/**
 * g_file_attribute_info_list_ref:
 * @list: a #GFileAttributeInfoList to reference.
 *
 * References a file attribute info list.
 *
 * Returns: #GFileAttributeInfoList or %NULL on error.
 */
GFileAttributeInfoList *
g_file_attribute_info_list_ref (GFileAttributeInfoList *list)
{
  GFileAttributeInfoListPriv *priv = (GFileAttributeInfoListPriv *)list;

  g_return_val_if_fail (list != NULL, NULL);
  g_return_val_if_fail (priv->ref_count > 0, NULL);

  g_atomic_int_inc (&priv->ref_count);

  return list;
}

/**
 * g_file_attribute_info_list_unref:
 * @list: The #GFileAttributeInfoList to unreference.
 *
 * Removes a reference from the given @list. If the reference count
 * falls to zero, the @list is deleted.
 */
void
g_file_attribute_info_list_unref (GFileAttributeInfoList *list)
{
  GFileAttributeInfoListPriv *priv = (GFileAttributeInfoListPriv *)list;
  int i;

  g_return_if_fail (list != NULL);
  g_return_if_fail (priv->ref_count > 0);

  if (g_atomic_int_dec_and_test (&priv->ref_count))
    {
      for (i = 0; i < list->n_infos; i++)
        g_free (list->infos[i].name);
      g_array_free (priv->array, TRUE);
      g_free (list);
    }
}

static int
g_file_attribute_info_list_bsearch (GFileAttributeInfoList *list,
				    const char             *name)
{
  int start, end, mid;

  start = 0;
  end = list->n_infos;

  while (start != end)
    {
      mid = start + (end - start) / 2;

      if (strcmp (name, list->infos[mid].name) < 0)
	end = mid;
      else if (strcmp (name, list->infos[mid].name) > 0)
	start = mid + 1;
      else
	return mid;
    }
  return start;
}

/**
 * g_file_attribute_info_list_lookup:
 * @list: a #GFileAttributeInfoList.
 * @name: the name of the attribute to lookup.
 *
 * Gets the file attribute with the name @name from @list.
 *
 * Returns: a #GFileAttributeInfo for the @name, or %NULL if an
 * attribute isn't found.
 */
const GFileAttributeInfo *
g_file_attribute_info_list_lookup (GFileAttributeInfoList *list,
				   const char             *name)
{
  int i;

  g_return_val_if_fail (list != NULL, NULL);
  g_return_val_if_fail (name != NULL, NULL);

  i = g_file_attribute_info_list_bsearch (list, name);

  if (i < list->n_infos && strcmp (list->infos[i].name, name) == 0)
    return &list->infos[i];

  return NULL;
}

/**
 * g_file_attribute_info_list_add:
 * @list: a #GFileAttributeInfoList.
 * @name: the name of the attribute to add.
 * @type: the #GFileAttributeType for the attribute.
 * @flags: #GFileAttributeInfoFlags for the attribute.
 *
 * Adds a new attribute with @name to the @list, setting
 * its @type and @flags.
 */
void
g_file_attribute_info_list_add (GFileAttributeInfoList *list,
				const char             *name,
				GFileAttributeType      type,
				GFileAttributeInfoFlags flags)
{
  GFileAttributeInfoListPriv *priv = (GFileAttributeInfoListPriv *)list;
  GFileAttributeInfo info;
  int i;

  g_return_if_fail (list != NULL);
  g_return_if_fail (name != NULL);

  i = g_file_attribute_info_list_bsearch (list, name);

  if (i < list->n_infos && strcmp (list->infos[i].name, name) == 0)
    {
      list->infos[i].type = type;
      return;
    }

  info.name = g_strdup (name);
  info.type = type;
  info.flags = flags;
  g_array_insert_vals (priv->array, i, &info, 1);

  list_update_public (priv);
}
