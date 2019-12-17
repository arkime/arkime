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
 */

#include "config.h"

#include <glib/gvariant-core.h>

#include <glib/gvariant-serialiser.h>
#include <glib/gtestutils.h>
#include <glib/gbitlock.h>
#include <glib/gatomic.h>
#include <glib/gbytes.h>
#include <glib/gslice.h>
#include <glib/gmem.h>
#include <string.h>


/*
 * This file includes the structure definition for GVariant and a small
 * set of functions that are allowed to access the structure directly.
 *
 * This minimises the amount of code that can possibly touch a GVariant
 * structure directly to a few simple fundamental operations.  These few
 * operations are written to be completely threadsafe with respect to
 * all possible outside access.  This means that we only need to be
 * concerned about thread safety issues in this one small file.
 *
 * Most GVariant API functions are in gvariant.c.
 */

/**
 * GVariant:
 *
 * #GVariant is an opaque data structure and can only be accessed
 * using the following functions.
 *
 * Since: 2.24
 **/
struct _GVariant
/* see below for field member documentation */
{
  GVariantTypeInfo *type_info;
  gsize size;

  union
  {
    struct
    {
      GBytes *bytes;
      gconstpointer data;
    } serialised;

    struct
    {
      GVariant **children;
      gsize n_children;
    } tree;
  } contents;

  gint state;
  gint ref_count;
};

/* struct GVariant:
 *
 * There are two primary forms of GVariant instances: "serialised form"
 * and "tree form".
 *
 * "serialised form": A serialised GVariant instance stores its value in
 *                    the GVariant serialisation format.  All
 *                    basic-typed instances (ie: non-containers) are in
 *                    serialised format, as are some containers.
 *
 * "tree form": Some containers are in "tree form".  In this case,
 *              instead of containing the serialised data for the
 *              container, the instance contains an array of pointers to
 *              the child values of the container (thus forming a tree).
 *
 * It is possible for an instance to transition from tree form to
 * serialised form.  This happens, implicitly, if the serialised data is
 * requested (eg: via g_variant_get_data()).  Serialised form instances
 * never transition into tree form.
 *
 *
 * The fields of the structure are documented here:
 *
 * type_info: this is a reference to a GVariantTypeInfo describing the
 *            type of the instance.  When the instance is freed, this
 *            reference must be released with g_variant_type_info_unref().
 *
 *            The type_info field never changes during the life of the
 *            instance, so it can be accessed without a lock.
 *
 * size: this is the size of the serialised form for the instance, if it
 *       is known.  If the instance is in serialised form then it is, by
 *       definition, known.  If the instance is in tree form then it may
 *       be unknown (in which case it is -1).  It is possible for the
 *       size to be known when in tree form if, for example, the user
 *       has called g_variant_get_size() without calling
 *       g_variant_get_data().  Additionally, even when the user calls
 *       g_variant_get_data() the size of the data must first be
 *       determined so that a large enough buffer can be allocated for
 *       the data.
 *
 *       Once the size is known, it can never become unknown again.
 *       g_variant_ensure_size() is used to ensure that the size is in
 *       the known state -- it calculates the size if needed.  After
 *       that, the size field can be accessed without a lock.
 *
 * contents: a union containing either the information associated with
 *           holding a value in serialised form or holding a value in
 *           tree form.
 *
 *   .serialised: Only valid when the instance is in serialised form.
 *
 *                Since an instance can never transition away from
 *                serialised form, once these fields are set, they will
 *                never be changed.  It is therefore valid to access
 *                them without holding a lock.
 *
 *     .bytes:  the #GBytes that contains the memory pointed to by
 *              .data, or %NULL if .data is %NULL.  In the event that
 *              the instance was deserialised from another instance,
 *              then the bytes will be shared by both of them.  When
 *              the instance is freed, this reference must be released
 *              with g_bytes_unref().
 *
 *     .data: the serialised data (of size 'size') of the instance.
 *            This pointer should not be freed or modified in any way.
 *            #GBytes is responsible for memory management.
 *
 *            This pointer may be %NULL in two cases:
 *
 *              - if the serialised size of the instance is 0
 *
 *              - if the instance is of a fixed-sized type and was
 *                deserialised out of a corrupted container such that
 *                the container contains too few bytes to point to the
 *                entire proper fixed-size of this instance.  In this
 *                case, 'size' will still be equal to the proper fixed
 *                size, but this pointer will be %NULL.  This is exactly
 *                the reason that g_variant_get_data() sometimes returns
 *                %NULL.  For all other calls, the effect should be as
 *                if .data pointed to the appropriate number of nul
 *                bytes.
 *
 *   .tree: Only valid when the instance is in tree form.
 *
 *          Note that accesses from other threads could result in
 *          conversion of the instance from tree form to serialised form
 *          at any time.  For this reason, the instance lock must always
 *          be held while performing any operations on 'contents.tree'.
 *
 *     .children: the array of the child instances of this instance.
 *                When the instance is freed (or converted to serialised
 *                form) then each child must have g_variant_unref()
 *                called on it and the array must be freed using
 *                g_free().
 *
 *     .n_children: the number of items in the .children array.
 *
 * state: a bitfield describing the state of the instance.  It is a
 *        bitwise-or of the following STATE_* constants:
 *
 *    STATE_LOCKED: the instance lock is held.  This is the bit used by
 *                  g_bit_lock().
 *
 *    STATE_SERIALISED: the instance is in serialised form.  If this
 *                      flag is not set then the instance is in tree
 *                      form.
 *
 *    STATE_TRUSTED: for serialised form instances, this means that the
 *                   serialised data is known to be in normal form (ie:
 *                   not corrupted).
 *
 *                   For tree form instances, this means that all of the
 *                   child instances in the contents.tree.children array
 *                   are trusted.  This means that if the container is
 *                   serialised then the resulting data will be in
 *                   normal form.
 *
 *                   If this flag is unset it does not imply that the
 *                   data is corrupted.  It merely means that we're not
 *                   sure that it's valid.  See g_variant_is_trusted().
 *
 *    STATE_FLOATING: if this flag is set then the object has a floating
 *                    reference.  See g_variant_ref_sink().
 *
 * ref_count: the reference count of the instance
 */
