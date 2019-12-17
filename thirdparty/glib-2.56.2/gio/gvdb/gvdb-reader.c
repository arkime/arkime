/*
 * Copyright © 2010 Codethink Limited
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
 * Author: Ryan Lortie <desrt@desrt.ca>
 */

#include "gvdb-reader.h"
#include "gvdb-format.h"

#include <string.h>

struct _GvdbTable {
  gint ref_count;

  const gchar *data;
  gsize size;

  gpointer user_data;
  GvdbRefFunc ref_user_data;
  GDestroyNotify unref_user_data;

  gboolean byteswapped;
  gboolean trusted;

  const guint32_le *bloom_words;
  guint32 n_bloom_words;
  guint bloom_shift;

  const guint32_le *hash_buckets;
  guint32 n_buckets;

  struct gvdb_hash_item *hash_items;
  guint32 n_hash_items;
};

static const gchar *
gvdb_table_item_get_key (GvdbTable                   *file,
                         const struct gvdb_hash_item *item,
                         gsize                       *size)
{
  guint32 start, end;

  start = guint32_from_le (item->key_start);
  *size = guint16_from_le (item->key_size);
  end = start + *size;

  if G_UNLIKELY (start > end || end > file->size)
    return NULL;

  return file->data + start;
}

static gconstpointer
gvdb_table_dereference (GvdbTable                 *file,
                        const struct gvdb_pointer *pointer,
                        gint                       alignment,
                        gsize                     *size)
{
  guint32 start, end;

  start = guint32_from_le (pointer->start);
  end = guint32_from_le (pointer->end);

  if G_UNLIKELY (start > end || end > file->size || start & (alignment - 1))
    return NULL;

  *size = end - start;

  return file->data + start;
}

static void
gvdb_table_setup_root (GvdbTable                 *file,
                       const struct gvdb_pointer *pointer)
{
  const struct gvdb_hash_header *header;
  guint32 n_bloom_words;
  guint32 n_buckets;
  gsize size;

  header = gvdb_table_dereference (file, pointer, 4, &size);

  if G_UNLIKELY (header == NULL || size < sizeof *header)
    return;

  size -= sizeof *header;

  n_bloom_words = guint32_from_le (header->n_bloom_words);
  n_buckets = guint32_from_le (header->n_buckets);
  n_bloom_words &= (1u << 27) - 1;

  if G_UNLIKELY (n_bloom_words * sizeof (guint32_le) > size)
    return;

  file->bloom_words = (gpointer) (header + 1);
  size -= n_bloom_words * sizeof (guint32_le);
  file->n_bloom_words = n_bloom_words;

  if G_UNLIKELY (n_buckets > G_MAXUINT / sizeof (guint32_le) ||
                 n_buckets * sizeof (guint32_le) > size)
    return;

  file->hash_buckets = file->bloom_words + file->n_bloom_words;
  size -= n_buckets * sizeof (guint32_le);
  file->n_buckets = n_buckets;

  if G_UNLIKELY (size % sizeof (struct gvdb_hash_item))
    return;

  file->hash_items = (gpointer) (file->hash_buckets + n_buckets);
  file->n_hash_items = size / sizeof (struct gvdb_hash_item);
}

static GvdbTable *
new_from_data (const void    *data,
	       gsize          data_len,
	       gboolean       trusted,
	       gpointer       user_data,
	       GvdbRefFunc    ref,
	       GDestroyNotify unref,
	       const char    *filename,
	       GError       **error)
{
  GvdbTable *file;

  file = g_slice_new0 (GvdbTable);
  file->data = data;
  file->size = data_len;
  file->trusted = trusted;
  file->ref_count = 1;
  file->ref_user_data = ref;
  file->unref_user_data = unref;
  file->user_data = user_data;

  if (sizeof (struct gvdb_header) <= file->size)
    {
      const struct gvdb_header *header = (gpointer) file->data;

      if (header->signature[0] == GVDB_SIGNATURE0 &&
          header->signature[1] == GVDB_SIGNATURE1 &&
          guint32_from_le (header->version) == 0)
        file->byteswapped = FALSE;

      else if (header->signature[0] == GVDB_SWAPPED_SIGNATURE0 &&
               header->signature[1] == GVDB_SWAPPED_SIGNATURE1 &&
               guint32_from_le (header->version) == 0)
        file->byteswapped = TRUE;

      else
        {
	  if (filename)
	    g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
			 "%s: invalid header", filename);
	  else
	    g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
			 "invalid gvdb header");
          g_slice_free (GvdbTable, file);
	  if (unref)
	    unref (user_data);

          return NULL;
        }

      gvdb_table_setup_root (file, &header->root);
    }

  return file;
}

