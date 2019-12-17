/* GObject - GLib Type, Object, Parameter and Signal Library
 * Copyright (C) 2001 Red Hat, Inc.
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
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

/*
 * MT safe
 */

#include "config.h"

#include <string.h>
#include <stdlib.h>  /* qsort() */

#include "gvaluearray.h"


/**
 * SECTION:value_arrays
 * @short_description: A container structure to maintain an array of
 *     generic values
 * @see_also: #GValue, #GParamSpecValueArray, g_param_spec_value_array()
 * @title: Value arrays
 *
 * The prime purpose of a #GValueArray is for it to be used as an
 * object property that holds an array of values. A #GValueArray wraps
 * an array of #GValue elements in order for it to be used as a boxed
 * type through %G_TYPE_VALUE_ARRAY.
 *
 * #GValueArray is deprecated in favour of #GArray since GLib 2.32. It
 * is possible to create a #GArray that behaves like a #GValueArray by
 * using the size of #GValue as the element size, and by setting
 * g_value_unset() as the clear function using g_array_set_clear_func(),
 * for instance, the following code:
 *
 * |[<!-- language="C" --> 
 *   GValueArray *array = g_value_array_new (10);
 * ]|
 *
 * can be replaced by:
 *
 * |[<!-- language="C" --> 
 *   GArray *array = g_array_sized_new (FALSE, TRUE, sizeof (GValue), 10);
 *   g_array_set_clear_func (array, (GDestroyNotify) g_value_unset);
 * ]|
 */


#ifdef	DISABLE_MEM_POOLS
#  define	GROUP_N_VALUES	(1)	/* power of 2 !! */
#else
#  define	GROUP_N_VALUES	(8)	/* power of 2 !! */
#endif


/* --- functions --- */
/**
 * g_value_array_get_nth:
 * @value_array: #GValueArray to get a value from
 * @index_: index of the value of interest
 *
 * Return a pointer to the value at @index_ containd in @value_array.
 *
 * Returns: (transfer none): pointer to a value at @index_ in @value_array
 *
 * Deprecated: 2.32: Use g_array_index() instead.
 */
GValue*
g_value_array_get_nth (GValueArray *value_array,
		       guint        index)
{
  g_return_val_if_fail (value_array != NULL, NULL);
  g_return_val_if_fail (index < value_array->n_values, NULL);

  return value_array->values + index;
}

static inline void
value_array_grow (GValueArray *value_array,
		  guint        n_values,
		  gboolean     zero_init)
{
  g_return_if_fail (n_values >= value_array->n_values);

  value_array->n_values = n_values;
  if (value_array->n_values > value_array->n_prealloced)
    {
      guint i = value_array->n_prealloced;

      value_array->n_prealloced = (value_array->n_values + GROUP_N_VALUES - 1) & ~(GROUP_N_VALUES - 1);
      value_array->values = g_renew (GValue, value_array->values, value_array->n_prealloced);
      if (!zero_init)
	i = value_array->n_values;
      memset (value_array->values + i, 0,
	      (value_array->n_prealloced - i) * sizeof (value_array->values[0]));
    }
}

static inline void
value_array_shrink (GValueArray *value_array)
{
#ifdef  DISABLE_MEM_POOLS
  if (value_array->n_prealloced >= value_array->n_values + GROUP_N_VALUES)
    {
      value_array->n_prealloced = (value_array->n_values + GROUP_N_VALUES - 1) & ~(GROUP_N_VALUES - 1);
      value_array->values = g_renew (GValue, value_array->values, value_array->n_prealloced);
    }
#endif
}

/**
 * g_value_array_new:
 * @n_prealloced: number of values to preallocate space for
 *
 * Allocate and initialize a new #GValueArray, optionally preserve space
 * for @n_prealloced elements. New arrays always contain 0 elements,
 * regardless of the value of @n_prealloced.
 *
 * Returns: a newly allocated #GValueArray with 0 values
 *
 * Deprecated: 2.32: Use #GArray and g_array_sized_new() instead.
 */
