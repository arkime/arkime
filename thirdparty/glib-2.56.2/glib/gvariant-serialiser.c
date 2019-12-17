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

/* Prologue {{{1 */
#include "config.h"

#include "gvariant-serialiser.h"

#include <glib/gtestutils.h>
#include <glib/gstrfuncs.h>
#include <glib/gtypes.h>

#include <string.h>


/* GVariantSerialiser
 *
 * After this prologue section, this file has roughly 2 parts.
 *
 * The first part is split up into sections according to various
 * container types.  Maybe, Array, Tuple, Variant.  The Maybe and Array
 * sections are subdivided for element types being fixed or
 * variable-sized types.
 *
 * Each section documents the format of that particular type of
 * container and implements 5 functions for dealing with it:
 *
 *  n_children:
 *    - determines (according to serialised data) how many child values
 *      are inside a particular container value.
 *
 *  get_child:
 *    - gets the type of and the serialised data corresponding to a
 *      given child value within the container value.
 *
 *  needed_size:
 *    - determines how much space would be required to serialise a
 *      container of this type, containing the given children so that
 *      buffers can be preallocated before serialising.
 *
 *  serialise:
 *    - write the serialised data for a container of this type,
 *      containing the given children, to a buffer.
 *
 *  is_normal:
 *    - check the given data to ensure that it is in normal form.  For a
 *      given set of child values, there is exactly one normal form for
 *      the serialised data of a container.  Other forms are possible
 *      while maintaining the same children (for example, by inserting
 *      something other than zero bytes as padding) but only one form is
 *      the normal form.
 *
 * The second part contains the main entry point for each of the above 5
 * functions and logic to dispatch it to the handler for the appropriate
 * container type code.
 *
 * The second part also contains a routine to byteswap serialised
 * values.  This code makes use of the n_children() and get_child()
 * functions above to do its work so no extra support is needed on a
 * per-container-type basis.
 *
 * There is also additional code for checking for normal form.  All
 * numeric types are always in normal form since the full range of
 * values is permitted (eg: 0 to 255 is a valid byte).  Special checks
 * need to be performed for booleans (only 0 or 1 allowed), strings
 * (properly nul-terminated) and object paths and signature strings
 * (meeting the D-Bus specification requirements).
 */

/* < private >
 * GVariantSerialised:
 * @type_info: the #GVariantTypeInfo of this value
 * @data: (nullable): the serialised data of this value, or %NULL
 * @size: the size of this value
 *
 * A structure representing a GVariant in serialised form.  This
 * structure is used with #GVariantSerialisedFiller functions and as the
 * primary interface to the serialiser.  See #GVariantSerialisedFiller
 * for a description of its use there.
 *
 * When used with the serialiser API functions, the following invariants
 * apply to all #GVariantTypeSerialised structures passed to and
 * returned from the serialiser.
 *
 * @type_info must be non-%NULL.
 *
 * @data must be properly aligned for the type described by @type_info.
 *
 * If @type_info describes a fixed-sized type then @size must always be
 * equal to the fixed size of that type.
 *
 * For fixed-sized types (and only fixed-sized types), @data may be
 * %NULL even if @size is non-zero.  This happens when a framing error
 * occurs while attempting to extract a fixed-sized value out of a
 * variable-sized container.  There is no data to return for the
 * fixed-sized type, yet @size must be non-zero.  The effect of this
 * combination should be as if @data were a pointer to an
 * appropriately-sized zero-filled region.
 */

/* < private >
 * g_variant_serialised_check:
 * @serialised: a #GVariantSerialised struct
 *
 * Checks @serialised for validity according to the invariants described
 * above.
 */
static void
g_variant_serialised_check (GVariantSerialised serialised)
{
  gsize fixed_size;
  guint alignment;

  g_assert (serialised.type_info != NULL);
  g_variant_type_info_query (serialised.type_info, &alignment, &fixed_size);

  if (fixed_size)
    g_assert_cmpint (serialised.size, ==, fixed_size);
  else
    g_assert (serialised.size == 0 || serialised.data != NULL);

  /* Depending on the native alignment requirements of the machine, the
   * compiler will insert either 3 or 7 padding bytes after the char.
   * This will result in the sizeof() the struct being 12 or 16.
   * Subtract 9 to get 3 or 7 which is a nice bitmask to apply to get
   * the alignment bits that we "care about" being zero: in the
   * 4-aligned case, we care about 2 bits, and in the 8-aligned case, we
   * care about 3 bits.
   */
  alignment &= sizeof (struct {
                         char a;
                         union {
                           guint64 x;
                           void *y;
                           gdouble z;
                         } b;
                       }
                      ) - 9;

  /* Some OSes (FreeBSD is a known example) have a malloc() that returns
   * unaligned memory if you request small sizes.  'malloc (1);', for
   * example, has been seen to return pointers aligned to 6 mod 16.
   *
   * Check if this is a small allocation and return without enforcing
   * the alignment assertion if this is the case.
   */
  if (serialised.size <= alignment)
    return;

  g_assert_cmpint (alignment & (gsize) serialised.data, ==, 0);
}

/* < private >
 * GVariantSerialisedFiller:
 * @serialised: a #GVariantSerialised instance to fill
 * @data: data from the children array
 *
 * This function is called back from g_variant_serialiser_needed_size()
 * and g_variant_serialiser_serialise().  It fills in missing details
 * from a partially-complete #GVariantSerialised.
 *
 * The @data parameter passed back to the function is one of the items
 * that was passed to the serialiser in the @children array.  It
 * represents a single child item of the container that is being
 * serialised.  The information filled in to @serialised is the
 * information for this child.
 *
 * If the @type_info field of @serialised is %NULL then the callback
 * function must set it to the type information corresponding to the
 * type of the child.  No reference should be added.  If it is non-%NULL
 * then the callback should assert that it is equal to the actual type
 * of the child.
 *
 * If the @size field is zero then the callback must fill it in with the
 * required amount of space to store the serialised form of the child.
 * If it is non-zero then the callback should assert that it is equal to
 * the needed size of the child.
 *
 * If @data is non-%NULL then it points to a space that is properly
 * aligned for and large enough to store the serialised data of the
 * child.  The callback must store the serialised form of the child at
 * @data.
 *
 * If the child value is another container then the callback will likely
 * recurse back into the serialiser by calling
 * g_variant_serialiser_needed_size() to determine @size and
 * g_variant_serialiser_serialise() to write to @data.
 */

