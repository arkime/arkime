/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
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
 */

/*
 * Modified by the GLib Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GLib Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GLib at ftp://ftp.gtk.org/pub/gtk/.
 */

/*
 * MT safe
 */

#include "config.h"

#include "gcache.h"

#include "gslice.h"
#include "ghash.h"
#include "gtestutils.h"

/**
 * SECTION:caches
 * @title: Caches
 * @short_description: caches allow sharing of complex data structures
 *                     to save resources
 *
 * A #GCache allows sharing of complex data structures, in order to
 * save system resources.
 *
 * GCache uses keys and values. A GCache key describes the properties
 * of a particular resource. A GCache value is the actual resource.
 *
 * GCache has been marked as deprecated, since this API is rarely
 * used and not very actively maintained.
 */

typedef struct _GCacheNode  GCacheNode;

struct _GCacheNode
{
  /* A reference counted node */
  gpointer value;
  gint ref_count;
};

/**
 * GCache:
 *
 * The #GCache struct is an opaque data structure containing
 * information about a #GCache. It should only be accessed via the
 * following functions.
 *
 * Deprecated:2.32: Use a #GHashTable instead
 */
struct _GCache
{
  /* Called to create a value from a key */
  GCacheNewFunc value_new_func;

  /* Called to destroy a value */
  GCacheDestroyFunc value_destroy_func;

  /* Called to duplicate a key */
  GCacheDupFunc key_dup_func;

  /* Called to destroy a key */
  GCacheDestroyFunc key_destroy_func;

  /* Associates keys with nodes */
  GHashTable *key_table;

  /* Associates nodes with keys */
  GHashTable *value_table;
};

static inline GCacheNode*
g_cache_node_new (gpointer value)
{
  GCacheNode *node = g_slice_new (GCacheNode);
  node->value = value;
  node->ref_count = 1;
  return node;
}

static inline void
g_cache_node_destroy (GCacheNode *node)
{
  g_slice_free (GCacheNode, node);
}

/**
 * g_cache_new:
 * @value_new_func: a function to create a new object given a key.
 *                  This is called by g_cache_insert() if an object
 *                  with the given key does not already exist
 * @value_destroy_func: a function to destroy an object. It is called
 *                      by g_cache_remove() when the object is no
 *                      longer needed (i.e. its reference count drops
 *                      to 0)
 * @key_dup_func: a function to copy a key. It is called by
 *                g_cache_insert() if the key does not already exist in
 *                the #GCache
 * @key_destroy_func: a function to destroy a key. It is called by
 *                    g_cache_remove() when the object is no longer
 *                    needed (i.e. its reference count drops to 0)
 * @hash_key_func: a function to create a hash value from a key
 * @hash_value_func: a function to create a hash value from a value
 * @key_equal_func: a function to compare two keys. It should return
 *                  %TRUE if the two keys are equivalent
 *
 * Creates a new #GCache.
 *
 * Returns: a new #GCache
 *
 * Deprecated:2.32: Use a #GHashTable instead
 */

/**
 * GCacheNewFunc:
 * @key: a #GCache key
 *
 * Specifies the type of the @value_new_func function passed to
 * g_cache_new(). It is passed a #GCache key and should create the
 * value corresponding to the key.
 *
 * Returns: a new #GCache value corresponding to the key.
 */

/**
 * GCacheDestroyFunc:
 * @value: the #GCache value to destroy
 *
 * Specifies the type of the @value_destroy_func and @key_destroy_func
 * functions passed to g_cache_new(). The functions are passed a
 * pointer to the #GCache key or #GCache value and should free any
 * memory and other resources associated with it.
 */

/**
 * GCacheDupFunc:
 * @value: the #GCache key to destroy (__not__ a
 *         #GCache value as it seems)
 *
 * Specifies the type of the @key_dup_func function passed to
 * g_cache_new(). The function is passed a key
 * (__not__ a value as the prototype implies) and
 * should return a duplicate of the key.
 *
 * Returns: a copy of the #GCache key
 */
GCache*
g_cache_new (GCacheNewFunc      value_new_func,
             GCacheDestroyFunc  value_destroy_func,
             GCacheDupFunc      key_dup_func,
             GCacheDestroyFunc  key_destroy_func,
             GHashFunc          hash_key_func,
             GHashFunc          hash_value_func,
             GEqualFunc         key_equal_func)
{
  GCache *cache;

  g_return_val_if_fail (value_new_func != NULL, NULL);
  g_return_val_if_fail (value_destroy_func != NULL, NULL);
  g_return_val_if_fail (key_dup_func != NULL, NULL);
  g_return_val_if_fail (key_destroy_func != NULL, NULL);
  g_return_val_if_fail (hash_key_func != NULL, NULL);
  g_return_val_if_fail (hash_value_func != NULL, NULL);
  g_return_val_if_fail (key_equal_func != NULL, NULL);

  cache = g_slice_new (GCache);
  cache->value_new_func = value_new_func;
  cache->value_destroy_func = value_destroy_func;
  cache->key_dup_func = key_dup_func;
  cache->key_destroy_func = key_destroy_func;
  cache->key_table = g_hash_table_new (hash_key_func, key_equal_func);
  cache->value_table = g_hash_table_new (hash_value_func, NULL);

  return cache;
}

