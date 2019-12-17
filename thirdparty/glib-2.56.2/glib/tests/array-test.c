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

#undef G_DISABLE_ASSERT
#undef G_LOG_DOMAIN

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "glib.h"

static void
sum_up (gpointer data,
	gpointer user_data)
{
  gint *sum = (gint *)user_data;

  *sum += GPOINTER_TO_INT (data);
}

/* Check that expanding an array with g_array_set_size() clears the new elements
 * if @clear_ was specified during construction. */
static void
array_new_cleared (void)
{
  GArray *garray;
  gsize i;

  garray = g_array_new (FALSE, TRUE, sizeof (gint));
  g_assert_cmpuint (garray->len, ==, 0);

  g_array_set_size (garray, 5);
  g_assert_cmpuint (garray->len, ==, 5);

  for (i = 0; i < 5; i++)
    g_assert_cmpint (g_array_index (garray, gint, i), ==, 0);

  g_array_unref (garray);
}

/* As with array_new_cleared(), but with a sized array. */
static void
array_new_sized_cleared (void)
{
  GArray *garray;
  gsize i;

  garray = g_array_sized_new (FALSE, TRUE, sizeof (gint), 10);
  g_assert_cmpuint (garray->len, ==, 0);

  g_array_set_size (garray, 5);
  g_assert_cmpuint (garray->len, ==, 5);

  for (i = 0; i < 5; i++)
    g_assert_cmpint (g_array_index (garray, gint, i), ==, 0);

  g_array_unref (garray);
}

/* Check that a zero-terminated array does actually have a zero terminator. */
static void
array_new_zero_terminated (void)
{
  GArray *garray;
  gchar *out_str = NULL;

  garray = g_array_new (TRUE, FALSE, sizeof (gchar));
  g_assert_cmpuint (garray->len, ==, 0);

  g_array_append_vals (garray, "hello", strlen ("hello"));
  g_assert_cmpuint (garray->len, ==, 5);
  g_assert_cmpstr (garray->data, ==, "hello");

  out_str = g_array_free (garray, FALSE);
  g_assert_cmpstr (out_str, ==, "hello");
  g_free (out_str);
}

static void
array_append (void)
{
  GArray *garray;
  gint i;
  gint *segment;

  garray = g_array_new (FALSE, FALSE, sizeof (gint));
  for (i = 0; i < 10000; i++)
    g_array_append_val (garray, i);

  for (i = 0; i < 10000; i++)
    g_assert_cmpint (g_array_index (garray, gint, i), ==, i);

  segment = (gint*)g_array_free (garray, FALSE);
  for (i = 0; i < 10000; i++)
    g_assert_cmpint (segment[i], ==, i);
  g_free (segment);
}

static void
array_prepend (void)
{
  GArray *garray;
  gint i;

  garray = g_array_new (FALSE, FALSE, sizeof (gint));
  for (i = 0; i < 100; i++)
    g_array_prepend_val (garray, i);

  for (i = 0; i < 100; i++)
    g_assert_cmpint (g_array_index (garray, gint, i), ==, (100 - i - 1));

  g_array_free (garray, TRUE);
}

static void
array_remove (void)
{
  GArray *garray;
  gint i;
  gint prev, cur;

  garray = g_array_new (FALSE, FALSE, sizeof (gint));
  for (i = 0; i < 100; i++)
    g_array_append_val (garray, i);

  g_assert_cmpint (garray->len, ==, 100);

  g_array_remove_index (garray, 1);
  g_array_remove_index (garray, 3);
  g_array_remove_index (garray, 21);
  g_array_remove_index (garray, 57);

  g_assert_cmpint (garray->len, ==, 96);

  prev = -1;
  for (i = 0; i < garray->len; i++)
    {
      cur = g_array_index (garray, gint, i);
      g_assert (cur != 1 &&  cur != 4 && cur != 23 && cur != 60);
      g_assert_cmpint (prev, <, cur);
      prev = cur;
    }

  g_array_free (garray, TRUE);
}