GValueArray*
g_value_array_new (guint n_prealloced)
{
  GValueArray *value_array = g_slice_new (GValueArray);

  value_array->n_values = 0;
  value_array->n_prealloced = 0;
  value_array->values = NULL;
  value_array_grow (value_array, n_prealloced, TRUE);
  value_array->n_values = 0;

  return value_array;
}

/**
 * g_value_array_free: (skip)
 * @value_array: #GValueArray to free
 *
 * Free a #GValueArray including its contents.
 *
 * Deprecated: 2.32: Use #GArray and g_array_unref() instead.
 */
void
g_value_array_free (GValueArray *value_array)
{
  guint i;

  g_return_if_fail (value_array != NULL);

  for (i = 0; i < value_array->n_values; i++)
    {
      GValue *value = value_array->values + i;

      if (G_VALUE_TYPE (value) != 0) /* we allow unset values in the array */
	g_value_unset (value);
    }
  g_free (value_array->values);
  g_slice_free (GValueArray, value_array);
}

/**
 * g_value_array_copy:
 * @value_array: #GValueArray to copy
 *
 * Construct an exact copy of a #GValueArray by duplicating all its
 * contents.
 *
 * Returns: (transfer full): Newly allocated copy of #GValueArray
 *
 * Deprecated: 2.32: Use #GArray and g_array_ref() instead.
 */
GValueArray*
g_value_array_copy (const GValueArray *value_array)
{
  GValueArray *new_array;
  guint i;

  g_return_val_if_fail (value_array != NULL, NULL);

  new_array = g_slice_new (GValueArray);
  new_array->n_values = 0;
  new_array->values = NULL;
  new_array->n_prealloced = 0;
  value_array_grow (new_array, value_array->n_values, TRUE);
  for (i = 0; i < new_array->n_values; i++)
    if (G_VALUE_TYPE (value_array->values + i) != 0)
      {
	GValue *value = new_array->values + i;
	
	g_value_init (value, G_VALUE_TYPE (value_array->values + i));
	g_value_copy (value_array->values + i, value);
      }
  return new_array;
}

/**
 * g_value_array_prepend:
 * @value_array: #GValueArray to add an element to
 * @value: (nullable): #GValue to copy into #GValueArray, or %NULL
 *
 * Insert a copy of @value as first element of @value_array. If @value is
 * %NULL, an uninitialized value is prepended.
 *
 *
 * Returns: (transfer none): the #GValueArray passed in as @value_array
 *
 * Deprecated: 2.32: Use #GArray and g_array_prepend_val() instead.
 */
GValueArray*
g_value_array_prepend (GValueArray  *value_array,
		       const GValue *value)
{
  g_return_val_if_fail (value_array != NULL, NULL);

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  return g_value_array_insert (value_array, 0, value);
  G_GNUC_END_IGNORE_DEPRECATIONS
}

/**
 * g_value_array_append:
 * @value_array: #GValueArray to add an element to
 * @value: (nullable): #GValue to copy into #GValueArray, or %NULL
 *
 * Insert a copy of @value as last element of @value_array. If @value is
 * %NULL, an uninitialized value is appended.
 *
 * Returns: (transfer none): the #GValueArray passed in as @value_array
 *
 * Deprecated: 2.32: Use #GArray and g_array_append_val() instead.
 */
GValueArray*
g_value_array_append (GValueArray  *value_array,
		      const GValue *value)
{
  g_return_val_if_fail (value_array != NULL, NULL);

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  return g_value_array_insert (value_array, value_array->n_values, value);
  G_GNUC_END_IGNORE_DEPRECATIONS
}

/**
 * g_value_array_insert:
 * @value_array: #GValueArray to add an element to
 * @index_: insertion position, must be <= value_array->;n_values
 * @value: (nullable): #GValue to copy into #GValueArray, or %NULL
 *
 * Insert a copy of @value at specified position into @value_array. If @value
 * is %NULL, an uninitialized value is inserted.
 *
 * Returns: (transfer none): the #GValueArray passed in as @value_array
 *
 * Deprecated: 2.32: Use #GArray and g_array_insert_val() instead.
 */
