/* GLib testing framework examples and tests
 * Copyright (C) 2008 Red Hat, Inc
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

#include <gio/gio.h>
#include <gio/gunixinputstream.h>
#include <gio/gunixoutputstream.h>
#include <glib/glib-unix.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define DATA "abcdefghijklmnopqrstuvwxyz"

int writer_pipe[2], reader_pipe[2];
GCancellable *writer_cancel, *reader_cancel, *main_cancel;
GMainLoop *loop;


static gpointer
writer_thread (gpointer user_data)
{
  GOutputStream *out;
  gssize nwrote, offset;
  GError *err = NULL;

  out = g_unix_output_stream_new (writer_pipe[1], TRUE);

  do
    {
      g_usleep (10);

      offset = 0;
      while (offset < (gssize) sizeof (DATA))
	{
	  nwrote = g_output_stream_write (out, DATA + offset,
					  sizeof (DATA) - offset,
					  writer_cancel, &err);
	  if (nwrote <= 0 || err != NULL)
	    break;
	  offset += nwrote;
	}

      g_assert (nwrote > 0 || err != NULL);
    }
  while (err == NULL);

  if (g_cancellable_is_cancelled (writer_cancel))
    {
      g_clear_error (&err);
      g_cancellable_cancel (main_cancel);
      g_object_unref (out);
      return NULL;
    }

  g_warning ("writer: %s", err->message);
  g_assert_not_reached ();
}

static gpointer
reader_thread (gpointer user_data)
{
  GInputStream *in;
  gssize nread = 0, total;
  GError *err = NULL;
  char buf[sizeof (DATA)];

  in = g_unix_input_stream_new (reader_pipe[0], TRUE);

  do
    {
      total = 0;
      while (total < (gssize) sizeof (DATA))
	{
	  nread = g_input_stream_read (in, buf + total, sizeof (buf) - total,
				       reader_cancel, &err);
	  if (nread <= 0 || err != NULL)
	    break;
	  total += nread;
	}

      if (err)
	break;

      if (nread == 0)
	{
	  g_assert (err == NULL);
	  /* pipe closed */
	  g_object_unref (in);
	  return NULL;
	}

      g_assert_cmpstr (buf, ==, DATA);
      g_assert (!g_cancellable_is_cancelled (reader_cancel));
    }
  while (err == NULL);

  g_warning ("reader: %s", err->message);
  g_assert_not_reached ();
}

char main_buf[sizeof (DATA)];
gssize main_len, main_offset;

static void main_thread_read (GObject *source, GAsyncResult *res, gpointer user_data);
static void main_thread_skipped (GObject *source, GAsyncResult *res, gpointer user_data);
static void main_thread_wrote (GObject *source, GAsyncResult *res, gpointer user_data);

static void
do_main_cancel (GOutputStream *out)
{
  g_output_stream_close (out, NULL, NULL);
  g_main_loop_quit (loop);
}

static void
main_thread_skipped (GObject *source, GAsyncResult *res, gpointer user_data)
{
  GInputStream *in = G_INPUT_STREAM (source);
  GOutputStream *out = user_data;
  GError *err = NULL;
  gssize nskipped;

  nskipped = g_input_stream_skip_finish (in, res, &err);

  if (g_cancellable_is_cancelled (main_cancel))
    {
      do_main_cancel (out);
      return;
    }

  g_assert_no_error (err);

  main_offset += nskipped;
  if (main_offset == main_len)
    {
      main_offset = 0;
      g_output_stream_write_async (out, main_buf, main_len,
                                   G_PRIORITY_DEFAULT, main_cancel,
                                   main_thread_wrote, in);
    }
  else
    {
      g_input_stream_skip_async (in, main_len - main_offset,
				 G_PRIORITY_DEFAULT, main_cancel,
				 main_thread_skipped, out);
    }
}

static void
main_thread_read (GObject *source, GAsyncResult *res, gpointer user_data)
{
  GInputStream *in = G_INPUT_STREAM (source);
  GOutputStream *out = user_data;
  GError *err = NULL;
  gssize nread;

  nread = g_input_stream_read_finish (in, res, &err);

  if (g_cancellable_is_cancelled (main_cancel))
    {
      do_main_cancel (out);
      return;
    }

  g_assert_no_error (err);

  main_offset += nread;
  if (main_offset == sizeof (DATA))
    {
      main_len = main_offset;
      main_offset = 0;
      /* Now skip the same amount */
      g_input_stream_skip_async (in, main_len,
				 G_PRIORITY_DEFAULT, main_cancel,
				 main_thread_skipped, out);
    }
  else
    {
      g_input_stream_read_async (in, main_buf, sizeof (main_buf),
				 G_PRIORITY_DEFAULT, main_cancel,
				 main_thread_read, out);
    }
}

