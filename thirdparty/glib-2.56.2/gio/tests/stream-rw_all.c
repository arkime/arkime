/*
 * Copyright © 2014 Canonical Limited
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
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Ryan Lortie <desrt@desrt.ca>
 */

#include <gio/gio.h>
#include <string.h>

static gboolean expected_read_success;
static guint    expected_read;
static gboolean got_read_done;

static void
read_done (GObject      *source,
           GAsyncResult *result,
           gpointer      user_data)
{
  gboolean success;
  gsize read;

  success = g_input_stream_read_all_finish (G_INPUT_STREAM (source), result, &read, NULL);
  g_assert_cmpint (expected_read_success, ==, success);
  g_assert_cmpint (expected_read, ==, read);
  got_read_done = TRUE;
}

static void
wait_for_read (gboolean success,
               gsize    read)
{
  g_assert (!got_read_done);
  expected_read_success = success;
  expected_read = read;

  while (!got_read_done)
    g_main_context_iteration (NULL, TRUE);

  got_read_done = FALSE;
}

static gboolean expected_write_success;
static guint    expected_written;
static gboolean got_write_done;

static void
write_done (GObject      *source,
            GAsyncResult *result,
            gpointer      user_data)
{
  gboolean success;
  gsize written;

  success = g_output_stream_write_all_finish (G_OUTPUT_STREAM (source), result, &written, NULL);
  g_assert_cmpint (expected_write_success, ==, success);
  g_assert_cmpint (expected_written, ==, written);
  got_write_done = TRUE;
}

static void
wait_for_write (gboolean success,
                gsize    written)
{
  g_assert (!got_write_done);
  expected_write_success = success;
  expected_written = written;

  while (!got_write_done)
    g_main_context_iteration (NULL, TRUE);

  got_write_done = FALSE;
}

static void
test_write_all_async_memory (void)
{
  GOutputStream *ms;
  gchar b[24];

  ms = g_memory_output_stream_new (b, sizeof b, NULL, NULL);

  g_output_stream_write_all_async (ms, "0123456789", 10, 0, NULL, write_done, NULL);
  wait_for_write (TRUE, 10);

  g_output_stream_write_all_async (ms, "0123456789", 10, 0, NULL, write_done, NULL);
  wait_for_write (TRUE, 10);

  /* this will trigger an out-of-space error, but we will see the
   * partial write...
   */
  g_output_stream_write_all_async (ms, "0123456789", 10, 0, NULL, write_done, NULL);
  wait_for_write (FALSE, 4);

  /* and still an error, but no further bytes written */
  g_output_stream_write_all_async (ms, "0123456789", 10, 0, NULL, write_done, NULL);
  wait_for_write (FALSE, 0);

  g_assert (!memcmp (b, "012345678901234567890123", 24));

  g_object_unref (ms);
}

static void
test_read_all_async_memory (void)
{
  GInputStream *ms;
  gchar b[24] = "0123456789ABCDEFGHIJ!@#$";
  gchar buf[10];

  ms = g_memory_input_stream_new_from_data (b, sizeof b, NULL);

  g_input_stream_read_all_async (ms, buf, 10, 0, NULL, read_done, NULL);
  wait_for_read (TRUE, 10);
  g_assert (!memcmp (buf, "0123456789", 10));

  g_input_stream_read_all_async (ms, buf, 10, 0, NULL, read_done, NULL);
  wait_for_read (TRUE, 10);
  g_assert (!memcmp (buf, "ABCDEFGHIJ", 10));

  /* partial read... */
  g_input_stream_read_all_async (ms, buf, 10, 0, NULL, read_done, NULL);
  wait_for_read (TRUE, 4);
  g_assert (!memcmp (buf, "!@#$", 4));

  /* EOF */
  g_input_stream_read_all_async (ms, buf, 10, 0, NULL, read_done, NULL);
  wait_for_read (TRUE, 0);

  g_object_unref (ms);
}

#ifdef G_OS_UNIX
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <gio/gunixinputstream.h>
#include <gio/gunixoutputstream.h>

static void
test_read_write_all_async_pipe (void)
{
  GCancellable *cancellable;
  GError *error = NULL;
  GOutputStream *out;
  GInputStream *in;
  gsize in_flight;
  gsize s;
  gchar wbuf[100] = { 0, };
  gchar rbuf[100];

  {
    gint sv[2];
    gint s;

    s = socketpair (AF_UNIX, SOCK_STREAM, 0, sv);
    g_assert (s == 0);

    out = g_unix_output_stream_new (sv[0], TRUE);
    in = g_unix_input_stream_new (sv[1], TRUE);
  }

  /* Try to fill up the buffer */
  in_flight = 0;
  while (g_pollable_output_stream_is_writable (G_POLLABLE_OUTPUT_STREAM (out)))
    {
      s = g_output_stream_write (out, wbuf, sizeof wbuf, NULL, &error);
      g_assert_no_error (error);
      g_assert (s > 0);
      in_flight += s;
    }

  /* Now start a blocking write_all; nothing should happen. */
  cancellable = g_cancellable_new ();
  g_output_stream_write_all_async (out, "0123456789", 10, 0, cancellable, write_done, NULL);
  while (g_main_context_iteration (NULL, FALSE))
    ;
  g_assert (!got_write_done);

  /* Cancel that to make sure it works */
  g_cancellable_cancel (cancellable);
  g_object_unref (cancellable);
  wait_for_write (FALSE, 0);

  /* Start it again */
  g_output_stream_write_all_async (out, "0123456789", 10, 0, NULL, write_done, NULL);
  while (g_main_context_iteration (NULL, FALSE))
    ;
  g_assert (!got_write_done);

  /* Now drain as much as we originally put in the buffer to make it
   * block -- this will unblock the writer.
   */
  while (in_flight)
    {
      s = g_input_stream_read (in, rbuf, MIN (sizeof wbuf, in_flight), NULL, &error);
      g_assert_no_error (error);
      g_assert (s > 0);
      in_flight -= s;
    }

  /* That will have caused some writing to start happening.  Do a
   * read_all as well, for more bytes than was written.
   */
  g_input_stream_read_all_async (in, rbuf, sizeof rbuf, 0, NULL, read_done, NULL);

  /* The write is surely finished by now */
  wait_for_write (TRUE, 10);
  /* ...but the read will not yet be satisfied */
  g_assert (!got_read_done);

  /* Feed the read more than it asked for; this really should not block
   * since the buffer is so small...
   */
  g_output_stream_write_all (out, wbuf, sizeof wbuf, 0, NULL, &error);
  g_assert_no_error (error);

  /* Read will have finished now */
  wait_for_read (TRUE, sizeof rbuf);

  /* Close the writer end to make an EOF condition */
  g_output_stream_close (out, NULL, NULL);

  /* ... and we should have exactly 10 extra bytes left in the buffer */
  g_input_stream_read_all_async (in, rbuf, sizeof rbuf, 0, NULL, read_done, NULL);
  wait_for_read (TRUE, 10);

  g_object_unref (out);
  g_object_unref (in);
}
#endif

int
main (int    argc,
      char **argv)
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/stream/read_all_async/memory", test_read_all_async_memory);
  g_test_add_func ("/stream/write_all_async/memory", test_write_all_async_memory);
#ifdef G_OS_UNIX
  g_test_add_func ("/stream/read_write_all_async/pipe", test_read_write_all_async_pipe);
#endif

  return g_test_run();
}
