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
 */

#ifndef __G_SETTINGS_SCHEMA_INTERNAL_H__
#define __G_SETTINGS_SCHEMA_INTERNAL_H__

#include "gsettingsschema.h"

struct _GSettingsSchemaKey
{
  GSettingsSchema *schema;
  const gchar *name;

  guint is_flags : 1;
  guint is_enum  : 1;

  const guint32 *strinfo;
  gsize strinfo_length;

  const gchar *unparsed;
  gchar lc_char;

  const GVariantType *type;
  GVariant *minimum, *maximum;
  GVariant *default_value;

  gint ref_count;
};

const gchar *           g_settings_schema_get_gettext_domain            (GSettingsSchema  *schema);
GVariantIter *          g_settings_schema_get_value                     (GSettingsSchema  *schema,
                                                                         const gchar      *key);
const GQuark *          g_settings_schema_list                          (GSettingsSchema  *schema,
                                                                         gint             *n_items);
const gchar *           g_settings_schema_get_string                    (GSettingsSchema  *schema,
                                                                         const gchar      *key);

void                    g_settings_schema_key_init                      (GSettingsSchemaKey *key,
                                                                         GSettingsSchema    *schema,
                                                                         const gchar        *name);
void                    g_settings_schema_key_clear                     (GSettingsSchemaKey *key);
gboolean                g_settings_schema_key_type_check                (GSettingsSchemaKey *key,
                                                                         GVariant           *value);
GVariant *              g_settings_schema_key_range_fixup               (GSettingsSchemaKey *key,
                                                                         GVariant           *value);
GVariant *              g_settings_schema_key_get_translated_default    (GSettingsSchemaKey *key);

gint                    g_settings_schema_key_to_enum                   (GSettingsSchemaKey *key,
                                                                         GVariant           *value);
GVariant *              g_settings_schema_key_from_enum                 (GSettingsSchemaKey *key,
                                                                         gint                value);
guint                   g_settings_schema_key_to_flags                  (GSettingsSchemaKey *key,
                                                                         GVariant           *value);
GVariant *              g_settings_schema_key_from_flags                (GSettingsSchemaKey *key,
                                                                         guint               value);

#endif /* __G_SETTINGS_SCHEMA_INTERNAL_H__ */
