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

#include <glib/gvariant-serialiser.h>
#include "gvariant-internal.h"
#include <glib/gvariant-core.h>
#include <glib/gtestutils.h>
#include <glib/gstrfuncs.h>
#include <glib/gslice.h>
#include <glib/ghash.h>
#include <glib/gmem.h>

#include <string.h>


/**
 * SECTION:gvariant
 * @title: GVariant
 * @short_description: strongly typed value datatype
 * @see_also: GVariantType
 *
 * #GVariant is a variant datatype; it can contain one or more values
 * along with information about the type of the values.
 *
 * A #GVariant may contain simple types, like an integer, or a boolean value;
 * or complex types, like an array of two strings, or a dictionary of key
 * value pairs. A #GVariant is also immutable: once it's been created neither
 * its type nor its content can be modified further.
 *
 * GVariant is useful whenever data needs to be serialized, for example when
 * sending method parameters in DBus, or when saving settings using GSettings.
 *
 * When creating a new #GVariant, you pass the data you want to store in it
 * along with a string representing the type of data you wish to pass to it.
 *
 * For instance, if you want to create a #GVariant holding an integer value you
 * can use:
 *
 * |[<!-- language="C" -->
 *   GVariant *v = g_variant_new ("u", 40);
 * ]|
 *
 * The string "u" in the first argument tells #GVariant that the data passed to
 * the constructor (40) is going to be an unsigned integer.
 *
 * More advanced examples of #GVariant in use can be found in documentation for
 * [GVariant format strings][gvariant-format-strings-pointers].
 *
 * The range of possible values is determined by the type.
 *
 * The type system used by #GVariant is #GVariantType. 
 *
 * #GVariant instances always have a type and a value (which are given
 * at construction time).  The type and value of a #GVariant instance
 * can never change other than by the #GVariant itself being
 * destroyed.  A #GVariant cannot contain a pointer.
 *
 * #GVariant is reference counted using g_variant_ref() and
 * g_variant_unref().  #GVariant also has floating reference counts --
 * see g_variant_ref_sink().
 *
 * #GVariant is completely threadsafe.  A #GVariant instance can be
 * concurrently accessed in any way from any number of threads without
 * problems.
 *
 * #GVariant is heavily optimised for dealing with data in serialised
 * form.  It works particularly well with data located in memory-mapped
 * files.  It can perform nearly all deserialisation operations in a
 * small constant time, usually touching only a single memory page.
 * Serialised #GVariant data can also be sent over the network.
 *
 * #GVariant is largely compatible with D-Bus.  Almost all types of
 * #GVariant instances can be sent over D-Bus.  See #GVariantType for
 * exceptions.  (However, #GVariant's serialisation format is not the same
 * as the serialisation format of a D-Bus message body: use #GDBusMessage,
 * in the gio library, for those.)
 *
 * For space-efficiency, the #GVariant serialisation format does not
 * automatically include the variant's length, type or endianness,
 * which must either be implied from context (such as knowledge that a
 * particular file format always contains a little-endian
 * %G_VARIANT_TYPE_VARIANT which occupies the whole length of the file)
 * or supplied out-of-band (for instance, a length, type and/or endianness
 * indicator could be placed at the beginning of a file, network message
 * or network stream).
 *
 * A #GVariant's size is limited mainly by any lower level operating
 * system constraints, such as the number of bits in #gsize.  For
 * example, it is reasonable to have a 2GB file mapped into memory
 * with #GMappedFile, and call g_variant_new_from_data() on it.
 *
 * For convenience to C programmers, #GVariant features powerful
 * varargs-based value construction and destruction.  This feature is
 * designed to be embedded in other libraries.
 *
 * There is a Python-inspired text language for describing #GVariant
 * values.  #GVariant includes a printer for this language and a parser
 * with type inferencing.
 *
 * ## Memory Use
 *
 * #GVariant tries to be quite efficient with respect to memory use.
 * This section gives a rough idea of how much memory is used by the
 * current implementation.  The information here is subject to change
 * in the future.
 *
 * The memory allocated by #GVariant can be grouped into 4 broad
 * purposes: memory for serialised data, memory for the type
 * information cache, buffer management memory and memory for the
 * #GVariant structure itself.
 *
 * ## Serialised Data Memory
 *
 * This is the memory that is used for storing GVariant data in
 * serialised form.  This is what would be sent over the network or
 * what would end up on disk, not counting any indicator of the
 * endianness, or of the length or type of the top-level variant.
 *
 * The amount of memory required to store a boolean is 1 byte. 16,
 * 32 and 64 bit integers and double precision floating point numbers
 * use their "natural" size.  Strings (including object path and
 * signature strings) are stored with a nul terminator, and as such
 * use the length of the string plus 1 byte.
 *
 * Maybe types use no space at all to represent the null value and
 * use the same amount of space (sometimes plus one byte) as the
 * equivalent non-maybe-typed value to represent the non-null case.
 *
 * Arrays use the amount of space required to store each of their
 * members, concatenated.  Additionally, if the items stored in an
 * array are not of a fixed-size (ie: strings, other arrays, etc)
 * then an additional framing offset is stored for each item.  The
 * size of this offset is either 1, 2 or 4 bytes depending on the
 * overall size of the container.  Additionally, extra padding bytes
 * are added as required for alignment of child values.
 *
 * Tuples (including dictionary entries) use the amount of space
 * required to store each of their members, concatenated, plus one
 * framing offset (as per arrays) for each non-fixed-sized item in
 * the tuple, except for the last one.  Additionally, extra padding
 * bytes are added as required for alignment of child values.
 *
 * Variants use the same amount of space as the item inside of the
 * variant, plus 1 byte, plus the length of the type string for the
 * item inside the variant.
 *
 * As an example, consider a dictionary mapping strings to variants.
 * In the case that the dictionary is empty, 0 bytes are required for
 * the serialisation.
 *
 * If we add an item "width" that maps to the int32 value of 500 then
 * we will use 4 byte to store the int32 (so 6 for the variant
 * containing it) and 6 bytes for the string.  The variant must be
 * aligned to 8 after the 6 bytes of the string, so that's 2 extra
 * bytes.  6 (string) + 2 (padding) + 6 (variant) is 14 bytes used
 * for the dictionary entry.  An additional 1 byte is added to the
 * array as a framing offset making a total of 15 bytes.
 *
 * If we add another entry, "title" that maps to a nullable string
 * that happens to have a value of null, then we use 0 bytes for the
 * null value (and 3 bytes for the variant to contain it along with
 * its type string) plus 6 bytes for the string.  Again, we need 2
 * padding bytes.  That makes a total of 6 + 2 + 3 = 11 bytes.
 *
 * We now require extra padding between the two items in the array.
 * After the 14 bytes of the first item, that's 2 bytes required.
 * We now require 2 framing offsets for an extra two
 * bytes. 14 + 2 + 11 + 2 = 29 bytes to encode the entire two-item
 * dictionary.
 *
 * ## Type Information Cache
 *
 * For each GVariant type that currently exists in the program a type
 * information structure is kept in the type information cache.  The
 * type information structure is required for rapid deserialisation.
 *
 * Continuing with the above example, if a #GVariant exists with the
 * type "a{sv}" then a type information struct will exist for
 * "a{sv}", "{sv}", "s", and "v".  Multiple uses of the same type
 * will share the same type information.  Additionally, all
 * single-digit types are stored in read-only static memory and do
 * not contribute to the writable memory footprint of a program using
 * #GVariant.
 *
 * Aside from the type information structures stored in read-only
 * memory, there are two forms of type information.  One is used for
 * container types where there is a single element type: arrays and
 * maybe types.  The other is used for container types where there
 * are multiple element types: tuples and dictionary entries.
 *
 * Array type info structures are 6 * sizeof (void *), plus the
 * memory required to store the type string itself.  This means that
 * on 32-bit systems, the cache entry for "a{sv}" would require 30
 * bytes of memory (plus malloc overhead).
 *
 * Tuple type info structures are 6 * sizeof (void *), plus 4 *
 * sizeof (void *) for each item in the tuple, plus the memory
 * required to store the type string itself.  A 2-item tuple, for
 * example, would have a type information structure that consumed
 * writable memory in the size of 14 * sizeof (void *) (plus type
 * string)  This means that on 32-bit systems, the cache entry for
 * "{sv}" would require 61 bytes of memory (plus malloc overhead).
 *
 * This means that in total, for our "a{sv}" example, 91 bytes of
 * type information would be allocated.
 * 
 * The type information cache, additionally, uses a #GHashTable to
 * store and lookup the cached items and stores a pointer to this
 * hash table in static storage.  The hash table is freed when there
 * are zero items in the type cache.
 *
 * Although these sizes may seem large it is important to remember
 * that a program will probably only have a very small number of
 * different types of values in it and that only one type information
 * structure is required for many different values of the same type.
 *
 * ## Buffer Management Memory
 *
 * #GVariant uses an internal buffer management structure to deal
 * with the various different possible sources of serialised data
 * that it uses.  The buffer is responsible for ensuring that the
 * correct call is made when the data is no longer in use by
 * #GVariant.  This may involve a g_free() or a g_slice_free() or
 * even g_mapped_file_unref().
 *
 * One buffer management structure is used for each chunk of
 * serialised data.  The size of the buffer management structure
 * is 4 * (void *).  On 32-bit systems, that's 16 bytes.
 *
 * ## GVariant structure
 *
 * The size of a #GVariant structure is 6 * (void *).  On 32-bit
 * systems, that's 24 bytes.
 *
 * #GVariant structures only exist if they are explicitly created
 * with API calls.  For example, if a #GVariant is constructed out of
 * serialised data for the example given above (with the dictionary)
 * then although there are 9 individual values that comprise the
 * entire dictionary (two keys, two values, two variants containing
 * the values, two dictionary entries, plus the dictionary itself),
 * only 1 #GVariant instance exists -- the one referring to the
 * dictionary.
 *
 * If calls are made to start accessing the other values then
 * #GVariant instances will exist for those values only for as long
 * as they are in use (ie: until you call g_variant_unref()).  The
 * type information is shared.  The serialised data and the buffer
 * management structure for that serialised data is shared by the
 * child.
 *
 * ## Summary
 *
 * To put the entire example together, for our dictionary mapping
 * strings to variants (with two entries, as given above), we are
 * using 91 bytes of memory for type information, 29 bytes of memory
 * for the serialised data, 16 bytes for buffer management and 24
 * bytes for the #GVariant instance, or a total of 160 bytes, plus
 * malloc overhead.  If we were to use g_variant_get_child_value() to
 * access the two dictionary entries, we would use an additional 48
 * bytes.  If we were to have other dictionaries of the same type, we
 * would use more memory for the serialised data and buffer
 * management for those dictionaries, but the type information would
 * be shared.
 */

/* definition of GVariant structure is in gvariant-core.c */

/* this is a g_return_val_if_fail() for making
 * sure a (GVariant *) has the required type.
 */
#define TYPE_CHECK(value, TYPE, val) \
  if G_UNLIKELY (!g_variant_is_of_type (value, TYPE)) {           \
    g_return_if_fail_warning (G_LOG_DOMAIN, G_STRFUNC,            \
                              "g_variant_is_of_type (" #value     \
                              ", " #TYPE ")");                    \
    return val;                                                   \
  }

/* Numeric Type Constructor/Getters {{{1 */
/* < private >
 * g_variant_new_from_trusted:
 * @type: the #GVariantType
 * @data: the data to use
 * @size: the size of @data
 *
 * Constructs a new trusted #GVariant instance from the provided data.
 * This is used to implement g_variant_new_* for all the basic types.
 *
 * Returns: a new floating #GVariant
 */
static GVariant *
g_variant_new_from_trusted (const GVariantType *type,
                            gconstpointer       data,
                            gsize               size)
{
  GVariant *value;
  GBytes *bytes;

  bytes = g_bytes_new (data, size);
  value = g_variant_new_from_bytes (type, bytes, TRUE);
  g_bytes_unref (bytes);

  return value;
}

/**
 * g_variant_new_boolean:
 * @value: a #gboolean value
 *
 * Creates a new boolean #GVariant instance -- either %TRUE or %FALSE.
 *
 * Returns: (transfer none): a floating reference to a new boolean #GVariant instance
 *
 * Since: 2.24
 **/
GVariant *
g_variant_new_boolean (gboolean value)
{
  guchar v = value;

  return g_variant_new_from_trusted (G_VARIANT_TYPE_BOOLEAN, &v, 1);
}

/**
 * g_variant_get_boolean:
 * @value: a boolean #GVariant instance
 *
 * Returns the boolean value of @value.
 *
 * It is an error to call this function with a @value of any type
 * other than %G_VARIANT_TYPE_BOOLEAN.
 *
 * Returns: %TRUE or %FALSE
 *
 * Since: 2.24
 **/
gboolean
g_variant_get_boolean (GVariant *value)
{
  const guchar *data;

  TYPE_CHECK (value, G_VARIANT_TYPE_BOOLEAN, FALSE);

  data = g_variant_get_data (value);

  return data != NULL ? *data != 0 : FALSE;
}

/* the constructors and accessors for byte, int{16,32,64}, handles and
 * doubles all look pretty much exactly the same, so we reduce
 * copy/pasting here.
 */
#define NUMERIC_TYPE(TYPE, type, ctype) \
  GVariant *g_variant_new_##type (ctype value) {                \
    return g_variant_new_from_trusted (G_VARIANT_TYPE_##TYPE,   \
                                       &value, sizeof value);   \
  }                                                             \
  ctype g_variant_get_##type (GVariant *value) {                \
    const ctype *data;                                          \
    TYPE_CHECK (value, G_VARIANT_TYPE_ ## TYPE, 0);             \
    data = g_variant_get_data (value);                          \
    return data != NULL ? *data : 0;                            \
  }


/**
 * g_variant_new_byte:
 * @value: a #guint8 value
 *
 * Creates a new byte #GVariant instance.
 *
 * Returns: (transfer none): a floating reference to a new byte #GVariant instance
 *
 * Since: 2.24
 **/
/**
 * g_variant_get_byte:
 * @value: a byte #GVariant instance
 *
 * Returns the byte value of @value.
 *
 * It is an error to call this function with a @value of any type
 * other than %G_VARIANT_TYPE_BYTE.
 *
 * Returns: a #guchar
 *
 * Since: 2.24
 **/
NUMERIC_TYPE (BYTE, byte, guchar)

/**
 * g_variant_new_int16:
 * @value: a #gint16 value
 *
 * Creates a new int16 #GVariant instance.
 *
 * Returns: (transfer none): a floating reference to a new int16 #GVariant instance
 *
 * Since: 2.24
 **/
/**
 * g_variant_get_int16:
 * @value: a int16 #GVariant instance
 *
 * Returns the 16-bit signed integer value of @value.
 *
 * It is an error to call this function with a @value of any type
 * other than %G_VARIANT_TYPE_INT16.
 *
 * Returns: a #gint16
 *
 * Since: 2.24
 **/
NUMERIC_TYPE (INT16, int16, gint16)

/**
 * g_variant_new_uint16:
 * @value: a #guint16 value
 *
 * Creates a new uint16 #GVariant instance.
 *
 * Returns: (transfer none): a floating reference to a new uint16 #GVariant instance
 *
 * Since: 2.24
 **/
/**
 * g_variant_get_uint16:
 * @value: a uint16 #GVariant instance
 *
 * Returns the 16-bit unsigned integer value of @value.
 *
 * It is an error to call this function with a @value of any type
 * other than %G_VARIANT_TYPE_UINT16.
 *
 * Returns: a #guint16
 *
 * Since: 2.24
 **/
NUMERIC_TYPE (UINT16, uint16, guint16)

/**
 * g_variant_new_int32:
 * @value: a #gint32 value
 *
 * Creates a new int32 #GVariant instance.
 *
 * Returns: (transfer none): a floating reference to a new int32 #GVariant instance
 *
 * Since: 2.24
 **/
/**
 * g_variant_get_int32:
 * @value: a int32 #GVariant instance
 *
 * Returns the 32-bit signed integer value of @value.
 *
 * It is an error to call this function with a @value of any type
 * other than %G_VARIANT_TYPE_INT32.
 *
 * Returns: a #gint32
 *
 * Since: 2.24
 **/
NUMERIC_TYPE (INT32, int32, gint32)

/**
 * g_variant_new_uint32:
 * @value: a #guint32 value
 *
 * Creates a new uint32 #GVariant instance.
 *
 * Returns: (transfer none): a floating reference to a new uint32 #GVariant instance
 *
 * Since: 2.24
 **/
/**
 * g_variant_get_uint32:
 * @value: a uint32 #GVariant instance
 *
 * Returns the 32-bit unsigned integer value of @value.
 *
 * It is an error to call this function with a @value of any type
 * other than %G_VARIANT_TYPE_UINT32.
 *
 * Returns: a #guint32
 *
 * Since: 2.24
 **/
NUMERIC_TYPE (UINT32, uint32, guint32)

/**
 * g_variant_new_int64:
 * @value: a #gint64 value
 *
 * Creates a new int64 #GVariant instance.
 *
 * Returns: (transfer none): a floating reference to a new int64 #GVariant instance
 *
 * Since: 2.24
 **/
/**
 * g_variant_get_int64:
 * @value: a int64 #GVariant instance
 *
 * Returns the 64-bit signed integer value of @value.
 *
 * It is an error to call this function with a @value of any type
 * other than %G_VARIANT_TYPE_INT64.
 *
 * Returns: a #gint64
 *
 * Since: 2.24
 **/
NUMERIC_TYPE (INT64, int64, gint64)

/**
 * g_variant_new_uint64:
 * @value: a #guint64 value
 *
 * Creates a new uint64 #GVariant instance.
 *
 * Returns: (transfer none): a floating reference to a new uint64 #GVariant instance
 *
 * Since: 2.24
 **/
/**
 * g_variant_get_uint64:
 * @value: a uint64 #GVariant instance
 *
 * Returns the 64-bit unsigned integer value of @value.
 *
 * It is an error to call this function with a @value of any type
 * other than %G_VARIANT_TYPE_UINT64.
 *
 * Returns: a #guint64
 *
 * Since: 2.24
 **/
NUMERIC_TYPE (UINT64, uint64, guint64)

/**
 * g_variant_new_handle:
 * @value: a #gint32 value
 *
 * Creates a new handle #GVariant instance.
 *
 * By convention, handles are indexes into an array of file descriptors
 * that are sent alongside a D-Bus message.  If you're not interacting
 * with D-Bus, you probably don't need them.
 *
 * Returns: (transfer none): a floating reference to a new handle #GVariant instance
 *
 * Since: 2.24
 **/
/**
 * g_variant_get_handle:
 * @value: a handle #GVariant instance
 *
 * Returns the 32-bit signed integer value of @value.
 *
 * It is an error to call this function with a @value of any type other
 * than %G_VARIANT_TYPE_HANDLE.
 *
 * By convention, handles are indexes into an array of file descriptors
 * that are sent alongside a D-Bus message.  If you're not interacting
 * with D-Bus, you probably don't need them.
 *
 * Returns: a #gint32
 *
 * Since: 2.24
 **/
NUMERIC_TYPE (HANDLE, handle, gint32)

/**
 * g_variant_new_double:
 * @value: a #gdouble floating point value
 *
 * Creates a new double #GVariant instance.
 *
 * Returns: (transfer none): a floating reference to a new double #GVariant instance
 *
 * Since: 2.24
 **/
/**
 * g_variant_get_double:
 * @value: a double #GVariant instance
 *
 * Returns the double precision floating point value of @value.
 *
 * It is an error to call this function with a @value of any type
 * other than %G_VARIANT_TYPE_DOUBLE.
 *
 * Returns: a #gdouble
 *
 * Since: 2.24
 **/
NUMERIC_TYPE (DOUBLE, double, gdouble)

/* Container type Constructor / Deconstructors {{{1 */
/**
 * g_variant_new_maybe:
 * @child_type: (nullable): the #GVariantType of the child, or %NULL
 * @child: (nullable): the child value, or %NULL
 *
 * Depending on if @child is %NULL, either wraps @child inside of a
 * maybe container or creates a Nothing instance for the given @type.
 *
 * At least one of @child_type and @child must be non-%NULL.
 * If @child_type is non-%NULL then it must be a definite type.
 * If they are both non-%NULL then @child_type must be the type
 * of @child.
 *
 * If @child is a floating reference (see g_variant_ref_sink()), the new
 * instance takes ownership of @child.
 *
 * Returns: (transfer none): a floating reference to a new #GVariant maybe instance
 *
 * Since: 2.24
 **/
GVariant *
g_variant_new_maybe (const GVariantType *child_type,
                     GVariant           *child)
{
  GVariantType *maybe_type;
  GVariant *value;

  g_return_val_if_fail (child_type == NULL || g_variant_type_is_definite
                        (child_type), 0);
  g_return_val_if_fail (child_type != NULL || child != NULL, NULL);
  g_return_val_if_fail (child_type == NULL || child == NULL ||
                        g_variant_is_of_type (child, child_type),
                        NULL);

  if (child_type == NULL)
    child_type = g_variant_get_type (child);

  maybe_type = g_variant_type_new_maybe (child_type);

  if (child != NULL)
    {
      GVariant **children;
      gboolean trusted;

      children = g_new (GVariant *, 1);
      children[0] = g_variant_ref_sink (child);
      trusted = g_variant_is_trusted (children[0]);

      value = g_variant_new_from_children (maybe_type, children, 1, trusted);
    }
  else
    value = g_variant_new_from_children (maybe_type, NULL, 0, TRUE);

  g_variant_type_free (maybe_type);

  return value;
}

/**
 * g_variant_get_maybe:
 * @value: a maybe-typed value
 *
 * Given a maybe-typed #GVariant instance, extract its value.  If the
 * value is Nothing, then this function returns %NULL.
 *
 * Returns: (nullable) (transfer full): the contents of @value, or %NULL
 *
 * Since: 2.24
 **/
