/* GLib testing framework examples and tests
 * Copyright (C) 2008 Red Hat, Inc.
 * Author: Matthias Clasen
 *
 * This work is provided "as is"; redistribution and modification
 * in whole or in part, in any medium, physical or electronic is
 * permitted without restriction.
 *
 * This work is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * In no event shall the authors or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even
 * if advised of the possibility of such damage.
 */
#include <glib/glib.h>
#include <gio/gio.h>
#include <stdlib.h>
#include <string.h>

static void
test_truncate (void)
{
  GOutputStream *mo;
  GDataOutputStream *o;
  int i;
  GError *error = NULL;
  guint8 *data;

  g_test_bug ("540423");

  mo = g_memory_output_stream_new_resizable ();
  g_assert (g_seekable_can_truncate (G_SEEKABLE (mo)));
  o = g_data_output_stream_new (mo);
  for (i = 0; i < 1000; i++)
    {
      g_data_output_stream_put_byte (o, 1, NULL, &error);
      g_assert_no_error (error);
    }
  g_assert_cmpint (g_memory_output_stream_get_data_size (G_MEMORY_OUTPUT_STREAM (mo)), ==, 1000);
  g_seekable_truncate (G_SEEKABLE (mo), 0, NULL, &error);
  g_assert_cmpuint (g_seekable_tell (G_SEEKABLE (mo)), ==, 1000);
  
  g_assert_no_error (error);
  g_assert_cmpint (g_memory_output_stream_get_data_size (G_MEMORY_OUTPUT_STREAM (mo)), ==, 0);
  for (i = 0; i < 2000; i++)
    {
      g_data_output_stream_put_byte (o, 2, NULL, &error);
      g_assert_no_error (error);
    }
  g_assert_cmpint (g_memory_output_stream_get_data_size (G_MEMORY_OUTPUT_STREAM (mo)), ==, 3000);

  data = (guint8 *)g_memory_output_stream_get_data (G_MEMORY_OUTPUT_STREAM (mo));

  /* The 1's written initially were lost when we truncated to 0
   * and then started writing at position 1000.
   */
  for (i = 0; i < 1000; i++)
    g_assert_cmpuint (data[i], ==, 0);
  for (i = 1000; i < 3000; i++)
    g_assert_cmpuint (data[i], ==, 2);

  g_test_bug ("720080");

  g_seekable_truncate (G_SEEKABLE (mo), 8192, NULL, &error);
  g_assert_cmpint (g_memory_output_stream_get_data_size (G_MEMORY_OUTPUT_STREAM (mo)), ==, 8192);

  data = (guint8 *)g_memory_output_stream_get_data (G_MEMORY_OUTPUT_STREAM (mo));
  for (i = 3000; i < 8192; i++)
    g_assert_cmpuint (data[i], ==, 0);

  g_object_unref (o);
  g_object_unref (mo);
}

static void
test_seek_fixed (void)
{
  GOutputStream *mo;
  GError *error;

  mo = g_memory_output_stream_new (g_new (gchar, 100), 100, NULL, g_free);

  g_assert (G_IS_SEEKABLE (mo));
  g_assert (g_seekable_can_seek (G_SEEKABLE (mo)));
  g_assert_cmpint (g_seekable_tell (G_SEEKABLE (mo)), ==, 0);

  error = NULL;
  g_assert (!g_seekable_seek (G_SEEKABLE (mo), 222, G_SEEK_CUR, NULL, &error));
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);
  g_clear_error (&error);
  g_assert_cmpint (g_seekable_tell (G_SEEKABLE (mo)), ==, 0);

  g_assert (g_seekable_seek (G_SEEKABLE (mo), 26, G_SEEK_SET, NULL, &error));
  g_assert_no_error (error);
  g_assert_cmpint (g_seekable_tell (G_SEEKABLE (mo)), ==, 26);

  g_assert (g_seekable_seek (G_SEEKABLE (mo), 20, G_SEEK_CUR, NULL, &error));
  g_assert_cmpint (g_seekable_tell (G_SEEKABLE (mo)), ==, 46);
  g_assert_no_error (error);

  g_assert (!g_seekable_seek (G_SEEKABLE (mo), 200, G_SEEK_CUR, NULL, &error));
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);
  g_clear_error (&error);
  g_assert_cmpint (g_seekable_tell (G_SEEKABLE (mo)), ==, 46);

  g_assert (!g_seekable_seek (G_SEEKABLE (mo), 1, G_SEEK_END, NULL, &error));
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);
  g_clear_error (&error);
  g_assert_cmpint (g_seekable_tell (G_SEEKABLE (mo)), ==, 46);

  g_assert (g_seekable_seek (G_SEEKABLE (mo), 0, G_SEEK_END, NULL, &error));
  g_assert_no_error (error);
  g_assert_cmpint (g_seekable_tell (G_SEEKABLE (mo)), ==, 100);

  g_assert (g_seekable_seek (G_SEEKABLE (mo), -1, G_SEEK_END, NULL, &error));
  g_assert_no_error (error);
  g_assert_cmpint (g_seekable_tell (G_SEEKABLE (mo)), ==, 99);

  g_object_unref (mo);
}

