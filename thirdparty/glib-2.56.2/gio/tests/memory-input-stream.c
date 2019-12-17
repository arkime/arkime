/* GLib testing framework examples and tests
 * Copyright (C) 2007 Imendio AB
 * Authors: Tim Janik
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
test_read_chunks (void)
{
  const char *data1 = "abcdefghijklmnopqrstuvwxyz";
  const char *data2 = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  const char *result = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
  char buffer[128];
  gsize bytes_read, pos, len, chunk_size;
  GError *error = NULL;
  GInputStream *stream;
  gboolean res;

  stream = g_memory_input_stream_new ();

  g_memory_input_stream_add_data (G_MEMORY_INPUT_STREAM (stream),
                                  data1, -1, NULL);  
  g_memory_input_stream_add_data (G_MEMORY_INPUT_STREAM (stream),
                                  data2, -1, NULL);  
  len = strlen (data1) + strlen (data2);

  for (chunk_size = 1; chunk_size < len - 1; chunk_size++)
    {
      pos = 0;
      while (pos < len) 
        {
          bytes_read = g_input_stream_read (stream, buffer, chunk_size, NULL, &error);
          g_assert_no_error (error);
          g_assert_cmpint (bytes_read, ==, MIN (chunk_size, len - pos));
          g_assert (strncmp (buffer, result + pos, bytes_read) == 0);

          pos += bytes_read;
        }
      
      g_assert_cmpint (pos, ==, len);
      res = g_seekable_seek (G_SEEKABLE (stream), 0, G_SEEK_SET, NULL, &error);
      g_assert_cmpint (res, ==, TRUE);
      g_assert_no_error (error);
    }

  g_object_unref (stream);
}

GMainLoop *loop;

static void
async_read_chunk (GObject      *object,
		  GAsyncResult *result,
		  gpointer      user_data)
{
  gsize *bytes_read = user_data;
  GError *error = NULL;

  *bytes_read = g_input_stream_read_finish (G_INPUT_STREAM (object),
					    result, &error);
  g_assert_no_error (error);

  g_main_loop_quit (loop);
}

static void
async_skipped_chunk (GObject      *object,
                     GAsyncResult *result,
                     gpointer      user_data)
{
  gsize *bytes_skipped = user_data;
  GError *error = NULL;

  *bytes_skipped = g_input_stream_skip_finish (G_INPUT_STREAM (object),
                                               result, &error);
  g_assert_no_error (error);

  g_main_loop_quit (loop);
}

static void
test_async (void)
{
  const char *data1 = "abcdefghijklmnopqrstuvwxyz";
  const char *data2 = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  const char *result = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
  char buffer[128];
  gsize bytes_read, bytes_skipped;
  gsize pos, len, chunk_size;
  GError *error = NULL;
  GInputStream *stream;
  gboolean res;

  loop = g_main_loop_new (NULL, FALSE);

  stream = g_memory_input_stream_new ();

  g_memory_input_stream_add_data (G_MEMORY_INPUT_STREAM (stream),
                                  data1, -1, NULL);  
  g_memory_input_stream_add_data (G_MEMORY_INPUT_STREAM (stream),
                                  data2, -1, NULL);  
  len = strlen (data1) + strlen (data2);

  for (chunk_size = 1; chunk_size < len - 1; chunk_size++)
    {
      pos = 0;
      while (pos < len) 
        {
          g_input_stream_read_async (stream, buffer, chunk_size,
				     G_PRIORITY_DEFAULT, NULL,
				     async_read_chunk, &bytes_read);
	  g_main_loop_run (loop);

          g_assert_cmpint (bytes_read, ==, MIN (chunk_size, len - pos));
          g_assert (strncmp (buffer, result + pos, bytes_read) == 0);

          pos += bytes_read;
        }
      
      g_assert_cmpint (pos, ==, len);
      res = g_seekable_seek (G_SEEKABLE (stream), 0, G_SEEK_SET, NULL, &error);
      g_assert_cmpint (res, ==, TRUE);
      g_assert_no_error (error);

      pos = 0;
      while (pos + chunk_size + 1 < len)
        {
          g_input_stream_skip_async (stream, chunk_size,
				     G_PRIORITY_DEFAULT, NULL,
				     async_skipped_chunk, &bytes_skipped);
	  g_main_loop_run (loop);

          g_assert_cmpint (bytes_skipped, ==, MIN (chunk_size, len - pos));

          pos += bytes_skipped;
        }

      g_input_stream_read_async (stream, buffer, len - pos,
                                 G_PRIORITY_DEFAULT, NULL,
                                 async_read_chunk, &bytes_read);
      g_main_loop_run (loop);

      g_assert_cmpint (bytes_read, ==, len - pos);
      g_assert (strncmp (buffer, result + pos, bytes_read) == 0);

      res = g_seekable_seek (G_SEEKABLE (stream), 0, G_SEEK_SET, NULL, &error);
      g_assert_cmpint (res, ==, TRUE);
      g_assert_no_error (error);
    }

  g_object_unref (stream);
  g_main_loop_unref (loop);
}

static void
test_seek (void)
{
  const char *data1 = "abcdefghijklmnopqrstuvwxyz";
  const char *data2 = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  GInputStream *stream;
  GError *error;
  char buffer[10];

  stream = g_memory_input_stream_new ();

  g_memory_input_stream_add_data (G_MEMORY_INPUT_STREAM (stream),
                                  data1, -1, NULL);
  g_memory_input_stream_add_data (G_MEMORY_INPUT_STREAM (stream),
                                  data2, -1, NULL);

  g_assert (G_IS_SEEKABLE (stream));
  g_assert (g_seekable_can_seek (G_SEEKABLE (stream)));

  error = NULL;
  g_assert (g_seekable_seek (G_SEEKABLE (stream), 26, G_SEEK_SET, NULL, &error));
  g_assert_no_error (error);
  g_assert_cmpint (g_seekable_tell (G_SEEKABLE (stream)), ==, 26);

  g_assert (g_input_stream_read (stream, buffer, 1, NULL, &error) == 1);
  g_assert_no_error (error);

  g_assert (buffer[0] == 'A');

  g_assert (!g_seekable_seek (G_SEEKABLE (stream), 26, G_SEEK_CUR, NULL, &error));
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);
  g_error_free (error);

  g_object_unref (stream);
}

static void
test_truncate (void)
{
  const char *data1 = "abcdefghijklmnopqrstuvwxyz";
  const char *data2 = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  GInputStream *stream;
  GError *error;

  stream = g_memory_input_stream_new ();

  g_memory_input_stream_add_data (G_MEMORY_INPUT_STREAM (stream),
                                  data1, -1, NULL);
  g_memory_input_stream_add_data (G_MEMORY_INPUT_STREAM (stream),
                                  data2, -1, NULL);

  g_assert (G_IS_SEEKABLE (stream));
  g_assert (!g_seekable_can_truncate (G_SEEKABLE (stream)));

  error = NULL;
  g_assert (!g_seekable_truncate (G_SEEKABLE (stream), 26, NULL, &error));
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED);
  g_error_free (error);

  g_object_unref (stream);
}

static void
test_read_bytes (void)
{
  const char *data1 = "abcdefghijklmnopqrstuvwxyz";
  const char *data2 = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  GInputStream *stream;
  GError *error = NULL;
  GBytes *bytes;
  gsize size;
  gconstpointer data;

  stream = g_memory_input_stream_new ();
  g_memory_input_stream_add_data (G_MEMORY_INPUT_STREAM (stream),
                                  data1, -1, NULL);
  g_memory_input_stream_add_data (G_MEMORY_INPUT_STREAM (stream),
                                  data2, -1, NULL);

  bytes = g_input_stream_read_bytes (stream, 26, NULL, &error);
  g_assert_no_error (error);

  data = g_bytes_get_data (bytes, &size);
  g_assert_cmpint (size, ==, 26);
  g_assert (strncmp (data, data1, 26) == 0);

  g_bytes_unref (bytes);
  g_object_unref (stream);
}

static void
test_from_bytes (void)
{
  gchar data[4096], buffer[4096];
  GBytes *bytes;
  GError *error = NULL;
  GInputStream *stream;
  gint i;

  for (i = 0; i < 4096; i++)
    data[i] = 1 + i % 255;

  bytes = g_bytes_new_static (data, 4096);
  stream = g_memory_input_stream_new_from_bytes (bytes);
  g_assert (g_input_stream_read (stream, buffer, 2048, NULL, &error) == 2048);
  g_assert_no_error (error);
  g_assert (strncmp (data, buffer, 2048) == 0);

  g_object_unref (stream);
  g_bytes_unref (bytes);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/memory-input-stream/read-chunks", test_read_chunks);
  g_test_add_func ("/memory-input-stream/async", test_async);
  g_test_add_func ("/memory-input-stream/seek", test_seek);
  g_test_add_func ("/memory-input-stream/truncate", test_truncate);
  g_test_add_func ("/memory-input-stream/read-bytes", test_read_bytes);
  g_test_add_func ("/memory-input-stream/from-bytes", test_from_bytes);

  return g_test_run();
}
