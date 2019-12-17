/*
 * Copyright © 2009 Codethink Limited
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * See the included COPYING file for more information.
 *
 * Author: Ryan Lortie <desrt@desrt.ca>
 */

#include <string.h>
#include <glib/glib.h>
#include <gio/gio.h>

/* GFilterInputStream and GFilterOutputStream are abstract, so define
 * minimal subclasses for testing. (This used to use
 * GBufferedInputStream and GBufferedOutputStream, but those have
 * their own test program, and they override some methods, meaning the
 * core filter stream functionality wasn't getting fully tested.)
 */

GType test_filter_input_stream_get_type (void);
GType test_filter_output_stream_get_type (void);

#define TEST_TYPE_FILTER_INPUT_STREAM  (test_filter_input_stream_get_type ())
#define TEST_FILTER_INPUT_STREAM(o)    (G_TYPE_CHECK_INSTANCE_CAST ((o), TEST_TYPE_FILTER_INPUT_STREAM, TestFilterInputStream))
#define TEST_TYPE_FILTER_OUTPUT_STREAM (test_filter_output_stream_get_type ())
#define TEST_FILTER_OUTPUT_STREAM(o)   (G_TYPE_CHECK_INSTANCE_CAST ((o), TEST_TYPE_FILTER_OUTPUT_STREAM, TestFilterOutputStream))

typedef GFilterInputStream       TestFilterInputStream;
typedef GFilterInputStreamClass  TestFilterInputStreamClass;
typedef GFilterOutputStream      TestFilterOutputStream;
typedef GFilterOutputStreamClass TestFilterOutputStreamClass;

G_DEFINE_TYPE (TestFilterInputStream, test_filter_input_stream, G_TYPE_FILTER_INPUT_STREAM)
G_DEFINE_TYPE (TestFilterOutputStream, test_filter_output_stream, G_TYPE_FILTER_OUTPUT_STREAM)

static void
test_filter_input_stream_init (TestFilterInputStream *stream)
{
}

static void
test_filter_input_stream_class_init (TestFilterInputStreamClass *klass)
{
}

static void
test_filter_output_stream_init (TestFilterOutputStream *stream)
{
}

static void
test_filter_output_stream_class_init (TestFilterOutputStreamClass *klass)
{
}

/* Now the tests */

static void
test_input_filter (void)
{
  GInputStream *base, *f1, *f2, *s;
  gboolean close_base;
  gchar buf[1024];
  GError *error = NULL;

  g_test_bug ("568394");
  base = g_memory_input_stream_new_from_data ("abcdefghijk", -1, NULL);
  f1 = g_object_new (TEST_TYPE_FILTER_INPUT_STREAM,
                     "base-stream", base,
                     "close-base-stream", FALSE,
                     NULL);
  f2 = g_object_new (TEST_TYPE_FILTER_INPUT_STREAM,
                     "base-stream", base,
                     NULL);

  g_assert (g_filter_input_stream_get_base_stream (G_FILTER_INPUT_STREAM (f1)) == base);
  g_assert (g_filter_input_stream_get_base_stream (G_FILTER_INPUT_STREAM (f2)) == base);

  g_assert (!g_input_stream_is_closed (base));
  g_assert (!g_input_stream_is_closed (f1));
  g_assert (!g_input_stream_is_closed (f2));

  g_object_get (f1,
                "close-base-stream", &close_base,
                "base-stream", &s,
                NULL);
  g_assert (!close_base);
  g_assert (s == base);
  g_object_unref (s);

  g_object_unref (f1);

  g_assert (!g_input_stream_is_closed (base));
  g_assert (!g_input_stream_is_closed (f2));

  g_input_stream_skip (f2, 3, NULL, &error);
  g_assert_no_error (error);

  memset (buf, 0, 1024);
  g_input_stream_read_all (f2, buf, 1024, NULL, NULL, &error);
  g_assert_no_error (error);
  g_assert_cmpstr (buf, ==, "defghijk");

  g_object_unref (f2);

  g_assert (g_input_stream_is_closed (base));

  g_object_unref (base);
}

static void
test_output_filter (void)
{
  GOutputStream *base, *f1, *f2;

  base = g_memory_output_stream_new (NULL, 0, g_realloc, g_free);
  f1 = g_object_new (TEST_TYPE_FILTER_OUTPUT_STREAM,
                     "base-stream", base,
                     "close-base-stream", FALSE,
                     NULL);
  f2 = g_object_new (TEST_TYPE_FILTER_OUTPUT_STREAM,
                     "base-stream", base,
                     NULL);

  g_assert (g_filter_output_stream_get_base_stream (G_FILTER_OUTPUT_STREAM (f1)) == base);
  g_assert (g_filter_output_stream_get_base_stream (G_FILTER_OUTPUT_STREAM (f2)) == base);

  g_assert (!g_output_stream_is_closed (base));
  g_assert (!g_output_stream_is_closed (f1));
  g_assert (!g_output_stream_is_closed (f2));

  g_object_unref (f1);

  g_assert (!g_output_stream_is_closed (base));
  g_assert (!g_output_stream_is_closed (f2));

  g_object_unref (f2);

  g_assert (g_output_stream_is_closed (base));

  g_object_unref (base);
}