#define STATE_LOCKED     1
#define STATE_SERIALISED 2
#define STATE_TRUSTED    4
#define STATE_FLOATING   8

/* -- private -- */
/* < private >
 * g_variant_lock:
 * @value: a #GVariant
 *
 * Locks @value for performing sensitive operations.
 */
static void
g_variant_lock (GVariant *value)
{
  g_bit_lock (&value->state, 0);
}

/* < private >
 * g_variant_unlock:
 * @value: a #GVariant
 *
 * Unlocks @value after performing sensitive operations.
 */
static void
g_variant_unlock (GVariant *value)
{
  g_bit_unlock (&value->state, 0);
}

/* < private >
 * g_variant_release_children:
 * @value: a #GVariant
 *
 * Releases the reference held on each child in the 'children' array of
 * @value and frees the array itself.  @value must be in tree form.
 *
 * This is done when freeing a tree-form instance or converting it to
 * serialised form.
 *
 * The current thread must hold the lock on @value.
 */
static void
g_variant_release_children (GVariant *value)
{
  gsize i;

  g_assert (value->state & STATE_LOCKED);
  g_assert (~value->state & STATE_SERIALISED);

  for (i = 0; i < value->contents.tree.n_children; i++)
    g_variant_unref (value->contents.tree.children[i]);

  g_free (value->contents.tree.children);
}