static void
array_remove_fast (void)
{
  GArray *garray;
  gint i;
  gint prev, cur;

  garray = g_array_new (FALSE, FALSE, sizeof (gint));
  for (i = 0; i < 100; i++)
    g_array_append_val (garray, i);

  g_assert_cmpint (garray->len, ==, 100);

  g_array_remove_index_fast (garray, 1);
  g_array_remove_index_fast (garray, 3);
  g_array_remove_index_fast (garray, 21);
  g_array_remove_index_fast (garray, 57);

  g_assert_cmpint (garray->len, ==, 96);

  prev = -1;
  for (i = 0; i < garray->len; i++)
    {
      cur = g_array_index (garray, gint, i);
      g_assert (cur != 1 &&  cur != 3 && cur != 21 && cur != 57);
      if (cur < 96)
        {
          g_assert_cmpint (prev, <, cur);
          prev = cur;
        }
    }

  g_array_free (garray, TRUE);
}

static void
array_remove_range (void)
{
  GArray *garray;
  gint i;
  gint prev, cur;

  garray = g_array_new (FALSE, FALSE, sizeof (gint));
  for (i = 0; i < 100; i++)
    g_array_append_val (garray, i);

  g_assert_cmpint (garray->len, ==, 100);

  g_array_remove_range (garray, 31, 4);

  g_assert_cmpint (garray->len, ==, 96);

  prev = -1;
  for (i = 0; i < garray->len; i++)
    {
      cur = g_array_index (garray, gint, i);
      g_assert (cur < 31 || cur > 34);
      g_assert_cmpint (prev, <, cur);
      prev = cur;
    }

  /* Ensure the entire array can be cleared, even when empty. */
  g_array_remove_range (garray, 0, garray->len);
  g_array_remove_range (garray, 0, garray->len);

  g_array_free (garray, TRUE);
}

static void
array_ref_count (void)
{
  GArray *garray;
  GArray *garray2;
  gint i;

  garray = g_array_new (FALSE, FALSE, sizeof (gint));
  g_assert_cmpint (g_array_get_element_size (garray), ==, sizeof (gint));
  for (i = 0; i < 100; i++)
    g_array_prepend_val (garray, i);

  /* check we can ref, unref and still access the array */
  garray2 = g_array_ref (garray);
  g_assert (garray == garray2);
  g_array_unref (garray2);
  for (i = 0; i < 100; i++)
    g_assert_cmpint (g_array_index (garray, gint, i), ==, (100 - i - 1));

  /* garray2 should be an empty valid GArray wrapper */
  garray2 = g_array_ref (garray);
  g_array_free (garray, TRUE);

  g_assert_cmpint (garray2->len, ==, 0);
  g_array_unref (garray2);
}

static int
int_compare (gconstpointer p1, gconstpointer p2)
{
  const gint *i1 = p1;
  const gint *i2 = p2;

  return *i1 - *i2;
}

static int
int_compare_data (gconstpointer p1, gconstpointer p2, gpointer data)
{
  const gint *i1 = p1;
  const gint *i2 = p2;

  return *i1 - *i2;
}
static void
array_sort (void)
{
  GArray *garray;
  gint i;
  gint prev, cur;

  garray = g_array_new (FALSE, FALSE, sizeof (gint));
  for (i = 0; i < 10000; i++)
    {
      cur = g_random_int_range (0, 10000);
      g_array_append_val (garray, cur);
    }
  g_array_sort (garray,  int_compare);

  prev = -1;
  for (i = 0; i < garray->len; i++)
    {
      cur = g_array_index (garray, gint, i);
      g_assert_cmpint (prev, <=, cur);
      prev = cur;
    }

  g_array_free (garray, TRUE);
}