/**
 * g_cache_destroy:
 * @cache: a #GCache
 *
 * Frees the memory allocated for the #GCache.
 *
 * Note that it does not destroy the keys and values which were
 * contained in the #GCache.
 *
 * Deprecated:2.32: Use a #GHashTable instead
 */
void
g_cache_destroy (GCache *cache)
{
  g_return_if_fail (cache != NULL);

  g_hash_table_destroy (cache->key_table);
  g_hash_table_destroy (cache->value_table);
  g_slice_free (GCache, cache);
}

/**
 * g_cache_insert:
 * @cache: a #GCache
 * @key: a key describing a #GCache object
 *
 * Gets the value corresponding to the given key, creating it if
 * necessary. It first checks if the value already exists in the
 * #GCache, by using the @key_equal_func function passed to
 * g_cache_new(). If it does already exist it is returned, and its
 * reference count is increased by one. If the value does not currently
 * exist, if is created by calling the @value_new_func. The key is
 * duplicated by calling @key_dup_func and the duplicated key and value
 * are inserted into the #GCache.
 *
 * Returns: a pointer to a #GCache value
 *
 * Deprecated:2.32: Use a #GHashTable instead
 */
gpointer
g_cache_insert (GCache   *cache,
                gpointer  key)
{
  GCacheNode *node;
  gpointer value;

  g_return_val_if_fail (cache != NULL, NULL);

  node = g_hash_table_lookup (cache->key_table, key);
  if (node)
    {
      node->ref_count += 1;
      return node->value;
    }

  key = (* cache->key_dup_func) (key);
  value = (* cache->value_new_func) (key);
  node = g_cache_node_new (value);

  g_hash_table_insert (cache->key_table, key, node);
  g_hash_table_insert (cache->value_table, value, key);

  return node->value;
}

/**
 * g_cache_remove:
 * @cache: a #GCache
 * @value: the value to remove
 *
 * Decreases the reference count of the given value. If it drops to 0
 * then the value and its corresponding key are destroyed, using the
 * @value_destroy_func and @key_destroy_func passed to g_cache_new().
 *
 * Deprecated:2.32: Use a #GHashTable instead
 */
void
g_cache_remove (GCache        *cache,
                gconstpointer  value)
{
  GCacheNode *node;
  gpointer key;

  g_return_if_fail (cache != NULL);

  key = g_hash_table_lookup (cache->value_table, value);
  node = g_hash_table_lookup (cache->key_table, key);

  g_return_if_fail (node != NULL);

  node->ref_count -= 1;
  if (node->ref_count == 0)
    {
      g_hash_table_remove (cache->value_table, value);
      g_hash_table_remove (cache->key_table, key);

      (* cache->key_destroy_func) (key);
      (* cache->value_destroy_func) (node->value);
      g_cache_node_destroy (node);
    }
}

/**
 * g_cache_key_foreach:
 * @cache: a #GCache
 * @func: the function to call with each #GCache key
 * @user_data: user data to pass to the function
 *
 * Calls the given function for each of the keys in the #GCache.
 *
 * NOTE @func is passed three parameters, the value and key of a cache
 * entry and the @user_data. The order of value and key is different
 * from the order in which g_hash_table_foreach() passes key-value
 * pairs to its callback function !
 *
 * Deprecated:2.32: Use a #GHashTable instead
 */
void
g_cache_key_foreach (GCache   *cache,
                     GHFunc    func,
                     gpointer  user_data)
{
  g_return_if_fail (cache != NULL);
  g_return_if_fail (func != NULL);

  g_hash_table_foreach (cache->value_table, func, user_data);
}

/**
 * g_cache_value_foreach:
 * @cache: a #GCache
 * @func: the function to call with each #GCache value
 * @user_data: user data to pass to the function
 *
 * Calls the given function for each of the values in the #GCache.
 *
 * Deprecated:2.10: The reason is that it passes pointers to internal
 *    data structures to @func; use g_cache_key_foreach() instead
 */
void
g_cache_value_foreach (GCache   *cache,
                       GHFunc    func,
                       gpointer  user_data)
{
  g_return_if_fail (cache != NULL);
  g_return_if_fail (func != NULL);

  g_hash_table_foreach (cache->key_table, func, user_data);
}
