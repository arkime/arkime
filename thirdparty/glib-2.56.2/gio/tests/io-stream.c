/* GLib testing framework examples and tests
 * Copyright (C) 2010 Collabora Ltd.
 * Authors: Xavier Claessens <xclaesse@gmail.com>
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

typedef struct
{
  GMainLoop *main_loop;
  const gchar *data1;
  const gchar *data2;
  GIOStream *iostream1;
  GIOStream *iostream2;
} TestCopyChunksData;

static void
test_copy_chunks_splice_cb (GObject *source_object,
    GAsyncResult *res,
    gpointer user_data)
{
  TestCopyChunksData *data = user_data;
  GMemoryOutputStream *ostream;
  gchar *received_data;
  GError *error = NULL;

  g_io_stream_splice_finish (res, &error);
  g_assert_no_error (error);

  ostream = G_MEMORY_OUTPUT_STREAM (g_io_stream_get_output_stream (data->iostream1));
  received_data = g_memory_output_stream_get_data (ostream);
  g_assert_cmpstr (received_data, ==, data->data2);

  ostream = G_MEMORY_OUTPUT_STREAM (g_io_stream_get_output_stream (data->iostream2));
  received_data = g_memory_output_stream_get_data (ostream);
  g_assert_cmpstr (received_data, ==, data->data1);

  g_assert (g_io_stream_is_closed (data->iostream1));
  g_assert (g_io_stream_is_closed (data->iostream2));

  g_main_loop_quit (data->main_loop);
}

static void
test_copy_chunks (void)
{
  TestCopyChunksData data;
  GInputStream *istream;
  GOutputStream *ostream;

  data.main_loop = g_main_loop_new (NULL, FALSE);
  data.data1 = "abcdefghijklmnopqrstuvwxyz";
  data.data2 = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

  istream = g_memory_input_stream_new_from_data (data.data1, -1, NULL);
  ostream = g_memory_output_stream_new (NULL, 0, g_realloc, g_free);
  data.iostream1 = g_simple_io_stream_new (istream, ostream);
  g_object_unref (istream);
  g_object_unref (ostream);

  istream = g_memory_input_stream_new_from_data (data.data2, -1, NULL);
  ostream = g_memory_output_stream_new (NULL, 0, g_realloc, g_free);
  data.iostream2 = g_simple_io_stream_new (istream, ostream);
  g_object_unref (istream);
  g_object_unref (ostream);

  g_io_stream_splice_async (data.iostream1, data.iostream2,
      G_IO_STREAM_SPLICE_CLOSE_STREAM1 | G_IO_STREAM_SPLICE_CLOSE_STREAM2 |
      G_IO_STREAM_SPLICE_WAIT_FOR_BOTH, G_PRIORITY_DEFAULT,
      NULL, test_copy_chunks_splice_cb, &data);

  /* We do not hold a ref in data struct, this is to make sure the operation
   * keeps the iostream objects alive until it finishes */
  g_object_unref (data.iostream1);
  g_object_unref (data.iostream2);

  g_main_loop_run (data.main_loop);
  g_main_loop_unref (data.main_loop);
}

static void
close_async_done (GObject *source,
                  GAsyncResult *result,
                  gpointer user_data)
{
  gboolean *done = user_data;

  *done = TRUE;
}

static void
test_close_file (void)
{
#ifdef G_OS_UNIX
  GFileIOStream *fios;
  gboolean done;
  GIOStream *io;
  GFile *file;

  file = g_file_new_for_path ("/dev/null");
  fios = g_file_open_readwrite (file, NULL, NULL);
  g_object_unref (file);
  g_assert (fios);

  io = g_simple_io_stream_new (g_io_stream_get_input_stream (G_IO_STREAM (fios)),
                               g_io_stream_get_output_stream (G_IO_STREAM (fios)));
  g_object_unref (fios);

  g_io_stream_close_async (io, 0, NULL, close_async_done, &done);
  g_object_unref (io);

  done = FALSE;
  while (!done)
    g_main_context_iteration (NULL, TRUE);
#endif
}

static void
test_close_memory (void)
{
  GInputStream *in;
  GOutputStream *out;
  gboolean done;
  GIOStream *io;

  in = g_memory_input_stream_new ();
  out = g_memory_output_stream_new_resizable ();
  io = g_simple_io_stream_new (in, out);
  g_object_unref (out);
  g_object_unref (in);

  g_io_stream_close_async (io, 0, NULL, close_async_done, &done);
  g_object_unref (io);

  done = FALSE;
  while (!done)
    g_main_context_iteration (NULL, TRUE);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/io-stream/copy-chunks", test_copy_chunks);
  g_test_add_func ("/io-stream/close/async/memory", test_close_memory);
  g_test_add_func ("/io-stream/close/async/file", test_close_file);

  return g_test_run();
}