static void
array_sort_with_data (void)
{
  GArray *garray;
  gint i;
  gint prev, cur;

  garray = g_array_new (FALSE, FALSE, sizeof (gint));
  for (i = 0; i < 10000; i++)
    {
      cur = g_random_int_range (0, 10000);
      g_array_append_val (garray, cur);
    }
  g_array_sort_with_data (garray, int_compare_data, NULL);

  prev = -1;
  for (i = 0; i < garray->len; i++)
    {
      cur = g_array_index (garray, gint, i);
      g_assert_cmpint (prev, <=, cur);
      prev = cur;
    }

  g_array_free (garray, TRUE);
}

static gint num_clear_func_invocations = 0;

static void
my_clear_func (gpointer data)
{
  num_clear_func_invocations += 1;
}

static void
array_clear_func (void)
{
  GArray *garray;
  gint i;
  gint cur;

  garray = g_array_new (FALSE, FALSE, sizeof (gint));
  g_array_set_clear_func (garray, my_clear_func);

  for (i = 0; i < 10; i++)
    {
      cur = g_random_int_range (0, 100);
      g_array_append_val (garray, cur);
    }

  g_array_remove_index (garray, 9);
  g_assert_cmpint (num_clear_func_invocations, ==, 1);

  g_array_remove_range (garray, 5, 3);
  g_assert_cmpint (num_clear_func_invocations, ==, 4);

  g_array_remove_index_fast (garray, 4);
  g_assert_cmpint (num_clear_func_invocations, ==, 5);

  g_array_free (garray, TRUE);
  g_assert_cmpint (num_clear_func_invocations, ==, 10);
}

static void
pointer_array_add (void)
{
  GPtrArray *gparray;
  gint i;
  gint sum = 0;
  gpointer *segment;

  gparray = g_ptr_array_sized_new (1000);

  for (i = 0; i < 10000; i++)
    g_ptr_array_add (gparray, GINT_TO_POINTER (i));

  for (i = 0; i < 10000; i++)
    g_assert (g_ptr_array_index (gparray, i) == GINT_TO_POINTER (i));
  
  g_ptr_array_foreach (gparray, sum_up, &sum);
  g_assert (sum == 49995000);

  segment = g_ptr_array_free (gparray, FALSE);
  for (i = 0; i < 10000; i++)
    g_assert (segment[i] == GINT_TO_POINTER (i));
  g_free (segment);
}

static void
pointer_array_insert (void)
{
  GPtrArray *gparray;
  gint i;
  gint sum = 0;
  gint index;

  gparray = g_ptr_array_sized_new (1000);

  for (i = 0; i < 10000; i++)
    {
      index = g_random_int_range (-1, i + 1);
      g_ptr_array_insert (gparray, index, GINT_TO_POINTER (i));
    }

  g_ptr_array_foreach (gparray, sum_up, &sum);
  g_assert (sum == 49995000);

  g_ptr_array_free (gparray, TRUE);
}

static void
pointer_array_ref_count (void)
{
  GPtrArray *gparray;
  GPtrArray *gparray2;
  gint i;
  gint sum = 0;

  gparray = g_ptr_array_new ();
  for (i = 0; i < 10000; i++)
    g_ptr_array_add (gparray, GINT_TO_POINTER (i));

  /* check we can ref, unref and still access the array */
  gparray2 = g_ptr_array_ref (gparray);
  g_assert (gparray == gparray2);
  g_ptr_array_unref (gparray2);
  for (i = 0; i < 10000; i++)
    g_assert (g_ptr_array_index (gparray, i) == GINT_TO_POINTER (i));

  g_ptr_array_foreach (gparray, sum_up, &sum);
  g_assert (sum == 49995000);

  /* gparray2 should be an empty valid GPtrArray wrapper */
  gparray2 = g_ptr_array_ref (gparray);
  g_ptr_array_free (gparray, TRUE);

  g_assert_cmpint (gparray2->len, ==, 0);
  g_ptr_array_unref (gparray2);
}

