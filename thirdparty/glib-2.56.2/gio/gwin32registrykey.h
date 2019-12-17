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
#ifndef __G_WIN32_REGISTRY_KEY_H__
#define __G_WIN32_REGISTRY_KEY_H__

#include <gio/gio.h>

#ifdef G_PLATFORM_WIN32

G_BEGIN_DECLS

#define G_TYPE_WIN32_REGISTRY_KEY            (g_win32_registry_key_get_type ())
#define G_WIN32_REGISTRY_KEY(o)              (G_TYPE_CHECK_INSTANCE_CAST ((o), G_TYPE_WIN32_REGISTRY_KEY, GWin32RegistryKey))
#define G_WIN32_REGISTRY_KEY_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), G_TYPE_WIN32_REGISTRY_KEY, GWin32RegistryKeyClass))
#define G_IS_WIN32_REGISTRY_KEY(o)           (G_TYPE_CHECK_INSTANCE_TYPE ((o), G_TYPE_WIN32_REGISTRY_KEY))
#define G_IS_WIN32_REGISTRY_KEY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), G_TYPE_WIN32_REGISTRY_KEY))
#define G_WIN32_REGISTRY_KEY_GET_CLASS(o)    (G_TYPE_INSTANCE_GET_CLASS ((o), G_TYPE_WIN32_REGISTRY_KEY, GWin32RegistryKeyClass))

typedef enum {
  G_WIN32_REGISTRY_VALUE_NONE = 0,
  G_WIN32_REGISTRY_VALUE_BINARY = 1,
  G_WIN32_REGISTRY_VALUE_UINT32LE = 2,
  G_WIN32_REGISTRY_VALUE_UINT32BE = 3,
#if G_BYTE_ORDER == G_BIG_ENDIAN
  G_WIN32_REGISTRY_VALUE_UINT32 = G_WIN32_REGISTRY_VALUE_UINT32BE,
#else
  G_WIN32_REGISTRY_VALUE_UINT32 = G_WIN32_REGISTRY_VALUE_UINT32LE,
#endif
  G_WIN32_REGISTRY_VALUE_EXPAND_STR = 4,
  G_WIN32_REGISTRY_VALUE_LINK = 5,
  G_WIN32_REGISTRY_VALUE_MULTI_STR = 6,
  G_WIN32_REGISTRY_VALUE_UINT64LE = 7,
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
  G_WIN32_REGISTRY_VALUE_UINT64 = G_WIN32_REGISTRY_VALUE_UINT64LE,
#endif
  G_WIN32_REGISTRY_VALUE_STR = 8
} GWin32RegistryValueType;

typedef enum {
  G_WIN32_REGISTRY_WATCH_NAME = 1 << 0,
  G_WIN32_REGISTRY_WATCH_ATTRIBUTES = 1 << 1,
  G_WIN32_REGISTRY_WATCH_VALUES = 1 << 2,
  G_WIN32_REGISTRY_WATCH_SECURITY = 1 << 3,
} GWin32RegistryKeyWatcherFlags;

typedef struct _GWin32RegistryKey GWin32RegistryKey;
typedef struct _GWin32RegistryKeyClass GWin32RegistryKeyClass;
typedef struct _GWin32RegistryKeyPrivate GWin32RegistryKeyPrivate;
typedef struct _GWin32RegistrySubkeyIter GWin32RegistrySubkeyIter;
typedef struct _GWin32RegistryValueIter GWin32RegistryValueIter;

struct _GWin32RegistryKey {
  GObject parent_instance;

  /*< private >*/
  GWin32RegistryKeyPrivate *priv;
};

struct _GWin32RegistryKeyClass {
  GObjectClass parent_class;
};

/**
 * GWin32RegistryKeyWatchCallbackFunc:
 * @key: A #GWin32RegistryKey that was watched.
 * @user_data: The @user_data #gpointer passed to g_win32_registry_key_watch().
 *
 * The type of the callback passed to g_win32_registry_key_watch().
 *
 * The callback is invoked after a change matching the watch flags and arguments
 * occurs. If the children of the key were watched also, there is no way to know
 * which one of them triggered the callback.
 *
 * Since: 2.42
 */
typedef void (*GWin32RegistryKeyWatchCallbackFunc) (GWin32RegistryKey  *key,
                                                    gpointer            user_data);

#define G_TYPE_WIN32_REGISTRY_SUBKEY_ITER (g_win32_registry_subkey_iter_get_type ())