/* This begins the main body of the recursive serialiser.
 *
 * There are 3 functions here that work as a team with the serialiser to
 * get things done.  g_variant_store() has a trivial role, but as a
 * public API function, it has its definition elsewhere.
 *
 * Note that "serialisation" of an instance does not mean that the
 * instance is converted to serialised form -- it means that the
 * serialised form of an instance is written to an external buffer.
 * g_variant_ensure_serialised() (which is not part of this set of
 * functions) is the function that is responsible for converting an
 * instance to serialised form.
 *
 * We are only concerned here with container types since non-container
 * instances are always in serialised form.  For these instances,
 * storing their serialised form merely involves a memcpy().
 *
 * Serialisation is a two-step process.  First, the size of the
 * serialised data must be calculated so that an appropriately-sized
 * buffer can be allocated.  Second, the data is written into the
 * buffer.
 *
 * Determining the size:
 *   The process of determining the size is triggered by a call to
 *   g_variant_ensure_size() on a container.  This invokes the
 *   serialiser code to determine the size.  The serialiser is passed
 *   g_variant_fill_gvs() as a callback.
 *
 *   g_variant_fill_gvs() is called by the serialiser on each child of
 *   the container which, in turn, calls g_variant_ensure_size() on
 *   itself and fills in the result of its own size calculation.
 *
 *   The serialiser uses the size information from the children to
 *   calculate the size needed for the entire container.
 *
 * Writing the data:
 *   After the buffer has been allocated, g_variant_serialise() is
 *   called on the container.  This invokes the serialiser code to write
 *   the bytes to the container.  The serialiser is, again, passed
 *   g_variant_fill_gvs() as a callback.
 *
 *   This time, when g_variant_fill_gvs() is called for each child, the
 *   child is given a pointer to a sub-region of the allocated buffer
 *   where it should write its data.  This is done by calling
 *   g_variant_store().  In the event that the instance is in serialised
 *   form this means a memcpy() of the serialised data into the
 *   allocated buffer.  In the event that the instance is in tree form
 *   this means a recursive call back into g_variant_serialise().
 *
 *
 * The forward declaration here allows corecursion via callback:
 */
static void g_variant_fill_gvs (GVariantSerialised *, gpointer);

/* < private >
 * g_variant_ensure_size:
 * @value: a #GVariant
 *
 * Ensures that the ->size field of @value is filled in properly.  This
 * must be done as a precursor to any serialisation of the value in
 * order to know how large of a buffer is needed to store the data.
 *
 * The current thread must hold the lock on @value.
 */
static void
g_variant_ensure_size (GVariant *value)
{
  g_assert (value->state & STATE_LOCKED);

  if (value->size == (gssize) -1)
    {
      gpointer *children;
      gsize n_children;

      children = (gpointer *) value->contents.tree.children;
      n_children = value->contents.tree.n_children;
      value->size = g_variant_serialiser_needed_size (value->type_info,
                                                      g_variant_fill_gvs,
                                                      children, n_children);
    }
}

/* < private >
 * g_variant_serialise:
 * @value: a #GVariant
 * @data: an appropriately-sized buffer
 *
 * Serialises @value into @data.  @value must be in tree form.
 *
 * No change is made to @value.
 *
 * The current thread must hold the lock on @value.
 */
static void
g_variant_serialise (GVariant *value,
                     gpointer  data)
{
  GVariantSerialised serialised = { 0, };
  gpointer *children;
  gsize n_children;

  g_assert (~value->state & STATE_SERIALISED);
  g_assert (value->state & STATE_LOCKED);

  serialised.type_info = value->type_info;
  serialised.size = value->size;
  serialised.data = data;

  children = (gpointer *) value->contents.tree.children;
  n_children = value->contents.tree.n_children;

  g_variant_serialiser_serialise (serialised, g_variant_fill_gvs,
                                  children, n_children);
}

/* < private >
 * g_variant_fill_gvs:
 * @serialised: a pointer to a #GVariantSerialised
 * @data: a #GVariant instance
 *
 * This is the callback that is passed by a tree-form container instance
 * to the serialiser.  This callback gets called on each child of the
 * container.  Each child is responsible for performing the following
 * actions:
 *
 *  - reporting its type
 *
 *  - reporting its serialised size (requires knowing the size first)
 *
 *  - possibly storing its serialised form into the provided buffer
 */