static gint num_free_func_invocations = 0;

static void
my_free_func (gpointer data)
{
  num_free_func_invocations++;
  g_free (data);
}

static void
pointer_array_free_func (void)
{
  GPtrArray *gparray;
  GPtrArray *gparray2;
  gchar **strv;
  gchar *s;

  num_free_func_invocations = 0;
  gparray = g_ptr_array_new_with_free_func (my_free_func);
  g_ptr_array_unref (gparray);
  g_assert_cmpint (num_free_func_invocations, ==, 0);

  gparray = g_ptr_array_new_with_free_func (my_free_func);
  g_ptr_array_free (gparray, TRUE);
  g_assert_cmpint (num_free_func_invocations, ==, 0);

  num_free_func_invocations = 0;
  gparray = g_ptr_array_new_with_free_func (my_free_func);
  g_ptr_array_add (gparray, g_strdup ("foo"));
  g_ptr_array_add (gparray, g_strdup ("bar"));
  g_ptr_array_add (gparray, g_strdup ("baz"));
  g_ptr_array_remove_index (gparray, 0);
  g_assert_cmpint (num_free_func_invocations, ==, 1);
  g_ptr_array_remove_index_fast (gparray, 1);
  g_assert_cmpint (num_free_func_invocations, ==, 2);
  s = g_strdup ("frob");
  g_ptr_array_add (gparray, s);
  g_assert (g_ptr_array_remove (gparray, s));
  g_assert (!g_ptr_array_remove (gparray, "nuun"));
  g_assert (!g_ptr_array_remove_fast (gparray, "mlo"));
  g_assert_cmpint (num_free_func_invocations, ==, 3);
  s = g_strdup ("frob");
  g_ptr_array_add (gparray, s);
  g_ptr_array_set_size (gparray, 1);
  g_assert_cmpint (num_free_func_invocations, ==, 4);
  g_ptr_array_ref (gparray);
  g_ptr_array_unref (gparray);
  g_assert_cmpint (num_free_func_invocations, ==, 4);
  g_ptr_array_unref (gparray);
  g_assert_cmpint (num_free_func_invocations, ==, 5);

  num_free_func_invocations = 0;
  gparray = g_ptr_array_new_full (10, my_free_func);
  g_ptr_array_add (gparray, g_strdup ("foo"));
  g_ptr_array_add (gparray, g_strdup ("bar"));
  g_ptr_array_add (gparray, g_strdup ("baz"));
  g_ptr_array_set_size (gparray, 20);
  g_ptr_array_add (gparray, NULL);
  gparray2 = g_ptr_array_ref (gparray);
  strv = (gchar **) g_ptr_array_free (gparray, FALSE);
  g_assert_cmpint (num_free_func_invocations, ==, 0);
  g_strfreev (strv);
  g_ptr_array_unref (gparray2);
  g_assert_cmpint (num_free_func_invocations, ==, 0);

  num_free_func_invocations = 0;
  gparray = g_ptr_array_new_with_free_func (my_free_func);
  g_ptr_array_add (gparray, g_strdup ("foo"));
  g_ptr_array_add (gparray, g_strdup ("bar"));
  g_ptr_array_add (gparray, g_strdup ("baz"));
  g_ptr_array_remove_range (gparray, 1, 1);
  g_ptr_array_unref (gparray);
  g_assert_cmpint (num_free_func_invocations, ==, 3);

  num_free_func_invocations = 0;
  gparray = g_ptr_array_new_with_free_func (my_free_func);
  g_ptr_array_add (gparray, g_strdup ("foo"));
  g_ptr_array_add (gparray, g_strdup ("bar"));
  g_ptr_array_add (gparray, g_strdup ("baz"));
  g_ptr_array_free (gparray, TRUE);
  g_assert_cmpint (num_free_func_invocations, ==, 3);

  num_free_func_invocations = 0;
  gparray = g_ptr_array_new_with_free_func (my_free_func);
  g_ptr_array_add (gparray, "foo");
  g_ptr_array_add (gparray, "bar");
  g_ptr_array_add (gparray, "baz");
  g_ptr_array_set_free_func (gparray, NULL);
  g_ptr_array_free (gparray, TRUE);
  g_assert_cmpint (num_free_func_invocations, ==, 0);
}

