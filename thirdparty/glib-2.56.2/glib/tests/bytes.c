/*
 * Copyright 2011 Collabora Ltd.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * See the included COPYING file for more information.
 */

#undef G_DISABLE_ASSERT
#undef G_LOG_DOMAIN

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "glib.h"

/* Keep in sync with glib/gbytes.c */
struct _GBytes
{
  gconstpointer data;
  gsize size;
  gint ref_count;
  GDestroyNotify free_func;
  gpointer user_data;
};

static const gchar *NYAN = "nyannyan";
static const gsize N_NYAN = 8;

static void
test_new (void)
{
  const gchar *data;
  GBytes *bytes;
  gsize size;

  data = "test";
  bytes = g_bytes_new (data, 4);
  g_assert (bytes != NULL);
  g_assert (g_bytes_get_data (bytes, &size) != data);
  g_assert_cmpuint (size, ==, 4);
  g_assert_cmpuint (g_bytes_get_size (bytes), ==, 4);
  g_assert_cmpmem (data, 4, g_bytes_get_data (bytes, NULL), g_bytes_get_size (bytes));

  g_bytes_unref (bytes);
}

static void
test_new_take (void)
{
  gchar *data;
  GBytes *bytes;
  gsize size;

  data = g_strdup ("test");
  bytes = g_bytes_new_take (data, 4);
  g_assert (bytes != NULL);
  g_assert (g_bytes_get_data (bytes, &size) == data);
  g_assert_cmpuint (size, ==, 4);
  g_assert_cmpuint (g_bytes_get_size (bytes), ==, 4);

  g_bytes_unref (bytes);
}

static void
test_new_static (void)
{
  const gchar *data;
  GBytes *bytes;
  gsize size;

  data = "test";
  bytes = g_bytes_new_static (data, 4);
  g_assert (bytes != NULL);
  g_assert (g_bytes_get_data (bytes, &size) == data);
  g_assert_cmpuint (size, ==, 4);
  g_assert_cmpuint (g_bytes_get_size (bytes), ==, 4);

  g_bytes_unref (bytes);
}

static void
test_new_from_bytes (void)
{
  const gchar *data = "smile and wave";
  GBytes *bytes;
  GBytes *sub;

  bytes = g_bytes_new (data, 14);
  sub = g_bytes_new_from_bytes (bytes, 10, 4);

  g_assert (sub != NULL);
  g_assert (g_bytes_get_data (sub, NULL) == ((gchar *)g_bytes_get_data (bytes, NULL)) + 10);
  g_bytes_unref (bytes);

  g_assert_cmpmem (g_bytes_get_data (sub, NULL), g_bytes_get_size (sub), "wave", 4);
  g_bytes_unref (sub);
}

/* Verify that creating slices of GBytes reference the top-most bytes
 * at the correct offset. Ensure that intermediate GBytes are not referenced.
 */
static void
test_new_from_bytes_slice (void)
{
  GBytes *bytes = g_bytes_new_static ("Some stupid data", strlen ("Some stupid data") + 1);
  GBytes *bytes1 = g_bytes_new_from_bytes (bytes, 4, 13);
  GBytes *bytes2 = g_bytes_new_from_bytes (bytes1, 1, 12);
  GBytes *bytes3 = g_bytes_new_from_bytes (bytes2, 0, 6);

  g_assert_cmpint (bytes->ref_count, ==, 4);
  g_assert_cmpint (bytes1->ref_count, ==, 1);
  g_assert_cmpint (bytes2->ref_count, ==, 1);
  g_assert_cmpint (bytes3->ref_count, ==, 1);

  g_assert_null (bytes->user_data);
  g_assert (bytes1->user_data == bytes);
  g_assert (bytes2->user_data == bytes);
  g_assert (bytes3->user_data == bytes);

  g_assert_cmpint (17, ==, g_bytes_get_size (bytes));
  g_assert_cmpint (13, ==, g_bytes_get_size (bytes1));
  g_assert_cmpint (12, ==, g_bytes_get_size (bytes2));
  g_assert_cmpint (6, ==, g_bytes_get_size (bytes3));

  g_assert_cmpint (0, ==, strncmp ("Some stupid data", (gchar *)bytes->data, 17));
  g_assert_cmpint (0, ==, strncmp (" stupid data", (gchar *)bytes1->data, 13));
  g_assert_cmpint (0, ==, strncmp ("stupid data", (gchar *)bytes2->data, 12));
  g_assert_cmpint (0, ==, strncmp ("stupid", (gchar *)bytes3->data, 6));

  g_bytes_unref (bytes);
  g_bytes_unref (bytes1);
  g_bytes_unref (bytes2);
  g_bytes_unref (bytes3);
}

