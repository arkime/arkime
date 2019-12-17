/* GLib testing framework examples and tests
 *
 * Copyright (C) 2010 Red Hat, Inc.
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
 */

#include <gio/gio.h>
#include <glib/gstdio.h>

#ifdef G_OS_UNIX
#include <fcntl.h>
#include <gio/gunixinputstream.h>
#include <gio/gunixoutputstream.h>
#endif

GMainLoop *loop;
GPollableInputStream *in;
GOutputStream *out;

static gboolean
poll_source_callback (GPollableInputStream *in,
		      gpointer              user_data)
{
  GError *error = NULL;
  char buf[2];
  gssize nread;
  gboolean *success = user_data;

  nread = g_pollable_input_stream_read_nonblocking (in, buf, 2, NULL, &error);
  g_assert_no_error (error);
  g_assert_cmpint (nread, ==, 2);
  g_assert_cmpstr (buf, ==, "x");

  *success = TRUE;
  return G_SOURCE_REMOVE;
}

static gboolean
check_source_readability_callback (gpointer user_data)
{
  gboolean expected = GPOINTER_TO_INT (user_data);
  gboolean readable;

  readable = g_pollable_input_stream_is_readable (in);
  g_assert_cmpint (readable, ==, expected);
  return G_SOURCE_REMOVE;
}

static gboolean
write_callback (gpointer user_data)
{
  char *buf = "x";
  gssize nwrote;
  GError *error = NULL;

  nwrote = g_output_stream_write (out, buf, 2, NULL, &error);
  g_assert_no_error (error);
  g_assert_cmpint (nwrote, ==, 2);
/* Give the pipe a few ticks to propagate the write for sockets. On my
 * iMac i7, 40 works, 30 doesn't. */
  g_usleep (80L);

  check_source_readability_callback (GINT_TO_POINTER (TRUE));

  return G_SOURCE_REMOVE;
}

static gboolean
check_source_and_quit_callback (gpointer user_data)
{
  check_source_readability_callback (user_data);
  g_main_loop_quit (loop);
  return G_SOURCE_REMOVE;
}

static void
test_streams (void)
{
  gboolean readable;
  GError *error = NULL;
  char buf[1];
  gssize nread;
  GSource *poll_source;
  gboolean success = FALSE;

  g_assert (g_pollable_input_stream_can_poll (in));
  g_assert (g_pollable_output_stream_can_poll (G_POLLABLE_OUTPUT_STREAM (out)));

  readable = g_pollable_input_stream_is_readable (in);
  g_assert (!readable);

  nread = g_pollable_input_stream_read_nonblocking (in, buf, 1, NULL, &error);
  g_assert_cmpint (nread, ==, -1);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_WOULD_BLOCK);
  g_clear_error (&error);

  /* Create 4 sources, in decreasing order of priority:
   *   1. poll source on @in
   *   2. idle source that checks if @in is readable once
   *      (it won't be) and then removes itself
   *   3. idle source that writes a byte to @out, checks that
   *      @in is now readable, and removes itself
   *   4. idle source that checks if @in is readable once
   *      (it won't be, since the poll source will fire before
   *      this one does) and then quits the loop.
   *
   * If the poll source triggers before it should, then it will get a
   * %G_IO_ERROR_WOULD_BLOCK, and if check() fails in either
   * direction, we will catch it at some point.
   */

  poll_source = g_pollable_input_stream_create_source (in, NULL);
  g_source_set_priority (poll_source, 1);
  g_source_set_callback (poll_source, (GSourceFunc) poll_source_callback, &success, NULL);
  g_source_attach (poll_source, NULL);
  g_source_unref (poll_source);

  g_idle_add_full (2, check_source_readability_callback, GINT_TO_POINTER (FALSE), NULL);
  g_idle_add_full (3, write_callback, NULL, NULL);
  g_idle_add_full (4, check_source_and_quit_callback, GINT_TO_POINTER (FALSE), NULL);

  loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (loop);
  g_main_loop_unref (loop);

  g_assert_cmpint (success, ==, TRUE);
}

