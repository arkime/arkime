/*
 * Copyright © 2010 Novell, Inc.
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
 * Author: Vincent Untz <vuntz@gnome.org>
 */

#ifndef __G_SETTINGS_MAPPING_H__
#define __G_SETTINGS_MAPPING_H__

#include <glib-object.h>

GVariant *              g_settings_set_mapping                          (const GValue       *value,
                                                                         const GVariantType *expected_type,
                                                                         gpointer            user_data);
gboolean                g_settings_get_mapping                          (GValue             *value,
                                                                         GVariant           *variant,
                                                                         gpointer            user_data);
gboolean                g_settings_mapping_is_compatible                (GType               gvalue_type,
                                                                         const GVariantType *variant_type);

#endif /* __G_SETTINGS_MAPPING_H__ */