/* PART 1: Container types {{{1
 *
 * This section contains the serialiser implementation functions for
 * each container type.
 */

/* Maybe {{{2
 *
 * Maybe types are handled depending on if the element type of the maybe
 * type is a fixed-sized or variable-sized type.  Although all maybe
 * types themselves are variable-sized types, herein, a maybe value with
 * a fixed-sized element type is called a "fixed-sized maybe" for
 * convenience and a maybe value with a variable-sized element type is
 * called a "variable-sized maybe".
 */

/* Fixed-sized Maybe {{{3
 *
 * The size of a maybe value with a fixed-sized element type is either 0
 * or equal to the fixed size of its element type.  The case where the
 * size of the maybe value is zero corresponds to the "Nothing" case and
 * the case where the size of the maybe value is equal to the fixed size
 * of the element type corresponds to the "Just" case; in that case, the
 * serialised data of the child value forms the entire serialised data
 * of the maybe value.
 *
 * In the event that a fixed-sized maybe value is presented with a size
 * that is not equal to the fixed size of the element type then the
 * value must be taken to be "Nothing".
 */

static gsize
gvs_fixed_sized_maybe_n_children (GVariantSerialised value)
{
  gsize element_fixed_size;

  g_variant_type_info_query_element (value.type_info, NULL,
                                     &element_fixed_size);

  return (element_fixed_size == value.size) ? 1 : 0;
}

static GVariantSerialised
gvs_fixed_sized_maybe_get_child (GVariantSerialised value,
                                 gsize              index_)
{
  /* the child has the same bounds as the
   * container, so just update the type.
   */
  value.type_info = g_variant_type_info_element (value.type_info);
  g_variant_type_info_ref (value.type_info);

  return value;
}

static gsize
gvs_fixed_sized_maybe_needed_size (GVariantTypeInfo         *type_info,
                                   GVariantSerialisedFiller  gvs_filler,
                                   const gpointer           *children,
                                   gsize                     n_children)
{
  if (n_children)
    {
      gsize element_fixed_size;

      g_variant_type_info_query_element (type_info, NULL,
                                         &element_fixed_size);

      return element_fixed_size;
    }
  else
    return 0;
}

static void
gvs_fixed_sized_maybe_serialise (GVariantSerialised        value,
                                 GVariantSerialisedFiller  gvs_filler,
                                 const gpointer           *children,
                                 gsize                     n_children)
{
  if (n_children)
    {
      GVariantSerialised child = { NULL, value.data, value.size };

      gvs_filler (&child, children[0]);
    }
}

static gboolean
gvs_fixed_sized_maybe_is_normal (GVariantSerialised value)
{
  if (value.size > 0)
    {
      gsize element_fixed_size;

      g_variant_type_info_query_element (value.type_info,
                                         NULL, &element_fixed_size);

      if (value.size != element_fixed_size)
        return FALSE;

      /* proper element size: "Just".  recurse to the child. */
      value.type_info = g_variant_type_info_element (value.type_info);

      return g_variant_serialised_is_normal (value);
    }

  /* size of 0: "Nothing" */
  return TRUE;
}

/* Variable-sized Maybe
 *
 * The size of a maybe value with a variable-sized element type is
 * either 0 or strictly greater than 0.  The case where the size of the
 * maybe value is zero corresponds to the "Nothing" case and the case
 * where the size of the maybe value is greater than zero corresponds to
 * the "Just" case; in that case, the serialised data of the child value
 * forms the first part of the serialised data of the maybe value and is
 * followed by a single zero byte.  This zero byte is always appended,
 * regardless of any zero bytes that may already be at the end of the
 * serialised ata of the child value.
 */

static gsize
gvs_variable_sized_maybe_n_children (GVariantSerialised value)
{
  return (value.size > 0) ? 1 : 0;
}

static GVariantSerialised
gvs_variable_sized_maybe_get_child (GVariantSerialised value,
                                    gsize              index_)
{
  /* remove the padding byte and update the type. */
  value.type_info = g_variant_type_info_element (value.type_info);
  g_variant_type_info_ref (value.type_info);
  value.size--;

  /* if it's zero-sized then it may as well be NULL */
  if (value.size == 0)
    value.data = NULL;

  return value;
}

static gsize
gvs_variable_sized_maybe_needed_size (GVariantTypeInfo         *type_info,
                                      GVariantSerialisedFiller  gvs_filler,
                                      const gpointer           *children,
                                      gsize                     n_children)
{
  if (n_children)
    {
      GVariantSerialised child = { 0, };

      gvs_filler (&child, children[0]);

      return child.size + 1;
    }
  else
    return 0;
}

static void
gvs_variable_sized_maybe_serialise (GVariantSerialised        value,
                                    GVariantSerialisedFiller  gvs_filler,
                                    const gpointer           *children,
                                    gsize                     n_children)
{
  if (n_children)
    {
      GVariantSerialised child = { NULL, value.data, value.size - 1 };

      /* write the data for the child.  */
      gvs_filler (&child, children[0]);
      value.data[child.size] = '\0';
    }
}

static gboolean
gvs_variable_sized_maybe_is_normal (GVariantSerialised value)
{
  if (value.size == 0)
    return TRUE;

  if (value.data[value.size - 1] != '\0')
    return FALSE;

  value.type_info = g_variant_type_info_element (value.type_info);
  value.size--;

  return g_variant_serialised_is_normal (value);
}