struct _GWin32RegistrySubkeyIter {
  /*< private >*/
  GWin32RegistryKey *key;
  gint               counter;
  gint               subkey_count;

  gunichar2         *subkey_name;
  gsize              subkey_name_size;
  gsize              subkey_name_len;

  gchar             *subkey_name_u8;
};

#define G_TYPE_WIN32_REGISTRY_VALUE_ITER (g_win32_registry_value_iter_get_type ())

struct _GWin32RegistryValueIter {
  /*< private >*/
  GWin32RegistryKey       *key;
  gint                     counter;
  gint                     value_count;

  gunichar2               *value_name;
  gsize                    value_name_size;
  gsize                    value_name_len;
  GWin32RegistryValueType  value_type;
  guint8                  *value_data;
  gsize                    value_data_size;
  gsize                    value_actual_data_size;
  GWin32RegistryValueType  value_expanded_type;
  gunichar2               *value_data_expanded;
  gsize                    value_data_expanded_charsize;

  gchar                   *value_name_u8;
  gsize                    value_name_u8_len;
  gchar                   *value_data_u8;
  gsize                    value_data_u8_size;
  gchar                   *value_data_expanded_u8;
  gsize                    value_data_expanded_u8_size;
};

GLIB_AVAILABLE_IN_2_46
GWin32RegistrySubkeyIter *g_win32_registry_subkey_iter_copy     (const GWin32RegistrySubkeyIter *iter);
GLIB_AVAILABLE_IN_2_46
void                      g_win32_registry_subkey_iter_free     (GWin32RegistrySubkeyIter       *iter);
GLIB_AVAILABLE_IN_2_46
void                      g_win32_registry_subkey_iter_assign   (GWin32RegistrySubkeyIter       *iter,
                                                                 const GWin32RegistrySubkeyIter *other);
GLIB_AVAILABLE_IN_2_46
GType                     g_win32_registry_subkey_iter_get_type (void) G_GNUC_CONST;


GLIB_AVAILABLE_IN_2_46
GWin32RegistryValueIter  *g_win32_registry_value_iter_copy      (const GWin32RegistryValueIter *iter);
GLIB_AVAILABLE_IN_2_46
void                      g_win32_registry_value_iter_free      (GWin32RegistryValueIter       *iter);
GLIB_AVAILABLE_IN_2_46
void                      g_win32_registry_value_iter_assign    (GWin32RegistryValueIter       *iter,
                                                                 const GWin32RegistryValueIter *other);
GLIB_AVAILABLE_IN_2_46
GType                     g_win32_registry_value_iter_get_type  (void) G_GNUC_CONST;


GLIB_AVAILABLE_IN_2_46
GType              g_win32_registry_key_get_type             (void);

GLIB_AVAILABLE_IN_2_46
GWin32RegistryKey *g_win32_registry_key_new                  (const gchar                    *path,
                                                              GError                        **error);

GLIB_AVAILABLE_IN_2_46
GWin32RegistryKey *g_win32_registry_key_new_w                (const gunichar2                *path,
                                                              GError                        **error);

GLIB_AVAILABLE_IN_2_46
GWin32RegistryKey *g_win32_registry_key_get_child            (GWin32RegistryKey              *key,
                                                              const gchar                    *subkey,
                                                              GError                        **error);

GLIB_AVAILABLE_IN_2_46
GWin32RegistryKey *g_win32_registry_key_get_child_w          (GWin32RegistryKey              *key,
                                                              const gunichar2                *subkey,
                                                              GError                        **error);

GLIB_AVAILABLE_IN_2_46
gboolean         g_win32_registry_subkey_iter_init           (GWin32RegistrySubkeyIter       *iter,
                                                              GWin32RegistryKey              *key,
                                                              GError                        **error);
GLIB_AVAILABLE_IN_2_46
void             g_win32_registry_subkey_iter_clear          (GWin32RegistrySubkeyIter       *iter);
GLIB_AVAILABLE_IN_2_46
gsize            g_win32_registry_subkey_iter_n_subkeys      (GWin32RegistrySubkeyIter       *iter);
GLIB_AVAILABLE_IN_2_46
gboolean         g_win32_registry_subkey_iter_next           (GWin32RegistrySubkeyIter       *iter,
                                                              gboolean                        skip_errors,
                                                              GError                        **error);
GLIB_AVAILABLE_IN_2_46
gboolean         g_win32_registry_subkey_iter_get_name       (GWin32RegistrySubkeyIter        *iter,
                                                              gchar                          **subkey_name,
                                                              gsize                           *subkey_name_len,
                                                              GError                         **error);