/**
 * gvdb_table_new:
 * @filename: the path to the hash file
 * @trusted: if the contents of @filename are trusted
 * @error: %NULL, or a pointer to a %NULL #GError
 *
 * Creates a new #GvdbTable from the contents of the file found at
 * @filename.
 *
 * The only time this function fails is if the file cannot be opened.
 * In that case, the #GError that is returned will be an error from
 * g_mapped_file_new().
 *
 * An empty or otherwise corrupted file is considered to be a valid
 * #GvdbTable with no entries.
 *
 * You should call gvdb_table_unref() on the return result when you no
 * longer require it.
 *
 * Returns: a new #GvdbTable
 **/
GvdbTable *
gvdb_table_new (const gchar  *filename,
                gboolean      trusted,
                GError      **error)
{
  GMappedFile *mapped;

  if ((mapped = g_mapped_file_new (filename, FALSE, error)) == NULL)
    return NULL;

  return new_from_data (g_mapped_file_get_contents (mapped),
			g_mapped_file_get_length (mapped),
			trusted,
			mapped,
			(GvdbRefFunc)g_mapped_file_ref,
			(GDestroyNotify)g_mapped_file_unref,
			filename,
			error);
}

/**
 * gvdb_table_new_from_data:
 * @data: the data
 * @data_len: the length of @data in bytes
 * @trusted: if the contents of @data are trusted
 * @user_data: User supplied data that owns @data
 * @ref: Ref function for @user_data
 * @unref: Unref function for @user_data
 *
 * Creates a new #GvdbTable from the data in @data.
 *
 * An empty or otherwise corrupted data is considered to be a valid
 * #GvdbTable with no entries.
 *
 * You should call gvdb_table_unref() on the return result when you no
 * longer require it.
 *
 * Returns: a new #GvdbTable
 **/
GvdbTable *
gvdb_table_new_from_data (const void    *data,
			  gsize          data_len,
			  gboolean       trusted,
			  gpointer       user_data,
			  GvdbRefFunc    ref,
			  GDestroyNotify unref,
			  GError        **error)
{
  return new_from_data (data, data_len,
			trusted,
			user_data, ref, unref,
			NULL,
			error);
}

static gboolean
gvdb_table_bloom_filter (GvdbTable *file,
                          guint32    hash_value)
{
  guint32 word, mask;

  if (file->n_bloom_words == 0)
    return TRUE;

  word = (hash_value / 32) % file->n_bloom_words;
  mask = 1 << (hash_value & 31);
  mask |= 1 << ((hash_value >> file->bloom_shift) & 31);

  return (guint32_from_le (file->bloom_words[word]) & mask) == mask;
}

static gboolean
gvdb_table_check_name (GvdbTable             *file,
                       struct gvdb_hash_item *item,
                       const gchar           *key,
                       guint                  key_length)
{
  const gchar *this_key;
  gsize this_size;
  guint32 parent;

  this_key = gvdb_table_item_get_key (file, item, &this_size);

  if G_UNLIKELY (this_key == NULL || this_size > key_length)
    return FALSE;

  key_length -= this_size;

  if G_UNLIKELY (memcmp (this_key, key + key_length, this_size) != 0)
    return FALSE;

  parent = guint32_from_le (item->parent);
  if (key_length == 0 && parent == 0xffffffffu)
    return TRUE;

  if G_LIKELY (parent < file->n_hash_items && this_size > 0)
    return gvdb_table_check_name (file,
                                   &file->hash_items[parent],
                                   key, key_length);

  return FALSE;
}

