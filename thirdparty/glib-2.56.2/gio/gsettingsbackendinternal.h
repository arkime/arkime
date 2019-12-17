/*
 * Copyright © 2009, 2010 Codethink Limited
 * Copyright © 2010 Red Hat, Inc.
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
 * Authors: Ryan Lortie <desrt@desrt.ca>
 *          Matthias Clasen <mclasen@redhat.com>
 */

#ifndef __G_SETTINGS_BACKEND_INTERNAL_H__
#define __G_SETTINGS_BACKEND_INTERNAL_H__

#include "gsettingsbackend.h"

typedef struct
{
  void (* changed)               (GObject             *target,
                                  GSettingsBackend    *backend,
                                  const gchar         *key,
                                  gpointer             origin_tag);
  void (* path_changed)          (GObject             *target,
                                  GSettingsBackend    *backend,
                                  const gchar         *path,
                                  gpointer             origin_tag);
  void (* keys_changed)          (GObject             *target,
                                  GSettingsBackend    *backend,
                                  const gchar         *prefix,
                                  gpointer             origin_tag,
                                  const gchar * const *names);
  void (* writable_changed)      (GObject             *target,
                                  GSettingsBackend    *backend,
                                  const gchar         *key);
  void (* path_writable_changed) (GObject             *target,
                                  GSettingsBackend    *backend,
                                  const gchar         *path);
} GSettingsListenerVTable;

void                    g_settings_backend_watch                        (GSettingsBackend               *backend,
                                                                         const GSettingsListenerVTable  *vtable,
                                                                         GObject                        *target,
                                                                         GMainContext                   *context);
void                    g_settings_backend_unwatch                      (GSettingsBackend               *backend,
                                                                         GObject                        *target);

GTree *                 g_settings_backend_create_tree                  (void);

GVariant *              g_settings_backend_read                         (GSettingsBackend               *backend,
                                                                         const gchar                    *key,
                                                                         const GVariantType             *expected_type,
                                                                         gboolean                        default_value);
GVariant *              g_settings_backend_read_user_value              (GSettingsBackend               *backend,
                                                                         const gchar                    *key,
                                                                         const GVariantType             *expected_type);
gboolean                g_settings_backend_write                        (GSettingsBackend               *backend,
                                                                         const gchar                    *key,
                                                                         GVariant                       *value,
                                                                         gpointer                        origin_tag);
gboolean                g_settings_backend_write_tree                   (GSettingsBackend               *backend,
                                                                         GTree                          *tree,
                                                                         gpointer                        origin_tag);
void                    g_settings_backend_reset                        (GSettingsBackend               *backend,
                                                                         const gchar                    *key,
                                                                         gpointer                        origin_tag);
gboolean                g_settings_backend_get_writable                 (GSettingsBackend               *backend,
                                                                         const char                     *key);
void                    g_settings_backend_unsubscribe                  (GSettingsBackend               *backend,
                                                                         const char                     *name);
void                    g_settings_backend_subscribe                    (GSettingsBackend               *backend,
                                                                         const char                     *name);
GPermission *           g_settings_backend_get_permission               (GSettingsBackend               *backend,
                                                                         const gchar                    *path);
void                    g_settings_backend_sync_default                 (void);

GType                   g_null_settings_backend_get_type                (void);

GType                   g_memory_settings_backend_get_type              (void);

#ifdef HAVE_COCOA
GType                   g_nextstep_settings_backend_get_type            (void);
#endif

#endif  /* __G_SETTINGS_BACKEND_INTERNAL_H__ */