/* Arrays {{{2
 *
 * Just as with maybe types, array types are handled depending on if the
 * element type of the array type is a fixed-sized or variable-sized
 * type.  Similar to maybe types, for convenience, an array value with a
 * fixed-sized element type is called a "fixed-sized array" and an array
 * value with a variable-sized element type is called a "variable sized
 * array".
 */

/* Fixed-sized Array {{{3
 *
 * For fixed sized arrays, the serialised data is simply a concatenation
 * of the serialised data of each element, in order.  Since fixed-sized
 * values always have a fixed size that is a multiple of their alignment
 * requirement no extra padding is required.
 *
 * In the event that a fixed-sized array is presented with a size that
 * is not an integer multiple of the element size then the value of the
 * array must be taken as being empty.
 */

static gsize
gvs_fixed_sized_array_n_children (GVariantSerialised value)
{
  gsize element_fixed_size;

  g_variant_type_info_query_element (value.type_info, NULL,
                                     &element_fixed_size);

  if (value.size % element_fixed_size == 0)
    return value.size / element_fixed_size;

  return 0;
}

static GVariantSerialised
gvs_fixed_sized_array_get_child (GVariantSerialised value,
                                 gsize              index_)
{
  GVariantSerialised child = { 0, };

  child.type_info = g_variant_type_info_element (value.type_info);
  g_variant_type_info_query (child.type_info, NULL, &child.size);
  child.data = value.data + (child.size * index_);
  g_variant_type_info_ref (child.type_info);

  return child;
}

static gsize
gvs_fixed_sized_array_needed_size (GVariantTypeInfo         *type_info,
                                   GVariantSerialisedFiller  gvs_filler,
                                   const gpointer           *children,
                                   gsize                     n_children)
{
  gsize element_fixed_size;

  g_variant_type_info_query_element (type_info, NULL, &element_fixed_size);

  return element_fixed_size * n_children;
}

static void
gvs_fixed_sized_array_serialise (GVariantSerialised        value,
                                 GVariantSerialisedFiller  gvs_filler,
                                 const gpointer           *children,
                                 gsize                     n_children)
{
  GVariantSerialised child = { 0, };
  gsize i;

  child.type_info = g_variant_type_info_element (value.type_info);
  g_variant_type_info_query (child.type_info, NULL, &child.size);
  child.data = value.data;

  for (i = 0; i < n_children; i++)
    {
      gvs_filler (&child, children[i]);
      child.data += child.size;
    }
}

static gboolean
gvs_fixed_sized_array_is_normal (GVariantSerialised value)
{
  GVariantSerialised child = { 0, };

  child.type_info = g_variant_type_info_element (value.type_info);
  g_variant_type_info_query (child.type_info, NULL, &child.size);

  if (value.size % child.size != 0)
    return FALSE;

  for (child.data = value.data;
       child.data < value.data + value.size;
       child.data += child.size)
    {
      if (!g_variant_serialised_is_normal (child))
        return FALSE;
    }

  return TRUE;
}

/* Variable-sized Array {{{3
 *
 * Variable sized arrays, containing variable-sized elements, must be
 * able to determine the boundaries between the elements.  The items
 * cannot simply be concatenated.  Additionally, we are faced with the
 * fact that non-fixed-sized values do not necessarily have a size that
 * is a multiple of their alignment requirement, so we may need to
 * insert zero-filled padding.
 *
 * While it is possible to find the start of an item by starting from
 * the end of the item before it and padding for alignment, it is not
 * generally possible to do the reverse operation.  For this reason, we
 * record the end point of each element in the array.
 *
 * GVariant works in terms of "offsets".  An offset is a pointer to a
 * boundary between two bytes.  In 4 bytes of serialised data, there
 * would be 5 possible offsets: one at the start ('0'), one between each
 * pair of adjacent bytes ('1', '2', '3') and one at the end ('4').
 *
 * The numeric value of an offset is an unsigned integer given relative
 * to the start of the serialised data of the array.  Offsets are always
 * stored in little endian byte order and are always only as big as they
 * need to be.  For example, in 255 bytes of serialised data, there are
 * 256 offsets.  All possibilities can be stored in an 8 bit unsigned
 * integer.  In 256 bytes of serialised data, however, there are 257
 * possible offsets so 16 bit integers must be used.  The size of an
 * offset is always a power of 2.
 *
 * The offsets are stored at the end of the serialised data of the
 * array.  They are simply concatenated on without any particular
 * alignment.  The size of the offsets is included in the size of the
 * serialised data for purposes of determining the size of the offsets.
 * This presents a possibly ambiguity; in certain cases, a particular
 * value of array could have two different serialised forms.
 *
 * Imagine an array containing a single string of 253 bytes in length
 * (so, 254 bytes including the nul terminator).  Now the offset must be
 * written.  If an 8 bit offset is written, it will bring the size of
 * the array's serialised data to 255 -- which means that the use of an
 * 8 bit offset was valid.  If a 16 bit offset is used then the total
 * size of the array will be 256 -- which means that the use of a 16 bit
 * offset was valid.  Although both of these will be accepted by the
 * deserialiser, only the smaller of the two is considered to be in
 * normal form and that is the one that the serialiser must produce.
 */

/* bytes may be NULL if (size == 0). */
static inline gsize
gvs_read_unaligned_le (guchar *bytes,
                       guint   size)
{
  union
  {
    guchar bytes[GLIB_SIZEOF_SIZE_T];
    gsize integer;
  } tmpvalue;

  tmpvalue.integer = 0;
  if (bytes != NULL)
    memcpy (&tmpvalue.bytes, bytes, size);

  return GSIZE_FROM_LE (tmpvalue.integer);
}