GVariant *
g_variant_get_maybe (GVariant *value)
{
  TYPE_CHECK (value, G_VARIANT_TYPE_MAYBE, NULL);

  if (g_variant_n_children (value))
    return g_variant_get_child_value (value, 0);

  return NULL;
}

/**
 * g_variant_new_variant: (constructor)
 * @value: a #GVariant instance
 *
 * Boxes @value.  The result is a #GVariant instance representing a
 * variant containing the original value.
 *
 * If @child is a floating reference (see g_variant_ref_sink()), the new
 * instance takes ownership of @child.
 *
 * Returns: (transfer none): a floating reference to a new variant #GVariant instance
 *
 * Since: 2.24
 **/
GVariant *
g_variant_new_variant (GVariant *value)
{
  g_return_val_if_fail (value != NULL, NULL);

  g_variant_ref_sink (value);

  return g_variant_new_from_children (G_VARIANT_TYPE_VARIANT,
                                      g_memdup (&value, sizeof value),
                                      1, g_variant_is_trusted (value));
}

/**
 * g_variant_get_variant:
 * @value: a variant #GVariant instance
 *
 * Unboxes @value.  The result is the #GVariant instance that was
 * contained in @value.
 *
 * Returns: (transfer full): the item contained in the variant
 *
 * Since: 2.24
 **/
GVariant *
g_variant_get_variant (GVariant *value)
{
  TYPE_CHECK (value, G_VARIANT_TYPE_VARIANT, NULL);

  return g_variant_get_child_value (value, 0);
}

/**
 * g_variant_new_array:
 * @child_type: (nullable): the element type of the new array
 * @children: (nullable) (array length=n_children): an array of
 *            #GVariant pointers, the children
 * @n_children: the length of @children
 *
 * Creates a new #GVariant array from @children.
 *
 * @child_type must be non-%NULL if @n_children is zero.  Otherwise, the
 * child type is determined by inspecting the first element of the
 * @children array.  If @child_type is non-%NULL then it must be a
 * definite type.
 *
 * The items of the array are taken from the @children array.  No entry
 * in the @children array may be %NULL.
 *
 * All items in the array must have the same type, which must be the
 * same as @child_type, if given.
 *
 * If the @children are floating references (see g_variant_ref_sink()), the
 * new instance takes ownership of them as if via g_variant_ref_sink().
 *
 * Returns: (transfer none): a floating reference to a new #GVariant array
 *
 * Since: 2.24
 **/
GVariant *
g_variant_new_array (const GVariantType *child_type,
                     GVariant * const   *children,
                     gsize               n_children)
{
  GVariantType *array_type;
  GVariant **my_children;
  gboolean trusted;
  GVariant *value;
  gsize i;

  g_return_val_if_fail (n_children > 0 || child_type != NULL, NULL);
  g_return_val_if_fail (n_children == 0 || children != NULL, NULL);
  g_return_val_if_fail (child_type == NULL ||
                        g_variant_type_is_definite (child_type), NULL);

  my_children = g_new (GVariant *, n_children);
  trusted = TRUE;

  if (child_type == NULL)
    child_type = g_variant_get_type (children[0]);
  array_type = g_variant_type_new_array (child_type);

  for (i = 0; i < n_children; i++)
    {
      TYPE_CHECK (children[i], child_type, NULL);
      my_children[i] = g_variant_ref_sink (children[i]);
      trusted &= g_variant_is_trusted (children[i]);
    }

  value = g_variant_new_from_children (array_type, my_children,
                                       n_children, trusted);
  g_variant_type_free (array_type);

  return value;
}

/*< private >
 * g_variant_make_tuple_type:
 * @children: (array length=n_children): an array of GVariant *
 * @n_children: the length of @children
 *
 * Return the type of a tuple containing @children as its items.
 **/
static GVariantType *
g_variant_make_tuple_type (GVariant * const *children,
                           gsize             n_children)
{
  const GVariantType **types;
  GVariantType *type;
  gsize i;

  types = g_new (const GVariantType *, n_children);

  for (i = 0; i < n_children; i++)
    types[i] = g_variant_get_type (children[i]);

  type = g_variant_type_new_tuple (types, n_children);
  g_free (types);

  return type;
}

/**
 * g_variant_new_tuple:
 * @children: (array length=n_children): the items to make the tuple out of
 * @n_children: the length of @children
 *
 * Creates a new tuple #GVariant out of the items in @children.  The
 * type is determined from the types of @children.  No entry in the
 * @children array may be %NULL.
 *
 * If @n_children is 0 then the unit tuple is constructed.
 *
 * If the @children are floating references (see g_variant_ref_sink()), the
 * new instance takes ownership of them as if via g_variant_ref_sink().
 *
 * Returns: (transfer none): a floating reference to a new #GVariant tuple
 *
 * Since: 2.24
 **/
GVariant *
g_variant_new_tuple (GVariant * const *children,
                     gsize             n_children)
{
  GVariantType *tuple_type;
  GVariant **my_children;
  gboolean trusted;
  GVariant *value;
  gsize i;

  g_return_val_if_fail (n_children == 0 || children != NULL, NULL);

  my_children = g_new (GVariant *, n_children);
  trusted = TRUE;

  for (i = 0; i < n_children; i++)
    {
      my_children[i] = g_variant_ref_sink (children[i]);
      trusted &= g_variant_is_trusted (children[i]);
    }

  tuple_type = g_variant_make_tuple_type (children, n_children);
  value = g_variant_new_from_children (tuple_type, my_children,
                                       n_children, trusted);
  g_variant_type_free (tuple_type);

  return value;
}

/*< private >
 * g_variant_make_dict_entry_type:
 * @key: a #GVariant, the key
 * @val: a #GVariant, the value
 *
 * Return the type of a dictionary entry containing @key and @val as its
 * children.
 **/
static GVariantType *
g_variant_make_dict_entry_type (GVariant *key,
                                GVariant *val)
{
  return g_variant_type_new_dict_entry (g_variant_get_type (key),
                                        g_variant_get_type (val));
}

/**
 * g_variant_new_dict_entry: (constructor)
 * @key: a basic #GVariant, the key
 * @value: a #GVariant, the value
 *
 * Creates a new dictionary entry #GVariant. @key and @value must be
 * non-%NULL. @key must be a value of a basic type (ie: not a container).
 *
 * If the @key or @value are floating references (see g_variant_ref_sink()),
 * the new instance takes ownership of them as if via g_variant_ref_sink().
 *
 * Returns: (transfer none): a floating reference to a new dictionary entry #GVariant
 *
 * Since: 2.24
 **/
GVariant *
g_variant_new_dict_entry (GVariant *key,
                          GVariant *value)
{
  GVariantType *dict_type;
  GVariant **children;
  gboolean trusted;

  g_return_val_if_fail (key != NULL && value != NULL, NULL);
  g_return_val_if_fail (!g_variant_is_container (key), NULL);

  children = g_new (GVariant *, 2);
  children[0] = g_variant_ref_sink (key);
  children[1] = g_variant_ref_sink (value);
  trusted = g_variant_is_trusted (key) && g_variant_is_trusted (value);

  dict_type = g_variant_make_dict_entry_type (key, value);
  value = g_variant_new_from_children (dict_type, children, 2, trusted);
  g_variant_type_free (dict_type);

  return value;
}

/**
 * g_variant_lookup: (skip)
 * @dictionary: a dictionary #GVariant
 * @key: the key to lookup in the dictionary
 * @format_string: a GVariant format string
 * @...: the arguments to unpack the value into
 *
 * Looks up a value in a dictionary #GVariant.
 *
 * This function is a wrapper around g_variant_lookup_value() and
 * g_variant_get().  In the case that %NULL would have been returned,
 * this function returns %FALSE.  Otherwise, it unpacks the returned
 * value and returns %TRUE.
 *
 * @format_string determines the C types that are used for unpacking
 * the values and also determines if the values are copied or borrowed,
 * see the section on
 * [GVariant format strings][gvariant-format-strings-pointers].
 *
 * This function is currently implemented with a linear scan.  If you
 * plan to do many lookups then #GVariantDict may be more efficient.
 *
 * Returns: %TRUE if a value was unpacked
 *
 * Since: 2.28
 */
gboolean
g_variant_lookup (GVariant    *dictionary,
                  const gchar *key,
                  const gchar *format_string,
                  ...)
{
  GVariantType *type;
  GVariant *value;

  /* flatten */
  g_variant_get_data (dictionary);

  type = g_variant_format_string_scan_type (format_string, NULL, NULL);
  value = g_variant_lookup_value (dictionary, key, type);
  g_variant_type_free (type);

  if (value)
    {
      va_list ap;

      va_start (ap, format_string);
      g_variant_get_va (value, format_string, NULL, &ap);
      g_variant_unref (value);
      va_end (ap);

      return TRUE;
    }

  else
    return FALSE;
}

/**
 * g_variant_lookup_value:
 * @dictionary: a dictionary #GVariant
 * @key: the key to lookup in the dictionary
 * @expected_type: (nullable): a #GVariantType, or %NULL
 *
 * Looks up a value in a dictionary #GVariant.
 *
 * This function works with dictionaries of the type a{s*} (and equally
 * well with type a{o*}, but we only further discuss the string case
 * for sake of clarity).
 *
 * In the event that @dictionary has the type a{sv}, the @expected_type
 * string specifies what type of value is expected to be inside of the
 * variant. If the value inside the variant has a different type then
 * %NULL is returned. In the event that @dictionary has a value type other
 * than v then @expected_type must directly match the key type and it is
 * used to unpack the value directly or an error occurs.
 *
 * In either case, if @key is not found in @dictionary, %NULL is returned.
 *
 * If the key is found and the value has the correct type, it is
 * returned.  If @expected_type was specified then any non-%NULL return
 * value will have this type.
 *
 * This function is currently implemented with a linear scan.  If you
 * plan to do many lookups then #GVariantDict may be more efficient.
 *
 * Returns: (transfer full): the value of the dictionary key, or %NULL
 *
 * Since: 2.28
 */
GVariant *
g_variant_lookup_value (GVariant           *dictionary,
                        const gchar        *key,
                        const GVariantType *expected_type)
{
  GVariantIter iter;
  GVariant *entry;
  GVariant *value;

  g_return_val_if_fail (g_variant_is_of_type (dictionary,
                                              G_VARIANT_TYPE ("a{s*}")) ||
                        g_variant_is_of_type (dictionary,
                                              G_VARIANT_TYPE ("a{o*}")),
                        NULL);

  g_variant_iter_init (&iter, dictionary);

  while ((entry = g_variant_iter_next_value (&iter)))
    {
      GVariant *entry_key;
      gboolean matches;

      entry_key = g_variant_get_child_value (entry, 0);
      matches = strcmp (g_variant_get_string (entry_key, NULL), key) == 0;
      g_variant_unref (entry_key);

      if (matches)
        break;

      g_variant_unref (entry);
    }

  if (entry == NULL)
    return NULL;

  value = g_variant_get_child_value (entry, 1);
  g_variant_unref (entry);

  if (g_variant_is_of_type (value, G_VARIANT_TYPE_VARIANT))
    {
      GVariant *tmp;

      tmp = g_variant_get_variant (value);
      g_variant_unref (value);

      if (expected_type && !g_variant_is_of_type (tmp, expected_type))
        {
          g_variant_unref (tmp);
          tmp = NULL;
        }

      value = tmp;
    }

  g_return_val_if_fail (expected_type == NULL || value == NULL ||
                        g_variant_is_of_type (value, expected_type), NULL);

  return value;
}

/**
 * g_variant_get_fixed_array:
 * @value: a #GVariant array with fixed-sized elements
 * @n_elements: (out): a pointer to the location to store the number of items
 * @element_size: the size of each element
 *
 * Provides access to the serialised data for an array of fixed-sized
 * items.
 *
 * @value must be an array with fixed-sized elements.  Numeric types are
 * fixed-size, as are tuples containing only other fixed-sized types.
 *
 * @element_size must be the size of a single element in the array,
 * as given by the section on
 * [serialized data memory][gvariant-serialised-data-memory].
 *
 * In particular, arrays of these fixed-sized types can be interpreted
 * as an array of the given C type, with @element_size set to the size
 * the appropriate type:
 * - %G_VARIANT_TYPE_INT16 (etc.): #gint16 (etc.)
 * - %G_VARIANT_TYPE_BOOLEAN: #guchar (not #gboolean!)
 * - %G_VARIANT_TYPE_BYTE: #guchar
 * - %G_VARIANT_TYPE_HANDLE: #guint32
 * - %G_VARIANT_TYPE_DOUBLE: #gdouble
 *
 * For example, if calling this function for an array of 32-bit integers,
 * you might say `sizeof(gint32)`. This value isn't used except for the purpose
 * of a double-check that the form of the serialised data matches the caller's
 * expectation.
 *
 * @n_elements, which must be non-%NULL, is set equal to the number of
 * items in the array.
 *
 * Returns: (array length=n_elements) (transfer none): a pointer to
 *     the fixed array
 *
 * Since: 2.24
 **/
gconstpointer
g_variant_get_fixed_array (GVariant *value,
                           gsize    *n_elements,
                           gsize     element_size)
{
  GVariantTypeInfo *array_info;
  gsize array_element_size;
  gconstpointer data;
  gsize size;

  TYPE_CHECK (value, G_VARIANT_TYPE_ARRAY, NULL);

  g_return_val_if_fail (n_elements != NULL, NULL);
  g_return_val_if_fail (element_size > 0, NULL);

  array_info = g_variant_get_type_info (value);
  g_variant_type_info_query_element (array_info, NULL, &array_element_size);

  g_return_val_if_fail (array_element_size, NULL);

  if G_UNLIKELY (array_element_size != element_size)
    {
      if (array_element_size)
        g_critical ("g_variant_get_fixed_array: assertion "
                    "'g_variant_array_has_fixed_size (value, element_size)' "
                    "failed: array size %"G_GSIZE_FORMAT" does not match "
                    "given element_size %"G_GSIZE_FORMAT".",
                    array_element_size, element_size);
      else
        g_critical ("g_variant_get_fixed_array: assertion "
                    "'g_variant_array_has_fixed_size (value, element_size)' "
                    "failed: array does not have fixed size.");
    }

  data = g_variant_get_data (value);
  size = g_variant_get_size (value);

  if (size % element_size)
    *n_elements = 0;
  else
    *n_elements = size / element_size;

  if (*n_elements)
    return data;

  return NULL;
}

/**
 * g_variant_new_fixed_array:
 * @element_type: the #GVariantType of each element
 * @elements: a pointer to the fixed array of contiguous elements
 * @n_elements: the number of elements
 * @element_size: the size of each element
 *
 * Constructs a new array #GVariant instance, where the elements are
 * of @element_type type.
 *
 * @elements must be an array with fixed-sized elements.  Numeric types are
 * fixed-size as are tuples containing only other fixed-sized types.
 *
 * @element_size must be the size of a single element in the array.
 * For example, if calling this function for an array of 32-bit integers,
 * you might say sizeof(gint32). This value isn't used except for the purpose
 * of a double-check that the form of the serialised data matches the caller's
 * expectation.
 *
 * @n_elements must be the length of the @elements array.
 *
 * Returns: (transfer none): a floating reference to a new array #GVariant instance
 *
 * Since: 2.32
 **/
GVariant *
g_variant_new_fixed_array (const GVariantType  *element_type,
                           gconstpointer        elements,
                           gsize                n_elements,
                           gsize                element_size)
{
  GVariantType *array_type;
  gsize array_element_size;
  GVariantTypeInfo *array_info;
  GVariant *value;
  gpointer data;

  g_return_val_if_fail (g_variant_type_is_definite (element_type), NULL);
  g_return_val_if_fail (element_size > 0, NULL);

  array_type = g_variant_type_new_array (element_type);
  array_info = g_variant_type_info_get (array_type);
  g_variant_type_info_query_element (array_info, NULL, &array_element_size);
  if G_UNLIKELY (array_element_size != element_size)
    {
      if (array_element_size)
        g_critical ("g_variant_new_fixed_array: array size %" G_GSIZE_FORMAT
                    " does not match given element_size %" G_GSIZE_FORMAT ".",
                    array_element_size, element_size);
      else
        g_critical ("g_variant_get_fixed_array: array does not have fixed size.");
      return NULL;
    }

  data = g_memdup (elements, n_elements * element_size);
  value = g_variant_new_from_data (array_type, data,
                                   n_elements * element_size,
                                   FALSE, g_free, data);

  g_variant_type_free (array_type);
  g_variant_type_info_unref (array_info);

  return value;
}

/* String type constructor/getters/validation {{{1 */
/**
 * g_variant_new_string:
 * @string: a normal UTF-8 nul-terminated string
 *
 * Creates a string #GVariant with the contents of @string.
 *
 * @string must be valid UTF-8, and must not be %NULL. To encode
 * potentially-%NULL strings, use g_variant_new() with `ms` as the
 * [format string][gvariant-format-strings-maybe-types].
 *
 * Returns: (transfer none): a floating reference to a new string #GVariant instance
 *
 * Since: 2.24
 **/
GVariant *
g_variant_new_string (const gchar *string)
{
  g_return_val_if_fail (string != NULL, NULL);
  g_return_val_if_fail (g_utf8_validate (string, -1, NULL), NULL);

  return g_variant_new_from_trusted (G_VARIANT_TYPE_STRING,
                                     string, strlen (string) + 1);
}

/**
 * g_variant_new_take_string: (skip)
 * @string: a normal UTF-8 nul-terminated string
 *
 * Creates a string #GVariant with the contents of @string.
 *
 * @string must be valid UTF-8, and must not be %NULL. To encode
 * potentially-%NULL strings, use this with g_variant_new_maybe().
 *
 * This function consumes @string.  g_free() will be called on @string
 * when it is no longer required.
 *
 * You must not modify or access @string in any other way after passing
 * it to this function.  It is even possible that @string is immediately
 * freed.
 *
 * Returns: (transfer none): a floating reference to a new string
 *   #GVariant instance
 *
 * Since: 2.38
 **/
GVariant *
g_variant_new_take_string (gchar *string)
{
  GVariant *value;
  GBytes *bytes;

  g_return_val_if_fail (string != NULL, NULL);
  g_return_val_if_fail (g_utf8_validate (string, -1, NULL), NULL);

  bytes = g_bytes_new_take (string, strlen (string) + 1);
  value = g_variant_new_from_bytes (G_VARIANT_TYPE_STRING, bytes, TRUE);
  g_bytes_unref (bytes);

  return value;
}

/**
 * g_variant_new_printf: (skip)
 * @format_string: a printf-style format string
 * @...: arguments for @format_string
 *
 * Creates a string-type GVariant using printf formatting.
 *
 * This is similar to calling g_strdup_printf() and then
 * g_variant_new_string() but it saves a temporary variable and an
 * unnecessary copy.
 *
 * Returns: (transfer none): a floating reference to a new string
 *   #GVariant instance
 *
 * Since: 2.38
 **/
GVariant *
g_variant_new_printf (const gchar *format_string,
                      ...)
{
  GVariant *value;
  GBytes *bytes;
  gchar *string;
  va_list ap;

  g_return_val_if_fail (format_string != NULL, NULL);

  va_start (ap, format_string);
  string = g_strdup_vprintf (format_string, ap);
  va_end (ap);

  bytes = g_bytes_new_take (string, strlen (string) + 1);
  value = g_variant_new_from_bytes (G_VARIANT_TYPE_STRING, bytes, TRUE);
  g_bytes_unref (bytes);

  return value;
}

/**
 * g_variant_new_object_path:
 * @object_path: a normal C nul-terminated string
 *
 * Creates a D-Bus object path #GVariant with the contents of @string.
 * @string must be a valid D-Bus object path.  Use
 * g_variant_is_object_path() if you're not sure.
 *
 * Returns: (transfer none): a floating reference to a new object path #GVariant instance
 *
 * Since: 2.24
 **/
GVariant *
g_variant_new_object_path (const gchar *object_path)
{
  g_return_val_if_fail (g_variant_is_object_path (object_path), NULL);

  return g_variant_new_from_trusted (G_VARIANT_TYPE_OBJECT_PATH,
                                     object_path, strlen (object_path) + 1);
}

/**
 * g_variant_is_object_path:
 * @string: a normal C nul-terminated string
 *
 * Determines if a given string is a valid D-Bus object path.  You
 * should ensure that a string is a valid D-Bus object path before
 * passing it to g_variant_new_object_path().
 *
 * A valid object path starts with '/' followed by zero or more
 * sequences of characters separated by '/' characters.  Each sequence
 * must contain only the characters "[A-Z][a-z][0-9]_".  No sequence
 * (including the one following the final '/' character) may be empty.
 *
 * Returns: %TRUE if @string is a D-Bus object path
 *
 * Since: 2.24
 **/
gboolean
g_variant_is_object_path (const gchar *string)
{
  g_return_val_if_fail (string != NULL, FALSE);

  return g_variant_serialiser_is_object_path (string, strlen (string) + 1);
}

/**
 * g_variant_new_signature:
 * @signature: a normal C nul-terminated string
 *
 * Creates a D-Bus type signature #GVariant with the contents of
 * @string.  @string must be a valid D-Bus type signature.  Use
 * g_variant_is_signature() if you're not sure.
 *
 * Returns: (transfer none): a floating reference to a new signature #GVariant instance
 *
 * Since: 2.24
 **/
GVariant *
g_variant_new_signature (const gchar *signature)
{
  g_return_val_if_fail (g_variant_is_signature (signature), NULL);

  return g_variant_new_from_trusted (G_VARIANT_TYPE_SIGNATURE,
                                     signature, strlen (signature) + 1);
}

