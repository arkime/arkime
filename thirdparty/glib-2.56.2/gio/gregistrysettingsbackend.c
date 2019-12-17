/*
 * Copyright © 2009-10 Sam Thursfield
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
 * Author: Sam Thursfield <ssssam@gmail.com>
 */

/* GRegistryBackend implementation notes:
 *
 *   - All settings are stored under the path:
 *       HKEY_CURRENT_USER\Software\GSettings\
 *     This means all settings are per-user. Permissions and system-wide
 *     defaults are not implemented and will probably always be out of scope of
 *     the Windows port of GLib.
 *
 *   - The registry type system is limited. Most GVariant types are stored as
 *     literals via g_variant_print/parse(). Strings are stored without the
 *     quotes that GVariant requires. Integer types are stored as native
 *     REG_DWORD or REG_QWORD. The REG_MULTI_SZ (string array) type could be
 *     used to avoid flattening container types.
 *
 *   - Notifications are handled; the change event is watched for in a separate
 *     thread (Windows does not provide a callback API) which sends them with
 *     g_idle_add to the GLib main loop. The threading is done using Windows
 *     API functions, so there is no dependence on GThread.
 *
 *   - Windows doesn't tell us which value has changed. This means we have to
 *     maintain a cache of every stored value so we can play spot the
 *     difference. This should not be a performance issue because if you are
 *     storing thousands of values in GSettings, you are probably using it
 *     wrong.
 *
 *   - The cache stores the value as a registry type. Because many variants are
 *     stored as string representations, values which have changed equality but
 *     not equivalence may trigger spurious change notifications. GSettings
 *     users must already deal with this possibility and converting all data to
 *     GVariant values would be more effort.
 *
 *   - Because we have to cache every registry value locally, reads are done
 *     from the cache rather than directly from the registry. Writes update
 *     both. This means that the backend will not work if the watch thread is
 *     not running. A GSettings object always subscribes to changes so we can
 *     be sure that the watch thread will be running, but if for some reason
 *     the backend is being used directly you should bear that in mind.
 *
 *   - The registry is totally user-editable, so we are very forgiving about
 *     errors in the data we get.
 *
 *   - The registry uses backslashes as path separators. GSettings keys only
 *     allow [A-Za-z\-] so no escaping is needed. No attempt is made to solve
 *     clashes between keys differing only in case.
 *
 *   - RegCreateKeyW is used - We should always make the UTF-8 -> UTF-16
 *     conversion ourselves to avoid problems when the system language changes.
 *
 *   - The Windows registry has the following limitations: a key may not exceed
 *     255 characters, an entry's value may not exceed 16,383 characters, and
 *     all the values of a key may not exceed 65,535 characters.
 *
 *   - Terminology:
 *     * in GSettings, a 'key' is eg. /desktop/gnome/background/primary-color
 *     * in the registry, the 'key' is path, which contains some 'values'.
 *     * in this file, any GSettings key is a 'key', while a registry key is
 *       termed a 'path', which contains 'values'.
 *
 *   - My set of tests for this backend are currently at:
 *       http://gitorious.org/gsettings-gtk/gsettings-test.git
 *
 *   - There is an undocumented function in ntdll.dll which might be more
 *     than RegNotifyChangeKeyValue(), NtNotifyChangeKey:
 *       http://source.winehq.org/source/dlls/ntdll/reg.c#L618
 *       http://undocumented.ntinternals.net/UserMode/Undocumented%20Functions/NT%20Objects/Key/NtNotifyChangeKey.html
 *
 *   - If updating the cache ever becomes a performance issue it may make sense
 *     to use a red-black tree, but I don't currently think it's worth the time
 */

#include "config.h"

#include "gregistrysettingsbackend.h"
#include "gsettingsbackend.h"
#include "giomodule.h"

#include <windows.h>

//#define TRACE

/* GSettings' limit */
#define MAX_KEY_NAME_LENGTH   32

/* Testing (on Windows XP SP3) shows that WaitForMultipleObjects fails with
 * "The parameter is incorrect" after 64 watches. We need one for the
 * message_sent cond, which is allowed for in the way the watches_remaining
 * variable is used.
 */
#define MAX_WATCHES   64

/* A watch on one registry path and its subkeys */
typedef struct
{
  HANDLE event;
  HKEY hpath;
  char *prefix;
  GNode *cache_node;
} RegistryWatch;

/* Simple message passing for the watch thread. Not enough traffic to
 * justify a queue.
 */
typedef enum
{
  WATCH_THREAD_NONE,
  WATCH_THREAD_ADD_WATCH,
  WATCH_THREAD_REMOVE_WATCH,
  WATCH_THREAD_STOP
} WatchThreadMessageType;

typedef struct
{
  WatchThreadMessageType type;
  RegistryWatch watch;
} WatchThreadMessage;

typedef struct
{
  GSettingsBackend *owner;
  HANDLE *thread;

  /* Details of the things we are watching. */
  int watches_remaining;
  GPtrArray *events, *handles, *prefixes, *cache_nodes;

  /* Communication with the main thread. Only one message is stored at a time,
   * to make sure that messages are acknowledged before being overwritten we
   * create two events - one is signalled when a new message is set, the
   * other is signalled by the thread when it has processed the message.
   */
  WatchThreadMessage message;
  CRITICAL_SECTION *message_lock;
  HANDLE message_sent_event, message_received_event;
} WatchThreadState;

#define G_TYPE_REGISTRY_BACKEND      (g_registry_backend_get_type ())
#define G_REGISTRY_BACKEND(inst)     (G_TYPE_CHECK_INSTANCE_CAST ((inst),         \
                                      G_TYPE_REGISTRY_BACKEND, GRegistryBackend))
#define G_IS_REGISTRY_BACKEND(inst)  (G_TYPE_CHECK_INSTANCE_TYPE ((inst),         \
                                      G_TYPE_REGISTRY_BACKEND))

typedef GSettingsBackendClass GRegistryBackendClass;

typedef struct {
  GSettingsBackend parent_instance;

  gchar *base_path;
  gunichar2 *base_pathw;

  /* A stored copy of the whole tree being watched. When we receive a change notification
   * we have to check against this to see what has changed ... every time ...*/
  CRITICAL_SECTION *cache_lock;
  GNode *cache_root;

  WatchThreadState *watch;
} GRegistryBackend;

G_DEFINE_TYPE_WITH_CODE (GRegistryBackend,
                         g_registry_backend,
                         G_TYPE_SETTINGS_BACKEND,
                         g_io_extension_point_implement (G_SETTINGS_BACKEND_EXTENSION_POINT_NAME,
                                                         g_define_type_id, "registry", 90))

/**********************************************************************************
 * Utility functions
 **********************************************************************************/

#include <stdio.h>
static void
trace (const char *format,
       ...)
{
#ifdef TRACE
  va_list va; va_start (va, format);
  vprintf (format, va);
  fflush (stdout);
  va_end (va);
#endif
}

/* g_message including a windows error message. It is not useful to have an
 * equivalent function for g_warning because none of the registry errors can
 * result from programmer error (Microsoft programmers don't count), instead
 * they will mostly occur from people messing with the registry by hand. */
static void
g_message_win32_error (DWORD        result_code,
                       const gchar *format,
                      ...)
{
  va_list va;
  gchar *message;
  gchar *win32_error;
  gchar *win32_message;

  g_return_if_fail (result_code != 0);

  va_start (va, format);
  message = g_strdup_vprintf (format, va);
  win32_error = g_win32_error_message (result_code);
  win32_message = g_strdup_printf ("%s: %s", message, win32_error);
  g_free (message);
  g_free (win32_error);

  if (result_code == ERROR_KEY_DELETED)
    trace ("(%s)", win32_message);
  else
    g_message ("%s", win32_message);

  g_free (win32_message);
}

/* Make gsettings key into a registry path & value pair. 
 * 
 * Note that the return value *only* needs freeing - registry_value_name
 * is a pointer to further inside the same block of memory.
 */