/* Ensure that referencing an entire GBytes just returns the same bytes
 * instance (with incremented reference count) instead of a new instance.
 */
static void
test_new_from_bytes_shared_ref (void)
{
  GBytes *bytes = g_bytes_new_static ("Some data", strlen ("Some data") + 1);
  GBytes *other = g_bytes_new_from_bytes (bytes, 0, g_bytes_get_size (bytes));

  g_assert (bytes == other);
  g_assert_cmpint (bytes->ref_count, ==, 2);

  g_bytes_unref (bytes);
  g_bytes_unref (other);
}

static void
on_destroy_increment (gpointer data)
{
  gint *count = data;
  g_assert (count != NULL);
  (*count)++;
}

static void
test_new_with_free_func (void)
{
  GBytes *bytes;
  gchar *data;
  gint count = 0;
  gsize size;

  data = "test";
  bytes = g_bytes_new_with_free_func (data, 4, on_destroy_increment, &count);
  g_assert (bytes != NULL);
  g_assert_cmpint (count, ==, 0);
  g_assert (g_bytes_get_data (bytes, &size) == data);
  g_assert_cmpuint (size, ==, 4);
  g_assert_cmpuint (g_bytes_get_size (bytes), ==, 4);

  g_bytes_unref (bytes);
  g_assert_cmpuint (count, ==, 1);
}

static void
test_hash (void)
{
  GBytes *bytes1;
  GBytes *bytes2;
  guint hash1;
  guint hash2;

  bytes1 = g_bytes_new ("blah", 4);
  bytes2 = g_bytes_new ("blah", 4);

  hash1 = g_bytes_hash (bytes1);
  hash2 = g_bytes_hash (bytes2);
  g_assert (hash1 == hash2);

  g_bytes_unref (bytes1);
  g_bytes_unref (bytes2);
}

static void
test_equal (void)
{
  GBytes *bytes;
  GBytes *bytes2;

  bytes = g_bytes_new ("blah", 4);

  bytes2 = g_bytes_new ("blah", 4);
  g_assert (g_bytes_equal (bytes, bytes2));
  g_assert (g_bytes_equal (bytes2, bytes));
  g_bytes_unref (bytes2);

  bytes2 = g_bytes_new ("bla", 3);
  g_assert (!g_bytes_equal (bytes, bytes2));
  g_assert (!g_bytes_equal (bytes2, bytes));
  g_bytes_unref (bytes2);

  bytes2 = g_bytes_new ("true", 4);
  g_assert (!g_bytes_equal (bytes, bytes2));
  g_assert (!g_bytes_equal (bytes2, bytes));
  g_bytes_unref (bytes2);

  g_bytes_unref (bytes);
}

static void
test_compare (void)
{
  GBytes *bytes;
  GBytes *bytes2;

  bytes = g_bytes_new ("blah", 4);

  bytes2 = g_bytes_new ("blah", 4);
  g_assert_cmpint (g_bytes_compare (bytes, bytes2), ==, 0);
  g_bytes_unref (bytes2);

  bytes2 = g_bytes_new ("bla", 3);
  g_assert_cmpint (g_bytes_compare (bytes, bytes2), >, 0);
  g_bytes_unref (bytes2);

  bytes2 = g_bytes_new ("abcd", 4);
  g_assert_cmpint (g_bytes_compare (bytes, bytes2), >, 0);
  g_bytes_unref (bytes2);

  bytes2 = g_bytes_new ("blahblah", 8);
  g_assert_cmpint (g_bytes_compare (bytes, bytes2), <, 0);
  g_bytes_unref (bytes2);

  bytes2 = g_bytes_new ("zyx", 3);
  g_assert_cmpint (g_bytes_compare (bytes, bytes2), <, 0);
  g_bytes_unref (bytes2);

  bytes2 = g_bytes_new ("zyxw", 4);
  g_assert_cmpint (g_bytes_compare (bytes, bytes2), <, 0);
  g_bytes_unref (bytes2);

  g_bytes_unref (bytes);
}

static void
test_to_data_transferred (void)
{
  gconstpointer memory;
  gpointer data;
  gsize size;
  GBytes *bytes;

  /* Memory transferred: one reference, and allocated with g_malloc */
  bytes = g_bytes_new (NYAN, N_NYAN);
  memory = g_bytes_get_data (bytes, NULL);
  data = g_bytes_unref_to_data (bytes, &size);
  g_assert (data == memory);
  g_assert_cmpmem (data, size, NYAN, N_NYAN);
  g_free (data);
}