static const struct gvdb_hash_item *
gvdb_table_lookup (GvdbTable   *file,
                   const gchar *key,
                   gchar        type)
{
  guint32 hash_value = 5381;
  guint key_length;
  guint32 bucket;
  guint32 lastno;
  guint32 itemno;

  if G_UNLIKELY (file->n_buckets == 0 || file->n_hash_items == 0)
    return NULL;

  for (key_length = 0; key[key_length]; key_length++)
    hash_value = (hash_value * 33) + ((signed char *) key)[key_length];

  if (!gvdb_table_bloom_filter (file, hash_value))
    return NULL;

  bucket = hash_value % file->n_buckets;
  itemno = guint32_from_le (file->hash_buckets[bucket]);

  if (bucket == file->n_buckets - 1 ||
      (lastno = guint32_from_le(file->hash_buckets[bucket + 1])) > file->n_hash_items)
    lastno = file->n_hash_items;

  while G_LIKELY (itemno < lastno)
    {
      struct gvdb_hash_item *item = &file->hash_items[itemno];

      if (hash_value == guint32_from_le (item->hash_value))
        if G_LIKELY (gvdb_table_check_name (file, item, key, key_length))
          if G_LIKELY (item->type == type)
            return item;

      itemno++;
    }

  return NULL;
}

static const struct gvdb_hash_item *
gvdb_table_get_item (GvdbTable  *table,
                     guint32_le  item_no)
{
  guint32 item_no_native = guint32_from_le (item_no);

  if G_LIKELY (item_no_native < table->n_hash_items)
    return table->hash_items + item_no_native;

  return NULL;
}

static gboolean
gvdb_table_list_from_item (GvdbTable                    *table,
                           const struct gvdb_hash_item  *item,
                           const guint32_le            **list,
                           guint                        *length)
{
  gsize size;

  *list = gvdb_table_dereference (table, &item->value.pointer, 4, &size);

  if G_LIKELY (*list == NULL || size % 4)
    return FALSE;

  *length = size / 4;

  return TRUE;
}


/**
 * gvdb_table_list:
 * @file: a #GvdbTable
 * @key: a string
 *
 * List all of the keys that appear below @key.  The nesting of keys
 * within the hash file is defined by the program that created the hash
 * file.  One thing is constant: each item in the returned array can be
 * concatenated to @key to obtain the full name of that key.
 *
 * It is not possible to tell from this function if a given key is
 * itself a path, a value, or another hash table; you are expected to
 * know this for yourself.
 *
 * You should call g_strfreev() on the return result when you no longer
 * require it.
 *
 * Returns: a %NULL-terminated string array
 **/
gchar **
gvdb_table_list (GvdbTable   *file,
                 const gchar *key)
{
  const struct gvdb_hash_item *item;
  const guint32_le *list;
  gchar **strv;
  guint length;
  guint i;

  if ((item = gvdb_table_lookup (file, key, 'L')) == NULL)
    return NULL;

  if (!gvdb_table_list_from_item (file, item, &list, &length))
    return NULL;

  strv = g_new (gchar *, length + 1);
  for (i = 0; i < length; i++)
    {
      guint32 itemno = guint32_from_le (list[i]);

      if (itemno < file->n_hash_items)
        {
          const struct gvdb_hash_item *item;
          const gchar *string;
          gsize strsize;

          item = file->hash_items + itemno;

          string = gvdb_table_item_get_key (file, item, &strsize);

          if (string != NULL)
            strv[i] = g_strndup (string, strsize);
          else
            strv[i] = g_malloc0 (1);
        }
      else
        strv[i] = g_malloc0 (1);
    }

  strv[i] = NULL;

  return strv;
}

/**
 * gvdb_table_has_value:
 * @file: a #GvdbTable
 * @key: a string
 *
 * Checks for a value named @key in @file.
 *
 * Note: this function does not consider non-value nodes (other hash
 * tables, for example).
 *
 * Returns: %TRUE if @key is in the table
 **/
