/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
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

/*
 * Modified by the GLib Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GLib Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GLib at ftp://ftp.gtk.org/pub/gtk/.
 */

/*
 * MT safe
 */

#include "config.h"

#include "glist.h"
#include "gslice.h"
#include "gmessages.h"

#include "gtestutils.h"

/**
 * SECTION:linked_lists_double
 * @title: Doubly-Linked Lists
 * @short_description: linked lists that can be iterated over in both directions
 *
 * The #GList structure and its associated functions provide a standard
 * doubly-linked list data structure.
 *
 * Each element in the list contains a piece of data, together with
 * pointers which link to the previous and next elements in the list.
 * Using these pointers it is possible to move through the list in both
 * directions (unlike the singly-linked [GSList][glib-Singly-Linked-Lists],
 * which only allows movement through the list in the forward direction).
 *
 * The double linked list does not keep track of the number of items 
 * and does not keep track of both the start and end of the list. If
 * you want fast access to both the start and the end of the list, 
 * and/or the number of items in the list, use a
 * [GQueue][glib-Double-ended-Queues] instead.
 *
 * The data contained in each element can be either integer values, by
 * using one of the [Type Conversion Macros][glib-Type-Conversion-Macros],
 * or simply pointers to any type of data.
 *
 * List elements are allocated from the [slice allocator][glib-Memory-Slices],
 * which is more efficient than allocating elements individually.
 *
 * Note that most of the #GList functions expect to be passed a pointer
 * to the first element in the list. The functions which insert
 * elements return the new start of the list, which may have changed.
 *
 * There is no function to create a #GList. %NULL is considered to be
 * a valid, empty list so you simply set a #GList* to %NULL to initialize
 * it.
 *
 * To add elements, use g_list_append(), g_list_prepend(),
 * g_list_insert() and g_list_insert_sorted().
 *
 * To visit all elements in the list, use a loop over the list:
 * |[<!-- language="C" -->
 * GList *l;
 * for (l = list; l != NULL; l = l->next)
 *   {
 *     // do something with l->data
 *   }
 * ]|
 *
 * To call a function for each element in the list, use g_list_foreach().
 *
 * To loop over the list and modify it (e.g. remove a certain element)
 * a while loop is more appropriate, for example:
 * |[<!-- language="C" -->
 * GList *l = list;
 * while (l != NULL)
 *   {
 *     GList *next = l->next;
 *     if (should_be_removed (l))
 *       {
 *         // possibly free l->data
 *         list = g_list_delete_link (list, l);
 *       }
 *     l = next;
 *   }
 * ]|
 *
 * To remove elements, use g_list_remove().
 *
 * To navigate in a list, use g_list_first(), g_list_last(),
 * g_list_next(), g_list_previous().
 *
 * To find elements in the list use g_list_nth(), g_list_nth_data(),
 * g_list_find() and g_list_find_custom().
 *
 * To find the index of an element use g_list_position() and
 * g_list_index().
 *
 * To free the entire list, use g_list_free() or g_list_free_full().
 */

/**
 * GList:
 * @data: holds the element's data, which can be a pointer to any kind
 *        of data, or any integer value using the 
 *        [Type Conversion Macros][glib-Type-Conversion-Macros]
 * @next: contains the link to the next element in the list
 * @prev: contains the link to the previous element in the list
 *
 * The #GList struct is used for each element in a doubly-linked list.
 **/

/**
 * g_list_previous:
 * @list: an element in a #GList
 *
 * A convenience macro to get the previous element in a #GList.
 * Note that it is considered perfectly acceptable to access
 * @list->prev directly.
 *
 * Returns: the previous element, or %NULL if there are no previous
 *          elements
 **/

/**
 * g_list_next:
 * @list: an element in a #GList
 *
 * A convenience macro to get the next element in a #GList.
 * Note that it is considered perfectly acceptable to access
 * @list->next directly.
 *
 * Returns: the next element, or %NULL if there are no more elements
 **/

#define _g_list_alloc()         g_slice_new (GList)
#define _g_list_alloc0()        g_slice_new0 (GList)
#define _g_list_free1(list)     g_slice_free (GList, list)

/**
 * g_list_alloc:
 *
 * Allocates space for one #GList element. It is called by
 * g_list_append(), g_list_prepend(), g_list_insert() and
 * g_list_insert_sorted() and so is rarely used on its own.
 *
 * Returns: a pointer to the newly-allocated #GList element
 **/