static inline void
gvs_write_unaligned_le (guchar *bytes,
                        gsize   value,
                        guint   size)
{
  union
  {
    guchar bytes[GLIB_SIZEOF_SIZE_T];
    gsize integer;
  } tmpvalue;

  tmpvalue.integer = GSIZE_TO_LE (value);
  memcpy (bytes, &tmpvalue.bytes, size);
}

static guint
gvs_get_offset_size (gsize size)
{
  if (size > G_MAXUINT32)
    return 8;

  else if (size > G_MAXUINT16)
    return 4;

  else if (size > G_MAXUINT8)
    return 2;

  else if (size > 0)
    return 1;

  return 0;
}

static gsize
gvs_calculate_total_size (gsize body_size,
                          gsize offsets)
{
  if (body_size + 1 * offsets <= G_MAXUINT8)
    return body_size + 1 * offsets;

  if (body_size + 2 * offsets <= G_MAXUINT16)
    return body_size + 2 * offsets;

  if (body_size + 4 * offsets <= G_MAXUINT32)
    return body_size + 4 * offsets;

  return body_size + 8 * offsets;
}

static gsize
gvs_variable_sized_array_n_children (GVariantSerialised value)
{
  gsize offsets_array_size;
  gsize offset_size;
  gsize last_end;

  if (value.size == 0)
    return 0;

  offset_size = gvs_get_offset_size (value.size);

  last_end = gvs_read_unaligned_le (value.data + value.size -
                                    offset_size, offset_size);

  if (last_end > value.size)
    return 0;

  offsets_array_size = value.size - last_end;

  if (offsets_array_size % offset_size)
    return 0;

  return offsets_array_size / offset_size;
}

static GVariantSerialised
gvs_variable_sized_array_get_child (GVariantSerialised value,
                                    gsize              index_)
{
  GVariantSerialised child = { 0, };
  gsize offset_size;
  gsize last_end;
  gsize start;
  gsize end;

  child.type_info = g_variant_type_info_element (value.type_info);
  g_variant_type_info_ref (child.type_info);

  offset_size = gvs_get_offset_size (value.size);

  last_end = gvs_read_unaligned_le (value.data + value.size -
                                    offset_size, offset_size);

  if (index_ > 0)
    {
      guint alignment;

      start = gvs_read_unaligned_le (value.data + last_end +
                                     (offset_size * (index_ - 1)),
                                     offset_size);

      g_variant_type_info_query (child.type_info, &alignment, NULL);
      start += (-start) & alignment;
    }
  else
    start = 0;

  end = gvs_read_unaligned_le (value.data + last_end +
                               (offset_size * index_),
                               offset_size);

  if (start < end && end <= value.size)
    {
      child.data = value.data + start;
      child.size = end - start;
    }

  return child;
}

static gsize
gvs_variable_sized_array_needed_size (GVariantTypeInfo         *type_info,
                                      GVariantSerialisedFiller  gvs_filler,
                                      const gpointer           *children,
                                      gsize                     n_children)
{
  guint alignment;
  gsize offset;
  gsize i;

  g_variant_type_info_query (type_info, &alignment, NULL);
  offset = 0;

  for (i = 0; i < n_children; i++)
    {
      GVariantSerialised child = { 0, };

      offset += (-offset) & alignment;
      gvs_filler (&child, children[i]);
      offset += child.size;
    }

  return gvs_calculate_total_size (offset, n_children);
}

static void
gvs_variable_sized_array_serialise (GVariantSerialised        value,
                                    GVariantSerialisedFiller  gvs_filler,
                                    const gpointer           *children,
                                    gsize                     n_children)
{
  guchar *offset_ptr;
  gsize offset_size;
  guint alignment;
  gsize offset;
  gsize i;

  g_variant_type_info_query (value.type_info, &alignment, NULL);
  offset_size = gvs_get_offset_size (value.size);
  offset = 0;

  offset_ptr = value.data + value.size - offset_size * n_children;

  for (i = 0; i < n_children; i++)
    {
      GVariantSerialised child = { 0, };

      while (offset & alignment)
        value.data[offset++] = '\0';

      child.data = value.data + offset;
      gvs_filler (&child, children[i]);
      offset += child.size;

      gvs_write_unaligned_le (offset_ptr, offset, offset_size);
      offset_ptr += offset_size;
    }
}

static gboolean
gvs_variable_sized_array_is_normal (GVariantSerialised value)
{
  GVariantSerialised child = { 0, };
  gsize offsets_array_size;
  guchar *offsets_array;
  guint offset_size;
  guint alignment;
  gsize last_end;
  gsize length;
  gsize offset;
  gsize i;

  if (value.size == 0)
    return TRUE;

  offset_size = gvs_get_offset_size (value.size);
  last_end = gvs_read_unaligned_le (value.data + value.size -
                                    offset_size, offset_size);

  if (last_end > value.size)
    return FALSE;

  offsets_array_size = value.size - last_end;

  if (offsets_array_size % offset_size)
    return FALSE;

  offsets_array = value.data + value.size - offsets_array_size;
  length = offsets_array_size / offset_size;

  if (length == 0)
    return FALSE;

  child.type_info = g_variant_type_info_element (value.type_info);
  g_variant_type_info_query (child.type_info, &alignment, NULL);
  offset = 0;

  for (i = 0; i < length; i++)
    {
      gsize this_end;

      this_end = gvs_read_unaligned_le (offsets_array + offset_size * i,
                                        offset_size);

      if (this_end < offset || this_end > last_end)
        return FALSE;

      while (offset & alignment)
        {
          if (!(offset < this_end && value.data[offset] == '\0'))
            return FALSE;
          offset++;
        }

      child.data = value.data + offset;
      child.size = this_end - offset;

      if (child.size == 0)
        child.data = NULL;

      if (!g_variant_serialised_is_normal (child))
        return FALSE;

      offset = this_end;
    }

  g_assert (offset == last_end);

  return TRUE;
}