static gint
ptr_compare (gconstpointer p1, gconstpointer p2)
{
  gpointer i1 = *(gpointer*)p1;
  gpointer i2 = *(gpointer*)p2;

  return GPOINTER_TO_INT (i1) - GPOINTER_TO_INT (i2);
}

static gint
ptr_compare_data (gconstpointer p1, gconstpointer p2, gpointer data)
{
  gpointer i1 = *(gpointer*)p1;
  gpointer i2 = *(gpointer*)p2;

  return GPOINTER_TO_INT (i1) - GPOINTER_TO_INT (i2);
}

static void
pointer_array_sort (void)
{
  GPtrArray *gparray;
  gint i;
  gint val;
  gint prev, cur;

  gparray = g_ptr_array_new ();
  for (i = 0; i < 10000; i++)
    {
      val = g_random_int_range (0, 10000);
      g_ptr_array_add (gparray, GINT_TO_POINTER (val));
    }

  g_ptr_array_sort (gparray, ptr_compare);

  prev = -1;
  for (i = 0; i < 10000; i++)
    {
      cur = GPOINTER_TO_INT (g_ptr_array_index (gparray, i));
      g_assert_cmpint (prev, <=, cur);
      prev = cur;
    }

  g_ptr_array_free (gparray, TRUE);
}

static void
pointer_array_sort_with_data (void)
{
  GPtrArray *gparray;
  gint i;
  gint prev, cur;

  gparray = g_ptr_array_new ();
  for (i = 0; i < 10000; i++)
    g_ptr_array_add (gparray, GINT_TO_POINTER (g_random_int_range (0, 10000)));

  g_ptr_array_sort_with_data (gparray, ptr_compare_data, NULL);

  prev = -1;
  for (i = 0; i < 10000; i++)
    {
      cur = GPOINTER_TO_INT (g_ptr_array_index (gparray, i));
      g_assert_cmpint (prev, <=, cur);
      prev = cur;
    }

  g_ptr_array_free (gparray, TRUE);
}

static void
pointer_array_find_empty (void)
{
  GPtrArray *array;
  guint idx;

  array = g_ptr_array_new ();

  g_assert_false (g_ptr_array_find (array, "some-value", NULL));  /* NULL index */
  g_assert_false (g_ptr_array_find (array, "some-value", &idx));  /* non-NULL index */
  g_assert_false (g_ptr_array_find_with_equal_func (array, "some-value", g_str_equal, NULL));  /* NULL index */
  g_assert_false (g_ptr_array_find_with_equal_func (array, "some-value", g_str_equal, &idx));  /* non-NULL index */

  g_ptr_array_free (array, TRUE);
}

static void
pointer_array_find_non_empty (void)
{
  GPtrArray *array;
  guint idx;
  const gchar *str_pointer = "static-string";

  array = g_ptr_array_new ();

  g_ptr_array_add (array, "some");
  g_ptr_array_add (array, "random");
  g_ptr_array_add (array, "values");
  g_ptr_array_add (array, "some");
  g_ptr_array_add (array, "duplicated");
  g_ptr_array_add (array, (gpointer) str_pointer);

  g_assert_true (g_ptr_array_find_with_equal_func (array, "random", g_str_equal, NULL));  /* NULL index */
  g_assert_true (g_ptr_array_find_with_equal_func (array, "random", g_str_equal, &idx));  /* non-NULL index */
  g_assert_cmpuint (idx, ==, 1);

  g_assert_true (g_ptr_array_find_with_equal_func (array, "some", g_str_equal, &idx));  /* duplicate element */
  g_assert_cmpuint (idx, ==, 0);

  g_assert_false (g_ptr_array_find_with_equal_func (array, "nope", g_str_equal, NULL));

  g_assert_true (g_ptr_array_find_with_equal_func (array, str_pointer, g_str_equal, &idx));
  g_assert_cmpuint (idx, ==, 5);
  idx = G_MAXUINT;
  g_assert_true (g_ptr_array_find_with_equal_func (array, str_pointer, NULL, &idx));  /* NULL equal func */
  g_assert_cmpuint (idx, ==, 5);
  idx = G_MAXUINT;
  g_assert_true (g_ptr_array_find (array, str_pointer, &idx));  /* NULL equal func */
  g_assert_cmpuint (idx, ==, 5);

  g_ptr_array_free (array, TRUE);
}

