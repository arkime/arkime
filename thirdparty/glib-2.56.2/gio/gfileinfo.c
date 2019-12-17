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

/**
 * SECTION:gfileinfo
 * @short_description: File Information and Attributes
 * @include: gio/gio.h
 * @see_also: #GFile, [GFileAttribute][gio-GFileAttribute]
 *
 * Functionality for manipulating basic metadata for files. #GFileInfo
 * implements methods for getting information that all files should
 * contain, and allows for manipulation of extended attributes.
 *
 * See [GFileAttribute][gio-GFileAttribute] for more information on how
 * GIO handles file attributes.
 *
 * To obtain a #GFileInfo for a #GFile, use g_file_query_info() (or its
 * async variant). To obtain a #GFileInfo for a file input or output
 * stream, use g_file_input_stream_query_info() or
 * g_file_output_stream_query_info() (or their async variants).
 *
 * To change the actual attributes of a file, you should then set the
 * attribute in the #GFileInfo and call g_file_set_attributes_from_info()
 * or g_file_set_attributes_async() on a GFile.
 *
 * However, not all attributes can be changed in the file. For instance,
 * the actual size of a file cannot be changed via g_file_info_set_size().
 * You may call g_file_query_settable_attributes() and
 * g_file_query_writable_namespaces() to discover the settable attributes
 * of a particular file at runtime.
 *
 * #GFileAttributeMatcher allows for searching through a #GFileInfo for
 * attributes.
 **/

#include "config.h"

#include <string.h>

#include "gfileinfo.h"
#include "gfileinfo-priv.h"
#include "gfileattribute-priv.h"
#include "gicon.h"
#include "glibintl.h"


/* We use this nasty thing, because NULL is a valid attribute matcher (matches nothing) */
#define NO_ATTRIBUTE_MASK ((GFileAttributeMatcher *)1)

typedef struct  {
  guint32 attribute;
  GFileAttributeValue value;
} GFileAttribute;

struct _GFileInfo
{
  GObject parent_instance;

  GArray *attributes;
  GFileAttributeMatcher *mask;
};

struct _GFileInfoClass
{
  GObjectClass parent_class;
};


G_DEFINE_TYPE (GFileInfo, g_file_info, G_TYPE_OBJECT)

typedef struct {
  guint32 id;
  guint32 attribute_id_counter;
} NSInfo;

G_LOCK_DEFINE_STATIC (attribute_hash);
static int namespace_id_counter = 0;
static GHashTable *ns_hash = NULL;
static GHashTable *attribute_hash = NULL;
static char ***attributes = NULL;

/* Attribute ids are 32bit, we split it up like this:
 * |------------|--------------------|
 *   12 bit          20 bit
 *   namespace      attribute id
 *
 * This way the attributes gets sorted in namespace order
 */

#define NS_POS 20
#define NS_MASK ((guint32)((1<<12) - 1))
#define ID_POS 0
#define ID_MASK ((guint32)((1<<20) - 1))

#define GET_NS(_attr_id) \
    (((guint32) (_attr_id) >> NS_POS) & NS_MASK)
#define GET_ID(_attr_id) \
    (((guint32)(_attr_id) >> ID_POS) & ID_MASK)

#define MAKE_ATTR_ID(_ns, _id)				\
    ( ((((guint32) _ns) & NS_MASK) << NS_POS) |		\
      ((((guint32) _id) & ID_MASK) << ID_POS) )

static NSInfo *
_lookup_namespace (const char *namespace)
{
  NSInfo *ns_info;

  ns_info = g_hash_table_lookup (ns_hash, namespace);
  if (ns_info == NULL)
    {
      ns_info = g_new0 (NSInfo, 1);
      ns_info->id = ++namespace_id_counter;
      g_hash_table_insert (ns_hash, g_strdup (namespace), ns_info);
      attributes = g_realloc (attributes, (ns_info->id + 1) * sizeof (char **));
      attributes[ns_info->id] = g_new (char *, 1);
      attributes[ns_info->id][0] = g_strconcat (namespace, "::*", NULL);
    }
  return ns_info;
}

static guint32
_lookup_attribute (const char *attribute)
{
  guint32 attr_id, id;
  char *ns;
  const char *colon;
  NSInfo *ns_info;

  attr_id = GPOINTER_TO_UINT (g_hash_table_lookup (attribute_hash, attribute));

  if (attr_id != 0)
    return attr_id;

  colon = strstr (attribute, "::");
  if (colon)
    ns = g_strndup (attribute, colon - attribute);
  else
    ns = g_strdup ("");

  ns_info = _lookup_namespace (ns);
  g_free (ns);

  id = ++ns_info->attribute_id_counter;
  attributes[ns_info->id] = g_realloc (attributes[ns_info->id], (id + 1) * sizeof (char *));
  attributes[ns_info->id][id] = g_strdup (attribute);

  attr_id = MAKE_ATTR_ID (ns_info->id, id);

  g_hash_table_insert (attribute_hash, attributes[ns_info->id][id], GUINT_TO_POINTER (attr_id));

  return attr_id;
}