static void
test_to_data_two_refs (void)
{
  gconstpointer memory;
  gpointer data;
  gsize size;
  GBytes *bytes;

  /* Memory copied: two references */
  bytes = g_bytes_new (NYAN, N_NYAN);
  bytes = g_bytes_ref (bytes);
  memory = g_bytes_get_data (bytes, NULL);
  data = g_bytes_unref_to_data (bytes, &size);
  g_assert (data != memory);
  g_assert_cmpmem (data, size, NYAN, N_NYAN);
  g_free (data);
  g_assert (g_bytes_get_data (bytes, &size) == memory);
  g_assert_cmpuint (size, ==, N_NYAN);
  g_assert_cmpuint (g_bytes_get_size (bytes), ==, N_NYAN);
  g_bytes_unref (bytes);
}

static void
test_to_data_non_malloc (void)
{
  gpointer data;
  gsize size;
  GBytes *bytes;

  /* Memory copied: non malloc memory */
  bytes = g_bytes_new_static (NYAN, N_NYAN);
  g_assert (g_bytes_get_data (bytes, NULL) == NYAN);
  data = g_bytes_unref_to_data (bytes, &size);
  g_assert (data != (gpointer)NYAN);
  g_assert_cmpmem (data, size, NYAN, N_NYAN);
  g_free (data);
}

static void
test_to_array_transferred (void)
{
  gconstpointer memory;
  GByteArray *array;
  GBytes *bytes;

  /* Memory transferred: one reference, and allocated with g_malloc */
  bytes = g_bytes_new (NYAN, N_NYAN);
  memory = g_bytes_get_data (bytes, NULL);
  array = g_bytes_unref_to_array (bytes);
  g_assert (array != NULL);
  g_assert (array->data == memory);
  g_assert_cmpmem (array->data, array->len, NYAN, N_NYAN);
  g_byte_array_unref (array);
}

static void
test_to_array_two_refs (void)
{
  gconstpointer memory;
  GByteArray *array;
  GBytes *bytes;
  gsize size;

  /* Memory copied: two references */
  bytes = g_bytes_new (NYAN, N_NYAN);
  bytes = g_bytes_ref (bytes);
  memory = g_bytes_get_data (bytes, NULL);
  array = g_bytes_unref_to_array (bytes);
  g_assert (array != NULL);
  g_assert (array->data != memory);
  g_assert_cmpmem (array->data, array->len, NYAN, N_NYAN);
  g_byte_array_unref (array);
  g_assert (g_bytes_get_data (bytes, &size) == memory);
  g_assert_cmpuint (size, ==, N_NYAN);
  g_assert_cmpuint (g_bytes_get_size (bytes), ==, N_NYAN);
  g_bytes_unref (bytes);
}

static void
test_to_array_non_malloc (void)
{
  GByteArray *array;
  GBytes *bytes;

  /* Memory copied: non malloc memory */
  bytes = g_bytes_new_static (NYAN, N_NYAN);
  g_assert (g_bytes_get_data (bytes, NULL) == NYAN);
  array = g_bytes_unref_to_array (bytes);
  g_assert (array != NULL);
  g_assert (array->data != (gpointer)NYAN);
  g_assert_cmpmem (array->data, array->len, NYAN, N_NYAN);
  g_byte_array_unref (array);
}

static void
test_null (void)
{
  GBytes *bytes;
  gpointer data;
  gsize size;

  bytes = g_bytes_new (NULL, 0);

  data = g_bytes_unref_to_data (bytes, &size);

  g_assert (data == NULL);
  g_assert (size == 0);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_bug_base ("https://bugzilla.gnome.org/");

  g_test_add_func ("/bytes/new", test_new);
  g_test_add_func ("/bytes/new-take", test_new_take);
  g_test_add_func ("/bytes/new-static", test_new_static);
  g_test_add_func ("/bytes/new-with-free-func", test_new_with_free_func);
  g_test_add_func ("/bytes/new-from-bytes", test_new_from_bytes);
  g_test_add_func ("/bytes/new-from-bytes-slice", test_new_from_bytes_slice);
  g_test_add_func ("/bytes/new-from-bytes-shared-ref", test_new_from_bytes_shared_ref);
  g_test_add_func ("/bytes/hash", test_hash);
  g_test_add_func ("/bytes/equal", test_equal);
  g_test_add_func ("/bytes/compare", test_compare);
  g_test_add_func ("/bytes/to-data/transfered", test_to_data_transferred);
  g_test_add_func ("/bytes/to-data/two-refs", test_to_data_two_refs);
  g_test_add_func ("/bytes/to-data/non-malloc", test_to_data_non_malloc);
  g_test_add_func ("/bytes/to-array/transfered", test_to_array_transferred);
  g_test_add_func ("/bytes/to-array/two-refs", test_to_array_two_refs);
  g_test_add_func ("/bytes/to-array/non-malloc", test_to_array_non_malloc);
  g_test_add_func ("/bytes/null", test_null);

  return g_test_run ();
}