static gchar *
parse_key (const gchar  *key_name,
           const gchar  *registry_prefix,
           gchar       **value_name)
{
  gchar *path_name, *c;

  /* All key paths are treated as absolute; gsettings doesn't seem to enforce a
   * preceding /.
   */
  if (key_name[0] == '/')
    key_name++;

  if (registry_prefix == NULL)
    path_name = g_strdup (key_name);
  else
    path_name = g_strjoin ("/", registry_prefix, key_name, NULL);

  /* Prefix is expected to be in registry format (\ separators) so don't escape that. */
  for (c = path_name + (registry_prefix ? strlen (registry_prefix) : 0); *c != 0; c++)
    {
      if (*c == '/')
        {
          *c = '\\';
          *value_name = c;
        }
    }

  **value_name = 0;
  (*value_name)++;

  return path_name;
}

static DWORD
g_variant_get_as_dword (GVariant *variant)
{
  switch (g_variant_get_type_string (variant)[0])
    {
    case 'b':
      return g_variant_get_boolean (variant);
    case 'y':
      return g_variant_get_byte (variant);
    case 'n':
      return g_variant_get_int16 (variant);
    case 'q':
      return g_variant_get_uint16 (variant);
    case 'i':
      return g_variant_get_int32 (variant);
    case 'u':
      return g_variant_get_uint32 (variant);
    default:
      g_warn_if_reached ();
    }
  return 0;
}

static DWORDLONG
g_variant_get_as_qword (GVariant *variant)
{
  switch (g_variant_get_type_string (variant)[0])
    {
    case 'x':
      return g_variant_get_int64 (variant);
    case 't':
      return g_variant_get_uint64 (variant);
    default:
      g_warn_if_reached ();
    }
  return 0;
}

static void
handle_read_error (LONG         result,
                   const gchar *path_name,
                   const gchar *value_name)
{
  /* file not found means key value not set, this isn't an error for us. */
  if (result != ERROR_FILE_NOT_FOUND)
    g_message_win32_error (result, "Unable to query value %s/%s: %s.\n",
                           path_name, value_name);
}

/***************************************************************************
 * Cache of registry values
 ***************************************************************************/

/* Generic container for registry values */
typedef struct {
  DWORD type;

  union {
    gint  dword;  /* FIXME: could inline QWORD on 64-bit systems too */
    void *ptr;
  };
} RegistryValue;

static char *
registry_value_dump (RegistryValue value)
{
  if (value.type == REG_DWORD)
    return g_strdup_printf ("%d", value.dword);
  else if (value.type == REG_QWORD)
    return g_strdup_printf ("%"G_GINT64_FORMAT, value.ptr == NULL ? 0: *(DWORDLONG *)value.ptr);
  else if (value.type == REG_SZ)
    return g_strdup_printf ("%s", (char *)value.ptr);
  else if (value.type == REG_NONE)
    return g_strdup_printf ("<empty>");
  else
    return g_strdup_printf ("<invalid>");
}

static void
registry_value_free (RegistryValue value)
{
  if (value.type == REG_SZ || value.type == REG_QWORD)
    g_free (value.ptr);

  value.type = REG_NONE;
  value.ptr = NULL;
}

/* The registry cache is stored as a tree, for easy traversal. Right now we
 * don't sort it in a clever way. Each node corresponds to a path element
 * ('key' in registry terms) or a value.
 *
 * Each subscription uses the same cache. Because GSettings can subscribe to
 * the tree at any node any number of times, we need to reference count the
 * nodes.
 */
typedef struct
{
  /* Component of path that this node represents */
  gchar *name;

  /* If a watch is subscribed at this point (subscription_count > 0) we can
   * block its next notification. This is useful because if two watches cover
   * the same path, both will trigger when it changes. It also allows changes
   * done by the application to be ignored by the watch thread.
   */
  gint32 block_count : 8;

  /* Number of times g_settings_subscribe has been called for this location
   * (I guess you can't subscribe more than 16383 times) */
  gint32 subscription_count : 14;
  
  gint32 ref_count : 9;

  gint32 readable : 1;
  RegistryValue value;
} RegistryCacheItem;

static GNode *
registry_cache_add_item (GNode         *parent,
                         gchar         *name,
                         RegistryValue  value,
                         gint           ref_count)
{
  RegistryCacheItem *item;
  GNode *cache_node;

  g_return_val_if_fail (name != NULL, NULL);
  g_return_val_if_fail (parent != NULL, NULL);

  item = g_slice_new (RegistryCacheItem);

  /* Ref count should be the number of watch points above this node */
  item->ref_count = ref_count;

  item->name = g_strdup (name);
  item->value = value;
  item->subscription_count = 0;
  item->block_count = 0;
  item->readable = FALSE;

  trace ("\treg cache: adding %s to %s\n",
         name, ((RegistryCacheItem *)parent->data)->name);

  cache_node = g_node_new (item);
  g_node_append (parent, cache_node);

  return cache_node;
}

/* The reference counting of cache tree nodes works like this: when a node is
 * subscribed to (GSettings wants us to watch that path and everything below
 * it) the reference count of that node and everything below is increased, as
 * well as each parent up to the root.
 */

static void
_ref_down (GNode *node)
{
  RegistryCacheItem *item = node->data;

  g_node_children_foreach (node, G_TRAVERSE_ALL,
                           (GNodeForeachFunc)_ref_down, NULL);
  item->ref_count++;
}

static void
registry_cache_ref_tree (GNode *tree)
{
  RegistryCacheItem *item = tree->data;
  GNode *node = tree->parent;

  g_return_if_fail (tree != NULL);

  item->ref_count++;

  g_node_children_foreach (tree, G_TRAVERSE_ALL,
                           (GNodeForeachFunc)_ref_down, NULL);

  for (node = tree->parent; node; node = node->parent)
    {
      item = node->data;
      item->ref_count++;
    }
}

static void
registry_cache_item_free (RegistryCacheItem *item)
{
  trace ("\t -- Free node %s\n", item->name);

  g_free (item->name);
  registry_value_free (item->value);
  g_slice_free (RegistryCacheItem, item);
}

/* Unreferencing has to be done bottom-up */
static void
_unref_node (GNode *node)
{
  RegistryCacheItem *item = node->data;

  item->ref_count--;

  g_warn_if_fail (item->ref_count >= 0);

  if (item->ref_count == 0)
    {
      registry_cache_item_free (item);
      g_node_destroy (node);
    }
}

static void
_unref_down (GNode *node)
{
  g_node_children_foreach (node, G_TRAVERSE_ALL,
                           (GNodeForeachFunc)_unref_down, NULL);
  _unref_node (node);
}

static void
registry_cache_unref_tree (GNode *tree)
{
  GNode *parent = tree->parent, *next_parent;

  _unref_down (tree);

  while (parent)
    {
      next_parent = parent->parent;
      _unref_node (parent);
      parent = next_parent;
    }
}

#if 0
static void
registry_cache_dump (GNode    *cache_node,
                     gpointer  data)
{
  RegistryCacheItem *item = cache_node->data;

  int depth     = GPOINTER_TO_INT(data),
      new_depth = depth+1,
      i;

  g_return_if_fail (cache_node != NULL);

  for (i=0; i<depth; i++)
    g_print ("  ");
  if (item == NULL)
    g_print ("*root*\n");
  else
    g_print ("'%s'  [%i] @ %x = %s\n", item->name, item->ref_count, (guint)cache_node,
             registry_value_dump (item->value));
  g_node_children_foreach (cache_node, G_TRAVERSE_ALL, registry_cache_dump,
                           GINT_TO_POINTER (new_depth));
}
#endif

typedef struct
{
  gchar *name;
  GNode *result;
} RegistryCacheSearch;

static gboolean
registry_cache_find_compare (GNode    *node,
                             gpointer  data)
{
  RegistryCacheSearch *search = data;
  RegistryCacheItem *item = node->data;

  if (item == NULL)  /* root node */
    return FALSE;

  g_return_val_if_fail (search->name != NULL, FALSE);
  g_return_val_if_fail (item->name != NULL, FALSE);

  if (strcmp (search->name, item->name) == 0)
    {
      search->result = node;
      return TRUE;
    }

  return FALSE;
}