static void
g_variant_fill_gvs (GVariantSerialised *serialised,
                    gpointer            data)
{
  GVariant *value = data;

  g_variant_lock (value);
  g_variant_ensure_size (value);
  g_variant_unlock (value);

  if (serialised->type_info == NULL)
    serialised->type_info = value->type_info;
  g_assert (serialised->type_info == value->type_info);

  if (serialised->size == 0)
    serialised->size = value->size;
  g_assert (serialised->size == value->size);

  if (serialised->data)
    /* g_variant_store() is a public API, so it
     * it will reacquire the lock if it needs to.
     */
    g_variant_store (value, serialised->data);
}

/* this ends the main body of the recursive serialiser */

/* < private >
 * g_variant_ensure_serialised:
 * @value: a #GVariant
 *
 * Ensures that @value is in serialised form.
 *
 * If @value is in tree form then this function ensures that the
 * serialised size is known and then allocates a buffer of that size and
 * serialises the instance into the buffer.  The 'children' array is
 * then released and the instance is set to serialised form based on the
 * contents of the buffer.
 *
 * The current thread must hold the lock on @value.
 */
static void
g_variant_ensure_serialised (GVariant *value)
{
  g_assert (value->state & STATE_LOCKED);

  if (~value->state & STATE_SERIALISED)
    {
      GBytes *bytes;
      gpointer data;

      g_variant_ensure_size (value);
      data = g_malloc (value->size);
      g_variant_serialise (value, data);

      g_variant_release_children (value);

      bytes = g_bytes_new_take (data, value->size);
      value->contents.serialised.data = g_bytes_get_data (bytes, NULL);
      value->contents.serialised.bytes = bytes;
      value->state |= STATE_SERIALISED;
    }
}

/* < private >
 * g_variant_alloc:
 * @type: the type of the new instance
 * @serialised: if the instance will be in serialised form
 * @trusted: if the instance will be trusted
 *
 * Allocates a #GVariant instance and does some common work (such as
 * looking up and filling in the type info), setting the state field,
 * and setting the ref_count to 1.
 *
 * Returns: a new #GVariant with a floating reference
 */
static GVariant *
g_variant_alloc (const GVariantType *type,
                 gboolean            serialised,
                 gboolean            trusted)
{
  GVariant *value;

  value = g_slice_new (GVariant);
  value->type_info = g_variant_type_info_get (type);
  value->state = (serialised ? STATE_SERIALISED : 0) |
                 (trusted ? STATE_TRUSTED : 0) |
                 STATE_FLOATING;
  value->size = (gssize) -1;
  value->ref_count = 1;

  return value;
}

/**
 * g_variant_new_from_bytes:
 * @type: a #GVariantType
 * @bytes: a #GBytes
 * @trusted: if the contents of @bytes are trusted
 *
 * Constructs a new serialised-mode #GVariant instance.  This is the
 * inner interface for creation of new serialised values that gets
 * called from various functions in gvariant.c.
 *
 * A reference is taken on @bytes.
 *
 * Returns: (transfer none): a new #GVariant with a floating reference
 *
 * Since: 2.36
 */
GVariant *
g_variant_new_from_bytes (const GVariantType *type,
                          GBytes             *bytes,
                          gboolean            trusted)
{
  GVariant *value;
  guint alignment;
  gsize size;

  value = g_variant_alloc (type, TRUE, trusted);

  value->contents.serialised.bytes = g_bytes_ref (bytes);

  g_variant_type_info_query (value->type_info,
                             &alignment, &size);

  if (size && g_bytes_get_size (bytes) != size)
    {
      /* Creating a fixed-sized GVariant with a bytes of the wrong
       * size.
       *
       * We should do the equivalent of pulling a fixed-sized child out
       * of a brozen container (ie: data is NULL size is equal to the correct
       * fixed size).
       */
      value->contents.serialised.data = NULL;
      value->size = size;
    }
  else
    {
      value->contents.serialised.data = g_bytes_get_data (bytes, &value->size);
    }

  return value;
}

/* -- internal -- */

