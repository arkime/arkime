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

#include "gvdb-builder.h"
#include "gvdb-format.h"

#include <glib.h>
#include <fcntl.h>
#if !defined(G_OS_WIN32) || !defined(_MSC_VER)
#include <unistd.h>
#endif
#include <string.h>


struct _GvdbItem
{
  gchar *key;
  guint32 hash_value;
  guint32_le assigned_index;
  GvdbItem *parent;
  GvdbItem *sibling;
  GvdbItem *next;

  /* one of:
   * this:
   */
  GVariant *value;

  /* this: */
  GHashTable *table;

  /* or this: */
  GvdbItem *child;
};

static void
gvdb_item_free (gpointer data)
{
  GvdbItem *item = data;

  g_free (item->key);

  if (item->value)
    g_variant_unref (item->value);

  if (item->table)
    g_hash_table_unref (item->table);

  g_slice_free (GvdbItem, item);
}

GHashTable *
gvdb_hash_table_new (GHashTable  *parent,
                     const gchar *name_in_parent)
{
  GHashTable *table;

  table = g_hash_table_new_full (g_str_hash, g_str_equal,
                                 g_free, gvdb_item_free);

  if (parent)
    {
      GvdbItem *item;

      item = gvdb_hash_table_insert (parent, name_in_parent);
      gvdb_item_set_hash_table (item, table);
    }

  return table;
}

static guint32
djb_hash (const gchar *key)
{
  guint32 hash_value = 5381;

  while (*key)
    hash_value = hash_value * 33 + *(signed char *)key++;

  return hash_value;
}

GvdbItem *
gvdb_hash_table_insert (GHashTable  *table,
                        const gchar *key)
{
  GvdbItem *item;

  item = g_slice_new0 (GvdbItem);
  item->key = g_strdup (key);
  item->hash_value = djb_hash (key);

  g_hash_table_insert (table, g_strdup (key), item);

  return item;
}

void
gvdb_hash_table_insert_string (GHashTable  *table,
                               const gchar *key,
                               const gchar *value)
{
  GvdbItem *item;

  item = gvdb_hash_table_insert (table, key);
  gvdb_item_set_value (item, g_variant_new_string (value));
}

void
gvdb_item_set_value (GvdbItem *item,
                     GVariant *value)
{
  g_return_if_fail (!item->value && !item->table && !item->child);

  item->value = g_variant_ref_sink (value);
}

void
gvdb_item_set_hash_table (GvdbItem   *item,
                          GHashTable *table)
{
  g_return_if_fail (!item->value && !item->table && !item->child);

  item->table = g_hash_table_ref (table);
}

void
gvdb_item_set_parent (GvdbItem *item,
                      GvdbItem *parent)
{
  GvdbItem **node;

  g_return_if_fail (g_str_has_prefix (item->key, parent->key));
  g_return_if_fail (!parent->value && !parent->table);
  g_return_if_fail (!item->parent && !item->sibling);

  for (node = &parent->child; *node; node = &(*node)->sibling)
    if (strcmp ((*node)->key, item->key) > 0)
      break;

  item->parent = parent;
  item->sibling = *node;
  *node = item;
}

typedef struct
{
  GvdbItem **buckets;
  gint n_buckets;
} HashTable;

static HashTable *
hash_table_new (gint n_buckets)
{
  HashTable *table;

  table = g_slice_new (HashTable);
  table->buckets = g_new0 (GvdbItem *, n_buckets);
  table->n_buckets = n_buckets;

  return table;
}

static void
hash_table_free (HashTable *table)
{
  g_free (table->buckets);

  g_slice_free (HashTable, table);
}

static void
hash_table_insert (gpointer key,
                   gpointer value,
                   gpointer data)
{
  guint32 hash_value, bucket;
  HashTable *table = data;
  GvdbItem *item = value;

  hash_value = djb_hash (key);
  bucket = hash_value % table->n_buckets;
  item->next = table->buckets[bucket];
  table->buckets[bucket] = item;
}

static guint32_le
item_to_index (GvdbItem *item)
{
  if (item != NULL)
    return item->assigned_index;

  return guint32_to_le (-1u);
}

typedef struct
{
  GQueue *chunks;
  guint64 offset;
  gboolean byteswap;
} FileBuilder;