static GNode *
registry_cache_find_immediate_child (GNode *node,
                                     gchar *name)
{
  RegistryCacheSearch search;

  search.result = NULL;
  search.name = name;

  g_node_traverse (node, G_POST_ORDER, G_TRAVERSE_ALL, 2,
                   registry_cache_find_compare, &search);

  return search.result;
}

static GNode *
registry_cache_get_node_for_key_recursive (GNode    *node,
                                           gchar    *key_name,
                                           gboolean  create_if_not_found,
                                           gint      n_parent_watches)
{
  RegistryCacheItem *item;
  gchar *component = key_name;
  gchar *c = strchr (component, '/');
  GNode *child;

  if (c != NULL)
    *c = 0;

  /* We count up how many watch points we travel through finding this node,
   * because a new node should have as many references as there are watches at
   * points above it in the tree.
   */
  item = node->data;
  if (item->subscription_count > 0)
    n_parent_watches++;

  child = registry_cache_find_immediate_child (node, component);
  if (child == NULL && create_if_not_found)
    {
      RegistryValue null_value = { REG_NONE, {0} };

      child = registry_cache_add_item (node, component,
                                       null_value, n_parent_watches);

      trace ("\tget node for key recursive: new %x = %s.\n", node, component);
    }

  /* We are done if there are no more path components. Allow for a trailing /. */
  if (child == NULL || c == NULL || *(c + 1) == 0)
    return child;

  trace ("get node for key recursive: next: %s.\n", c + 1);

  return registry_cache_get_node_for_key_recursive (child, c + 1,
                                                    create_if_not_found,
                                                    n_parent_watches);
}

/* Look up a GSettings key in the cache. */
static GNode *
registry_cache_get_node_for_key (GNode       *root,
                                 const gchar *key_name,
                                 gboolean     create_if_not_found)
{
  GNode *child = NULL;
  GNode *result = NULL;
  gchar *component, *c;

  g_return_val_if_fail (key_name != NULL, NULL);

  if (key_name[0] == '/')
    key_name++;

  /* Ignore preceding / */
  component = g_strdup (key_name);
  c = strchr (component, '/');

  if (c == NULL)
    {
      g_free (component);
      return root;
    }

  if (c != NULL)
    *c = 0;

  child = registry_cache_find_immediate_child (root, component);
  if (child == NULL && create_if_not_found)
    {
      RegistryValue null_value = { REG_NONE, {0} };

      /* Reference count is set to 0, tree should be referenced by the caller */
      child = registry_cache_add_item (root, component,
                                       null_value, 0);

      trace ("get_node_for_key: New node for component '%s'\n", component);
    }

  if (*(c + 1) == 0)
    result = child;
  else if (child != NULL)
    result = registry_cache_get_node_for_key_recursive (child, c + 1,
                                                        create_if_not_found, 0);

  g_free (component);

  return result;
}

/* Check the cache node against the registry key it represents. Return TRUE if
 * they differ, and update the cache with the new value.
 */
static gboolean
registry_cache_update_node (GNode        *cache_node,
                            RegistryValue registry_value)
{
  RegistryCacheItem *cache_item;

  g_return_val_if_fail (cache_node != NULL, FALSE);
  g_return_val_if_fail (cache_node->data != NULL, FALSE);

  cache_item = cache_node->data;

  if (registry_value.type != cache_item->value.type)
    {
      /* The type has changed. Update cache item and register it as changed.
       * Either the schema has changed and this is entirely legitimate, or
       * whenever the app reads the key it will get the default value due to
       * the type mismatch.
       */
      cache_item->value = registry_value;
      return TRUE;
    }
 
  switch (registry_value.type)
    {
    case REG_DWORD:
      {
        if (cache_item->value.dword == registry_value.dword)
          return FALSE;
        else
          {
            cache_item->value.dword = registry_value.dword;
            return TRUE;
          }
      }
    case REG_QWORD:
      {
        g_return_val_if_fail (registry_value.ptr != NULL &&
                              cache_item->value.ptr != NULL, FALSE);

        if (memcmp (registry_value.ptr, cache_item->value.ptr, 8)==0)
          {
            g_free (registry_value.ptr);
            return FALSE;
          }
        else
          {
            g_free (cache_item->value.ptr);
            cache_item->value.ptr = registry_value.ptr;
            return TRUE;
          }
      }
    case REG_SZ:
      {
        /* Value should not exist if it is NULL, an empty string is "" */
        g_return_val_if_fail (cache_item->value.ptr != NULL, FALSE);
        g_return_val_if_fail (registry_value.ptr != NULL, FALSE);

        if (strcmp (registry_value.ptr, cache_item->value.ptr) == 0)
          {
            g_free (registry_value.ptr);
            return FALSE;
          }
        else
          {
            g_free (cache_item->value.ptr);
            cache_item->value.ptr = registry_value.ptr;
            return TRUE;
          }
      }
    default:
      g_warning ("gregistrybackend: registry_cache_update_node: Unhandled value type");
      return FALSE;
    }
}

/* Blocking notifications is a useful optimisation. When a change is made
 * through GSettings we update the cache manually, but a notifcation is
 * triggered as well. This function is also used for nested notifications,
 * eg. if /test and /test/foo are watched, and /test/foo/value is changed then
 * we will get notified both for /test/foo and /test and it is helpful to block
 * the second.
 */
static void
registry_cache_block_notification (GNode *node)
{
  RegistryCacheItem *item = node->data;

  g_return_if_fail (node != NULL);

  if (item->subscription_count > 0)
    item->block_count++;

  if (node->parent != NULL)
    registry_cache_block_notification (node->parent);
}

static void registry_cache_destroy_tree (GNode            *node,
                                         WatchThreadState *self);

/***************************************************************************
 * Reading and writing
 ***************************************************************************/

static gboolean
registry_read (HKEY           hpath,
               const gchar   *path_name,
               const gchar   *value_name,
               RegistryValue *p_value)
{
  LONG result;
  DWORD value_data_size;
  gpointer *buffer;
  gunichar2 *value_namew;

  g_return_val_if_fail (p_value != NULL, FALSE);

  p_value->type = REG_NONE;
  p_value->ptr = NULL;

  value_namew = g_utf8_to_utf16 (value_name, -1, NULL, NULL, NULL);

  result = RegQueryValueExW (hpath, value_namew, 0, &p_value->type, NULL, &value_data_size);
  if (result != ERROR_SUCCESS)
    {
      handle_read_error (result, path_name, value_name);
      g_free (value_namew);
      return FALSE;
    }

  if (p_value->type == REG_SZ && value_data_size == 0)
    {
      p_value->ptr = g_strdup ("");
      g_free (value_namew);
      return TRUE;
    }

  if (p_value->type == REG_DWORD)
    /* REG_DWORD is inlined */
    buffer = (void *)&p_value->dword;
  else
    buffer = p_value->ptr = g_malloc (value_data_size);

  result = RegQueryValueExW (hpath, value_namew, 0, NULL, (LPBYTE)buffer, &value_data_size);
  g_free (value_namew);

  if (result != ERROR_SUCCESS)
    {
      handle_read_error (result, path_name, value_name);

      if (p_value->type != REG_DWORD)
        g_free (buffer);

      return FALSE;
    }

  if (p_value->type == REG_SZ)
    {
      gchar *valueu8 = g_utf16_to_utf8 (p_value->ptr, -1, NULL, NULL, NULL);
      g_free (p_value->ptr);
      p_value->ptr = valueu8;
    }

  return TRUE;
}