/**
 * g_variant_is_signature:
 * @string: a normal C nul-terminated string
 *
 * Determines if a given string is a valid D-Bus type signature.  You
 * should ensure that a string is a valid D-Bus type signature before
 * passing it to g_variant_new_signature().
 *
 * D-Bus type signatures consist of zero or more definite #GVariantType
 * strings in sequence.
 *
 * Returns: %TRUE if @string is a D-Bus type signature
 *
 * Since: 2.24
 **/
gboolean
g_variant_is_signature (const gchar *string)
{
  g_return_val_if_fail (string != NULL, FALSE);

  return g_variant_serialiser_is_signature (string, strlen (string) + 1);
}

/**
 * g_variant_get_string:
 * @value: a string #GVariant instance
 * @length: (optional) (default 0) (out): a pointer to a #gsize,
 *          to store the length
 *
 * Returns the string value of a #GVariant instance with a string
 * type.  This includes the types %G_VARIANT_TYPE_STRING,
 * %G_VARIANT_TYPE_OBJECT_PATH and %G_VARIANT_TYPE_SIGNATURE.
 *
 * The string will always be UTF-8 encoded, and will never be %NULL.
 *
 * If @length is non-%NULL then the length of the string (in bytes) is
 * returned there.  For trusted values, this information is already
 * known.  For untrusted values, a strlen() will be performed.
 *
 * It is an error to call this function with a @value of any type
 * other than those three.
 *
 * The return value remains valid as long as @value exists.
 *
 * Returns: (transfer none): the constant string, UTF-8 encoded
 *
 * Since: 2.24
 **/
const gchar *
g_variant_get_string (GVariant *value,
                      gsize    *length)
{
  gconstpointer data;
  gsize size;

  g_return_val_if_fail (value != NULL, NULL);
  g_return_val_if_fail (
    g_variant_is_of_type (value, G_VARIANT_TYPE_STRING) ||
    g_variant_is_of_type (value, G_VARIANT_TYPE_OBJECT_PATH) ||
    g_variant_is_of_type (value, G_VARIANT_TYPE_SIGNATURE), NULL);

  data = g_variant_get_data (value);
  size = g_variant_get_size (value);

  if (!g_variant_is_trusted (value))
    {
      switch (g_variant_classify (value))
        {
        case G_VARIANT_CLASS_STRING:
          if (g_variant_serialiser_is_string (data, size))
            break;

          data = "";
          size = 1;
          break;

        case G_VARIANT_CLASS_OBJECT_PATH:
          if (g_variant_serialiser_is_object_path (data, size))
            break;

          data = "/";
          size = 2;
          break;

        case G_VARIANT_CLASS_SIGNATURE:
          if (g_variant_serialiser_is_signature (data, size))
            break;

          data = "";
          size = 1;
          break;

        default:
          g_assert_not_reached ();
        }
    }

  if (length)
    *length = size - 1;

  return data;
}

/**
 * g_variant_dup_string:
 * @value: a string #GVariant instance
 * @length: (out): a pointer to a #gsize, to store the length
 *
 * Similar to g_variant_get_string() except that instead of returning
 * a constant string, the string is duplicated.
 *
 * The string will always be UTF-8 encoded.
 *
 * The return value must be freed using g_free().
 *
 * Returns: (transfer full): a newly allocated string, UTF-8 encoded
 *
 * Since: 2.24
 **/
gchar *
g_variant_dup_string (GVariant *value,
                      gsize    *length)
{
  return g_strdup (g_variant_get_string (value, length));
}

/**
 * g_variant_new_strv:
 * @strv: (array length=length) (element-type utf8): an array of strings
 * @length: the length of @strv, or -1
 *
 * Constructs an array of strings #GVariant from the given array of
 * strings.
 *
 * If @length is -1 then @strv is %NULL-terminated.
 *
 * Returns: (transfer none): a new floating #GVariant instance
 *
 * Since: 2.24
 **/
GVariant *
g_variant_new_strv (const gchar * const *strv,
                    gssize               length)
{
  GVariant **strings;
  gsize i;

  g_return_val_if_fail (length == 0 || strv != NULL, NULL);

  if (length < 0)
    length = g_strv_length ((gchar **) strv);

  strings = g_new (GVariant *, length);
  for (i = 0; i < length; i++)
    strings[i] = g_variant_ref_sink (g_variant_new_string (strv[i]));

  return g_variant_new_from_children (G_VARIANT_TYPE_STRING_ARRAY,
                                      strings, length, TRUE);
}

/**
 * g_variant_get_strv:
 * @value: an array of strings #GVariant
 * @length: (out) (optional): the length of the result, or %NULL
 *
 * Gets the contents of an array of strings #GVariant.  This call
 * makes a shallow copy; the return result should be released with
 * g_free(), but the individual strings must not be modified.
 *
 * If @length is non-%NULL then the number of elements in the result
 * is stored there.  In any case, the resulting array will be
 * %NULL-terminated.
 *
 * For an empty array, @length will be set to 0 and a pointer to a
 * %NULL pointer will be returned.
 *
 * Returns: (array length=length zero-terminated=1) (transfer container): an array of constant strings
 *
 * Since: 2.24
 **/
const gchar **
g_variant_get_strv (GVariant *value,
                    gsize    *length)
{
  const gchar **strv;
  gsize n;
  gsize i;

  TYPE_CHECK (value, G_VARIANT_TYPE_STRING_ARRAY, NULL);

  g_variant_get_data (value);
  n = g_variant_n_children (value);
  strv = g_new (const gchar *, n + 1);

  for (i = 0; i < n; i++)
    {
      GVariant *string;

      string = g_variant_get_child_value (value, i);
      strv[i] = g_variant_get_string (string, NULL);
      g_variant_unref (string);
    }
  strv[i] = NULL;

  if (length)
    *length = n;

  return strv;
}

/**
 * g_variant_dup_strv:
 * @value: an array of strings #GVariant
 * @length: (out) (optional): the length of the result, or %NULL
 *
 * Gets the contents of an array of strings #GVariant.  This call
 * makes a deep copy; the return result should be released with
 * g_strfreev().
 *
 * If @length is non-%NULL then the number of elements in the result
 * is stored there.  In any case, the resulting array will be
 * %NULL-terminated.
 *
 * For an empty array, @length will be set to 0 and a pointer to a
 * %NULL pointer will be returned.
 *
 * Returns: (array length=length zero-terminated=1) (transfer full): an array of strings
 *
 * Since: 2.24
 **/
gchar **
g_variant_dup_strv (GVariant *value,
                    gsize    *length)
{
  gchar **strv;
  gsize n;
  gsize i;

  TYPE_CHECK (value, G_VARIANT_TYPE_STRING_ARRAY, NULL);

  n = g_variant_n_children (value);
  strv = g_new (gchar *, n + 1);

  for (i = 0; i < n; i++)
    {
      GVariant *string;

      string = g_variant_get_child_value (value, i);
      strv[i] = g_variant_dup_string (string, NULL);
      g_variant_unref (string);
    }
  strv[i] = NULL;

  if (length)
    *length = n;

  return strv;
}

/**
 * g_variant_new_objv:
 * @strv: (array length=length) (element-type utf8): an array of strings
 * @length: the length of @strv, or -1
 *
 * Constructs an array of object paths #GVariant from the given array of
 * strings.
 *
 * Each string must be a valid #GVariant object path; see
 * g_variant_is_object_path().
 *
 * If @length is -1 then @strv is %NULL-terminated.
 *
 * Returns: (transfer none): a new floating #GVariant instance
 *
 * Since: 2.30
 **/
GVariant *
g_variant_new_objv (const gchar * const *strv,
                    gssize               length)
{
  GVariant **strings;
  gsize i;

  g_return_val_if_fail (length == 0 || strv != NULL, NULL);

  if (length < 0)
    length = g_strv_length ((gchar **) strv);

  strings = g_new (GVariant *, length);
  for (i = 0; i < length; i++)
    strings[i] = g_variant_ref_sink (g_variant_new_object_path (strv[i]));

  return g_variant_new_from_children (G_VARIANT_TYPE_OBJECT_PATH_ARRAY,
                                      strings, length, TRUE);
}

/**
 * g_variant_get_objv:
 * @value: an array of object paths #GVariant
 * @length: (out) (optional): the length of the result, or %NULL
 *
 * Gets the contents of an array of object paths #GVariant.  This call
 * makes a shallow copy; the return result should be released with
 * g_free(), but the individual strings must not be modified.
 *
 * If @length is non-%NULL then the number of elements in the result
 * is stored there.  In any case, the resulting array will be
 * %NULL-terminated.
 *
 * For an empty array, @length will be set to 0 and a pointer to a
 * %NULL pointer will be returned.
 *
 * Returns: (array length=length zero-terminated=1) (transfer container): an array of constant strings
 *
 * Since: 2.30
 **/
const gchar **
g_variant_get_objv (GVariant *value,
                    gsize    *length)
{
  const gchar **strv;
  gsize n;
  gsize i;

  TYPE_CHECK (value, G_VARIANT_TYPE_OBJECT_PATH_ARRAY, NULL);

  g_variant_get_data (value);
  n = g_variant_n_children (value);
  strv = g_new (const gchar *, n + 1);

  for (i = 0; i < n; i++)
    {
      GVariant *string;

      string = g_variant_get_child_value (value, i);
      strv[i] = g_variant_get_string (string, NULL);
      g_variant_unref (string);
    }
  strv[i] = NULL;

  if (length)
    *length = n;

  return strv;
}

/**
 * g_variant_dup_objv:
 * @value: an array of object paths #GVariant
 * @length: (out) (optional): the length of the result, or %NULL
 *
 * Gets the contents of an array of object paths #GVariant.  This call
 * makes a deep copy; the return result should be released with
 * g_strfreev().
 *
 * If @length is non-%NULL then the number of elements in the result
 * is stored there.  In any case, the resulting array will be
 * %NULL-terminated.
 *
 * For an empty array, @length will be set to 0 and a pointer to a
 * %NULL pointer will be returned.
 *
 * Returns: (array length=length zero-terminated=1) (transfer full): an array of strings
 *
 * Since: 2.30
 **/
gchar **
g_variant_dup_objv (GVariant *value,
                    gsize    *length)
{
  gchar **strv;
  gsize n;
  gsize i;

  TYPE_CHECK (value, G_VARIANT_TYPE_OBJECT_PATH_ARRAY, NULL);

  n = g_variant_n_children (value);
  strv = g_new (gchar *, n + 1);

  for (i = 0; i < n; i++)
    {
      GVariant *string;

      string = g_variant_get_child_value (value, i);
      strv[i] = g_variant_dup_string (string, NULL);
      g_variant_unref (string);
    }
  strv[i] = NULL;

  if (length)
    *length = n;

  return strv;
}


/**
 * g_variant_new_bytestring:
 * @string: (array zero-terminated=1) (element-type guint8): a normal
 *          nul-terminated string in no particular encoding
 *
 * Creates an array-of-bytes #GVariant with the contents of @string.
 * This function is just like g_variant_new_string() except that the
 * string need not be valid UTF-8.
 *
 * The nul terminator character at the end of the string is stored in
 * the array.
 *
 * Returns: (transfer none): a floating reference to a new bytestring #GVariant instance
 *
 * Since: 2.26
 **/
GVariant *
g_variant_new_bytestring (const gchar *string)
{
  g_return_val_if_fail (string != NULL, NULL);

  return g_variant_new_from_trusted (G_VARIANT_TYPE_BYTESTRING,
                                     string, strlen (string) + 1);
}

/**
 * g_variant_get_bytestring:
 * @value: an array-of-bytes #GVariant instance
 *
 * Returns the string value of a #GVariant instance with an
 * array-of-bytes type.  The string has no particular encoding.
 *
 * If the array does not end with a nul terminator character, the empty
 * string is returned.  For this reason, you can always trust that a
 * non-%NULL nul-terminated string will be returned by this function.
 *
 * If the array contains a nul terminator character somewhere other than
 * the last byte then the returned string is the string, up to the first
 * such nul character.
 *
 * g_variant_get_fixed_array() should be used instead if the array contains
 * arbitrary data that could not be nul-terminated or could contain nul bytes.
 *
 * It is an error to call this function with a @value that is not an
 * array of bytes.
 *
 * The return value remains valid as long as @value exists.
 *
 * Returns: (transfer none) (array zero-terminated=1) (element-type guint8):
 *          the constant string
 *
 * Since: 2.26
 **/
const gchar *
g_variant_get_bytestring (GVariant *value)
{
  const gchar *string;
  gsize size;

  TYPE_CHECK (value, G_VARIANT_TYPE_BYTESTRING, NULL);

  /* Won't be NULL since this is an array type */
  string = g_variant_get_data (value);
  size = g_variant_get_size (value);

  if (size && string[size - 1] == '\0')
    return string;
  else
    return "";
}

/**
 * g_variant_dup_bytestring:
 * @value: an array-of-bytes #GVariant instance
 * @length: (out) (optional) (default NULL): a pointer to a #gsize, to store
 *          the length (not including the nul terminator)
 *
 * Similar to g_variant_get_bytestring() except that instead of
 * returning a constant string, the string is duplicated.
 *
 * The return value must be freed using g_free().
 *
 * Returns: (transfer full) (array zero-terminated=1 length=length) (element-type guint8):
 *          a newly allocated string
 *
 * Since: 2.26
 **/
gchar *
g_variant_dup_bytestring (GVariant *value,
                          gsize    *length)
{
  const gchar *original = g_variant_get_bytestring (value);
  gsize size;

  /* don't crash in case get_bytestring() had an assert failure */
  if (original == NULL)
    return NULL;

  size = strlen (original);

  if (length)
    *length = size;

  return g_memdup (original, size + 1);
}

/**
 * g_variant_new_bytestring_array:
 * @strv: (array length=length): an array of strings
 * @length: the length of @strv, or -1
 *
 * Constructs an array of bytestring #GVariant from the given array of
 * strings.
 *
 * If @length is -1 then @strv is %NULL-terminated.
 *
 * Returns: (transfer none): a new floating #GVariant instance
 *
 * Since: 2.26
 **/
GVariant *
g_variant_new_bytestring_array (const gchar * const *strv,
                                gssize               length)
{
  GVariant **strings;
  gsize i;

  g_return_val_if_fail (length == 0 || strv != NULL, NULL);

  if (length < 0)
    length = g_strv_length ((gchar **) strv);

  strings = g_new (GVariant *, length);
  for (i = 0; i < length; i++)
    strings[i] = g_variant_ref_sink (g_variant_new_bytestring (strv[i]));

  return g_variant_new_from_children (G_VARIANT_TYPE_BYTESTRING_ARRAY,
                                      strings, length, TRUE);
}

/**
 * g_variant_get_bytestring_array:
 * @value: an array of array of bytes #GVariant ('aay')
 * @length: (out) (optional): the length of the result, or %NULL
 *
 * Gets the contents of an array of array of bytes #GVariant.  This call
 * makes a shallow copy; the return result should be released with
 * g_free(), but the individual strings must not be modified.
 *
 * If @length is non-%NULL then the number of elements in the result is
 * stored there.  In any case, the resulting array will be
 * %NULL-terminated.
 *
 * For an empty array, @length will be set to 0 and a pointer to a
 * %NULL pointer will be returned.
 *
 * Returns: (array length=length) (transfer container): an array of constant strings
 *
 * Since: 2.26
 **/
const gchar **
g_variant_get_bytestring_array (GVariant *value,
                                gsize    *length)
{
  const gchar **strv;
  gsize n;
  gsize i;

  TYPE_CHECK (value, G_VARIANT_TYPE_BYTESTRING_ARRAY, NULL);

  g_variant_get_data (value);
  n = g_variant_n_children (value);
  strv = g_new (const gchar *, n + 1);

  for (i = 0; i < n; i++)
    {
      GVariant *string;

      string = g_variant_get_child_value (value, i);
      strv[i] = g_variant_get_bytestring (string);
      g_variant_unref (string);
    }
  strv[i] = NULL;

  if (length)
    *length = n;

  return strv;
}

/**
 * g_variant_dup_bytestring_array:
 * @value: an array of array of bytes #GVariant ('aay')
 * @length: (out) (optional): the length of the result, or %NULL
 *
 * Gets the contents of an array of array of bytes #GVariant.  This call
 * makes a deep copy; the return result should be released with
 * g_strfreev().
 *
 * If @length is non-%NULL then the number of elements in the result is
 * stored there.  In any case, the resulting array will be
 * %NULL-terminated.
 *
 * For an empty array, @length will be set to 0 and a pointer to a
 * %NULL pointer will be returned.
 *
 * Returns: (array length=length) (transfer full): an array of strings
 *
 * Since: 2.26
 **/
gchar **
g_variant_dup_bytestring_array (GVariant *value,
                                gsize    *length)
{
  gchar **strv;
  gsize n;
  gsize i;

  TYPE_CHECK (value, G_VARIANT_TYPE_BYTESTRING_ARRAY, NULL);

  g_variant_get_data (value);
  n = g_variant_n_children (value);
  strv = g_new (gchar *, n + 1);

  for (i = 0; i < n; i++)
    {
      GVariant *string;

      string = g_variant_get_child_value (value, i);
      strv[i] = g_variant_dup_bytestring (string, NULL);
      g_variant_unref (string);
    }
  strv[i] = NULL;

  if (length)
    *length = n;

  return strv;
}

/* Type checking and querying {{{1 */
/**
 * g_variant_get_type:
 * @value: a #GVariant
 *
 * Determines the type of @value.
 *
 * The return value is valid for the lifetime of @value and must not
 * be freed.
 *
 * Returns: a #GVariantType
 *
 * Since: 2.24
 **/
const GVariantType *
g_variant_get_type (GVariant *value)
{
  GVariantTypeInfo *type_info;

  g_return_val_if_fail (value != NULL, NULL);

  type_info = g_variant_get_type_info (value);

  return (GVariantType *) g_variant_type_info_get_type_string (type_info);
}

/**
 * g_variant_get_type_string:
 * @value: a #GVariant
 *
 * Returns the type string of @value.  Unlike the result of calling
 * g_variant_type_peek_string(), this string is nul-terminated.  This
 * string belongs to #GVariant and must not be freed.
 *
 * Returns: the type string for the type of @value
 *
 * Since: 2.24
 **/
const gchar *
g_variant_get_type_string (GVariant *value)
{
  GVariantTypeInfo *type_info;

  g_return_val_if_fail (value != NULL, NULL);

  type_info = g_variant_get_type_info (value);

  return g_variant_type_info_get_type_string (type_info);
}

/**
 * g_variant_is_of_type:
 * @value: a #GVariant instance
 * @type: a #GVariantType
 *
 * Checks if a value has a type matching the provided type.
 *
 * Returns: %TRUE if the type of @value matches @type
 *
 * Since: 2.24
 **/
gboolean
g_variant_is_of_type (GVariant           *value,
                      const GVariantType *type)
{
  return g_variant_type_is_subtype_of (g_variant_get_type (value), type);
}

/**
 * g_variant_is_container:
 * @value: a #GVariant instance
 *
 * Checks if @value is a container.
 *
 * Returns: %TRUE if @value is a container
 *
 * Since: 2.24
 */
gboolean
g_variant_is_container (GVariant *value)
{
  return g_variant_type_is_container (g_variant_get_type (value));
}


/**
 * g_variant_classify:
 * @value: a #GVariant
 *
 * Classifies @value according to its top-level type.
 *
 * Returns: the #GVariantClass of @value
 *
 * Since: 2.24
 **/
/**
 * GVariantClass:
 * @G_VARIANT_CLASS_BOOLEAN: The #GVariant is a boolean.
 * @G_VARIANT_CLASS_BYTE: The #GVariant is a byte.
 * @G_VARIANT_CLASS_INT16: The #GVariant is a signed 16 bit integer.
 * @G_VARIANT_CLASS_UINT16: The #GVariant is an unsigned 16 bit integer.
 * @G_VARIANT_CLASS_INT32: The #GVariant is a signed 32 bit integer.
 * @G_VARIANT_CLASS_UINT32: The #GVariant is an unsigned 32 bit integer.
 * @G_VARIANT_CLASS_INT64: The #GVariant is a signed 64 bit integer.
 * @G_VARIANT_CLASS_UINT64: The #GVariant is an unsigned 64 bit integer.
 * @G_VARIANT_CLASS_HANDLE: The #GVariant is a file handle index.
 * @G_VARIANT_CLASS_DOUBLE: The #GVariant is a double precision floating 
 *                          point value.
 * @G_VARIANT_CLASS_STRING: The #GVariant is a normal string.
 * @G_VARIANT_CLASS_OBJECT_PATH: The #GVariant is a D-Bus object path 
 *                               string.
 * @G_VARIANT_CLASS_SIGNATURE: The #GVariant is a D-Bus signature string.
 * @G_VARIANT_CLASS_VARIANT: The #GVariant is a variant.
 * @G_VARIANT_CLASS_MAYBE: The #GVariant is a maybe-typed value.
 * @G_VARIANT_CLASS_ARRAY: The #GVariant is an array.
 * @G_VARIANT_CLASS_TUPLE: The #GVariant is a tuple.
 * @G_VARIANT_CLASS_DICT_ENTRY: The #GVariant is a dictionary entry.
 *
 * The range of possible top-level types of #GVariant instances.
 *
 * Since: 2.24
 **/
GVariantClass
g_variant_classify (GVariant *value)
{
  g_return_val_if_fail (value != NULL, 0);

  return *g_variant_get_type_string (value);
}

/* Pretty printer {{{1 */
/* This function is not introspectable because if @string is NULL,
   @returns is (transfer full), otherwise it is (transfer none), which
   is not supported by GObjectIntrospection */
