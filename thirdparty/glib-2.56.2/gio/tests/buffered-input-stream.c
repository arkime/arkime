/* GLib testing framework examples and tests
 * Copyright (C) 2008 Red Hat, Inc.
 * Authors: Matthias Clasen <mclasen@redhat.com>
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
test_peek (void)
{
  GInputStream *base;
  GInputStream *in;
  gssize npeek;
  char *buffer;

  base = g_memory_input_stream_new_from_data ("abcdefghijk", -1, NULL);
  in = g_buffered_input_stream_new_sized (base, 64);

  g_buffered_input_stream_fill (G_BUFFERED_INPUT_STREAM (in), 5, NULL, NULL);
  g_assert_cmpint (g_buffered_input_stream_get_available (G_BUFFERED_INPUT_STREAM (in)), ==, 5);
  g_buffered_input_stream_fill (G_BUFFERED_INPUT_STREAM (in), -1, NULL, NULL);
  g_assert_cmpint (g_buffered_input_stream_get_available (G_BUFFERED_INPUT_STREAM (in)), ==, strlen ("abcdefjhijk"));

  buffer = g_new0 (char, 64);
  npeek = g_buffered_input_stream_peek (G_BUFFERED_INPUT_STREAM (in), buffer, 2, 3);
  g_assert_cmpint (npeek, ==, 3);
  g_assert_cmpstr ("cde", ==, buffer);
  g_free (buffer);

  buffer = g_new0 (char, 64);
  npeek = g_buffered_input_stream_peek (G_BUFFERED_INPUT_STREAM (in), buffer, 9, 5);
  g_assert_cmpint (npeek, ==, 2);
  g_assert_cmpstr ("jk", ==, buffer);
  g_free (buffer);

  buffer = g_new0 (char, 64);
  npeek = g_buffered_input_stream_peek (G_BUFFERED_INPUT_STREAM (in), buffer, 75, 3);
  g_assert_cmpint (npeek, ==, 0);
  g_free (buffer);

  g_object_unref (in);
  g_object_unref (base);
}

static void
test_peek_buffer (void)
{
  GInputStream *base;
  GInputStream *in;
  gssize nfill;
  gsize bufsize;
  char *buffer;

  base = g_memory_input_stream_new_from_data ("abcdefghijk", -1, NULL);
  in = g_buffered_input_stream_new (base);

  nfill = g_buffered_input_stream_fill (G_BUFFERED_INPUT_STREAM (in), strlen ("abcdefghijk"), NULL, NULL);
  buffer = (char *) g_buffered_input_stream_peek_buffer (G_BUFFERED_INPUT_STREAM (in), &bufsize);
  g_assert_cmpint (nfill, ==, bufsize);
  g_assert (0 == strncmp ("abcdefghijk", buffer, bufsize));

  g_object_unref (in);
  g_object_unref (base);
}

static void
test_set_buffer_size (void)
{
  GInputStream *base;
  GInputStream *in;
  guint bufsize_prop;
  gsize size, bufsize;

  base = g_memory_input_stream_new_from_data ("abcdefghijk", -1, NULL);
  in = g_buffered_input_stream_new (base);
  size = g_buffered_input_stream_get_buffer_size (G_BUFFERED_INPUT_STREAM (in));
  g_assert_cmpint (size, ==, 4096);

  g_buffered_input_stream_set_buffer_size (G_BUFFERED_INPUT_STREAM (in), 64);
  size = g_buffered_input_stream_get_buffer_size (G_BUFFERED_INPUT_STREAM (in));
  g_assert_cmpint (size, ==, 64);

  /* size cannot shrink below current content len */
  g_buffered_input_stream_fill (G_BUFFERED_INPUT_STREAM (in), strlen ("abcdefghijk"), NULL, NULL);
  g_buffered_input_stream_peek_buffer (G_BUFFERED_INPUT_STREAM (in), &bufsize);
  g_buffered_input_stream_set_buffer_size (G_BUFFERED_INPUT_STREAM (in), 2);
  size = g_buffered_input_stream_get_buffer_size (G_BUFFERED_INPUT_STREAM (in));
  g_assert_cmpint (size, ==, bufsize);
  g_object_get (in, "buffer-size", &bufsize_prop, NULL);
  g_assert_cmpint (bufsize_prop, ==, bufsize);

  g_object_unref (in);

  in = g_buffered_input_stream_new_sized (base, 64);
  size = g_buffered_input_stream_get_buffer_size (G_BUFFERED_INPUT_STREAM (in));
  g_assert_cmpint (size, ==, 64);

  g_object_unref (in);
  g_object_unref (base);
}