static GVariant *
g_registry_backend_read (GSettingsBackend   *backend,
                         const gchar        *key_name,
                         const GVariantType *expected_type,
                         gboolean            default_value)
{
  GRegistryBackend *self = G_REGISTRY_BACKEND (backend);
  GNode *cache_node;
  RegistryValue registry_value;
  GVariant *gsettings_value = NULL;
  gchar *gsettings_type;

  g_return_val_if_fail (expected_type != NULL, NULL);

  if (default_value)
    return NULL;

  /* Simply read from the cache, which is updated from the registry by the
   * watch thread as soon as changes can propagate. Any changes not yet in the
   * cache will have the 'changed' signal emitted after this function returns.
   */
  EnterCriticalSection (self->cache_lock);
  cache_node = registry_cache_get_node_for_key (self->cache_root, key_name, FALSE);
  LeaveCriticalSection (self->cache_lock);

  trace ("Reading key %s, cache node %x\n", key_name, cache_node);

  /* Maybe it's not set, we can return to default */
  if (cache_node == NULL)
    return NULL;

  trace ("\t- cached value %s\n", registry_value_dump (((RegistryCacheItem *)cache_node->data)->value));

  registry_value = ((RegistryCacheItem *)cache_node->data)->value;

  gsettings_type = g_variant_type_dup_string (expected_type);

  /* The registry is user-editable, so we need to be fault-tolerant here. */
  switch (gsettings_type[0])
    {
    case 'b':
    case 'y':
    case 'n':
    case 'q':
    case 'i':
    case 'u':
      if (registry_value.type == REG_DWORD)
        gsettings_value = g_variant_new (gsettings_type, registry_value.dword);
      break;

    case 't':
    case 'x':
      if (registry_value.type == REG_QWORD)
        {
          DWORDLONG qword_value = *(DWORDLONG *)registry_value.ptr;
          gsettings_value = g_variant_new (gsettings_type, qword_value);
        }
      break;

    default:
      if (registry_value.type == REG_SZ)
        {
          if (gsettings_type[0] == 's')
            gsettings_value = g_variant_new_string ((char *)registry_value.ptr);
          else
            {
              GError *error = NULL;

              gsettings_value = g_variant_parse (expected_type, registry_value.ptr,
                                                 NULL, NULL, &error);

              if (error != NULL)
                g_message ("gregistrysettingsbackend: error parsing key %s: %s",
                           key_name, error->message);
            }
        }
        break;
    }

  g_free (gsettings_type);

  return gsettings_value;
}


typedef struct
{
  GRegistryBackend *self;
  HKEY hroot;
} RegistryWrite;

static gboolean
g_registry_backend_write_one (const char *key_name,
                              GVariant   *variant,
                              gpointer    user_data)
{
  GRegistryBackend *self;
  RegistryWrite *action;
  RegistryValue value;
  HKEY hroot;
  HKEY hpath;
  gchar *path_name;
  gunichar2 *path_namew;
  gchar *value_name = NULL;
  gunichar2 *value_namew;
  DWORD value_data_size;
  LPVOID value_data;
  gunichar2 *value_dataw;
  LONG result;
  GNode *node;
  gboolean changed;
  const gchar *type_string;

  type_string = g_variant_get_type_string (variant);
  action = user_data;
  self = G_REGISTRY_BACKEND (action->self);
  hroot = action->hroot;

  value.type = REG_NONE;
  value.ptr = NULL;

  switch (type_string[0])
    {
    case 'b':
    case 'y':
    case 'n':
    case 'q':
    case 'i':
    case 'u':
      value.type = REG_DWORD;
      value.dword = g_variant_get_as_dword (variant);
      value_data_size = 4;
      value_data = &value.dword;
      break;

    case 'x':
    case 't':
      value.type = REG_QWORD;
      value.ptr = g_malloc (8);
      *(DWORDLONG *)value.ptr = g_variant_get_as_qword (variant);
      value_data_size = 8;
      value_data = value.ptr;
      break;

    default:
      value.type = REG_SZ;
      if (type_string[0] == 's')
        {
          gsize length;
          value.ptr = g_strdup (g_variant_get_string (variant, &length));
          value_data_size = length + 1;
          value_data = value.ptr;
        }
      else
        {
          GString *value_string;
          value_string = g_variant_print_string (variant, NULL, FALSE);
          value_data_size = value_string->len + 1;
          value.ptr = value_data = g_string_free (value_string, FALSE);
        }
      break;
    }

  /* First update the cache, because the value may not have changed and we can
   * save a write.
   * 
   * If 'value' has changed then its memory will not be freed by update_node(),
   * because it will be stored in the node.
   */
  EnterCriticalSection (self->cache_lock);
  node = registry_cache_get_node_for_key (self->cache_root, key_name, TRUE);
  changed = registry_cache_update_node (node, value);
  LeaveCriticalSection (self->cache_lock);

  if (!changed)
    return FALSE;

  /* Block the next notification to any watch points above this location,
   * because they will each get triggered on a change that is already updated
   * in the cache.
   */
  registry_cache_block_notification (node);

  path_name = parse_key (key_name, NULL, &value_name);

  trace ("Set key: %s / %s\n", path_name, value_name);

  path_namew = g_utf8_to_utf16 (path_name, -1, NULL, NULL, NULL);

  /* Store the value in the registry */
  result = RegCreateKeyExW (hroot, path_namew, 0, NULL, 0, KEY_WRITE, NULL, &hpath, NULL);
  if (result != ERROR_SUCCESS)
    {
      g_message_win32_error (result, "gregistrybackend: opening key %s failed",
                             path_name + 1);
      registry_value_free (value);
      g_free (path_namew);
      g_free (path_name);
      return FALSE;
    }

  g_free (path_namew);

  value_namew = g_utf8_to_utf16 (value_name, -1, NULL, NULL, NULL);

  value_dataw = NULL;

  switch (type_string[0])
    {
    case 'b':
    case 'y':
    case 'n':
    case 'q':
    case 'i':
    case 'u':
    case 'x':
    case 't':
      break;
    default:
      value_dataw = g_utf8_to_utf16 (value_data, -1, NULL, NULL, NULL);
      value_data = value_dataw;
      value_data_size = (DWORD)((wcslen (value_data) + 1) * sizeof (gunichar2));
      break;
    }

  result = RegSetValueExW (hpath, value_namew, 0, value.type, value_data, value_data_size);

  if (result != ERROR_SUCCESS)
    g_message_win32_error (result, "gregistrybackend: setting value %s\\%s\\%s failed.\n",
                           self->base_path, path_name, value_name);

  /* If the write fails then it will seem like the value has changed until the
   * next execution (because we wrote to the cache first). There's no reason
   * for it to fail unless something is weirdly broken, however.
   */

  RegCloseKey (hpath);
  g_free (path_name);
  g_free (value_namew);
  g_free (value_dataw);

  return FALSE;
}

/* The dconf write policy is to do the write while making out it succeeded, 
 * and then backtrack if it didn't. The registry functions are synchronous so
 * we can't do that. */

static gboolean
g_registry_backend_write (GSettingsBackend *backend,
                          const gchar      *key_name,
                          GVariant         *value,
                          gpointer          origin_tag)
{
  GRegistryBackend *self = G_REGISTRY_BACKEND (backend);
  LONG result;
  HKEY hroot;
  RegistryWrite action;

  result = RegCreateKeyExW (HKEY_CURRENT_USER, self->base_pathw, 0, NULL, 0,
                            KEY_WRITE, NULL, &hroot, NULL);
  if (result != ERROR_SUCCESS)
    {
      trace ("Error opening/creating key %s.\n", self->base_path);
      return FALSE;
    }

  action.self = self;
  action.hroot = hroot;
  g_registry_backend_write_one (key_name, value, &action);
  g_settings_backend_changed (backend, key_name, origin_tag);

  RegCloseKey (hroot);

  return TRUE;
}

static gboolean
g_registry_backend_write_tree (GSettingsBackend *backend,
                               GTree            *values,
                               gpointer          origin_tag)
{
  GRegistryBackend *self = G_REGISTRY_BACKEND (backend);
  LONG result;
  HKEY hroot;
  RegistryWrite action;

  result = RegCreateKeyExW (HKEY_CURRENT_USER, self->base_pathw, 0, NULL, 0,
                            KEY_WRITE, NULL, &hroot, NULL);
  if (result != ERROR_SUCCESS)
    {
      trace ("Error opening/creating key %s.\n", self->base_path);
      return FALSE;
    }

  action.self =  self;
  action.hroot = hroot;
  g_tree_foreach (values, (GTraverseFunc)g_registry_backend_write_one,
                  &action);

  g_settings_backend_changed_tree (backend, values, origin_tag);
  RegCloseKey (hroot);

  return TRUE;
}