/**
 * g_variant_print_string: (skip)
 * @value: a #GVariant
 * @string: (nullable) (default NULL): a #GString, or %NULL
 * @type_annotate: %TRUE if type information should be included in
 *                 the output
 *
 * Behaves as g_variant_print(), but operates on a #GString.
 *
 * If @string is non-%NULL then it is appended to and returned.  Else,
 * a new empty #GString is allocated and it is returned.
 *
 * Returns: a #GString containing the string
 *
 * Since: 2.24
 **/
GString *
g_variant_print_string (GVariant *value,
                        GString  *string,
                        gboolean  type_annotate)
{
  if G_UNLIKELY (string == NULL)
    string = g_string_new (NULL);

  switch (g_variant_classify (value))
    {
    case G_VARIANT_CLASS_MAYBE:
      if (type_annotate)
        g_string_append_printf (string, "@%s ",
                                g_variant_get_type_string (value));

      if (g_variant_n_children (value))
        {
          gchar *printed_child;
          GVariant *element;

          /* Nested maybes:
           *
           * Consider the case of the type "mmi".  In this case we could
           * write "just just 4", but "4" alone is totally unambiguous,
           * so we try to drop "just" where possible.
           *
           * We have to be careful not to always drop "just", though,
           * since "nothing" needs to be distinguishable from "just
           * nothing".  The case where we need to ensure we keep the
           * "just" is actually exactly the case where we have a nested
           * Nothing.
           *
           * Instead of searching for that nested Nothing, we just print
           * the contained value into a separate string and see if we
           * end up with "nothing" at the end of it.  If so, we need to
           * add "just" at our level.
           */
          element = g_variant_get_child_value (value, 0);
          printed_child = g_variant_print (element, FALSE);
          g_variant_unref (element);

          if (g_str_has_suffix (printed_child, "nothing"))
            g_string_append (string, "just ");
          g_string_append (string, printed_child);
          g_free (printed_child);
        }
      else
        g_string_append (string, "nothing");

      break;

    case G_VARIANT_CLASS_ARRAY:
      /* it's an array so the first character of the type string is 'a'
       *
       * if the first two characters are 'ay' then it's a bytestring.
       * under certain conditions we print those as strings.
       */
      if (g_variant_get_type_string (value)[1] == 'y')
        {
          const gchar *str;
          gsize size;
          gsize i;

          /* first determine if it is a byte string.
           * that's when there's a single nul character: at the end.
           */
          str = g_variant_get_data (value);
          size = g_variant_get_size (value);

          for (i = 0; i < size; i++)
            if (str[i] == '\0')
              break;

          /* first nul byte is the last byte -> it's a byte string. */
          if (i == size - 1)
            {
              gchar *escaped = g_strescape (str, NULL);

              /* use double quotes only if a ' is in the string */
              if (strchr (str, '\''))
                g_string_append_printf (string, "b\"%s\"", escaped);
              else
                g_string_append_printf (string, "b'%s'", escaped);

              g_free (escaped);
              break;
            }

          else
            {
              /* fall through and handle normally... */
            }
        }

      /*
       * if the first two characters are 'a{' then it's an array of
       * dictionary entries (ie: a dictionary) so we print that
       * differently.
       */
      if (g_variant_get_type_string (value)[1] == '{')
        /* dictionary */
        {
          const gchar *comma = "";
          gsize n, i;

          if ((n = g_variant_n_children (value)) == 0)
            {
              if (type_annotate)
                g_string_append_printf (string, "@%s ",
                                        g_variant_get_type_string (value));
              g_string_append (string, "{}");
              break;
            }

          g_string_append_c (string, '{');
          for (i = 0; i < n; i++)
            {
              GVariant *entry, *key, *val;

              g_string_append (string, comma);
              comma = ", ";

              entry = g_variant_get_child_value (value, i);
              key = g_variant_get_child_value (entry, 0);
              val = g_variant_get_child_value (entry, 1);
              g_variant_unref (entry);

              g_variant_print_string (key, string, type_annotate);
              g_variant_unref (key);
              g_string_append (string, ": ");
              g_variant_print_string (val, string, type_annotate);
              g_variant_unref (val);
              type_annotate = FALSE;
            }
          g_string_append_c (string, '}');
        }
      else
        /* normal (non-dictionary) array */
        {
          const gchar *comma = "";
          gsize n, i;

          if ((n = g_variant_n_children (value)) == 0)
            {
              if (type_annotate)
                g_string_append_printf (string, "@%s ",
                                        g_variant_get_type_string (value));
              g_string_append (string, "[]");
              break;
            }

          g_string_append_c (string, '[');
          for (i = 0; i < n; i++)
            {
              GVariant *element;

              g_string_append (string, comma);
              comma = ", ";

              element = g_variant_get_child_value (value, i);

              g_variant_print_string (element, string, type_annotate);
              g_variant_unref (element);
              type_annotate = FALSE;
            }
          g_string_append_c (string, ']');
        }

      break;

    case G_VARIANT_CLASS_TUPLE:
      {
        gsize n, i;

        n = g_variant_n_children (value);

        g_string_append_c (string, '(');
        for (i = 0; i < n; i++)
          {
            GVariant *element;

            element = g_variant_get_child_value (value, i);
            g_variant_print_string (element, string, type_annotate);
            g_string_append (string, ", ");
            g_variant_unref (element);
          }

        /* for >1 item:  remove final ", "
         * for 1 item:   remove final " ", but leave the ","
         * for 0 items:  there is only "(", so remove nothing
         */
        g_string_truncate (string, string->len - (n > 0) - (n > 1));
        g_string_append_c (string, ')');
      }
      break;

    case G_VARIANT_CLASS_DICT_ENTRY:
      {
        GVariant *element;

        g_string_append_c (string, '{');

        element = g_variant_get_child_value (value, 0);
        g_variant_print_string (element, string, type_annotate);
        g_variant_unref (element);

        g_string_append (string, ", ");

        element = g_variant_get_child_value (value, 1);
        g_variant_print_string (element, string, type_annotate);
        g_variant_unref (element);

        g_string_append_c (string, '}');
      }
      break;

    case G_VARIANT_CLASS_VARIANT:
      {
        GVariant *child = g_variant_get_variant (value);

        /* Always annotate types in nested variants, because they are
         * (by nature) of variable type.
         */
        g_string_append_c (string, '<');
        g_variant_print_string (child, string, TRUE);
        g_string_append_c (string, '>');

        g_variant_unref (child);
      }
      break;

    case G_VARIANT_CLASS_BOOLEAN:
      if (g_variant_get_boolean (value))
        g_string_append (string, "true");
      else
        g_string_append (string, "false");
      break;

    case G_VARIANT_CLASS_STRING:
      {
        const gchar *str = g_variant_get_string (value, NULL);
        gunichar quote = strchr (str, '\'') ? '"' : '\'';

        g_string_append_c (string, quote);

        while (*str)
          {
            gunichar c = g_utf8_get_char (str);

            if (c == quote || c == '\\')
              g_string_append_c (string, '\\');

            if (g_unichar_isprint (c))
              g_string_append_unichar (string, c);

            else
              {
                g_string_append_c (string, '\\');
                if (c < 0x10000)
                  switch (c)
                    {
                    case '\a':
                      g_string_append_c (string, 'a');
                      break;

                    case '\b':
                      g_string_append_c (string, 'b');
                      break;

                    case '\f':
                      g_string_append_c (string, 'f');
                      break;

                    case '\n':
                      g_string_append_c (string, 'n');
                      break;

                    case '\r':
                      g_string_append_c (string, 'r');
                      break;

                    case '\t':
                      g_string_append_c (string, 't');
                      break;

                    case '\v':
                      g_string_append_c (string, 'v');
                      break;

                    default:
                      g_string_append_printf (string, "u%04x", c);
                      break;
                    }
                 else
                   g_string_append_printf (string, "U%08x", c);
              }

            str = g_utf8_next_char (str);
          }

        g_string_append_c (string, quote);
      }
      break;

    case G_VARIANT_CLASS_BYTE:
      if (type_annotate)
        g_string_append (string, "byte ");
      g_string_append_printf (string, "0x%02x",
                              g_variant_get_byte (value));
      break;

    case G_VARIANT_CLASS_INT16:
      if (type_annotate)
        g_string_append (string, "int16 ");
      g_string_append_printf (string, "%"G_GINT16_FORMAT,
                              g_variant_get_int16 (value));
      break;

    case G_VARIANT_CLASS_UINT16:
      if (type_annotate)
        g_string_append (string, "uint16 ");
      g_string_append_printf (string, "%"G_GUINT16_FORMAT,
                              g_variant_get_uint16 (value));
      break;

    case G_VARIANT_CLASS_INT32:
      /* Never annotate this type because it is the default for numbers
       * (and this is a *pretty* printer)
       */
      g_string_append_printf (string, "%"G_GINT32_FORMAT,
                              g_variant_get_int32 (value));
      break;

    case G_VARIANT_CLASS_HANDLE:
      if (type_annotate)
        g_string_append (string, "handle ");
      g_string_append_printf (string, "%"G_GINT32_FORMAT,
                              g_variant_get_handle (value));
      break;

    case G_VARIANT_CLASS_UINT32:
      if (type_annotate)
        g_string_append (string, "uint32 ");
      g_string_append_printf (string, "%"G_GUINT32_FORMAT,
                              g_variant_get_uint32 (value));
      break;

    case G_VARIANT_CLASS_INT64:
      if (type_annotate)
        g_string_append (string, "int64 ");
      g_string_append_printf (string, "%"G_GINT64_FORMAT,
                              g_variant_get_int64 (value));
      break;

    case G_VARIANT_CLASS_UINT64:
      if (type_annotate)
        g_string_append (string, "uint64 ");
      g_string_append_printf (string, "%"G_GUINT64_FORMAT,
                              g_variant_get_uint64 (value));
      break;

    case G_VARIANT_CLASS_DOUBLE:
      {
        gchar buffer[100];
        gint i;

        g_ascii_dtostr (buffer, sizeof buffer, g_variant_get_double (value));

        for (i = 0; buffer[i]; i++)
          if (buffer[i] == '.' || buffer[i] == 'e' ||
              buffer[i] == 'n' || buffer[i] == 'N')
            break;

        /* if there is no '.' or 'e' in the float then add one */
        if (buffer[i] == '\0')
          {
            buffer[i++] = '.';
            buffer[i++] = '0';
            buffer[i++] = '\0';
          }

        g_string_append (string, buffer);
      }
      break;

    case G_VARIANT_CLASS_OBJECT_PATH:
      if (type_annotate)
        g_string_append (string, "objectpath ");
      g_string_append_printf (string, "\'%s\'",
                              g_variant_get_string (value, NULL));
      break;

    case G_VARIANT_CLASS_SIGNATURE:
      if (type_annotate)
        g_string_append (string, "signature ");
      g_string_append_printf (string, "\'%s\'",
                              g_variant_get_string (value, NULL));
      break;

    default:
      g_assert_not_reached ();
  }

  return string;
}

/**
 * g_variant_print:
 * @value: a #GVariant
 * @type_annotate: %TRUE if type information should be included in
 *                 the output
 *
 * Pretty-prints @value in the format understood by g_variant_parse().
 *
 * The format is described [here][gvariant-text].
 *
 * If @type_annotate is %TRUE, then type information is included in
 * the output.
 *
 * Returns: (transfer full): a newly-allocated string holding the result.
 *
 * Since: 2.24
 */
gchar *
g_variant_print (GVariant *value,
                 gboolean  type_annotate)
{
  return g_string_free (g_variant_print_string (value, NULL, type_annotate),
                        FALSE);
}

/* Hash, Equal, Compare {{{1 */
/**
 * g_variant_hash:
 * @value: (type GVariant): a basic #GVariant value as a #gconstpointer
 *
 * Generates a hash value for a #GVariant instance.
 *
 * The output of this function is guaranteed to be the same for a given
 * value only per-process.  It may change between different processor
 * architectures or even different versions of GLib.  Do not use this
 * function as a basis for building protocols or file formats.
 *
 * The type of @value is #gconstpointer only to allow use of this
 * function with #GHashTable.  @value must be a #GVariant.
 *
 * Returns: a hash value corresponding to @value
 *
 * Since: 2.24
 **/
guint
g_variant_hash (gconstpointer value_)
{
  GVariant *value = (GVariant *) value_;

  switch (g_variant_classify (value))
    {
    case G_VARIANT_CLASS_STRING:
    case G_VARIANT_CLASS_OBJECT_PATH:
    case G_VARIANT_CLASS_SIGNATURE:
      return g_str_hash (g_variant_get_string (value, NULL));

    case G_VARIANT_CLASS_BOOLEAN:
      /* this is a very odd thing to hash... */
      return g_variant_get_boolean (value);

    case G_VARIANT_CLASS_BYTE:
      return g_variant_get_byte (value);

    case G_VARIANT_CLASS_INT16:
    case G_VARIANT_CLASS_UINT16:
      {
        const guint16 *ptr;

        ptr = g_variant_get_data (value);

        if (ptr)
          return *ptr;
        else
          return 0;
      }

    case G_VARIANT_CLASS_INT32:
    case G_VARIANT_CLASS_UINT32:
    case G_VARIANT_CLASS_HANDLE:
      {
        const guint *ptr;

        ptr = g_variant_get_data (value);

        if (ptr)
          return *ptr;
        else
          return 0;
      }

    case G_VARIANT_CLASS_INT64:
    case G_VARIANT_CLASS_UINT64:
    case G_VARIANT_CLASS_DOUBLE:
      /* need a separate case for these guys because otherwise
       * performance could be quite bad on big endian systems
       */
      {
        const guint *ptr;

        ptr = g_variant_get_data (value);

        if (ptr)
          return ptr[0] + ptr[1];
        else
          return 0;
      }

    default:
      g_return_val_if_fail (!g_variant_is_container (value), 0);
      g_assert_not_reached ();
    }
}

/**
 * g_variant_equal:
 * @one: (type GVariant): a #GVariant instance
 * @two: (type GVariant): a #GVariant instance
 *
 * Checks if @one and @two have the same type and value.
 *
 * The types of @one and @two are #gconstpointer only to allow use of
 * this function with #GHashTable.  They must each be a #GVariant.
 *
 * Returns: %TRUE if @one and @two are equal
 *
 * Since: 2.24
 **/
gboolean
g_variant_equal (gconstpointer one,
                 gconstpointer two)
{
  gboolean equal;

  g_return_val_if_fail (one != NULL && two != NULL, FALSE);

  if (g_variant_get_type_info ((GVariant *) one) !=
      g_variant_get_type_info ((GVariant *) two))
    return FALSE;

  /* if both values are trusted to be in their canonical serialised form
   * then a simple memcmp() of their serialised data will answer the
   * question.
   *
   * if not, then this might generate a false negative (since it is
   * possible for two different byte sequences to represent the same
   * value).  for now we solve this by pretty-printing both values and
   * comparing the result.
   */
  if (g_variant_is_trusted ((GVariant *) one) &&
      g_variant_is_trusted ((GVariant *) two))
    {
      gconstpointer data_one, data_two;
      gsize size_one, size_two;

      size_one = g_variant_get_size ((GVariant *) one);
      size_two = g_variant_get_size ((GVariant *) two);

      if (size_one != size_two)
        return FALSE;

      data_one = g_variant_get_data ((GVariant *) one);
      data_two = g_variant_get_data ((GVariant *) two);

      equal = memcmp (data_one, data_two, size_one) == 0;
    }
  else
    {
      gchar *strone, *strtwo;

      strone = g_variant_print ((GVariant *) one, FALSE);
      strtwo = g_variant_print ((GVariant *) two, FALSE);
      equal = strcmp (strone, strtwo) == 0;
      g_free (strone);
      g_free (strtwo);
    }

  return equal;
}

/**
 * g_variant_compare:
 * @one: (type GVariant): a basic-typed #GVariant instance
 * @two: (type GVariant): a #GVariant instance of the same type
 *
 * Compares @one and @two.
 *
 * The types of @one and @two are #gconstpointer only to allow use of
 * this function with #GTree, #GPtrArray, etc.  They must each be a
 * #GVariant.
 *
 * Comparison is only defined for basic types (ie: booleans, numbers,
 * strings).  For booleans, %FALSE is less than %TRUE.  Numbers are
 * ordered in the usual way.  Strings are in ASCII lexographical order.
 *
 * It is a programmer error to attempt to compare container values or
 * two values that have types that are not exactly equal.  For example,
 * you cannot compare a 32-bit signed integer with a 32-bit unsigned
 * integer.  Also note that this function is not particularly
 * well-behaved when it comes to comparison of doubles; in particular,
 * the handling of incomparable values (ie: NaN) is undefined.
 *
 * If you only require an equality comparison, g_variant_equal() is more
 * general.
 *
 * Returns: negative value if a < b;
 *          zero if a = b;
 *          positive value if a > b.
 *
 * Since: 2.26
 **/
gint
g_variant_compare (gconstpointer one,
                   gconstpointer two)
{
  GVariant *a = (GVariant *) one;
  GVariant *b = (GVariant *) two;

  g_return_val_if_fail (g_variant_classify (a) == g_variant_classify (b), 0);

  switch (g_variant_classify (a))
    {
    case G_VARIANT_CLASS_BOOLEAN:
      return g_variant_get_boolean (a) -
             g_variant_get_boolean (b);

    case G_VARIANT_CLASS_BYTE:
      return ((gint) g_variant_get_byte (a)) -
             ((gint) g_variant_get_byte (b));

    case G_VARIANT_CLASS_INT16:
      return ((gint) g_variant_get_int16 (a)) -
             ((gint) g_variant_get_int16 (b));

    case G_VARIANT_CLASS_UINT16:
      return ((gint) g_variant_get_uint16 (a)) -
             ((gint) g_variant_get_uint16 (b));

    case G_VARIANT_CLASS_INT32:
      {
        gint32 a_val = g_variant_get_int32 (a);
        gint32 b_val = g_variant_get_int32 (b);

        return (a_val == b_val) ? 0 : (a_val > b_val) ? 1 : -1;
      }

    case G_VARIANT_CLASS_UINT32:
      {
        guint32 a_val = g_variant_get_uint32 (a);
        guint32 b_val = g_variant_get_uint32 (b);

        return (a_val == b_val) ? 0 : (a_val > b_val) ? 1 : -1;
      }

    case G_VARIANT_CLASS_INT64:
      {
        gint64 a_val = g_variant_get_int64 (a);
        gint64 b_val = g_variant_get_int64 (b);

        return (a_val == b_val) ? 0 : (a_val > b_val) ? 1 : -1;
      }

    case G_VARIANT_CLASS_UINT64:
      {
        guint64 a_val = g_variant_get_uint64 (a);
        guint64 b_val = g_variant_get_uint64 (b);

        return (a_val == b_val) ? 0 : (a_val > b_val) ? 1 : -1;
      }

    case G_VARIANT_CLASS_DOUBLE:
      {
        gdouble a_val = g_variant_get_double (a);
        gdouble b_val = g_variant_get_double (b);

        return (a_val == b_val) ? 0 : (a_val > b_val) ? 1 : -1;
      }

    case G_VARIANT_CLASS_STRING:
    case G_VARIANT_CLASS_OBJECT_PATH:
    case G_VARIANT_CLASS_SIGNATURE:
      return strcmp (g_variant_get_string (a, NULL),
                     g_variant_get_string (b, NULL));

    default:
      g_return_val_if_fail (!g_variant_is_container (a), 0);
      g_assert_not_reached ();
    }
}

/* GVariantIter {{{1 */
/**
 * GVariantIter: (skip)
 *
 * #GVariantIter is an opaque data structure and can only be accessed
 * using the following functions.
 **/
struct stack_iter
{
  GVariant *value;
  gssize n, i;

  const gchar *loop_format;

  gsize padding[3];
  gsize magic;
};

G_STATIC_ASSERT (sizeof (struct stack_iter) <= sizeof (GVariantIter));

struct heap_iter
{
  struct stack_iter iter;

  GVariant *value_ref;
  gsize magic;
};

#define GVSI(i)                 ((struct stack_iter *) (i))
#define GVHI(i)                 ((struct heap_iter *) (i))
#define GVSI_MAGIC              ((gsize) 3579507750u)
#define GVHI_MAGIC              ((gsize) 1450270775u)
#define is_valid_iter(i)        (i != NULL && \
                                 GVSI(i)->magic == GVSI_MAGIC)
#define is_valid_heap_iter(i)   (is_valid_iter(i) && \
                                 GVHI(i)->magic == GVHI_MAGIC)

/**
 * g_variant_iter_new:
 * @value: a container #GVariant
 *
 * Creates a heap-allocated #GVariantIter for iterating over the items
 * in @value.
 *
 * Use g_variant_iter_free() to free the return value when you no longer
 * need it.
 *
 * A reference is taken to @value and will be released only when
 * g_variant_iter_free() is called.
 *
 * Returns: (transfer full): a new heap-allocated #GVariantIter
 *
 * Since: 2.24
 **/
GVariantIter *
g_variant_iter_new (GVariant *value)
{
  GVariantIter *iter;

  iter = (GVariantIter *) g_slice_new (struct heap_iter);
  GVHI(iter)->value_ref = g_variant_ref (value);
  GVHI(iter)->magic = GVHI_MAGIC;

  g_variant_iter_init (iter, value);

  return iter;
}

/**
 * g_variant_iter_init: (skip)
 * @iter: a pointer to a #GVariantIter
 * @value: a container #GVariant
 *
 * Initialises (without allocating) a #GVariantIter.  @iter may be
 * completely uninitialised prior to this call; its old value is
 * ignored.
 *
 * The iterator remains valid for as long as @value exists, and need not
 * be freed in any way.
 *
 * Returns: the number of items in @value
 *
 * Since: 2.24
 **/