#ifdef G_OS_UNIX
static void
test_pollable_unix (void)
{
  int pipefds[2], status, fd;

  status = pipe (pipefds);
  g_assert_cmpint (status, ==, 0);

  in = G_POLLABLE_INPUT_STREAM (g_unix_input_stream_new (pipefds[0], TRUE));
  out = g_unix_output_stream_new (pipefds[1], TRUE);

  test_streams ();

  g_object_unref (in);
  g_object_unref (out);

  /* Non-pipe/socket unix streams are not pollable */
  fd = g_open ("/dev/null", O_RDWR, 0);
  g_assert_cmpint (fd, !=, -1);
  in = G_POLLABLE_INPUT_STREAM (g_unix_input_stream_new (fd, FALSE));
  out = g_unix_output_stream_new (fd, FALSE);

  g_assert (!g_pollable_input_stream_can_poll (in));
  g_assert (!g_pollable_output_stream_can_poll (G_POLLABLE_OUTPUT_STREAM (out)));

  g_object_unref (in);
  g_object_unref (out);
  close (fd);
}

static void
test_pollable_converter (void)
{
  GConverter *converter;
  GError *error = NULL;
  GInputStream *ibase;
  int pipefds[2], status;

  status = pipe (pipefds);
  g_assert_cmpint (status, ==, 0);

  ibase = G_INPUT_STREAM (g_unix_input_stream_new (pipefds[0], TRUE));
  converter = G_CONVERTER (g_charset_converter_new ("UTF-8", "UTF-8", &error));
  g_assert_no_error (error);

  in = G_POLLABLE_INPUT_STREAM (g_converter_input_stream_new (ibase, converter));
  g_object_unref (converter);
  g_object_unref (ibase);

  out = g_unix_output_stream_new (pipefds[1], TRUE);

  test_streams ();

  g_object_unref (in);
  g_object_unref (out);
}

#endif

static void
client_connected (GObject      *source,
		  GAsyncResult *result,
		  gpointer      user_data)
{
  GSocketClient *client = G_SOCKET_CLIENT (source);
  GSocketConnection **conn = user_data;
  GError *error = NULL;

  *conn = g_socket_client_connect_finish (client, result, &error);
  g_assert_no_error (error);
}

static void
server_connected (GObject      *source,
		  GAsyncResult *result,
		  gpointer      user_data)
{
  GSocketListener *listener = G_SOCKET_LISTENER (source);
  GSocketConnection **conn = user_data;
  GError *error = NULL;

  *conn = g_socket_listener_accept_finish (listener, result, NULL, &error);
  g_assert_no_error (error);
}

static void
test_pollable_socket (void)
{
  GInetAddress *iaddr;
  GSocketAddress *saddr, *effective_address;
  GSocketListener *listener;
  GSocketClient *client;
  GError *error = NULL;
  GSocketConnection *client_conn = NULL, *server_conn = NULL;

  iaddr = g_inet_address_new_loopback (G_SOCKET_FAMILY_IPV4);
  saddr = g_inet_socket_address_new (iaddr, 0);
  g_object_unref (iaddr);

  listener = g_socket_listener_new ();
  g_socket_listener_add_address (listener, saddr,
				 G_SOCKET_TYPE_STREAM,
				 G_SOCKET_PROTOCOL_TCP,
				 NULL,
				 &effective_address,
				 &error);
  g_assert_no_error (error);
  g_object_unref (saddr);

  client = g_socket_client_new ();

  g_socket_client_connect_async (client,
				 G_SOCKET_CONNECTABLE (effective_address),
				 NULL, client_connected, &client_conn);
  g_socket_listener_accept_async (listener, NULL,
				  server_connected, &server_conn);

  while (!client_conn || !server_conn)
    g_main_context_iteration (NULL, TRUE);

  in = G_POLLABLE_INPUT_STREAM (g_io_stream_get_input_stream (G_IO_STREAM (client_conn)));
  out = g_io_stream_get_output_stream (G_IO_STREAM (server_conn));

  test_streams ();

  g_object_unref (client_conn);
  g_object_unref (server_conn);
  g_object_unref (client);
  g_object_unref (listener);
  g_object_unref (effective_address);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

#ifdef G_OS_UNIX
  g_test_add_func ("/pollable/unix", test_pollable_unix);
  g_test_add_func ("/pollable/converter", test_pollable_converter);
#endif
  g_test_add_func ("/pollable/socket", test_pollable_socket);

  return g_test_run();
}