static void
g_registry_backend_reset (GSettingsBackend *backend,
                          const gchar      *key_name,
                          gpointer          origin_tag)
{
  GRegistryBackend *self = G_REGISTRY_BACKEND (backend);
  gchar *path_name;
  gunichar2 *path_namew;
  gchar *value_name = NULL;
  gunichar2 *value_namew;
  GNode *cache_node;
  LONG result;
  HKEY hpath;

  /* Remove from cache */
  EnterCriticalSection (self->cache_lock);
  cache_node = registry_cache_get_node_for_key (self->cache_root, key_name, FALSE);
  if (cache_node)
    registry_cache_destroy_tree (cache_node, self->watch);
  LeaveCriticalSection (self->cache_lock);

  /* Remove from the registry */
  path_name = parse_key (key_name, self->base_path, &value_name);
  path_namew = g_utf8_to_utf16 (path_name, -1, NULL, NULL, NULL);

  result = RegOpenKeyExW (HKEY_CURRENT_USER, path_namew, 0, KEY_SET_VALUE, &hpath);
  g_free (path_namew);

  if (result != ERROR_SUCCESS)
    {
      g_message_win32_error (result, "Registry: resetting key '%s'", path_name);
      g_free (path_name);
      return;
    }

  value_namew = g_utf8_to_utf16 (value_name, -1, NULL, NULL, NULL);

  result = RegDeleteValueW (hpath, value_namew);
  g_free (value_namew);
  RegCloseKey (hpath);

  if (result != ERROR_SUCCESS)
    {
      g_message_win32_error (result, "Registry: resetting key '%s'", path_name);
      g_free (path_name);
      return;
    }

  g_free (path_name);

  g_settings_backend_changed (backend, key_name, origin_tag);
}

static gboolean
g_registry_backend_get_writable (GSettingsBackend *backend,
                                 const gchar      *key_name)
{
  GRegistryBackend *self = G_REGISTRY_BACKEND (backend);
  gchar *path_name;
  gunichar2 *path_namew;
  gchar *value_name;
  HKEY hpath;
  LONG result;

  path_name = parse_key (key_name, self->base_path, &value_name);
  path_namew = g_utf8_to_utf16 (path_name, -1, NULL, NULL, NULL);

  /* Note: we create the key if it wasn't created yet, but it is not much
   * of a problem since at the end of the day we have to create it anyway
   * to read or to write from it
   */
  result = RegCreateKeyExW (HKEY_CURRENT_USER, path_namew, 0, NULL, 0,
                            KEY_WRITE, NULL, &hpath, NULL);
  g_free (path_namew);

  if (result != ERROR_SUCCESS)
    {
      trace ("Error opening/creating key to check writability: %s.\n",
             path_name);
      g_free (path_name);

      return FALSE;
    }

  g_free (path_name);
  RegCloseKey (hpath);

  return TRUE;
}

/********************************************************************************
 * Spot-the-difference engine
 ********************************************************************************/

static void
_free_watch (WatchThreadState *self,
             guint             index,
             GNode            *cache_node);

static void
registry_cache_item_reset_readable (GNode    *node,
                                    gpointer  data)
{
  RegistryCacheItem *item = node->data;
  item->readable = FALSE;
}

/* Delete a node and any children, for when it has been deleted from the registry */
static void
registry_cache_destroy_tree (GNode            *node,
                             WatchThreadState *self)
{
  RegistryCacheItem *item = node->data;

  g_node_children_foreach (node, G_TRAVERSE_ALL,
                           (GNodeForeachFunc)registry_cache_destroy_tree, self);

  if (item->subscription_count > 0)
    {
      guint i;

      /* There must be some watches active if this node is a watch point */
      g_warn_if_fail (self->cache_nodes->len > 1);

      /* This is a watch point that has been deleted. Let's free the watch! */
      for (i = 1; i < self->cache_nodes->len; i++)
        {
          if (g_ptr_array_index (self->cache_nodes, i) == node)
            break;
        }

      if (i >= self->cache_nodes->len)
        g_warning ("watch thread: a watch point was deleted, but unable to "
                   "find '%s' in the list of %i watch nodes\n", item->name,
                   self->cache_nodes->len - 1);
      else
        {
          _free_watch (self, i, node);
          g_atomic_int_inc (&self->watches_remaining);
        }
    }
  registry_cache_item_free (node->data);
  g_node_destroy (node);
}

/* One of these is sent down the pipe when something happens in the registry. */
typedef struct
{
  GRegistryBackend *self;
  gchar *prefix;          /* prefix is a gsettings path, all items are subkeys of this. */
  GPtrArray *items;       /* each item is a subkey below prefix that has changed. */
} RegistryEvent;

typedef struct
{
  RegistryEvent *event;
  gchar *current_key_name;
} DeletedItemData;

static void
mark_all_subkeys_as_changed (GNode    *node,
                             gpointer  data)
{
  RegistryCacheItem *item = node->data;
  DeletedItemData *item_data = data;

  if (item_data->current_key_name == NULL)
    item_data->current_key_name = g_strdup (item->name);
  else
    {
      gchar *name;

      name = g_build_path ("/", item_data->current_key_name, item->name, NULL);
      g_free (item_data->current_key_name);
      item_data->current_key_name = name;
    }

  /* Iterate until we find an item that is a value */
  if (item->value.type == REG_NONE)
    g_node_children_foreach (node, G_TRAVERSE_ALL,
                             mark_all_subkeys_as_changed, data);
  else
    g_ptr_array_add (item_data->event->items, item_data->current_key_name);
}

static void
registry_cache_remove_deleted (GNode    *node,
                               gpointer  data)
{
  RegistryCacheItem *item = node->data;
  RegistryEvent *event = data;

  if (!item->readable)
    {
      DeletedItemData item_data;

      item_data.event = event;
      item_data.current_key_name = NULL;

      mark_all_subkeys_as_changed (node, &item_data);
      registry_cache_destroy_tree (node, event->self->watch);
    }
}

/* Update cache from registry, and optionally report on the changes.
 * 
 * This function is sometimes called from the watch thread, with no locking. It
 * does call g_registry_backend functions, but this is okay because they only
 * access self->base which is constant.
 *
 * When looking at this code bear in mind the terminology: in the registry, keys
 * are containers that contain values, and other keys. Keys have a 'default'
 * value which we always ignore.
 *
 * n_parent_watches: a counter used to set the reference count of any new nodes
 *                   that are created - they should have as many references as
 *                   there are notifications that are watching them.
 */