/* < internal >
 * g_variant_new_from_children:
 * @type: a #GVariantType
 * @children: an array of #GVariant pointers.  Consumed.
 * @n_children: the length of @children
 * @trusted: %TRUE if every child in @children in trusted
 *
 * Constructs a new tree-mode #GVariant instance.  This is the inner
 * interface for creation of new serialised values that gets called from
 * various functions in gvariant.c.
 *
 * @children is consumed by this function.  g_free() will be called on
 * it some time later.
 *
 * Returns: a new #GVariant with a floating reference
 */
GVariant *
g_variant_new_from_children (const GVariantType  *type,
                             GVariant           **children,
                             gsize                n_children,
                             gboolean             trusted)
{
  GVariant *value;

  value = g_variant_alloc (type, FALSE, trusted);
  value->contents.tree.children = children;
  value->contents.tree.n_children = n_children;

  return value;
}

/* < internal >
 * g_variant_get_type_info:
 * @value: a #GVariant
 *
 * Returns the #GVariantTypeInfo corresponding to the type of @value.  A
 * reference is not added, so the return value is only good for the
 * duration of the life of @value.
 *
 * Returns: the #GVariantTypeInfo for @value
 */
GVariantTypeInfo *
g_variant_get_type_info (GVariant *value)
{
  return value->type_info;
}

/* < internal >
 * g_variant_is_trusted:
 * @value: a #GVariant
 *
 * Determines if @value is trusted by #GVariant to contain only
 * fully-valid data.  All values constructed solely via #GVariant APIs
 * are trusted, but values containing data read in from other sources
 * are usually not trusted.
 *
 * The main advantage of trusted data is that certain checks can be
 * skipped.  For example, we don't need to check that a string is
 * properly nul-terminated or that an object path is actually a
 * properly-formatted object path.
 *
 * Returns: if @value is trusted
 */
gboolean
g_variant_is_trusted (GVariant *value)
{
  return (value->state & STATE_TRUSTED) != 0;
}

/* -- public -- */

/**
 * g_variant_unref:
 * @value: a #GVariant
 *
 * Decreases the reference count of @value.  When its reference count
 * drops to 0, the memory used by the variant is freed.
 *
 * Since: 2.24
 **/
void
g_variant_unref (GVariant *value)
{
  g_return_if_fail (value != NULL);
  g_return_if_fail (value->ref_count > 0);

  if (g_atomic_int_dec_and_test (&value->ref_count))
    {
      if G_UNLIKELY (value->state & STATE_LOCKED)
        g_critical ("attempting to free a locked GVariant instance.  "
                    "This should never happen.");

      value->state |= STATE_LOCKED;

      g_variant_type_info_unref (value->type_info);

      if (value->state & STATE_SERIALISED)
        g_bytes_unref (value->contents.serialised.bytes);
      else
        g_variant_release_children (value);

      memset (value, 0, sizeof (GVariant));
      g_slice_free (GVariant, value);
    }
}

/**
 * g_variant_ref:
 * @value: a #GVariant
 *
 * Increases the reference count of @value.
 *
 * Returns: the same @value
 *
 * Since: 2.24
 **/
GVariant *
g_variant_ref (GVariant *value)
{
  g_return_val_if_fail (value != NULL, NULL);
  g_return_val_if_fail (value->ref_count > 0, NULL);

  g_atomic_int_inc (&value->ref_count);

  return value;
}

/**
 * g_variant_ref_sink:
 * @value: a #GVariant
 *
 * #GVariant uses a floating reference count system.  All functions with
 * names starting with `g_variant_new_` return floating
 * references.
 *
 * Calling g_variant_ref_sink() on a #GVariant with a floating reference
 * will convert the floating reference into a full reference.  Calling
 * g_variant_ref_sink() on a non-floating #GVariant results in an
 * additional normal reference being added.
 *
 * In other words, if the @value is floating, then this call "assumes
 * ownership" of the floating reference, converting it to a normal
 * reference.  If the @value is not floating, then this call adds a
 * new normal reference increasing the reference count by one.
 *
 * All calls that result in a #GVariant instance being inserted into a
 * container will call g_variant_ref_sink() on the instance.  This means
 * that if the value was just created (and has only its floating
 * reference) then the container will assume sole ownership of the value
 * at that point and the caller will not need to unreference it.  This
 * makes certain common styles of programming much easier while still
 * maintaining normal refcounting semantics in situations where values
 * are not floating.
 *
 * Returns: the same @value
 *
 * Since: 2.24
 **/