gsize
g_variant_iter_init (GVariantIter *iter,
                     GVariant     *value)
{
  GVSI(iter)->magic = GVSI_MAGIC;
  GVSI(iter)->value = value;
  GVSI(iter)->n = g_variant_n_children (value);
  GVSI(iter)->i = -1;
  GVSI(iter)->loop_format = NULL;

  return GVSI(iter)->n;
}

/**
 * g_variant_iter_copy:
 * @iter: a #GVariantIter
 *
 * Creates a new heap-allocated #GVariantIter to iterate over the
 * container that was being iterated over by @iter.  Iteration begins on
 * the new iterator from the current position of the old iterator but
 * the two copies are independent past that point.
 *
 * Use g_variant_iter_free() to free the return value when you no longer
 * need it.
 *
 * A reference is taken to the container that @iter is iterating over
 * and will be releated only when g_variant_iter_free() is called.
 *
 * Returns: (transfer full): a new heap-allocated #GVariantIter
 *
 * Since: 2.24
 **/
GVariantIter *
g_variant_iter_copy (GVariantIter *iter)
{
  GVariantIter *copy;

  g_return_val_if_fail (is_valid_iter (iter), 0);

  copy = g_variant_iter_new (GVSI(iter)->value);
  GVSI(copy)->i = GVSI(iter)->i;

  return copy;
}

/**
 * g_variant_iter_n_children:
 * @iter: a #GVariantIter
 *
 * Queries the number of child items in the container that we are
 * iterating over.  This is the total number of items -- not the number
 * of items remaining.
 *
 * This function might be useful for preallocation of arrays.
 *
 * Returns: the number of children in the container
 *
 * Since: 2.24
 **/
gsize
g_variant_iter_n_children (GVariantIter *iter)
{
  g_return_val_if_fail (is_valid_iter (iter), 0);

  return GVSI(iter)->n;
}

/**
 * g_variant_iter_free:
 * @iter: (transfer full): a heap-allocated #GVariantIter
 *
 * Frees a heap-allocated #GVariantIter.  Only call this function on
 * iterators that were returned by g_variant_iter_new() or
 * g_variant_iter_copy().
 *
 * Since: 2.24
 **/
void
g_variant_iter_free (GVariantIter *iter)
{
  g_return_if_fail (is_valid_heap_iter (iter));

  g_variant_unref (GVHI(iter)->value_ref);
  GVHI(iter)->magic = 0;

  g_slice_free (struct heap_iter, GVHI(iter));
}

/**
 * g_variant_iter_next_value:
 * @iter: a #GVariantIter
 *
 * Gets the next item in the container.  If no more items remain then
 * %NULL is returned.
 *
 * Use g_variant_unref() to drop your reference on the return value when
 * you no longer need it.
 *
 * Here is an example for iterating with g_variant_iter_next_value():
 * |[<!-- language="C" --> 
 *   // recursively iterate a container
 *   void
 *   iterate_container_recursive (GVariant *container)
 *   {
 *     GVariantIter iter;
 *     GVariant *child;
 *
 *     g_variant_iter_init (&iter, container);
 *     while ((child = g_variant_iter_next_value (&iter)))
 *       {
 *         g_print ("type '%s'\n", g_variant_get_type_string (child));
 *
 *         if (g_variant_is_container (child))
 *           iterate_container_recursive (child);
 *
 *         g_variant_unref (child);
 *       }
 *   }
 * ]|
 *
 * Returns: (nullable) (transfer full): a #GVariant, or %NULL
 *
 * Since: 2.24
 **/
GVariant *
g_variant_iter_next_value (GVariantIter *iter)
{
  g_return_val_if_fail (is_valid_iter (iter), FALSE);

  if G_UNLIKELY (GVSI(iter)->i >= GVSI(iter)->n)
    {
      g_critical ("g_variant_iter_next_value: must not be called again "
                  "after NULL has already been returned.");
      return NULL;
    }

  GVSI(iter)->i++;

  if (GVSI(iter)->i < GVSI(iter)->n)
    return g_variant_get_child_value (GVSI(iter)->value, GVSI(iter)->i);

  return NULL;
}

/* GVariantBuilder {{{1 */
/**
 * GVariantBuilder:
 *
 * A utility type for constructing container-type #GVariant instances.
 *
 * This is an opaque structure and may only be accessed using the
 * following functions.
 *
 * #GVariantBuilder is not threadsafe in any way.  Do not attempt to
 * access it from more than one thread.
 **/

struct stack_builder
{
  GVariantBuilder *parent;
  GVariantType *type;

  /* type constraint explicitly specified by 'type'.
   * for tuple types, this moves along as we add more items.
   */
  const GVariantType *expected_type;

  /* type constraint implied by previous array item.
   */
  const GVariantType *prev_item_type;

  /* constraints on the number of children.  max = -1 for unlimited. */
  gsize min_items;
  gsize max_items;

  /* dynamically-growing pointer array */
  GVariant **children;
  gsize allocated_children;
  gsize offset;

  /* set to '1' if all items in the container will have the same type
   * (ie: maybe, array, variant) '0' if not (ie: tuple, dict entry)
   */
  guint uniform_item_types : 1;

  /* set to '1' initially and changed to '0' if an untrusted value is
   * added
   */
  guint trusted : 1;

  gsize magic;
};

G_STATIC_ASSERT (sizeof (struct stack_builder) <= sizeof (GVariantBuilder));

struct heap_builder
{
  GVariantBuilder builder;
  gsize magic;

  gint ref_count;
};

#define GVSB(b)                  ((struct stack_builder *) (b))
#define GVHB(b)                  ((struct heap_builder *) (b))
#define GVSB_MAGIC               ((gsize) 1033660112u)
#define GVSB_MAGIC_PARTIAL       ((gsize) 2942751021u)
#define GVHB_MAGIC               ((gsize) 3087242682u)
#define is_valid_builder(b)      (b != NULL && \
                                  GVSB(b)->magic == GVSB_MAGIC)
#define is_valid_heap_builder(b) (GVHB(b)->magic == GVHB_MAGIC)

/* Just to make sure that by adding a union to GVariantBuilder, we
 * didn't accidentally change ABI. */
G_STATIC_ASSERT (sizeof (GVariantBuilder) == sizeof (gsize[16]));

static gboolean
ensure_valid_builder (GVariantBuilder *builder)
{
  if (is_valid_builder (builder))
    return TRUE;
  if (builder->u.s.partial_magic == GVSB_MAGIC_PARTIAL)
    {
      static GVariantBuilder cleared_builder;

      /* Make sure that only first two fields were set and the rest is
       * zeroed to avoid messing up the builder that had parent
       * address equal to GVSB_MAGIC_PARTIAL. */
      if (memcmp (cleared_builder.u.s.y, builder->u.s.y, sizeof cleared_builder.u.s.y))
        return FALSE;

      g_variant_builder_init (builder, builder->u.s.type);
    }
  return is_valid_builder (builder);
}

/**
 * g_variant_builder_new:
 * @type: a container type
 *
 * Allocates and initialises a new #GVariantBuilder.
 *
 * You should call g_variant_builder_unref() on the return value when it
 * is no longer needed.  The memory will not be automatically freed by
 * any other call.
 *
 * In most cases it is easier to place a #GVariantBuilder directly on
 * the stack of the calling function and initialise it with
 * g_variant_builder_init().
 *
 * Returns: (transfer full): a #GVariantBuilder
 *
 * Since: 2.24
 **/
GVariantBuilder *
g_variant_builder_new (const GVariantType *type)
{
  GVariantBuilder *builder;

  builder = (GVariantBuilder *) g_slice_new (struct heap_builder);
  g_variant_builder_init (builder, type);
  GVHB(builder)->magic = GVHB_MAGIC;
  GVHB(builder)->ref_count = 1;

  return builder;
}

/**
 * g_variant_builder_unref:
 * @builder: (transfer full): a #GVariantBuilder allocated by g_variant_builder_new()
 *
 * Decreases the reference count on @builder.
 *
 * In the event that there are no more references, releases all memory
 * associated with the #GVariantBuilder.
 *
 * Don't call this on stack-allocated #GVariantBuilder instances or bad
 * things will happen.
 *
 * Since: 2.24
 **/
void
g_variant_builder_unref (GVariantBuilder *builder)
{
  g_return_if_fail (is_valid_heap_builder (builder));

  if (--GVHB(builder)->ref_count)
    return;

  g_variant_builder_clear (builder);
  GVHB(builder)->magic = 0;

  g_slice_free (struct heap_builder, GVHB(builder));
}

/**
 * g_variant_builder_ref:
 * @builder: a #GVariantBuilder allocated by g_variant_builder_new()
 *
 * Increases the reference count on @builder.
 *
 * Don't call this on stack-allocated #GVariantBuilder instances or bad
 * things will happen.
 *
 * Returns: (transfer full): a new reference to @builder
 *
 * Since: 2.24
 **/
GVariantBuilder *
g_variant_builder_ref (GVariantBuilder *builder)
{
  g_return_val_if_fail (is_valid_heap_builder (builder), NULL);

  GVHB(builder)->ref_count++;

  return builder;
}

/**
 * g_variant_builder_clear: (skip)
 * @builder: a #GVariantBuilder
 *
 * Releases all memory associated with a #GVariantBuilder without
 * freeing the #GVariantBuilder structure itself.
 *
 * It typically only makes sense to do this on a stack-allocated
 * #GVariantBuilder if you want to abort building the value part-way
 * through.  This function need not be called if you call
 * g_variant_builder_end() and it also doesn't need to be called on
 * builders allocated with g_variant_builder_new() (see
 * g_variant_builder_unref() for that).
 *
 * This function leaves the #GVariantBuilder structure set to all-zeros.
 * It is valid to call this function on either an initialised
 * #GVariantBuilder or one that is set to all-zeros but it is not valid
 * to call this function on uninitialised memory.
 *
 * Since: 2.24
 **/
void
g_variant_builder_clear (GVariantBuilder *builder)
{
  gsize i;

  if (GVSB(builder)->magic == 0)
    /* all-zeros or partial case */
    return;

  g_return_if_fail (ensure_valid_builder (builder));

  g_variant_type_free (GVSB(builder)->type);

  for (i = 0; i < GVSB(builder)->offset; i++)
    g_variant_unref (GVSB(builder)->children[i]);

  g_free (GVSB(builder)->children);

  if (GVSB(builder)->parent)
    {
      g_variant_builder_clear (GVSB(builder)->parent);
      g_slice_free (GVariantBuilder, GVSB(builder)->parent);
    }

  memset (builder, 0, sizeof (GVariantBuilder));
}

/**
 * g_variant_builder_init: (skip)
 * @builder: a #GVariantBuilder
 * @type: a container type
 *
 * Initialises a #GVariantBuilder structure.
 *
 * @type must be non-%NULL.  It specifies the type of container to
 * construct.  It can be an indefinite type such as
 * %G_VARIANT_TYPE_ARRAY or a definite type such as "as" or "(ii)".
 * Maybe, array, tuple, dictionary entry and variant-typed values may be
 * constructed.
 *
 * After the builder is initialised, values are added using
 * g_variant_builder_add_value() or g_variant_builder_add().
 *
 * After all the child values are added, g_variant_builder_end() frees
 * the memory associated with the builder and returns the #GVariant that
 * was created.
 *
 * This function completely ignores the previous contents of @builder.
 * On one hand this means that it is valid to pass in completely
 * uninitialised memory.  On the other hand, this means that if you are
 * initialising over top of an existing #GVariantBuilder you need to
 * first call g_variant_builder_clear() in order to avoid leaking
 * memory.
 *
 * You must not call g_variant_builder_ref() or
 * g_variant_builder_unref() on a #GVariantBuilder that was initialised
 * with this function.  If you ever pass a reference to a
 * #GVariantBuilder outside of the control of your own code then you
 * should assume that the person receiving that reference may try to use
 * reference counting; you should use g_variant_builder_new() instead of
 * this function.
 *
 * Since: 2.24
 **/
void
g_variant_builder_init (GVariantBuilder    *builder,
                        const GVariantType *type)
{
  g_return_if_fail (type != NULL);
  g_return_if_fail (g_variant_type_is_container (type));

  memset (builder, 0, sizeof (GVariantBuilder));

  GVSB(builder)->type = g_variant_type_copy (type);
  GVSB(builder)->magic = GVSB_MAGIC;
  GVSB(builder)->trusted = TRUE;

  switch (*(const gchar *) type)
    {
    case G_VARIANT_CLASS_VARIANT:
      GVSB(builder)->uniform_item_types = TRUE;
      GVSB(builder)->allocated_children = 1;
      GVSB(builder)->expected_type = NULL;
      GVSB(builder)->min_items = 1;
      GVSB(builder)->max_items = 1;
      break;

    case G_VARIANT_CLASS_ARRAY:
      GVSB(builder)->uniform_item_types = TRUE;
      GVSB(builder)->allocated_children = 8;
      GVSB(builder)->expected_type =
        g_variant_type_element (GVSB(builder)->type);
      GVSB(builder)->min_items = 0;
      GVSB(builder)->max_items = -1;
      break;

    case G_VARIANT_CLASS_MAYBE:
      GVSB(builder)->uniform_item_types = TRUE;
      GVSB(builder)->allocated_children = 1;
      GVSB(builder)->expected_type =
        g_variant_type_element (GVSB(builder)->type);
      GVSB(builder)->min_items = 0;
      GVSB(builder)->max_items = 1;
      break;

    case G_VARIANT_CLASS_DICT_ENTRY:
      GVSB(builder)->uniform_item_types = FALSE;
      GVSB(builder)->allocated_children = 2;
      GVSB(builder)->expected_type =
        g_variant_type_key (GVSB(builder)->type);
      GVSB(builder)->min_items = 2;
      GVSB(builder)->max_items = 2;
      break;

    case 'r': /* G_VARIANT_TYPE_TUPLE was given */
      GVSB(builder)->uniform_item_types = FALSE;
      GVSB(builder)->allocated_children = 8;
      GVSB(builder)->expected_type = NULL;
      GVSB(builder)->min_items = 0;
      GVSB(builder)->max_items = -1;
      break;

    case G_VARIANT_CLASS_TUPLE: /* a definite tuple type was given */
      GVSB(builder)->allocated_children = g_variant_type_n_items (type);
      GVSB(builder)->expected_type =
        g_variant_type_first (GVSB(builder)->type);
      GVSB(builder)->min_items = GVSB(builder)->allocated_children;
      GVSB(builder)->max_items = GVSB(builder)->allocated_children;
      GVSB(builder)->uniform_item_types = FALSE;
      break;

    default:
      g_assert_not_reached ();
   }

  GVSB(builder)->children = g_new (GVariant *,
                                   GVSB(builder)->allocated_children);
}

static void
g_variant_builder_make_room (struct stack_builder *builder)
{
  if (builder->offset == builder->allocated_children)
    {
      builder->allocated_children *= 2;
      builder->children = g_renew (GVariant *, builder->children,
                                   builder->allocated_children);
    }
}

/**
 * g_variant_builder_add_value:
 * @builder: a #GVariantBuilder
 * @value: a #GVariant
 *
 * Adds @value to @builder.
 *
 * It is an error to call this function in any way that would create an
 * inconsistent value to be constructed.  Some examples of this are
 * putting different types of items into an array, putting the wrong
 * types or number of items in a tuple, putting more than one value into
 * a variant, etc.
 *
 * If @value is a floating reference (see g_variant_ref_sink()),
 * the @builder instance takes ownership of @value.
 *
 * Since: 2.24
 **/
void
g_variant_builder_add_value (GVariantBuilder *builder,
                             GVariant        *value)
{
  g_return_if_fail (ensure_valid_builder (builder));
  g_return_if_fail (GVSB(builder)->offset < GVSB(builder)->max_items);
  g_return_if_fail (!GVSB(builder)->expected_type ||
                    g_variant_is_of_type (value,
                                          GVSB(builder)->expected_type));
  g_return_if_fail (!GVSB(builder)->prev_item_type ||
                    g_variant_is_of_type (value,
                                          GVSB(builder)->prev_item_type));

  GVSB(builder)->trusted &= g_variant_is_trusted (value);

  if (!GVSB(builder)->uniform_item_types)
    {
      /* advance our expected type pointers */
      if (GVSB(builder)->expected_type)
        GVSB(builder)->expected_type =
          g_variant_type_next (GVSB(builder)->expected_type);

      if (GVSB(builder)->prev_item_type)
        GVSB(builder)->prev_item_type =
          g_variant_type_next (GVSB(builder)->prev_item_type);
    }
  else
    GVSB(builder)->prev_item_type = g_variant_get_type (value);

  g_variant_builder_make_room (GVSB(builder));

  GVSB(builder)->children[GVSB(builder)->offset++] =
    g_variant_ref_sink (value);
}

/**
 * g_variant_builder_open:
 * @builder: a #GVariantBuilder
 * @type: the #GVariantType of the container
 *
 * Opens a subcontainer inside the given @builder.  When done adding
 * items to the subcontainer, g_variant_builder_close() must be called. @type
 * is the type of the container: so to build a tuple of several values, @type
 * must include the tuple itself.
 *
 * It is an error to call this function in any way that would cause an
 * inconsistent value to be constructed (ie: adding too many values or
 * a value of an incorrect type).
 *
 * Example of building a nested variant:
 * |[<!-- language="C" -->
 * GVariantBuilder builder;
 * guint32 some_number = get_number ();
 * g_autoptr (GHashTable) some_dict = get_dict ();
 * GHashTableIter iter;
 * const gchar *key;
 * const GVariant *value;
 * g_autoptr (GVariant) output = NULL;
 *
 * g_variant_builder_init (&builder, G_VARIANT_TYPE ("(ua{sv})"));
 * g_variant_builder_add (&builder, "u", some_number);
 * g_variant_builder_open (&builder, G_VARIANT_TYPE ("a{sv}"));
 *
 * g_hash_table_iter_init (&iter, some_dict);
 * while (g_hash_table_iter_next (&iter, (gpointer *) &key, (gpointer *) &value))
 *   {
 *     g_variant_builder_open (&builder, G_VARIANT_TYPE ("{sv}"));
 *     g_variant_builder_add (&builder, "s", key);
 *     g_variant_builder_add (&builder, "v", value);
 *     g_variant_builder_close (&builder);
 *   }
 *
 * g_variant_builder_close (&builder);
 *
 * output = g_variant_builder_end (&builder);
 * ]|
 *
 * Since: 2.24
 **/
void
g_variant_builder_open (GVariantBuilder    *builder,
                        const GVariantType *type)
{
  GVariantBuilder *parent;

  g_return_if_fail (ensure_valid_builder (builder));
  g_return_if_fail (GVSB(builder)->offset < GVSB(builder)->max_items);
  g_return_if_fail (!GVSB(builder)->expected_type ||
                    g_variant_type_is_subtype_of (type,
                                                  GVSB(builder)->expected_type));
  g_return_if_fail (!GVSB(builder)->prev_item_type ||
                    g_variant_type_is_subtype_of (GVSB(builder)->prev_item_type,
                                                  type));

  parent = g_slice_dup (GVariantBuilder, builder);
  g_variant_builder_init (builder, type);
  GVSB(builder)->parent = parent;

  /* push the prev_item_type down into the subcontainer */
  if (GVSB(parent)->prev_item_type)
    {
      if (!GVSB(builder)->uniform_item_types)
        /* tuples and dict entries */
        GVSB(builder)->prev_item_type =
          g_variant_type_first (GVSB(parent)->prev_item_type);

      else if (!g_variant_type_is_variant (GVSB(builder)->type))
        /* maybes and arrays */
        GVSB(builder)->prev_item_type =
          g_variant_type_element (GVSB(parent)->prev_item_type);
    }
}

/**
 * g_variant_builder_close:
 * @builder: a #GVariantBuilder
 *
 * Closes the subcontainer inside the given @builder that was opened by
 * the most recent call to g_variant_builder_open().
 *
 * It is an error to call this function in any way that would create an
 * inconsistent value to be constructed (ie: too few values added to the
 * subcontainer).
 *
 * Since: 2.24
 **/
void
g_variant_builder_close (GVariantBuilder *builder)
{
  GVariantBuilder *parent;

  g_return_if_fail (ensure_valid_builder (builder));
  g_return_if_fail (GVSB(builder)->parent != NULL);

  parent = GVSB(builder)->parent;
  GVSB(builder)->parent = NULL;

  g_variant_builder_add_value (parent, g_variant_builder_end (builder));
  *builder = *parent;

  g_slice_free (GVariantBuilder, parent);
}

/*< private >
 * g_variant_make_maybe_type:
 * @element: a #GVariant
 *
 * Return the type of a maybe containing @element.
 */
static GVariantType *
g_variant_make_maybe_type (GVariant *element)
{
  return g_variant_type_new_maybe (g_variant_get_type (element));
}

/*< private >
 * g_variant_make_array_type:
 * @element: a #GVariant
 *
 * Return the type of an array containing @element.
 */
static GVariantType *
g_variant_make_array_type (GVariant *element)
{
  return g_variant_type_new_array (g_variant_get_type (element));
}

/**
 * g_variant_builder_end:
 * @builder: a #GVariantBuilder
 *
 * Ends the builder process and returns the constructed value.
 *
 * It is not permissible to use @builder in any way after this call
 * except for reference counting operations (in the case of a
 * heap-allocated #GVariantBuilder) or by reinitialising it with
 * g_variant_builder_init() (in the case of stack-allocated). This
 * means that for the stack-allocated builders there is no need to
 * call g_variant_builder_clear() after the call to
 * g_variant_builder_end().
 *
 * It is an error to call this function in any way that would create an
 * inconsistent value to be constructed (ie: insufficient number of
 * items added to a container with a specific number of children
 * required).  It is also an error to call this function if the builder
 * was created with an indefinite array or maybe type and no children
 * have been added; in this case it is impossible to infer the type of
 * the empty array.
 *
 * Returns: (transfer none): a new, floating, #GVariant
 *
 * Since: 2.24
 **/