static void
registry_cache_update (GRegistryBackend *self,
                       HKEY              hpath,
                       const gchar      *prefix,
                       const gchar      *partial_key_name,
                       GNode            *cache_node,
                       int               n_watches,
                       RegistryEvent    *event)
{
  gunichar2 bufferw[MAX_KEY_NAME_LENGTH + 1];
  gchar *buffer;
  gchar *key_name;
  gint i;
  LONG result;
  RegistryCacheItem *item;

  item = cache_node->data;

  if (item->subscription_count > 0)
    n_watches++;

  /* prefix is the level that all changes occur below; partial_key_name should
   * be NULL on the first call to this function */
  key_name = g_build_path ("/", prefix, partial_key_name, NULL);

  trace ("registry cache update: %s. Node %x has %i children\n", key_name,
         cache_node, g_node_n_children (cache_node));

  /* Start by zeroing 'readable' flag. When the registry traversal is done, any unreadable nodes
   * must have been deleted from the registry.
   */
  g_node_children_foreach (cache_node, G_TRAVERSE_ALL,
                           registry_cache_item_reset_readable, NULL);

  /* Recurse into each subpath at the current level, if any */
  i = 0;
  while (1)
    {
      DWORD bufferw_size = MAX_KEY_NAME_LENGTH + 1;
      HKEY  hsubpath;

      result = RegEnumKeyExW (hpath, i++, bufferw, &bufferw_size, NULL, NULL, NULL, NULL);
      if (result != ERROR_SUCCESS)
        break;

      result = RegOpenKeyExW (hpath, bufferw, 0, KEY_READ, &hsubpath);
      if (result == ERROR_SUCCESS)
        {
          GNode *subkey_node;
          RegistryCacheItem *child_item;
          gchar *new_partial_key_name;

          buffer = g_utf16_to_utf8 (bufferw, -1, NULL, NULL, NULL);
          if (buffer == NULL)
            continue;

          subkey_node = registry_cache_find_immediate_child (cache_node, buffer);
          if (subkey_node == NULL)
            {
              RegistryValue null_value = {REG_NONE, {0}};
              subkey_node = registry_cache_add_item (cache_node, buffer,
                                                     null_value, n_watches);
            }

          new_partial_key_name = g_build_path ("/", partial_key_name, buffer, NULL);
          registry_cache_update (self, hsubpath, prefix, new_partial_key_name,
                                 subkey_node, n_watches, event);
          g_free (new_partial_key_name);

          child_item = subkey_node->data;
          child_item->readable = TRUE;

          g_free (buffer);
          RegCloseKey (hsubpath);
        }
    }

  if (result != ERROR_NO_MORE_ITEMS)
    g_message_win32_error (result, "gregistrybackend: error enumerating subkeys for cache.");

  /* Enumerate each value at 'path' and check if it has changed */
  i = 0;
  while (1)
    {
      DWORD bufferw_size = MAX_KEY_NAME_LENGTH + 1;
      GNode *cache_child_node;
      RegistryCacheItem *child_item;
      RegistryValue value;
      gboolean changed = FALSE;

      result = RegEnumValueW (hpath, i++, bufferw, &bufferw_size, NULL, NULL, NULL, NULL);
      if (result != ERROR_SUCCESS)
        break;

      buffer = g_utf16_to_utf8 (bufferw, -1, NULL, NULL, NULL);

      if (buffer == NULL || buffer[0] == 0)
        {
          /* This is the key's 'default' value, for which we have no use. */
          g_free (buffer);
          continue;
        }

      cache_child_node = registry_cache_find_immediate_child (cache_node, buffer);

      if (!registry_read (hpath, key_name, buffer, &value))
        {
          g_free (buffer);
          continue;
        }

      trace ("\tgot value %s for %s, node %x\n",
             registry_value_dump (value), buffer, cache_child_node);

      if (cache_child_node == NULL)
        {
          /* This is a new value */
          cache_child_node = registry_cache_add_item (cache_node, buffer, value,
                                                      n_watches);
          changed = TRUE;
        }
      else
        {
         /* For efficiency, instead of converting every value back to a GVariant to
          * compare it, we compare them as registry values (integers, or string
          * representations of the variant). The spurious change notifications that may
          * result should not be a big issue.
          *
          * Note that 'value' is swallowed or freed.
          */
          changed = registry_cache_update_node (cache_child_node, value);
        }

      child_item = cache_child_node->data;
      child_item->readable = TRUE;
      if (changed && event != NULL)
        {
          gchar *item;

          if (partial_key_name == NULL)
            item = g_strdup (buffer);
          else
            item = g_build_path ("/", partial_key_name, buffer, NULL);

          g_ptr_array_add (event->items, item);
        }

      g_free (buffer);
    }

  if (result != ERROR_NO_MORE_ITEMS)
    g_message_win32_error (result, "gregistrybackend: error enumerating values for cache");

  /* Any nodes now left unreadable must have been deleted, remove them from cache */
  g_node_children_foreach (cache_node, G_TRAVERSE_ALL,
                           registry_cache_remove_deleted, event);

  trace ("registry cache update complete.\n");

  g_free (key_name);
}

/***********************************************************************************
 * Thread to watch for registry change events
 ***********************************************************************************/

/* Called by watch thread. Apply for notifications on a registry key and its subkeys. */
static DWORD
registry_watch_key (HKEY   hpath,
                    HANDLE event)
{
  return RegNotifyChangeKeyValue (hpath, TRUE,
                                  REG_NOTIFY_CHANGE_NAME | REG_NOTIFY_CHANGE_LAST_SET,
                                  event, TRUE);
}

/* This handler runs in the main thread to emit the changed signals */
static gboolean
watch_handler (RegistryEvent *event)
{
  trace ("Watch handler: got event in %s, items %i.\n", event->prefix, event->items->len);

  /* GSettings requires us to NULL-terminate the array. */
  g_ptr_array_add (event->items, NULL);
  g_settings_backend_keys_changed (G_SETTINGS_BACKEND (event->self), event->prefix,
                                   (gchar const **)event->items->pdata, NULL);

  g_ptr_array_free (event->items, TRUE);
  g_free (event->prefix);
  g_object_unref (event->self);
  g_slice_free (RegistryEvent, event);

  return G_SOURCE_REMOVE;
}

static void
_free_watch (WatchThreadState *self,
             guint             index,
             GNode            *cache_node)
{
  HKEY hpath;
  HANDLE cond;
  gchar *prefix;

  g_return_if_fail (index > 0 && index < self->events->len);

  cond = g_ptr_array_index (self->events, index);
  hpath = g_ptr_array_index (self->handles, index);
  prefix = g_ptr_array_index (self->prefixes, index);

  trace ("Freeing watch %i [%s]\n", index, prefix);
 
  /* These can be NULL if the watch was already dead, this can happen when eg.
   * a key is deleted but GSettings is still subscribed to it - the watch is
   * kept alive so that the unsubscribe function works properly, but does not
   * do anything.
   */
  if (hpath != NULL)
    RegCloseKey (hpath);

  if (cache_node != NULL)
    {
      //registry_cache_dump (G_REGISTRY_BACKEND (self->owner)->cache_root, NULL);
      registry_cache_unref_tree (cache_node);
    }

  CloseHandle (cond);
  g_free (prefix);

  /* As long as we remove from each array at the same time, it doesn't matter that
   * their orders get messed up - they all get messed up the same.
   */
  g_ptr_array_remove_index_fast (self->handles, index);
  g_ptr_array_remove_index_fast (self->events, index);
  g_ptr_array_remove_index_fast (self->prefixes, index);
  g_ptr_array_remove_index_fast (self->cache_nodes, index);
}

static void
watch_thread_handle_message (WatchThreadState *self)
{
  switch (self->message.type)
    {
    case WATCH_THREAD_NONE:
      trace ("watch thread: you woke me up for nothin', man!");
      break;

    case WATCH_THREAD_ADD_WATCH:
      {
        RegistryWatch *watch = &self->message.watch;
        LONG result;

        result = registry_watch_key (watch->hpath, watch->event);

        if (result == ERROR_SUCCESS)
          {
            g_ptr_array_add (self->events, watch->event);
            g_ptr_array_add (self->handles, watch->hpath);
            g_ptr_array_add (self->prefixes, watch->prefix);
            g_ptr_array_add (self->cache_nodes, watch->cache_node);

            trace ("watch thread: new watch on %s, %i total\n", watch->prefix,
                   self->events->len);
          }
        else
          {
            g_message_win32_error (result, "watch thread: could not watch %s", watch->prefix);

            CloseHandle (watch->event);
            RegCloseKey (watch->hpath);
            g_free (watch->prefix);
            registry_cache_unref_tree (watch->cache_node);
          }
        break;
      }

    case WATCH_THREAD_REMOVE_WATCH:
      {
        GNode *cache_node;
        RegistryCacheItem *cache_item;
        guint i;

        for (i = 1; i < self->prefixes->len; i++)
          {
            if (strcmp (g_ptr_array_index (self->prefixes, i),
                        self->message.watch.prefix) == 0)
              break;
          }

        if (i >= self->prefixes->len)
          {
            /* Don't make a fuss if the prefix is not being watched because
             * maybe the path was deleted so we removed the watch.
             */
            trace ("unsubscribe: prefix %s is not being watched [%i things are]!\n",
                   self->message.watch.prefix, self->prefixes->len);
            g_free (self->message.watch.prefix);
            break;
          }

        cache_node = g_ptr_array_index (self->cache_nodes, i);

        trace ("watch thread: unsubscribe: freeing node %p, prefix %s, index %i\n",
               cache_node, self->message.watch.prefix, i);

        if (cache_node != NULL)
          {
            cache_item = cache_node->data;

            /* There may be more than one GSettings object subscribed to this
             * path, only free the watch when the last one unsubscribes.
             */
            cache_item->subscription_count--;
            if (cache_item->subscription_count > 0)
              break;
          }

        _free_watch (self, i, cache_node);
        g_free (self->message.watch.prefix);

        g_atomic_int_inc (&self->watches_remaining);
        break;
      }

    case WATCH_THREAD_STOP:
      {
        guint i;

        /* Free any remaining cache and watch handles */
        for (i = 1; i < self->events->len; i++)
          _free_watch (self, i, g_ptr_array_index (self->cache_nodes, i));

        SetEvent (self->message_received_event);
        ExitThread (0);
      }
    }

  self->message.type = WATCH_THREAD_NONE;
  SetEvent (self->message_received_event);
}