GList *
g_list_alloc (void)
{
  return _g_list_alloc0 ();
}

/**
 * g_list_free: 
 * @list: a #GList
 *
 * Frees all of the memory used by a #GList.
 * The freed elements are returned to the slice allocator.
 *
 * If list elements contain dynamically-allocated memory, you should
 * either use g_list_free_full() or free them manually first.
 */
void
g_list_free (GList *list)
{
  g_slice_free_chain (GList, list, next);
}

/**
 * g_list_free_1:
 * @list: a #GList element
 *
 * Frees one #GList element, but does not update links from the next and
 * previous elements in the list, so you should not call this function on an
 * element that is currently part of a list.
 *
 * It is usually used after g_list_remove_link().
 */
/**
 * g_list_free1:
 *
 * Another name for g_list_free_1().
 **/
void
g_list_free_1 (GList *list)
{
  _g_list_free1 (list);
}

/**
 * g_list_free_full:
 * @list: a pointer to a #GList
 * @free_func: the function to be called to free each element's data
 *
 * Convenience method, which frees all the memory used by a #GList,
 * and calls @free_func on every element's data.
 *
 * @free_func must not modify the list (eg, by removing the freed
 * element from it).
 *
 * Since: 2.28
 */
void
g_list_free_full (GList          *list,
                  GDestroyNotify  free_func)
{
  g_list_foreach (list, (GFunc) free_func, NULL);
  g_list_free (list);
}

/**
 * g_list_append:
 * @list: a pointer to a #GList
 * @data: the data for the new element
 *
 * Adds a new element on to the end of the list.
 *
 * Note that the return value is the new start of the list,
 * if @list was empty; make sure you store the new value.
 *
 * g_list_append() has to traverse the entire list to find the end,
 * which is inefficient when adding multiple elements. A common idiom
 * to avoid the inefficiency is to use g_list_prepend() and reverse
 * the list with g_list_reverse() when all elements have been added.
 *
 * |[<!-- language="C" -->
 * // Notice that these are initialized to the empty list.
 * GList *string_list = NULL, *number_list = NULL;
 *
 * // This is a list of strings.
 * string_list = g_list_append (string_list, "first");
 * string_list = g_list_append (string_list, "second");
 * 
 * // This is a list of integers.
 * number_list = g_list_append (number_list, GINT_TO_POINTER (27));
 * number_list = g_list_append (number_list, GINT_TO_POINTER (14));
 * ]|
 *
 * Returns: either @list or the new start of the #GList if @list was %NULL
 */
GList *
g_list_append (GList    *list,
               gpointer  data)
{
  GList *new_list;
  GList *last;
  
  new_list = _g_list_alloc ();
  new_list->data = data;
  new_list->next = NULL;
  
  if (list)
    {
      last = g_list_last (list);
      /* g_assert (last != NULL); */
      last->next = new_list;
      new_list->prev = last;

      return list;
    }
  else
    {
      new_list->prev = NULL;
      return new_list;
    }
}

/**
 * g_list_prepend:
 * @list: a pointer to a #GList, this must point to the top of the list
 * @data: the data for the new element
 *
 * Prepends a new element on to the start of the list.
 *
 * Note that the return value is the new start of the list,
 * which will have changed, so make sure you store the new value. 
 *
 * |[<!-- language="C" -->
 * // Notice that it is initialized to the empty list.
 * GList *list = NULL;
 *
 * list = g_list_prepend (list, "last");
 * list = g_list_prepend (list, "first");
 * ]|
 *
 * Do not use this function to prepend a new element to a different
 * element than the start of the list. Use g_list_insert_before() instead.
 *
 * Returns: a pointer to the newly prepended element, which is the new 
 *     start of the #GList
 */
GList *
g_list_prepend (GList    *list,
                gpointer  data)
{
  GList *new_list;
  
  new_list = _g_list_alloc ();
  new_list->data = data;
  new_list->next = list;
  
  if (list)
    {
      new_list->prev = list->prev;
      if (list->prev)
        list->prev->next = new_list;
      list->prev = new_list;
    }
  else
    new_list->prev = NULL;
  
  return new_list;
}