/* Tuples {{{2
 *
 * Since tuples can contain a mix of variable- and fixed-sized items,
 * they are, in terms of serialisation, a hybrid of variable-sized and
 * fixed-sized arrays.
 *
 * Offsets are only stored for variable-sized items.  Also, since the
 * number of items in a tuple is known from its type, we are able to
 * know exactly how many offsets to expect in the serialised data (and
 * therefore how much space is taken up by the offset array).  This
 * means that we know where the end of the serialised data for the last
 * item is -- we can just subtract the size of the offset array from the
 * total size of the tuple.  For this reason, the last item in the tuple
 * doesn't need an offset stored.
 *
 * Tuple offsets are stored in reverse.  This design choice allows
 * iterator-based deserialisers to be more efficient.
 *
 * Most of the "heavy lifting" here is handled by the GVariantTypeInfo
 * for the tuple.  See the notes in gvarianttypeinfo.h.
 */

static gsize
gvs_tuple_n_children (GVariantSerialised value)
{
  return g_variant_type_info_n_members (value.type_info);
}

static GVariantSerialised
gvs_tuple_get_child (GVariantSerialised value,
                     gsize              index_)
{
  const GVariantMemberInfo *member_info;
  GVariantSerialised child = { 0, };
  gsize offset_size;
  gsize start, end;

  member_info = g_variant_type_info_member_info (value.type_info, index_);
  child.type_info = g_variant_type_info_ref (member_info->type_info);
  offset_size = gvs_get_offset_size (value.size);

  /* tuples are the only (potentially) fixed-sized containers, so the
   * only ones that have to deal with the possibility of having %NULL
   * data with a non-zero %size if errors occurred elsewhere.
   */
  if G_UNLIKELY (value.data == NULL && value.size != 0)
    {
      g_variant_type_info_query (child.type_info, NULL, &child.size);

      /* this can only happen in fixed-sized tuples,
       * so the child must also be fixed sized.
       */
      g_assert (child.size != 0);
      child.data = NULL;

      return child;
    }

  if (member_info->ending_type == G_VARIANT_MEMBER_ENDING_OFFSET)
    {
      if (offset_size * (member_info->i + 2) > value.size)
        return child;
    }
  else
    {
      if (offset_size * (member_info->i + 1) > value.size)
        {
          /* if the child is fixed size, return its size.
           * if child is not fixed-sized, return size = 0.
           */
          g_variant_type_info_query (child.type_info, NULL, &child.size);

          return child;
        }
    }

  if (member_info->i + 1)
    start = gvs_read_unaligned_le (value.data + value.size -
                                   offset_size * (member_info->i + 1),
                                   offset_size);
  else
    start = 0;

  start += member_info->a;
  start &= member_info->b;
  start |= member_info->c;

  if (member_info->ending_type == G_VARIANT_MEMBER_ENDING_LAST)
    end = value.size - offset_size * (member_info->i + 1);

  else if (member_info->ending_type == G_VARIANT_MEMBER_ENDING_FIXED)
    {
      gsize fixed_size;

      g_variant_type_info_query (child.type_info, NULL, &fixed_size);
      end = start + fixed_size;
      child.size = fixed_size;
    }

  else /* G_VARIANT_MEMBER_ENDING_OFFSET */
    end = gvs_read_unaligned_le (value.data + value.size -
                                 offset_size * (member_info->i + 2),
                                 offset_size);

  if (start < end && end <= value.size)
    {
      child.data = value.data + start;
      child.size = end - start;
    }

  return child;
}

static gsize
gvs_tuple_needed_size (GVariantTypeInfo         *type_info,
                       GVariantSerialisedFiller  gvs_filler,
                       const gpointer           *children,
                       gsize                     n_children)
{
  const GVariantMemberInfo *member_info = NULL;
  gsize fixed_size;
  gsize offset;
  gsize i;

  g_variant_type_info_query (type_info, NULL, &fixed_size);

  if (fixed_size)
    return fixed_size;

  offset = 0;

  for (i = 0; i < n_children; i++)
    {
      guint alignment;

      member_info = g_variant_type_info_member_info (type_info, i);
      g_variant_type_info_query (member_info->type_info,
                                 &alignment, &fixed_size);
      offset += (-offset) & alignment;

      if (fixed_size)
        offset += fixed_size;
      else
        {
          GVariantSerialised child = { 0, };

          gvs_filler (&child, children[i]);
          offset += child.size;
        }
    }

  return gvs_calculate_total_size (offset, member_info->i + 1);
}

static void
gvs_tuple_serialise (GVariantSerialised        value,
                     GVariantSerialisedFiller  gvs_filler,
                     const gpointer           *children,
                     gsize                     n_children)
{
  gsize offset_size;
  gsize offset;
  gsize i;

  offset_size = gvs_get_offset_size (value.size);
  offset = 0;

  for (i = 0; i < n_children; i++)
    {
      const GVariantMemberInfo *member_info;
      GVariantSerialised child = { 0, };
      guint alignment;

      member_info = g_variant_type_info_member_info (value.type_info, i);
      g_variant_type_info_query (member_info->type_info, &alignment, NULL);

      while (offset & alignment)
        value.data[offset++] = '\0';

      child.data = value.data + offset;
      gvs_filler (&child, children[i]);
      offset += child.size;

      if (member_info->ending_type == G_VARIANT_MEMBER_ENDING_OFFSET)
        {
          value.size -= offset_size;
          gvs_write_unaligned_le (value.data + value.size,
                                  offset, offset_size);
        }
    }

  while (offset < value.size)
    value.data[offset++] = '\0';
}