static void
ensure_attribute_hash (void)
{
  if (attribute_hash != NULL)
    return;

  ns_hash = g_hash_table_new (g_str_hash, g_str_equal);
  attribute_hash = g_hash_table_new (g_str_hash, g_str_equal);

#define REGISTER_ATTRIBUTE(name) G_STMT_START{\
  guint _u = _lookup_attribute (G_FILE_ATTRIBUTE_ ## name); \
  /* use for generating the ID: g_print ("#define G_FILE_ATTRIBUTE_ID_%s (%u + %u)\n", #name + 17, _u & ~ID_MASK, _u & ID_MASK); */ \
  g_assert (_u == G_FILE_ATTRIBUTE_ID_ ## name); \
}G_STMT_END

  REGISTER_ATTRIBUTE (STANDARD_TYPE);
  REGISTER_ATTRIBUTE (STANDARD_IS_HIDDEN);
  REGISTER_ATTRIBUTE (STANDARD_IS_BACKUP);
  REGISTER_ATTRIBUTE (STANDARD_IS_SYMLINK);
  REGISTER_ATTRIBUTE (STANDARD_IS_VIRTUAL);
  REGISTER_ATTRIBUTE (STANDARD_NAME);
  REGISTER_ATTRIBUTE (STANDARD_DISPLAY_NAME);
  REGISTER_ATTRIBUTE (STANDARD_EDIT_NAME);
  REGISTER_ATTRIBUTE (STANDARD_COPY_NAME);
  REGISTER_ATTRIBUTE (STANDARD_DESCRIPTION);
  REGISTER_ATTRIBUTE (STANDARD_ICON);
  REGISTER_ATTRIBUTE (STANDARD_CONTENT_TYPE);
  REGISTER_ATTRIBUTE (STANDARD_FAST_CONTENT_TYPE);
  REGISTER_ATTRIBUTE (STANDARD_SIZE);
  REGISTER_ATTRIBUTE (STANDARD_ALLOCATED_SIZE);
  REGISTER_ATTRIBUTE (STANDARD_SYMLINK_TARGET);
  REGISTER_ATTRIBUTE (STANDARD_TARGET_URI);
  REGISTER_ATTRIBUTE (STANDARD_SORT_ORDER);
  REGISTER_ATTRIBUTE (STANDARD_SYMBOLIC_ICON);
  REGISTER_ATTRIBUTE (STANDARD_IS_VOLATILE);
  REGISTER_ATTRIBUTE (ETAG_VALUE);
  REGISTER_ATTRIBUTE (ID_FILE);
  REGISTER_ATTRIBUTE (ID_FILESYSTEM);
  REGISTER_ATTRIBUTE (ACCESS_CAN_READ);
  REGISTER_ATTRIBUTE (ACCESS_CAN_WRITE);
  REGISTER_ATTRIBUTE (ACCESS_CAN_EXECUTE);
  REGISTER_ATTRIBUTE (ACCESS_CAN_DELETE);
  REGISTER_ATTRIBUTE (ACCESS_CAN_TRASH);
  REGISTER_ATTRIBUTE (ACCESS_CAN_RENAME);
  REGISTER_ATTRIBUTE (MOUNTABLE_CAN_MOUNT);
  REGISTER_ATTRIBUTE (MOUNTABLE_CAN_UNMOUNT);
  REGISTER_ATTRIBUTE (MOUNTABLE_CAN_EJECT);
  REGISTER_ATTRIBUTE (MOUNTABLE_UNIX_DEVICE);
  REGISTER_ATTRIBUTE (MOUNTABLE_UNIX_DEVICE_FILE);
  REGISTER_ATTRIBUTE (MOUNTABLE_HAL_UDI);
  REGISTER_ATTRIBUTE (MOUNTABLE_CAN_START);
  REGISTER_ATTRIBUTE (MOUNTABLE_CAN_START_DEGRADED);
  REGISTER_ATTRIBUTE (MOUNTABLE_CAN_STOP);
  REGISTER_ATTRIBUTE (MOUNTABLE_START_STOP_TYPE);
  REGISTER_ATTRIBUTE (MOUNTABLE_CAN_POLL);
  REGISTER_ATTRIBUTE (MOUNTABLE_IS_MEDIA_CHECK_AUTOMATIC);
  REGISTER_ATTRIBUTE (TIME_MODIFIED);
  REGISTER_ATTRIBUTE (TIME_MODIFIED_USEC);
  REGISTER_ATTRIBUTE (TIME_ACCESS);
  REGISTER_ATTRIBUTE (TIME_ACCESS_USEC);
  REGISTER_ATTRIBUTE (TIME_CHANGED);
  REGISTER_ATTRIBUTE (TIME_CHANGED_USEC);
  REGISTER_ATTRIBUTE (TIME_CREATED);
  REGISTER_ATTRIBUTE (TIME_CREATED_USEC);
  REGISTER_ATTRIBUTE (UNIX_DEVICE);
  REGISTER_ATTRIBUTE (UNIX_INODE);
  REGISTER_ATTRIBUTE (UNIX_MODE);
  REGISTER_ATTRIBUTE (UNIX_NLINK);
  REGISTER_ATTRIBUTE (UNIX_UID);
  REGISTER_ATTRIBUTE (UNIX_GID);
  REGISTER_ATTRIBUTE (UNIX_RDEV);
  REGISTER_ATTRIBUTE (UNIX_BLOCK_SIZE);
  REGISTER_ATTRIBUTE (UNIX_BLOCKS);
  REGISTER_ATTRIBUTE (UNIX_IS_MOUNTPOINT);
  REGISTER_ATTRIBUTE (DOS_IS_ARCHIVE);
  REGISTER_ATTRIBUTE (DOS_IS_SYSTEM);
  REGISTER_ATTRIBUTE (OWNER_USER);
  REGISTER_ATTRIBUTE (OWNER_USER_REAL);
  REGISTER_ATTRIBUTE (OWNER_GROUP);
  REGISTER_ATTRIBUTE (THUMBNAIL_PATH);
  REGISTER_ATTRIBUTE (THUMBNAILING_FAILED);
  REGISTER_ATTRIBUTE (THUMBNAIL_IS_VALID);
  REGISTER_ATTRIBUTE (PREVIEW_ICON);
  REGISTER_ATTRIBUTE (FILESYSTEM_SIZE);
  REGISTER_ATTRIBUTE (FILESYSTEM_FREE);
  REGISTER_ATTRIBUTE (FILESYSTEM_TYPE);
  REGISTER_ATTRIBUTE (FILESYSTEM_READONLY);
  REGISTER_ATTRIBUTE (FILESYSTEM_USE_PREVIEW);
  REGISTER_ATTRIBUTE (GVFS_BACKEND);
  REGISTER_ATTRIBUTE (SELINUX_CONTEXT);
  REGISTER_ATTRIBUTE (TRASH_ITEM_COUNT);
  REGISTER_ATTRIBUTE (TRASH_ORIG_PATH);
  REGISTER_ATTRIBUTE (TRASH_DELETION_DATE);

#undef REGISTER_ATTRIBUTE
}

static guint32
lookup_namespace (const char *namespace)
{
  NSInfo *ns_info;
  guint32 id;

  G_LOCK (attribute_hash);

  ensure_attribute_hash ();

  ns_info = _lookup_namespace (namespace);
  id = 0;
  if (ns_info)
    id = ns_info->id;

  G_UNLOCK (attribute_hash);

  return id;
}

static char *
get_attribute_for_id (int attribute)
{
  char *s;
  G_LOCK (attribute_hash);
  s = attributes[GET_NS(attribute)][GET_ID(attribute)];
  G_UNLOCK (attribute_hash);
  return s;
}

static guint32
lookup_attribute (const char *attribute)
{
  guint32 attr_id;

  G_LOCK (attribute_hash);
  ensure_attribute_hash ();

  attr_id = _lookup_attribute (attribute);

  G_UNLOCK (attribute_hash);

  return attr_id;
}

static void
g_file_info_finalize (GObject *object)
{
  GFileInfo *info;
  int i;
  GFileAttribute *attrs;

  info = G_FILE_INFO (object);

  attrs = (GFileAttribute *)info->attributes->data;
  for (i = 0; i < info->attributes->len; i++)
    _g_file_attribute_value_clear (&attrs[i].value);
  g_array_free (info->attributes, TRUE);

  if (info->mask != NO_ATTRIBUTE_MASK)
    g_file_attribute_matcher_unref (info->mask);

  G_OBJECT_CLASS (g_file_info_parent_class)->finalize (object);
}

static void
g_file_info_class_init (GFileInfoClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = g_file_info_finalize;
}

static void
g_file_info_init (GFileInfo *info)
{
  info->mask = NO_ATTRIBUTE_MASK;
  info->attributes = g_array_new (FALSE, FALSE,
				  sizeof (GFileAttribute));
}

/**
 * g_file_info_new:
 *
 * Creates a new file info structure.
 *
 * Returns: a #GFileInfo.
 **/
GFileInfo *
g_file_info_new (void)
{
  return g_object_new (G_TYPE_FILE_INFO, NULL);
}

/**
 * g_file_info_copy_into:
 * @src_info: source to copy attributes from.
 * @dest_info: destination to copy attributes to.
 *
 * First clears all of the [GFileAttribute][gio-GFileAttribute] of @dest_info,
 * and then copies all of the file attributes from @src_info to @dest_info.
 **/
void
g_file_info_copy_into (GFileInfo *src_info,
                       GFileInfo *dest_info)
{
  GFileAttribute *source, *dest;
  int i;

  g_return_if_fail (G_IS_FILE_INFO (src_info));
  g_return_if_fail (G_IS_FILE_INFO (dest_info));

  dest = (GFileAttribute *)dest_info->attributes->data;
  for (i = 0; i < dest_info->attributes->len; i++)
    _g_file_attribute_value_clear (&dest[i].value);

  g_array_set_size (dest_info->attributes,
		    src_info->attributes->len);

  source = (GFileAttribute *)src_info->attributes->data;
  dest = (GFileAttribute *)dest_info->attributes->data;

  for (i = 0; i < src_info->attributes->len; i++)
    {
      dest[i].attribute = source[i].attribute;
      dest[i].value.type = G_FILE_ATTRIBUTE_TYPE_INVALID;
      _g_file_attribute_value_set (&dest[i].value, &source[i].value);
    }

  if (dest_info->mask != NO_ATTRIBUTE_MASK)
    g_file_attribute_matcher_unref (dest_info->mask);

  if (src_info->mask == NO_ATTRIBUTE_MASK)
    dest_info->mask = NO_ATTRIBUTE_MASK;
  else
    dest_info->mask = g_file_attribute_matcher_ref (src_info->mask);
}

/**
 * g_file_info_dup:
 * @other: a #GFileInfo.
 *
 * Duplicates a file info structure.
 *
 * Returns: (transfer full): a duplicate #GFileInfo of @other.
 **/
GFileInfo *
g_file_info_dup (GFileInfo *other)
{
  GFileInfo *new;

  g_return_val_if_fail (G_IS_FILE_INFO (other), NULL);

  new = g_file_info_new ();
  g_file_info_copy_into (other, new);
  return new;
}

/**
 * g_file_info_set_attribute_mask:
 * @info: a #GFileInfo.
 * @mask: a #GFileAttributeMatcher.
 *
 * Sets @mask on @info to match specific attribute types.
 **/
void
g_file_info_set_attribute_mask (GFileInfo             *info,
				GFileAttributeMatcher *mask)
{
  GFileAttribute *attr;
  int i;

  g_return_if_fail (G_IS_FILE_INFO (info));

  if (mask != info->mask)
    {
      if (info->mask != NO_ATTRIBUTE_MASK)
	g_file_attribute_matcher_unref (info->mask);
      info->mask = g_file_attribute_matcher_ref (mask);

      /* Remove non-matching attributes */
      for (i = 0; i < info->attributes->len; i++)
	{
	  attr = &g_array_index (info->attributes, GFileAttribute, i);
	  if (!_g_file_attribute_matcher_matches_id (mask,
						    attr->attribute))
	    {
	      _g_file_attribute_value_clear (&attr->value);
	      g_array_remove_index (info->attributes, i);
	      i--;
	    }
	}
    }
}

/**
 * g_file_info_unset_attribute_mask:
 * @info: #GFileInfo.
 *
 * Unsets a mask set by g_file_info_set_attribute_mask(), if one
 * is set.
 **/
void
g_file_info_unset_attribute_mask (GFileInfo *info)
{
  g_return_if_fail (G_IS_FILE_INFO (info));

  if (info->mask != NO_ATTRIBUTE_MASK)
    g_file_attribute_matcher_unref (info->mask);
  info->mask = NO_ATTRIBUTE_MASK;
}

/**
 * g_file_info_clear_status:
 * @info: a #GFileInfo.
 *
 * Clears the status information from @info.
 **/
void
g_file_info_clear_status (GFileInfo  *info)
{
  GFileAttribute *attrs;
  int i;

  g_return_if_fail (G_IS_FILE_INFO (info));

  attrs = (GFileAttribute *)info->attributes->data;
  for (i = 0; i < info->attributes->len; i++)
    attrs[i].value.status = G_FILE_ATTRIBUTE_STATUS_UNSET;
}

static int
g_file_info_find_place (GFileInfo  *info,
			guint32     attribute)
{
  int min, max, med;
  GFileAttribute *attrs;
  /* Binary search for the place where attribute would be, if it's
     in the array */

  min = 0;
  max = info->attributes->len;

  attrs = (GFileAttribute *)info->attributes->data;

  while (min < max)
    {
      med = min + (max - min) / 2;
      if (attrs[med].attribute == attribute)
	{
	  min = med;
	  break;
	}
      else if (attrs[med].attribute < attribute)
	min = med + 1;
      else /* attrs[med].attribute > attribute */
	max = med;
    }

  return min;
}

static GFileAttributeValue *
g_file_info_find_value (GFileInfo *info,
			guint32    attr_id)
{
  GFileAttribute *attrs;
  int i;

  i = g_file_info_find_place (info, attr_id);
  attrs = (GFileAttribute *)info->attributes->data;
  if (i < info->attributes->len &&
      attrs[i].attribute == attr_id)
    return &attrs[i].value;

  return NULL;
}

static GFileAttributeValue *
g_file_info_find_value_by_name (GFileInfo  *info,
				const char *attribute)
{
  guint32 attr_id;

  attr_id = lookup_attribute (attribute);
  return g_file_info_find_value (info, attr_id);
}

/**
 * g_file_info_has_attribute:
 * @info: a #GFileInfo.
 * @attribute: a file attribute key.
 *
 * Checks if a file info structure has an attribute named @attribute.
 *
 * Returns: %TRUE if @Ginfo has an attribute named @attribute,
 *     %FALSE otherwise.
 **/
gboolean
g_file_info_has_attribute (GFileInfo  *info,
			   const char *attribute)
{
  GFileAttributeValue *value;

  g_return_val_if_fail (G_IS_FILE_INFO (info), FALSE);
  g_return_val_if_fail (attribute != NULL && *attribute != '\0', FALSE);

  value = g_file_info_find_value_by_name (info, attribute);
  return value != NULL;
}

/**
 * g_file_info_has_namespace:
 * @info: a #GFileInfo.
 * @name_space: a file attribute namespace.
 *
 * Checks if a file info structure has an attribute in the
 * specified @name_space.
 *
 * Returns: %TRUE if @Ginfo has an attribute in @name_space,
 *     %FALSE otherwise.
 *
 * Since: 2.22
 **/
gboolean
g_file_info_has_namespace (GFileInfo  *info,
			   const char *name_space)
{
  GFileAttribute *attrs;
  guint32 ns_id;
  int i;

  g_return_val_if_fail (G_IS_FILE_INFO (info), FALSE);
  g_return_val_if_fail (name_space != NULL, FALSE);

  ns_id = lookup_namespace (name_space);

  attrs = (GFileAttribute *)info->attributes->data;
  for (i = 0; i < info->attributes->len; i++)
    {
      if (GET_NS (attrs[i].attribute) == ns_id)
	return TRUE;
    }

  return FALSE;
}

/**
 * g_file_info_list_attributes:
 * @info: a #GFileInfo.
 * @name_space: (nullable): a file attribute key's namespace, or %NULL to list
 *   all attributes.
 *
 * Lists the file info structure's attributes.
 *
 * Returns: (nullable) (array zero-terminated=1) (transfer full): a
 * null-terminated array of strings of all of the possible attribute
 * types for the given @name_space, or %NULL on error.
 **/
char **
g_file_info_list_attributes (GFileInfo  *info,
			     const char *name_space)
{
  GPtrArray *names;
  GFileAttribute *attrs;
  guint32 attribute;
  guint32 ns_id = (name_space) ? lookup_namespace (name_space) : 0;
  int i;

  g_return_val_if_fail (G_IS_FILE_INFO (info), NULL);

  names = g_ptr_array_new ();
  attrs = (GFileAttribute *)info->attributes->data;
  for (i = 0; i < info->attributes->len; i++)
    {
      attribute = attrs[i].attribute;
      if (ns_id == 0 || GET_NS (attribute) == ns_id)
        g_ptr_array_add (names, g_strdup (get_attribute_for_id (attribute)));
    }

  /* NULL terminate */
  g_ptr_array_add (names, NULL);

  return (char **)g_ptr_array_free (names, FALSE);
}

/**
 * g_file_info_get_attribute_type:
 * @info: a #GFileInfo.
 * @attribute: a file attribute key.
 *
 * Gets the attribute type for an attribute key.
 *
 * Returns: a #GFileAttributeType for the given @attribute, or
 * %G_FILE_ATTRIBUTE_TYPE_INVALID if the key is not set.
 **/
GFileAttributeType
g_file_info_get_attribute_type (GFileInfo  *info,
				const char *attribute)
{
  GFileAttributeValue *value;

  g_return_val_if_fail (G_IS_FILE_INFO (info), G_FILE_ATTRIBUTE_TYPE_INVALID);
  g_return_val_if_fail (attribute != NULL && *attribute != '\0', G_FILE_ATTRIBUTE_TYPE_INVALID);

  value = g_file_info_find_value_by_name (info, attribute);
  if (value)
    return value->type;
  else
    return G_FILE_ATTRIBUTE_TYPE_INVALID;
}

/**
 * g_file_info_remove_attribute:
 * @info: a #GFileInfo.
 * @attribute: a file attribute key.
 *
 * Removes all cases of @attribute from @info if it exists.
 **/
void
g_file_info_remove_attribute (GFileInfo  *info,
			      const char *attribute)
{
  guint32 attr_id;
  GFileAttribute *attrs;
  int i;

  g_return_if_fail (G_IS_FILE_INFO (info));
  g_return_if_fail (attribute != NULL && *attribute != '\0');

  attr_id = lookup_attribute (attribute);

  i = g_file_info_find_place (info, attr_id);
  attrs = (GFileAttribute *)info->attributes->data;
  if (i < info->attributes->len &&
      attrs[i].attribute == attr_id)
    {
      _g_file_attribute_value_clear (&attrs[i].value);
      g_array_remove_index (info->attributes, i);
    }
}

/**
 * g_file_info_get_attribute_data:
 * @info: a #GFileInfo
 * @attribute: a file attribute key
 * @type: (out) (optional): return location for the attribute type, or %NULL
 * @value_pp: (out) (optional) (not nullable): return location for the
 *    attribute value, or %NULL; the attribute value will not be %NULL
 * @status: (out) (optional): return location for the attribute status, or %NULL
 *
 * Gets the attribute type, value and status for an attribute key.
 *
 * Returns: (transfer none): %TRUE if @info has an attribute named @attribute,
 *      %FALSE otherwise.
 */
gboolean
g_file_info_get_attribute_data (GFileInfo            *info,
				const char           *attribute,
				GFileAttributeType   *type,
				gpointer             *value_pp,
				GFileAttributeStatus *status)
{
  GFileAttributeValue *value;

  value = g_file_info_find_value_by_name (info, attribute);
  if (value == NULL)
    return FALSE;

  if (status)
    *status = value->status;

  if (type)
    *type = value->type;

  if (value_pp)
    *value_pp = _g_file_attribute_value_peek_as_pointer (value);

  return TRUE;
}

/**
 * g_file_info_get_attribute_status:
 * @info: a #GFileInfo
 * @attribute: a file attribute key
 *
 * Gets the attribute status for an attribute key.
 *
 * Returns: a #GFileAttributeStatus for the given @attribute, or
 *    %G_FILE_ATTRIBUTE_STATUS_UNSET if the key is invalid.
 *
 */
GFileAttributeStatus
g_file_info_get_attribute_status (GFileInfo  *info,
				  const char *attribute)
{
  GFileAttributeValue *val;

  g_return_val_if_fail (G_IS_FILE_INFO (info), 0);
  g_return_val_if_fail (attribute != NULL && *attribute != '\0', 0);

  val = g_file_info_find_value_by_name (info, attribute);
  if (val)
    return val->status;

  return G_FILE_ATTRIBUTE_STATUS_UNSET;
}

/**
 * g_file_info_set_attribute_status:
 * @info: a #GFileInfo
 * @attribute: a file attribute key
 * @status: a #GFileAttributeStatus
 *
 * Sets the attribute status for an attribute key. This is only
 * needed by external code that implement g_file_set_attributes_from_info()
 * or similar functions.
 *
 * The attribute must exist in @info for this to work. Otherwise %FALSE
 * is returned and @info is unchanged.
 *
 * Returns: %TRUE if the status was changed, %FALSE if the key was not set.
 *
 * Since: 2.22
 */
gboolean
g_file_info_set_attribute_status (GFileInfo  *info,
				  const char *attribute,
				  GFileAttributeStatus status)
{
  GFileAttributeValue *val;

  g_return_val_if_fail (G_IS_FILE_INFO (info), FALSE);
  g_return_val_if_fail (attribute != NULL && *attribute != '\0', FALSE);

  val = g_file_info_find_value_by_name (info, attribute);
  if (val)
    {
      val->status = status;
      return TRUE;
    }

  return FALSE;
}

GFileAttributeValue *
_g_file_info_get_attribute_value (GFileInfo  *info,
				  const char *attribute)

{
  g_return_val_if_fail (G_IS_FILE_INFO (info), NULL);
  g_return_val_if_fail (attribute != NULL && *attribute != '\0', NULL);

  return g_file_info_find_value_by_name (info, attribute);
}

/**
 * g_file_info_get_attribute_as_string:
 * @info: a #GFileInfo.
 * @attribute: a file attribute key.
 *
 * Gets the value of a attribute, formated as a string.
 * This escapes things as needed to make the string valid
 * utf8.
 *
 * Returns: a UTF-8 string associated with the given @attribute.
 *    When you're done with the string it must be freed with g_free().
 **/
char *
g_file_info_get_attribute_as_string (GFileInfo  *info,
				     const char *attribute)
{
  GFileAttributeValue *val;
  val = _g_file_info_get_attribute_value (info, attribute);
  if (val)
    return _g_file_attribute_value_as_string (val);
  return NULL;
}


/**
 * g_file_info_get_attribute_object:
 * @info: a #GFileInfo.
 * @attribute: a file attribute key.
 *
 * Gets the value of a #GObject attribute. If the attribute does
 * not contain a #GObject, %NULL will be returned.
 *
 * Returns: (transfer none): a #GObject associated with the given @attribute, or
 * %NULL otherwise.
 **/
GObject *
g_file_info_get_attribute_object (GFileInfo  *info,
				  const char *attribute)
{
  GFileAttributeValue *value;

  g_return_val_if_fail (G_IS_FILE_INFO (info), NULL);
  g_return_val_if_fail (attribute != NULL && *attribute != '\0', NULL);

  value = g_file_info_find_value_by_name (info, attribute);
  return _g_file_attribute_value_get_object (value);
}

/**
 * g_file_info_get_attribute_string:
 * @info: a #GFileInfo.
 * @attribute: a file attribute key.
 *
 * Gets the value of a string attribute. If the attribute does
 * not contain a string, %NULL will be returned.
 *
 * Returns: the contents of the @attribute value as a UTF-8 string, or
 * %NULL otherwise.
 **/
const char *
g_file_info_get_attribute_string (GFileInfo  *info,
				  const char *attribute)
{
  GFileAttributeValue *value;

  g_return_val_if_fail (G_IS_FILE_INFO (info), NULL);
  g_return_val_if_fail (attribute != NULL && *attribute != '\0', NULL);

  value = g_file_info_find_value_by_name (info, attribute);
  return _g_file_attribute_value_get_string (value);
}

/**
 * g_file_info_get_attribute_byte_string:
 * @info: a #GFileInfo.
 * @attribute: a file attribute key.
 *
 * Gets the value of a byte string attribute. If the attribute does
 * not contain a byte string, %NULL will be returned.
 *
 * Returns: the contents of the @attribute value as a byte string, or
 * %NULL otherwise.
 **/
const char *
g_file_info_get_attribute_byte_string (GFileInfo  *info,
				       const char *attribute)
{
  GFileAttributeValue *value;

  g_return_val_if_fail (G_IS_FILE_INFO (info), NULL);
  g_return_val_if_fail (attribute != NULL && *attribute != '\0', NULL);

  value = g_file_info_find_value_by_name (info, attribute);
  return _g_file_attribute_value_get_byte_string (value);
}

/**
 * g_file_info_get_attribute_stringv:
 * @info: a #GFileInfo.
 * @attribute: a file attribute key.
 *
 * Gets the value of a stringv attribute. If the attribute does
 * not contain a stringv, %NULL will be returned.
 *
 * Returns: (transfer none): the contents of the @attribute value as a stringv, or
 * %NULL otherwise. Do not free. These returned strings are UTF-8.
 *
 * Since: 2.22
 **/
char **
g_file_info_get_attribute_stringv (GFileInfo  *info,
				   const char *attribute)
{
  GFileAttributeValue *value;

  g_return_val_if_fail (G_IS_FILE_INFO (info), NULL);
  g_return_val_if_fail (attribute != NULL && *attribute != '\0', NULL);

  value = g_file_info_find_value_by_name (info, attribute);
  return _g_file_attribute_value_get_stringv (value);
}

/**
 * g_file_info_get_attribute_boolean:
 * @info: a #GFileInfo.
 * @attribute: a file attribute key.
 *
 * Gets the value of a boolean attribute. If the attribute does not
 * contain a boolean value, %FALSE will be returned.
 *
 * Returns: the boolean value contained within the attribute.
 **/
gboolean
g_file_info_get_attribute_boolean (GFileInfo  *info,
				   const char *attribute)
{
  GFileAttributeValue *value;

  g_return_val_if_fail (G_IS_FILE_INFO (info), FALSE);
  g_return_val_if_fail (attribute != NULL && *attribute != '\0', FALSE);

  value = g_file_info_find_value_by_name (info, attribute);
  return _g_file_attribute_value_get_boolean (value);
}

/**
 * g_file_info_get_attribute_uint32:
 * @info: a #GFileInfo.
 * @attribute: a file attribute key.
 *
 * Gets an unsigned 32-bit integer contained within the attribute. If the
 * attribute does not contain an unsigned 32-bit integer, or is invalid,
 * 0 will be returned.
 *
 * Returns: an unsigned 32-bit integer from the attribute.
 **/
guint32
g_file_info_get_attribute_uint32 (GFileInfo  *info,
				  const char *attribute)
{
  GFileAttributeValue *value;

  g_return_val_if_fail (G_IS_FILE_INFO (info), 0);
  g_return_val_if_fail (attribute != NULL && *attribute != '\0', 0);

  value = g_file_info_find_value_by_name (info, attribute);
  return _g_file_attribute_value_get_uint32 (value);
}

/**
 * g_file_info_get_attribute_int32:
 * @info: a #GFileInfo.
 * @attribute: a file attribute key.
 *
 * Gets a signed 32-bit integer contained within the attribute. If the
 * attribute does not contain a signed 32-bit integer, or is invalid,
 * 0 will be returned.
 *
 * Returns: a signed 32-bit integer from the attribute.
 **/
gint32
g_file_info_get_attribute_int32 (GFileInfo  *info,
				 const char *attribute)
{
  GFileAttributeValue *value;

  g_return_val_if_fail (G_IS_FILE_INFO (info), 0);
  g_return_val_if_fail (attribute != NULL && *attribute != '\0', 0);

  value = g_file_info_find_value_by_name (info, attribute);
  return _g_file_attribute_value_get_int32 (value);
}

/**
 * g_file_info_get_attribute_uint64:
 * @info: a #GFileInfo.
 * @attribute: a file attribute key.
 *
 * Gets a unsigned 64-bit integer contained within the attribute. If the
 * attribute does not contain an unsigned 64-bit integer, or is invalid,
 * 0 will be returned.
 *
 * Returns: a unsigned 64-bit integer from the attribute.
 **/
guint64
g_file_info_get_attribute_uint64 (GFileInfo  *info,
				  const char *attribute)
{
  GFileAttributeValue *value;

  g_return_val_if_fail (G_IS_FILE_INFO (info), 0);
  g_return_val_if_fail (attribute != NULL && *attribute != '\0', 0);

  value = g_file_info_find_value_by_name (info, attribute);
  return _g_file_attribute_value_get_uint64 (value);
}

/**
 * g_file_info_get_attribute_int64:
 * @info: a #GFileInfo.
 * @attribute: a file attribute key.
 *
 * Gets a signed 64-bit integer contained within the attribute. If the
 * attribute does not contain an signed 64-bit integer, or is invalid,
 * 0 will be returned.
 *
 * Returns: a signed 64-bit integer from the attribute.
 **/
gint64
g_file_info_get_attribute_int64  (GFileInfo  *info,
				  const char *attribute)
{
  GFileAttributeValue *value;

  g_return_val_if_fail (G_IS_FILE_INFO (info), 0);
  g_return_val_if_fail (attribute != NULL && *attribute != '\0', 0);

  value = g_file_info_find_value_by_name (info, attribute);
  return _g_file_attribute_value_get_int64 (value);
}

static GFileAttributeValue *
g_file_info_create_value (GFileInfo *info,
			  guint32 attr_id)
{
  GFileAttribute *attrs;
  int i;

  if (info->mask != NO_ATTRIBUTE_MASK &&
      !_g_file_attribute_matcher_matches_id (info->mask, attr_id))
    return NULL;

  i = g_file_info_find_place (info, attr_id);

  attrs = (GFileAttribute *)info->attributes->data;
  if (i < info->attributes->len &&
      attrs[i].attribute == attr_id)
    return &attrs[i].value;
  else
    {
      GFileAttribute attr = { 0 };
      attr.attribute = attr_id;
      g_array_insert_val (info->attributes, i, attr);

      attrs = (GFileAttribute *)info->attributes->data;
      return &attrs[i].value;
    }
}

void
_g_file_info_set_attribute_by_id (GFileInfo                 *info,
                                  guint32                    attribute,
                                  GFileAttributeType         type,
                                  gpointer                   value_p)
{
  GFileAttributeValue *value;

  value = g_file_info_create_value (info, attribute);

  if (value)
    _g_file_attribute_value_set_from_pointer (value, type, value_p, TRUE);
}

/**
 * g_file_info_set_attribute:
 * @info: a #GFileInfo.
 * @attribute: a file attribute key.
 * @type: a #GFileAttributeType
 * @value_p: (not nullable): pointer to the value
 *
 * Sets the @attribute to contain the given value, if possible. To unset the
 * attribute, use %G_ATTRIBUTE_TYPE_INVALID for @type.
 **/
void
g_file_info_set_attribute (GFileInfo                 *info,
			   const char                *attribute,
			   GFileAttributeType         type,
			   gpointer                   value_p)
{
  g_return_if_fail (G_IS_FILE_INFO (info));
  g_return_if_fail (attribute != NULL && *attribute != '\0');

  _g_file_info_set_attribute_by_id (info, lookup_attribute (attribute), type, value_p);
}

void
_g_file_info_set_attribute_object_by_id (GFileInfo *info,
                                         guint32    attribute,
				         GObject   *attr_value)
{
  GFileAttributeValue *value;

  value = g_file_info_create_value (info, attribute);
  if (value)
    _g_file_attribute_value_set_object (value, attr_value);
}

/**
 * g_file_info_set_attribute_object:
 * @info: a #GFileInfo.
 * @attribute: a file attribute key.
 * @attr_value: a #GObject.
 *
 * Sets the @attribute to contain the given @attr_value,
 * if possible.
 **/
void
g_file_info_set_attribute_object (GFileInfo  *info,
				  const char *attribute,
				  GObject    *attr_value)
{
  g_return_if_fail (G_IS_FILE_INFO (info));
  g_return_if_fail (attribute != NULL && *attribute != '\0');
  g_return_if_fail (G_IS_OBJECT (attr_value));

  _g_file_info_set_attribute_object_by_id (info,
                                           lookup_attribute (attribute),
                                           attr_value);
}

void
_g_file_info_set_attribute_stringv_by_id (GFileInfo *info,
                                          guint32    attribute,
				          char     **attr_value)
{
  GFileAttributeValue *value;

  value = g_file_info_create_value (info, attribute);
  if (value)
    _g_file_attribute_value_set_stringv (value, attr_value);
}

/**
 * g_file_info_set_attribute_stringv:
 * @info: a #GFileInfo.
 * @attribute: a file attribute key
 * @attr_value: (array) (element-type utf8): a %NULL terminated array of UTF-8 strings.
 *
 * Sets the @attribute to contain the given @attr_value,
 * if possible.
 *
 * Sinze: 2.22
 **/
void
g_file_info_set_attribute_stringv (GFileInfo  *info,
				   const char *attribute,
				   char      **attr_value)
{
  g_return_if_fail (G_IS_FILE_INFO (info));
  g_return_if_fail (attribute != NULL && *attribute != '\0');
  g_return_if_fail (attr_value != NULL);

  _g_file_info_set_attribute_stringv_by_id (info,
                                            lookup_attribute (attribute),
                                            attr_value);
}

void
_g_file_info_set_attribute_string_by_id (GFileInfo  *info,
                                         guint32     attribute,
				         const char *attr_value)
{
  GFileAttributeValue *value;

  value = g_file_info_create_value (info, attribute);
  if (value)
    _g_file_attribute_value_set_string (value, attr_value);
}

/**
 * g_file_info_set_attribute_string:
 * @info: a #GFileInfo.
 * @attribute: a file attribute key.
 * @attr_value: a UTF-8 string.
 *
 * Sets the @attribute to contain the given @attr_value,
 * if possible.
 **/
void
g_file_info_set_attribute_string (GFileInfo  *info,
				  const char *attribute,
				  const char *attr_value)
{
  g_return_if_fail (G_IS_FILE_INFO (info));
  g_return_if_fail (attribute != NULL && *attribute != '\0');
  g_return_if_fail (attr_value != NULL);

  _g_file_info_set_attribute_string_by_id (info,
                                           lookup_attribute (attribute),
                                           attr_value);
}

void
_g_file_info_set_attribute_byte_string_by_id (GFileInfo  *info,
                                              guint32     attribute,
				              const char *attr_value)
{
  GFileAttributeValue *value;

  value = g_file_info_create_value (info, attribute);
  if (value)
    _g_file_attribute_value_set_byte_string (value, attr_value);
}

/**
 * g_file_info_set_attribute_byte_string:
 * @info: a #GFileInfo.
 * @attribute: a file attribute key.
 * @attr_value: a byte string.
 *
 * Sets the @attribute to contain the given @attr_value,
 * if possible.
 **/
void
g_file_info_set_attribute_byte_string (GFileInfo  *info,
				       const char *attribute,
				       const char *attr_value)
{
  g_return_if_fail (G_IS_FILE_INFO (info));
  g_return_if_fail (attribute != NULL && *attribute != '\0');
  g_return_if_fail (attr_value != NULL);

  _g_file_info_set_attribute_byte_string_by_id (info,
                                                lookup_attribute (attribute),
                                                attr_value);
}

void
_g_file_info_set_attribute_boolean_by_id (GFileInfo *info,
                                          guint32    attribute,
				          gboolean   attr_value)
{
  GFileAttributeValue *value;

  value = g_file_info_create_value (info, attribute);
  if (value)
    _g_file_attribute_value_set_boolean (value, attr_value);
}

/**
 * g_file_info_set_attribute_boolean:
 * @info: a #GFileInfo.
 * @attribute: a file attribute key.
 * @attr_value: a boolean value.
 *
 * Sets the @attribute to contain the given @attr_value,
 * if possible.
 **/
void
g_file_info_set_attribute_boolean (GFileInfo  *info,
				   const char *attribute,
				   gboolean    attr_value)
{
  g_return_if_fail (G_IS_FILE_INFO (info));
  g_return_if_fail (attribute != NULL && *attribute != '\0');

  _g_file_info_set_attribute_boolean_by_id (info,
                                            lookup_attribute (attribute),
                                            attr_value);
}

void
_g_file_info_set_attribute_uint32_by_id (GFileInfo *info,
                                         guint32    attribute,
				         guint32    attr_value)
{
  GFileAttributeValue *value;

  value = g_file_info_create_value (info, attribute);
  if (value)
    _g_file_attribute_value_set_uint32 (value, attr_value);
}

/**
 * g_file_info_set_attribute_uint32:
 * @info: a #GFileInfo.
 * @attribute: a file attribute key.
 * @attr_value: an unsigned 32-bit integer.
 *
 * Sets the @attribute to contain the given @attr_value,
 * if possible.
 **/
void
g_file_info_set_attribute_uint32 (GFileInfo  *info,
				  const char *attribute,
				  guint32     attr_value)
{
  g_return_if_fail (G_IS_FILE_INFO (info));
  g_return_if_fail (attribute != NULL && *attribute != '\0');

  _g_file_info_set_attribute_uint32_by_id (info,
                                           lookup_attribute (attribute),
                                           attr_value);
}

void
_g_file_info_set_attribute_int32_by_id (GFileInfo *info,
                                        guint32    attribute,
				        gint32     attr_value)
{
  GFileAttributeValue *value;

  value = g_file_info_create_value (info, attribute);
  if (value)
    _g_file_attribute_value_set_int32 (value, attr_value);
}

/**
 * g_file_info_set_attribute_int32:
 * @info: a #GFileInfo.
 * @attribute: a file attribute key.
 * @attr_value: a signed 32-bit integer
 *
 * Sets the @attribute to contain the given @attr_value,
 * if possible.
 **/
void
g_file_info_set_attribute_int32 (GFileInfo  *info,
                                 const char *attribute,
                                 gint32      attr_value)
{
  g_return_if_fail (G_IS_FILE_INFO (info));
  g_return_if_fail (attribute != NULL && *attribute != '\0');

  _g_file_info_set_attribute_int32_by_id (info,
                                          lookup_attribute (attribute),
                                          attr_value);
}

void
_g_file_info_set_attribute_uint64_by_id (GFileInfo *info,
                                         guint32    attribute,
				         guint64    attr_value)
{
  GFileAttributeValue *value;

  value = g_file_info_create_value (info, attribute);
  if (value)
    _g_file_attribute_value_set_uint64 (value, attr_value);
}

/**
 * g_file_info_set_attribute_uint64:
 * @info: a #GFileInfo.
 * @attribute: a file attribute key.
 * @attr_value: an unsigned 64-bit integer.
 *
 * Sets the @attribute to contain the given @attr_value,
 * if possible.
 **/
void
g_file_info_set_attribute_uint64 (GFileInfo  *info,
				  const char *attribute,
				  guint64     attr_value)
{
  g_return_if_fail (G_IS_FILE_INFO (info));
  g_return_if_fail (attribute != NULL && *attribute != '\0');

  _g_file_info_set_attribute_uint64_by_id (info,
                                           lookup_attribute (attribute),
                                           attr_value);
}

void
_g_file_info_set_attribute_int64_by_id (GFileInfo *info,
                                        guint32    attribute,
				        gint64     attr_value)
{
  GFileAttributeValue *value;

  value = g_file_info_create_value (info, attribute);
  if (value)
    _g_file_attribute_value_set_int64 (value, attr_value);
}

/**
 * g_file_info_set_attribute_int64:
 * @info: a #GFileInfo.
 * @attribute: attribute name to set.
 * @attr_value: int64 value to set attribute to.
 *
 * Sets the @attribute to contain the given @attr_value,
 * if possible.
 *
 **/
void
g_file_info_set_attribute_int64  (GFileInfo  *info,
				  const char *attribute,
				  gint64      attr_value)
{
  g_return_if_fail (G_IS_FILE_INFO (info));
  g_return_if_fail (attribute != NULL && *attribute != '\0');

  _g_file_info_set_attribute_int64_by_id (info,
                                          lookup_attribute (attribute),
                                          attr_value);
}

/* Helper getters */
/**
 * g_file_info_get_deletion_date:
 * @info: a #GFileInfo.
 *
 * Returns the #GDateTime representing the deletion date of the file, as
 * available in G_FILE_ATTRIBUTE_TRASH_DELETION_DATE. If the
 * G_FILE_ATTRIBUTE_TRASH_DELETION_DATE attribute is unset, %NULL is returned.
 *
 * Returns: a #GDateTime, or %NULL.
 *
 * Since: 2.36
 **/
GDateTime *
g_file_info_get_deletion_date (GFileInfo *info)
{
  static guint32 attr = 0;
  GFileAttributeValue *value;
  const char *date_str;
  GTimeVal tv;

  g_return_val_if_fail (G_IS_FILE_INFO (info), FALSE);

  if (attr == 0)
    attr = lookup_attribute (G_FILE_ATTRIBUTE_TRASH_DELETION_DATE);

  value = g_file_info_find_value (info, attr);
  date_str = _g_file_attribute_value_get_string (value);
  if (!date_str)
    return NULL;

  if (g_time_val_from_iso8601 (date_str, &tv) == FALSE)
    return NULL;

  return g_date_time_new_from_timeval_local (&tv);
}

/**
 * g_file_info_get_file_type:
 * @info: a #GFileInfo.
 *
 * Gets a file's type (whether it is a regular file, symlink, etc).
 * This is different from the file's content type, see g_file_info_get_content_type().
 *
 * Returns: a #GFileType for the given file.
 **/
GFileType
g_file_info_get_file_type (GFileInfo *info)
{
  static guint32 attr = 0;
  GFileAttributeValue *value;

  g_return_val_if_fail (G_IS_FILE_INFO (info), G_FILE_TYPE_UNKNOWN);

  if (attr == 0)
    attr = lookup_attribute (G_FILE_ATTRIBUTE_STANDARD_TYPE);

  value = g_file_info_find_value (info, attr);
  return (GFileType)_g_file_attribute_value_get_uint32 (value);
}

/**
 * g_file_info_get_is_hidden:
 * @info: a #GFileInfo.
 *
 * Checks if a file is hidden.
 *
 * Returns: %TRUE if the file is a hidden file, %FALSE otherwise.
 **/
gboolean
g_file_info_get_is_hidden (GFileInfo *info)
{
  static guint32 attr = 0;
  GFileAttributeValue *value;

  g_return_val_if_fail (G_IS_FILE_INFO (info), FALSE);

  if (attr == 0)
    attr = lookup_attribute (G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN);

  value = g_file_info_find_value (info, attr);
  return (GFileType)_g_file_attribute_value_get_boolean (value);
}

/**
 * g_file_info_get_is_backup:
 * @info: a #GFileInfo.
 *
 * Checks if a file is a backup file.
 *
 * Returns: %TRUE if file is a backup file, %FALSE otherwise.
 **/
gboolean
g_file_info_get_is_backup (GFileInfo *info)
{
  static guint32 attr = 0;
  GFileAttributeValue *value;

  g_return_val_if_fail (G_IS_FILE_INFO (info), FALSE);

  if (attr == 0)
    attr = lookup_attribute (G_FILE_ATTRIBUTE_STANDARD_IS_BACKUP);

  value = g_file_info_find_value (info, attr);
  return (GFileType)_g_file_attribute_value_get_boolean (value);
}

/**
 * g_file_info_get_is_symlink:
 * @info: a #GFileInfo.
 *
 * Checks if a file is a symlink.
 *
 * Returns: %TRUE if the given @info is a symlink.
 **/
gboolean
g_file_info_get_is_symlink (GFileInfo *info)
{
  static guint32 attr = 0;
  GFileAttributeValue *value;

  g_return_val_if_fail (G_IS_FILE_INFO (info), FALSE);

  if (attr == 0)
    attr = lookup_attribute (G_FILE_ATTRIBUTE_STANDARD_IS_SYMLINK);

  value = g_file_info_find_value (info, attr);
  return (GFileType)_g_file_attribute_value_get_boolean (value);
}

/**
 * g_file_info_get_name:
 * @info: a #GFileInfo.
 *
 * Gets the name for a file.
 *
 * Returns: (type filename): a string containing the file name.
 **/
const char *
g_file_info_get_name (GFileInfo *info)
{
  static guint32 attr = 0;
  GFileAttributeValue *value;

  g_return_val_if_fail (G_IS_FILE_INFO (info), NULL);

  if (attr == 0)
    attr = lookup_attribute (G_FILE_ATTRIBUTE_STANDARD_NAME);

  value = g_file_info_find_value (info, attr);
  return _g_file_attribute_value_get_byte_string (value);
}

/**
 * g_file_info_get_display_name:
 * @info: a #GFileInfo.
 *
 * Gets a display name for a file.
 *
 * Returns: a string containing the display name.
 **/
const char *
g_file_info_get_display_name (GFileInfo *info)
{
  static guint32 attr = 0;
  GFileAttributeValue *value;

  g_return_val_if_fail (G_IS_FILE_INFO (info), NULL);

  if (attr == 0)
    attr = lookup_attribute (G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME);

  value = g_file_info_find_value (info, attr);
  return _g_file_attribute_value_get_string (value);
}

/**
 * g_file_info_get_edit_name:
 * @info: a #GFileInfo.
 *
 * Gets the edit name for a file.
 *
 * Returns: a string containing the edit name.
 **/
const char *
g_file_info_get_edit_name (GFileInfo *info)
{
  static guint32 attr = 0;
  GFileAttributeValue *value;

  g_return_val_if_fail (G_IS_FILE_INFO (info), NULL);

  if (attr == 0)
    attr = lookup_attribute (G_FILE_ATTRIBUTE_STANDARD_EDIT_NAME);

  value = g_file_info_find_value (info, attr);
  return _g_file_attribute_value_get_string (value);
}

/**
 * g_file_info_get_icon:
 * @info: a #GFileInfo.
 *
 * Gets the icon for a file.
 *
 * Returns: (transfer none): #GIcon for the given @info.
 **/
GIcon *
g_file_info_get_icon (GFileInfo *info)
{
  static guint32 attr = 0;
  GFileAttributeValue *value;
  GObject *obj;

  g_return_val_if_fail (G_IS_FILE_INFO (info), NULL);

  if (attr == 0)
    attr = lookup_attribute (G_FILE_ATTRIBUTE_STANDARD_ICON);

  value = g_file_info_find_value (info, attr);
  obj = _g_file_attribute_value_get_object (value);
  if (G_IS_ICON (obj))
    return G_ICON (obj);
  return NULL;
}

/**
 * g_file_info_get_symbolic_icon:
 * @info: a #GFileInfo.
 *
 * Gets the symbolic icon for a file.
 *
 * Returns: (transfer none): #GIcon for the given @info.
 *
 * Since: 2.34
 **/
GIcon *
g_file_info_get_symbolic_icon (GFileInfo *info)
{
  static guint32 attr = 0;
  GFileAttributeValue *value;
  GObject *obj;

  g_return_val_if_fail (G_IS_FILE_INFO (info), NULL);

  if (attr == 0)
    attr = lookup_attribute (G_FILE_ATTRIBUTE_STANDARD_SYMBOLIC_ICON);

  value = g_file_info_find_value (info, attr);
  obj = _g_file_attribute_value_get_object (value);
  if (G_IS_ICON (obj))
    return G_ICON (obj);
  return NULL;
}

/**
 * g_file_info_get_content_type:
 * @info: a #GFileInfo.
 *
 * Gets the file's content type.
 *
 * Returns: a string containing the file's content type.
 **/
const char *
g_file_info_get_content_type (GFileInfo *info)
{
  static guint32 attr = 0;
  GFileAttributeValue *value;

  g_return_val_if_fail (G_IS_FILE_INFO (info), NULL);

  if (attr == 0)
    attr = lookup_attribute (G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE);

  value = g_file_info_find_value (info, attr);
  return _g_file_attribute_value_get_string (value);
}

/**
 * g_file_info_get_size:
 * @info: a #GFileInfo.
 *
 * Gets the file's size.
 *
 * Returns: a #goffset containing the file's size.
 **/
goffset
g_file_info_get_size (GFileInfo *info)
{
  static guint32 attr = 0;
  GFileAttributeValue *value;

  g_return_val_if_fail (G_IS_FILE_INFO (info), (goffset) 0);

  if (attr == 0)
    attr = lookup_attribute (G_FILE_ATTRIBUTE_STANDARD_SIZE);

  value = g_file_info_find_value (info, attr);
  return (goffset) _g_file_attribute_value_get_uint64 (value);
}

/**
 * g_file_info_get_modification_time:
 * @info: a #GFileInfo.
 * @result: (out caller-allocates): a #GTimeVal.
 *
 * Gets the modification time of the current @info and sets it
 * in @result.
 **/
void
g_file_info_get_modification_time (GFileInfo *info,
				   GTimeVal  *result)
{
  static guint32 attr_mtime = 0, attr_mtime_usec;
  GFileAttributeValue *value;

  g_return_if_fail (G_IS_FILE_INFO (info));
  g_return_if_fail (result != NULL);

  if (attr_mtime == 0)
    {
      attr_mtime = lookup_attribute (G_FILE_ATTRIBUTE_TIME_MODIFIED);
      attr_mtime_usec = lookup_attribute (G_FILE_ATTRIBUTE_TIME_MODIFIED_USEC);
    }

  value = g_file_info_find_value (info, attr_mtime);
  result->tv_sec = _g_file_attribute_value_get_uint64 (value);
  value = g_file_info_find_value (info, attr_mtime_usec);
  result->tv_usec = _g_file_attribute_value_get_uint32 (value);
}

/**
 * g_file_info_get_symlink_target:
 * @info: a #GFileInfo.
 *
 * Gets the symlink target for a given #GFileInfo.
 *
 * Returns: a string containing the symlink target.
 **/
const char *
g_file_info_get_symlink_target (GFileInfo *info)
{
  static guint32 attr = 0;
  GFileAttributeValue *value;

  g_return_val_if_fail (G_IS_FILE_INFO (info), NULL);

  if (attr == 0)
    attr = lookup_attribute (G_FILE_ATTRIBUTE_STANDARD_SYMLINK_TARGET);

  value = g_file_info_find_value (info, attr);
  return _g_file_attribute_value_get_byte_string (value);
}

/**
 * g_file_info_get_etag:
 * @info: a #GFileInfo.
 *
 * Gets the [entity tag][gfile-etag] for a given
 * #GFileInfo. See %G_FILE_ATTRIBUTE_ETAG_VALUE.
 *
 * Returns: a string containing the value of the "etag:value" attribute.
 **/
const char *
g_file_info_get_etag (GFileInfo *info)
{
  static guint32 attr = 0;
  GFileAttributeValue *value;

  g_return_val_if_fail (G_IS_FILE_INFO (info), NULL);

  if (attr == 0)
    attr = lookup_attribute (G_FILE_ATTRIBUTE_ETAG_VALUE);

  value = g_file_info_find_value (info, attr);
  return _g_file_attribute_value_get_string (value);
}

/**
 * g_file_info_get_sort_order:
 * @info: a #GFileInfo.
 *
 * Gets the value of the sort_order attribute from the #GFileInfo.
 * See %G_FILE_ATTRIBUTE_STANDARD_SORT_ORDER.
 *
 * Returns: a #gint32 containing the value of the "standard::sort_order" attribute.
 **/
gint32
g_file_info_get_sort_order (GFileInfo *info)
{
  static guint32 attr = 0;
  GFileAttributeValue *value;

  g_return_val_if_fail (G_IS_FILE_INFO (info), 0);

  if (attr == 0)
    attr = lookup_attribute (G_FILE_ATTRIBUTE_STANDARD_SORT_ORDER);

  value = g_file_info_find_value (info, attr);
  return _g_file_attribute_value_get_int32 (value);
}

/* Helper setters: */
/**
 * g_file_info_set_file_type:
 * @info: a #GFileInfo.
 * @type: a #GFileType.
 *
 * Sets the file type in a #GFileInfo to @type.
 * See %G_FILE_ATTRIBUTE_STANDARD_TYPE.
 **/
void
g_file_info_set_file_type (GFileInfo *info,
			   GFileType  type)
{
  static guint32 attr = 0;
  GFileAttributeValue *value;

  g_return_if_fail (G_IS_FILE_INFO (info));

  if (attr == 0)
    attr = lookup_attribute (G_FILE_ATTRIBUTE_STANDARD_TYPE);

  value = g_file_info_create_value (info, attr);
  if (value)
    _g_file_attribute_value_set_uint32 (value, type);
}

/**
 * g_file_info_set_is_hidden:
 * @info: a #GFileInfo.
 * @is_hidden: a #gboolean.
 *
 * Sets the "is_hidden" attribute in a #GFileInfo according to @is_hidden.
 * See %G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN.
 **/
void
g_file_info_set_is_hidden (GFileInfo *info,
			   gboolean   is_hidden)
{
  static guint32 attr = 0;
  GFileAttributeValue *value;

  g_return_if_fail (G_IS_FILE_INFO (info));

  if (attr == 0)
    attr = lookup_attribute (G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN);

  value = g_file_info_create_value (info, attr);
  if (value)
    _g_file_attribute_value_set_boolean (value, is_hidden);
}

/**
 * g_file_info_set_is_symlink:
 * @info: a #GFileInfo.
 * @is_symlink: a #gboolean.
 *
 * Sets the "is_symlink" attribute in a #GFileInfo according to @is_symlink.
 * See %G_FILE_ATTRIBUTE_STANDARD_IS_SYMLINK.
 **/
void
g_file_info_set_is_symlink (GFileInfo *info,
			    gboolean   is_symlink)
{
  static guint32 attr = 0;
  GFileAttributeValue *value;

  g_return_if_fail (G_IS_FILE_INFO (info));

  if (attr == 0)
    attr = lookup_attribute (G_FILE_ATTRIBUTE_STANDARD_IS_SYMLINK);

  value = g_file_info_create_value (info, attr);
  if (value)
    _g_file_attribute_value_set_boolean (value, is_symlink);
}

/**
 * g_file_info_set_name:
 * @info: a #GFileInfo.
 * @name: (type filename): a string containing a name.
 *
 * Sets the name attribute for the current #GFileInfo.
 * See %G_FILE_ATTRIBUTE_STANDARD_NAME.
 **/
void
g_file_info_set_name (GFileInfo  *info,
		      const char *name)
{
  static guint32 attr = 0;
  GFileAttributeValue *value;

  g_return_if_fail (G_IS_FILE_INFO (info));
  g_return_if_fail (name != NULL);

  if (attr == 0)
    attr = lookup_attribute (G_FILE_ATTRIBUTE_STANDARD_NAME);

  value = g_file_info_create_value (info, attr);
  if (value)
    _g_file_attribute_value_set_byte_string (value, name);
}

/**
 * g_file_info_set_display_name:
 * @info: a #GFileInfo.
 * @display_name: a string containing a display name.
 *
 * Sets the display name for the current #GFileInfo.
 * See %G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME.
 **/
void
g_file_info_set_display_name (GFileInfo  *info,
			      const char *display_name)
{
  static guint32 attr = 0;
  GFileAttributeValue *value;

  g_return_if_fail (G_IS_FILE_INFO (info));
  g_return_if_fail (display_name != NULL);

  if (attr == 0)
    attr = lookup_attribute (G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME);

  value = g_file_info_create_value (info, attr);
  if (value)
    _g_file_attribute_value_set_string (value, display_name);
}

/**
 * g_file_info_set_edit_name:
 * @info: a #GFileInfo.
 * @edit_name: a string containing an edit name.
 *
 * Sets the edit name for the current file.
 * See %G_FILE_ATTRIBUTE_STANDARD_EDIT_NAME.
 **/
void
g_file_info_set_edit_name (GFileInfo  *info,
			   const char *edit_name)
{
  static guint32 attr = 0;
  GFileAttributeValue *value;

  g_return_if_fail (G_IS_FILE_INFO (info));
  g_return_if_fail (edit_name != NULL);

  if (attr == 0)
    attr = lookup_attribute (G_FILE_ATTRIBUTE_STANDARD_EDIT_NAME);

  value = g_file_info_create_value (info, attr);
  if (value)
    _g_file_attribute_value_set_string (value, edit_name);
}

/**
 * g_file_info_set_icon:
 * @info: a #GFileInfo.
 * @icon: a #GIcon.
 *
 * Sets the icon for a given #GFileInfo.
 * See %G_FILE_ATTRIBUTE_STANDARD_ICON.
 **/
void
g_file_info_set_icon (GFileInfo *info,
		      GIcon     *icon)
{
  static guint32 attr = 0;
  GFileAttributeValue *value;

  g_return_if_fail (G_IS_FILE_INFO (info));
  g_return_if_fail (G_IS_ICON (icon));

  if (attr == 0)
    attr = lookup_attribute (G_FILE_ATTRIBUTE_STANDARD_ICON);

  value = g_file_info_create_value (info, attr);
  if (value)
    _g_file_attribute_value_set_object (value, G_OBJECT (icon));
}

/**
 * g_file_info_set_symbolic_icon:
 * @info: a #GFileInfo.
 * @icon: a #GIcon.
 *
 * Sets the symbolic icon for a given #GFileInfo.
 * See %G_FILE_ATTRIBUTE_STANDARD_SYMBOLIC_ICON.
 *
 * Since: 2.34
 **/
void
g_file_info_set_symbolic_icon (GFileInfo *info,
                               GIcon     *icon)
{
  static guint32 attr = 0;
  GFileAttributeValue *value;

  g_return_if_fail (G_IS_FILE_INFO (info));
  g_return_if_fail (G_IS_ICON (icon));

  if (attr == 0)
    attr = lookup_attribute (G_FILE_ATTRIBUTE_STANDARD_SYMBOLIC_ICON);

  value = g_file_info_create_value (info, attr);
  if (value)
    _g_file_attribute_value_set_object (value, G_OBJECT (icon));
}

/**
 * g_file_info_set_content_type:
 * @info: a #GFileInfo.
 * @content_type: a content type. See [GContentType][gio-GContentType]
 *
 * Sets the content type attribute for a given #GFileInfo.
 * See %G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE.
 **/
void
g_file_info_set_content_type (GFileInfo  *info,
			      const char *content_type)
{
  static guint32 attr = 0;
  GFileAttributeValue *value;

  g_return_if_fail (G_IS_FILE_INFO (info));
  g_return_if_fail (content_type != NULL);

  if (attr == 0)
    attr = lookup_attribute (G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE);

  value = g_file_info_create_value (info, attr);
  if (value)
    _g_file_attribute_value_set_string (value, content_type);
}

/**
 * g_file_info_set_size:
 * @info: a #GFileInfo.
 * @size: a #goffset containing the file's size.
 *
 * Sets the %G_FILE_ATTRIBUTE_STANDARD_SIZE attribute in the file info
 * to the given size.
 **/
void
g_file_info_set_size (GFileInfo *info,
		      goffset    size)
{
  static guint32 attr = 0;
  GFileAttributeValue *value;

  g_return_if_fail (G_IS_FILE_INFO (info));

  if (attr == 0)
    attr = lookup_attribute (G_FILE_ATTRIBUTE_STANDARD_SIZE);

  value = g_file_info_create_value (info, attr);
  if (value)
    _g_file_attribute_value_set_uint64 (value, size);
}

/**
 * g_file_info_set_modification_time:
 * @info: a #GFileInfo.
 * @mtime: a #GTimeVal.
 *
 * Sets the %G_FILE_ATTRIBUTE_TIME_MODIFIED attribute in the file
 * info to the given time value.
 **/
void
g_file_info_set_modification_time (GFileInfo *info,
				   GTimeVal  *mtime)
{
  static guint32 attr_mtime = 0, attr_mtime_usec;
  GFileAttributeValue *value;

  g_return_if_fail (G_IS_FILE_INFO (info));
  g_return_if_fail (mtime != NULL);

  if (attr_mtime == 0)
    {
      attr_mtime = lookup_attribute (G_FILE_ATTRIBUTE_TIME_MODIFIED);
      attr_mtime_usec = lookup_attribute (G_FILE_ATTRIBUTE_TIME_MODIFIED_USEC);
    }

  value = g_file_info_create_value (info, attr_mtime);
  if (value)
    _g_file_attribute_value_set_uint64 (value, mtime->tv_sec);
  value = g_file_info_create_value (info, attr_mtime_usec);
  if (value)
    _g_file_attribute_value_set_uint32 (value, mtime->tv_usec);
}

/**
 * g_file_info_set_symlink_target:
 * @info: a #GFileInfo.
 * @symlink_target: a static string containing a path to a symlink target.
 *
 * Sets the %G_FILE_ATTRIBUTE_STANDARD_SYMLINK_TARGET attribute in the file info
 * to the given symlink target.
 **/
void
g_file_info_set_symlink_target (GFileInfo  *info,
				const char *symlink_target)
{
  static guint32 attr = 0;
  GFileAttributeValue *value;

  g_return_if_fail (G_IS_FILE_INFO (info));
  g_return_if_fail (symlink_target != NULL);

  if (attr == 0)
    attr = lookup_attribute (G_FILE_ATTRIBUTE_STANDARD_SYMLINK_TARGET);

  value = g_file_info_create_value (info, attr);
  if (value)
    _g_file_attribute_value_set_byte_string (value, symlink_target);
}

/**
 * g_file_info_set_sort_order:
 * @info: a #GFileInfo.
 * @sort_order: a sort order integer.
 *
 * Sets the sort order attribute in the file info structure. See
 * %G_FILE_ATTRIBUTE_STANDARD_SORT_ORDER.
 **/
void
g_file_info_set_sort_order (GFileInfo *info,
			    gint32     sort_order)
{
  static guint32 attr = 0;
  GFileAttributeValue *value;

  g_return_if_fail (G_IS_FILE_INFO (info));

  if (attr == 0)
    attr = lookup_attribute (G_FILE_ATTRIBUTE_STANDARD_SORT_ORDER);

  value = g_file_info_create_value (info, attr);
  if (value)
    _g_file_attribute_value_set_int32 (value, sort_order);
}


typedef struct {
  guint32 id;
  guint32 mask;
} SubMatcher;

struct _GFileAttributeMatcher {
  gboolean all;
  gint ref;

  GArray *sub_matchers;

  /* Interator */
  guint32 iterator_ns;
  gint iterator_pos;
};

G_DEFINE_BOXED_TYPE (GFileAttributeMatcher, g_file_attribute_matcher,
                     g_file_attribute_matcher_ref,
                     g_file_attribute_matcher_unref)

static gint
compare_sub_matchers (gconstpointer a,
                      gconstpointer b)
{
  const SubMatcher *suba = a;
  const SubMatcher *subb = b;
  int diff;

  diff = suba->id - subb->id;

  if (diff)
    return diff;

  return suba->mask - subb->mask;
}

static gboolean
sub_matcher_matches (SubMatcher *matcher,
                     SubMatcher *submatcher)
{
  if ((matcher->mask & submatcher->mask) != matcher->mask)
    return FALSE;
  
  return matcher->id == (submatcher->id & matcher->mask);
}

/* Call this function after modifying a matcher.
 * It will ensure all the invariants other functions rely on.
 */
static GFileAttributeMatcher *
matcher_optimize (GFileAttributeMatcher *matcher)
{
  SubMatcher *submatcher, *compare;
  guint i, j;

  /* remove sub_matchers if we match everything anyway */
  if (matcher->all)
    {
      if (matcher->sub_matchers)
        {
          g_array_free (matcher->sub_matchers, TRUE);
          matcher->sub_matchers = NULL;
        }
      return matcher;
    }

  if (matcher->sub_matchers->len == 0)
    {
      g_file_attribute_matcher_unref (matcher);
      return NULL;
    }

  /* sort sub_matchers by id (and then mask), so we can bsearch
   * and compare matchers in O(N) instead of O(N²) */
  g_array_sort (matcher->sub_matchers, compare_sub_matchers);

  /* remove duplicates and specific matches when we match the whole namespace */
  j = 0;
  compare = &g_array_index (matcher->sub_matchers, SubMatcher, j);

  for (i = 1; i < matcher->sub_matchers->len; i++)
    {
      submatcher = &g_array_index (matcher->sub_matchers, SubMatcher, i);
      if (sub_matcher_matches (compare, submatcher))
        continue;

      j++;
      compare++;

      if (j < i)
        *compare = *submatcher;
    }

  g_array_set_size (matcher->sub_matchers, j + 1);

  return matcher;
}

/**
 * g_file_attribute_matcher_new:
 * @attributes: an attribute string to match.
 *
 * Creates a new file attribute matcher, which matches attributes
 * against a given string. #GFileAttributeMatchers are reference
 * counted structures, and are created with a reference count of 1. If
 * the number of references falls to 0, the #GFileAttributeMatcher is
 * automatically destroyed.
 *
 * The @attribute string should be formatted with specific keys separated
 * from namespaces with a double colon. Several "namespace::key" strings may be
 * concatenated with a single comma (e.g. "standard::type,standard::is-hidden").
 * The wildcard "*" may be used to match all keys and namespaces, or
 * "namespace::*" will match all keys in a given namespace.
 *
 * ## Examples of file attribute matcher strings and results
 *
 * - `"*"`: matches all attributes.
 * - `"standard::is-hidden"`: matches only the key is-hidden in the
 *   standard namespace.
 * - `"standard::type,unix::*"`: matches the type key in the standard
 *   namespace and all keys in the unix namespace.
 *
 * Returns: a #GFileAttributeMatcher
 */
GFileAttributeMatcher *
g_file_attribute_matcher_new (const char *attributes)
{
  char **split;
  char *colon;
  int i;
  GFileAttributeMatcher *matcher;

  if (attributes == NULL || *attributes == '\0')
    return NULL;

  matcher = g_malloc0 (sizeof (GFileAttributeMatcher));
  matcher->ref = 1;
  matcher->sub_matchers = g_array_new (FALSE, FALSE, sizeof (SubMatcher));

  split = g_strsplit (attributes, ",", -1);

  for (i = 0; split[i] != NULL; i++)
    {
      if (strcmp (split[i], "*") == 0)
	matcher->all = TRUE;
      else
	{
          SubMatcher s;

	  colon = strstr (split[i], "::");
	  if (colon != NULL &&
	      !(colon[2] == 0 ||
		(colon[2] == '*' &&
		 colon[3] == 0)))
	    {
	      s.id = lookup_attribute (split[i]);
	      s.mask = 0xffffffff;
	    }
	  else
	    {
	      if (colon)
		*colon = 0;

	      s.id = lookup_namespace (split[i]) << NS_POS;
	      s.mask = NS_MASK << NS_POS;
	    }

          g_array_append_val (matcher->sub_matchers, s);
	}
    }

  g_strfreev (split);

  matcher = matcher_optimize (matcher);

  return matcher;
}

/**
 * g_file_attribute_matcher_subtract:
 * @matcher: Matcher to subtract from 
 * @subtract: The matcher to subtract
 *
 * Subtracts all attributes of @subtract from @matcher and returns
 * a matcher that supports those attributes.
 *
 * Note that currently it is not possible to remove a single
 * attribute when the @matcher matches the whole namespace - or remove
 * a namespace or attribute when the matcher matches everything. This
 * is a limitation of the current implementation, but may be fixed
 * in the future.
 *
 * Returns: A file attribute matcher matching all attributes of
 *     @matcher that are not matched by @subtract
 **/
GFileAttributeMatcher *
g_file_attribute_matcher_subtract (GFileAttributeMatcher *matcher,
                                   GFileAttributeMatcher *subtract)
{
  GFileAttributeMatcher *result;
  guint mi, si;
  SubMatcher *msub, *ssub;

  if (matcher == NULL)
    return NULL;
  if (subtract == NULL)
    return g_file_attribute_matcher_ref (matcher);
  if (subtract->all)
    return NULL;
  if (matcher->all)
    return g_file_attribute_matcher_ref (matcher);

  result = g_malloc0 (sizeof (GFileAttributeMatcher));
  result->ref = 1;
  result->sub_matchers = g_array_new (FALSE, FALSE, sizeof (SubMatcher));

  si = 0;
  g_assert (subtract->sub_matchers->len > 0);
  ssub = &g_array_index (subtract->sub_matchers, SubMatcher, si);

  for (mi = 0; mi < matcher->sub_matchers->len; mi++)
    {
      msub = &g_array_index (matcher->sub_matchers, SubMatcher, mi);

retry:
      if (sub_matcher_matches (ssub, msub))
        continue;

      si++;
      if (si >= subtract->sub_matchers->len)
        break;

      ssub = &g_array_index (subtract->sub_matchers, SubMatcher, si);
      if (ssub->id <= msub->id)
        goto retry;

      g_array_append_val (result->sub_matchers, *msub);
    }

  if (mi < matcher->sub_matchers->len)
    g_array_append_vals (result->sub_matchers,
                         &g_array_index (matcher->sub_matchers, SubMatcher, mi),
                         matcher->sub_matchers->len - mi);

  result = matcher_optimize (result);

  return result;
}

/**
 * g_file_attribute_matcher_ref:
 * @matcher: a #GFileAttributeMatcher.
 *
 * References a file attribute matcher.
 *
 * Returns: a #GFileAttributeMatcher.
 **/
GFileAttributeMatcher *
g_file_attribute_matcher_ref (GFileAttributeMatcher *matcher)
{
  if (matcher)
    {
      g_return_val_if_fail (matcher->ref > 0, NULL);
      g_atomic_int_inc (&matcher->ref);
    }
  return matcher;
}

/**
 * g_file_attribute_matcher_unref:
 * @matcher: a #GFileAttributeMatcher.
 *
 * Unreferences @matcher. If the reference count falls below 1,
 * the @matcher is automatically freed.
 *
 **/
void
g_file_attribute_matcher_unref (GFileAttributeMatcher *matcher)
{
  if (matcher)
    {
      g_return_if_fail (matcher->ref > 0);

      if (g_atomic_int_dec_and_test (&matcher->ref))
	{
	  if (matcher->sub_matchers)
	    g_array_free (matcher->sub_matchers, TRUE);

	  g_free (matcher);
	}
    }
}

/**
 * g_file_attribute_matcher_matches_only:
 * @matcher: a #GFileAttributeMatcher.
 * @attribute: a file attribute key.
 *
 * Checks if a attribute matcher only matches a given attribute. Always
 * returns %FALSE if "*" was used when creating the matcher.
 *
 * Returns: %TRUE if the matcher only matches @attribute. %FALSE otherwise.
 **/
gboolean
g_file_attribute_matcher_matches_only (GFileAttributeMatcher *matcher,
				       const char            *attribute)
{
  SubMatcher *sub_matcher;
  guint32 id;

  g_return_val_if_fail (attribute != NULL && *attribute != '\0', FALSE);

  if (matcher == NULL ||
      matcher->all)
    return FALSE;

  if (matcher->sub_matchers->len != 1)
    return FALSE;
  
  id = lookup_attribute (attribute);
  
  sub_matcher = &g_array_index (matcher->sub_matchers, SubMatcher, 0);
  
  return sub_matcher->id == id &&
         sub_matcher->mask == 0xffffffff;
}

static gboolean
matcher_matches_id (GFileAttributeMatcher *matcher,
                    guint32                id)
{
  SubMatcher *sub_matchers;
  int i;

  if (matcher->sub_matchers)
    {
      sub_matchers = (SubMatcher *)matcher->sub_matchers->data;
      for (i = 0; i < matcher->sub_matchers->len; i++)
	{
	  if (sub_matchers[i].id == (id & sub_matchers[i].mask))
	    return TRUE;
	}
    }

  return FALSE;
}

gboolean
_g_file_attribute_matcher_matches_id (GFileAttributeMatcher *matcher,
                                      guint32                id)
{
  /* We return a NULL matcher for an empty match string, so handle this */
  if (matcher == NULL)
    return FALSE;

  if (matcher->all)
    return TRUE;

  return matcher_matches_id (matcher, id);
}

/**
 * g_file_attribute_matcher_matches:
 * @matcher: a #GFileAttributeMatcher.
 * @attribute: a file attribute key.
 *
 * Checks if an attribute will be matched by an attribute matcher. If
 * the matcher was created with the "*" matching string, this function
 * will always return %TRUE.
 *
 * Returns: %TRUE if @attribute matches @matcher. %FALSE otherwise.
 **/
gboolean
g_file_attribute_matcher_matches (GFileAttributeMatcher *matcher,
				  const char            *attribute)
{
  g_return_val_if_fail (attribute != NULL && *attribute != '\0', FALSE);

  /* We return a NULL matcher for an empty match string, so handle this */
  if (matcher == NULL)
    return FALSE;

  if (matcher->all)
    return TRUE;

  return matcher_matches_id (matcher, lookup_attribute (attribute));
}

/* return TRUE -> all */
/**
 * g_file_attribute_matcher_enumerate_namespace:
 * @matcher: a #GFileAttributeMatcher.
 * @ns: a string containing a file attribute namespace.
 *
 * Checks if the matcher will match all of the keys in a given namespace.
 * This will always return %TRUE if a wildcard character is in use (e.g. if
 * matcher was created with "standard::*" and @ns is "standard", or if matcher was created
 * using "*" and namespace is anything.)
 *
 * TODO: this is awkwardly worded.
 *
 * Returns: %TRUE if the matcher matches all of the entries
 * in the given @ns, %FALSE otherwise.
 **/
gboolean
g_file_attribute_matcher_enumerate_namespace (GFileAttributeMatcher *matcher,
					      const char            *ns)
{
  SubMatcher *sub_matchers;
  int ns_id;
  int i;

  g_return_val_if_fail (ns != NULL && *ns != '\0', FALSE);

  /* We return a NULL matcher for an empty match string, so handle this */
  if (matcher == NULL)
    return FALSE;

  if (matcher->all)
    return TRUE;

  ns_id = lookup_namespace (ns) << NS_POS;

  if (matcher->sub_matchers)
    {
      sub_matchers = (SubMatcher *)matcher->sub_matchers->data;
      for (i = 0; i < matcher->sub_matchers->len; i++)
	{
	  if (sub_matchers[i].id == ns_id)
	    return TRUE;
	}
    }

  matcher->iterator_ns = ns_id;
  matcher->iterator_pos = 0;

  return FALSE;
}

/**
 * g_file_attribute_matcher_enumerate_next:
 * @matcher: a #GFileAttributeMatcher.
 *
 * Gets the next matched attribute from a #GFileAttributeMatcher.
 *
 * Returns: a string containing the next attribute or %NULL if
 * no more attribute exist.
 **/
const char *
g_file_attribute_matcher_enumerate_next (GFileAttributeMatcher *matcher)
{
  int i;
  SubMatcher *sub_matcher;

  /* We return a NULL matcher for an empty match string, so handle this */
  if (matcher == NULL)
    return NULL;

  while (1)
    {
      i = matcher->iterator_pos++;

      if (matcher->sub_matchers == NULL)
        return NULL;

      if (i < matcher->sub_matchers->len)
        sub_matcher = &g_array_index (matcher->sub_matchers, SubMatcher, i);
      else
        return NULL;

      if (sub_matcher->mask == 0xffffffff &&
	  (sub_matcher->id & (NS_MASK << NS_POS)) == matcher->iterator_ns)
	return get_attribute_for_id (sub_matcher->id);
    }
}

/**
 * g_file_attribute_matcher_to_string:
 * @matcher: (nullable): a #GFileAttributeMatcher.
 *
 * Prints what the matcher is matching against. The format will be 
 * equal to the format passed to g_file_attribute_matcher_new().
 * The output however, might not be identical, as the matcher may
 * decide to use a different order or omit needless parts.
 *
 * Returns: a string describing the attributes the matcher matches
 *   against or %NULL if @matcher was %NULL.
 *
 * Since: 2.32
 **/
char *
g_file_attribute_matcher_to_string (GFileAttributeMatcher *matcher)
{
  GString *string;
  guint i;

  if (matcher == NULL)
    return NULL;

  if (matcher->all)
    return g_strdup ("*");

  string = g_string_new ("");
  for (i = 0; i < matcher->sub_matchers->len; i++)
    {
      SubMatcher *submatcher = &g_array_index (matcher->sub_matchers, SubMatcher, i);

      if (i > 0)
        g_string_append_c (string, ',');

      g_string_append (string, get_attribute_for_id (submatcher->id));
    }

  return g_string_free (string, FALSE);
}