gboolean
gvdb_table_has_value (GvdbTable    *file,
                      const gchar  *key)
{
  return gvdb_table_lookup (file, key, 'v') != NULL;
}

static GVariant *
gvdb_table_value_from_item (GvdbTable                   *table,
                            const struct gvdb_hash_item *item)
{
  GVariant *variant, *value;
  gconstpointer data;
  gsize size;

  data = gvdb_table_dereference (table, &item->value.pointer, 8, &size);

  if G_UNLIKELY (data == NULL)
    return NULL;

  variant = g_variant_new_from_data (G_VARIANT_TYPE_VARIANT,
                                     data, size, table->trusted,
                                     table->unref_user_data,
                                     table->ref_user_data ? table->ref_user_data (table->user_data) : table->user_data);
  value = g_variant_get_variant (variant);
  g_variant_unref (variant);

  return value;
}

/**
 * gvdb_table_get_value:
 * @file: a #GvdbTable
 * @key: a string
 *
 * Looks up a value named @key in @file.
 *
 * If the value is not found then %NULL is returned.  Otherwise, a new
 * #GVariant instance is returned.  The #GVariant does not depend on the
 * continued existence of @file.
 *
 * You should call g_variant_unref() on the return result when you no
 * longer require it.
 *
 * Returns: a #GVariant, or %NULL
 **/
GVariant *
gvdb_table_get_value (GvdbTable    *file,
                      const gchar  *key)
{
  const struct gvdb_hash_item *item;
  GVariant *value;

  if ((item = gvdb_table_lookup (file, key, 'v')) == NULL)
    return NULL;

  value = gvdb_table_value_from_item (file, item);

  if (value && file->byteswapped)
    {
      GVariant *tmp;

      tmp = g_variant_byteswap (value);
      g_variant_unref (value);
      value = tmp;
    }

  return value;
}

/**
 * gvdb_table_get_raw_value:
 * @table: a #GvdbTable
 * @key: a string
 *
 * Looks up a value named @key in @file.
 *
 * This call is equivalent to gvdb_table_get_value() except that it
 * never byteswaps the value.
 *
 * Returns: a #GVariant, or %NULL
 **/
GVariant *
gvdb_table_get_raw_value (GvdbTable   *table,
                          const gchar *key)
{
  const struct gvdb_hash_item *item;

  if ((item = gvdb_table_lookup (table, key, 'v')) == NULL)
    return NULL;

  return gvdb_table_value_from_item (table, item);
}

/**
 * gvdb_table_get_table:
 * @file: a #GvdbTable
 * @key: a string
 *
 * Looks up the hash table named @key in @file.
 *
 * The toplevel hash table in a #GvdbTable can contain reference to
 * child hash tables (and those can contain further references...).
 *
 * If @key is not found in @file then %NULL is returned.  Otherwise, a
 * new #GvdbTable is returned, referring to the child hashtable as
 * contained in the file.  This newly-created #GvdbTable does not depend
 * on the continued existence of @file.
 *
 * You should call gvdb_table_unref() on the return result when you no
 * longer require it.
 *
 * Returns: a new #GvdbTable, or %NULL
 **/
GvdbTable *
gvdb_table_get_table (GvdbTable   *file,
                      const gchar *key)
{
  const struct gvdb_hash_item *item;
  GvdbTable *new;

  item = gvdb_table_lookup (file, key, 'H');

  if (item == NULL)
    return NULL;

  new = g_slice_new0 (GvdbTable);
  new->user_data = file->ref_user_data ? file->ref_user_data (file->user_data) : file->user_data;
  new->ref_user_data = file->ref_user_data;
  new->unref_user_data = file->unref_user_data;
  new->byteswapped = file->byteswapped;
  new->trusted = file->trusted;
  new->data = file->data;
  new->size = file->size;
  new->ref_count = 1;

  gvdb_table_setup_root (new, &item->value.pointer);

  return new;
}