static gboolean
gvs_tuple_is_normal (GVariantSerialised value)
{
  guint offset_size;
  gsize offset_ptr;
  gsize length;
  gsize offset;
  gsize i;

  /* as per the comment in gvs_tuple_get_child() */
  if G_UNLIKELY (value.data == NULL && value.size != 0)
    return FALSE;

  offset_size = gvs_get_offset_size (value.size);
  length = g_variant_type_info_n_members (value.type_info);
  offset_ptr = value.size;
  offset = 0;

  for (i = 0; i < length; i++)
    {
      const GVariantMemberInfo *member_info;
      GVariantSerialised child;
      gsize fixed_size;
      guint alignment;
      gsize end;

      member_info = g_variant_type_info_member_info (value.type_info, i);
      child.type_info = member_info->type_info;

      g_variant_type_info_query (child.type_info, &alignment, &fixed_size);

      while (offset & alignment)
        {
          if (offset > value.size || value.data[offset] != '\0')
            return FALSE;
          offset++;
        }

      child.data = value.data + offset;

      switch (member_info->ending_type)
        {
        case G_VARIANT_MEMBER_ENDING_FIXED:
          end = offset + fixed_size;
          break;

        case G_VARIANT_MEMBER_ENDING_LAST:
          end = offset_ptr;
          break;

        case G_VARIANT_MEMBER_ENDING_OFFSET:
          offset_ptr -= offset_size;

          if (offset_ptr < offset)
            return FALSE;

          end = gvs_read_unaligned_le (value.data + offset_ptr, offset_size);
          break;

        default:
          g_assert_not_reached ();
        }

      if (end < offset || end > offset_ptr)
        return FALSE;

      child.size = end - offset;

      if (child.size == 0)
        child.data = NULL;

      if (!g_variant_serialised_is_normal (child))
        return FALSE;

      offset = end;
    }

  {
    gsize fixed_size;
    guint alignment;

    g_variant_type_info_query (value.type_info, &alignment, &fixed_size);

    if (fixed_size)
      {
        g_assert (fixed_size == value.size);
        g_assert (offset_ptr == value.size);

        if (i == 0)
          {
            if (value.data[offset++] != '\0')
              return FALSE;
          }
        else
          {
            while (offset & alignment)
              if (value.data[offset++] != '\0')
                return FALSE;
          }

        g_assert (offset == value.size);
      }
  }

  return offset_ptr == offset;
}

/* Variants {{{2
 *
 * Variants are stored by storing the serialised data of the child,
 * followed by a '\0' character, followed by the type string of the
 * child.
 *
 * In the case that a value is presented that contains no '\0'
 * character, or doesn't have a single well-formed definite type string
 * following that character, the variant must be taken as containing the
 * unit tuple: ().
 */

static inline gsize
gvs_variant_n_children (GVariantSerialised value)
{
  return 1;
}

static inline GVariantSerialised
gvs_variant_get_child (GVariantSerialised value,
                       gsize              index_)
{
  GVariantSerialised child = { 0, };

  /* NOTE: not O(1) and impossible for it to be... */
  if (value.size)
    {
      /* find '\0' character */
      for (child.size = value.size - 1; child.size; child.size--)
        if (value.data[child.size] == '\0')
          break;

      /* ensure we didn't just hit the start of the string */
      if (value.data[child.size] == '\0')
        {
          const gchar *type_string = (gchar *) &value.data[child.size + 1];
          const gchar *limit = (gchar *) &value.data[value.size];
          const gchar *end;

          if (g_variant_type_string_scan (type_string, limit, &end) &&
              end == limit)
            {
              const GVariantType *type = (GVariantType *) type_string;

              if (g_variant_type_is_definite (type))
                {
                  gsize fixed_size;

                  child.type_info = g_variant_type_info_get (type);

                  if (child.size != 0)
                    /* only set to non-%NULL if size > 0 */
                    child.data = value.data;

                  g_variant_type_info_query (child.type_info,
                                             NULL, &fixed_size);

                  if (!fixed_size || fixed_size == child.size)
                    return child;

                  g_variant_type_info_unref (child.type_info);
                }
            }
        }
    }

  child.type_info = g_variant_type_info_get (G_VARIANT_TYPE_UNIT);
  child.data = NULL;
  child.size = 1;

  return child;
}

static inline gsize
gvs_variant_needed_size (GVariantTypeInfo         *type_info,
                         GVariantSerialisedFiller  gvs_filler,
                         const gpointer           *children,
                         gsize                     n_children)
{
  GVariantSerialised child = { 0, };
  const gchar *type_string;

  gvs_filler (&child, children[0]);
  type_string = g_variant_type_info_get_type_string (child.type_info);

  return child.size + 1 + strlen (type_string);
}

static inline void
gvs_variant_serialise (GVariantSerialised        value,
                       GVariantSerialisedFiller  gvs_filler,
                       const gpointer           *children,
                       gsize                     n_children)
{
  GVariantSerialised child = { 0, };
  const gchar *type_string;

  child.data = value.data;

  gvs_filler (&child, children[0]);
  type_string = g_variant_type_info_get_type_string (child.type_info);
  value.data[child.size] = '\0';
  memcpy (value.data + child.size + 1, type_string, strlen (type_string));
}

static inline gboolean
gvs_variant_is_normal (GVariantSerialised value)
{
  GVariantSerialised child;
  gboolean normal;

  child = gvs_variant_get_child (value, 0);

  normal = (child.data != NULL || child.size == 0) &&
           g_variant_serialised_is_normal (child);

  g_variant_type_info_unref (child.type_info);

  return normal;
}



/* PART 2: Serialiser API {{{1
 *
 * This is the implementation of the API of the serialiser as advertised
 * in gvariant-serialiser.h.
 */

/* Dispatch Utilities {{{2
 *
 * These macros allow a given function (for example,
 * g_variant_serialiser_serialise) to be dispatched to the appropriate
 * type-specific function above (fixed/variable-sized maybe,
 * fixed/variable-sized array, tuple or variant).
 */
#define DISPATCH_FIXED(type_info, before, after) \
  {                                                     \
    gsize fixed_size;                                   \
                                                        \
    g_variant_type_info_query_element (type_info, NULL, \
                                       &fixed_size);    \
                                                        \
    if (fixed_size)                                     \
      {                                                 \
        before ## fixed_sized ## after                  \
      }                                                 \
    else                                                \
      {                                                 \
        before ## variable_sized ## after               \
      }                                                 \
  }

