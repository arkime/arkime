/*
 * Copyright © 2007, 2008 Ryan Lortie
 * Copyright © 2009, 2010 Codethink Limited
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


/* The purpose of this header is to allow certain internal symbols of
 * GVariant to be put under test cases.
 */

#ifndef __G_VARIANT_INTERNAL_H__
#define __G_VARIANT_INTERNAL_H__

/* Hack */
#define __GLIB_H_INSIDE__

#include <glib/gvarianttype.h>
#include <glib/gtypes.h>

#include "gvariant-serialiser.h"
#include "gvarianttypeinfo.h"

#undef __GLIB_H_INSIDE__

GLIB_AVAILABLE_IN_ALL
gboolean                        g_variant_format_string_scan            (const gchar          *string,
                                                                         const gchar          *limit,
                                                                         const gchar         **endptr);

GLIB_AVAILABLE_IN_ALL
GVariantType *                  g_variant_format_string_scan_type       (const gchar          *string,
                                                                         const gchar          *limit,
                                                                         const gchar         **endptr);

#endif /* __G_VARIANT_INTERNAL_H__ */