GValueArray*
g_value_array_insert (GValueArray  *value_array,
		      guint         index,
		      const GValue *value)
{
  guint i;

  g_return_val_if_fail (value_array != NULL, NULL);
  g_return_val_if_fail (index <= value_array->n_values, value_array);

  i = value_array->n_values;
  value_array_grow (value_array, value_array->n_values + 1, FALSE);
  if (index + 1 < value_array->n_values)
    memmove (value_array->values + index + 1, value_array->values + index,
             (i - index) * sizeof (value_array->values[0]));
  memset (value_array->values + index, 0, sizeof (value_array->values[0]));
  if (value)
    {
      g_value_init (value_array->values + index, G_VALUE_TYPE (value));
      g_value_copy (value, value_array->values + index);
    }
  return value_array;
}

/**
 * g_value_array_remove:
 * @value_array: #GValueArray to remove an element from
 * @index_: position of value to remove, which must be less than
 *     @value_array->n_values
 *
 * Remove the value at position @index_ from @value_array.
 *
 * Returns: (transfer none): the #GValueArray passed in as @value_array
 *
 * Deprecated: 2.32: Use #GArray and g_array_remove_index() instead.
 */
GValueArray*
g_value_array_remove (GValueArray *value_array,
		      guint        index)
{
  g_return_val_if_fail (value_array != NULL, NULL);
  g_return_val_if_fail (index < value_array->n_values, value_array);

  if (G_VALUE_TYPE (value_array->values + index) != 0)
    g_value_unset (value_array->values + index);
  value_array->n_values--;
  if (index < value_array->n_values)
    memmove (value_array->values + index, value_array->values + index + 1,
             (value_array->n_values - index) * sizeof (value_array->values[0]));
  value_array_shrink (value_array);
  if (value_array->n_prealloced > value_array->n_values)
    memset (value_array->values + value_array->n_values, 0, sizeof (value_array->values[0]));

  return value_array;
}

/**
 * g_value_array_sort:
 * @value_array: #GValueArray to sort
 * @compare_func: (scope call): function to compare elements
 *
 * Sort @value_array using @compare_func to compare the elements according to
 * the semantics of #GCompareFunc.
 *
 * The current implementation uses the same sorting algorithm as standard
 * C qsort() function.
 *
 * Returns: (transfer none): the #GValueArray passed in as @value_array
 *
 * Deprecated: 2.32: Use #GArray and g_array_sort().
 */
GValueArray*
g_value_array_sort (GValueArray *value_array,
		    GCompareFunc compare_func)
{
  g_return_val_if_fail (compare_func != NULL, NULL);

  if (value_array->n_values)
    qsort (value_array->values,
	   value_array->n_values,
	   sizeof (value_array->values[0]),
	   compare_func);
  return value_array;
}

/**
 * g_value_array_sort_with_data: (rename-to g_value_array_sort)
 * @value_array: #GValueArray to sort
 * @compare_func: (scope call): function to compare elements
 * @user_data: (closure): extra data argument provided for @compare_func
 *
 * Sort @value_array using @compare_func to compare the elements according
 * to the semantics of #GCompareDataFunc.
 *
 * The current implementation uses the same sorting algorithm as standard
 * C qsort() function.
 *
 * Returns: (transfer none): the #GValueArray passed in as @value_array
 *
 * Deprecated: 2.32: Use #GArray and g_array_sort_with_data().
 */
GValueArray*
g_value_array_sort_with_data (GValueArray     *value_array,
			      GCompareDataFunc compare_func,
			      gpointer         user_data)
{
  g_return_val_if_fail (value_array != NULL, NULL);
  g_return_val_if_fail (compare_func != NULL, NULL);

  if (value_array->n_values)
    g_qsort_with_data (value_array->values,
		       value_array->n_values,
		       sizeof (value_array->values[0]),
		       compare_func, user_data);
  return value_array;
}