static void
byte_array_append (void)
{
  GByteArray *gbarray;
  gint i;
  guint8 *segment;

  gbarray = g_byte_array_sized_new (1000);
  for (i = 0; i < 10000; i++)
    g_byte_array_append (gbarray, (guint8*) "abcd", 4);

  for (i = 0; i < 10000; i++)
    {
      g_assert (gbarray->data[4*i] == 'a');
      g_assert (gbarray->data[4*i+1] == 'b');
      g_assert (gbarray->data[4*i+2] == 'c');
      g_assert (gbarray->data[4*i+3] == 'd');
    }

  segment = g_byte_array_free (gbarray, FALSE);

  for (i = 0; i < 10000; i++)
    {
      g_assert (segment[4*i] == 'a');
      g_assert (segment[4*i+1] == 'b');
      g_assert (segment[4*i+2] == 'c');
      g_assert (segment[4*i+3] == 'd');
    }

  g_free (segment);
}

static void
byte_array_prepend (void)
{
  GByteArray *gbarray;
  gint i;

  gbarray = g_byte_array_new ();
  g_byte_array_set_size (gbarray, 1000);

  for (i = 0; i < 10000; i++)
    g_byte_array_prepend (gbarray, (guint8*) "abcd", 4);

  for (i = 0; i < 10000; i++)
    {
      g_assert (gbarray->data[4*i] == 'a');
      g_assert (gbarray->data[4*i+1] == 'b');
      g_assert (gbarray->data[4*i+2] == 'c');
      g_assert (gbarray->data[4*i+3] == 'd');
    }

  g_byte_array_free (gbarray, TRUE);
}

static void
byte_array_ref_count (void)
{
  GByteArray *gbarray;
  GByteArray *gbarray2;
  gint i;

  gbarray = g_byte_array_new ();
  for (i = 0; i < 10000; i++)
    g_byte_array_append (gbarray, (guint8*) "abcd", 4);

  gbarray2 = g_byte_array_ref (gbarray);
  g_assert (gbarray2 == gbarray);
  g_byte_array_unref (gbarray2);
  for (i = 0; i < 10000; i++)
    {
      g_assert (gbarray->data[4*i] == 'a');
      g_assert (gbarray->data[4*i+1] == 'b');
      g_assert (gbarray->data[4*i+2] == 'c');
      g_assert (gbarray->data[4*i+3] == 'd');
    }

  gbarray2 = g_byte_array_ref (gbarray);
  g_assert (gbarray2 == gbarray);
  g_byte_array_free (gbarray, TRUE);
  g_assert_cmpint (gbarray2->len, ==, 0);
  g_byte_array_unref (gbarray2);
}

