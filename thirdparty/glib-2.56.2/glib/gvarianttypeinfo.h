/*
 * Copyright © 2008 Ryan Lortie
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

#ifndef __G_VARIANT_TYPE_INFO_H__
#define __G_VARIANT_TYPE_INFO_H__

#include <glib/gvarianttype.h>

#define G_VARIANT_TYPE_INFO_CHAR_MAYBE      'm'
#define G_VARIANT_TYPE_INFO_CHAR_ARRAY      'a'
#define G_VARIANT_TYPE_INFO_CHAR_TUPLE      '('
#define G_VARIANT_TYPE_INFO_CHAR_DICT_ENTRY '{'
#define G_VARIANT_TYPE_INFO_CHAR_VARIANT    'v'
#define g_variant_type_info_get_type_char(info) \
  (g_variant_type_info_get_type_string(info)[0])

typedef struct _GVariantTypeInfo GVariantTypeInfo;

/* < private >
 * GVariantMemberInfo:
 *
 * This structure describes how to construct a GVariant instance
 * corresponding to a given child of a tuple or dictionary entry in a
 * very short constant time.  It contains the typeinfo of the child,
 * along with 4 constants that allow the bounds of the child's
 * serialised data within the container's serialised data to be found
 * very efficiently.
 *
 * Since dictionary entries are serialised as if they were tuples of 2
 * items, the term "tuple" will be used here in the general sense to
 * refer to tuples and dictionary entries.
 *
 * BACKGROUND:
 *   The serialised data for a tuple contains an array of "offsets" at
 *   the end.  There is one "offset" in this array for each
 *   variable-sized item in the tuple (except for the last one).  The
 *   offset points to the end point of that item's serialised data.  The
 *   procedure for finding the start point is described below.  An
 *   offset is not needed for the last item because the end point of the
 *   last item is merely the end point of the container itself (after
 *   the offsets array has been accounted for).  An offset is not needed
 *   for fixed-sized items (like integers) because, due to their fixed
 *   size, the end point is a constant addition to the start point.
 *
 *   It is clear that the starting point of a given item in the tuple is
 *   determined by the items that precede it in the tuple.  Logically,
 *   the start point of a particular item in a given type of tuple can
 *   be determined entirely by the end point of the nearest
 *   variable-sized item that came before it (or from the start of the
 *   container itself in case there is no preceding variable-sized
 *   item).  In the case of "(isis)" for example, in order to find out
 *   the start point of the last string, one must start at the end point
 *   of the first string, align to 4 (for the integer's alignment) and
 *   then add 4 (for storing the integer).  That's the point where the
 *   string starts (since no special alignment is required for strings).
 *
 *   Of course, this process requires iterating over the types in the
 *   tuple up to the item of interest.  As it turns out, it is possible
 *   to determine 3 constants 'a', 'b', and 'c' for each item in the
 *   tuple, such that, given the ending offset of the nearest previous
 *   variable-sized item (prev_end), a very simple calculation can be
 *   performed to determine the start of the item of interest.
 *
 * The constants in this structure are used as follows:
 *
 * First, among the array of offets contained in the tuple, 'i' is the
 * index of the offset that refers to the end of the variable-sized item
 * preceding the item of interest.  If no variable-sized items precede
 * this item, then 'i' will be -1.
 *
 * Let 'prev_end' be the end offset of the previous item (or 0 in the
 * case that there was no such item).  The start address of this item
 * can then be calculate using 'a', 'b', and 'c':
 *
 *    item_start = ((prev_end + a) & b) | c;
 *
 * For details about how 'a', 'b' and 'c' are calculated, see the
 * comments at the point of the implementation in gvariantypeinfo.c.
 *
 * The end address of the item is then determined in one of three ways,
 * according to the 'end_type' field.
 *
 *   - FIXED: For fixed sized items, the end address is equal to the
 *     start address plus the fixed size.
 *
 *   - LAST: For the last variable sized item in the tuple, the end
 *     address is equal to the end address of the tuple, minus the size
 *     of the offset array.
 *
 *   - OFFSET: For other variable sized items, the next index past 'i'
 *     (ie: 'i + 1') must be consulted to find the end of this item.
 */

typedef struct
{
  GVariantTypeInfo *type_info;

  gsize i, a;
  gint8 b, c;

  guint8 ending_type;
} GVariantMemberInfo;

#define G_VARIANT_MEMBER_ENDING_FIXED   0
#define G_VARIANT_MEMBER_ENDING_LAST    1
#define G_VARIANT_MEMBER_ENDING_OFFSET  2

/* query */
GLIB_AVAILABLE_IN_ALL
const gchar *                   g_variant_type_info_get_type_string     (GVariantTypeInfo   *typeinfo);

GLIB_AVAILABLE_IN_ALL
void                            g_variant_type_info_query               (GVariantTypeInfo   *typeinfo,
                                                                         guint              *alignment,
                                                                         gsize              *size);

/* array */
GLIB_AVAILABLE_IN_ALL
GVariantTypeInfo *              g_variant_type_info_element             (GVariantTypeInfo   *typeinfo);
GLIB_AVAILABLE_IN_ALL
void                            g_variant_type_info_query_element       (GVariantTypeInfo   *typeinfo,
                                                                         guint              *alignment,
                                                                         gsize              *size);

/* structure */
GLIB_AVAILABLE_IN_ALL
gsize                           g_variant_type_info_n_members           (GVariantTypeInfo   *typeinfo);
GLIB_AVAILABLE_IN_ALL
const GVariantMemberInfo *      g_variant_type_info_member_info         (GVariantTypeInfo   *typeinfo,
                                                                         gsize               index);

/* new/ref/unref */
GLIB_AVAILABLE_IN_ALL
GVariantTypeInfo *              g_variant_type_info_get                 (const GVariantType *type);
GLIB_AVAILABLE_IN_ALL
GVariantTypeInfo *              g_variant_type_info_ref                 (GVariantTypeInfo   *typeinfo);
GLIB_AVAILABLE_IN_ALL
void                            g_variant_type_info_unref               (GVariantTypeInfo   *typeinfo);
GLIB_AVAILABLE_IN_ALL
void                            g_variant_type_info_assert_no_infos     (void);

#endif /* __G_VARIANT_TYPE_INFO_H__ */
