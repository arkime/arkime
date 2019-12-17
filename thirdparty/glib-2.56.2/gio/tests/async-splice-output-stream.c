/* GLib testing framework examples and tests
 * Copyright (C) 2010-2012 Collabora Ltd.
 * Authors: Xavier Claessens <xclaesse@gmail.com>
 *          Mike Ruprecht <mike.ruprecht@collabora.co.uk>
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
#include <glib/gstdio.h>
#include <gio/gio.h>
#include <stdlib.h>
#include <string.h>

typedef enum
{
  TEST_THREADED_NONE    = 0,
  TEST_THREADED_ISTREAM = 1,
  TEST_THREADED_OSTREAM = 2,
  TEST_THREADED_BOTH    = TEST_THREADED_ISTREAM | TEST_THREADED_OSTREAM,
} TestThreadedFlags;

typedef struct
{
  GMainLoop *main_loop;
  const gchar *data;
  GInputStream *istream;
  GOutputStream *ostream;
  TestThreadedFlags flags;
  gchar *input_path;
  gchar *output_path;
} TestCopyChunksData;

static void
test_copy_chunks_splice_cb (GObject      *source,
                            GAsyncResult *res,
                            gpointer      user_data)
{
  TestCopyChunksData *data = user_data;
  gchar *received_data;
  GError *error = NULL;
  gssize bytes_spliced;

  bytes_spliced = g_output_stream_splice_finish (G_OUTPUT_STREAM (source),
                                                 res, &error);
  g_assert_no_error (error);
  g_assert_cmpint (bytes_spliced, ==, strlen (data->data));

  if (data->flags & TEST_THREADED_OSTREAM)
    {
      gsize length = 0;

      g_file_get_contents (data->output_path, &received_data,
                           &length, &error);
      g_assert_no_error (error);
      g_assert_cmpstr (received_data, ==, data->data);
      g_free (received_data);
    }
  else
    {
      received_data = g_memory_output_stream_get_data (G_MEMORY_OUTPUT_STREAM (data->ostream));
      g_assert_cmpstr (received_data, ==, data->data);
    }

  g_assert (g_input_stream_is_closed (data->istream));
  g_assert (g_output_stream_is_closed (data->ostream));

  if (data->flags & TEST_THREADED_ISTREAM)
    {
      g_unlink (data->input_path);
      g_free (data->input_path);
    }

  if (data->flags & TEST_THREADED_OSTREAM)
    {
      g_unlink (data->output_path);
      g_free (data->output_path);
    }

  g_main_loop_quit (data->main_loop);
}

static void
test_copy_chunks_start (TestThreadedFlags flags)
{
  TestCopyChunksData data;
  GError *error = NULL;

  data.main_loop = g_main_loop_new (NULL, FALSE);
  data.data = "abcdefghijklmnopqrstuvwxyz";
  data.flags = flags;

  if (data.flags & TEST_THREADED_ISTREAM)
    {
      GFile *file;
      GFileIOStream *stream;

      file = g_file_new_tmp ("test-inputXXXXXX", &stream, &error);
      g_assert_no_error (error);
      g_object_unref (stream);
      data.input_path = g_file_get_path (file);
      g_file_set_contents (data.input_path,
                           data.data, strlen (data.data),
                           &error);
      g_assert_no_error (error);
      data.istream = G_INPUT_STREAM (g_file_read (file, NULL, &error));
      g_assert_no_error (error);
      g_object_unref (file);
    }
  else
    {
      data.istream = g_memory_input_stream_new_from_data (data.data, -1, NULL);
    }

  if (data.flags & TEST_THREADED_OSTREAM)
    {
      GFile *file;
      GFileIOStream *stream;

      file = g_file_new_tmp ("test-outputXXXXXX", &stream, &error);
      g_assert_no_error (error);
      g_object_unref (stream);
      data.output_path = g_file_get_path (file);
      data.ostream = G_OUTPUT_STREAM (g_file_replace (file, NULL, FALSE,
                                                      G_FILE_CREATE_NONE,
                                                      NULL, &error));
      g_assert_no_error (error);
      g_object_unref (file);
    }
  else
    {
      data.ostream = g_memory_output_stream_new (NULL, 0, g_realloc, g_free);
    }

  g_output_stream_splice_async (data.ostream, data.istream,
                                G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE |
                                G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET,
                                G_PRIORITY_DEFAULT, NULL,
                                test_copy_chunks_splice_cb, &data);

  /* We do not hold a ref in data struct, this is to make sure the operation
   * keeps the iostream objects alive until it finishes
   */
  g_object_unref (data.istream);
  g_object_unref (data.ostream);

  g_main_loop_run (data.main_loop);
  g_main_loop_unref (data.main_loop);
}

static void
test_copy_chunks (void)
{
  test_copy_chunks_start (TEST_THREADED_NONE);
}

static void
test_copy_chunks_threaded_input (void)
{
  test_copy_chunks_start (TEST_THREADED_ISTREAM);
}

static void
test_copy_chunks_threaded_output (void)
{
  test_copy_chunks_start (TEST_THREADED_OSTREAM);
}

static void
test_copy_chunks_threaded (void)
{
  test_copy_chunks_start (TEST_THREADED_BOTH);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/async-splice/copy-chunks", test_copy_chunks);
  g_test_add_func ("/async-splice/copy-chunks-threaded-input",
                   test_copy_chunks_threaded_input);
  g_test_add_func ("/async-splice/copy-chunks-threaded-output",
                   test_copy_chunks_threaded_output);
  g_test_add_func ("/async-splice/copy-chunks-threaded",
                   test_copy_chunks_threaded);

  return g_test_run();
}