/**
 * g_list_insert:
 * @list: a pointer to a #GList, this must point to the top of the list
 * @data: the data for the new element
 * @position: the position to insert the element. If this is 
 *     negative, or is larger than the number of elements in the 
 *     list, the new element is added on to the end of the list.
 * 
 * Inserts a new element into the list at the given position.
 *
 * Returns: the (possibly changed) start of the #GList
 */
GList *
g_list_insert (GList    *list,
               gpointer  data,
               gint      position)
{
  GList *new_list;
  GList *tmp_list;

  if (position < 0)
    return g_list_append (list, data);
  else if (position == 0)
    return g_list_prepend (list, data);

  tmp_list = g_list_nth (list, position);
  if (!tmp_list)
    return g_list_append (list, data);

  new_list = _g_list_alloc ();
  new_list->data = data;
  new_list->prev = tmp_list->prev;
  tmp_list->prev->next = new_list;
  new_list->next = tmp_list;
  tmp_list->prev = new_list;

  return list;
}

/**
 * g_list_insert_before:
 * @list: a pointer to a #GList, this must point to the top of the list
 * @sibling: the list element before which the new element 
 *     is inserted or %NULL to insert at the end of the list
 * @data: the data for the new element
 *
 * Inserts a new element into the list before the given position.
 *
 * Returns: the (possibly changed) start of the #GList
 */
GList *
g_list_insert_before (GList    *list,
                      GList    *sibling,
                      gpointer  data)
{
  if (!list)
    {
      list = g_list_alloc ();
      list->data = data;
      g_return_val_if_fail (sibling == NULL, list);
      return list;
    }
  else if (sibling)
    {
      GList *node;

      node = _g_list_alloc ();
      node->data = data;
      node->prev = sibling->prev;
      node->next = sibling;
      sibling->prev = node;
      if (node->prev)
        {
          node->prev->next = node;
          return list;
        }
      else
        {
          g_return_val_if_fail (sibling == list, node);
          return node;
        }
    }
  else
    {
      GList *last;

      last = list;
      while (last->next)
        last = last->next;

      last->next = _g_list_alloc ();
      last->next->data = data;
      last->next->prev = last;
      last->next->next = NULL;

      return list;
    }
}

/**
 * g_list_concat:
 * @list1: a #GList, this must point to the top of the list
 * @list2: the #GList to add to the end of the first #GList,
 *     this must point  to the top of the list
 *
 * Adds the second #GList onto the end of the first #GList.
 * Note that the elements of the second #GList are not copied.
 * They are used directly.
 *
 * This function is for example used to move an element in the list.
 * The following example moves an element to the top of the list:
 * |[<!-- language="C" -->
 * list = g_list_remove_link (list, llink);
 * list = g_list_concat (llink, list);
 * ]|
 *
 * Returns: the start of the new #GList, which equals @list1 if not %NULL 
 */
GList *
g_list_concat (GList *list1,
               GList *list2)
{
  GList *tmp_list;
  
  if (list2)
    {
      tmp_list = g_list_last (list1);
      if (tmp_list)
        tmp_list->next = list2;
      else
        list1 = list2;
      list2->prev = tmp_list;
    }
  
  return list1;
}

static inline GList *
_g_list_remove_link (GList *list,
                     GList *link)
{
  if (link == NULL)
    return list;

  if (link->prev)
    {
      if (link->prev->next == link)
        link->prev->next = link->next;
      else
        g_warning ("corrupted double-linked list detected");
    }
  if (link->next)
    {
      if (link->next->prev == link)
        link->next->prev = link->prev;
      else
        g_warning ("corrupted double-linked list detected");
    }

  if (link == list)
    list = list->next;

  link->next = NULL;
  link->prev = NULL;

  return list;
}

/**
 * g_list_remove:
 * @list: a #GList, this must point to the top of the list
 * @data: the data of the element to remove
 *
 * Removes an element from a #GList.
 * If two elements contain the same data, only the first is removed.
 * If none of the elements contain the data, the #GList is unchanged.
 *
 * Returns: the (possibly changed) start of the #GList
 */
GList *
g_list_remove (GList         *list,
               gconstpointer  data)
{
  GList *tmp;

  tmp = list;
  while (tmp)
    {
      if (tmp->data != data)
        tmp = tmp->next;
      else
        {
          list = _g_list_remove_link (list, tmp);
          _g_list_free1 (tmp);

          break;
        }
    }
  return list;
}