gpointer expected_obj;
gpointer expected_data;
gboolean callback_happened;
GMainLoop *loop;

static void
return_result_cb (GObject      *object,
                  GAsyncResult *result,
                  gpointer      user_data)
{
  GAsyncResult **ret = user_data;

  *ret = g_object_ref (result);
  g_main_loop_quit (loop);
}

static void
in_cb (GObject      *object,
       GAsyncResult *result,
       gpointer      user_data)
{
  GError *error = NULL;

  g_assert (object == expected_obj);
  g_assert (user_data == expected_data);
  g_assert (callback_happened == FALSE);

  g_input_stream_close_finish (expected_obj, result, &error);
  g_assert (error == NULL);

  callback_happened = TRUE;
  g_main_loop_quit (loop);
}

static void
test_input_async (void)
{
  GInputStream *base, *f1, *f2;
  char buf[20];
  GAsyncResult *result = NULL;
  GError *error = NULL;

  loop = g_main_loop_new (NULL, FALSE);

  base = g_memory_input_stream_new_from_data ("abcdefghijklmnopqrstuvwxyz", -1, NULL);
  f1 = g_object_new (TEST_TYPE_FILTER_INPUT_STREAM,
                     "base-stream", base,
                     "close-base-stream", FALSE,
                     NULL);
  f2 = g_object_new (TEST_TYPE_FILTER_INPUT_STREAM,
                     "base-stream", base,
                     NULL);

  g_assert (g_filter_input_stream_get_base_stream (G_FILTER_INPUT_STREAM (f1)) == base);
  g_assert (g_filter_input_stream_get_base_stream (G_FILTER_INPUT_STREAM (f2)) == base);


  memset (buf, 0, sizeof (buf));
  g_input_stream_read_async (f1, buf, 10, G_PRIORITY_DEFAULT,
                             NULL, return_result_cb, &result);
  g_main_loop_run (loop);
  g_assert_cmpint (g_input_stream_read_finish (f1, result, &error), ==, 10);
  g_assert_cmpstr (buf, ==, "abcdefghij");
  g_assert_no_error (error);
  g_clear_object (&result);

  g_assert_cmpint (g_seekable_tell (G_SEEKABLE (base)), ==, 10);

  g_input_stream_skip_async (f2, 10, G_PRIORITY_DEFAULT,
                             NULL, return_result_cb, &result);
  g_main_loop_run (loop);
  g_assert_cmpint (g_input_stream_skip_finish (f2, result, &error), ==, 10);
  g_assert_no_error (error);
  g_clear_object (&result);

  g_assert_cmpint (g_seekable_tell (G_SEEKABLE (base)), ==, 20);

  memset (buf, 0, sizeof (buf));
  g_input_stream_read_async (f1, buf, 10, G_PRIORITY_DEFAULT,
                             NULL, return_result_cb, &result);
  g_main_loop_run (loop);
  g_assert_cmpint (g_input_stream_read_finish (f1, result, &error), ==, 6);
  g_assert_cmpstr (buf, ==, "uvwxyz");
  g_assert_no_error (error);
  g_clear_object (&result);

  g_assert_cmpint (g_seekable_tell (G_SEEKABLE (base)), ==, 26);


  g_assert (!g_input_stream_is_closed (base));
  g_assert (!g_input_stream_is_closed (f1));
  g_assert (!g_input_stream_is_closed (f2));

  expected_obj = f1;
  expected_data = g_malloc (20);
  callback_happened = FALSE;
  g_input_stream_close_async (f1, 0, NULL, in_cb, expected_data);

  g_assert (callback_happened == FALSE);
  g_main_loop_run (loop);
  g_assert (callback_happened == TRUE);

  g_assert (!g_input_stream_is_closed (base));
  g_assert (!g_input_stream_is_closed (f2));
  g_free (expected_data);
  g_object_unref (f1);
  g_assert (!g_input_stream_is_closed (base));
  g_assert (!g_input_stream_is_closed (f2));

  expected_obj = f2;
  expected_data = g_malloc (20);
  callback_happened = FALSE;
  g_input_stream_close_async (f2, 0, NULL, in_cb, expected_data);

  g_assert (callback_happened == FALSE);
  g_main_loop_run (loop);
  g_assert (callback_happened == TRUE);

  g_assert (g_input_stream_is_closed (base));
  g_assert (g_input_stream_is_closed (f2));
  g_free (expected_data);
  g_object_unref (f2);

  g_assert (g_input_stream_is_closed (base));
  g_object_unref (base);
  g_main_loop_unref (loop);
}

