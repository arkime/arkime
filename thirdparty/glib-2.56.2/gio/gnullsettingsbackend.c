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

#include "config.h"

#include "gsettingsbackendinternal.h"
#include "giomodule.h"
#include "gsimplepermission.h"


#define G_TYPE_NULL_SETTINGS_BACKEND    (g_null_settings_backend_get_type ())
#define G_NULL_SETTINGS_BACKEND(inst)   (G_TYPE_CHECK_INSTANCE_CAST ((inst), \
                                         G_TYPE_NULL_SETTINGS_BACKEND,       \
                                         GNullSettingsBackend))


typedef GSettingsBackendClass GNullSettingsBackendClass;
typedef GSettingsBackend      GNullSettingsBackend;

G_DEFINE_TYPE_WITH_CODE (GNullSettingsBackend,
                         g_null_settings_backend,
                         G_TYPE_SETTINGS_BACKEND,
                         g_io_extension_point_implement (G_SETTINGS_BACKEND_EXTENSION_POINT_NAME,
                                                         g_define_type_id, "null", 10))

static GVariant *
g_null_settings_backend_read (GSettingsBackend   *backend,
                              const gchar        *key,
                              const GVariantType *expected_type,
                              gboolean            default_value)
{
  return NULL;
}

static gboolean
g_null_settings_backend_write (GSettingsBackend *backend,
                               const gchar      *key,
                               GVariant         *value,
                               gpointer          origin_tag)
{
  if (value)
    g_variant_unref (g_variant_ref_sink (value));
  return FALSE;
}

static gboolean
g_null_settings_backend_write_one (gpointer key,
                                   gpointer value,
                                   gpointer data)
{
  if (value)
    g_variant_unref (g_variant_ref_sink (value));
  return FALSE;
}

static gboolean
g_null_settings_backend_write_tree (GSettingsBackend *backend,
                                    GTree            *tree,
                                    gpointer          origin_tag)
{
  g_tree_foreach (tree, g_null_settings_backend_write_one, backend);
  return FALSE;
}

static void
g_null_settings_backend_reset (GSettingsBackend *backend,
                               const gchar      *key,
                               gpointer          origin_tag)
{
}

static gboolean
g_null_settings_backend_get_writable (GSettingsBackend *backend,
                                      const gchar      *name)
{
  return FALSE;
}

static GPermission *
g_null_settings_backend_get_permission (GSettingsBackend *backend,
                                        const gchar      *path)
{
  return g_simple_permission_new (FALSE);
}

static void
g_null_settings_backend_init (GNullSettingsBackend *memory)
{
}

static void
g_null_settings_backend_class_init (GNullSettingsBackendClass *class)
{
  GSettingsBackendClass *backend_class = G_SETTINGS_BACKEND_CLASS (class);

  backend_class->read = g_null_settings_backend_read;
  backend_class->write = g_null_settings_backend_write;
  backend_class->write_tree = g_null_settings_backend_write_tree;
  backend_class->reset = g_null_settings_backend_reset;
  backend_class->get_writable = g_null_settings_backend_get_writable;
  backend_class->get_permission = g_null_settings_backend_get_permission;
}

/**
 * g_null_settings_backend_new:
 *
 *
 * Creates a readonly #GSettingsBackend.
 *
 * This backend does not allow changes to settings, so all settings
 * will always have their default values.
 *
 * Returns: (transfer full): a newly created #GSettingsBackend
 *
 * Since: 2.28
 */
GSettingsBackend *
g_null_settings_backend_new (void)
{
  return g_object_new (G_TYPE_NULL_SETTINGS_BACKEND, NULL);
}