/* Thread which watches for win32 registry events */
static DWORD WINAPI
watch_thread_function (LPVOID parameter)
{
  WatchThreadState *self = (WatchThreadState *)parameter;
  DWORD result;

  self->events = g_ptr_array_new ();
  self->handles = g_ptr_array_new ();
  self->prefixes = g_ptr_array_new ();
  self->cache_nodes = g_ptr_array_new ();
  g_ptr_array_add (self->events, self->message_sent_event);
  g_ptr_array_add (self->handles, NULL);
  g_ptr_array_add (self->prefixes, NULL);
  g_ptr_array_add (self->cache_nodes, NULL);

  while (1)
    {
      trace ("watch thread: going to sleep; %i events watched.\n", self->events->len);
      result = WaitForMultipleObjects (self->events->len, self->events->pdata, FALSE, INFINITE);

      if (result == WAIT_OBJECT_0)
        {
          /* A message to you. The sender (main thread) will block until we signal the received
           * event, so there should be no danger of it sending another before we receive the
           * first.
           */
          watch_thread_handle_message (self);
        }
      else if (result > WAIT_OBJECT_0 && result <= WAIT_OBJECT_0 + self->events->len)
        {
          HKEY hpath;
          HANDLE cond;
          gchar *prefix;
          GNode *cache_node;
          RegistryCacheItem *cache_item;
          RegistryEvent *event;
          gint notify_index;

          /* One of our notifications has triggered. All we know is which one, and which key
           * this is for. We do most of the processing here, because we may as well. If the
           * registry changes further while we are processing it doesn't matter - we will then
           * receive another change notification from the OS anyway.
           */
          notify_index = result - WAIT_OBJECT_0;
          hpath = g_ptr_array_index (self->handles, notify_index);
          cond = g_ptr_array_index (self->events, notify_index);
          prefix = g_ptr_array_index (self->prefixes, notify_index);
          cache_node = g_ptr_array_index (self->cache_nodes, notify_index);

          trace ("Watch thread: notify received on prefix %i: %s.\n", notify_index, prefix);

          if (cache_node == NULL)
            {
              /* This path has been deleted */
              trace ("Notify received on a path that was deleted\n");
              continue;
            }

          /* Firstly we need to reapply for the notification, because (what a
           * sensible API) we won't receive any more. MSDN is pretty
           * inconsistent on this matter:
           *   http://msdn.microsoft.com/en-us/library/ms724892%28VS.85%29.aspx
           *   http://support.microsoft.com/kb/236570
           * But my tests (on Windows XP SP3) show that we need to reapply
           * each time.
           */
          result = registry_watch_key (hpath, cond);

          if (result != ERROR_SUCCESS)
            {
              /* Watch failed, most likely because the key has just been
               * deleted. Free the watch and unref the cache nodes.
               */
             if (result != ERROR_KEY_DELETED)
               g_message_win32_error (result, "watch thread: failed to watch %s", prefix);

             _free_watch (self, notify_index, cache_node);
             g_atomic_int_inc (&self->watches_remaining);
             continue;
            }

          /* The notification may have been blocked because we just changed
           * some data ourselves.
           */
          cache_item = cache_node->data;
          if (cache_item->block_count)
            {
              cache_item->block_count--;
              trace ("Watch thread: notify blocked at %s\n", prefix);
              continue;
            }

          /* Now we update our stored cache from registry data, and find which keys have
           * actually changed. If more changes happen while we are processing, we will get
           * another event because we have reapplied for change notifications already.
           *
           * Working here rather than in the main thread is preferable because the UI is less
           * likely to block (only when changing notification subscriptions).
           */
          event = g_slice_new (RegistryEvent);
          event->self = g_object_ref (self->owner);
          event->prefix = g_strdup (prefix);
          event->items = g_ptr_array_new_with_free_func (g_free);

          EnterCriticalSection (G_REGISTRY_BACKEND (self->owner)->cache_lock);
          registry_cache_update (G_REGISTRY_BACKEND (self->owner), hpath,
                                 prefix, NULL, cache_node, 0, event);
          LeaveCriticalSection (G_REGISTRY_BACKEND (self->owner)->cache_lock);

          if (event->items->len > 0)
            g_idle_add ((GSourceFunc) watch_handler, event);
          else
            {
              g_object_unref (event->self);
              g_free (event->prefix);
              g_ptr_array_free (event->items, TRUE);
              g_slice_free (RegistryEvent, event);
            }
        }
      else
        {
          /* God knows what has happened */
          g_message_win32_error (GetLastError(), "watch thread: WaitForMultipleObjects error");
        }
    }

  return -1;
}

static gboolean
watch_start (GRegistryBackend *self)
{
  WatchThreadState *watch;

  g_return_val_if_fail (self->watch == NULL, FALSE);

  watch = g_slice_new (WatchThreadState);
  watch->owner = G_SETTINGS_BACKEND (self);

  watch->watches_remaining = MAX_WATCHES;

  watch->message_lock = g_slice_new (CRITICAL_SECTION);
  InitializeCriticalSection (watch->message_lock);
  watch->message_sent_event = CreateEvent (NULL, FALSE, FALSE, NULL);
  watch->message_received_event = CreateEvent (NULL, FALSE, FALSE, NULL);
  if (watch->message_sent_event == NULL || watch->message_received_event == NULL)
    {
      g_message_win32_error (GetLastError (), "gregistrybackend: Failed to create sync objects.");
      goto fail;
    }

  /* Use a small stack to make the thread more lightweight. */
  watch->thread = CreateThread (NULL, 1024, watch_thread_function, watch, 0, NULL);
  if (watch->thread == NULL)
    {
      g_message_win32_error (GetLastError (), "gregistrybackend: Failed to create notify watch thread.");
      goto fail;
    }

  self->watch = watch;

  return TRUE;

fail:
  DeleteCriticalSection (watch->message_lock);
  g_slice_free (CRITICAL_SECTION, watch->message_lock);
  if (watch->message_sent_event != NULL)
    CloseHandle (watch->message_sent_event);
  if (watch->message_received_event != NULL)
    CloseHandle (watch->message_received_event);
  g_slice_free (WatchThreadState, watch);

  return FALSE;
}

/* This function assumes you hold the message lock! */
static void
watch_stop_unlocked (GRegistryBackend *self)
{
  WatchThreadState *watch = self->watch;
  DWORD result;

  g_return_if_fail (watch != NULL);

  watch->message.type = WATCH_THREAD_STOP;
  SetEvent (watch->message_sent_event);

  /* This is signalled as soon as the message is received. We must not return
   * while the watch thread is still firing off callbacks. Freeing all of the
   * memory is done in the watch thread after this is signalled.
   */
  result = WaitForSingleObject (watch->message_received_event, INFINITE);
  if (result != WAIT_OBJECT_0)
    {
      g_warning ("gregistrybackend: unable to stop watch thread.");
      return;
    }

  LeaveCriticalSection (watch->message_lock);
  DeleteCriticalSection (watch->message_lock);
  g_slice_free (CRITICAL_SECTION, watch->message_lock);
  CloseHandle (watch->message_sent_event);
  CloseHandle (watch->message_received_event);
  CloseHandle (watch->thread);
  g_slice_free (WatchThreadState, watch);

  trace ("\nwatch thread: %x: all data freed.\n", self);
  self->watch = NULL;
}