static void
byte_array_remove (void)
{
  GByteArray *gbarray;
  gint i;

  gbarray = g_byte_array_new ();
  for (i = 0; i < 100; i++)
    g_byte_array_append (gbarray, (guint8*) "abcd", 4);

  g_assert_cmpint (gbarray->len, ==, 400);

  g_byte_array_remove_index (gbarray, 4);
  g_byte_array_remove_index (gbarray, 4);
  g_byte_array_remove_index (gbarray, 4);
  g_byte_array_remove_index (gbarray, 4);

  g_assert_cmpint (gbarray->len, ==, 396);

  for (i = 0; i < 99; i++)
    {
      g_assert (gbarray->data[4*i] == 'a');
      g_assert (gbarray->data[4*i+1] == 'b');
      g_assert (gbarray->data[4*i+2] == 'c');
      g_assert (gbarray->data[4*i+3] == 'd');
    }

  g_byte_array_free (gbarray, TRUE);
}

static void
byte_array_remove_fast (void)
{
  GByteArray *gbarray;
  gint i;

  gbarray = g_byte_array_new ();
  for (i = 0; i < 100; i++)
    g_byte_array_append (gbarray, (guint8*) "abcd", 4);

  g_assert_cmpint (gbarray->len, ==, 400);

  g_byte_array_remove_index_fast (gbarray, 4);
  g_byte_array_remove_index_fast (gbarray, 4);
  g_byte_array_remove_index_fast (gbarray, 4);
  g_byte_array_remove_index_fast (gbarray, 4);

  g_assert_cmpint (gbarray->len, ==, 396);

  for (i = 0; i < 99; i++)
    {
      g_assert (gbarray->data[4*i] == 'a');
      g_assert (gbarray->data[4*i+1] == 'b');
      g_assert (gbarray->data[4*i+2] == 'c');
      g_assert (gbarray->data[4*i+3] == 'd');
    }

  g_byte_array_free (gbarray, TRUE);
}

static void
byte_array_remove_range (void)
{
  GByteArray *gbarray;
  gint i;

  gbarray = g_byte_array_new ();
  for (i = 0; i < 100; i++)
    g_byte_array_append (gbarray, (guint8*) "abcd", 4);

  g_assert_cmpint (gbarray->len, ==, 400);

  g_byte_array_remove_range (gbarray, 12, 4);

  g_assert_cmpint (gbarray->len, ==, 396);

  for (i = 0; i < 99; i++)
    {
      g_assert (gbarray->data[4*i] == 'a');
      g_assert (gbarray->data[4*i+1] == 'b');
      g_assert (gbarray->data[4*i+2] == 'c');
      g_assert (gbarray->data[4*i+3] == 'd');
    }

  /* Ensure the entire array can be cleared, even when empty. */
  g_byte_array_remove_range (gbarray, 0, gbarray->len);
  g_byte_array_remove_range (gbarray, 0, gbarray->len);

  g_byte_array_free (gbarray, TRUE);
}

static int
byte_compare (gconstpointer p1, gconstpointer p2)
{
  const guint8 *i1 = p1;
  const guint8 *i2 = p2;

  return *i1 - *i2;
}

static int
byte_compare_data (gconstpointer p1, gconstpointer p2, gpointer data)
{
  const guint8 *i1 = p1;
  const guint8 *i2 = p2;

  return *i1 - *i2;
}

static void
byte_array_sort (void)
{
  GByteArray *gbarray;
  gint i;
  guint8 val;
  guint8 prev, cur;

  gbarray = g_byte_array_new ();
  for (i = 0; i < 100; i++)
    {
      val = 'a' + g_random_int_range (0, 26);
      g_byte_array_append (gbarray, (guint8*) &val, 1);
    }

  g_byte_array_sort (gbarray, byte_compare);

  prev = 'a';
  for (i = 0; i < gbarray->len; i++)
    {
      cur = gbarray->data[i];
      g_assert_cmpint (prev, <=, cur);
      prev = cur;
    }

  g_byte_array_free (gbarray, TRUE);
}