GVariant *
g_variant_builder_end (GVariantBuilder *builder)
{
  GVariantType *my_type;
  GVariant *value;

  g_return_val_if_fail (ensure_valid_builder (builder), NULL);
  g_return_val_if_fail (GVSB(builder)->offset >= GVSB(builder)->min_items,
                        NULL);
  g_return_val_if_fail (!GVSB(builder)->uniform_item_types ||
                        GVSB(builder)->prev_item_type != NULL ||
                        g_variant_type_is_definite (GVSB(builder)->type),
                        NULL);

  if (g_variant_type_is_definite (GVSB(builder)->type))
    my_type = g_variant_type_copy (GVSB(builder)->type);

  else if (g_variant_type_is_maybe (GVSB(builder)->type))
    my_type = g_variant_make_maybe_type (GVSB(builder)->children[0]);

  else if (g_variant_type_is_array (GVSB(builder)->type))
    my_type = g_variant_make_array_type (GVSB(builder)->children[0]);

  else if (g_variant_type_is_tuple (GVSB(builder)->type))
    my_type = g_variant_make_tuple_type (GVSB(builder)->children,
                                         GVSB(builder)->offset);

  else if (g_variant_type_is_dict_entry (GVSB(builder)->type))
    my_type = g_variant_make_dict_entry_type (GVSB(builder)->children[0],
                                              GVSB(builder)->children[1]);
  else
    g_assert_not_reached ();

  value = g_variant_new_from_children (my_type,
                                       g_renew (GVariant *,
                                                GVSB(builder)->children,
                                                GVSB(builder)->offset),
                                       GVSB(builder)->offset,
                                       GVSB(builder)->trusted);
  GVSB(builder)->children = NULL;
  GVSB(builder)->offset = 0;

  g_variant_builder_clear (builder);
  g_variant_type_free (my_type);

  return value;
}

/* GVariantDict {{{1 */

/**
 * GVariantDict:
 *
 * #GVariantDict is a mutable interface to #GVariant dictionaries.
 *
 * It can be used for doing a sequence of dictionary lookups in an
 * efficient way on an existing #GVariant dictionary or it can be used
 * to construct new dictionaries with a hashtable-like interface.  It
 * can also be used for taking existing dictionaries and modifying them
 * in order to create new ones.
 *
 * #GVariantDict can only be used with %G_VARIANT_TYPE_VARDICT
 * dictionaries.
 *
 * It is possible to use #GVariantDict allocated on the stack or on the
 * heap.  When using a stack-allocated #GVariantDict, you begin with a
 * call to g_variant_dict_init() and free the resources with a call to
 * g_variant_dict_clear().
 *
 * Heap-allocated #GVariantDict follows normal refcounting rules: you
 * allocate it with g_variant_dict_new() and use g_variant_dict_ref()
 * and g_variant_dict_unref().
 *
 * g_variant_dict_end() is used to convert the #GVariantDict back into a
 * dictionary-type #GVariant.  When used with stack-allocated instances,
 * this also implicitly frees all associated memory, but for
 * heap-allocated instances, you must still call g_variant_dict_unref()
 * afterwards.
 *
 * You will typically want to use a heap-allocated #GVariantDict when
 * you expose it as part of an API.  For most other uses, the
 * stack-allocated form will be more convenient.
 *
 * Consider the following two examples that do the same thing in each
 * style: take an existing dictionary and look up the "count" uint32
 * key, adding 1 to it if it is found, or returning an error if the
 * key is not found.  Each returns the new dictionary as a floating
 * #GVariant.
 *
 * ## Using a stack-allocated GVariantDict
 *
 * |[<!-- language="C" -->
 *   GVariant *
 *   add_to_count (GVariant  *orig,
 *                 GError   **error)
 *   {
 *     GVariantDict dict;
 *     guint32 count;
 *
 *     g_variant_dict_init (&dict, orig);
 *     if (!g_variant_dict_lookup (&dict, "count", "u", &count))
 *       {
 *         g_set_error (...);
 *         g_variant_dict_clear (&dict);
 *         return NULL;
 *       }
 *
 *     g_variant_dict_insert (&dict, "count", "u", count + 1);
 *
 *     return g_variant_dict_end (&dict);
 *   }
 * ]|
 *
 * ## Using heap-allocated GVariantDict
 *
 * |[<!-- language="C" -->
 *   GVariant *
 *   add_to_count (GVariant  *orig,
 *                 GError   **error)
 *   {
 *     GVariantDict *dict;
 *     GVariant *result;
 *     guint32 count;
 *
 *     dict = g_variant_dict_new (orig);
 *
 *     if (g_variant_dict_lookup (dict, "count", "u", &count))
 *       {
 *         g_variant_dict_insert (dict, "count", "u", count + 1);
 *         result = g_variant_dict_end (dict);
 *       }
 *     else
 *       {
 *         g_set_error (...);
 *         result = NULL;
 *       }
 *
 *     g_variant_dict_unref (dict);
 *
 *     return result;
 *   }
 * ]|
 *
 * Since: 2.40
 **/
struct stack_dict
{
  GHashTable *values;
  gsize magic;
};

G_STATIC_ASSERT (sizeof (struct stack_dict) <= sizeof (GVariantDict));

struct heap_dict
{
  struct stack_dict dict;
  gint ref_count;
  gsize magic;
};

#define GVSD(d)                 ((struct stack_dict *) (d))
#define GVHD(d)                 ((struct heap_dict *) (d))
#define GVSD_MAGIC              ((gsize) 2579507750u)
#define GVSD_MAGIC_PARTIAL      ((gsize) 3488698669u)
#define GVHD_MAGIC              ((gsize) 2450270775u)
#define is_valid_dict(d)        (d != NULL && \
                                 GVSD(d)->magic == GVSD_MAGIC)
#define is_valid_heap_dict(d)   (GVHD(d)->magic == GVHD_MAGIC)

/* Just to make sure that by adding a union to GVariantDict, we didn't
 * accidentally change ABI. */
G_STATIC_ASSERT (sizeof (GVariantDict) == sizeof (gsize[16]));

static gboolean
ensure_valid_dict (GVariantDict *dict)
{
  if (is_valid_dict (dict))
    return TRUE;
  if (dict->u.s.partial_magic == GVSD_MAGIC_PARTIAL)
    {
      static GVariantDict cleared_dict;

      /* Make sure that only first two fields were set and the rest is
       * zeroed to avoid messing up the builder that had parent
       * address equal to GVSB_MAGIC_PARTIAL. */
      if (memcmp (cleared_dict.u.s.y, dict->u.s.y, sizeof cleared_dict.u.s.y))
        return FALSE;

      g_variant_dict_init (dict, dict->u.s.asv);
    }
  return is_valid_dict (dict);
}

/**
 * g_variant_dict_new:
 * @from_asv: (nullable): the #GVariant with which to initialise the
 *   dictionary
 *
 * Allocates and initialises a new #GVariantDict.
 *
 * You should call g_variant_dict_unref() on the return value when it
 * is no longer needed.  The memory will not be automatically freed by
 * any other call.
 *
 * In some cases it may be easier to place a #GVariantDict directly on
 * the stack of the calling function and initialise it with
 * g_variant_dict_init().  This is particularly useful when you are
 * using #GVariantDict to construct a #GVariant.
 *
 * Returns: (transfer full): a #GVariantDict
 *
 * Since: 2.40
 **/
GVariantDict *
g_variant_dict_new (GVariant *from_asv)
{
  GVariantDict *dict;

  dict = g_slice_alloc (sizeof (struct heap_dict));
  g_variant_dict_init (dict, from_asv);
  GVHD(dict)->magic = GVHD_MAGIC;
  GVHD(dict)->ref_count = 1;

  return dict;
}

/**
 * g_variant_dict_init: (skip)
 * @dict: a #GVariantDict
 * @from_asv: (nullable): the initial value for @dict
 *
 * Initialises a #GVariantDict structure.
 *
 * If @from_asv is given, it is used to initialise the dictionary.
 *
 * This function completely ignores the previous contents of @dict.  On
 * one hand this means that it is valid to pass in completely
 * uninitialised memory.  On the other hand, this means that if you are
 * initialising over top of an existing #GVariantDict you need to first
 * call g_variant_dict_clear() in order to avoid leaking memory.
 *
 * You must not call g_variant_dict_ref() or g_variant_dict_unref() on a
 * #GVariantDict that was initialised with this function.  If you ever
 * pass a reference to a #GVariantDict outside of the control of your
 * own code then you should assume that the person receiving that
 * reference may try to use reference counting; you should use
 * g_variant_dict_new() instead of this function.
 *
 * Since: 2.40
 **/
void
g_variant_dict_init (GVariantDict *dict,
                     GVariant     *from_asv)
{
  GVariantIter iter;
  gchar *key;
  GVariant *value;

  GVSD(dict)->values = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) g_variant_unref);
  GVSD(dict)->magic = GVSD_MAGIC;

  if (from_asv)
    {
      g_variant_iter_init (&iter, from_asv);
      while (g_variant_iter_next (&iter, "{sv}", &key, &value))
        g_hash_table_insert (GVSD(dict)->values, key, value);
    }
}

/**
 * g_variant_dict_lookup:
 * @dict: a #GVariantDict
 * @key: the key to lookup in the dictionary
 * @format_string: a GVariant format string
 * @...: the arguments to unpack the value into
 *
 * Looks up a value in a #GVariantDict.
 *
 * This function is a wrapper around g_variant_dict_lookup_value() and
 * g_variant_get().  In the case that %NULL would have been returned,
 * this function returns %FALSE.  Otherwise, it unpacks the returned
 * value and returns %TRUE.
 *
 * @format_string determines the C types that are used for unpacking the
 * values and also determines if the values are copied or borrowed, see the
 * section on [GVariant format strings][gvariant-format-strings-pointers].
 *
 * Returns: %TRUE if a value was unpacked
 *
 * Since: 2.40
 **/
gboolean
g_variant_dict_lookup (GVariantDict *dict,
                       const gchar  *key,
                       const gchar  *format_string,
                       ...)
{
  GVariant *value;
  va_list ap;

  g_return_val_if_fail (ensure_valid_dict (dict), FALSE);
  g_return_val_if_fail (key != NULL, FALSE);
  g_return_val_if_fail (format_string != NULL, FALSE);

  value = g_hash_table_lookup (GVSD(dict)->values, key);

  if (value == NULL || !g_variant_check_format_string (value, format_string, FALSE))
    return FALSE;

  va_start (ap, format_string);
  g_variant_get_va (value, format_string, NULL, &ap);
  va_end (ap);

  return TRUE;
}

/**
 * g_variant_dict_lookup_value:
 * @dict: a #GVariantDict
 * @key: the key to lookup in the dictionary
 * @expected_type: (nullable): a #GVariantType, or %NULL
 *
 * Looks up a value in a #GVariantDict.
 *
 * If @key is not found in @dictionary, %NULL is returned.
 *
 * The @expected_type string specifies what type of value is expected.
 * If the value associated with @key has a different type then %NULL is
 * returned.
 *
 * If the key is found and the value has the correct type, it is
 * returned.  If @expected_type was specified then any non-%NULL return
 * value will have this type.
 *
 * Returns: (transfer full): the value of the dictionary key, or %NULL
 *
 * Since: 2.40
 **/
GVariant *
g_variant_dict_lookup_value (GVariantDict       *dict,
                             const gchar        *key,
                             const GVariantType *expected_type)
{
  GVariant *result;

  g_return_val_if_fail (ensure_valid_dict (dict), NULL);
  g_return_val_if_fail (key != NULL, NULL);

  result = g_hash_table_lookup (GVSD(dict)->values, key);

  if (result && (!expected_type || g_variant_is_of_type (result, expected_type)))
    return g_variant_ref (result);

  return NULL;
}

/**
 * g_variant_dict_contains:
 * @dict: a #GVariantDict
 * @key: the key to lookup in the dictionary
 *
 * Checks if @key exists in @dict.
 *
 * Returns: %TRUE if @key is in @dict
 *
 * Since: 2.40
 **/
gboolean
g_variant_dict_contains (GVariantDict *dict,
                         const gchar  *key)
{
  g_return_val_if_fail (ensure_valid_dict (dict), FALSE);
  g_return_val_if_fail (key != NULL, FALSE);

  return g_hash_table_contains (GVSD(dict)->values, key);
}

/**
 * g_variant_dict_insert:
 * @dict: a #GVariantDict
 * @key: the key to insert a value for
 * @format_string: a #GVariant varargs format string
 * @...: arguments, as per @format_string
 *
 * Inserts a value into a #GVariantDict.
 *
 * This call is a convenience wrapper that is exactly equivalent to
 * calling g_variant_new() followed by g_variant_dict_insert_value().
 *
 * Since: 2.40
 **/
void
g_variant_dict_insert (GVariantDict *dict,
                       const gchar  *key,
                       const gchar  *format_string,
                       ...)
{
  va_list ap;

  g_return_if_fail (ensure_valid_dict (dict));
  g_return_if_fail (key != NULL);
  g_return_if_fail (format_string != NULL);

  va_start (ap, format_string);
  g_variant_dict_insert_value (dict, key, g_variant_new_va (format_string, NULL, &ap));
  va_end (ap);
}

/**
 * g_variant_dict_insert_value:
 * @dict: a #GVariantDict
 * @key: the key to insert a value for
 * @value: the value to insert
 *
 * Inserts (or replaces) a key in a #GVariantDict.
 *
 * @value is consumed if it is floating.
 *
 * Since: 2.40
 **/
void
g_variant_dict_insert_value (GVariantDict *dict,
                             const gchar  *key,
                             GVariant     *value)
{
  g_return_if_fail (ensure_valid_dict (dict));
  g_return_if_fail (key != NULL);
  g_return_if_fail (value != NULL);

  g_hash_table_insert (GVSD(dict)->values, g_strdup (key), g_variant_ref_sink (value));
}

/**
 * g_variant_dict_remove:
 * @dict: a #GVariantDict
 * @key: the key to remove
 *
 * Removes a key and its associated value from a #GVariantDict.
 *
 * Returns: %TRUE if the key was found and removed
 *
 * Since: 2.40
 **/
gboolean
g_variant_dict_remove (GVariantDict *dict,
                       const gchar  *key)
{
  g_return_val_if_fail (ensure_valid_dict (dict), FALSE);
  g_return_val_if_fail (key != NULL, FALSE);

  return g_hash_table_remove (GVSD(dict)->values, key);
}

/**
 * g_variant_dict_clear:
 * @dict: a #GVariantDict
 *
 * Releases all memory associated with a #GVariantDict without freeing
 * the #GVariantDict structure itself.
 *
 * It typically only makes sense to do this on a stack-allocated
 * #GVariantDict if you want to abort building the value part-way
 * through.  This function need not be called if you call
 * g_variant_dict_end() and it also doesn't need to be called on dicts
 * allocated with g_variant_dict_new (see g_variant_dict_unref() for
 * that).
 *
 * It is valid to call this function on either an initialised
 * #GVariantDict or one that was previously cleared by an earlier call
 * to g_variant_dict_clear() but it is not valid to call this function
 * on uninitialised memory.
 *
 * Since: 2.40
 **/
void
g_variant_dict_clear (GVariantDict *dict)
{
  if (GVSD(dict)->magic == 0)
    /* all-zeros case */
    return;

  g_return_if_fail (ensure_valid_dict (dict));

  g_hash_table_unref (GVSD(dict)->values);
  GVSD(dict)->values = NULL;

  GVSD(dict)->magic = 0;
}

/**
 * g_variant_dict_end:
 * @dict: a #GVariantDict
 *
 * Returns the current value of @dict as a #GVariant of type
 * %G_VARIANT_TYPE_VARDICT, clearing it in the process.
 *
 * It is not permissible to use @dict in any way after this call except
 * for reference counting operations (in the case of a heap-allocated
 * #GVariantDict) or by reinitialising it with g_variant_dict_init() (in
 * the case of stack-allocated).
 *
 * Returns: (transfer none): a new, floating, #GVariant
 *
 * Since: 2.40
 **/
GVariant *
g_variant_dict_end (GVariantDict *dict)
{
  GVariantBuilder builder;
  GHashTableIter iter;
  gpointer key, value;

  g_return_val_if_fail (ensure_valid_dict (dict), NULL);

  g_variant_builder_init (&builder, G_VARIANT_TYPE_VARDICT);

  g_hash_table_iter_init (&iter, GVSD(dict)->values);
  while (g_hash_table_iter_next (&iter, &key, &value))
    g_variant_builder_add (&builder, "{sv}", (const gchar *) key, (GVariant *) value);

  g_variant_dict_clear (dict);

  return g_variant_builder_end (&builder);
}

/**
 * g_variant_dict_ref:
 * @dict: a heap-allocated #GVariantDict
 *
 * Increases the reference count on @dict.
 *
 * Don't call this on stack-allocated #GVariantDict instances or bad
 * things will happen.
 *
 * Returns: (transfer full): a new reference to @dict
 *
 * Since: 2.40
 **/
GVariantDict *
g_variant_dict_ref (GVariantDict *dict)
{
  g_return_val_if_fail (is_valid_heap_dict (dict), NULL);

  GVHD(dict)->ref_count++;

  return dict;
}

/**
 * g_variant_dict_unref:
 * @dict: (transfer full): a heap-allocated #GVariantDict
 *
 * Decreases the reference count on @dict.
 *
 * In the event that there are no more references, releases all memory
 * associated with the #GVariantDict.
 *
 * Don't call this on stack-allocated #GVariantDict instances or bad
 * things will happen.
 *
 * Since: 2.40
 **/
void
g_variant_dict_unref (GVariantDict *dict)
{
  g_return_if_fail (is_valid_heap_dict (dict));

  if (--GVHD(dict)->ref_count == 0)
    {
      g_variant_dict_clear (dict);
      g_slice_free (struct heap_dict, (struct heap_dict *) dict);
    }
}


/* Format strings {{{1 */
/*< private >
 * g_variant_format_string_scan:
 * @string: a string that may be prefixed with a format string
 * @limit: (nullable) (default NULL): a pointer to the end of @string,
 *         or %NULL
 * @endptr: (nullable) (default NULL): location to store the end pointer,
 *          or %NULL
 *
 * Checks the string pointed to by @string for starting with a properly
 * formed #GVariant varargs format string.  If no valid format string is
 * found then %FALSE is returned.
 *
 * If @string does start with a valid format string then %TRUE is
 * returned.  If @endptr is non-%NULL then it is updated to point to the
 * first character after the format string.
 *
 * If @limit is non-%NULL then @limit (and any character after it) will
 * not be accessed and the effect is otherwise equivalent to if the
 * character at @limit were nul.
 *
 * See the section on [GVariant format strings][gvariant-format-strings].
 *
 * Returns: %TRUE if there was a valid format string
 *
 * Since: 2.24
 */
gboolean
g_variant_format_string_scan (const gchar  *string,
                              const gchar  *limit,
                              const gchar **endptr)
{
#define next_char() (string == limit ? '\0' : *string++)
#define peek_char() (string == limit ? '\0' : *string)
  char c;

  switch (next_char())
    {
    case 'b': case 'y': case 'n': case 'q': case 'i': case 'u':
    case 'x': case 't': case 'h': case 'd': case 's': case 'o':
    case 'g': case 'v': case '*': case '?': case 'r':
      break;

    case 'm':
      return g_variant_format_string_scan (string, limit, endptr);

    case 'a':
    case '@':
      return g_variant_type_string_scan (string, limit, endptr);

    case '(':
      while (peek_char() != ')')
        if (!g_variant_format_string_scan (string, limit, &string))
          return FALSE;

      next_char(); /* consume ')' */
      break;

    case '{':
      c = next_char();

      if (c == '&')
        {
          c = next_char ();

          if (c != 's' && c != 'o' && c != 'g')
            return FALSE;
        }
      else
        {
          if (c == '@')
            c = next_char ();

          /* ISO/IEC 9899:1999 (C99) §7.21.5.2:
           *    The terminating null character is considered to be
           *    part of the string.
           */
          if (c != '\0' && strchr ("bynqiuxthdsog?", c) == NULL)
            return FALSE;
        }

      if (!g_variant_format_string_scan (string, limit, &string))
        return FALSE;

      if (next_char() != '}')
        return FALSE;

      break;

    case '^':
      if ((c = next_char()) == 'a')
        {
          if ((c = next_char()) == '&')
            {
              if ((c = next_char()) == 'a')
                {
                  if ((c = next_char()) == 'y')
                    break;      /* '^a&ay' */
                }

              else if (c == 's' || c == 'o')
                break;          /* '^a&s', '^a&o' */
            }

          else if (c == 'a')
            {
              if ((c = next_char()) == 'y')
                break;          /* '^aay' */
            }

          else if (c == 's' || c == 'o')
            break;              /* '^as', '^ao' */

          else if (c == 'y')
            break;              /* '^ay' */
        }
      else if (c == '&')
        {
          if ((c = next_char()) == 'a')
            {
              if ((c = next_char()) == 'y')
                break;          /* '^&ay' */
            }
        }

      return FALSE;

    case '&':
      c = next_char();

      if (c != 's' && c != 'o' && c != 'g')
        return FALSE;

      break;

    default:
      return FALSE;
    }

  if (endptr != NULL)
    *endptr = string;

#undef next_char
#undef peek_char

  return TRUE;
}