GVariant *
g_variant_ref_sink (GVariant *value)
{
  g_return_val_if_fail (value != NULL, NULL);
  g_return_val_if_fail (value->ref_count > 0, NULL);

  g_variant_lock (value);

  if (~value->state & STATE_FLOATING)
    g_variant_ref (value);
  else
    value->state &= ~STATE_FLOATING;

  g_variant_unlock (value);

  return value;
}

/**
 * g_variant_take_ref:
 * @value: a #GVariant
 *
 * If @value is floating, sink it.  Otherwise, do nothing.
 *
 * Typically you want to use g_variant_ref_sink() in order to
 * automatically do the correct thing with respect to floating or
 * non-floating references, but there is one specific scenario where
 * this function is helpful.
 *
 * The situation where this function is helpful is when creating an API
 * that allows the user to provide a callback function that returns a
 * #GVariant.  We certainly want to allow the user the flexibility to
 * return a non-floating reference from this callback (for the case
 * where the value that is being returned already exists).
 *
 * At the same time, the style of the #GVariant API makes it likely that
 * for newly-created #GVariant instances, the user can be saved some
 * typing if they are allowed to return a #GVariant with a floating
 * reference.
 *
 * Using this function on the return value of the user's callback allows
 * the user to do whichever is more convenient for them.  The caller
 * will alway receives exactly one full reference to the value: either
 * the one that was returned in the first place, or a floating reference
 * that has been converted to a full reference.
 *
 * This function has an odd interaction when combined with
 * g_variant_ref_sink() running at the same time in another thread on
 * the same #GVariant instance.  If g_variant_ref_sink() runs first then
 * the result will be that the floating reference is converted to a hard
 * reference.  If g_variant_take_ref() runs first then the result will
 * be that the floating reference is converted to a hard reference and
 * an additional reference on top of that one is added.  It is best to
 * avoid this situation.
 *
 * Returns: the same @value
 **/
GVariant *
g_variant_take_ref (GVariant *value)
{
  g_return_val_if_fail (value != NULL, NULL);
  g_return_val_if_fail (value->ref_count > 0, NULL);

  g_atomic_int_and (&value->state, ~STATE_FLOATING);

  return value;
}

/**
 * g_variant_is_floating:
 * @value: a #GVariant
 *
 * Checks whether @value has a floating reference count.
 *
 * This function should only ever be used to assert that a given variant
 * is or is not floating, or for debug purposes. To acquire a reference
 * to a variant that might be floating, always use g_variant_ref_sink()
 * or g_variant_take_ref().
 *
 * See g_variant_ref_sink() for more information about floating reference
 * counts.
 *
 * Returns: whether @value is floating
 *
 * Since: 2.26
 **/
gboolean
g_variant_is_floating (GVariant *value)
{
  g_return_val_if_fail (value != NULL, FALSE);

  return (value->state & STATE_FLOATING) != 0;
}

/**
 * g_variant_get_size:
 * @value: a #GVariant instance
 *
 * Determines the number of bytes that would be required to store @value
 * with g_variant_store().
 *
 * If @value has a fixed-sized type then this function always returned
 * that fixed size.
 *
 * In the case that @value is already in serialised form or the size has
 * already been calculated (ie: this function has been called before)
 * then this function is O(1).  Otherwise, the size is calculated, an
 * operation which is approximately O(n) in the number of values
 * involved.
 *
 * Returns: the serialised size of @value
 *
 * Since: 2.24
 **/
gsize
g_variant_get_size (GVariant *value)
{
  g_variant_lock (value);
  g_variant_ensure_size (value);
  g_variant_unlock (value);

  return value->size;
}