static void
test_read_byte (void)
{
  GInputStream *base;
  GInputStream *in;
  GError *error;

  g_test_bug ("562393");

  base = g_memory_input_stream_new_from_data ("abcdefgh", -1, NULL);
  in = g_buffered_input_stream_new (base);

  error = NULL;
  g_assert_cmpint (g_buffered_input_stream_read_byte (G_BUFFERED_INPUT_STREAM (in), NULL, &error), ==, 'a');
  g_assert_no_error (error);
  g_assert_cmpint (g_buffered_input_stream_read_byte (G_BUFFERED_INPUT_STREAM (in), NULL, &error), ==, 'b');
  g_assert_no_error (error);
  g_assert_cmpint (g_buffered_input_stream_read_byte (G_BUFFERED_INPUT_STREAM (in), NULL, &error), ==, 'c');
  g_assert_no_error (error);

  g_assert_cmpint (g_input_stream_skip (in, 3, NULL, &error), ==, 3);
  g_assert_no_error (error);

  g_assert_cmpint (g_buffered_input_stream_read_byte (G_BUFFERED_INPUT_STREAM (in), NULL, &error), ==, 'g');
  g_assert_no_error (error);
  g_assert_cmpint (g_buffered_input_stream_read_byte (G_BUFFERED_INPUT_STREAM (in), NULL, &error), ==, 'h');
  g_assert_no_error (error);
  g_assert_cmpint (g_buffered_input_stream_read_byte (G_BUFFERED_INPUT_STREAM (in), NULL, &error), ==, -1);
  g_assert_no_error (error);

  g_assert (g_input_stream_close (in, NULL, &error));
  g_assert_no_error (error);
  g_assert_cmpint (g_buffered_input_stream_read_byte (G_BUFFERED_INPUT_STREAM (in), NULL, &error), ==, -1);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_CLOSED);
  g_error_free (error);

  g_object_unref (in);
  g_object_unref (base);
}

static void
test_read (void)
{
  GInputStream *base;
  GInputStream *in;
  gchar buffer[20];
  GError *error;

  base = g_memory_input_stream_new_from_data ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ", -1, NULL);
  in = g_buffered_input_stream_new_sized (base, 8);

  g_assert_cmpint (g_buffered_input_stream_get_available (G_BUFFERED_INPUT_STREAM (in)), ==, 0);

  error = NULL;
  g_assert_cmpint (g_buffered_input_stream_fill (G_BUFFERED_INPUT_STREAM (in), 8, NULL, &error), ==, 8);
  g_assert_no_error (error);

  g_assert_cmpint (g_buffered_input_stream_get_available (G_BUFFERED_INPUT_STREAM (in)), ==, 8);

  memset (buffer, 0, 20);
  g_assert_cmpint (g_input_stream_read (in, &buffer, 16, NULL, &error), ==, 16);
  g_assert_cmpstr (buffer, ==, "abcdefghijklmnop");
  g_assert_no_error (error);

  g_assert_cmpint (g_buffered_input_stream_get_available (G_BUFFERED_INPUT_STREAM (in)), ==, 0);

  memset (buffer, 0, 20);
  g_assert_cmpint (g_input_stream_read (in, &buffer, 16, NULL, &error), ==, 16);
  g_assert_cmpstr (buffer, ==, "qrstuvwxyzABCDEF");
  g_assert_no_error (error);

  memset (buffer, 0, 20);
  g_assert_cmpint (g_input_stream_read (in, &buffer, 16, NULL, &error), ==, 16);
  g_assert_cmpstr (buffer, ==, "GHIJKLMNOPQRSTUV");
  g_assert_no_error (error);

  memset (buffer, 0, 20);
  g_assert_cmpint (g_input_stream_read (in, &buffer, 16, NULL, &error), ==, 4);
  g_assert_cmpstr (buffer, ==, "WXYZ");
  g_assert_no_error (error);

  memset (buffer, 0, 20);
  g_assert_cmpint (g_input_stream_read (in, &buffer, 16, NULL, &error), ==, 0);
  g_assert_no_error (error);

  g_object_unref (in);
  g_object_unref (base);
}