/**
 * g_list_remove_all:
 * @list: a #GList, this must point to the top of the list
 * @data: data to remove
 *
 * Removes all list nodes with data equal to @data.
 * Returns the new head of the list. Contrast with
 * g_list_remove() which removes only the first node
 * matching the given data.
 *
 * Returns: the (possibly changed) start of the #GList
 */
GList *
g_list_remove_all (GList         *list,
                   gconstpointer  data)
{
  GList *tmp = list;

  while (tmp)
    {
      if (tmp->data != data)
        tmp = tmp->next;
      else
        {
          GList *next = tmp->next;

          if (tmp->prev)
            tmp->prev->next = next;
          else
            list = next;
          if (next)
            next->prev = tmp->prev;

          _g_list_free1 (tmp);
          tmp = next;
        }
    }
  return list;
}

/**
 * g_list_remove_link:
 * @list: a #GList, this must point to the top of the list
 * @llink: an element in the #GList
 *
 * Removes an element from a #GList, without freeing the element.
 * The removed element's prev and next links are set to %NULL, so 
 * that it becomes a self-contained list with one element.
 *
 * This function is for example used to move an element in the list
 * (see the example for g_list_concat()) or to remove an element in
 * the list before freeing its data:
 * |[<!-- language="C" --> 
 * list = g_list_remove_link (list, llink);
 * free_some_data_that_may_access_the_list_again (llink->data);
 * g_list_free (llink);
 * ]|
 *
 * Returns: the (possibly changed) start of the #GList
 */
GList *
g_list_remove_link (GList *list,
                    GList *llink)
{
  return _g_list_remove_link (list, llink);
}

/**
 * g_list_delete_link:
 * @list: a #GList, this must point to the top of the list
 * @link_: node to delete from @list
 *
 * Removes the node link_ from the list and frees it. 
 * Compare this to g_list_remove_link() which removes the node 
 * without freeing it.
 *
 * Returns: the (possibly changed) start of the #GList
 */
GList *
g_list_delete_link (GList *list,
                    GList *link_)
{
  list = _g_list_remove_link (list, link_);
  _g_list_free1 (link_);

  return list;
}

/**
 * g_list_copy:
 * @list: a #GList, this must point to the top of the list
 *
 * Copies a #GList.
 *
 * Note that this is a "shallow" copy. If the list elements 
 * consist of pointers to data, the pointers are copied but 
 * the actual data is not. See g_list_copy_deep() if you need
 * to copy the data as well.
 *
 * Returns: the start of the new list that holds the same data as @list
 */
GList *
g_list_copy (GList *list)
{
  return g_list_copy_deep (list, NULL, NULL);
}

/**
 * g_list_copy_deep:
 * @list: a #GList, this must point to the top of the list
 * @func: a copy function used to copy every element in the list
 * @user_data: user data passed to the copy function @func, or %NULL
 *
 * Makes a full (deep) copy of a #GList.
 *
 * In contrast with g_list_copy(), this function uses @func to make
 * a copy of each list element, in addition to copying the list
 * container itself.
 *
 * @func, as a #GCopyFunc, takes two arguments, the data to be copied
 * and a @user_data pointer. It's safe to pass %NULL as user_data,
 * if the copy function takes only one argument.
 *
 * For instance, if @list holds a list of GObjects, you can do:
 * |[<!-- language="C" -->   
 * another_list = g_list_copy_deep (list, (GCopyFunc) g_object_ref, NULL);
 * ]|
 *
 * And, to entirely free the new list, you could do:
 * |[<!-- language="C" --> 
 * g_list_free_full (another_list, g_object_unref);
 * ]|
 *
 * Returns: the start of the new list that holds a full copy of @list, 
 *     use g_list_free_full() to free it
 *
 * Since: 2.34
 */
GList *
g_list_copy_deep (GList     *list,
                  GCopyFunc  func,
                  gpointer   user_data)
{
  GList *new_list = NULL;

  if (list)
    {
      GList *last;

      new_list = _g_list_alloc ();
      if (func)
        new_list->data = func (list->data, user_data);
      else
        new_list->data = list->data;
      new_list->prev = NULL;
      last = new_list;
      list = list->next;
      while (list)
        {
          last->next = _g_list_alloc ();
          last->next->prev = last;
          last = last->next;
          if (func)
            last->data = func (list->data, user_data);
          else
            last->data = list->data;
          list = list->next;
        }
      last->next = NULL;
    }

  return new_list;
}