#define DISPATCH_CASES(type_info, before, after) \
  switch (g_variant_type_info_get_type_char (type_info))        \
    {                                                           \
      case G_VARIANT_TYPE_INFO_CHAR_MAYBE:                      \
        DISPATCH_FIXED (type_info, before, _maybe ## after)     \
                                                                \
      case G_VARIANT_TYPE_INFO_CHAR_ARRAY:                      \
        DISPATCH_FIXED (type_info, before, _array ## after)     \
                                                                \
      case G_VARIANT_TYPE_INFO_CHAR_DICT_ENTRY:                 \
      case G_VARIANT_TYPE_INFO_CHAR_TUPLE:                      \
        {                                                       \
          before ## tuple ## after                              \
        }                                                       \
                                                                \
      case G_VARIANT_TYPE_INFO_CHAR_VARIANT:                    \
        {                                                       \
          before ## variant ## after                            \
        }                                                       \
    }

/* Serialiser entry points {{{2
 *
 * These are the functions that are called in order for the serialiser
 * to do its thing.
 */

/* < private >
 * g_variant_serialised_n_children:
 * @serialised: a #GVariantSerialised
 *
 * For serialised data that represents a container value (maybes,
 * tuples, arrays, variants), determine how many child items are inside
 * that container.
 *
 * Returns: the number of children
 */
gsize
g_variant_serialised_n_children (GVariantSerialised serialised)
{
  g_variant_serialised_check (serialised);

  DISPATCH_CASES (serialised.type_info,

                  return gvs_/**/,/**/_n_children (serialised);

                 )
  g_assert_not_reached ();
}

/* < private >
 * g_variant_serialised_get_child:
 * @serialised: a #GVariantSerialised
 * @index_: the index of the child to fetch
 *
 * Extracts a child from a serialised data representing a container
 * value.
 *
 * It is an error to call this function with an index out of bounds.
 *
 * If the result .data == %NULL and .size > 0 then there has been an
 * error extracting the requested fixed-sized value.  This number of
 * zero bytes needs to be allocated instead.
 *
 * In the case that .data == %NULL and .size == 0 then a zero-sized
 * item of a variable-sized type is being returned.
 *
 * .data is never non-%NULL if size is 0.
 *
 * Returns: a #GVariantSerialised for the child
 */
GVariantSerialised
g_variant_serialised_get_child (GVariantSerialised serialised,
                                gsize              index_)
{
  GVariantSerialised child;

  g_variant_serialised_check (serialised);

  if G_LIKELY (index_ < g_variant_serialised_n_children (serialised))
    {
      DISPATCH_CASES (serialised.type_info,

                      child = gvs_/**/,/**/_get_child (serialised, index_);
                      g_assert (child.size || child.data == NULL);
                      g_variant_serialised_check (child);
                      return child;

                     )
      g_assert_not_reached ();
    }

  g_error ("Attempt to access item %"G_GSIZE_FORMAT
           " in a container with only %"G_GSIZE_FORMAT" items",
           index_, g_variant_serialised_n_children (serialised));
}

/* < private >
 * g_variant_serialiser_serialise:
 * @serialised: a #GVariantSerialised, properly set up
 * @gvs_filler: the filler function
 * @children: an array of child items
 * @n_children: the size of @children
 *
 * Writes data in serialised form.
 *
 * The type_info field of @serialised must be filled in to type info for
 * the type that we are serialising.
 *
 * The size field of @serialised must be filled in with the value
 * returned by a previous call to g_variant_serialiser_needed_size().
 *
 * The data field of @serialised must be a pointer to a properly-aligned
 * memory region large enough to serialise into (ie: at least as big as
 * the size field).
 *
 * This function is only resonsible for serialising the top-level
 * container.  @gvs_filler is called on each child of the container in
 * order for all of the data of that child to be filled in.
 */
void
g_variant_serialiser_serialise (GVariantSerialised        serialised,
                                GVariantSerialisedFiller  gvs_filler,
                                const gpointer           *children,
                                gsize                     n_children)
{
  g_variant_serialised_check (serialised);

  DISPATCH_CASES (serialised.type_info,

                  gvs_/**/,/**/_serialise (serialised, gvs_filler,
                                           children, n_children);
                  return;

                 )
  g_assert_not_reached ();
}

/* < private >
 * g_variant_serialiser_needed_size:
 * @type_info: the type to serialise for
 * @gvs_filler: the filler function
 * @children: an array of child items
 * @n_children: the size of @children
 *
 * Determines how much memory would be needed to serialise this value.
 *
 * This function is only resonsible for performing calculations for the
 * top-level container.  @gvs_filler is called on each child of the
 * container in order to determine its size.
 */
gsize
g_variant_serialiser_needed_size (GVariantTypeInfo         *type_info,
                                  GVariantSerialisedFiller  gvs_filler,
                                  const gpointer           *children,
                                  gsize                     n_children)
{
  DISPATCH_CASES (type_info,

                  return gvs_/**/,/**/_needed_size (type_info, gvs_filler,
                                                    children, n_children);

                 )
  g_assert_not_reached ();
}

/* Byteswapping {{{2 */

/* < private >
 * g_variant_serialised_byteswap:
 * @value: a #GVariantSerialised
 *
 * Byte-swap serialised data.  The result of this function is only
 * well-defined if the data is in normal form.
 */
void
g_variant_serialised_byteswap (GVariantSerialised serialised)
{
  gsize fixed_size;
  guint alignment;

  g_variant_serialised_check (serialised);

  if (!serialised.data)
    return;

  /* the types we potentially need to byteswap are
   * exactly those with alignment requirements.
   */
  g_variant_type_info_query (serialised.type_info, &alignment, &fixed_size);
  if (!alignment)
    return;

  /* if fixed size and alignment are equal then we are down
   * to the base integer type and we should swap it.  the
   * only exception to this is if we have a tuple with a
   * single item, and then swapping it will be OK anyway.
   */
  if (alignment + 1 == fixed_size)
    {
      switch (fixed_size)
      {
        case 2:
          {
            guint16 *ptr = (guint16 *) serialised.data;

            g_assert_cmpint (serialised.size, ==, 2);
            *ptr = GUINT16_SWAP_LE_BE (*ptr);
          }
          return;

        case 4:
          {
            guint32 *ptr = (guint32 *) serialised.data;

            g_assert_cmpint (serialised.size, ==, 4);
            *ptr = GUINT32_SWAP_LE_BE (*ptr);
          }
          return;

        case 8:
          {
            guint64 *ptr = (guint64 *) serialised.data;

            g_assert_cmpint (serialised.size, ==, 8);
            *ptr = GUINT64_SWAP_LE_BE (*ptr);
          }
          return;

        default:
          g_assert_not_reached ();
      }
    }

  /* else, we have a container that potentially contains
   * some children that need to be byteswapped.
   */
  else
    {
      gsize children, i;

      children = g_variant_serialised_n_children (serialised);
      for (i = 0; i < children; i++)
        {
          GVariantSerialised child;

          child = g_variant_serialised_get_child (serialised, i);
          g_variant_serialised_byteswap (child);
          g_variant_type_info_unref (child.type_info);
        }
    }
}

/* Normal form checking {{{2 */

/* < private >
 * g_variant_serialised_is_normal:
 * @serialised: a #GVariantSerialised
 *
 * Determines, recursively if @serialised is in normal form.  There is
 * precisely one normal form of serialised data for each possible value.
 *
 * It is possible that multiple byte sequences form the serialised data
 * for a given value if, for example, the padding bytes are filled in
 * with something other than zeros, but only one form is the normal
 * form.
 */
gboolean
g_variant_serialised_is_normal (GVariantSerialised serialised)
{
  DISPATCH_CASES (serialised.type_info,

                  return gvs_/**/,/**/_is_normal (serialised);

                 )

  if (serialised.data == NULL)
    return FALSE;

  /* some hard-coded terminal cases */
  switch (g_variant_type_info_get_type_char (serialised.type_info))
    {
    case 'b': /* boolean */
      return serialised.data[0] < 2;

    case 's': /* string */
      return g_variant_serialiser_is_string (serialised.data,
                                             serialised.size);

    case 'o':
      return g_variant_serialiser_is_object_path (serialised.data,
                                                  serialised.size);

    case 'g':
      return g_variant_serialiser_is_signature (serialised.data,
                                                serialised.size);

    default:
      /* all of the other types are fixed-sized numerical types for
       * which all possible values are valid (including various NaN
       * representations for floating point values).
       */
      return TRUE;
    }
}

/* Validity-checking functions {{{2
 *
 * Checks if strings, object paths and signature strings are valid.
 */

/* < private >
 * g_variant_serialiser_is_string:
 * @data: a possible string
 * @size: the size of @data
 *
 * Ensures that @data is a valid string with a nul terminator at the end
 * and no nul bytes embedded.
 */
gboolean
g_variant_serialiser_is_string (gconstpointer data,
                                gsize         size)
{
  const gchar *expected_end;
  const gchar *end;

  if (size == 0)
    return FALSE;

  expected_end = ((gchar *) data) + size - 1;

  if (*expected_end != '\0')
    return FALSE;

  g_utf8_validate (data, size, &end);

  return end == expected_end;
}

/* < private >
 * g_variant_serialiser_is_object_path:
 * @data: a possible D-Bus object path
 * @size: the size of @data
 *
 * Performs the checks for being a valid string.
 *
 * Also, ensures that @data is a valid DBus object path, as per the D-Bus
 * specification.
 */
gboolean
g_variant_serialiser_is_object_path (gconstpointer data,
                                     gsize         size)
{
  const gchar *string = data;
  gsize i;

  if (!g_variant_serialiser_is_string (data, size))
    return FALSE;

  /* The path must begin with an ASCII '/' (integer 47) character */
  if (string[0] != '/')
    return FALSE;

  for (i = 1; string[i]; i++)
    /* Each element must only contain the ASCII characters
     * "[A-Z][a-z][0-9]_"
     */
    if (g_ascii_isalnum (string[i]) || string[i] == '_')
      ;

    /* must consist of elements separated by slash characters. */
    else if (string[i] == '/')
      {
        /* No element may be the empty string. */
        /* Multiple '/' characters cannot occur in sequence. */
        if (string[i - 1] == '/')
          return FALSE;
      }

    else
      return FALSE;

  /* A trailing '/' character is not allowed unless the path is the
   * root path (a single '/' character).
   */
  if (i > 1 && string[i - 1] == '/')
    return FALSE;

  return TRUE;
}

/* < private >
 * g_variant_serialiser_is_signature:
 * @data: a possible D-Bus signature
 * @size: the size of @data
 *
 * Performs the checks for being a valid string.
 *
 * Also, ensures that @data is a valid D-Bus type signature, as per the
 * D-Bus specification.
 */
gboolean
g_variant_serialiser_is_signature (gconstpointer data,
                                   gsize         size)
{
  const gchar *string = data;
  gsize first_invalid;

  if (!g_variant_serialiser_is_string (data, size))
    return FALSE;

  /* make sure no non-definite characters appear */
  first_invalid = strspn (string, "ybnqiuxthdvasog(){}");
  if (string[first_invalid])
    return FALSE;

  /* make sure each type string is well-formed */
  while (*string)
    if (!g_variant_type_string_scan (string, NULL, &string))
      return FALSE;

  return TRUE;
}

/* Epilogue {{{1 */
/* vim:set foldmethod=marker: */