static gboolean
watch_add_notify (GRegistryBackend *self,
                  HANDLE            event,
                  HKEY              hpath,
                  gchar            *gsettings_prefix)
{
  WatchThreadState *watch = self->watch;
  GNode *cache_node;
  RegistryCacheItem *cache_item;
#ifdef TRACE
  DWORD result;
#endif

  g_return_val_if_fail (watch != NULL, FALSE);

  trace ("watch_add_notify: prefix %s.\n", gsettings_prefix);

  /* Duplicate tree into the cache in the main thread, before we add the notify: if we do it in the
   * thread we can miss changes while we are caching.
   */
  EnterCriticalSection (self->cache_lock);
  cache_node = registry_cache_get_node_for_key (self->cache_root, gsettings_prefix, TRUE);

  if (cache_node == NULL || cache_node->data == NULL)
    {
      LeaveCriticalSection (self->cache_lock);
      g_warn_if_reached ();
      return FALSE;
    }
  
  cache_item = cache_node->data;

  cache_item->subscription_count++;
  if (cache_item->subscription_count > 1)
    {
      trace ("watch_add_notify: prefix %s already watched, %i subscribers.\n",
             gsettings_prefix, cache_item->subscription_count);
      LeaveCriticalSection (self->cache_lock);
      return FALSE;
    }

  registry_cache_ref_tree (cache_node);
  registry_cache_update (self, hpath, gsettings_prefix, NULL, cache_node, 0, NULL);
  //registry_cache_dump (self->cache_root, NULL);
  LeaveCriticalSection (self->cache_lock);

  EnterCriticalSection (watch->message_lock);
  watch->message.type = WATCH_THREAD_ADD_WATCH;
  watch->message.watch.event = event;
  watch->message.watch.hpath = hpath;
  watch->message.watch.prefix = gsettings_prefix;
  watch->message.watch.cache_node = cache_node;

  SetEvent (watch->message_sent_event);

  /* Wait for the received event in return, to avoid sending another message before the first
   * one was received. If it takes > 200ms there is a possible race but the worst outcome is
   * a notification is ignored.
   */
#ifdef TRACE
  result =
#endif
    WaitForSingleObject (watch->message_received_event, 200);
#ifdef TRACE
  if (result != WAIT_OBJECT_0)
    trace ("watch thread is slow to respond - notification may not be added.");
#endif

  LeaveCriticalSection (watch->message_lock);

  return TRUE;
}

static void
watch_remove_notify (GRegistryBackend *self,
                     const gchar      *key_name)
{
  WatchThreadState *watch = self->watch;
  LONG result;

  if (self->watch == NULL)
    /* Here we assume that the unsubscribe message is for somewhere that was
     * deleted, and so it has already been removed and the watch thread has
     * stopped.
     */
    return;

  EnterCriticalSection (watch->message_lock);
  watch->message.type = WATCH_THREAD_REMOVE_WATCH;
  watch->message.watch.prefix = g_strdup (key_name);

  SetEvent (watch->message_sent_event);

  /* Wait for the received event in return, to avoid sending another message before the first
   * one was received.
   */
  result = WaitForSingleObject (watch->message_received_event, INFINITE);

  if (result != ERROR_SUCCESS)
    g_warning ("unsubscribe from %s: message not acknowledged", key_name);

  if (g_atomic_int_get (&watch->watches_remaining) >= MAX_WATCHES)
    /* Stop it before any new ones can get added and confuse things */
    watch_stop_unlocked (self);
  else
    LeaveCriticalSection (watch->message_lock);
}

/* dconf semantics are: if the key ends in /, watch the keys underneath it - if not, watch that
 * key. Our job is easier because keys and values are separate.
 */
static void
g_registry_backend_subscribe (GSettingsBackend *backend,
                              const char       *key_name)
{
  GRegistryBackend *self = G_REGISTRY_BACKEND (backend);
  gchar *path_name;
  gunichar2 *path_namew;
  gchar *value_name = NULL;
  HKEY hpath;
  HANDLE event;
  LONG result;

  if (self->watch == NULL && !watch_start (self))
    return;

  if (g_atomic_int_dec_and_test (&self->watch->watches_remaining))
    {
      g_atomic_int_inc (&self->watch->watches_remaining);
      g_warning ("subscribe() failed: only %i different paths may be watched.", MAX_WATCHES);
      return;
    }

  path_name = parse_key (key_name, self->base_path, &value_name);

  /* Must check for this, otherwise strange crashes occur because the cache
   * node that is being watched gets freed. All path names to subscribe must
   * end in a slash!
   */
  if (value_name != NULL && *value_name != 0)
    g_warning ("subscribe() failed: path must end in a /, got %s", key_name);

  trace ("Subscribing to %s [registry %s / %s] - watch %x\n", key_name, path_name, value_name, self->watch);

  path_namew = g_utf8_to_utf16 (path_name, -1, NULL, NULL, NULL);
  g_free (path_name);

  /* Give the caller the benefit of the doubt if the key doesn't exist and create it. The caller
   * is almost certainly a new g_settings with this path as base path. */
  result = RegCreateKeyExW (HKEY_CURRENT_USER, path_namew, 0, NULL, 0, KEY_READ, NULL, &hpath,
                            NULL);
  g_free (path_namew);

  if (result != ERROR_SUCCESS)
    {
      g_message_win32_error (result, "gregistrybackend: Unable to subscribe to key %s.", key_name);
      g_atomic_int_inc (&self->watch->watches_remaining);
      return;
    }

  event = CreateEvent (NULL, FALSE, FALSE, NULL);
  if (event == NULL)
    {
      g_message_win32_error (result, "gregistrybackend: CreateEvent failed.");
      g_atomic_int_inc (&self->watch->watches_remaining);
      RegCloseKey (hpath);
      return;
    }

  /* The actual watch is added by the thread, which has to re-subscribe each time it
   * receives a change. */
  if (!watch_add_notify (self, event, hpath, g_strdup (key_name)))
    {
      g_atomic_int_inc (&self->watch->watches_remaining);
      RegCloseKey (hpath);
      CloseHandle (event);
    }
}

static void
g_registry_backend_unsubscribe (GSettingsBackend *backend,
                                const char       *key_name)
{
  trace ("unsubscribe: %s.\n", key_name);

  watch_remove_notify (G_REGISTRY_BACKEND (backend), key_name);
}

/********************************************************************************
 * Object management junk
 ********************************************************************************/

static void
g_registry_backend_finalize (GObject *object)
{
  GRegistryBackend *self = G_REGISTRY_BACKEND (object);
  RegistryCacheItem *item;

  item = self->cache_root->data;
  g_warn_if_fail (item->ref_count == 1);

  registry_cache_item_free (item);
  g_node_destroy (self->cache_root);

  if (self->watch != NULL)
    {
      EnterCriticalSection (self->watch->message_lock);
      watch_stop_unlocked (self);
    }

  DeleteCriticalSection (self->cache_lock);
  g_slice_free (CRITICAL_SECTION, self->cache_lock);

  g_free (self->base_path);
  g_free (self->base_pathw);
}

static void
g_registry_backend_class_init (GRegistryBackendClass *class)
{
  GSettingsBackendClass *backend_class = G_SETTINGS_BACKEND_CLASS (class);
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = g_registry_backend_finalize;

  backend_class->read = g_registry_backend_read;
  backend_class->write = g_registry_backend_write;
  backend_class->write_tree = g_registry_backend_write_tree;
  backend_class->reset = g_registry_backend_reset;
  backend_class->get_writable = g_registry_backend_get_writable;
  backend_class->subscribe = g_registry_backend_subscribe;
  backend_class->unsubscribe = g_registry_backend_unsubscribe;
}

static void
g_registry_backend_init (GRegistryBackend *self)
{
  RegistryCacheItem *item;

  self->base_path = g_strdup_printf ("Software\\GSettings");
  self->base_pathw = g_utf8_to_utf16 (self->base_path, -1, NULL, NULL, NULL);

  item = g_slice_new (RegistryCacheItem);
  item->value.type = REG_NONE;
  item->value.ptr = NULL;
  item->name = g_strdup ("<root>");
  item->ref_count = 1;
  self->cache_root = g_node_new (item);

  self->cache_lock = g_slice_new (CRITICAL_SECTION);
  InitializeCriticalSection (self->cache_lock);

  self->watch = NULL;
}