/**
 * g_list_reverse:
 * @list: a #GList, this must point to the top of the list
 *
 * Reverses a #GList.
 * It simply switches the next and prev pointers of each element.
 *
 * Returns: the start of the reversed #GList
 */
GList *
g_list_reverse (GList *list)
{
  GList *last;
  
  last = NULL;
  while (list)
    {
      last = list;
      list = last->next;
      last->next = last->prev;
      last->prev = list;
    }
  
  return last;
}

/**
 * g_list_nth:
 * @list: a #GList, this must point to the top of the list
 * @n: the position of the element, counting from 0
 *
 * Gets the element at the given position in a #GList.
 *
 * This iterates over the list until it reaches the @n-th position. If you
 * intend to iterate over every element, it is better to use a for-loop as
 * described in the #GList introduction.
 *
 * Returns: the element, or %NULL if the position is off 
 *     the end of the #GList
 */
GList *
g_list_nth (GList *list,
            guint  n)
{
  while ((n-- > 0) && list)
    list = list->next;
  
  return list;
}

/**
 * g_list_nth_prev:
 * @list: a #GList
 * @n: the position of the element, counting from 0
 *
 * Gets the element @n places before @list.
 *
 * Returns: the element, or %NULL if the position is 
 *     off the end of the #GList
 */
GList *
g_list_nth_prev (GList *list,
                 guint  n)
{
  while ((n-- > 0) && list)
    list = list->prev;
  
  return list;
}

/**
 * g_list_nth_data:
 * @list: a #GList, this must point to the top of the list
 * @n: the position of the element
 *
 * Gets the data of the element at the given position.
 *
 * This iterates over the list until it reaches the @n-th position. If you
 * intend to iterate over every element, it is better to use a for-loop as
 * described in the #GList introduction.
 *
 * Returns: the element's data, or %NULL if the position 
 *     is off the end of the #GList
 */
gpointer
g_list_nth_data (GList *list,
                 guint  n)
{
  while ((n-- > 0) && list)
    list = list->next;
  
  return list ? list->data : NULL;
}

/**
 * g_list_find:
 * @list: a #GList, this must point to the top of the list
 * @data: the element data to find
 *
 * Finds the element in a #GList which contains the given data.
 *
 * Returns: the found #GList element, or %NULL if it is not found
 */
GList *
g_list_find (GList         *list,
             gconstpointer  data)
{
  while (list)
    {
      if (list->data == data)
        break;
      list = list->next;
    }
  
  return list;
}

/**
 * g_list_find_custom:
 * @list: a #GList, this must point to the top of the list
 * @data: user data passed to the function
 * @func: the function to call for each element. 
 *     It should return 0 when the desired element is found
 *
 * Finds an element in a #GList, using a supplied function to 
 * find the desired element. It iterates over the list, calling 
 * the given function which should return 0 when the desired 
 * element is found. The function takes two #gconstpointer arguments, 
 * the #GList element's data as the first argument and the 
 * given user data.
 *
 * Returns: the found #GList element, or %NULL if it is not found
 */
GList *
g_list_find_custom (GList         *list,
                    gconstpointer  data,
                    GCompareFunc   func)
{
  g_return_val_if_fail (func != NULL, list);

  while (list)
    {
      if (! func (list->data, data))
        return list;
      list = list->next;
    }

  return NULL;
}

/**
 * g_list_position:
 * @list: a #GList, this must point to the top of the list
 * @llink: an element in the #GList
 *
 * Gets the position of the given element 
 * in the #GList (starting from 0).
 *
 * Returns: the position of the element in the #GList, 
 *     or -1 if the element is not found
 */
gint
g_list_position (GList *list,
                 GList *llink)
{
  gint i;

  i = 0;
  while (list)
    {
      if (list == llink)
        return i;
      i++;
      list = list->next;
    }

  return -1;
}

/**
 * g_list_index:
 * @list: a #GList, this must point to the top of the list
 * @data: the data to find
 *
 * Gets the position of the element containing 
 * the given data (starting from 0).
 *
 * Returns: the index of the element containing the data, 
 *     or -1 if the data is not found
 */