typedef struct
{
  gsize offset;
  gsize size;
  gpointer data;
} FileChunk;

static gpointer
file_builder_allocate (FileBuilder         *fb,
                       guint                alignment,
                       gsize                size,
                       struct gvdb_pointer *pointer)
{
  FileChunk *chunk;

  if (size == 0)
    return NULL;

  fb->offset += (-fb->offset) & (alignment - 1);
  chunk = g_slice_new (FileChunk);
  chunk->offset = fb->offset;
  chunk->size = size;
  chunk->data = g_malloc (size);

  pointer->start = guint32_to_le (fb->offset);
  fb->offset += size;
  pointer->end = guint32_to_le (fb->offset);

  g_queue_push_tail (fb->chunks, chunk);

  return chunk->data;
}

static void
file_builder_add_value (FileBuilder         *fb,
                        GVariant            *value,
                        struct gvdb_pointer *pointer)
{
  GVariant *variant, *normal;
  gpointer data;
  gsize size;

  if (fb->byteswap)
    {
      value = g_variant_byteswap (value);
      variant = g_variant_new_variant (value);
      g_variant_unref (value);
    }
  else
    variant = g_variant_new_variant (value);

  normal = g_variant_get_normal_form (variant);
  g_variant_unref (variant);

  size = g_variant_get_size (normal);
  data = file_builder_allocate (fb, 8, size, pointer);
  g_variant_store (normal, data);
  g_variant_unref (normal);
}

static void
file_builder_add_string (FileBuilder *fb,
                         const gchar *string,
                         guint32_le  *start,
                         guint16_le  *size)
{
  FileChunk *chunk;
  gsize length;

  length = strlen (string);

  chunk = g_slice_new (FileChunk);
  chunk->offset = fb->offset;
  chunk->size = length;
  chunk->data = g_malloc (length);
  if (length != 0)
    memcpy (chunk->data, string, length);

  *start = guint32_to_le (fb->offset);
  *size = guint16_to_le (length);
  fb->offset += length;

  g_queue_push_tail (fb->chunks, chunk);
}

static void
file_builder_allocate_for_hash (FileBuilder            *fb,
                                gsize                   n_buckets,
                                gsize                   n_items,
                                guint                   bloom_shift,
                                gsize                   n_bloom_words,
                                guint32_le            **bloom_filter,
                                guint32_le            **hash_buckets,
                                struct gvdb_hash_item **hash_items,
                                struct gvdb_pointer    *pointer)
{
  guint32_le bloom_hdr, table_hdr;
  guchar *data;
  gsize size;

  g_assert (n_bloom_words < (1u << 27));

  bloom_hdr = guint32_to_le (bloom_shift << 27 | n_bloom_words);
  table_hdr = guint32_to_le (n_buckets);

  size = sizeof bloom_hdr + sizeof table_hdr +
         n_bloom_words * sizeof (guint32_le) +
         n_buckets     * sizeof (guint32_le) +
         n_items       * sizeof (struct gvdb_hash_item);

  data = file_builder_allocate (fb, 4, size, pointer);

#define chunk(s) (size -= (s), data += (s), data - (s))
  memcpy (chunk (sizeof bloom_hdr), &bloom_hdr, sizeof bloom_hdr);
  memcpy (chunk (sizeof table_hdr), &table_hdr, sizeof table_hdr);
  *bloom_filter = (guint32_le *) chunk (n_bloom_words * sizeof (guint32_le));
  *hash_buckets = (guint32_le *) chunk (n_buckets * sizeof (guint32_le));
  *hash_items = (struct gvdb_hash_item *) chunk (n_items *
                  sizeof (struct gvdb_hash_item));
  g_assert (size == 0);
#undef chunk

  memset (*bloom_filter, 0, n_bloom_words * sizeof (guint32_le));
}

static void
file_builder_add_hash (FileBuilder         *fb,
                       GHashTable          *table,
                       struct gvdb_pointer *pointer)
{
  guint32_le *buckets, *bloom_filter;
  struct gvdb_hash_item *items;
  HashTable *mytable;
  GvdbItem *item;
  guint32 index;
  gint bucket;

