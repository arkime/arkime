/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright (C) 2014 Руслан Ижбулатов <lrn1986@gmail.com>
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
 */

#include "config.h"
#include "ginitable.h"
#include "gwin32registrykey.h"
#include <gio/gioerror.h>
#ifdef _MSC_VER
#pragma warning ( disable:4005 )
#endif
#include <windows.h>
#include <ntstatus.h>
#include <winternl.h>

#ifndef _WDMDDK_
typedef enum _KEY_INFORMATION_CLASS {
  KeyBasicInformation,
  KeyNodeInformation,
  KeyFullInformation,
  KeyNameInformation,
  KeyCachedInformation,
  KeyFlagsInformation,
  KeyVirtualizationInformation,
  KeyHandleTagsInformation,
  MaxKeyInfoClass
} KEY_INFORMATION_CLASS;

typedef struct _KEY_BASIC_INFORMATION {
  LARGE_INTEGER LastWriteTime;
  ULONG TitleIndex;
  ULONG NameLength;
  WCHAR Name[1];
} KEY_BASIC_INFORMATION, *PKEY_BASIC_INFORMATION;
#endif

#if !defined (__OBJECT_ATTRIBUTES_DEFINED) && defined (__MINGW32_)
#define __OBJECT_ATTRIBUTES_DEFINED
  typedef struct _OBJECT_ATTRIBUTES {
    ULONG Length;
#ifdef _WIN64
    ULONG pad1;
#endif
    HANDLE RootDirectory;
    PUNICODE_STRING ObjectName;
    ULONG Attributes;
#ifdef _WIN64
    ULONG pad2;
#endif
    PVOID SecurityDescriptor;
    PVOID SecurityQualityOfService;
  } OBJECT_ATTRIBUTES, *POBJECT_ATTRIBUTES;
#endif

#ifndef HKEY_CURRENT_USER_LOCAL_SETTINGS
#define HKEY_CURRENT_USER_LOCAL_SETTINGS ((HKEY) (ULONG_PTR)((LONG)0x80000007))
#endif

#if !defined (__UNICODE_STRING_DEFINED) && defined (__MINGW32_)
#define __UNICODE_STRING_DEFINED
typedef struct _UNICODE_STRING {
  USHORT Length;
  USHORT MaximumLength;
  PWSTR  Buffer;
} UNICODE_STRING, *PUNICODE_STRING;
#endif
typedef const UNICODE_STRING* PCUNICODE_STRING;

typedef NTSTATUS
(NTAPI * NtQueryKeyFunc)(HANDLE                key_handle,
                         KEY_INFORMATION_CLASS key_info_class,
                         PVOID                 key_info_buffer,
                         ULONG                 key_info_buffer_size,
                         PULONG                result_size);

typedef NTSTATUS
(NTAPI * NtNotifyChangeMultipleKeysFunc)(HANDLE             key_handle,
                                         ULONG              subkey_count,
                                         POBJECT_ATTRIBUTES subkeys,
                                         HANDLE             event,
                                         PIO_APC_ROUTINE    apc_routine,
                                         PVOID              apc_closure,
                                         PIO_STATUS_BLOCK   status_block,
                                         ULONG              filter,
                                         BOOLEAN            watch_tree,
                                         PVOID              buffer,
                                         ULONG              buffer_size,
                                         BOOLEAN            async);

static NtQueryKeyFunc nt_query_key = NULL;
static NtNotifyChangeMultipleKeysFunc nt_notify_change_multiple_keys = NULL;

#define G_WIN32_KEY_UNWATCHED 0
#define G_WIN32_KEY_WATCHED 1
#define G_WIN32_KEY_UNCHANGED 0
#define G_WIN32_KEY_CHANGED 1
#define G_WIN32_KEY_UNKNOWN -1

enum
{
  PROP_0,
  PROP_PATH,
  PROP_PATH_UTF16,
  PROP_MAX,
};

typedef enum
{
  G_WIN32_REGISTRY_UPDATED_NOTHING = 0,
  G_WIN32_REGISTRY_UPDATED_PATH = 1,
} GWin32RegistryKeyUpdateFlag;

static gunichar2 *
g_wcsdup (const gunichar2 *str,
          gssize           str_size)
{
  if (str_size == -1)
    {
      str_size = wcslen (str) + 1;
      str_size *= sizeof (gunichar2);
    }
  return g_memdup (str, str_size);
}

/**
 * g_win32_registry_subkey_iter_copy:
 * @iter: an iterator
 *
 * Creates a dynamically-allocated copy of an iterator. Dynamically-allocated
 * state of the iterator is duplicated too.
 *
 * Returns: (transfer full): a copy of the @iter,
 * free with g_win32_registry_subkey_iter_free ()
 *
 * Since: 2.46
 **/
GWin32RegistrySubkeyIter *
g_win32_registry_subkey_iter_copy (const GWin32RegistrySubkeyIter *iter)
{
  GWin32RegistrySubkeyIter *new_iter;

  g_return_val_if_fail (iter != NULL, NULL);

  new_iter = g_new0 (GWin32RegistrySubkeyIter, 1);

  new_iter->key = g_object_ref (iter->key);
  new_iter->counter = iter->counter;
  new_iter->subkey_count = iter->subkey_count;
  new_iter->subkey_name = g_wcsdup (iter->subkey_name, iter->subkey_name_size);
  new_iter->subkey_name_size = iter->subkey_name_size;

  if (iter->subkey_name_u8)
    new_iter->subkey_name_u8 = iter->subkey_name_u8;
  else
    new_iter->subkey_name_u8 = NULL;

  return new_iter;
}

/**
 * g_win32_registry_subkey_iter_free:
 * @iter: a dynamically-allocated iterator
 *
 * Free an iterator allocated on the heap. For iterators that are allocated
 * on the stack use g_win32_registry_subkey_iter_clear () instead.
 *
 * Since: 2.46
 **/
void
g_win32_registry_subkey_iter_free (GWin32RegistrySubkeyIter *iter)
{
  g_return_if_fail (iter != NULL);

  g_object_unref (iter->key);
  g_free (iter->subkey_name);
  g_free (iter->subkey_name_u8);
  g_free (iter);
}

/**
 * g_win32_registry_subkey_iter_assign:
 * @iter: a #GWin32RegistrySubkeyIter
 * @other: another #GWin32RegistrySubkeyIter
 *
 * Assigns the value of @other to @iter.  This function
 * is not useful in applications, because iterators can be assigned
 * with `GWin32RegistrySubkeyIter i = j;`. The
 * function is used by language bindings.
 *
 * Since: 2.46
 **/
void
g_win32_registry_subkey_iter_assign (GWin32RegistrySubkeyIter       *iter,
                                     const GWin32RegistrySubkeyIter *other)
{
  g_return_if_fail (iter != NULL);
  g_return_if_fail (other != NULL);

  *iter = *other;
}


G_DEFINE_BOXED_TYPE (GWin32RegistrySubkeyIter, g_win32_registry_subkey_iter,
                     g_win32_registry_subkey_iter_copy,
                     g_win32_registry_subkey_iter_free)

/**
 * g_win32_registry_value_iter_copy:
 * @iter: an iterator
 *
 * Creates a dynamically-allocated copy of an iterator. Dynamically-allocated
 * state of the iterator is duplicated too.
 *
 * Returns: (transfer full): a copy of the @iter,
 * free with g_win32_registry_value_iter_free ().
 *
 * Since: 2.46
 **/
GWin32RegistryValueIter *
g_win32_registry_value_iter_copy (const GWin32RegistryValueIter *iter)
{
  GWin32RegistryValueIter *new_iter;

  g_return_val_if_fail (iter != NULL, NULL);

  new_iter = g_new0 (GWin32RegistryValueIter, 1);

  new_iter->key = g_object_ref (iter->key);
  new_iter->counter = iter->counter;
  new_iter->value_count = iter->value_count;
  new_iter->value_name = g_wcsdup (iter->value_name, iter->value_name_size);
  new_iter->value_name_size = iter->value_name_size;

  if (iter->value_data != NULL)
    new_iter->value_data = g_memdup (iter->value_data, iter->value_data_size);

  new_iter->value_data_size = iter->value_data_size;

  if (iter->value_name_u8 != NULL)
    new_iter->value_name_u8 = g_strdup (iter->value_name_u8);

  new_iter->value_name_u8_len = iter->value_name_u8_len;

  if (iter->value_data_u8 != NULL)
    new_iter->value_data_u8 = g_strdup (iter->value_data_u8);

  new_iter->value_data_u8_size = iter->value_data_u8_size;

  if (iter->value_data_expanded != NULL)
    new_iter->value_data_expanded = g_wcsdup ((gunichar2 *) iter->value_data_expanded,
                                              iter->value_data_expanded_charsize * sizeof (gunichar2));

  new_iter->value_data_expanded_charsize = iter->value_data_expanded_charsize;

  if (iter->value_data_expanded_u8 != NULL)
    new_iter->value_data_expanded_u8 = g_memdup (iter->value_data_expanded_u8,
                                                 iter->value_data_expanded_charsize);

  new_iter->value_data_expanded_u8_size = iter->value_data_expanded_charsize;

  return new_iter;
}

/**
 * g_win32_registry_value_iter_free:
 * @iter: a dynamically-allocated iterator
 *
 * Free an iterator allocated on the heap. For iterators that are allocated
 * on the stack use g_win32_registry_value_iter_clear () instead.
 *
 * Since: 2.46
 **/
void
g_win32_registry_value_iter_free (GWin32RegistryValueIter *iter)
{
  g_return_if_fail (iter != NULL);

  g_object_unref (iter->key);
  g_free (iter->value_name);
  g_free (iter->value_data);
  g_free (iter->value_data_expanded);
  g_free (iter->value_name_u8);
  g_free (iter->value_data_u8);
  g_free (iter->value_data_expanded_u8);
  g_free (iter);
}

/**
 * g_win32_registry_value_iter_assign:
 * @iter: a #GWin32RegistryValueIter
 * @other: another #GWin32RegistryValueIter
 *
 * Assigns the value of @other to @iter.  This function
 * is not useful in applications, because iterators can be assigned
 * with `GWin32RegistryValueIter i = j;`. The
 * function is used by language bindings.
 *
 * Since: 2.46
 **/