gint
g_list_index (GList         *list,
              gconstpointer  data)
{
  gint i;

  i = 0;
  while (list)
    {
      if (list->data == data)
        return i;
      i++;
      list = list->next;
    }

  return -1;
}

/**
 * g_list_last:
 * @list: any #GList element
 *
 * Gets the last element in a #GList.
 *
 * Returns: the last element in the #GList,
 *     or %NULL if the #GList has no elements
 */
GList *
g_list_last (GList *list)
{
  if (list)
    {
      while (list->next)
        list = list->next;
    }
  
  return list;
}

/**
 * g_list_first:
 * @list: any #GList element
 *
 * Gets the first element in a #GList.
 *
 * Returns: the first element in the #GList, 
 *     or %NULL if the #GList has no elements
 */
GList *
g_list_first (GList *list)
{
  if (list)
    {
      while (list->prev)
        list = list->prev;
    }
  
  return list;
}

/**
 * g_list_length:
 * @list: a #GList, this must point to the top of the list
 *
 * Gets the number of elements in a #GList.
 *
 * This function iterates over the whole list to count its elements.
 * Use a #GQueue instead of a GList if you regularly need the number
 * of items. To check whether the list is non-empty, it is faster to check
 * @list against %NULL.
 *
 * Returns: the number of elements in the #GList
 */
guint
g_list_length (GList *list)
{
  guint length;
  
  length = 0;
  while (list)
    {
      length++;
      list = list->next;
    }
  
  return length;
}

/**
 * g_list_foreach:
 * @list: a #GList, this must point to the top of the list
 * @func: the function to call with each element's data
 * @user_data: user data to pass to the function
 *
 * Calls a function for each element of a #GList.
 *
 * It is safe for @func to remove the element from @list, but it must
 * not modify any part of the list after that element.
 */
/**
 * GFunc:
 * @data: the element's data
 * @user_data: user data passed to g_list_foreach() or g_slist_foreach()
 *
 * Specifies the type of functions passed to g_list_foreach() and
 * g_slist_foreach().
 */
void
g_list_foreach (GList    *list,
                GFunc     func,
                gpointer  user_data)
{
  while (list)
    {
      GList *next = list->next;
      (*func) (list->data, user_data);
      list = next;
    }
}

static GList*
g_list_insert_sorted_real (GList    *list,
                           gpointer  data,
                           GFunc     func,
                           gpointer  user_data)
{
  GList *tmp_list = list;
  GList *new_list;
  gint cmp;

  g_return_val_if_fail (func != NULL, list);
  
  if (!list) 
    {
      new_list = _g_list_alloc0 ();
      new_list->data = data;
      return new_list;
    }
  
  cmp = ((GCompareDataFunc) func) (data, tmp_list->data, user_data);

  while ((tmp_list->next) && (cmp > 0))
    {
      tmp_list = tmp_list->next;

      cmp = ((GCompareDataFunc) func) (data, tmp_list->data, user_data);
    }

  new_list = _g_list_alloc0 ();
  new_list->data = data;

  if ((!tmp_list->next) && (cmp > 0))
    {
      tmp_list->next = new_list;
      new_list->prev = tmp_list;
      return list;
    }
   
  if (tmp_list->prev)
    {
      tmp_list->prev->next = new_list;
      new_list->prev = tmp_list->prev;
    }
  new_list->next = tmp_list;
  tmp_list->prev = new_list;
 
  if (tmp_list == list)
    return new_list;
  else
    return list;
}

/**
 * g_list_insert_sorted:
 * @list: a pointer to a #GList, this must point to the top of the
 *     already sorted list
 * @data: the data for the new element
 * @func: the function to compare elements in the list. It should 
 *     return a number > 0 if the first parameter comes after the 
 *     second parameter in the sort order.
 *
 * Inserts a new element into the list, using the given comparison 
 * function to determine its position.
 *
 * If you are adding many new elements to a list, and the number of
 * new elements is much larger than the length of the list, use
 * g_list_prepend() to add the new items and sort the list afterwards
 * with g_list_sort().
 *
 * Returns: the (possibly changed) start of the #GList
 */
GList *
g_list_insert_sorted (GList        *list,
                      gpointer      data,
                      GCompareFunc  func)
{
  return g_list_insert_sorted_real (list, data, (GFunc) func, NULL);
}