/**
 * g_variant_check_format_string:
 * @value: a #GVariant
 * @format_string: a valid #GVariant format string
 * @copy_only: %TRUE to ensure the format string makes deep copies
 *
 * Checks if calling g_variant_get() with @format_string on @value would
 * be valid from a type-compatibility standpoint.  @format_string is
 * assumed to be a valid format string (from a syntactic standpoint).
 *
 * If @copy_only is %TRUE then this function additionally checks that it
 * would be safe to call g_variant_unref() on @value immediately after
 * the call to g_variant_get() without invalidating the result.  This is
 * only possible if deep copies are made (ie: there are no pointers to
 * the data inside of the soon-to-be-freed #GVariant instance).  If this
 * check fails then a g_critical() is printed and %FALSE is returned.
 *
 * This function is meant to be used by functions that wish to provide
 * varargs accessors to #GVariant values of uncertain values (eg:
 * g_variant_lookup() or g_menu_model_get_item_attribute()).
 *
 * Returns: %TRUE if @format_string is safe to use
 *
 * Since: 2.34
 */
gboolean
g_variant_check_format_string (GVariant    *value,
                               const gchar *format_string,
                               gboolean     copy_only)
{
  const gchar *original_format = format_string;
  const gchar *type_string;

  /* Interesting factoid: assuming a format string is valid, it can be
   * converted to a type string by removing all '@' '&' and '^'
   * characters.
   *
   * Instead of doing that, we can just skip those characters when
   * comparing it to the type string of @value.
   *
   * For the copy-only case we can just drop the '&' from the list of
   * characters to skip over.  A '&' will never appear in a type string
   * so we know that it won't be possible to return %TRUE if it is in a
   * format string.
   */
  type_string = g_variant_get_type_string (value);

  while (*type_string || *format_string)
    {
      gchar format = *format_string++;

      switch (format)
        {
        case '&':
          if G_UNLIKELY (copy_only)
            {
              /* for the love of all that is good, please don't mark this string for translation... */
              g_critical ("g_variant_check_format_string() is being called by a function with a GVariant varargs "
                          "interface to validate the passed format string for type safety.  The passed format "
                          "(%s) contains a '&' character which would result in a pointer being returned to the "
                          "data inside of a GVariant instance that may no longer exist by the time the function "
                          "returns.  Modify your code to use a format string without '&'.", original_format);
              return FALSE;
            }

          /* fall through */
        case '^':
        case '@':
          /* ignore these 2 (or 3) */
          continue;

        case '?':
          /* attempt to consume one of 'bynqiuxthdsog' */
          {
            char s = *type_string++;

            if (s == '\0' || strchr ("bynqiuxthdsog", s) == NULL)
              return FALSE;
          }
          continue;

        case 'r':
          /* ensure it's a tuple */
          if (*type_string != '(')
            return FALSE;

          /* fall through */
        case '*':
          /* consume a full type string for the '*' or 'r' */
          if (!g_variant_type_string_scan (type_string, NULL, &type_string))
            return FALSE;

          continue;

        default:
          /* attempt to consume exactly one character equal to the format */
          if (format != *type_string++)
            return FALSE;
        }
    }

  return TRUE;
}

/*< private >
 * g_variant_format_string_scan_type:
 * @string: a string that may be prefixed with a format string
 * @limit: (nullable) (default NULL): a pointer to the end of @string,
 *         or %NULL
 * @endptr: (nullable) (default NULL): location to store the end pointer,
 *          or %NULL
 *
 * If @string starts with a valid format string then this function will
 * return the type that the format string corresponds to.  Otherwise
 * this function returns %NULL.
 *
 * Use g_variant_type_free() to free the return value when you no longer
 * need it.
 *
 * This function is otherwise exactly like
 * g_variant_format_string_scan().
 *
 * Returns: (nullable): a #GVariantType if there was a valid format string
 *
 * Since: 2.24
 */
GVariantType *
g_variant_format_string_scan_type (const gchar  *string,
                                   const gchar  *limit,
                                   const gchar **endptr)
{
  const gchar *my_end;
  gchar *dest;
  gchar *new;

  if (endptr == NULL)
    endptr = &my_end;

  if (!g_variant_format_string_scan (string, limit, endptr))
    return NULL;

  dest = new = g_malloc (*endptr - string + 1);
  while (string != *endptr)
    {
      if (*string != '@' && *string != '&' && *string != '^')
        *dest++ = *string;
      string++;
    }
  *dest = '\0';

  return (GVariantType *) G_VARIANT_TYPE (new);
}

static gboolean
valid_format_string (const gchar *format_string,
                     gboolean     single,
                     GVariant    *value)
{
  const gchar *endptr;
  GVariantType *type;

  type = g_variant_format_string_scan_type (format_string, NULL, &endptr);

  if G_UNLIKELY (type == NULL || (single && *endptr != '\0'))
    {
      if (single)
        g_critical ("'%s' is not a valid GVariant format string",
                    format_string);
      else
        g_critical ("'%s' does not have a valid GVariant format "
                    "string as a prefix", format_string);

      if (type != NULL)
        g_variant_type_free (type);

      return FALSE;
    }

  if G_UNLIKELY (value && !g_variant_is_of_type (value, type))
    {
      gchar *fragment;
      gchar *typestr;

      fragment = g_strndup (format_string, endptr - format_string);
      typestr = g_variant_type_dup_string (type);

      g_critical ("the GVariant format string '%s' has a type of "
                  "'%s' but the given value has a type of '%s'",
                  fragment, typestr, g_variant_get_type_string (value));

      g_variant_type_free (type);
      g_free (fragment);
      g_free (typestr);

      return FALSE;
    }

  g_variant_type_free (type);

  return TRUE;
}

/* Variable Arguments {{{1 */
/* We consider 2 main classes of format strings:
 *
 *   - recursive format strings
 *      these are ones that result in recursion and the collection of
 *      possibly more than one argument.  Maybe types, tuples,
 *      dictionary entries.
 *
 *   - leaf format string
 *      these result in the collection of a single argument.
 *
 * Leaf format strings are further subdivided into two categories:
 *
 *   - single non-null pointer ("nnp")
 *      these either collect or return a single non-null pointer.
 *
 *   - other
 *      these collect or return something else (bool, number, etc).
 *
 * Based on the above, the varargs handling code is split into 4 main parts:
 *
 *   - nnp handling code
 *   - leaf handling code (which may invoke nnp code)
 *   - generic handling code (may be recursive, may invoke leaf code)
 *   - user-facing API (which invokes the generic code)
 *
 * Each section implements some of the following functions:
 *
 *   - skip:
 *      collect the arguments for the format string as if
 *      g_variant_new() had been called, but do nothing with them.  used
 *      for skipping over arguments when constructing a Nothing maybe
 *      type.
 *
 *   - new:
 *      create a GVariant *
 *
 *   - get:
 *      unpack a GVariant *
 *
 *   - free (nnp only):
 *      free a previously allocated item
 */

static gboolean
g_variant_format_string_is_leaf (const gchar *str)
{
  return str[0] != 'm' && str[0] != '(' && str[0] != '{';
}

static gboolean
g_variant_format_string_is_nnp (const gchar *str)
{
  return str[0] == 'a' || str[0] == 's' || str[0] == 'o' || str[0] == 'g' ||
         str[0] == '^' || str[0] == '@' || str[0] == '*' || str[0] == '?' ||
         str[0] == 'r' || str[0] == 'v' || str[0] == '&';
}

/* Single non-null pointer ("nnp") {{{2 */
static void
g_variant_valist_free_nnp (const gchar *str,
                           gpointer     ptr)
{
  switch (*str)
    {
    case 'a':
      g_variant_iter_free (ptr);
      break;

    case '^':
      if (str[2] != '&')        /* '^as', '^ao' */
        g_strfreev (ptr);
      else                      /* '^a&s', '^a&o' */
        g_free (ptr);
      break;

    case 's':
    case 'o':
    case 'g':
      g_free (ptr);
      break;

    case '@':
    case '*':
    case '?':
    case 'v':
      g_variant_unref (ptr);
      break;

    case '&':
      break;

    default:
      g_assert_not_reached ();
    }
}

static gchar
g_variant_scan_convenience (const gchar **str,
                            gboolean     *constant,
                            guint        *arrays)
{
  *constant = FALSE;
  *arrays = 0;

  for (;;)
    {
      char c = *(*str)++;

      if (c == '&')
        *constant = TRUE;

      else if (c == 'a')
        (*arrays)++;

      else
        return c;
    }
}

static GVariant *
g_variant_valist_new_nnp (const gchar **str,
                          gpointer      ptr)
{
  if (**str == '&')
    (*str)++;

  switch (*(*str)++)
    {
    case 'a':
      if (ptr != NULL)
        {
          const GVariantType *type;
          GVariant *value;

          value = g_variant_builder_end (ptr);
          type = g_variant_get_type (value);

          if G_UNLIKELY (!g_variant_type_is_array (type))
            g_error ("g_variant_new: expected array GVariantBuilder but "
                     "the built value has type '%s'",
                     g_variant_get_type_string (value));

          type = g_variant_type_element (type);

          if G_UNLIKELY (!g_variant_type_is_subtype_of (type, (GVariantType *) *str))
            g_error ("g_variant_new: expected GVariantBuilder array element "
                     "type '%s' but the built value has element type '%s'",
                     g_variant_type_dup_string ((GVariantType *) *str),
                     g_variant_get_type_string (value) + 1);

          g_variant_type_string_scan (*str, NULL, str);

          return value;
        }
      else

        /* special case: NULL pointer for empty array */
        {
          const GVariantType *type = (GVariantType *) *str;

          g_variant_type_string_scan (*str, NULL, str);

          if G_UNLIKELY (!g_variant_type_is_definite (type))
            g_error ("g_variant_new: NULL pointer given with indefinite "
                     "array type; unable to determine which type of empty "
                     "array to construct.");

          return g_variant_new_array (type, NULL, 0);
        }

    case 's':
      {
        GVariant *value;

        value = g_variant_new_string (ptr);

        if (value == NULL)
          value = g_variant_new_string ("[Invalid UTF-8]");

        return value;
      }

    case 'o':
      return g_variant_new_object_path (ptr);

    case 'g':
      return g_variant_new_signature (ptr);

    case '^':
      {
        gboolean constant;
        guint arrays;
        gchar type;

        type = g_variant_scan_convenience (str, &constant, &arrays);

        if (type == 's')
          return g_variant_new_strv (ptr, -1);

        if (type == 'o')
          return g_variant_new_objv (ptr, -1);

        if (arrays > 1)
          return g_variant_new_bytestring_array (ptr, -1);

        return g_variant_new_bytestring (ptr);
      }

    case '@':
      if G_UNLIKELY (!g_variant_is_of_type (ptr, (GVariantType *) *str))
        g_error ("g_variant_new: expected GVariant of type '%s' but "
                 "received value has type '%s'",
                 g_variant_type_dup_string ((GVariantType *) *str),
                 g_variant_get_type_string (ptr));

      g_variant_type_string_scan (*str, NULL, str);

      return ptr;

    case '*':
      return ptr;

    case '?':
      if G_UNLIKELY (!g_variant_type_is_basic (g_variant_get_type (ptr)))
        g_error ("g_variant_new: format string '?' expects basic-typed "
                 "GVariant, but received value has type '%s'",
                 g_variant_get_type_string (ptr));

      return ptr;

    case 'r':
      if G_UNLIKELY (!g_variant_type_is_tuple (g_variant_get_type (ptr)))
        g_error ("g_variant_new: format string 'r' expects tuple-typed "
                 "GVariant, but received value has type '%s'",
                 g_variant_get_type_string (ptr));

      return ptr;

    case 'v':
      return g_variant_new_variant (ptr);

    default:
      g_assert_not_reached ();
    }
}

static gpointer
g_variant_valist_get_nnp (const gchar **str,
                          GVariant     *value)
{
  switch (*(*str)++)
    {
    case 'a':
      g_variant_type_string_scan (*str, NULL, str);
      return g_variant_iter_new (value);

    case '&':
      (*str)++;
      return (gchar *) g_variant_get_string (value, NULL);

    case 's':
    case 'o':
    case 'g':
      return g_variant_dup_string (value, NULL);

    case '^':
      {
        gboolean constant;
        guint arrays;
        gchar type;

        type = g_variant_scan_convenience (str, &constant, &arrays);

        if (type == 's')
          {
            if (constant)
              return g_variant_get_strv (value, NULL);
            else
              return g_variant_dup_strv (value, NULL);
          }

        else if (type == 'o')
          {
            if (constant)
              return g_variant_get_objv (value, NULL);
            else
              return g_variant_dup_objv (value, NULL);
          }

        else if (arrays > 1)
          {
            if (constant)
              return g_variant_get_bytestring_array (value, NULL);
            else
              return g_variant_dup_bytestring_array (value, NULL);
          }

        else
          {
            if (constant)
              return (gchar *) g_variant_get_bytestring (value);
            else
              return g_variant_dup_bytestring (value, NULL);
          }
      }

    case '@':
      g_variant_type_string_scan (*str, NULL, str);
      /* fall through */

    case '*':
    case '?':
    case 'r':
      return g_variant_ref (value);

    case 'v':
      return g_variant_get_variant (value);

    default:
      g_assert_not_reached ();
    }
}

/* Leaves {{{2 */
static void
g_variant_valist_skip_leaf (const gchar **str,
                            va_list      *app)
{
  if (g_variant_format_string_is_nnp (*str))
    {
      g_variant_format_string_scan (*str, NULL, str);
      va_arg (*app, gpointer);
      return;
    }

  switch (*(*str)++)
    {
    case 'b':
    case 'y':
    case 'n':
    case 'q':
    case 'i':
    case 'u':
    case 'h':
      va_arg (*app, int);
      return;

    case 'x':
    case 't':
      va_arg (*app, guint64);
      return;

    case 'd':
      va_arg (*app, gdouble);
      return;

    default:
      g_assert_not_reached ();
    }
}

static GVariant *
g_variant_valist_new_leaf (const gchar **str,
                           va_list      *app)
{
  if (g_variant_format_string_is_nnp (*str))
    return g_variant_valist_new_nnp (str, va_arg (*app, gpointer));

  switch (*(*str)++)
    {
    case 'b':
      return g_variant_new_boolean (va_arg (*app, gboolean));

    case 'y':
      return g_variant_new_byte (va_arg (*app, guint));

    case 'n':
      return g_variant_new_int16 (va_arg (*app, gint));

    case 'q':
      return g_variant_new_uint16 (va_arg (*app, guint));

    case 'i':
      return g_variant_new_int32 (va_arg (*app, gint));

    case 'u':
      return g_variant_new_uint32 (va_arg (*app, guint));

    case 'x':
      return g_variant_new_int64 (va_arg (*app, gint64));

    case 't':
      return g_variant_new_uint64 (va_arg (*app, guint64));

    case 'h':
      return g_variant_new_handle (va_arg (*app, gint));

    case 'd':
      return g_variant_new_double (va_arg (*app, gdouble));

    default:
      g_assert_not_reached ();
    }
}

/* The code below assumes this */
G_STATIC_ASSERT (sizeof (gboolean) == sizeof (guint32));
G_STATIC_ASSERT (sizeof (gdouble) == sizeof (guint64));

static void
g_variant_valist_get_leaf (const gchar **str,
                           GVariant     *value,
                           gboolean      free,
                           va_list      *app)
{
  gpointer ptr = va_arg (*app, gpointer);

  if (ptr == NULL)
    {
      g_variant_format_string_scan (*str, NULL, str);
      return;
    }

  if (g_variant_format_string_is_nnp (*str))
    {
      gpointer *nnp = (gpointer *) ptr;

      if (free && *nnp != NULL)
        g_variant_valist_free_nnp (*str, *nnp);

      *nnp = NULL;

      if (value != NULL)
        *nnp = g_variant_valist_get_nnp (str, value);
      else
        g_variant_format_string_scan (*str, NULL, str);

      return;
    }

  if (value != NULL)
    {
      switch (*(*str)++)
        {
        case 'b':
          *(gboolean *) ptr = g_variant_get_boolean (value);
          return;

        case 'y':
          *(guchar *) ptr = g_variant_get_byte (value);
          return;

        case 'n':
          *(gint16 *) ptr = g_variant_get_int16 (value);
          return;

        case 'q':
          *(guint16 *) ptr = g_variant_get_uint16 (value);
          return;

        case 'i':
          *(gint32 *) ptr = g_variant_get_int32 (value);
          return;

        case 'u':
          *(guint32 *) ptr = g_variant_get_uint32 (value);
          return;

        case 'x':
          *(gint64 *) ptr = g_variant_get_int64 (value);
          return;

        case 't':
          *(guint64 *) ptr = g_variant_get_uint64 (value);
          return;

        case 'h':
          *(gint32 *) ptr = g_variant_get_handle (value);
          return;

        case 'd':
          *(gdouble *) ptr = g_variant_get_double (value);
          return;
        }
    }
  else
    {
      switch (*(*str)++)
        {
        case 'y':
          *(guchar *) ptr = 0;
          return;

        case 'n':
        case 'q':
          *(guint16 *) ptr = 0;
          return;

        case 'i':
        case 'u':
        case 'h':
        case 'b':
          *(guint32 *) ptr = 0;
          return;

        case 'x':
        case 't':
        case 'd':
          *(guint64 *) ptr = 0;
          return;
        }
    }

  g_assert_not_reached ();
}

/* Generic (recursive) {{{2 */
static void
g_variant_valist_skip (const gchar **str,
                       va_list      *app)
{
  if (g_variant_format_string_is_leaf (*str))
    g_variant_valist_skip_leaf (str, app);

  else if (**str == 'm') /* maybe */
    {
      (*str)++;

      if (!g_variant_format_string_is_nnp (*str))
        va_arg (*app, gboolean);

      g_variant_valist_skip (str, app);
    }
  else /* tuple, dictionary entry */
    {
      g_assert (**str == '(' || **str == '{');
      (*str)++;
      while (**str != ')' && **str != '}')
        g_variant_valist_skip (str, app);
      (*str)++;
    }
}

static GVariant *
g_variant_valist_new (const gchar **str,
                      va_list      *app)
{
  if (g_variant_format_string_is_leaf (*str))
    return g_variant_valist_new_leaf (str, app);

  if (**str == 'm') /* maybe */
    {
      GVariantType *type = NULL;
      GVariant *value = NULL;

      (*str)++;

      if (g_variant_format_string_is_nnp (*str))
        {
          gpointer nnp = va_arg (*app, gpointer);

          if (nnp != NULL)
            value = g_variant_valist_new_nnp (str, nnp);
          else
            type = g_variant_format_string_scan_type (*str, NULL, str);
        }
      else
        {
          gboolean just = va_arg (*app, gboolean);

          if (just)
            value = g_variant_valist_new (str, app);
          else
            {
              type = g_variant_format_string_scan_type (*str, NULL, NULL);
              g_variant_valist_skip (str, app);
            }
        }

      value = g_variant_new_maybe (type, value);

      if (type != NULL)
        g_variant_type_free (type);

      return value;
    }
  else /* tuple, dictionary entry */
    {
      GVariantBuilder b;

      if (**str == '(')
        g_variant_builder_init (&b, G_VARIANT_TYPE_TUPLE);
      else
        {
          g_assert (**str == '{');
          g_variant_builder_init (&b, G_VARIANT_TYPE_DICT_ENTRY);
        }

      (*str)++; /* '(' */
      while (**str != ')' && **str != '}')
        g_variant_builder_add_value (&b, g_variant_valist_new (str, app));
      (*str)++; /* ')' */

      return g_variant_builder_end (&b);
    }
}

static void
g_variant_valist_get (const gchar **str,
                      GVariant     *value,
                      gboolean      free,
                      va_list      *app)
{
  if (g_variant_format_string_is_leaf (*str))
    g_variant_valist_get_leaf (str, value, free, app);

  else if (**str == 'm')
    {
      (*str)++;

      if (value != NULL)
        value = g_variant_get_maybe (value);

      if (!g_variant_format_string_is_nnp (*str))
        {
          gboolean *ptr = va_arg (*app, gboolean *);

          if (ptr != NULL)
            *ptr = value != NULL;
        }

      g_variant_valist_get (str, value, free, app);

      if (value != NULL)
        g_variant_unref (value);
    }

  else /* tuple, dictionary entry */
    {
      gint index = 0;

      g_assert (**str == '(' || **str == '{');

      (*str)++;
      while (**str != ')' && **str != '}')
        {
          if (value != NULL)
            {
              GVariant *child = g_variant_get_child_value (value, index++);
              g_variant_valist_get (str, child, free, app);
              g_variant_unref (child);
            }
          else
            g_variant_valist_get (str, NULL, free, app);
        }
      (*str)++;
    }
}

/* User-facing API {{{2 */
/**
 * g_variant_new: (skip)
 * @format_string: a #GVariant format string
 * @...: arguments, as per @format_string
 *
 * Creates a new #GVariant instance.
 *
 * Think of this function as an analogue to g_strdup_printf().
 *
 * The type of the created instance and the arguments that are expected
 * by this function are determined by @format_string. See the section on
 * [GVariant format strings][gvariant-format-strings]. Please note that
 * the syntax of the format string is very likely to be extended in the
 * future.
 *
 * The first character of the format string must not be '*' '?' '@' or
 * 'r'; in essence, a new #GVariant must always be constructed by this
 * function (and not merely passed through it unmodified).
 *
 * Note that the arguments must be of the correct width for their types
 * specified in @format_string. This can be achieved by casting them. See
 * the [GVariant varargs documentation][gvariant-varargs].
 *
 * |[<!-- language="C" -->
 * MyFlags some_flags = FLAG_ONE | FLAG_TWO;
 * const gchar *some_strings[] = { "a", "b", "c", NULL };
 * GVariant *new_variant;
 *
 * new_variant = g_variant_new ("(t^as)",
 *                              // This cast is required.
 *                              (guint64) some_flags,
 *                              some_strings);
 * ]|
 *
 * Returns: a new floating #GVariant instance
 *
 * Since: 2.24
 **/