static void
return_result_cb (GObject      *object,
                  GAsyncResult *result,
                  gpointer      user_data)
{
  GAsyncResult **ret = user_data;

  *ret = g_object_ref (result);
}

static void
test_read_async (void)
{
  GInputStream *base;
  GInputStream *in;
  gchar buffer[20];
  GError *error;
  GAsyncResult *result;

  base = g_memory_input_stream_new_from_data ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ", -1, NULL);
  in = g_buffered_input_stream_new_sized (base, 8);

  g_assert_cmpint (g_buffered_input_stream_get_available (G_BUFFERED_INPUT_STREAM (in)), ==, 0);

  error = NULL;
  result = NULL;
  g_buffered_input_stream_fill_async (G_BUFFERED_INPUT_STREAM (in), 8,
                                      G_PRIORITY_DEFAULT, NULL,
                                      return_result_cb, &result);
  while (!result)
    g_main_context_iteration (NULL, TRUE);
  g_assert_cmpint (g_buffered_input_stream_fill_finish (G_BUFFERED_INPUT_STREAM (in), result, &error), ==, 8);
  g_assert_no_error (error);
  g_clear_object (&result);

  g_assert_cmpint (g_buffered_input_stream_get_available (G_BUFFERED_INPUT_STREAM (in)), ==, 8);

  memset (buffer, 0, 20);
  g_input_stream_read_async (in, &buffer, 16, G_PRIORITY_DEFAULT,
                             NULL, return_result_cb, &result);
  while (!result)
    g_main_context_iteration (NULL, TRUE);
  g_assert_cmpint (g_input_stream_read_finish (in, result, &error), ==, 16);
  g_assert_cmpstr (buffer, ==, "abcdefghijklmnop");
  g_assert_no_error (error);
  g_clear_object (&result);

  g_assert_cmpint (g_buffered_input_stream_get_available (G_BUFFERED_INPUT_STREAM (in)), ==, 0);

  memset (buffer, 0, 20);
  g_input_stream_read_async (in, &buffer, 16, G_PRIORITY_DEFAULT,
                             NULL, return_result_cb, &result);
  while (!result)
    g_main_context_iteration (NULL, TRUE);
  g_assert_cmpint (g_input_stream_read_finish (in, result, &error), ==, 16);
  g_assert_cmpstr (buffer, ==, "qrstuvwxyzABCDEF");
  g_assert_no_error (error);
  g_clear_object (&result);

  memset (buffer, 0, 20);
  g_input_stream_read_async (in, &buffer, 16, G_PRIORITY_DEFAULT,
                             NULL, return_result_cb, &result);
  while (!result)
    g_main_context_iteration (NULL, TRUE);
  g_assert_cmpint (g_input_stream_read_finish (in, result, &error), ==, 16);
  g_assert_cmpstr (buffer, ==, "GHIJKLMNOPQRSTUV");
  g_assert_no_error (error);
  g_clear_object (&result);

  memset (buffer, 0, 20);
  g_input_stream_read_async (in, &buffer, 16, G_PRIORITY_DEFAULT,
                             NULL, return_result_cb, &result);
  while (!result)
    g_main_context_iteration (NULL, TRUE);
  g_assert_cmpint (g_input_stream_read_finish (in, result, &error), ==, 4);
  g_assert_cmpstr (buffer, ==, "WXYZ");
  g_assert_no_error (error);
  g_clear_object (&result);

  memset (buffer, 0, 20);
  g_input_stream_read_async (in, &buffer, 16, G_PRIORITY_DEFAULT,
                             NULL, return_result_cb, &result);
  while (!result)
    g_main_context_iteration (NULL, TRUE);
  g_assert_cmpint (g_input_stream_read_finish (in, result, &error), ==, 0);
  g_assert_no_error (error);
  g_clear_object (&result);

  g_object_unref (in);
  g_object_unref (base);
}