/**
 * g_list_insert_sorted_with_data:
 * @list: a pointer to a #GList, this must point to the top of the
 *     already sorted list
 * @data: the data for the new element
 * @func: the function to compare elements in the list. It should
 *     return a number > 0 if the first parameter  comes after the
 *     second parameter in the sort order.
 * @user_data: user data to pass to comparison function
 *
 * Inserts a new element into the list, using the given comparison 
 * function to determine its position.
 *
 * If you are adding many new elements to a list, and the number of
 * new elements is much larger than the length of the list, use
 * g_list_prepend() to add the new items and sort the list afterwards
 * with g_list_sort().
 *
 * Returns: the (possibly changed) start of the #GList
 *
 * Since: 2.10
 */
GList *
g_list_insert_sorted_with_data (GList            *list,
                                gpointer          data,
                                GCompareDataFunc  func,
                                gpointer          user_data)
{
  return g_list_insert_sorted_real (list, data, (GFunc) func, user_data);
}

static GList *
g_list_sort_merge (GList     *l1, 
                   GList     *l2,
                   GFunc     compare_func,
                   gpointer  user_data)
{
  GList list, *l, *lprev;
  gint cmp;

  l = &list; 
  lprev = NULL;

  while (l1 && l2)
    {
      cmp = ((GCompareDataFunc) compare_func) (l1->data, l2->data, user_data);

      if (cmp <= 0)
        {
          l->next = l1;
          l1 = l1->next;
        } 
      else 
        {
          l->next = l2;
          l2 = l2->next;
        }
      l = l->next;
      l->prev = lprev; 
      lprev = l;
    }
  l->next = l1 ? l1 : l2;
  l->next->prev = l;

  return list.next;
}

static GList * 
g_list_sort_real (GList    *list,
                  GFunc     compare_func,
                  gpointer  user_data)
{
  GList *l1, *l2;
  
  if (!list) 
    return NULL;
  if (!list->next) 
    return list;
  
  l1 = list; 
  l2 = list->next;

  while ((l2 = l2->next) != NULL)
    {
      if ((l2 = l2->next) == NULL) 
        break;
      l1 = l1->next;
    }
  l2 = l1->next; 
  l1->next = NULL; 

  return g_list_sort_merge (g_list_sort_real (list, compare_func, user_data),
                            g_list_sort_real (l2, compare_func, user_data),
                            compare_func,
                            user_data);
}

/**
 * g_list_sort:
 * @list: a #GList, this must point to the top of the list
 * @compare_func: the comparison function used to sort the #GList.
 *     This function is passed the data from 2 elements of the #GList 
 *     and should return 0 if they are equal, a negative value if the 
 *     first element comes before the second, or a positive value if 
 *     the first element comes after the second.
 *
 * Sorts a #GList using the given comparison function. The algorithm 
 * used is a stable sort.
 *
 * Returns: the (possibly changed) start of the #GList
 */
/**
 * GCompareFunc:
 * @a: a value
 * @b: a value to compare with
 *
 * Specifies the type of a comparison function used to compare two
 * values.  The function should return a negative integer if the first
 * value comes before the second, 0 if they are equal, or a positive
 * integer if the first value comes after the second.
 *
 * Returns: negative value if @a < @b; zero if @a = @b; positive
 *          value if @a > @b
 */
GList *
g_list_sort (GList        *list,
             GCompareFunc  compare_func)
{
  return g_list_sort_real (list, (GFunc) compare_func, NULL);
}

/**
 * g_list_sort_with_data:
 * @list: a #GList, this must point to the top of the list
 * @compare_func: comparison function
 * @user_data: user data to pass to comparison function
 *
 * Like g_list_sort(), but the comparison function accepts 
 * a user data argument.
 *
 * Returns: the (possibly changed) start of the #GList
 */
/**
 * GCompareDataFunc:
 * @a: a value
 * @b: a value to compare with
 * @user_data: user data
 *
 * Specifies the type of a comparison function used to compare two
 * values.  The function should return a negative integer if the first
 * value comes before the second, 0 if they are equal, or a positive
 * integer if the first value comes after the second.
 *
 * Returns: negative value if @a < @b; zero if @a = @b; positive
 *          value if @a > @b
 */
GList *
g_list_sort_with_data (GList            *list,
                       GCompareDataFunc  compare_func,
                       gpointer          user_data)
{
  return g_list_sort_real (list, (GFunc) compare_func, user_data);
}