static void
test_seek_resizable_stream (GOutputStream *mo)
{
  GError *error;

  g_assert (G_IS_SEEKABLE (mo));
  g_assert (g_seekable_can_seek (G_SEEKABLE (mo)));
  g_assert_cmpint (g_seekable_tell (G_SEEKABLE (mo)), ==, 0);

  error = NULL;
  g_assert (g_seekable_seek (G_SEEKABLE (mo), 222, G_SEEK_CUR, NULL, &error));
  g_assert_no_error (error);
  g_assert_cmpint (g_seekable_tell (G_SEEKABLE (mo)), ==, 222);

  g_assert (g_seekable_seek (G_SEEKABLE (mo), 26, G_SEEK_SET, NULL, &error));
  g_assert_no_error (error);
  g_assert_cmpint (g_seekable_tell (G_SEEKABLE (mo)), ==, 26);

  g_assert (g_seekable_seek (G_SEEKABLE (mo), 20, G_SEEK_CUR, NULL, &error));
  g_assert_cmpint (g_seekable_tell (G_SEEKABLE (mo)), ==, 46);
  g_assert_no_error (error);

  g_assert (g_seekable_seek (G_SEEKABLE (mo), 200, G_SEEK_CUR, NULL, &error));
  g_assert_cmpint (g_seekable_tell (G_SEEKABLE (mo)), ==, 246);
  g_assert_no_error (error);

  g_assert (g_seekable_seek (G_SEEKABLE (mo), 1, G_SEEK_END, NULL, &error));
  g_assert_cmpint (g_seekable_tell (G_SEEKABLE (mo)), ==, 1);
  g_assert_no_error (error);

  g_assert (g_seekable_seek (G_SEEKABLE (mo), 0, G_SEEK_END, NULL, &error));
  g_assert_no_error (error);
  g_assert_cmpint (g_seekable_tell (G_SEEKABLE (mo)), ==, 0);

  /* The 'end' is still zero, so this should fail */
  g_assert (!g_seekable_seek (G_SEEKABLE (mo), -1, G_SEEK_END, NULL, &error));
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);
  g_clear_error (&error);
  g_assert_cmpint (g_seekable_tell (G_SEEKABLE (mo)), ==, 0);
}

static void
test_seek_resizable (void)
{
  GOutputStream *mo;
  gint i;

  /* For resizable streams, the initially allocated size is purely an
   * implementation detail.  We should not be able to tell the
   * difference based on the seek API, so make a bunch of streams with
   * different sizes and subject them to the same test.
   */
  for (i = 0; i < 1024; i++)
    {
      mo = g_memory_output_stream_new (g_malloc (i), i, g_realloc, g_free);

      test_seek_resizable_stream (mo);

      g_assert_cmpint (g_memory_output_stream_get_data_size (G_MEMORY_OUTPUT_STREAM (mo)), ==, 0);
      /* No writes = no resizes */
      g_assert_cmpint (g_memory_output_stream_get_size (G_MEMORY_OUTPUT_STREAM (mo)), ==, i);

      g_object_unref (mo);
    }
}

static void
test_data_size (void)
{
  GOutputStream *mo;
  GDataOutputStream *o;
  int pos;

  g_test_bug ("540459");

  mo = g_memory_output_stream_new_resizable ();
  o = g_data_output_stream_new (mo);
  g_data_output_stream_put_byte (o, 1, NULL, NULL);
  pos = g_memory_output_stream_get_data_size (G_MEMORY_OUTPUT_STREAM (mo));
  g_assert_cmpint (pos, ==, 1);

  g_seekable_seek (G_SEEKABLE (mo), 0, G_SEEK_CUR, NULL, NULL);
  pos = g_seekable_tell (G_SEEKABLE (mo));
  g_assert_cmpint (pos, ==, 1);

  g_test_bug ("540461");
  
  g_seekable_seek (G_SEEKABLE (mo), 0, G_SEEK_SET, NULL, NULL);
  pos = g_seekable_tell (G_SEEKABLE (mo));
  g_assert_cmpint (pos, ==, 0);
  
  pos = g_memory_output_stream_get_data_size (G_MEMORY_OUTPUT_STREAM (mo));
  g_assert_cmpint (pos, ==, 1);

  g_assert_cmpint (g_memory_output_stream_get_size (G_MEMORY_OUTPUT_STREAM (mo)), ==, 16);

  g_object_unref (o);
  g_object_unref (mo);
}