static void
test_skip (void)
{
  GInputStream *base;
  GInputStream *in;
  GError *error;

  base = g_memory_input_stream_new_from_data ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVXYZ", -1, NULL);
  in = g_buffered_input_stream_new_sized (base, 5);

  error = NULL;
  g_assert_cmpint (g_buffered_input_stream_read_byte (G_BUFFERED_INPUT_STREAM (in), NULL, &error), ==, 'a');
  g_assert_no_error (error);
  g_assert_cmpint (g_buffered_input_stream_read_byte (G_BUFFERED_INPUT_STREAM (in), NULL, &error), ==, 'b');
  g_assert_no_error (error);
  g_assert_cmpint (g_buffered_input_stream_read_byte (G_BUFFERED_INPUT_STREAM (in), NULL, &error), ==, 'c');
  g_assert_no_error (error);

  g_assert_cmpint (g_input_stream_skip (in, 7, NULL, &error), ==, 7);
  g_assert_no_error (error);
  g_assert_cmpint (g_buffered_input_stream_read_byte (G_BUFFERED_INPUT_STREAM (in), NULL, &error), ==, 'k');
  g_assert_no_error (error);

  g_assert_cmpint (g_input_stream_skip (in, 10, NULL, &error), ==, 10);
  g_assert_no_error (error);
  g_assert_cmpint (g_buffered_input_stream_read_byte (G_BUFFERED_INPUT_STREAM (in), NULL, &error), ==, 'v');
  g_assert_no_error (error);

  g_assert_cmpint (g_input_stream_skip (in, 20, NULL, &error), ==, 20);
  g_assert_no_error (error);
  g_assert_cmpint (g_buffered_input_stream_read_byte (G_BUFFERED_INPUT_STREAM (in), NULL, &error), ==, 'Q');
  g_assert_no_error (error);

  g_assert_cmpint (g_input_stream_skip (in, 10, NULL, &error), ==, 8);
  g_assert_no_error (error);
  g_assert_cmpint (g_input_stream_skip (in, 10, NULL, &error), ==, 0);
  g_assert_no_error (error);

  g_object_unref (in);
  g_object_unref (base);
}

