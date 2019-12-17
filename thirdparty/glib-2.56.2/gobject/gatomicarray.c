/* GObject - GLib Type, Object, Parameter and Signal Library
 * Copyright (C) 2009 Benjamin Otte <otte@gnome.org>
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
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <string.h>

#include "gatomicarray.h"

/* A GAtomicArray is a growable, mutable array of data
 * generally of the form of a header of a specific size and
 * then a array of items of a fixed size.
 *
 * It is possible to do lock-less read transactions from the
 * array without any protection against other reads or writes,
 * but such read operation must be aware that the data in the
 * atomic array can change at any time during the transaction,
 * and only at the end can we verify if the transaction succeeded
 * or not. Thus the reading transaction cannot for instance
 * dereference a pointer in the array inside the transaction.
 *
 * The size of an array however cannot change during a read
 * transaction.
 *
 * Writes to the array is done in a copy-update style, but there
 * is no real protection against multiple writers overwriting each
 * others updates, so writes must be protected by an external lock.
 */

G_LOCK_DEFINE_STATIC (array);

typedef struct _FreeListNode FreeListNode;
struct _FreeListNode {
  FreeListNode *next;
};

/* This is really a list of array memory blocks, using the
 * first item as the next pointer to chain them together.
 * Protected by array lock */
static FreeListNode *freelist = NULL;

/* must hold array lock */
static gpointer
freelist_alloc (gsize size, gboolean reuse)
{
  gpointer mem;
  FreeListNode *free, **prev;
  gsize real_size;

  if (reuse)
    {
      for (free = freelist, prev = &freelist; free != NULL; prev = &free->next, free = free->next)
	{
	  if (G_ATOMIC_ARRAY_DATA_SIZE (free) == size)
	    {
	      *prev = free->next;
	      return (gpointer)free;
	    }
	}
    }

  real_size = sizeof (gsize) + MAX (size, sizeof (FreeListNode));
  mem = g_slice_alloc (real_size);
  mem = ((char *) mem) + sizeof (gsize);
  G_ATOMIC_ARRAY_DATA_SIZE (mem) = size;
  return mem;
}

/* must hold array lock */
static void
freelist_free (gpointer mem)
{
  FreeListNode *free;

  free = mem;
  free->next = freelist;
  freelist = free;
}

void
_g_atomic_array_init (GAtomicArray *array)
{
  array->data = NULL;
}

/* Get a copy of the data (if non-NULL) that
 * can be changed and then re-applied with
 * g_atomic_array_update().
 *
 * If additional_element_size is > 0 then
 * then the new memory chunk is that much
 * larger, or there were no data we return
 * a chunk of header_size + additional_element_size.
 * This means you can use this to grow the
 * array part and it handles the first element
 * being added automatically.
 *
 * We don't support shrinking arrays, as if
 * we then re-grow we may reuse an old pointer
 * value and confuse the transaction check.
 */
gpointer
_g_atomic_array_copy (GAtomicArray *array,
		      gsize header_size,
		      gsize additional_element_size)
{
  guint8 *new, *old;
  gsize old_size, new_size;

  G_LOCK (array);
  old = g_atomic_pointer_get (&array->data);
  if (old)
    {
      old_size = G_ATOMIC_ARRAY_DATA_SIZE (old);
      new_size = old_size + additional_element_size;
      /* Don't reuse if copying to same size, as this may end
	 up reusing the same pointer for the same array thus
	 confusing the transaction check */
      new = freelist_alloc (new_size, additional_element_size != 0);
      memcpy (new, old, old_size);
    }
  else if (additional_element_size != 0)
    {
      new_size = header_size + additional_element_size;
      new = freelist_alloc (new_size, TRUE);
    }
  else
    new = NULL;
  G_UNLOCK (array);
  return new;
}

/* Replace the data in the array with the new data,
 * freeing the old data (for reuse). The new data may
 * not be smaller than the current data.
 */
void
_g_atomic_array_update (GAtomicArray *array,
			gpointer new_data)
{
  guint8 *old;

  G_LOCK (array);
  old = g_atomic_pointer_get (&array->data);

  g_assert (old == NULL || G_ATOMIC_ARRAY_DATA_SIZE (old) <= G_ATOMIC_ARRAY_DATA_SIZE (new_data));

  g_atomic_pointer_set (&array->data, new_data);
  if (old)
    freelist_free (old);
  G_UNLOCK (array);
}