/**
 * gvdb_table_ref:
 * @file: a #GvdbTable
 *
 * Increases the reference count on @file.
 *
 * Returns: a new reference on @file
 **/
GvdbTable *
gvdb_table_ref (GvdbTable *file)
{
  g_atomic_int_inc (&file->ref_count);

  return file;
}

/**
 * gvdb_table_unref:
 * @file: a #GvdbTable
 *
 * Decreases the reference count on @file, possibly freeing it.
 *
 * Since: 2.26
 **/
void
gvdb_table_unref (GvdbTable *file)
{
  if (g_atomic_int_dec_and_test (&file->ref_count))
    {
      if (file->unref_user_data)
	file->unref_user_data (file->user_data);
      g_slice_free (GvdbTable, file);
    }
}

/**
 * gvdb_table_is_valid:
 * @table: a #GvdbTable
 *
 * Checks if the table is still valid.
 *
 * An on-disk GVDB can be marked as invalid.  This happens when the file
 * has been replaced.  The appropriate action is typically to reopen the
 * file.
 *
 * Returns: %TRUE if @table is still valid
 **/
gboolean
gvdb_table_is_valid (GvdbTable *table)
{
  return !!*table->data;
}

/**
 * gvdb_table_walk:
 * @table: a #GvdbTable
 * @key: a key corresponding to a list
 * @open_func: the #GvdbWalkOpenFunc
 * @value_func: the #GvdbWalkValueFunc
 * @close_func: the #GvdbWalkCloseFunc
 * @user_data: data to pass to the callbacks
 *
 * Looks up the list at @key and iterate over the items in it.
 *
 * First, @open_func is called to signal that we are starting to iterate over
 * the list.  Then the list is iterated.  When all items in the list have been
 * iterated over, the @close_func is called.
 *
 * When iterating, if a given item in the list is a value then @value_func is
 * called.
 *
 * If a given item in the list is itself a list then @open_func is called.  If
 * that function returns %TRUE then the walk begins iterating the items in the
 * sublist, until there are no more items, at which point a matching
 * @close_func call is made.  If @open_func returns %FALSE then no iteration of
 * the sublist occurs and no corresponding @close_func call is made.
 **/
void
gvdb_table_walk (GvdbTable         *table,
                 const gchar       *key,
                 GvdbWalkOpenFunc   open_func,
                 GvdbWalkValueFunc  value_func,
                 GvdbWalkCloseFunc  close_func,
                 gpointer           user_data)
{
  const struct gvdb_hash_item *item;
  const guint32_le *pointers[64];
  const guint32_le *enders[64];
  gsize name_lengths[64];
  gint index = 0;

  item = gvdb_table_lookup (table, key, 'L');
  name_lengths[0] = 0;
  pointers[0] = NULL;
  enders[0] = NULL;
  goto start_here;

  while (index)
    {
      close_func (name_lengths[index], user_data);
      index--;

      while (pointers[index] < enders[index])
        {
          const gchar *name;
          gsize name_len;

          item = gvdb_table_get_item (table, *pointers[index]++);
 start_here:

          if (item != NULL &&
              (name = gvdb_table_item_get_key (table, item, &name_len)))
            {
              if (item->type == 'L')
                {
                  if (open_func (name, name_len, user_data))
                    {
                      guint length = 0;

                      index++;
                      g_assert (index < 64);

                      gvdb_table_list_from_item (table, item,
                                                 &pointers[index],
                                                 &length);
                      enders[index] = pointers[index] + length;
                      name_lengths[index] = name_len;
                    }
                }
              else if (item->type == 'v')
                {
                  GVariant *value;

                  value = gvdb_table_value_from_item (table, item);

                  if (value != NULL)
                    {
                      if (table->byteswapped)
                        {
                          GVariant *tmp;

                          tmp = g_variant_byteswap (value);
                          g_variant_unref (value);
                          value = tmp;
                        }

                      value_func (name, name_len, value, user_data);
                      g_variant_unref (value);
                    }
                }
            }
        }
    }
}
