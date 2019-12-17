/*
 * Copyright © 2007, 2008 Ryan Lortie
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

#ifndef __G_VARIANT_SERIALISER_H__
#define __G_VARIANT_SERIALISER_H__

#include "gvarianttypeinfo.h"

typedef struct
{
  GVariantTypeInfo *type_info;
  guchar           *data;
  gsize             size;
} GVariantSerialised;

/* deserialisation */
GLIB_AVAILABLE_IN_ALL
gsize                           g_variant_serialised_n_children         (GVariantSerialised        container);
GLIB_AVAILABLE_IN_ALL
GVariantSerialised              g_variant_serialised_get_child          (GVariantSerialised        container,
                                                                         gsize                     index);

/* serialisation */
typedef void                  (*GVariantSerialisedFiller)               (GVariantSerialised       *serialised,
                                                                         gpointer                  data);

GLIB_AVAILABLE_IN_ALL
gsize                           g_variant_serialiser_needed_size        (GVariantTypeInfo         *info,
                                                                         GVariantSerialisedFiller  gsv_filler,
                                                                         const gpointer           *children,
                                                                         gsize                     n_children);

GLIB_AVAILABLE_IN_ALL
void                            g_variant_serialiser_serialise          (GVariantSerialised        container,
                                                                         GVariantSerialisedFiller  gsv_filler,
                                                                         const gpointer           *children,
                                                                         gsize                     n_children);

/* misc */
GLIB_AVAILABLE_IN_ALL
gboolean                        g_variant_serialised_is_normal          (GVariantSerialised        value);
GLIB_AVAILABLE_IN_ALL
void                            g_variant_serialised_byteswap           (GVariantSerialised        value);

/* validation of strings */
GLIB_AVAILABLE_IN_ALL
gboolean                        g_variant_serialiser_is_string          (gconstpointer             data,
                                                                         gsize                     size);
GLIB_AVAILABLE_IN_ALL
gboolean                        g_variant_serialiser_is_object_path     (gconstpointer             data,
                                                                         gsize                     size);
GLIB_AVAILABLE_IN_ALL
gboolean                        g_variant_serialiser_is_signature       (gconstpointer             data,
                                                                         gsize                     size);

#endif /* __G_VARIANT_SERIALISER_H__ */