static void
main_thread_wrote (GObject *source, GAsyncResult *res, gpointer user_data)
{
  GOutputStream *out = G_OUTPUT_STREAM (source);
  GInputStream *in = user_data;
  GError *err = NULL;
  gssize nwrote;

  nwrote = g_output_stream_write_finish (out, res, &err);

  if (g_cancellable_is_cancelled (main_cancel))
    {
      do_main_cancel (out);
      return;
    }

  g_assert_no_error (err);
  g_assert_cmpint (nwrote, <=, main_len - main_offset);

  main_offset += nwrote;
  if (main_offset == main_len)
    {
      main_offset = 0;
      g_input_stream_read_async (in, main_buf, sizeof (main_buf),
				 G_PRIORITY_DEFAULT, main_cancel,
				 main_thread_read, out);
    }
  else
    {
      g_output_stream_write_async (out, main_buf + main_offset,
				   main_len - main_offset,
				   G_PRIORITY_DEFAULT, main_cancel,
				   main_thread_wrote, in);
    }
}

static gboolean
timeout (gpointer cancellable)
{
  g_cancellable_cancel (cancellable);
  return FALSE;
}

static void
test_pipe_io (gconstpointer nonblocking)
{
  GThread *writer, *reader;
  GInputStream *in;
  GOutputStream *out;

  /* Split off two (additional) threads, a reader and a writer. From
   * the writer thread, write data synchronously in small chunks,
   * which gets alternately read and skipped asynchronously by the
   * main thread and then (if not skipped) written asynchronously to
   * the reader thread, which reads it synchronously. Eventually a
   * timeout in the main thread will cause it to cancel the writer
   * thread, which will in turn cancel the read op in the main thread,
   * which will then close the pipe to the reader thread, causing the
   * read op to fail.
   */

  g_assert (pipe (writer_pipe) == 0 && pipe (reader_pipe) == 0);

  if (nonblocking)
    {
      GError *error = NULL;

      g_unix_set_fd_nonblocking (writer_pipe[0], TRUE, &error);
      g_assert_no_error (error);
      g_unix_set_fd_nonblocking (writer_pipe[1], TRUE, &error);
      g_assert_no_error (error);
      g_unix_set_fd_nonblocking (reader_pipe[0], TRUE, &error);
      g_assert_no_error (error);
      g_unix_set_fd_nonblocking (reader_pipe[1], TRUE, &error);
      g_assert_no_error (error);
    }

  writer_cancel = g_cancellable_new ();
  reader_cancel = g_cancellable_new ();
  main_cancel = g_cancellable_new ();

  writer = g_thread_new ("writer", writer_thread, NULL);
  reader = g_thread_new ("reader", reader_thread, NULL);

  in = g_unix_input_stream_new (writer_pipe[0], TRUE);
  out = g_unix_output_stream_new (reader_pipe[1], TRUE);

  g_input_stream_read_async (in, main_buf, sizeof (main_buf),
			     G_PRIORITY_DEFAULT, main_cancel,
			     main_thread_read, out);

  g_timeout_add (500, timeout, writer_cancel);

  loop = g_main_loop_new (NULL, TRUE);
  g_main_loop_run (loop);
  g_main_loop_unref (loop);

  g_thread_join (reader);
  g_thread_join (writer);

  g_object_unref (main_cancel);
  g_object_unref (reader_cancel);
  g_object_unref (writer_cancel);
  g_object_unref (in);
  g_object_unref (out);
}

static void
test_basic (void)
{
  GUnixInputStream *is;
  GUnixOutputStream *os;
  gint fd;
  gboolean close_fd;

  is = G_UNIX_INPUT_STREAM (g_unix_input_stream_new (0, TRUE));
  g_object_get (is,
                "fd", &fd,
                "close-fd", &close_fd,
                NULL);
  g_assert_cmpint (fd, ==, 0);
  g_assert (close_fd);

  g_unix_input_stream_set_close_fd (is, FALSE);
  g_assert (!g_unix_input_stream_get_close_fd (is));
  g_assert_cmpint (g_unix_input_stream_get_fd (is), ==, 0);

  g_assert (!g_input_stream_has_pending (G_INPUT_STREAM (is)));

  g_object_unref (is);

  os = G_UNIX_OUTPUT_STREAM (g_unix_output_stream_new (1, TRUE));
  g_object_get (os,
                "fd", &fd,
                "close-fd", &close_fd,
                NULL);
  g_assert_cmpint (fd, ==, 1);
  g_assert (close_fd);

  g_unix_output_stream_set_close_fd (os, FALSE);
  g_assert (!g_unix_output_stream_get_close_fd (os));
  g_assert_cmpint (g_unix_output_stream_get_fd (os), ==, 1);

  g_assert (!g_output_stream_has_pending (G_OUTPUT_STREAM (os)));

  g_object_unref (os);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/unix-streams/basic", test_basic);
  g_test_add_data_func ("/unix-streams/pipe-io-test",
			GINT_TO_POINTER (FALSE),
			test_pipe_io);
  g_test_add_data_func ("/unix-streams/nonblocking-io-test",
			GINT_TO_POINTER (TRUE),
			test_pipe_io);

  return g_test_run();
}