GLIB_AVAILABLE_IN_2_46
gboolean         g_win32_registry_subkey_iter_get_name_w     (GWin32RegistrySubkeyIter        *iter,
                                                              gunichar2                      **subkey_name,
                                                              gsize                           *subkey_name_len,
                                                              GError                         **error);

GLIB_AVAILABLE_IN_2_46
gboolean         g_win32_registry_value_iter_init            (GWin32RegistryValueIter         *iter,
                                                              GWin32RegistryKey               *key,
                                                              GError                         **error);
GLIB_AVAILABLE_IN_2_46
void             g_win32_registry_value_iter_clear           (GWin32RegistryValueIter         *iter);
GLIB_AVAILABLE_IN_2_46
gsize            g_win32_registry_value_iter_n_values        (GWin32RegistryValueIter         *iter);
GLIB_AVAILABLE_IN_2_46
gboolean         g_win32_registry_value_iter_next            (GWin32RegistryValueIter         *iter,
                                                              gboolean                         skip_errors,
                                                              GError                         **error);
GLIB_AVAILABLE_IN_2_46
gboolean         g_win32_registry_value_iter_get_value_type  (GWin32RegistryValueIter         *iter,
                                                              GWin32RegistryValueType         *value_type,
                                                              GError                         **error);
GLIB_AVAILABLE_IN_2_46
gboolean         g_win32_registry_value_iter_get_name        (GWin32RegistryValueIter         *iter,
                                                              gchar                          **value_name,
                                                              gsize                           *value_name_len,
                                                              GError                         **error);
GLIB_AVAILABLE_IN_2_46
gboolean         g_win32_registry_value_iter_get_name_w      (GWin32RegistryValueIter         *iter,
                                                              gunichar2                      **value_name,
                                                              gsize                           *value_name_len,
                                                              GError                         **error);
GLIB_AVAILABLE_IN_2_46
gboolean         g_win32_registry_value_iter_get_data        (GWin32RegistryValueIter         *iter,
                                                              gboolean                         auto_expand,
                                                              gpointer                        *value_data,
                                                              gsize                           *value_data_size,
                                                              GError                         **error);
GLIB_AVAILABLE_IN_2_46
gboolean         g_win32_registry_value_iter_get_data_w      (GWin32RegistryValueIter         *iter,
                                                              gboolean                         auto_expand,
                                                              gpointer                        *value_data,
                                                              gsize                           *value_data_size,
                                                              GError                         **error);

GLIB_AVAILABLE_IN_2_46
gboolean         g_win32_registry_key_get_value              (GWin32RegistryKey               *key,
                                                              gboolean                         auto_expand,
                                                              const gchar                     *value_name,
                                                              GWin32RegistryValueType         *value_type,
                                                              gpointer                        *value_data,
                                                              gsize                           *value_data_size,
                                                              GError                         **error);

GLIB_AVAILABLE_IN_2_46
gboolean         g_win32_registry_key_get_value_w            (GWin32RegistryKey               *key,
                                                              gboolean                         auto_expand,
                                                              const gunichar2                 *value_name,
                                                              GWin32RegistryValueType         *value_type,
                                                              gpointer                        *value_data,
                                                              gsize                           *value_data_size,
                                                              GError                         **error);

GLIB_AVAILABLE_IN_2_46
const gchar     *g_win32_registry_key_get_path               (GWin32RegistryKey               *key);

GLIB_AVAILABLE_IN_2_46
const gunichar2 *g_win32_registry_key_get_path_w             (GWin32RegistryKey               *key);

GLIB_AVAILABLE_IN_2_46
gboolean         g_win32_registry_key_watch                  (GWin32RegistryKey               *key,
                                                              gboolean                         watch_children,
                                                              GWin32RegistryKeyWatcherFlags    watch_flags,
                                                              GWin32RegistryKeyWatchCallbackFunc callback,
                                                              gpointer                         user_data,
                                                              GError                         **error);
GLIB_AVAILABLE_IN_2_46
gboolean         g_win32_registry_key_has_changed            (GWin32RegistryKey               *key);

GLIB_AVAILABLE_IN_2_46
void             g_win32_registry_key_erase_change_indicator (GWin32RegistryKey               *key);

G_END_DECLS

#endif /* G_PLATFORM_WIN32 */

#endif /* __G_WIN32_REGISTRY_KEY_H__ */