void
g_win32_registry_value_iter_assign (GWin32RegistryValueIter       *iter,
                                    const GWin32RegistryValueIter *other)
{
  g_return_if_fail (iter != NULL);
  g_return_if_fail (other != NULL);

  *iter = *other;
}

G_DEFINE_BOXED_TYPE (GWin32RegistryValueIter, g_win32_registry_value_iter,
                     g_win32_registry_value_iter_copy,
                     g_win32_registry_value_iter_free)

/**
 * SECTION:gwin32registrykey
 * @title: GWin32RegistryKey
 * @short_description: W32 registry access helper
 * @include: gio/win32/gwin32registrykey.h
 *
 * #GWin32RegistryKey represents a single Windows Registry key.
 *
 * #GWin32RegistryKey is used by a number of helper functions that read
 * Windows Registry. All keys are opened with read-only access, and at
 * the moment there is no API for writing into registry keys or creating
 * new ones.
 *
 * #GWin32RegistryKey implements the #GInitable interface, so if it is manually
 * constructed by e.g. g_object_new() you must call g_initable_init() and check
 * the results before using the object. This is done automatically
 * in g_win32_registry_key_new() and g_win32_registry_key_get_child(), so these
 * functions can return %NULL.
 *
 * To increase efficiency, a UTF-16 variant is available for all functions
 * that deal with key or value names in the registry. Use these to perform
 * deep registry queries or other operations that require querying a name
 * of a key or a value and then opening it (or querying its data). The use
 * of UTF-16 functions avoids the overhead of converting names to UTF-8 and
 * back.
 *
 * All functions operate in current user's context (it is not possible to
 * access registry tree of a different user).
 *
 * Key paths must use '\\' as a separator, '/' is not supported. Key names
 * must not include '\\', because it's used as a separator. Value names
 * can include '\\'.
 *
 * Key and value names are not case sensitive.
 *
 * Full key name (excluding the pre-defined ancestor's name) can't exceed
 * 255 UTF-16 characters, give or take. Value name can't exceed 16383 UTF-16
 * characters. Tree depth is limited to 512 levels.
 **/

struct _GWin32RegistryKeyPrivate {
  /* Ancestor of this key. May not be the immediate parent, because
   * RegOpenKeyEx() allows grand*-children to be opened transitively.
   * May be NULL.
   */
  GWin32RegistryKey *ancestor;

  /* Handle to the key */
  HKEY handle;

  /* Full absolute path of the key, in UTF-16. Always allocated.
   * Can become out of sync if the key is renamed from while we have it
   * open, check watch_indicator to see if anything changed.
   */
  gunichar2 *absolute_path_w;

  /* Full absolute path of the key, in UTF-8. Allocated when needed by
   * converting the UTF-16 value from absolute_path_w. */
  gchar *absolute_path;

  /* TRUE if this object represents one of the pre-defined keys
   * (and thus must not be closed).
   */
  gboolean predefined;

  /* Set to G_WIN32_KEY_UNWATCHED if the key is not being watched.
   * Set to G_WIN32_KEY_WATCHED when the key is put on watch notification.
   */
  gint watch_indicator;

  /* Set to G_WIN32_KEY_UNKNOWN while the key is not being watched.
   * Set to G_WIN32_KEY_UNCHANGED once the key is put under watch.
   * Set to G_WIN32_KEY_CHANGED by the watch notification APC on key change.
   */
  gint change_indicator;

  /* Unset after the key is changed, individual bits are set when their
   * respective key parameters are updated from the registry.
   * This prevents GLib from re-querying things like key name each time
   * one is requested by the client while key is in G_WIN32_KEY_CHANGED state.
   */
  GWin32RegistryKeyUpdateFlag update_flags;

  GWin32RegistryKeyWatchCallbackFunc callback;

  gpointer user_data;
};

static void     g_win32_registry_key_initable_iface_init (GInitableIface  *iface);
static gboolean g_win32_registry_key_initable_init       (GInitable       *initable,
                                                          GCancellable    *cancellable,
                                                          GError         **error);

G_DEFINE_TYPE_WITH_CODE (GWin32RegistryKey, g_win32_registry_key, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (GWin32RegistryKey)
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                g_win32_registry_key_initable_iface_init));

static void
g_win32_registry_key_dispose (GObject *object)
{
  GWin32RegistryKey *key;
  GWin32RegistryKeyPrivate *priv;

  key = G_WIN32_REGISTRY_KEY (object);
  priv = key->priv;

  g_clear_object (&priv->ancestor);
  g_clear_pointer (&priv->absolute_path_w, g_free);
  g_clear_pointer (&priv->absolute_path, g_free);

  if (!priv->predefined && priv->handle != INVALID_HANDLE_VALUE)
    {
      RegCloseKey (priv->handle);
      priv->handle = INVALID_HANDLE_VALUE;
    }

  G_OBJECT_CLASS (g_win32_registry_key_parent_class)->dispose (object);
}

/**
 * g_win32_registry_key_new:
 * @path: absolute full name of a key to open (in UTF-8)
 * @error: (nullable): a pointer to a %NULL #GError, or %NULL
 *
 * Creates an object that represents a registry key specified by @path.
 * @path must start with one of the following pre-defined names:
 * - HKEY_CLASSES_ROOT
 * - HKEY_CURRENT_CONFIG
 * - HKEY_CURRENT_USER
 * - HKEY_CURRENT_USER_LOCAL_SETTINGS
 * - HKEY_LOCAL_MACHINE
 * - HKEY_PERFORMANCE_DATA
 * - HKEY_PERFORMANCE_NLSTEXT
 * - HKEY_PERFORMANCE_TEXT
 * - HKEY_USERS
 * @path must not end with '\\'.
 *
 * Returns: (nullable) (transfer full): a #GWin32RegistryKey or %NULL if can't
 *   be opened. Free with g_object_unref().
 */
GWin32RegistryKey *
g_win32_registry_key_new (const gchar  *path,
                          GError      **error)
{
  g_return_val_if_fail (path != NULL, NULL);

  return g_initable_new (G_TYPE_WIN32_REGISTRY_KEY,
                         NULL,
                         error,
                         "path",
                         path,
                         NULL);
}

/**
 * g_win32_registry_key_new_w:
 * @path: (in) (transfer none): absolute full name of a key to open (in UTF-16)
 * @error: (inout) (optional) (nullable): a pointer to a %NULL #GError, or %NULL
 *
 * Creates an object that represents a registry key specified by @path.
 * @path must start with one of the following pre-defined names:
 * - HKEY_CLASSES_ROOT
 * - HKEY_CURRENT_CONFIG
 * - HKEY_CURRENT_USER
 * - HKEY_CURRENT_USER_LOCAL_SETTINGS
 * - HKEY_LOCAL_MACHINE
 * - HKEY_PERFORMANCE_DATA
 * - HKEY_PERFORMANCE_NLSTEXT
 * - HKEY_PERFORMANCE_TEXT
 * - HKEY_USERS
 * @path must not end with L'\\'.
 *
 * Returns: (nullable) (transfer full): a #GWin32RegistryKey or %NULL if can't
 *   be opened. Free with g_object_unref().
 */
GWin32RegistryKey *
g_win32_registry_key_new_w (const gunichar2  *path,
                            GError          **error)
{
  GObject *result;

  g_return_val_if_fail (path != NULL, NULL);

  result = g_initable_new (G_TYPE_WIN32_REGISTRY_KEY,
                           NULL,
                           error,
                           "path-utf16",
                           g_wcsdup (path, -1),
                           NULL);

  return result ? G_WIN32_REGISTRY_KEY (result) : NULL;
}

static void
g_win32_registry_key_initable_iface_init (GInitableIface *iface)
{
  iface->init = g_win32_registry_key_initable_init;
}