static void
test_properties (void)
{
  GOutputStream *mo;
  GDataOutputStream *o;
  int i;
  GError *error = NULL;
  gsize data_size_fun;
  gsize data_size_prop = 0;
  gpointer data_fun;
  gpointer data_prop;
  gpointer func;

  g_test_bug ("605733");

  mo = (GOutputStream*) g_object_new (G_TYPE_MEMORY_OUTPUT_STREAM,
                                      "realloc-function", g_realloc,
                                      "destroy-function", g_free,
                                      NULL);
  o = g_data_output_stream_new (mo);

  for (i = 0; i < 1000; i++)
    {
      g_data_output_stream_put_byte (o, 1, NULL, &error);
      g_assert_no_error (error);
    }

  data_size_fun = g_memory_output_stream_get_data_size (G_MEMORY_OUTPUT_STREAM (mo));
  g_object_get (mo, "data-size", &data_size_prop, NULL);
  g_assert_cmpint (data_size_fun, ==, data_size_prop);

  data_fun = g_memory_output_stream_get_data (G_MEMORY_OUTPUT_STREAM (mo));
  g_object_get (mo, "data", &data_prop, NULL);
  g_assert_cmphex (GPOINTER_TO_SIZE (data_fun), ==, GPOINTER_TO_SIZE (data_prop));

  g_object_get (mo, "realloc-function", &func, NULL);
  g_assert (func == g_realloc);
  g_object_get (mo, "destroy-function", &func, NULL);
  g_assert (func == g_free);

  data_size_fun = g_memory_output_stream_get_size (G_MEMORY_OUTPUT_STREAM (mo));
  g_object_get (mo, "size", &data_size_prop, NULL);
  g_assert_cmpint (data_size_fun, ==, data_size_prop);

  g_object_unref (o);
  g_object_unref (mo);
}

static void
test_write_bytes (void)
{
  GOutputStream *mo;
  GBytes *bytes, *bytes2;
  GError *error = NULL;

  mo = (GOutputStream*) g_object_new (G_TYPE_MEMORY_OUTPUT_STREAM,
                                      "realloc-function", g_realloc,
                                      "destroy-function", g_free,
                                      NULL);
  bytes = g_bytes_new_static ("hello world!", strlen ("hello world!") + 1);
  g_output_stream_write_bytes (mo, bytes, NULL, &error);
  g_assert_no_error (error);

  g_output_stream_close (mo, NULL, &error);
  g_assert_no_error (error);

  bytes2 = g_memory_output_stream_steal_as_bytes (G_MEMORY_OUTPUT_STREAM (mo));
  g_object_unref (mo);
  g_assert (g_bytes_equal (bytes, bytes2));

  g_bytes_unref (bytes);
  g_bytes_unref (bytes2);
}

static void
test_steal_as_bytes (void)
{
  GOutputStream *mo;
  GDataOutputStream *o;
  GError *error = NULL;
  GBytes *bytes;
  gsize size;

  mo = (GOutputStream*) g_object_new (G_TYPE_MEMORY_OUTPUT_STREAM,
                                      "realloc-function", g_realloc,
                                      "destroy-function", g_free,
                                      NULL);
  o = g_data_output_stream_new (mo);

  g_data_output_stream_put_string (o, "hello ", NULL, &error);
  g_assert_no_error (error);

  g_data_output_stream_put_string (o, "world!", NULL, &error);
  g_assert_no_error (error);

  g_data_output_stream_put_byte (o, '\0', NULL, &error);
  g_assert_no_error (error);

  g_output_stream_close ((GOutputStream*) o, NULL, &error);
  g_assert_no_error (error);

  bytes = g_memory_output_stream_steal_as_bytes ((GMemoryOutputStream*)mo);
  g_object_unref (mo);

  g_assert_cmpint (g_bytes_get_size (bytes), ==, strlen ("hello world!") + 1);
  g_assert_cmpstr (g_bytes_get_data (bytes, &size), ==, "hello world!");

  g_bytes_unref (bytes);
  g_object_unref (o);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  g_test_bug_base ("http://bugzilla.gnome.org/");

  g_test_add_func ("/memory-output-stream/truncate", test_truncate);
  g_test_add_func ("/memory-output-stream/seek/fixed", test_seek_fixed);
  g_test_add_func ("/memory-output-stream/seek/resizable", test_seek_resizable);
  g_test_add_func ("/memory-output-stream/get-data-size", test_data_size);
  g_test_add_func ("/memory-output-stream/properties", test_properties);
  g_test_add_func ("/memory-output-stream/write-bytes", test_write_bytes);
  g_test_add_func ("/memory-output-stream/steal_as_bytes", test_steal_as_bytes);

  return g_test_run();
}