static void
test_skip_async (void)
{
  GInputStream *base;
  GInputStream *in;
  GError *error;
  GAsyncResult *result;

  base = g_memory_input_stream_new_from_data ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVXYZ", -1, NULL);
  in = g_buffered_input_stream_new_sized (base, 5);

  error = NULL;
  g_assert_cmpint (g_buffered_input_stream_read_byte (G_BUFFERED_INPUT_STREAM (in), NULL, &error), ==, 'a');
  g_assert_no_error (error);
  g_assert_cmpint (g_buffered_input_stream_read_byte (G_BUFFERED_INPUT_STREAM (in), NULL, &error), ==, 'b');
  g_assert_no_error (error);
  g_assert_cmpint (g_buffered_input_stream_read_byte (G_BUFFERED_INPUT_STREAM (in), NULL, &error), ==, 'c');
  g_assert_no_error (error);

  result = NULL;
  g_input_stream_skip_async (in, 7, G_PRIORITY_DEFAULT,
                             NULL, return_result_cb, &result);
  while (!result)
    g_main_context_iteration (NULL, TRUE);
  g_assert_cmpint (g_input_stream_skip_finish (in, result, &error), ==, 7);
  g_assert_no_error (error);
  g_clear_object (&result);
  g_assert_cmpint (g_buffered_input_stream_read_byte (G_BUFFERED_INPUT_STREAM (in), NULL, &error), ==, 'k');
  g_assert_no_error (error);

  g_input_stream_skip_async (in, 10, G_PRIORITY_DEFAULT,
                             NULL, return_result_cb, &result);
  while (!result)
    g_main_context_iteration (NULL, TRUE);
  g_assert_cmpint (g_input_stream_skip_finish (in, result, &error), ==, 10);
  g_assert_no_error (error);
  g_clear_object (&result);
  g_assert_cmpint (g_buffered_input_stream_read_byte (G_BUFFERED_INPUT_STREAM (in), NULL, &error), ==, 'v');
  g_assert_no_error (error);

  g_input_stream_skip_async (in, 20, G_PRIORITY_DEFAULT,
                             NULL, return_result_cb, &result);
  while (!result)
    g_main_context_iteration (NULL, TRUE);
  g_assert_cmpint (g_input_stream_skip_finish (in, result, &error), ==, 20);
  g_assert_no_error (error);
  g_clear_object (&result);
  g_assert_cmpint (g_buffered_input_stream_read_byte (G_BUFFERED_INPUT_STREAM (in), NULL, &error), ==, 'Q');
  g_assert_no_error (error);

  g_input_stream_skip_async (in, 10, G_PRIORITY_DEFAULT,
                             NULL, return_result_cb, &result);
  while (!result)
    g_main_context_iteration (NULL, TRUE);
  g_assert_cmpint (g_input_stream_skip_finish (in, result, &error), ==, 8);
  g_clear_object (&result);
  g_assert_no_error (error);

  g_input_stream_skip_async (in, 10, G_PRIORITY_DEFAULT,
                             NULL, return_result_cb, &result);
  while (!result)
    g_main_context_iteration (NULL, TRUE);
  g_assert_cmpint (g_input_stream_skip_finish (in, result, &error), ==, 0);
  g_clear_object (&result);
  g_assert_no_error (error);

  g_object_unref (in);
  g_object_unref (base);
}

static void
test_close (void)
{
  GInputStream *base;
  GInputStream *in;
  GError *error;

  base = g_memory_input_stream_new_from_data ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVXYZ", -1, NULL);
  in = g_buffered_input_stream_new (base);

  g_assert (g_filter_input_stream_get_close_base_stream (G_FILTER_INPUT_STREAM (in)));

  error = NULL;
  g_assert (g_input_stream_close (in, NULL, &error));
  g_assert_no_error (error);
  g_assert (g_input_stream_is_closed (base));

  g_object_unref (in);
  g_object_unref (base);

  base = g_memory_input_stream_new_from_data ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVXYZ", -1, NULL);
  in = g_buffered_input_stream_new (base);

  g_filter_input_stream_set_close_base_stream (G_FILTER_INPUT_STREAM (in), FALSE);

  error = NULL;
  g_assert (g_input_stream_close (in, NULL, &error));
  g_assert_no_error (error);
  g_assert (!g_input_stream_is_closed (base));

  g_object_unref (in);
  g_object_unref (base);
}