/**
 * g_variant_get_data:
 * @value: a #GVariant instance
 *
 * Returns a pointer to the serialised form of a #GVariant instance.
 * The returned data may not be in fully-normalised form if read from an
 * untrusted source.  The returned data must not be freed; it remains
 * valid for as long as @value exists.
 *
 * If @value is a fixed-sized value that was deserialised from a
 * corrupted serialised container then %NULL may be returned.  In this
 * case, the proper thing to do is typically to use the appropriate
 * number of nul bytes in place of @value.  If @value is not fixed-sized
 * then %NULL is never returned.
 *
 * In the case that @value is already in serialised form, this function
 * is O(1).  If the value is not already in serialised form,
 * serialisation occurs implicitly and is approximately O(n) in the size
 * of the result.
 *
 * To deserialise the data returned by this function, in addition to the
 * serialised data, you must know the type of the #GVariant, and (if the
 * machine might be different) the endianness of the machine that stored
 * it. As a result, file formats or network messages that incorporate
 * serialised #GVariants must include this information either
 * implicitly (for instance "the file always contains a
 * %G_VARIANT_TYPE_VARIANT and it is always in little-endian order") or
 * explicitly (by storing the type and/or endianness in addition to the
 * serialised data).
 *
 * Returns: (transfer none): the serialised form of @value, or %NULL
 *
 * Since: 2.24
 **/
gconstpointer
g_variant_get_data (GVariant *value)
{
  g_variant_lock (value);
  g_variant_ensure_serialised (value);
  g_variant_unlock (value);

  return value->contents.serialised.data;
}

/**
 * g_variant_get_data_as_bytes:
 * @value: a #GVariant
 *
 * Returns a pointer to the serialised form of a #GVariant instance.
 * The semantics of this function are exactly the same as
 * g_variant_get_data(), except that the returned #GBytes holds
 * a reference to the variant data.
 *
 * Returns: (transfer full): A new #GBytes representing the variant data
 *
 * Since: 2.36
 */ 
GBytes *
g_variant_get_data_as_bytes (GVariant *value)
{
  const gchar *bytes_data;
  const gchar *data;
  gsize bytes_size;
  gsize size;

  g_variant_lock (value);
  g_variant_ensure_serialised (value);
  g_variant_unlock (value);

  bytes_data = g_bytes_get_data (value->contents.serialised.bytes, &bytes_size);
  data = value->contents.serialised.data;
  size = value->size;

  if (data == bytes_data && size == bytes_size)
    return g_bytes_ref (value->contents.serialised.bytes);
  else
    return g_bytes_new_from_bytes (value->contents.serialised.bytes,
                                   data - bytes_data, size);
}


/**
 * g_variant_n_children:
 * @value: a container #GVariant
 *
 * Determines the number of children in a container #GVariant instance.
 * This includes variants, maybes, arrays, tuples and dictionary
 * entries.  It is an error to call this function on any other type of
 * #GVariant.
 *
 * For variants, the return value is always 1.  For values with maybe
 * types, it is always zero or one.  For arrays, it is the length of the
 * array.  For tuples it is the number of tuple items (which depends
 * only on the type).  For dictionary entries, it is always 2
 *
 * This function is O(1).
 *
 * Returns: the number of children in the container
 *
 * Since: 2.24
 **/
gsize
g_variant_n_children (GVariant *value)
{
  gsize n_children;

  g_variant_lock (value);

  if (value->state & STATE_SERIALISED)
    {
      GVariantSerialised serialised = {
        value->type_info,
        (gpointer) value->contents.serialised.data,
        value->size
      };

      n_children = g_variant_serialised_n_children (serialised);
    }
  else
    n_children = value->contents.tree.n_children;

  g_variant_unlock (value);

  return n_children;
}

/**
 * g_variant_get_child_value:
 * @value: a container #GVariant
 * @index_: the index of the child to fetch
 *
 * Reads a child item out of a container #GVariant instance.  This
 * includes variants, maybes, arrays, tuples and dictionary
 * entries.  It is an error to call this function on any other type of
 * #GVariant.
 *
 * It is an error if @index_ is greater than the number of child items
 * in the container.  See g_variant_n_children().
 *
 * The returned value is never floating.  You should free it with
 * g_variant_unref() when you're done with it.
 *
 * This function is O(1).
 *
 * Returns: (transfer full): the child at the specified index
 *
 * Since: 2.24
 **/