static void
out_cb (GObject      *object,
        GAsyncResult *result,
        gpointer      user_data)
{
  GError *error = NULL;

  g_assert (object == expected_obj);
  g_assert (user_data == expected_data);
  g_assert (callback_happened == FALSE);

  g_output_stream_close_finish (expected_obj, result, &error);
  g_assert (error == NULL);

  callback_happened = TRUE;
  g_main_loop_quit (loop);
}

static void
test_output_async (void)
{
  GOutputStream *base, *f1, *f2;
  GAsyncResult *result = NULL;
  GError *error = NULL;

  loop = g_main_loop_new (NULL, FALSE);

  base = g_memory_output_stream_new (NULL, 0, g_realloc, g_free);
  f1 = g_object_new (TEST_TYPE_FILTER_OUTPUT_STREAM,
                     "base-stream", base,
                     "close-base-stream", FALSE,
                     NULL);
  f2 = g_object_new (TEST_TYPE_FILTER_OUTPUT_STREAM,
                     "base-stream", base,
                     NULL);

  g_assert (g_filter_output_stream_get_base_stream (G_FILTER_OUTPUT_STREAM (f1)) == base);
  g_assert (g_filter_output_stream_get_base_stream (G_FILTER_OUTPUT_STREAM (f2)) == base);


  g_output_stream_write_async (f1, "abcdefghijklm", 13, G_PRIORITY_DEFAULT,
                               NULL, return_result_cb, &result);
  g_main_loop_run (loop);
  g_assert_cmpint (g_output_stream_write_finish (f1, result, &error), ==, 13);
  g_assert_no_error (error);
  g_clear_object (&result);

  g_assert_cmpint (g_seekable_tell (G_SEEKABLE (base)), ==, 13);

  g_output_stream_write_async (f2, "nopqrstuvwxyz", 13, G_PRIORITY_DEFAULT,
                               NULL, return_result_cb, &result);
  g_main_loop_run (loop);
  g_assert_cmpint (g_output_stream_write_finish (f2, result, &error), ==, 13);
  g_assert_no_error (error);
  g_clear_object (&result);

  g_assert_cmpint (g_seekable_tell (G_SEEKABLE (base)), ==, 26);

  g_assert_cmpint (g_memory_output_stream_get_data_size (G_MEMORY_OUTPUT_STREAM (base)), ==, 26);
  g_output_stream_write (base, "\0", 1, NULL, &error);
  g_assert_no_error (error);
  g_assert_cmpstr (g_memory_output_stream_get_data (G_MEMORY_OUTPUT_STREAM (base)), ==, "abcdefghijklmnopqrstuvwxyz");


  g_assert (!g_output_stream_is_closed (base));
  g_assert (!g_output_stream_is_closed (f1));
  g_assert (!g_output_stream_is_closed (f2));

  expected_obj = f1;
  expected_data = g_malloc (20);
  callback_happened = FALSE;
  g_output_stream_close_async (f1, 0, NULL, out_cb, expected_data);

  g_assert (callback_happened == FALSE);
  g_main_loop_run (loop);
  g_assert (callback_happened == TRUE);

  g_assert (!g_output_stream_is_closed (base));
  g_assert (!g_output_stream_is_closed (f2));
  g_free (expected_data);
  g_object_unref (f1);
  g_assert (!g_output_stream_is_closed (base));
  g_assert (!g_output_stream_is_closed (f2));

  expected_obj = f2;
  expected_data = g_malloc (20);
  callback_happened = FALSE;
  g_output_stream_close_async (f2, 0, NULL, out_cb, expected_data);

  g_assert (callback_happened == FALSE);
  g_main_loop_run (loop);
  g_assert (callback_happened == TRUE);

  g_assert (g_output_stream_is_closed (base));
  g_assert (g_output_stream_is_closed (f2));
  g_free (expected_data);
  g_object_unref (f2);

  g_assert (g_output_stream_is_closed (base));
  g_object_unref (base);
  g_main_loop_unref (loop);
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);
  g_test_bug_base ("http://bugzilla.gnome.org/");

  g_test_add_func ("/filter-stream/input", test_input_filter);
  g_test_add_func ("/filter-stream/output", test_output_filter);
  g_test_add_func ("/filter-stream/async-input", test_input_async);
  g_test_add_func ("/filter-stream/async-output", test_output_async);

  return g_test_run();
}
