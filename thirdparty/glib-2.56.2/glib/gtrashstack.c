/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1998  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GLib Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GLib Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GLib at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "config.h"

#include "gtrashstack.h"

/**
 * SECTION:trash_stack
 * @title: Trash Stacks
 * @short_description: maintain a stack of unused allocated memory chunks
 *
 * A #GTrashStack is an efficient way to keep a stack of unused allocated
 * memory chunks. Each memory chunk is required to be large enough to hold
 * a #gpointer. This allows the stack to be maintained without any space
 * overhead, since the stack pointers can be stored inside the memory chunks.
 *
 * There is no function to create a #GTrashStack. A %NULL #GTrashStack*
 * is a perfectly valid empty stack.
 *
 * There is no longer any good reason to use #GTrashStack.  If you have
 * extra pieces of memory, free() them and allocate them again later.
 *
 * Deprecated: 2.48: #GTrashStack is deprecated without replacement
 */

/**
 * GTrashStack:
 * @next: pointer to the previous element of the stack,
 *     gets stored in the first `sizeof (gpointer)`
 *     bytes of the element
 *
 * Each piece of memory that is pushed onto the stack
 * is cast to a GTrashStack*.
 *
 * Deprecated: 2.48: #GTrashStack is deprecated without replacement
 */

/**
 * g_trash_stack_push:
 * @stack_p: a #GTrashStack
 * @data_p: (not nullable): the piece of memory to push on the stack
 *
 * Pushes a piece of memory onto a #GTrashStack.
 * Deprecated: 2.48: #GTrashStack is deprecated without replacement
 */
void
g_trash_stack_push (GTrashStack **stack_p,
                    gpointer      data_p)
{
  GTrashStack *data = (GTrashStack *) data_p;

  data->next = *stack_p;
  *stack_p = data;
}

/**
 * g_trash_stack_pop:
 * @stack_p: a #GTrashStack
 *
 * Pops a piece of memory off a #GTrashStack.
 *
 * Returns: the element at the top of the stack
 * Deprecated: 2.48: #GTrashStack is deprecated without replacement
 */
gpointer
g_trash_stack_pop (GTrashStack **stack_p)
{
  GTrashStack *data;

  data = *stack_p;
  if (data)
    {
      *stack_p = data->next;
      /* NULLify private pointer here, most platforms store NULL as
       * subsequent 0 bytes
       */
      data->next = NULL;
    }

  return data;
}

/**
 * g_trash_stack_peek:
 * @stack_p: a #GTrashStack
 *
 * Returns the element at the top of a #GTrashStack
 * which may be %NULL.
 *
 * Returns: the element at the top of the stack
 * Deprecated: 2.48: #GTrashStack is deprecated without replacement
 */
gpointer
g_trash_stack_peek (GTrashStack **stack_p)
{
  GTrashStack *data;

  data = *stack_p;

  return data;
}

/**
 * g_trash_stack_height:
 * @stack_p: a #GTrashStack
 *
 * Returns the height of a #GTrashStack.
 *
 * Note that execution of this function is of O(N) complexity
 * where N denotes the number of items on the stack.
 *
 * Returns: the height of the stack
 * Deprecated: 2.48: #GTrashStack is deprecated without replacement
 */
guint
g_trash_stack_height (GTrashStack **stack_p)
{
  GTrashStack *data;
  guint i = 0;

  for (data = *stack_p; data; data = data->next)
    i++;

  return i;
}