static void
test_seek (void)
{
  GInputStream *base;
  GInputStream *in;
  GError *error;
  gint byte;
  gboolean ret;

  base = g_memory_input_stream_new_from_data ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVXYZ", -1, NULL);
  in = g_buffered_input_stream_new_sized (base, 4);
  error = NULL;

  /* Seek by read */
  g_assert_cmpint (g_seekable_tell (G_SEEKABLE (in)), ==, 0);
  byte = g_buffered_input_stream_read_byte (G_BUFFERED_INPUT_STREAM (in), NULL, &error);
  g_assert_no_error (error);
  g_assert_cmpint (byte, ==, 'a');
  g_assert_cmpint (g_seekable_tell (G_SEEKABLE (in)), ==, 1);

  /* Seek forward (in buffer) */
  ret = g_seekable_seek (G_SEEKABLE (in), 1, G_SEEK_CUR, NULL, &error);
  g_assert_no_error (error);
  g_assert (ret);
  g_assert_cmpint (g_seekable_tell (G_SEEKABLE (in)), ==, 2);
  byte = g_buffered_input_stream_read_byte (G_BUFFERED_INPUT_STREAM (in), NULL, &error);
  g_assert_no_error (error);
  g_assert_cmpint (byte, ==, 'c');
  g_assert_cmpint (g_seekable_tell (G_SEEKABLE (in)), ==, 3);

  /* Seek backward (in buffer) */
  ret = g_seekable_seek (G_SEEKABLE (in), -2, G_SEEK_CUR, NULL, &error);
  g_assert_no_error (error);
  g_assert (ret);
  g_assert_cmpint (g_seekable_tell (G_SEEKABLE (in)), ==, 1);
  byte = g_buffered_input_stream_read_byte (G_BUFFERED_INPUT_STREAM (in), NULL, &error);
  g_assert_no_error (error);
  g_assert_cmpint (byte, ==, 'b');
  g_assert_cmpint (g_seekable_tell (G_SEEKABLE (in)), ==, 2);

  /* Seek forward (outside buffer) */
  ret = g_seekable_seek (G_SEEKABLE (in), 6, G_SEEK_CUR, NULL, &error);
  g_assert_no_error (error);
  g_assert (ret);
  g_assert_cmpint (g_seekable_tell (G_SEEKABLE (in)), ==, 8);
  byte = g_buffered_input_stream_read_byte (G_BUFFERED_INPUT_STREAM (in), NULL, &error);
  g_assert_no_error (error);
  g_assert_cmpint (byte, ==, 'i');
  g_assert_cmpint (g_seekable_tell (G_SEEKABLE (in)), ==, 9);

  /* Seek backward (outside buffer) */
  ret = g_seekable_seek (G_SEEKABLE (in), -6, G_SEEK_CUR, NULL, &error);
  g_assert_no_error (error);
  g_assert (ret);
  g_assert_cmpint (g_seekable_tell (G_SEEKABLE (in)), ==, 3);
  byte = g_buffered_input_stream_read_byte (G_BUFFERED_INPUT_STREAM (in), NULL, &error);
  g_assert_no_error (error);
  g_assert_cmpint (byte, ==, 'd');
  g_assert_cmpint (g_seekable_tell (G_SEEKABLE (in)), ==, 4);

  /* Seek from beginning */
  ret = g_seekable_seek (G_SEEKABLE (in), 8, G_SEEK_SET, NULL, &error);
  g_assert_no_error (error);
  g_assert (ret);
  g_assert_cmpint (g_seekable_tell (G_SEEKABLE (in)), ==, 8);
  byte = g_buffered_input_stream_read_byte (G_BUFFERED_INPUT_STREAM (in), NULL, &error);
  g_assert_no_error (error);
  g_assert_cmpint (byte, ==, 'i');
  g_assert_cmpint (g_seekable_tell (G_SEEKABLE (in)), ==, 9);

  /* Seek from end */
  ret = g_seekable_seek (G_SEEKABLE (in), -1, G_SEEK_END, NULL, &error);
  g_assert_no_error (error);
  g_assert (ret);
  g_assert_cmpint (g_seekable_tell (G_SEEKABLE (in)), ==, 50);
  byte = g_buffered_input_stream_read_byte (G_BUFFERED_INPUT_STREAM (in), NULL, &error);
  g_assert_no_error (error);
  g_assert_cmpint (byte, ==, 'Z');
  g_assert_cmpint (g_seekable_tell (G_SEEKABLE (in)), ==, 51);

  /* Cleanup */
  g_object_unref (in);
  g_object_unref (base);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  g_test_bug_base ("http://bugzilla.gnome.org/");

  g_test_add_func ("/buffered-input-stream/peek", test_peek);
  g_test_add_func ("/buffered-input-stream/peek-buffer", test_peek_buffer);
  g_test_add_func ("/buffered-input-stream/set-buffer-size", test_set_buffer_size);
  g_test_add_func ("/buffered-input-stream/read-byte", test_read_byte);
  g_test_add_func ("/buffered-input-stream/read", test_read);
  g_test_add_func ("/buffered-input-stream/read-async", test_read_async);
  g_test_add_func ("/buffered-input-stream/skip", test_skip);
  g_test_add_func ("/buffered-input-stream/skip-async", test_skip_async);
  g_test_add_func ("/buffered-input-stream/seek", test_seek);
  g_test_add_func ("/filter-input-stream/close", test_close);

  return g_test_run();
}