  mytable = hash_table_new (g_hash_table_size (table));
  g_hash_table_foreach (table, hash_table_insert, mytable);
  index = 0;

  for (bucket = 0; bucket < mytable->n_buckets; bucket++)
    for (item = mytable->buckets[bucket]; item; item = item->next)
      item->assigned_index = guint32_to_le (index++);

  file_builder_allocate_for_hash (fb, mytable->n_buckets, index, 5, 0,
                                  &bloom_filter, &buckets, &items, pointer);

  index = 0;
  for (bucket = 0; bucket < mytable->n_buckets; bucket++)
    {
      buckets[bucket] = guint32_to_le (index);

      for (item = mytable->buckets[bucket]; item; item = item->next)
        {
          struct gvdb_hash_item *entry = items++;
          const gchar *basename;

          g_assert (index == guint32_from_le (item->assigned_index));
          entry->hash_value = guint32_to_le (item->hash_value);
          entry->parent = item_to_index (item->parent);
          entry->unused = 0;

          if (item->parent != NULL)
            basename = item->key + strlen (item->parent->key);
          else
            basename = item->key;

          file_builder_add_string (fb, basename,
                                   &entry->key_start,
                                   &entry->key_size);

          if (item->value != NULL)
            {
              g_assert (item->child == NULL && item->table == NULL);

              file_builder_add_value (fb, item->value, &entry->value.pointer);
              entry->type = 'v';
            }

          if (item->child != NULL)
            {
              guint32 children = 0, i = 0;
              guint32_le *offsets;
              GvdbItem *child;

              g_assert (item->table == NULL);

              for (child = item->child; child; child = child->sibling)
                children++;

              offsets = file_builder_allocate (fb, 4, 4 * children,
                                               &entry->value.pointer);
              entry->type = 'L';

              for (child = item->child; child; child = child->sibling)
                offsets[i++] = child->assigned_index;

              g_assert (children == i);
            }

          if (item->table != NULL)
            {
              entry->type = 'H';
              file_builder_add_hash (fb, item->table, &entry->value.pointer);
            }

          index++;
        }
    }

  hash_table_free (mytable);
}

static FileBuilder *
file_builder_new (gboolean byteswap)
{
  FileBuilder *builder;

  builder = g_slice_new (FileBuilder);
  builder->chunks = g_queue_new ();
  builder->offset = sizeof (struct gvdb_header);
  builder->byteswap = byteswap;

  return builder;
}

static GString *
file_builder_serialise (FileBuilder          *fb,
                        struct gvdb_pointer   root)
{
  struct gvdb_header header = { { 0, }, };
  GString *result;

  if (fb->byteswap)
    {
      header.signature[0] = GVDB_SWAPPED_SIGNATURE0;
      header.signature[1] = GVDB_SWAPPED_SIGNATURE1;
    }
  else
    {
      header.signature[0] = GVDB_SIGNATURE0;
      header.signature[1] = GVDB_SIGNATURE1;
    }

  result = g_string_new (NULL);

  header.root = root;
  g_string_append_len (result, (gpointer) &header, sizeof header);

  while (!g_queue_is_empty (fb->chunks))
    {
      FileChunk *chunk = g_queue_pop_head (fb->chunks);

      if (result->len != chunk->offset)
        {
          gchar zero[8] = { 0, };

          g_assert (chunk->offset > result->len);
          g_assert (chunk->offset - result->len < 8);

          g_string_append_len (result, zero, chunk->offset - result->len);
          g_assert (result->len == chunk->offset);
        }

      g_string_append_len (result, chunk->data, chunk->size);
      g_free (chunk->data);

      g_slice_free (FileChunk, chunk);
    }

  g_queue_free (fb->chunks);
  g_slice_free (FileBuilder, fb);

  return result;
}

gboolean
gvdb_table_write_contents (GHashTable   *table,
                           const gchar  *filename,
                           gboolean      byteswap,
                           GError      **error)
{
  struct gvdb_pointer root;
  gboolean status;
  FileBuilder *fb;
  GString *str;

  fb = file_builder_new (byteswap);
  file_builder_add_hash (fb, table, &root);
  str = file_builder_serialise (fb, root);

  status = g_file_set_contents (filename, str->str, str->len, error);
  g_string_free (str, TRUE);

  return status;
}