GVariant *
g_variant_get_child_value (GVariant *value,
                           gsize     index_)
{
  g_return_val_if_fail (index_ < g_variant_n_children (value), NULL);

  if (~g_atomic_int_get (&value->state) & STATE_SERIALISED)
    {
      g_variant_lock (value);

      if (~value->state & STATE_SERIALISED)
        {
          GVariant *child;

          child = g_variant_ref (value->contents.tree.children[index_]);
          g_variant_unlock (value);

          return child;
        }

      g_variant_unlock (value);
    }

  {
    GVariantSerialised serialised = {
      value->type_info,
      (gpointer) value->contents.serialised.data,
      value->size
    };
    GVariantSerialised s_child;
    GVariant *child;

    /* get the serialiser to extract the serialised data for the child
     * from the serialised data for the container
     */
    s_child = g_variant_serialised_get_child (serialised, index_);

    /* create a new serialised instance out of it */
    child = g_slice_new (GVariant);
    child->type_info = s_child.type_info;
    child->state = (value->state & STATE_TRUSTED) |
                   STATE_SERIALISED;
    child->size = s_child.size;
    child->ref_count = 1;
    child->contents.serialised.bytes =
      g_bytes_ref (value->contents.serialised.bytes);
    child->contents.serialised.data = s_child.data;

    return child;
  }
}

/**
 * g_variant_store:
 * @value: the #GVariant to store
 * @data: (not nullable): the location to store the serialised data at
 *
 * Stores the serialised form of @value at @data.  @data should be
 * large enough.  See g_variant_get_size().
 *
 * The stored data is in machine native byte order but may not be in
 * fully-normalised form if read from an untrusted source.  See
 * g_variant_get_normal_form() for a solution.
 *
 * As with g_variant_get_data(), to be able to deserialise the
 * serialised variant successfully, its type and (if the destination
 * machine might be different) its endianness must also be available.
 *
 * This function is approximately O(n) in the size of @data.
 *
 * Since: 2.24
 **/
void
g_variant_store (GVariant *value,
                 gpointer  data)
{
  g_variant_lock (value);

  if (value->state & STATE_SERIALISED)
    {
      if (value->contents.serialised.data != NULL)
        memcpy (data, value->contents.serialised.data, value->size);
      else
        memset (data, 0, value->size);
    }
  else
    g_variant_serialise (value, data);

  g_variant_unlock (value);
}

/**
 * g_variant_is_normal_form:
 * @value: a #GVariant instance
 *
 * Checks if @value is in normal form.
 *
 * The main reason to do this is to detect if a given chunk of
 * serialised data is in normal form: load the data into a #GVariant
 * using g_variant_new_from_data() and then use this function to
 * check.
 *
 * If @value is found to be in normal form then it will be marked as
 * being trusted.  If the value was already marked as being trusted then
 * this function will immediately return %TRUE.
 *
 * Returns: %TRUE if @value is in normal form
 *
 * Since: 2.24
 **/
gboolean
g_variant_is_normal_form (GVariant *value)
{
  if (value->state & STATE_TRUSTED)
    return TRUE;

  g_variant_lock (value);

  if (value->state & STATE_SERIALISED)
    {
      GVariantSerialised serialised = {
        value->type_info,
        (gpointer) value->contents.serialised.data,
        value->size
      };

      if (g_variant_serialised_is_normal (serialised))
        value->state |= STATE_TRUSTED;
    }
  else
    {
      gboolean normal = TRUE;
      gsize i;

      for (i = 0; i < value->contents.tree.n_children; i++)
        normal &= g_variant_is_normal_form (value->contents.tree.children[i]);

      if (normal)
        value->state |= STATE_TRUSTED;
    }

  g_variant_unlock (value);

  return (value->state & STATE_TRUSTED) != 0;
}