static gboolean
g_win32_registry_key_initable_init (GInitable     *initable,
                                    GCancellable  *cancellable,
                                    GError       **error)
{
  GWin32RegistryKey *key;
  GWin32RegistryKeyPrivate *priv;
  gunichar2 *path;
  gunichar2 *first_chunk_end;
  gsize first_chunk_len;
  gunichar2 *second_chunk_begin;
  gunichar2 *first_chunk;
  HKEY ancestor;
  HKEY key_handle;
  LONG opened;

  g_return_val_if_fail (G_IS_WIN32_REGISTRY_KEY (initable), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  key = G_WIN32_REGISTRY_KEY (initable);
  priv = key->priv;

  if (priv->absolute_path_w == NULL)
    {
      priv->absolute_path_w = g_utf8_to_utf16 (priv->absolute_path,
                                               -1,
                                               NULL,
                                               NULL,
                                               error);

      if (priv->absolute_path_w == NULL)
        return FALSE;
    }

  path = priv->absolute_path_w;

  first_chunk_end = wcschr (path, L'\\');

  if (first_chunk_end == NULL)
    first_chunk_end = &path[wcslen (path)];

  first_chunk_len = first_chunk_end - path;
  first_chunk = g_wcsdup (path, -1);
  first_chunk[first_chunk_len] = L'\0';
  if (wcscmp (first_chunk, L"HKEY_CLASSES_ROOT") == 0)
    ancestor = HKEY_CLASSES_ROOT;
  else if (wcscmp (first_chunk, L"HKEY_LOCAL_MACHINE") == 0)
    ancestor = HKEY_LOCAL_MACHINE;
  else if (wcscmp (first_chunk, L"HKEY_CURRENT_USER") == 0)
    ancestor = HKEY_CURRENT_USER;
  else if (wcscmp (first_chunk, L"HKEY_CURRENT_CONFIG") == 0)
    ancestor = HKEY_CURRENT_CONFIG;
  else if (wcscmp (first_chunk, L"HKEY_CURRENT_USER_LOCAL_SETTINGS") == 0)
    ancestor = HKEY_CURRENT_USER_LOCAL_SETTINGS;
  else if (wcscmp (first_chunk, L"HKEY_USERS") == 0)
    ancestor = HKEY_USERS;
  else if (wcscmp (first_chunk, L"HKEY_PERFORMANCE_DATA") == 0)
    ancestor = HKEY_PERFORMANCE_DATA;
  else if (wcscmp (first_chunk, L"HKEY_PERFORMANCE_NLSTEXT") == 0)
    ancestor = HKEY_PERFORMANCE_NLSTEXT;
  else if (wcscmp (first_chunk, L"HKEY_PERFORMANCE_TEXT") == 0)
    ancestor = HKEY_PERFORMANCE_TEXT;
  else
    {
      g_critical ("Root key '%S' is not a pre-defined key", first_chunk);
      g_free (first_chunk);
      return FALSE;
    }

  g_free (first_chunk);

  second_chunk_begin = first_chunk_end;

  while (second_chunk_begin[0] != L'\0' && second_chunk_begin[0] == L'\\')
    second_chunk_begin++;

  if (second_chunk_begin != first_chunk_end && second_chunk_begin[0] == L'\0')
    {
      g_critical ("Key name '%S' ends with '\\'", path);
      return FALSE;
    }

  opened = RegOpenKeyExW (ancestor, second_chunk_begin, 0, KEY_READ, &key_handle);

  if (opened != ERROR_SUCCESS)
    {
      g_set_error (error, G_IO_ERROR, g_io_error_from_win32_error (opened),
                   "Failed to open registry key '%S'", path);
      return FALSE;
    }

  priv->ancestor = NULL;
  priv->handle = key_handle;
  priv->predefined = (second_chunk_begin[0] == L'\0');

  return TRUE;
}

/**
 * g_win32_registry_key_get_child:
 * @key: (in) (transfer none): a parent #GWin32RegistryKey
 * @subkey: (in) (transfer none): name of a child key to open (in UTF-8), relative to @key
 * @error: (inout) (optional) (nullable): a pointer to a %NULL #GError, or %NULL
 *
 * Opens a @subkey of the @key.
 *
 * Returns: (nullable): a #GWin32RegistryKey or %NULL if can't be opened. Free
 *                      with g_object_unref().
 */
GWin32RegistryKey *
g_win32_registry_key_get_child (GWin32RegistryKey  *key,
                                const gchar        *subkey,
                                GError            **error)
{
  gunichar2 *subkey_w;
  GWin32RegistryKey *result = NULL;

  g_return_val_if_fail (G_IS_WIN32_REGISTRY_KEY (key), NULL);
  g_return_val_if_fail (subkey != NULL, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  subkey_w = g_utf8_to_utf16 (subkey, -1, NULL, NULL, error);

  if (subkey_w != NULL)
    {
      result = g_win32_registry_key_get_child_w (key, subkey_w, error);
      g_free (subkey_w);
    }

  return result;
}

/**
 * g_win32_registry_key_get_child_w:
 * @key: (in) (transfer none): a parent #GWin32RegistryKey
 * @subkey: (in) (transfer none): name of a child key to open (in UTF-8), relative to @key
 * @error: (inout) (optional) (nullable): a pointer to a %NULL #GError, or %NULL
 *
 * Opens a @subkey of the @key.
 *
 * Returns: (nullable): a #GWin32RegistryKey or %NULL if can't be opened. Free
 *                      with g_object_unref().
 */
GWin32RegistryKey *
g_win32_registry_key_get_child_w (GWin32RegistryKey  *key,
                                  const gunichar2    *subkey,
                                  GError            **error)
{
  HKEY key_handle;
  LONG opened;
  const gunichar2 *end_of_subkey;
  gsize subkey_len;
  GWin32RegistryKey *result;
  const gunichar2 *key_path;

  g_return_val_if_fail (G_IS_WIN32_REGISTRY_KEY (key), NULL);
  g_return_val_if_fail (subkey != NULL, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  if (subkey[0] == L'\\')
    {
      g_critical ("Subkey name '%S' starts with '\\'", subkey);
      return NULL;
    }

  subkey_len = wcslen (subkey);
  end_of_subkey = &subkey[subkey_len];

  if (subkey_len == 0)
    end_of_subkey = subkey;

  if (end_of_subkey[0] == L'\\')
    {
      g_critical ("Subkey name '%S' ends with '\\'", subkey);
      return NULL;
    }

  key_path = g_win32_registry_key_get_path_w (key);
  opened = RegOpenKeyExW (key->priv->handle, subkey, 0, KEY_READ, &key_handle);

  if (opened != ERROR_SUCCESS)
    {
      g_set_error (error, G_IO_ERROR, g_io_error_from_win32_error (opened),
                   "Failed to open registry subkey '%S' of key '%S'",
                   subkey, key_path);
      return NULL;
    }

  result = g_object_new (G_TYPE_WIN32_REGISTRY_KEY, NULL);

  result->priv->handle = key_handle;
  result->priv->absolute_path_w =
      g_malloc ((wcslen (key_path) + 2 + subkey_len) * sizeof (gunichar2));
  result->priv->absolute_path_w[0] = L'\0';
  wcscat (&result->priv->absolute_path_w[0], key_path);
  wcscat (&result->priv->absolute_path_w[wcslen (key_path)], L"\\");
  wcscat (&result->priv->absolute_path_w[wcslen (key_path) + 1], subkey);
  result->priv->predefined = (subkey[0] == L'\0' && key->priv->predefined);

  if (subkey[0] != L'\0')
    result->priv->ancestor = g_object_ref (key);
  else
    result->priv->ancestor = NULL;

  result->priv->change_indicator = G_WIN32_KEY_UNKNOWN;

  return result;
}

/**
 * g_win32_registry_subkey_iter_init:
 * @iter: (in) (transfer none): a pointer to a #GWin32RegistrySubkeyIter
 * @key: (in) (transfer none): a #GWin32RegistryKey to iterate over
 * @error: (inout) (optional) (nullable): a pointer to %NULL #GError, or %NULL
 *
 * Initialises (without allocating) a #GWin32RegistrySubkeyIter.  @iter may be
 * completely uninitialised prior to this call; its old value is
 * ignored.
 *
 * The iterator remains valid for as long as @key exists.
 * Clean up its internal buffers with a call to
 * g_win32_registry_subkey_iter_clear() when done.
 *
 * Returns: %TRUE if iterator was initialized successfully, %FALSE on error.
 *
 * Since: 2.46
 **/
gboolean
g_win32_registry_subkey_iter_init (GWin32RegistrySubkeyIter  *iter,
                                   GWin32RegistryKey         *key,
                                   GError                   **error)
{
  LONG status;
  DWORD subkey_count;
  DWORD max_subkey_len;

  g_return_val_if_fail (iter != NULL, FALSE);
  g_return_val_if_fail (G_IS_WIN32_REGISTRY_KEY (key), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  status = RegQueryInfoKeyW (key->priv->handle,
                             NULL, NULL, NULL,
                             &subkey_count, &max_subkey_len,
                             NULL, NULL, NULL, NULL, NULL, NULL);

  if (status != ERROR_SUCCESS)
    {
      g_set_error (error, G_IO_ERROR, g_io_error_from_win32_error (status),
                   "Failed to query info for registry key '%S'",
                   g_win32_registry_key_get_path_w (key));
      return FALSE;
    }

  iter->key = g_object_ref (key);
  iter->counter = -1;
  iter->subkey_count = subkey_count;
  iter->subkey_name_size = sizeof (gunichar2) * (max_subkey_len + 1);
  iter->subkey_name = g_malloc (iter->subkey_name_size);
  iter->subkey_name_u8 = NULL;

  return TRUE;
}

/**
 * g_win32_registry_subkey_iter_clear:
 * @iter: (in) (transfer none): a #GWin32RegistrySubkeyIter
 *
 * Frees internal buffers of a #GWin32RegistrySubkeyIter.
 *
 * Since: 2.46
 **/
void
g_win32_registry_subkey_iter_clear (GWin32RegistrySubkeyIter *iter)
{
  g_return_if_fail (iter != NULL);

  g_free (iter->subkey_name);
  g_free (iter->subkey_name_u8);
  g_clear_object (&iter->key);
}

/**
 * g_win32_registry_subkey_iter_n_subkeys:
 * @iter: (in) (transfer none): a #GWin32RegistrySubkeyIter
 *
 * Queries the number of subkeys items in the key that we are
 * iterating over.  This is the total number of subkeys -- not the number
 * of items remaining.
 *
 * This information is accurate at the point of iterator initialization,
 * and may go out of sync with reality even while subkeys are enumerated.
 *
 * Returns: the number of subkeys in the key
 *
 * Since: 2.46
 **/
gsize
g_win32_registry_subkey_iter_n_subkeys (GWin32RegistrySubkeyIter *iter)
{
  g_return_val_if_fail (iter != NULL, 0);

  return iter->subkey_count;
}

/**
 * g_win32_registry_subkey_iter_next:
 * @iter: (in) (transfer none): a #GWin32RegistrySubkeyIter
 * @skip_errors: (in): %TRUE if iterator should silently ignore errors (such as
 *     the actual number of subkeys being less than expected) and
 *     proceed forward
 * @error: (nullable): a pointer to %NULL #GError, or %NULL
 *
 * Moves iterator to the next subkey.
 * Enumeration errors can be ignored if @skip_errors is %TRUE
 *
 * Here is an example for iterating with g_win32_registry_subkey_iter_next():
 * |[<!-- language="C" -->
 *   // recursively iterate a key
 *   void
 *   iterate_key_recursive (GWin32RegistryKey *key)
 *   {
 *     GWin32RegistrySubkeyIter iter;
 *     gchar *name;
 *     GWin32RegistryKey *child;
 *
 *     if (!g_win32_registry_subkey_iter_init (&iter, key, NULL))
 *       return;
 *
 *     while (g_win32_registry_subkey_iter_next (&iter, TRUE, NULL))
 *       {
 *         if (!g_win32_registry_subkey_iter_get_name (&iter, &name, NULL, NULL))
 *           continue;
 *
 *         g_print ("subkey '%s'\n", name);
 *         child = g_win32_registry_key_get_child (key, name, NULL);
 *
 *         if (child)
 *           iterate_key_recursive (child);
 *       }
 *
 *     g_win32_registry_subkey_iter_clear (&iter);
 *   }
 * ]|
 *
 * Returns: %TRUE if next subkey info was retrieved, %FALSE otherwise.
 *
 * Since: 2.46
 **/
gboolean
g_win32_registry_subkey_iter_next (GWin32RegistrySubkeyIter  *iter,
                                   gboolean                   skip_errors,
                                   GError                   **error)
{
  LONG status;
  DWORD subkey_len;

  g_return_val_if_fail (iter != NULL, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  if G_UNLIKELY (iter->counter >= iter->subkey_count)
    {
      g_critical ("g_win32_registry_subkey_iter_get_next_w: must not be called again "
                  "after FALSE has already been returned.");
      return FALSE;
    }

  while (TRUE)
    {
      iter->counter += 1;

      if (iter->counter >= iter->subkey_count)
        return FALSE;

      /* Including 0-terminator */
      subkey_len = iter->subkey_name_size;
      status = RegEnumKeyExW (iter->key->priv->handle,
                              iter->counter,
                              iter->subkey_name,
                              &subkey_len,
                              NULL, NULL, NULL, NULL);

      if (status == ERROR_SUCCESS)
        {
          iter->subkey_name_len = subkey_len;

          return TRUE;
        }

      if (!skip_errors)
        {
          g_set_error (error, G_IO_ERROR, g_io_error_from_win32_error (status),
                       "Failed to enumerate subkey #%d for key '%S'",
                       iter->counter, g_win32_registry_key_get_path_w (iter->key));
          iter->subkey_count = 0;

          return FALSE;
        }
    }
}

/**
 * g_win32_registry_subkey_iter_get_name_w:
 * @iter: (in) (transfer none): a #GWin32RegistrySubkeyIter
 * @subkey_name: (out callee-allocates) (transfer none): Pointer to a location
 *     to store the name of a subkey (in UTF-16).
 * @subkey_name_len: (out) (optional) (transfer none): Pointer to a location
 *     to store the length of @subkey_name, in gunichar2s, excluding
 *     NUL-terminator.
 *     %NULL if length is not needed.
 * @error: (nullable): a pointer to %NULL #GError, or %NULL
 *
 * Same as g_win32_registry_subkey_iter_get_next(), but outputs UTF-16-encoded
 * data, without converting it to UTF-8 first.
 *
 * Returns: %TRUE if the name was retrieved, %FALSE otherwise.
 *
 * Since: 2.46
 **/
gboolean
g_win32_registry_subkey_iter_get_name_w (GWin32RegistrySubkeyIter  *iter,
                                         gunichar2                **subkey_name,
                                         gsize                     *subkey_name_len,
                                         GError                   **error)
{
  g_return_val_if_fail (iter != NULL, FALSE);
  g_return_val_if_fail (subkey_name != NULL, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  if G_UNLIKELY (iter->counter >= iter->subkey_count)
    {
      g_critical ("g_win32_registry_subkey_iter_get_name_w: must not be called "
                  "after FALSE has already been returned by "
                  "g_win32_registry_subkey_iter_next.");
      return FALSE;
    }

  *subkey_name = iter->subkey_name;

  if (subkey_name_len)
    *subkey_name_len = iter->subkey_name_len;

  return TRUE;
}

/**
 * g_win32_registry_subkey_iter_get_name:
 * @iter: (in) (transfer none): a #GWin32RegistrySubkeyIter
 * @subkey_name: (out callee-allocates) (transfer none): Pointer to a location
 *     to store the name of a subkey (in UTF-8). Free with g_free().
 * @subkey_name_len: (out) (optional): Pointer to a location to store the
 *     length of @subkey_name, in gchars, excluding NUL-terminator.
 *     %NULL if length is not needed.
 * @error: (nullable): a pointer to %NULL #GError, or %NULL
 *
 * Gets the name of the subkey at the @iter potision.
 *
 * Returns: %TRUE if the name was retrieved, %FALSE otherwise.
 *
 * Since: 2.46
 **/
gboolean
g_win32_registry_subkey_iter_get_name (GWin32RegistrySubkeyIter  *iter,
                                       gchar                    **subkey_name,
                                       gsize                     *subkey_name_len,
                                       GError                   **error)
{
  glong subkey_name_len_glong;

  g_return_val_if_fail (iter != NULL, FALSE);
  g_return_val_if_fail (subkey_name != NULL, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  if G_UNLIKELY (iter->counter >= iter->subkey_count)
    {
      g_critical ("g_win32_registry_subkey_iter_get_name_w: must not be called "
                  "after FALSE has already been returned by "
                  "g_win32_registry_subkey_iter_next.");
      return FALSE;
    }

  g_clear_pointer (&iter->subkey_name_u8, g_free);
  iter->subkey_name_u8 = g_utf16_to_utf8 (iter->subkey_name,
                                          iter->subkey_name_len,
                                          NULL,
                                          &subkey_name_len_glong,
                                          error);

  if (iter->subkey_name_u8 != NULL)
    {
      *subkey_name_len = subkey_name_len_glong;
      return TRUE;
    }

  return FALSE;
}

/**
 * g_win32_registry_value_iter_init:
 * @iter: (in) (transfer none): a pointer to a #GWin32RegistryValueIter
 * @key: (in) (transfer none): a #GWin32RegistryKey to iterate over
 * @error: (nullable): a pointer to %NULL #GError, or %NULL
 *
 * Initialises (without allocating) a #GWin32RegistryValueIter.  @iter may be
 * completely uninitialised prior to this call; its old value is
 * ignored.
 *
 * The iterator remains valid for as long as @key exists.
 * Clean up its internal buffers with a call to
 * g_win32_registry_value_iter_clear() when done.
 *
 * Returns: %TRUE if iterator was initialized successfully, %FALSE on error.
 *
 * Since: 2.46
 **/
gboolean
g_win32_registry_value_iter_init (GWin32RegistryValueIter  *iter,
                                  GWin32RegistryKey        *key,
                                  GError                  **error)
{
  LONG status;
  DWORD value_count;
  DWORD max_value_len;
  DWORD max_data_len;

  g_return_val_if_fail (iter != NULL, FALSE);
  g_return_val_if_fail (G_IS_WIN32_REGISTRY_KEY (key), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  status = RegQueryInfoKeyW (key->priv->handle,
                             NULL, NULL, NULL, NULL, NULL, NULL,
                             &value_count, &max_value_len,
                             &max_data_len, NULL, NULL);

  if (status != ERROR_SUCCESS)
    {
      g_set_error (error, G_IO_ERROR, g_io_error_from_win32_error (status),
                   "Failed to query info for registry key '%S'",
                   g_win32_registry_key_get_path_w (key));
      return FALSE;
    }

  iter->key = g_object_ref (key);
  iter->counter = -1;
  iter->value_count = value_count;
  iter->value_name_size = sizeof (gunichar2) * (max_value_len + 1);
  iter->value_name = g_malloc (iter->value_name_size);
  /* FIXME: max_value_data_len is said to have no size limit in newer W32
   * versions (and its size limit in older ones is 1MB!). Consider limiting it
   * with a hard-coded value, or by allowing the user to choose a limit.
   */
  /* Two extra gunichar2s is for cases when a string was stored in the
   * Registry without a 0-terminator (for multiline strings - 00-terminator),
   * and we need to terminate it ourselves.
   */
  iter->value_data_size = max_data_len + sizeof (gunichar2) * 2;
  iter->value_data = g_malloc (iter->value_data_size);
  iter->value_name_u8 = NULL;
  iter->value_data_u8 = NULL;
  iter->value_data_expanded = NULL;
  iter->value_data_expanded_charsize = 0;
  iter->value_data_expanded_u8 = NULL;
  iter->value_data_expanded_u8_size = 0;
  return TRUE;
}

/**
 * g_win32_registry_value_iter_clear:
 * @iter: (in) (transfer none): a #GWin32RegistryValueIter
 *
 * Frees internal buffers of a #GWin32RegistryValueIter.
 *
 * Since: 2.46
 **/
void
g_win32_registry_value_iter_clear (GWin32RegistryValueIter *iter)
{
  g_return_if_fail (iter != NULL);

  g_free (iter->value_name);
  g_free (iter->value_data);
  g_free (iter->value_name_u8);
  g_free (iter->value_data_u8);
  g_free (iter->value_data_expanded);
  g_free (iter->value_data_expanded_u8);
  g_clear_object (&iter->key);
}

/**
 * g_win32_registry_value_iter_n_values:
 * @iter: (in) (transfer none): a #GWin32RegistryValueIter
 *
 * Queries the number of values items in the key that we are
 * iterating over.  This is the total number of values -- not the number
 * of items remaining.
 *
 * This information is accurate at the point of iterator initialization,
 * and may go out of sync with reality even while values are enumerated.
 *
 * Returns: the number of values in the key
 *
 * Since: 2.46
 **/
gsize
g_win32_registry_value_iter_n_values (GWin32RegistryValueIter *iter)
{
  g_return_val_if_fail (iter != NULL, 0);

  return iter->value_count;
}

static GWin32RegistryValueType
_g_win32_registry_type_w_to_g (DWORD value_type)
{
  switch (value_type)
    {
    case REG_BINARY:
      return G_WIN32_REGISTRY_VALUE_BINARY;
    case REG_DWORD:
      return G_WIN32_REGISTRY_VALUE_UINT32;
#if G_BYTE_ORDER == G_BIG_ENDIAN
    case REG_DWORD_LITTLE_ENDIAN:
      return G_WIN32_REGISTRY_VALUE_UINT32LE;
#else
    case REG_DWORD_BIG_ENDIAN:
      return G_WIN32_REGISTRY_VALUE_UINT32BE;
#endif
    case REG_EXPAND_SZ:
      return G_WIN32_REGISTRY_VALUE_EXPAND_STR;
    case REG_LINK:
      return G_WIN32_REGISTRY_VALUE_LINK;
    case REG_MULTI_SZ:
      return G_WIN32_REGISTRY_VALUE_MULTI_STR;
    case REG_NONE:
      return G_WIN32_REGISTRY_VALUE_NONE;
    case REG_QWORD:
      return G_WIN32_REGISTRY_VALUE_UINT64;
#if G_BYTE_ORDER == G_BIG_ENDIAN
    case REG_QWORD_LITTLE_ENDIAN:
      return G_WIN32_REGISTRY_VALUE_UINT64LE;
#endif
    case REG_SZ:
      return G_WIN32_REGISTRY_VALUE_STR;
    default:
      return G_WIN32_REGISTRY_VALUE_NONE;
    }
}

static gsize
ensure_nul_termination (GWin32RegistryValueType  value_type,
                        guint8                  *value_data,
                        gsize                    value_data_size)
{
  gsize new_size = value_data_size;

  if (value_type == G_WIN32_REGISTRY_VALUE_EXPAND_STR ||
      value_type == G_WIN32_REGISTRY_VALUE_LINK ||
      value_type == G_WIN32_REGISTRY_VALUE_STR)
    {
      if ((value_data_size < 2) ||
          (value_data[value_data_size - 1] != 0) ||
          (value_data[value_data_size - 2] != 0))
        {
          value_data[value_data_size] = 0;
          value_data[value_data_size + 1] = 0;
          new_size += 2;
        }
    }
  else if (value_type == G_WIN32_REGISTRY_VALUE_MULTI_STR)
    {
      if ((value_data_size < 4) ||
          (value_data[value_data_size - 1] != 0) ||
          (value_data[value_data_size - 2] != 0) ||
          (value_data[value_data_size - 3] != 0) ||
          (value_data[value_data_size - 4] != 0))
        {
          value_data[value_data_size] = 0;
          value_data[value_data_size + 1] = 0;
          value_data[value_data_size + 2] = 0;
          value_data[value_data_size + 3] = 0;
          new_size += 4;
        }
    }

  return new_size;
}

/**
 * g_win32_registry_value_iter_next:
 * @iter: (in) (transfer none): a #GWin32RegistryValueIter
 * @skip_errors: (in): %TRUE if iterator should silently ignore errors (such as
 *     the actual number of values being less than expected) and
 *     proceed forward
 * @error: (nullable): a pointer to %NULL #GError, or %NULL
 *
 * Advances iterator to the next value in the key. If no more values remain then
 * FALSE is returned.
 * Enumeration errors can be ignored if @skip_errors is %TRUE
 *
 * Here is an example for iterating with g_win32_registry_value_iter_next():
 * |[<!-- language="C" -->
 *   // iterate values of a key
 *   void
 *   iterate_values_recursive (GWin32RegistryKey *key)
 *   {
 *     GWin32RegistryValueIter iter;
 *     gchar *name;
 *     GWin32RegistryValueType val_type;
 *     gchar *val_data;
 *
 *     if (!g_win32_registry_value_iter_init (&iter, key, NULL))
 *       return;
 *
 *     while (g_win32_registry_value_iter_next (&iter, TRUE, NULL))
 *       {
 *         if ((!g_win32_registry_value_iter_get_value_type (&iter, &value)) ||
 *             ((val_type != G_WIN32_REGISTRY_VALUE_STR) &&
 *              (val_type != G_WIN32_REGISTRY_VALUE_EXPAND_STR)))
 *           continue;
 *
 *         if (g_win32_registry_value_iter_get_value (&iter, TRUE, &name, NULL,
 *                                                    &val_data, NULL, NULL))
 *           g_print ("value '%s' = '%s'\n", name, val_data);
 *       }
 *
 *     g_win32_registry_value_iter_clear (&iter);
 *   }
 * ]|
 *
 * Returns: %TRUE if next value info was retrieved, %FALSE otherwise.
 *
 * Since: 2.46
 **/
gboolean
g_win32_registry_value_iter_next (GWin32RegistryValueIter  *iter,
                                  gboolean                  skip_errors,
                                  GError                  **error)
{
  LONG status;
  DWORD value_name_len_w;
  DWORD value_data_size_w;
  DWORD value_type_w;
  GWin32RegistryValueType value_type_g;

  g_return_val_if_fail (iter != NULL, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  if G_UNLIKELY (iter->counter >= iter->value_count)
    {
      g_critical ("g_win32_registry_value_iter_next: must not be called "
                  "again after FALSE has already been returned.");
      return FALSE;
    }

  while (TRUE)
    {
      iter->counter += 1;

      if (iter->counter >= iter->value_count)
        return FALSE;

      g_clear_pointer (&iter->value_name_u8, g_free);
      g_clear_pointer (&iter->value_data_u8, g_free);
      g_clear_pointer (&iter->value_data_expanded_u8, g_free);
      /* Including 0-terminator */
      value_name_len_w = iter->value_name_size / sizeof (gunichar2);
      value_data_size_w = iter->value_data_size;
      status = RegEnumValueW (iter->key->priv->handle,
                              iter->counter,
                              iter->value_name,
                              &value_name_len_w,
                              NULL,
                              &value_type_w,
                              (LPBYTE) iter->value_data,
                              &value_data_size_w);

      if (status != ERROR_SUCCESS && !skip_errors)
        {
          g_set_error (error, G_IO_ERROR, g_io_error_from_win32_error (status),
                       "Failed to enumerate value #%d for key '%S'",
                       iter->counter, g_win32_registry_key_get_path_w (iter->key));
          iter->value_count = 0;

          return FALSE;
        }
      else if (status != ERROR_SUCCESS && skip_errors)
        continue;

      value_type_g = _g_win32_registry_type_w_to_g (value_type_w);
      value_data_size_w = ensure_nul_termination (value_type_g,
                                                  iter->value_data,
                                                  value_data_size_w);
      iter->value_type = value_type_g;
      iter->value_expanded_type = value_type_g;
      iter->value_actual_data_size = value_data_size_w;
      iter->value_name_len = value_name_len_w;

      return TRUE;
    }
}

/**
 * g_win32_registry_value_iter_get_value_type:
 * @iter: (in) (transfer none): a #GWin32RegistryValueIter
 * @value_type: (out): Pointer to a location to store the type of
 *     the value.
 * @error: (nullable): a pointer to %NULL #GError, or %NULL
 *
 * Stores the type of the value currently being iterated over in @value_type.
 *
 * Returns: %TRUE if value type was retrieved, %FALSE otherwise.
 *
 * Since: 2.46
 **/
gboolean
g_win32_registry_value_iter_get_value_type (GWin32RegistryValueIter  *iter,
                                            GWin32RegistryValueType  *value_type,
                                            GError                  **error)
{
  g_return_val_if_fail (iter != NULL, FALSE);
  g_return_val_if_fail (value_type != NULL, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  if G_UNLIKELY (iter->counter >= iter->value_count)
    {
      g_critical ("g_win32_registry_value_iter_get_type: must not be called "
                  "again after NULL has already been returned.");
      return FALSE;
    }

  *value_type = iter->value_type;

  return TRUE;
}

/**
 * g_win32_registry_value_iter_get_name_w:
 * @iter: (in) (transfer none): a #GWin32RegistryValueIter
 * @value_name: (out callee-allocates) (transfer none): Pointer to a location
 *     to store the name of a value (in UTF-16).
 * @value_name_len: (out) (optional): Pointer to a location to store the length
 *     of @value_name, in gunichar2s, excluding NUL-terminator.
 *     %NULL if length is not needed.
 * @error: (nullable): a pointer to %NULL #GError, or %NULL
 *
 * Stores the name of the value currently being iterated over in @value_name,
 * and its length - in @value_name (if not %NULL).
 *
 * Returns: %TRUE if value name was retrieved, %FALSE otherwise.
 *
 * Since: 2.46
 **/
gboolean
g_win32_registry_value_iter_get_name_w (GWin32RegistryValueIter  *iter,
                                        gunichar2               **value_name,
                                        gsize                    *value_name_len,
                                        GError                  **error)
{
  g_return_val_if_fail (iter != NULL, FALSE);
  g_return_val_if_fail (value_name != NULL, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  if G_UNLIKELY (iter->counter >= iter->value_count)
    {
      g_critical ("g_win32_registry_value_iter_get_name_w: must not be called "
                  "again after NULL has already been returned.");
      return FALSE;
    }

  *value_name = iter->value_name;

  if (value_name_len)
    *value_name_len = iter->value_name_len;

  return TRUE;
}

/**
 * g_win32_registry_value_iter_get_name:
 * @iter: (in) (transfer none): a #GWin32RegistryValueIter
 * @value_name: (out callee-allocates) (transfer none): Pointer to a location
 *     to store the name of a value (in UTF-8).
 * @value_name_len: (out) (optional): Pointer to a location to store the length
 *     of @value_name, in gchars, excluding NUL-terminator.
 *     %NULL if length is not needed.
 * @error: (nullable): a pointer to %NULL #GError, or %NULL
 *
 * Stores the name of the value currently being iterated over in @value_name,
 * and its length - in @value_name_len (if not %NULL).
 *
 * Returns: %TRUE if value name was retrieved, %FALSE otherwise.
 *
 * Since: 2.46
 **/
gboolean
g_win32_registry_value_iter_get_name (GWin32RegistryValueIter  *iter,
                                      gchar                   **value_name,
                                      gsize                    *value_name_len,
                                      GError                  **error)
{
  glong value_name_len_glong;

  g_return_val_if_fail (iter != NULL, FALSE);
  g_return_val_if_fail (value_name != NULL, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  if G_UNLIKELY (iter->counter >= iter->value_count)
    {
      g_critical ("g_win32_registry_value_iter_get_name: must not be called "
                  "again after NULL has already been returned.");
      return FALSE;
    }

  if (iter->value_name_u8 == NULL)
    {
      iter->value_name_u8 = g_utf16_to_utf8 (iter->value_name, iter->value_name_len, NULL,
                                             &value_name_len_glong, error);

      if (iter->value_name_u8 == NULL)
        return FALSE;
    }

  *value_name = iter->value_name_u8;

  if (value_name_len)
    *value_name_len = iter->value_name_u8_len;

  return TRUE;
}

static gboolean
expand_value (gunichar2  *value,
              const gunichar2  *value_name,
              gpointer   *expanded_value,
              gsize      *expanded_charsize,
              GError    **error)
{
  DWORD value_data_expanded_charsize_w;

  value_data_expanded_charsize_w =
      ExpandEnvironmentStringsW (value,
                                 (gunichar2 *) *expanded_value,
                                 *expanded_charsize);

  if (value_data_expanded_charsize_w > *expanded_charsize)
    {
      *expanded_value = g_realloc (*expanded_value,
                                   value_data_expanded_charsize_w * sizeof (gunichar2));
      *expanded_charsize = value_data_expanded_charsize_w;
      value_data_expanded_charsize_w =
          ExpandEnvironmentStringsW (value,
                                     (gunichar2 *) *expanded_value,
                                     *expanded_charsize);
    }

  if (value_data_expanded_charsize_w == 0)
    {
      g_set_error (error, G_IO_ERROR,
                   g_io_error_from_win32_error (GetLastError ()),
                   "Failed to expand data '%S' of value %S",
                   value, value_name);
      return FALSE;
    }

  return TRUE;
}

/**
 * g_win32_registry_value_iter_get_data_w:
 * @iter: (in) (transfer none): a #GWin32RegistryValueIter
 * @auto_expand: (in): %TRUE to automatically expand G_WIN32_REGISTRY_VALUE_EXPAND_STR to
 *     G_WIN32_REGISTRY_VALUE_STR
 * @value_data: (out callee-allocates) (optional) (transfer none): Pointer to a
 *     location to store the data of the value (in UTF-16, if it's a string)
 * @value_data_size: (out) (optional): Pointer to a location to store the size
 *     of @value_data, in bytes (including any NUL-terminators, if it's a string).
 *     %NULL if length is not needed.
 * @error: (nullable): a pointer to %NULL #GError, or %NULL
 *
 * Stores the data of the value currently being iterated over in @value_data,
 * and its length - in @value_data_len (if not %NULL).
 *
 * Returns: %TRUE if value data was retrieved, %FALSE otherwise.
 *
 * Since: 2.46
 **/
gboolean
g_win32_registry_value_iter_get_data_w (GWin32RegistryValueIter  *iter,
                                        gboolean                  auto_expand,
                                        gpointer                 *value_data,
                                        gsize                    *value_data_size,
                                        GError                  **error)
{
  g_return_val_if_fail (iter != NULL, FALSE);
  g_return_val_if_fail (value_data != NULL, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  if G_UNLIKELY (iter->counter >= iter->value_count)
    {
      g_critical ("g_win32_registry_value_iter_get_data_w: must not be called "
                  "again after FALSE has already been returned.");
      return FALSE;
    }

  if (!auto_expand || (iter->value_type != G_WIN32_REGISTRY_VALUE_EXPAND_STR))
    {
      *value_data = iter->value_data;

      if (value_data_size)
        *value_data_size = iter->value_actual_data_size;

      return TRUE;
    }

  if (iter->value_type == iter->value_expanded_type)
    {
      if (!expand_value ((gunichar2 *) iter->value_data,
                         iter->value_name,
                         (gpointer *) &iter->value_data_expanded,
                         &iter->value_data_expanded_charsize,
                         error))
        return FALSE;

      iter->value_expanded_type = G_WIN32_REGISTRY_VALUE_STR;
    }

  *value_data = iter->value_data_expanded;

  if (value_data_size)
    *value_data_size = iter->value_data_expanded_charsize * sizeof (gunichar2);

  return TRUE;
}

/**
 * g_win32_registry_value_iter_get_data:
 * @iter: (in) (transfer none): a #GWin32RegistryValueIter
 * @auto_expand: (in): %TRUE to automatically expand G_WIN32_REGISTRY_VALUE_EXPAND_STR to
 *     G_WIN32_REGISTRY_VALUE_STR
 * @value_data: (out callee-allocates) (optional) (transfer none): Pointer to a
 *     location to store the data of the value (in UTF-8, if it's a string)
 * @value_data_size: (out) (optional): Pointer to a location to store the length
 *     of @value_data, in bytes (including any NUL-terminators, if it's a string).
 *     %NULL if length is not needed
 * @error: (nullable): a pointer to %NULL #GError, or %NULL
 *
 * Stores the data of the value currently being iterated over in @value_data,
 * and its length - in @value_data_len (if not %NULL).
 *
 * Returns: %TRUE if value data was retrieved, %FALSE otherwise.
 *
 * Since: 2.46
 **/
gboolean
g_win32_registry_value_iter_get_data (GWin32RegistryValueIter  *iter,
                                      gboolean                  auto_expand,
                                      gpointer                 *value_data,
                                      gsize                    *value_data_size,
                                      GError                  **error)
{
  gsize value_data_len_gsize;
  gpointer tmp;
  gsize tmp_size;

  g_return_val_if_fail (iter != NULL, FALSE);
  g_return_val_if_fail (value_data != NULL, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  if G_UNLIKELY (iter->counter >= iter->value_count)
    {
      g_critical ("g_win32_registry_value_iter_get_data: must not be called "
                  "again after FALSE has already been returned.");
      return FALSE;
    }

  if (iter->value_type != G_WIN32_REGISTRY_VALUE_EXPAND_STR &&
      iter->value_type != G_WIN32_REGISTRY_VALUE_LINK &&
      iter->value_type != G_WIN32_REGISTRY_VALUE_STR &&
      iter->value_type != G_WIN32_REGISTRY_VALUE_MULTI_STR)
    {
      *value_data = iter->value_data;

      if (value_data_size != NULL)
        *value_data_size = iter->value_actual_data_size;

      return TRUE;
    }

  if (!auto_expand || (iter->value_type != G_WIN32_REGISTRY_VALUE_EXPAND_STR))
    {
      if (iter->value_data_u8 == NULL)
        {
          iter->value_data_u8 = g_convert ((const gchar *) iter->value_data,
                                           iter->value_actual_data_size - sizeof (gunichar2) /* excl. 0 */,
                                           "UTF8", "UTF16", NULL,
                                           &value_data_len_gsize,
                                           error);

          if (iter->value_data_u8 == NULL)
            return FALSE;

          iter->value_data_u8_size = value_data_len_gsize + 1; /* incl. 0 */
        }

      *value_data = iter->value_data_u8;

      if (value_data_size != NULL)
        *value_data_size = iter->value_data_u8_size;

      return TRUE;
    }

  if (iter->value_data_expanded_u8 == NULL)
    {
      if (!g_win32_registry_value_iter_get_data_w (iter,
                                                   TRUE,
                                                   &tmp,
                                                   &tmp_size,
                                                   error))
        return FALSE;

      iter->value_data_expanded_u8 = g_convert ((const gchar *) iter->value_data_expanded,
                                                iter->value_data_expanded_charsize * sizeof (gunichar2) - sizeof (gunichar2) /* excl. 0 */,
                                                "UTF8", "UTF16", NULL,
                                                &value_data_len_gsize,
                                                error);

      if (iter->value_data_expanded_u8 == NULL)
        return FALSE;

      iter->value_data_u8_size = value_data_len_gsize + 1; /* incl. 0 */
    }

  *value_data = iter->value_data_expanded_u8;

  if (value_data_size != NULL)
    *value_data_size = iter->value_data_expanded_u8_size;

  return TRUE;
}

static void
_g_win32_registry_key_reread_kernel (GWin32RegistryKey        *key,
                                     GWin32RegistryKeyPrivate *buf)
{
  NTSTATUS status;
  KEY_BASIC_INFORMATION *basic_info;
  ULONG basic_info_size;
  ULONG datasize;

  basic_info_size = 256 * sizeof (gunichar2) + sizeof (KEY_BASIC_INFORMATION);
  basic_info = g_malloc (basic_info_size + sizeof (gunichar2));
  status = nt_query_key (key->priv->handle,
                         KeyBasicInformation,
                         basic_info,
                         basic_info_size,
                         &datasize);

  if (status == STATUS_BUFFER_OVERFLOW || status == STATUS_BUFFER_TOO_SMALL)
    {
      g_free (basic_info);
      basic_info_size = datasize;
       /* +1 for 0-terminator */
      basic_info = g_malloc (basic_info_size + sizeof (gunichar2));
      status = nt_query_key (key->priv->handle,
                             KeyBasicInformation,
                             basic_info,
                             basic_info_size,
                             &datasize);
    }

  if (status != STATUS_SUCCESS)
    {
      g_free (basic_info);
      return;
    }

  /* Ensure 0-termination */
  ((char *) basic_info)[datasize] = 0;
  ((char *) basic_info)[datasize + 1] = 0;

  buf->absolute_path_w = g_wcsdup (&basic_info->Name[0],
                                   basic_info->NameLength + sizeof (gunichar2));
  g_free (basic_info);
}

static void
_g_win32_registry_key_reread_user (GWin32RegistryKey        *key,
                                   GWin32RegistryKeyPrivate *buf)
{
  /* Use RegQueryInfoKey(). It's just like NtQueryKey(), but can't query
   * key name.
   * Since right now we only need the name, this function is a noop.
   */
}

static void
_g_win32_registry_key_reread (GWin32RegistryKey        *key,
                              GWin32RegistryKeyPrivate *buf)
{
  if (g_once_init_enter (&nt_query_key))
    {
      NtQueryKeyFunc func;
      HMODULE ntdll = GetModuleHandleW (L"ntdll.dll");

      if (ntdll != NULL)
        func = (NtQueryKeyFunc) GetProcAddress (ntdll, "NtQueryKey");
      else
        func = NULL;

      g_once_init_leave (&nt_query_key, func);
    }

  /* Assume that predefined keys never get renamed. Also, their handles probably
   * won't be accepted by NtQueryKey(), i suspect.
   */
  if (nt_query_key != NULL && !key->priv->predefined)
    _g_win32_registry_key_reread_kernel (key, buf);
  else
    _g_win32_registry_key_reread_user (key, buf);
}

static gboolean
_g_win32_registry_key_update_path (GWin32RegistryKey *key)
{
  GWin32RegistryKeyPrivate tmp;
  gboolean changed;
  gint change_indicator;

  change_indicator = g_atomic_int_get (&key->priv->change_indicator);

  if (change_indicator == G_WIN32_KEY_UNCHANGED)
    return FALSE;

  tmp.absolute_path_w = NULL;
  _g_win32_registry_key_reread (key, &tmp);
  changed = FALSE;

  if (wcscmp (key->priv->absolute_path_w, tmp.absolute_path_w) == 0)
    g_free (tmp.absolute_path_w);
  else
    {
      g_free (key->priv->absolute_path_w);
      key->priv->absolute_path_w = tmp.absolute_path_w;
      changed = TRUE;
    }

  return changed;
}

/**
 * g_win32_registry_key_get_path:
 * @key: (in) (transfer none): a #GWin32RegistryKey
 *
 * Get full path to the key
 *
 * Returns: (transfer none): a full path to the key (in UTF-8),
 *     or %NULL if it can't be converted to UTF-8.
 *
 * Since: 2.46
 **/
const gchar *
g_win32_registry_key_get_path (GWin32RegistryKey *key)
{
  gint change_indicator;

  g_return_val_if_fail (G_IS_WIN32_REGISTRY_KEY (key), NULL);

  change_indicator = g_atomic_int_get (&key->priv->change_indicator);

  if (change_indicator == G_WIN32_KEY_CHANGED &&
      !(key->priv->update_flags & G_WIN32_REGISTRY_UPDATED_PATH))
    {
      _g_win32_registry_key_update_path (key);
      key->priv->update_flags |= G_WIN32_REGISTRY_UPDATED_PATH;
    }

  if (key->priv->absolute_path == NULL)
    {
      g_free (key->priv->absolute_path);
      key->priv->absolute_path =
          g_utf16_to_utf8 (key->priv->absolute_path_w, -1,
                           NULL, NULL, NULL);
    }

  return key->priv->absolute_path;
}

/**
 * g_win32_registry_key_get_path_w:
 * @key: (in) (transfer none): a #GWin32RegistryKey
 *
 * Get full path to the key
 *
 * Returns: (transfer none): a full path to the key (in UTF-16)
 *
 * Since: 2.46
 **/
const gunichar2 *
g_win32_registry_key_get_path_w (GWin32RegistryKey *key)
{
  gint change_indicator;

  g_return_val_if_fail (G_IS_WIN32_REGISTRY_KEY (key), NULL);

  change_indicator = g_atomic_int_get (&key->priv->change_indicator);

  if (change_indicator == G_WIN32_KEY_CHANGED)
    _g_win32_registry_key_update_path (key);

  return key->priv->absolute_path_w;
}

/**
 * g_win32_registry_key_get_value:
 * @key: (in) (transfer none): a #GWin32RegistryKey
 * @auto_expand: (in) %TRUE to automatically expand G_WIN32_REGISTRY_VALUE_EXPAND_STR
 *     to G_WIN32_REGISTRY_VALUE_STR.
 * @value_name: (in) (transfer none): name of the value to get (in UTF-8).
 *   Empty string means the '(Default)' value.
 * @value_type: (out) (optional): type of the value retrieved.
 * @value_data: (out callee-allocates) (optional): contents of the value.
 * @value_data_size: (out) (optional): size of the buffer pointed
 *   by @value_data.
 * @error: (nullable): a pointer to %NULL #GError, or %NULL
 *
 * Get data from a value of a key. String data is guaranteed to be
 * appropriately terminated and will be in UTF-8.
 *
 * Returns: %TRUE on success, %FALSE on failure.
 *
 * Since: 2.46
 **/
gboolean
g_win32_registry_key_get_value (GWin32RegistryKey        *key,
                                gboolean                  auto_expand,
                                const gchar              *value_name,
                                GWin32RegistryValueType  *value_type,
                                gpointer                 *value_data,
                                gsize                    *value_data_size,
                                GError                  **error)
{
  GWin32RegistryValueType value_type_g;
  gpointer value_data_w;
  gsize value_data_w_size;
  gunichar2 *value_name_w;
  gchar *value_data_u8;
  gsize value_data_u8_len;
  gboolean result;

  g_return_val_if_fail (G_IS_WIN32_REGISTRY_KEY (key), FALSE);
  g_return_val_if_fail (value_name != NULL, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  /* No sense calling this function with all of these set to NULL */
  g_return_val_if_fail (value_type != NULL ||
                        value_data != NULL ||
                        value_data_size != NULL, FALSE);

  value_name_w = g_utf8_to_utf16 (value_name, -1, NULL, NULL, error);

  if (value_name_w == NULL)
    return FALSE;

  result = g_win32_registry_key_get_value_w (key,
                                             auto_expand,
                                             value_name_w,
                                             &value_type_g,
                                             &value_data_w,
                                             &value_data_w_size,
                                             error);

  g_free (value_name_w);

  if (!result)
    return FALSE;

  if (value_type_g == G_WIN32_REGISTRY_VALUE_EXPAND_STR ||
      value_type_g == G_WIN32_REGISTRY_VALUE_LINK ||
      value_type_g == G_WIN32_REGISTRY_VALUE_STR ||
      value_type_g == G_WIN32_REGISTRY_VALUE_MULTI_STR)
    {
      value_data_u8 = g_convert ((const gchar *) value_data_w,
                                 value_data_w_size - sizeof (gunichar2) /* excl. 0 */,
                                 "UTF8",
                                 "UTF16",
                                 NULL,
                                 &value_data_u8_len,
                                 error);
      g_free (value_data_w);

      if (value_data_u8 == NULL)
        return FALSE;

      if (value_data)
        *value_data = value_data_u8;
      else
        g_free (value_data_u8);

      if (value_data_size)
        *value_data_size = value_data_u8_len + 1;
    }
  else
    {
      if (value_data)
        *value_data = value_data_w;
      else
        g_free (value_data_w);

      if (value_data_size)
        *value_data_size = value_data_w_size;
    }

  if (value_type)
    *value_type = value_type_g;

  return TRUE;
}

/**
 * g_win32_registry_key_get_value_w:
 * @key: (in) (transfer none): a #GWin32RegistryKey
 * @auto_expand: (in) %TRUE to automatically expand G_WIN32_REGISTRY_VALUE_EXPAND_STR
 *     to G_WIN32_REGISTRY_VALUE_STR.
 * @value_name: (in) (transfer none): name of the value to get (in UTF-16).
 *   Empty string means the '(Default)' value.
 * @value_type: (out) (optional): type of the value retrieved.
 * @value_data: (out callee-allocates) (optional): contents of the value.
 * @value_data_size: (out) (optional): size of the buffer pointed
 *   by @value_data.
 * @error: (nullable): a pointer to %NULL #GError, or %NULL
 *
 * Get data from a value of a key.
 *
 * Get data from a value of a key. String data is guaranteed to be
 * appropriately terminated and will be in UTF-16.
 *
 * When calling with value_data == NULL (to get data size without getting
 * the data itself) remember that returned size corresponds to possibly
 * unterminated string data (if value is some kind of string), because
 * termination cannot be checked and fixed unless the data is retreived
 * too.
 *
 * Returns: %TRUE on success, %FALSE on failure.
 *
 * Since: 2.46
 **/
gboolean
g_win32_registry_key_get_value_w (GWin32RegistryKey        *key,
                                  gboolean                  auto_expand,
                                  const gunichar2          *value_name,
                                  GWin32RegistryValueType  *value_type,
                                  gpointer                 *value_data,
                                  gsize                    *value_data_size,
                                  GError                  **error)
{
  LONG status;
  DWORD value_type_w;
  DWORD value_type_w2;
  char *req_value_data;
  GWin32RegistryValueType value_type_g;
  GWin32RegistryValueType value_type_g2;
  DWORD req_value_data_size;
  DWORD req_value_data_size2;

  g_return_val_if_fail (G_IS_WIN32_REGISTRY_KEY (key), FALSE);
  g_return_val_if_fail (value_name != NULL, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  /* No sense calling this functions with all of these set to NULL */
  g_return_val_if_fail (value_type != NULL ||
                        value_data != NULL ||
                        value_data_size != NULL, FALSE);

  req_value_data_size = 0;
  status = RegQueryValueExW (key->priv->handle,
                             value_name,
                             NULL,
                             &value_type_w,
                             NULL,
                             &req_value_data_size);

  if (status != ERROR_MORE_DATA && status != ERROR_SUCCESS)
    {
      g_set_error (error, G_IO_ERROR, g_io_error_from_win32_error (status),
                   "Failed to query value '%S' for key '%S'",
                   value_name, g_win32_registry_key_get_path_w (key));

      return FALSE;
    }

  value_type_g = _g_win32_registry_type_w_to_g (value_type_w);

  if (value_data == NULL &&
      (!auto_expand || value_type_g != G_WIN32_REGISTRY_VALUE_EXPAND_STR))
    {
      if (value_type)
        *value_type = value_type_g;

      if (value_data_size)
        *value_data_size = req_value_data_size;

      return TRUE;
    }

  req_value_data = g_malloc (req_value_data_size + sizeof (gunichar2) * 2);
  req_value_data_size2 = req_value_data_size;
  status = RegQueryValueExW (key->priv->handle,
                             value_name,
                             NULL,
                             &value_type_w2,
                             (gpointer) req_value_data,
                             &req_value_data_size2);

  if (status != ERROR_SUCCESS)
    {
      g_set_error (error, G_IO_ERROR, g_io_error_from_win32_error (status),
                   "Failed to query value '%S' of size %lu for key '%S'",
                   value_name,
                   req_value_data_size,
                   g_win32_registry_key_get_path_w (key));
      g_free (req_value_data);
      return FALSE;
    }

  value_type_g2 = _g_win32_registry_type_w_to_g (value_type_w2);

  if (value_type_w != value_type_w2)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Type of value '%S' of key '%S' changed from %u to %u"
                   " between calls",
                   value_name,
                   g_win32_registry_key_get_path_w (key),
                   value_type_g, value_type_g2);
      g_free (req_value_data);
      return FALSE;
    }

  req_value_data_size = ensure_nul_termination (value_type_g,
                                                (guint8 *) req_value_data,
                                                req_value_data_size2);

  if (value_type_g == G_WIN32_REGISTRY_VALUE_EXPAND_STR && auto_expand)
    {
      gsize value_data_expanded_charsize_w = 0;
      gunichar2 *value_data_expanded = NULL;

      if (!expand_value ((gunichar2 *) req_value_data,
                         value_name,
                         (gpointer *) &value_data_expanded,
                         &value_data_expanded_charsize_w,
                         error))
        return FALSE;

      g_free (req_value_data);

      if (value_type)
        *value_type = G_WIN32_REGISTRY_VALUE_STR;

      if (value_data)
        *value_data = value_data_expanded;
      else
        g_free (value_data_expanded);

      if (value_data_size)
        *value_data_size = value_data_expanded_charsize_w * sizeof (gunichar2);

      return TRUE;
    }

  if (value_type)
    *value_type = value_type_g;

  if (value_data_size)
    *value_data_size = req_value_data_size;

  if (value_data)
    *value_data = req_value_data;
  else
    g_free (req_value_data);

  return TRUE;
}

static VOID NTAPI
key_changed (PVOID            closure,
             PIO_STATUS_BLOCK status_block,
             ULONG            reserved)
{
  GWin32RegistryKey *key = G_WIN32_REGISTRY_KEY (closure);

  g_free (status_block);
  g_atomic_int_set (&key->priv->change_indicator, G_WIN32_KEY_CHANGED);
  g_atomic_int_set (&key->priv->watch_indicator, G_WIN32_KEY_UNWATCHED);
  key->priv->update_flags = G_WIN32_REGISTRY_UPDATED_NOTHING;

  if (key->priv->callback)
    key->priv->callback (key, key->priv->user_data);

  key->priv->callback = NULL;
  key->priv->user_data = NULL;
  g_object_unref (key);
}

/**
 * g_win32_registry_key_watch:
 * @key: (in) (transfer none): a #GWin32RegistryKey
 * @watch_children: (in) %TRUE also watch the children of the @key, %FALSE
 *     to watch the key only.
 * @watch_flags: (in): specifies the types of changes to watch for.
 * @callback: (in) (nullable): a function to invoke when a change occurs.
 * @user_data: (in) (nullable): a pointer to pass to @callback on invocation.
 * @error: (nullable): a pointer to %NULL #GError, or %NULL
 *
 * Puts @key under a watch.
 *
 * When the key changes, an APC will be queued in the current thread. The APC
 * will run when the current thread enters alertable state (GLib main loop
 * should do that; if you are not using it, see MSDN documentation for W32API
 * calls that put thread into alertable state). When it runs, it will
 * atomically switch an indicator in the @key. If a callback was specified,
 * it is invoked at that point. Subsequent calls to
 * g_win32_registry_key_has_changed() will return %TRUE, and the callback (if
 * it was specified) will not be invoked anymore.
 * Calling g_win32_registry_key_erase_change_indicator() will reset the indicator,
 * and g_win32_registry_key_has_changed() will start returning %FALSE.
 * To resume the watch, call g_win32_registry_key_watch_for_changes() again.
 *
 * Calling g_win32_registry_key_watch_for_changes() for a key that is already
 * being watched is allowed and affects nothing.
 *
 * The fact that the key is being watched will be used internally to update
 * key path (if it changes).
 *
 * Returns: %TRUE on success, %FALSE on failure.
 *
 * Since: 2.46
 **/
gboolean
g_win32_registry_key_watch (GWin32RegistryKey                   *key,
                            gboolean                             watch_children,
                            GWin32RegistryKeyWatcherFlags        watch_flags,
                            GWin32RegistryKeyWatchCallbackFunc   callback,
                            gpointer                             user_data,
                            GError                             **error)
{
  ULONG filter;
  gboolean started_to_watch;
  NTSTATUS status;
  PIO_STATUS_BLOCK status_block;

  g_return_val_if_fail (G_IS_WIN32_REGISTRY_KEY (key), FALSE);

  filter = ((watch_flags & G_WIN32_REGISTRY_WATCH_NAME)       ? REG_NOTIFY_CHANGE_NAME       : 0) |
           ((watch_flags & G_WIN32_REGISTRY_WATCH_ATTRIBUTES) ? REG_NOTIFY_CHANGE_ATTRIBUTES : 0) |
           ((watch_flags & G_WIN32_REGISTRY_WATCH_VALUES)     ? REG_NOTIFY_CHANGE_LAST_SET   : 0) |
           ((watch_flags & G_WIN32_REGISTRY_WATCH_SECURITY)   ? REG_NOTIFY_CHANGE_SECURITY   : 0);

  if (filter == 0)
    {
      g_critical ("No supported flags specified in watch_flags (%x)", (guint) watch_flags);
      return FALSE;
    }

  if (g_once_init_enter (&nt_notify_change_multiple_keys))
  {
    NtNotifyChangeMultipleKeysFunc func;
    HMODULE ntdll = GetModuleHandle ("ntdll.dll");

    if (ntdll != NULL)
      func = (NtNotifyChangeMultipleKeysFunc) GetProcAddress (ntdll, "NtNotifyChangeMultipleKeys");
    else
      func = NULL;

    g_once_init_leave (&nt_notify_change_multiple_keys, func);
  }

  if (nt_notify_change_multiple_keys== NULL)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                   "Couldn't get NtNotifyChangeMultipleKeys() from ntdll");
      return FALSE;
    }

  started_to_watch =
      g_atomic_int_compare_and_exchange (&key->priv->watch_indicator,
                                         G_WIN32_KEY_UNWATCHED,
                                         G_WIN32_KEY_WATCHED);

  if (!started_to_watch)
    return TRUE;

  key->priv->callback = callback;
  key->priv->user_data = user_data;

  g_atomic_int_set (&key->priv->change_indicator, G_WIN32_KEY_UNCHANGED);

  /* Keep it alive until APC is called */
  g_object_ref (key);

  status_block = g_malloc (sizeof (IO_STATUS_BLOCK));

  status = nt_notify_change_multiple_keys (key->priv->handle,
                                           0,
                                           NULL,
                                           NULL,
                                           key_changed,
                                           (PVOID) key,
                                           status_block,
                                           filter,
                                           watch_children,
                                           NULL,
                                           0,
                                           TRUE);

  g_assert (status != STATUS_SUCCESS);

  if (status == STATUS_PENDING)
    return TRUE;

  g_atomic_int_set (&key->priv->change_indicator, G_WIN32_KEY_UNKNOWN);
  g_atomic_int_set (&key->priv->watch_indicator, G_WIN32_KEY_UNWATCHED);
  g_object_unref (key);
  g_free (status_block);

  return FALSE;
}

/**
 * g_win32_registry_key_erase_change_indicator:
 * @key: (in) (transfer none): a #GWin32RegistryKey
 *
 * Erases change indicator of the @key.
 *
 * Subsequent calls to g_win32_registry_key_has_changed() will return %FALSE
 * until the key is put on watch again by calling
 * g_win32_registry_key_watch() again.
 *
 * Since: 2.46
 */
void
g_win32_registry_key_erase_change_indicator (GWin32RegistryKey *key)
{
  g_return_if_fail (G_IS_WIN32_REGISTRY_KEY (key));

  g_atomic_int_set (&key->priv->change_indicator, G_WIN32_KEY_UNKNOWN);
}

/**
 * g_win32_registry_key_has_changed:
 * @key: (in) (transfer none): a #GWin32RegistryKey
 *
 * Check the @key's status indicator.
 *
 * Returns: %TRUE if the @key was put under watch at some point and has changed
 * since then, %FALSE if it either wasn't changed or wasn't watched at all.
 *
 * Since: 2.46
 */
gboolean
g_win32_registry_key_has_changed (GWin32RegistryKey *key)
{
  gint changed;

  g_return_val_if_fail (G_IS_WIN32_REGISTRY_KEY (key), FALSE);

  changed = g_atomic_int_get (&key->priv->change_indicator);

  return (changed == G_WIN32_KEY_CHANGED ? TRUE : FALSE);
}

static void
g_win32_registry_key_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  GWin32RegistryKey *key = G_WIN32_REGISTRY_KEY (object);

  switch (prop_id)
    {
      case PROP_PATH:
        g_value_set_string (value, g_win32_registry_key_get_path (key));
        break;

      case PROP_PATH_UTF16:
        g_value_set_pointer (value, (gpointer) g_win32_registry_key_get_path_w (key));
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
g_win32_registry_key_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  GWin32RegistryKey *key = G_WIN32_REGISTRY_KEY (object);
  GWin32RegistryKeyPrivate *priv = key->priv;
  const gchar *path;
  gunichar2 *path_w;

  switch (prop_id)
    {
    case PROP_PATH:
      g_assert (priv->absolute_path_w == NULL);
      g_assert (priv->absolute_path == NULL);
      path = g_value_get_string (value);

      if (path == NULL)
        break;

      path_w = g_utf8_to_utf16 (path, -1, NULL, NULL, NULL);

      if (path_w == NULL)
        break;

      g_free (priv->absolute_path_w);
      g_free (priv->absolute_path);
      priv->absolute_path_w = path_w;
      priv->absolute_path = g_value_dup_string (value);
      break;

    case PROP_PATH_UTF16:
      g_assert (priv->absolute_path_w == NULL);
      g_assert (priv->absolute_path == NULL);
      path_w = (gunichar2 *) g_value_get_pointer (value);

      if (path_w == NULL)
        break;

      priv->absolute_path_w = g_wcsdup (path_w, -1);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
g_win32_registry_key_class_init (GWin32RegistryKeyClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = g_win32_registry_key_dispose;
  gobject_class->set_property = g_win32_registry_key_set_property;
  gobject_class->get_property = g_win32_registry_key_get_property;

  /**
   * GWin32RegistryKey:path:
   *
   * A path to the key in the registry, in UTF-8.
   *
   * Since: 2.46
   */
  g_object_class_install_property (gobject_class,
                                   PROP_PATH,
                                   g_param_spec_string ("path",
                                                        "Path",
                                                        "Path to the key in the registry",
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));

  /**
   * GWin32RegistryKey:path-utf16:
   *
   * A path to the key in the registry, in UTF-16.
   *
   * Since: 2.46
   */
  g_object_class_install_property (gobject_class,
                                   PROP_PATH_UTF16,
                                   g_param_spec_pointer ("path-utf16",
                                                        "Path (UTF-16)",
                                                        "Path to the key in the registry, in UTF-16",
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));
}

static void
g_win32_registry_key_init (GWin32RegistryKey *key)
{
  key->priv = g_win32_registry_key_get_instance_private (key);
  key->priv->change_indicator = G_WIN32_KEY_UNKNOWN;
}