static void
byte_array_sort_with_data (void)
{
  GByteArray *gbarray;
  gint i;
  guint8 val;
  guint8 prev, cur;

  gbarray = g_byte_array_new ();
  for (i = 0; i < 100; i++)
    {
      val = 'a' + g_random_int_range (0, 26);
      g_byte_array_append (gbarray, (guint8*) &val, 1);
    }

  g_byte_array_sort_with_data (gbarray, byte_compare_data, NULL);

  prev = 'a';
  for (i = 0; i < gbarray->len; i++)
    {
      cur = gbarray->data[i];
      g_assert_cmpint (prev, <=, cur);
      prev = cur;
    }

  g_byte_array_free (gbarray, TRUE);
}

static void
byte_array_new_take (void)
{
  GByteArray *gbarray;
  guint8 *data;

  data = g_memdup ("woooweeewow", 11);
  gbarray = g_byte_array_new_take (data, 11);
  g_assert (gbarray->data == data);
  g_assert_cmpuint (gbarray->len, ==, 11);
  g_byte_array_free (gbarray, TRUE);
}

static void
byte_array_free_to_bytes (void)
{
  GByteArray *gbarray;
  gpointer memory;
  GBytes *bytes;
  gsize size;

  gbarray = g_byte_array_new ();
  g_byte_array_append (gbarray, (guint8 *)"woooweeewow", 11);
  memory = gbarray->data;

  bytes = g_byte_array_free_to_bytes (gbarray);
  g_assert (bytes != NULL);
  g_assert_cmpuint (g_bytes_get_size (bytes), ==, 11);
  g_assert (g_bytes_get_data (bytes, &size) == memory);
  g_assert_cmpuint (size, ==, 11);

  g_bytes_unref (bytes);
}
int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_bug_base ("https://bugzilla.gnome.org/");

  /* array tests */
  g_test_add_func ("/array/new/cleared", array_new_cleared);
  g_test_add_func ("/array/new/sized-cleared", array_new_sized_cleared);
  g_test_add_func ("/array/new/zero-terminated", array_new_zero_terminated);
  g_test_add_func ("/array/append", array_append);
  g_test_add_func ("/array/prepend", array_prepend);
  g_test_add_func ("/array/remove", array_remove);
  g_test_add_func ("/array/remove-fast", array_remove_fast);
  g_test_add_func ("/array/remove-range", array_remove_range);
  g_test_add_func ("/array/ref-count", array_ref_count);
  g_test_add_func ("/array/sort", array_sort);
  g_test_add_func ("/array/sort-with-data", array_sort_with_data);
  g_test_add_func ("/array/clear-func", array_clear_func);

  /* pointer arrays */
  g_test_add_func ("/pointerarray/add", pointer_array_add);
  g_test_add_func ("/pointerarray/insert", pointer_array_insert);
  g_test_add_func ("/pointerarray/ref-count", pointer_array_ref_count);
  g_test_add_func ("/pointerarray/free-func", pointer_array_free_func);
  g_test_add_func ("/pointerarray/sort", pointer_array_sort);
  g_test_add_func ("/pointerarray/sort-with-data", pointer_array_sort_with_data);
  g_test_add_func ("/pointerarray/find/empty", pointer_array_find_empty);
  g_test_add_func ("/pointerarray/find/non-empty", pointer_array_find_non_empty);

  /* byte arrays */
  g_test_add_func ("/bytearray/append", byte_array_append);
  g_test_add_func ("/bytearray/prepend", byte_array_prepend);
  g_test_add_func ("/bytearray/remove", byte_array_remove);
  g_test_add_func ("/bytearray/remove-fast", byte_array_remove_fast);
  g_test_add_func ("/bytearray/remove-range", byte_array_remove_range);
  g_test_add_func ("/bytearray/ref-count", byte_array_ref_count);
  g_test_add_func ("/bytearray/sort", byte_array_sort);
  g_test_add_func ("/bytearray/sort-with-data", byte_array_sort_with_data);
  g_test_add_func ("/bytearray/new-take", byte_array_new_take);
  g_test_add_func ("/bytearray/free-to-bytes", byte_array_free_to_bytes);

  return g_test_run ();
}