GVariant *
g_variant_new (const gchar *format_string,
               ...)
{
  GVariant *value;
  va_list ap;

  g_return_val_if_fail (valid_format_string (format_string, TRUE, NULL) &&
                        format_string[0] != '?' && format_string[0] != '@' &&
                        format_string[0] != '*' && format_string[0] != 'r',
                        NULL);

  va_start (ap, format_string);
  value = g_variant_new_va (format_string, NULL, &ap);
  va_end (ap);

  return value;
}

/**
 * g_variant_new_va: (skip)
 * @format_string: a string that is prefixed with a format string
 * @endptr: (nullable) (default NULL): location to store the end pointer,
 *          or %NULL
 * @app: a pointer to a #va_list
 *
 * This function is intended to be used by libraries based on
 * #GVariant that want to provide g_variant_new()-like functionality
 * to their users.
 *
 * The API is more general than g_variant_new() to allow a wider range
 * of possible uses.
 *
 * @format_string must still point to a valid format string, but it only
 * needs to be nul-terminated if @endptr is %NULL.  If @endptr is
 * non-%NULL then it is updated to point to the first character past the
 * end of the format string.
 *
 * @app is a pointer to a #va_list.  The arguments, according to
 * @format_string, are collected from this #va_list and the list is left
 * pointing to the argument following the last.
 *
 * Note that the arguments in @app must be of the correct width for their
 * types specified in @format_string when collected into the #va_list.
 * See the [GVariant varargs documentation][gvariant-varargs].
 *
 * These two generalisations allow mixing of multiple calls to
 * g_variant_new_va() and g_variant_get_va() within a single actual
 * varargs call by the user.
 *
 * The return value will be floating if it was a newly created GVariant
 * instance (for example, if the format string was "(ii)").  In the case
 * that the format_string was '*', '?', 'r', or a format starting with
 * '@' then the collected #GVariant pointer will be returned unmodified,
 * without adding any additional references.
 *
 * In order to behave correctly in all cases it is necessary for the
 * calling function to g_variant_ref_sink() the return result before
 * returning control to the user that originally provided the pointer.
 * At this point, the caller will have their own full reference to the
 * result.  This can also be done by adding the result to a container,
 * or by passing it to another g_variant_new() call.
 *
 * Returns: a new, usually floating, #GVariant
 *
 * Since: 2.24
 **/
GVariant *
g_variant_new_va (const gchar  *format_string,
                  const gchar **endptr,
                  va_list      *app)
{
  GVariant *value;

  g_return_val_if_fail (valid_format_string (format_string, !endptr, NULL),
                        NULL);
  g_return_val_if_fail (app != NULL, NULL);

  value = g_variant_valist_new (&format_string, app);

  if (endptr != NULL)
    *endptr = format_string;

  return value;
}

/**
 * g_variant_get: (skip)
 * @value: a #GVariant instance
 * @format_string: a #GVariant format string
 * @...: arguments, as per @format_string
 *
 * Deconstructs a #GVariant instance.
 *
 * Think of this function as an analogue to scanf().
 *
 * The arguments that are expected by this function are entirely
 * determined by @format_string.  @format_string also restricts the
 * permissible types of @value.  It is an error to give a value with
 * an incompatible type.  See the section on
 * [GVariant format strings][gvariant-format-strings].
 * Please note that the syntax of the format string is very likely to be
 * extended in the future.
 *
 * @format_string determines the C types that are used for unpacking
 * the values and also determines if the values are copied or borrowed,
 * see the section on
 * [GVariant format strings][gvariant-format-strings-pointers].
 *
 * Since: 2.24
 **/
void
g_variant_get (GVariant    *value,
               const gchar *format_string,
               ...)
{
  va_list ap;

  g_return_if_fail (valid_format_string (format_string, TRUE, value));

  /* if any direct-pointer-access formats are in use, flatten first */
  if (strchr (format_string, '&'))
    g_variant_get_data (value);

  va_start (ap, format_string);
  g_variant_get_va (value, format_string, NULL, &ap);
  va_end (ap);
}

/**
 * g_variant_get_va: (skip)
 * @value: a #GVariant
 * @format_string: a string that is prefixed with a format string
 * @endptr: (nullable) (default NULL): location to store the end pointer,
 *          or %NULL
 * @app: a pointer to a #va_list
 *
 * This function is intended to be used by libraries based on #GVariant
 * that want to provide g_variant_get()-like functionality to their
 * users.
 *
 * The API is more general than g_variant_get() to allow a wider range
 * of possible uses.
 *
 * @format_string must still point to a valid format string, but it only
 * need to be nul-terminated if @endptr is %NULL.  If @endptr is
 * non-%NULL then it is updated to point to the first character past the
 * end of the format string.
 *
 * @app is a pointer to a #va_list.  The arguments, according to
 * @format_string, are collected from this #va_list and the list is left
 * pointing to the argument following the last.
 *
 * These two generalisations allow mixing of multiple calls to
 * g_variant_new_va() and g_variant_get_va() within a single actual
 * varargs call by the user.
 *
 * @format_string determines the C types that are used for unpacking
 * the values and also determines if the values are copied or borrowed,
 * see the section on
 * [GVariant format strings][gvariant-format-strings-pointers].
 *
 * Since: 2.24
 **/
void
g_variant_get_va (GVariant     *value,
                  const gchar  *format_string,
                  const gchar **endptr,
                  va_list      *app)
{
  g_return_if_fail (valid_format_string (format_string, !endptr, value));
  g_return_if_fail (value != NULL);
  g_return_if_fail (app != NULL);

  /* if any direct-pointer-access formats are in use, flatten first */
  if (strchr (format_string, '&'))
    g_variant_get_data (value);

  g_variant_valist_get (&format_string, value, FALSE, app);

  if (endptr != NULL)
    *endptr = format_string;
}

/* Varargs-enabled Utility Functions {{{1 */

/**
 * g_variant_builder_add: (skip)
 * @builder: a #GVariantBuilder
 * @format_string: a #GVariant varargs format string
 * @...: arguments, as per @format_string
 *
 * Adds to a #GVariantBuilder.
 *
 * This call is a convenience wrapper that is exactly equivalent to
 * calling g_variant_new() followed by g_variant_builder_add_value().
 *
 * Note that the arguments must be of the correct width for their types
 * specified in @format_string. This can be achieved by casting them. See
 * the [GVariant varargs documentation][gvariant-varargs].
 *
 * This function might be used as follows:
 *
 * |[<!-- language="C" --> 
 * GVariant *
 * make_pointless_dictionary (void)
 * {
 *   GVariantBuilder builder;
 *   int i;
 *
 *   g_variant_builder_init (&builder, G_VARIANT_TYPE_ARRAY);
 *   for (i = 0; i < 16; i++)
 *     {
 *       gchar buf[3];
 *
 *       sprintf (buf, "%d", i);
 *       g_variant_builder_add (&builder, "{is}", i, buf);
 *     }
 *
 *   return g_variant_builder_end (&builder);
 * }
 * ]|
 *
 * Since: 2.24
 */
void
g_variant_builder_add (GVariantBuilder *builder,
                       const gchar     *format_string,
                       ...)
{
  GVariant *variant;
  va_list ap;

  va_start (ap, format_string);
  variant = g_variant_new_va (format_string, NULL, &ap);
  va_end (ap);

  g_variant_builder_add_value (builder, variant);
}

/**
 * g_variant_get_child: (skip)
 * @value: a container #GVariant
 * @index_: the index of the child to deconstruct
 * @format_string: a #GVariant format string
 * @...: arguments, as per @format_string
 *
 * Reads a child item out of a container #GVariant instance and
 * deconstructs it according to @format_string.  This call is
 * essentially a combination of g_variant_get_child_value() and
 * g_variant_get().
 *
 * @format_string determines the C types that are used for unpacking
 * the values and also determines if the values are copied or borrowed,
 * see the section on
 * [GVariant format strings][gvariant-format-strings-pointers].
 *
 * Since: 2.24
 **/
void
g_variant_get_child (GVariant    *value,
                     gsize        index_,
                     const gchar *format_string,
                     ...)
{
  GVariant *child;
  va_list ap;

  /* if any direct-pointer-access formats are in use, flatten first */
  if (strchr (format_string, '&'))
    g_variant_get_data (value);

  child = g_variant_get_child_value (value, index_);
  g_return_if_fail (valid_format_string (format_string, TRUE, child));

  va_start (ap, format_string);
  g_variant_get_va (child, format_string, NULL, &ap);
  va_end (ap);

  g_variant_unref (child);
}

/**
 * g_variant_iter_next: (skip)
 * @iter: a #GVariantIter
 * @format_string: a GVariant format string
 * @...: the arguments to unpack the value into
 *
 * Gets the next item in the container and unpacks it into the variable
 * argument list according to @format_string, returning %TRUE.
 *
 * If no more items remain then %FALSE is returned.
 *
 * All of the pointers given on the variable arguments list of this
 * function are assumed to point at uninitialised memory.  It is the
 * responsibility of the caller to free all of the values returned by
 * the unpacking process.
 *
 * Here is an example for memory management with g_variant_iter_next():
 * |[<!-- language="C" --> 
 *   // Iterates a dictionary of type 'a{sv}'
 *   void
 *   iterate_dictionary (GVariant *dictionary)
 *   {
 *     GVariantIter iter;
 *     GVariant *value;
 *     gchar *key;
 *
 *     g_variant_iter_init (&iter, dictionary);
 *     while (g_variant_iter_next (&iter, "{sv}", &key, &value))
 *       {
 *         g_print ("Item '%s' has type '%s'\n", key,
 *                  g_variant_get_type_string (value));
 *
 *         // must free data for ourselves
 *         g_variant_unref (value);
 *         g_free (key);
 *       }
 *   }
 * ]|
 *
 * For a solution that is likely to be more convenient to C programmers
 * when dealing with loops, see g_variant_iter_loop().
 *
 * @format_string determines the C types that are used for unpacking
 * the values and also determines if the values are copied or borrowed.
 *
 * See the section on
 * [GVariant format strings][gvariant-format-strings-pointers].
 *
 * Returns: %TRUE if a value was unpacked, or %FALSE if there as no value
 *
 * Since: 2.24
 **/
gboolean
g_variant_iter_next (GVariantIter *iter,
                     const gchar  *format_string,
                     ...)
{
  GVariant *value;

  value = g_variant_iter_next_value (iter);

  g_return_val_if_fail (valid_format_string (format_string, TRUE, value),
                        FALSE);

  if (value != NULL)
    {
      va_list ap;

      va_start (ap, format_string);
      g_variant_valist_get (&format_string, value, FALSE, &ap);
      va_end (ap);

      g_variant_unref (value);
    }

  return value != NULL;
}

/**
 * g_variant_iter_loop: (skip)
 * @iter: a #GVariantIter
 * @format_string: a GVariant format string
 * @...: the arguments to unpack the value into
 *
 * Gets the next item in the container and unpacks it into the variable
 * argument list according to @format_string, returning %TRUE.
 *
 * If no more items remain then %FALSE is returned.
 *
 * On the first call to this function, the pointers appearing on the
 * variable argument list are assumed to point at uninitialised memory.
 * On the second and later calls, it is assumed that the same pointers
 * will be given and that they will point to the memory as set by the
 * previous call to this function.  This allows the previous values to
 * be freed, as appropriate.
 *
 * This function is intended to be used with a while loop as
 * demonstrated in the following example.  This function can only be
 * used when iterating over an array.  It is only valid to call this
 * function with a string constant for the format string and the same
 * string constant must be used each time.  Mixing calls to this
 * function and g_variant_iter_next() or g_variant_iter_next_value() on
 * the same iterator causes undefined behavior.
 *
 * If you break out of a such a while loop using g_variant_iter_loop() then
 * you must free or unreference all the unpacked values as you would with
 * g_variant_get(). Failure to do so will cause a memory leak.
 *
 * Here is an example for memory management with g_variant_iter_loop():
 * |[<!-- language="C" --> 
 *   // Iterates a dictionary of type 'a{sv}'
 *   void
 *   iterate_dictionary (GVariant *dictionary)
 *   {
 *     GVariantIter iter;
 *     GVariant *value;
 *     gchar *key;
 *
 *     g_variant_iter_init (&iter, dictionary);
 *     while (g_variant_iter_loop (&iter, "{sv}", &key, &value))
 *       {
 *         g_print ("Item '%s' has type '%s'\n", key,
 *                  g_variant_get_type_string (value));
 *
 *         // no need to free 'key' and 'value' here
 *         // unless breaking out of this loop
 *       }
 *   }
 * ]|
 *
 * For most cases you should use g_variant_iter_next().
 *
 * This function is really only useful when unpacking into #GVariant or
 * #GVariantIter in order to allow you to skip the call to
 * g_variant_unref() or g_variant_iter_free().
 *
 * For example, if you are only looping over simple integer and string
 * types, g_variant_iter_next() is definitely preferred.  For string
 * types, use the '&' prefix to avoid allocating any memory at all (and
 * thereby avoiding the need to free anything as well).
 *
 * @format_string determines the C types that are used for unpacking
 * the values and also determines if the values are copied or borrowed.
 *
 * See the section on
 * [GVariant format strings][gvariant-format-strings-pointers].
 *
 * Returns: %TRUE if a value was unpacked, or %FALSE if there was no
 *          value
 *
 * Since: 2.24
 **/
gboolean
g_variant_iter_loop (GVariantIter *iter,
                     const gchar  *format_string,
                     ...)
{
  gboolean first_time = GVSI(iter)->loop_format == NULL;
  GVariant *value;
  va_list ap;

  g_return_val_if_fail (first_time ||
                        format_string == GVSI(iter)->loop_format,
                        FALSE);

  if (first_time)
    {
      TYPE_CHECK (GVSI(iter)->value, G_VARIANT_TYPE_ARRAY, FALSE);
      GVSI(iter)->loop_format = format_string;

      if (strchr (format_string, '&'))
        g_variant_get_data (GVSI(iter)->value);
    }

  value = g_variant_iter_next_value (iter);

  g_return_val_if_fail (!first_time ||
                        valid_format_string (format_string, TRUE, value),
                        FALSE);

  va_start (ap, format_string);
  g_variant_valist_get (&format_string, value, !first_time, &ap);
  va_end (ap);

  if (value != NULL)
    g_variant_unref (value);

  return value != NULL;
}

/* Serialised data {{{1 */
static GVariant *
g_variant_deep_copy (GVariant *value)
{
  switch (g_variant_classify (value))
    {
    case G_VARIANT_CLASS_MAYBE:
    case G_VARIANT_CLASS_ARRAY:
    case G_VARIANT_CLASS_TUPLE:
    case G_VARIANT_CLASS_DICT_ENTRY:
    case G_VARIANT_CLASS_VARIANT:
      {
        GVariantBuilder builder;
        GVariantIter iter;
        GVariant *child;

        g_variant_builder_init (&builder, g_variant_get_type (value));
        g_variant_iter_init (&iter, value);

        while ((child = g_variant_iter_next_value (&iter)))
          {
            g_variant_builder_add_value (&builder, g_variant_deep_copy (child));
            g_variant_unref (child);
          }

        return g_variant_builder_end (&builder);
      }

    case G_VARIANT_CLASS_BOOLEAN:
      return g_variant_new_boolean (g_variant_get_boolean (value));

    case G_VARIANT_CLASS_BYTE:
      return g_variant_new_byte (g_variant_get_byte (value));

    case G_VARIANT_CLASS_INT16:
      return g_variant_new_int16 (g_variant_get_int16 (value));

    case G_VARIANT_CLASS_UINT16:
      return g_variant_new_uint16 (g_variant_get_uint16 (value));

    case G_VARIANT_CLASS_INT32:
      return g_variant_new_int32 (g_variant_get_int32 (value));

    case G_VARIANT_CLASS_UINT32:
      return g_variant_new_uint32 (g_variant_get_uint32 (value));

    case G_VARIANT_CLASS_INT64:
      return g_variant_new_int64 (g_variant_get_int64 (value));

    case G_VARIANT_CLASS_UINT64:
      return g_variant_new_uint64 (g_variant_get_uint64 (value));

    case G_VARIANT_CLASS_HANDLE:
      return g_variant_new_handle (g_variant_get_handle (value));

    case G_VARIANT_CLASS_DOUBLE:
      return g_variant_new_double (g_variant_get_double (value));

    case G_VARIANT_CLASS_STRING:
      return g_variant_new_string (g_variant_get_string (value, NULL));

    case G_VARIANT_CLASS_OBJECT_PATH:
      return g_variant_new_object_path (g_variant_get_string (value, NULL));

    case G_VARIANT_CLASS_SIGNATURE:
      return g_variant_new_signature (g_variant_get_string (value, NULL));
    }

  g_assert_not_reached ();
}

/**
 * g_variant_get_normal_form:
 * @value: a #GVariant
 *
 * Gets a #GVariant instance that has the same value as @value and is
 * trusted to be in normal form.
 *
 * If @value is already trusted to be in normal form then a new
 * reference to @value is returned.
 *
 * If @value is not already trusted, then it is scanned to check if it
 * is in normal form.  If it is found to be in normal form then it is
 * marked as trusted and a new reference to it is returned.
 *
 * If @value is found not to be in normal form then a new trusted
 * #GVariant is created with the same value as @value.
 *
 * It makes sense to call this function if you've received #GVariant
 * data from untrusted sources and you want to ensure your serialised
 * output is definitely in normal form.
 *
 * If @value is already in normal form, a new reference will be returned
 * (which will be floating if @value is floating). If it is not in normal form,
 * the newly created #GVariant will be returned with a single non-floating
 * reference. Typically, g_variant_take_ref() should be called on the return
 * value from this function to guarantee ownership of a single non-floating
 * reference to it.
 *
 * Returns: (transfer full): a trusted #GVariant
 *
 * Since: 2.24
 **/
GVariant *
g_variant_get_normal_form (GVariant *value)
{
  GVariant *trusted;

  if (g_variant_is_normal_form (value))
    return g_variant_ref (value);

  trusted = g_variant_deep_copy (value);
  g_assert (g_variant_is_trusted (trusted));

  return g_variant_ref_sink (trusted);
}

/**
 * g_variant_byteswap:
 * @value: a #GVariant
 *
 * Performs a byteswapping operation on the contents of @value.  The
 * result is that all multi-byte numeric data contained in @value is
 * byteswapped.  That includes 16, 32, and 64bit signed and unsigned
 * integers as well as file handles and double precision floating point
 * values.
 *
 * This function is an identity mapping on any value that does not
 * contain multi-byte numeric data.  That include strings, booleans,
 * bytes and containers containing only these things (recursively).
 *
 * The returned value is always in normal form and is marked as trusted.
 *
 * Returns: (transfer full): the byteswapped form of @value
 *
 * Since: 2.24
 **/
GVariant *
g_variant_byteswap (GVariant *value)
{
  GVariantTypeInfo *type_info;
  guint alignment;
  GVariant *new;

  type_info = g_variant_get_type_info (value);

  g_variant_type_info_query (type_info, &alignment, NULL);

  if (alignment)
    /* (potentially) contains multi-byte numeric data */
    {
      GVariantSerialised serialised;
      GVariant *trusted;
      GBytes *bytes;

      trusted = g_variant_get_normal_form (value);
      serialised.type_info = g_variant_get_type_info (trusted);
      serialised.size = g_variant_get_size (trusted);
      serialised.data = g_malloc (serialised.size);
      g_variant_store (trusted, serialised.data);
      g_variant_unref (trusted);

      g_variant_serialised_byteswap (serialised);

      bytes = g_bytes_new_take (serialised.data, serialised.size);
      new = g_variant_new_from_bytes (g_variant_get_type (value), bytes, TRUE);
      g_bytes_unref (bytes);
    }
  else
    /* contains no multi-byte data */
    new = value;

  return g_variant_ref_sink (new);
}

/**
 * g_variant_new_from_data:
 * @type: a definite #GVariantType
 * @data: (array length=size) (element-type guint8): the serialised data
 * @size: the size of @data
 * @trusted: %TRUE if @data is definitely in normal form
 * @notify: (scope async): function to call when @data is no longer needed
 * @user_data: data for @notify
 *
 * Creates a new #GVariant instance from serialised data.
 *
 * @type is the type of #GVariant instance that will be constructed.
 * The interpretation of @data depends on knowing the type.
 *
 * @data is not modified by this function and must remain valid with an
 * unchanging value until such a time as @notify is called with
 * @user_data.  If the contents of @data change before that time then
 * the result is undefined.
 *
 * If @data is trusted to be serialised data in normal form then
 * @trusted should be %TRUE.  This applies to serialised data created
 * within this process or read from a trusted location on the disk (such
 * as a file installed in /usr/lib alongside your application).  You
 * should set trusted to %FALSE if @data is read from the network, a
 * file in the user's home directory, etc.
 *
 * If @data was not stored in this machine's native endianness, any multi-byte
 * numeric values in the returned variant will also be in non-native
 * endianness. g_variant_byteswap() can be used to recover the original values.
 *
 * @notify will be called with @user_data when @data is no longer
 * needed.  The exact time of this call is unspecified and might even be
 * before this function returns.
 *
 * Returns: (transfer none): a new floating #GVariant of type @type
 *
 * Since: 2.24
 **/
GVariant *
g_variant_new_from_data (const GVariantType *type,
                         gconstpointer       data,
                         gsize               size,
                         gboolean            trusted,
                         GDestroyNotify      notify,
                         gpointer            user_data)
{
  GVariant *value;
  GBytes *bytes;

  g_return_val_if_fail (g_variant_type_is_definite (type), NULL);
  g_return_val_if_fail (data != NULL || size == 0, NULL);

  if (notify)
    bytes = g_bytes_new_with_free_func (data, size, notify, user_data);
  else
    bytes = g_bytes_new_static (data, size);

  value = g_variant_new_from_bytes (type, bytes, trusted);
  g_bytes_unref (bytes);

  return value;
}

/* Epilogue {{{1 */
/* vim:set foldmethod=marker: */
