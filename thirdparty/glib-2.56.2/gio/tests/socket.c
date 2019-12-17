/* GLib testing framework examples and tests
 *
 * Copyright (C) 2008-2011 Red Hat, Inc.
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

#ifdef G_OS_UNIX
#include <errno.h>
#include <sys/wait.h>
#include <string.h>
#include <stdlib.h>
#include <gio/gnetworking.h>
#include <gio/gunixconnection.h>
#endif

#include "gnetworkingprivate.h"

static gboolean ipv6_supported;

typedef struct {
  GSocket *server;
  GSocket *client;
  GSocketFamily family;
  GThread *thread;
  GMainLoop *loop;
  GCancellable *cancellable; /* to shut down dgram echo server thread */
} IPTestData;

static gpointer
echo_server_dgram_thread (gpointer user_data)
{
  IPTestData *data = user_data;
  GSocketAddress *sa;
  GCancellable *cancellable = data->cancellable;
  GSocket *sock;
  GError *error = NULL;
  gssize nread, nwrote;
  gchar buf[128];

  sock = data->server;

  while (TRUE)
    {
      nread = g_socket_receive_from (sock, &sa, buf, sizeof (buf), cancellable, &error);
      if (error && g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        break;
      g_assert_no_error (error);
      g_assert_cmpint (nread, >=, 0);

      nwrote = g_socket_send_to (sock, sa, buf, nread, cancellable, &error);
      if (error && g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        break;
      g_assert_no_error (error);
      g_assert_cmpint (nwrote, ==, nread);

      g_object_unref (sa);
    }

  g_clear_error (&error);

  return NULL;
}

static gpointer
echo_server_thread (gpointer user_data)
{
  IPTestData *data = user_data;
  GSocket *sock;
  GError *error = NULL;
  gssize nread, nwrote;
  gchar buf[128];

  sock = g_socket_accept (data->server, NULL, &error);
  g_assert_no_error (error);

  while (TRUE)
    {
      nread = g_socket_receive (sock, buf, sizeof (buf), NULL, &error);
      g_assert_no_error (error);
      g_assert_cmpint (nread, >=, 0);

      if (nread == 0)
	break;

      nwrote = g_socket_send (sock, buf, nread, NULL, &error);
      g_assert_no_error (error);
      g_assert_cmpint (nwrote, ==, nread);
    }

  g_socket_close (sock, &error);
  g_assert_no_error (error);
  g_object_unref (sock);
  return NULL;
}

static IPTestData *
create_server_full (GSocketFamily   family,
                    GSocketType     socket_type,
                    GThreadFunc     server_thread,
                    gboolean        v4mapped,
                    GError        **error)
{
  IPTestData *data;
  GSocket *server;
  GSocketAddress *addr;
  GInetAddress *iaddr;

  data = g_slice_new (IPTestData);
  data->family = family;

  data->server = server = g_socket_new (family,
					socket_type,
					G_SOCKET_PROTOCOL_DEFAULT,
					error);
  if (server == NULL)
    goto error;

  g_assert_cmpint (g_socket_get_family (server), ==, family);
  g_assert_cmpint (g_socket_get_socket_type (server), ==, socket_type);
  g_assert_cmpint (g_socket_get_protocol (server), ==, G_SOCKET_PROTOCOL_DEFAULT);

  g_socket_set_blocking (server, TRUE);

#if defined (IPPROTO_IPV6) && defined (IPV6_V6ONLY)
  if (v4mapped)
    {
      g_socket_set_option (data->server, IPPROTO_IPV6, IPV6_V6ONLY, FALSE, NULL);
      if (!g_socket_speaks_ipv4 (data->server))
        goto error;
    }
#endif

  if (v4mapped)
    iaddr = g_inet_address_new_any (family);
  else
    iaddr = g_inet_address_new_loopback (family);
  addr = g_inet_socket_address_new (iaddr, 0);
  g_object_unref (iaddr);

  g_assert_cmpint (g_inet_socket_address_get_port (G_INET_SOCKET_ADDRESS (addr)), ==, 0);
  if (!g_socket_bind (server, addr, TRUE, error))
    {
      g_object_unref (addr);
      goto error;
    }
  g_object_unref (addr);

  addr = g_socket_get_local_address (server, error);
  if (addr == NULL)
    goto error;
  g_assert_cmpint (g_inet_socket_address_get_port (G_INET_SOCKET_ADDRESS (addr)), !=, 0);
  g_object_unref (addr);

  if (socket_type == G_SOCKET_TYPE_STREAM)
    {
      if (!g_socket_listen (server, error))
        goto error;
    }
  else
    {
      data->cancellable = g_cancellable_new ();
    }

  data->thread = g_thread_new ("server", server_thread, data);

  return data;

error:
  g_clear_object (&data->server);
  g_slice_free (IPTestData, data);

  return NULL;
}

static IPTestData *
create_server (GSocketFamily   family,
               GThreadFunc     server_thread,
               gboolean        v4mapped,
               GError        **error)
{
  return create_server_full (family, G_SOCKET_TYPE_STREAM, server_thread, v4mapped, error);
}

static const gchar *testbuf = "0123456789abcdef";

static gboolean
test_ip_async_read_ready (GSocket      *client,
			  GIOCondition  cond,
			  gpointer      user_data)
{
  IPTestData *data = user_data;
  GError *error = NULL;
  gssize len;
  gchar buf[128];

  g_assert_cmpint (cond, ==, G_IO_IN);

  len = g_socket_receive (client, buf, sizeof (buf), NULL, &error);
  g_assert_no_error (error);
  g_assert_cmpint (len, ==, strlen (testbuf) + 1);

  g_assert_cmpstr (testbuf, ==, buf);

  g_main_loop_quit (data->loop);

  return FALSE;
}

static gboolean
test_ip_async_write_ready (GSocket      *client,
			   GIOCondition  cond,
			   gpointer      user_data)
{
  IPTestData *data = user_data;
  GError *error = NULL;
  GSource *source;
  gssize len;

  g_assert_cmpint (cond, ==, G_IO_OUT);

  len = g_socket_send (client, testbuf, strlen (testbuf) + 1, NULL, &error);
  g_assert_no_error (error);
  g_assert_cmpint (len, ==, strlen (testbuf) + 1);

  source = g_socket_create_source (client, G_IO_IN, NULL);
  g_source_set_callback (source, (GSourceFunc)test_ip_async_read_ready,
			 data, NULL);
  g_source_attach (source, NULL);
  g_source_unref (source);

  return FALSE;
}

static gboolean
test_ip_async_timed_out (GSocket      *client,
			 GIOCondition  cond,
			 gpointer      user_data)
{
  IPTestData *data = user_data;
  GError *error = NULL;
  GSource *source;
  gssize len;
  gchar buf[128];

  if (data->family == G_SOCKET_FAMILY_IPV4)
    {
      g_assert_cmpint (cond, ==, G_IO_IN);
      len = g_socket_receive (client, buf, sizeof (buf), NULL, &error);
      g_assert_cmpint (len, ==, -1);
      g_assert_error (error, G_IO_ERROR, G_IO_ERROR_TIMED_OUT);
      g_clear_error (&error);
    }

  source = g_socket_create_source (client, G_IO_OUT, NULL);
  g_source_set_callback (source, (GSourceFunc)test_ip_async_write_ready,
			 data, NULL);
  g_source_attach (source, NULL);
  g_source_unref (source);

  return FALSE;
}

static gboolean
test_ip_async_connected (GSocket      *client,
			 GIOCondition  cond,
			 gpointer      user_data)
{
  IPTestData *data = user_data;
  GError *error = NULL;
  GSource *source;
  gssize len;
  gchar buf[128];

  g_socket_check_connect_result (client, &error);
  g_assert_no_error (error);
  /* We do this after the check_connect_result, since that will give a
   * more useful assertion in case of error.
   */
  g_assert_cmpint (cond, ==, G_IO_OUT);

  g_assert (g_socket_is_connected (client));

  /* This adds 1 second to "make check", so let's just only do it once. */
  if (data->family == G_SOCKET_FAMILY_IPV4)
    {
      len = g_socket_receive (client, buf, sizeof (buf), NULL, &error);
      g_assert_cmpint (len, ==, -1);
      g_assert_error (error, G_IO_ERROR, G_IO_ERROR_WOULD_BLOCK);
      g_clear_error (&error);

      source = g_socket_create_source (client, G_IO_IN, NULL);
      g_source_set_callback (source, (GSourceFunc)test_ip_async_timed_out,
			     data, NULL);
      g_source_attach (source, NULL);
      g_source_unref (source);
    }
  else
    test_ip_async_timed_out (client, 0, data);

  return FALSE;
}

static gboolean
idle_test_ip_async_connected (gpointer user_data)
{
  IPTestData *data = user_data;

  return test_ip_async_connected (data->client, G_IO_OUT, data);
}

static void
test_ip_async (GSocketFamily family)
{
  IPTestData *data;
  GError *error = NULL;
  GSocket *client;
  GSocketAddress *addr;
  GSource *source;
  gssize len;
  gchar buf[128];

  data = create_server (family, echo_server_thread, FALSE, &error);
  if (error != NULL)
    {
      gchar *message = g_strdup_printf ("Failed to create server: %s", error->message);
      g_test_skip (message);
      g_free (message);
      g_clear_error (&error);
      return;
    }

  addr = g_socket_get_local_address (data->server, &error);
  g_assert_no_error (error);

  client = g_socket_new (family,
			 G_SOCKET_TYPE_STREAM,
			 G_SOCKET_PROTOCOL_DEFAULT,
			 &error);
  g_assert_no_error (error);
  data->client = client;

  g_assert_cmpint (g_socket_get_family (client), ==, family);
  g_assert_cmpint (g_socket_get_socket_type (client), ==, G_SOCKET_TYPE_STREAM);
  g_assert_cmpint (g_socket_get_protocol (client), ==, G_SOCKET_PROTOCOL_DEFAULT);

  g_socket_set_blocking (client, FALSE);
  g_socket_set_timeout (client, 1);

  if (g_socket_connect (client, addr, NULL, &error))
    {
      g_assert_no_error (error);
      g_idle_add (idle_test_ip_async_connected, data);
    }
  else
    {
      g_assert_error (error, G_IO_ERROR, G_IO_ERROR_PENDING);
      g_clear_error (&error);
      source = g_socket_create_source (client, G_IO_OUT, NULL);
      g_source_set_callback (source, (GSourceFunc)test_ip_async_connected,
			     data, NULL);
      g_source_attach (source, NULL);
      g_source_unref (source);
    }
  g_object_unref (addr);

  data->loop = g_main_loop_new (NULL, TRUE);
  g_main_loop_run (data->loop);
  g_main_loop_unref (data->loop);

  g_socket_shutdown (client, FALSE, TRUE, &error);
  g_assert_no_error (error);

  g_thread_join (data->thread);

  if (family == G_SOCKET_FAMILY_IPV4)
    {
      /* Test that reading on a remote-closed socket gets back 0 bytes. */
      len = g_socket_receive_with_blocking (client, buf, sizeof (buf),
					    TRUE, NULL, &error);
      g_assert_no_error (error);
      g_assert_cmpint (len, ==, 0);
    }
  else
    {
      /* Test that writing to a remote-closed socket gets back CONNECTION_CLOSED. */
      len = g_socket_send_with_blocking (client, testbuf, strlen (testbuf) + 1,
					 TRUE, NULL, &error);
      g_assert_error (error, G_IO_ERROR, G_IO_ERROR_CONNECTION_CLOSED);
      g_assert_cmpint (len, ==, -1);
      g_clear_error (&error);
    }

  g_socket_close (client, &error);
  g_assert_no_error (error);
  g_socket_close (data->server, &error);
  g_assert_no_error (error);

  g_object_unref (data->server);
  g_object_unref (client);

  g_slice_free (IPTestData, data);
}

static void
test_ipv4_async (void)
{
  test_ip_async (G_SOCKET_FAMILY_IPV4);
}

static void
test_ipv6_async (void)
{
  if (!ipv6_supported)
    {
      g_test_skip ("No support for IPv6");
      return;
    }

  test_ip_async (G_SOCKET_FAMILY_IPV6);
}

static const gchar testbuf2[] = "0123456789abcdefghijklmnopqrstuvwxyz";

static void
test_ip_sync (GSocketFamily family)
{
  IPTestData *data;
  GError *error = NULL;
  GSocket *client;
  GSocketAddress *addr;
  gssize len;
  gchar buf[128];

  data = create_server (family, echo_server_thread, FALSE, &error);
  if (error != NULL)
    {
      gchar *message = g_strdup_printf ("Failed to create server: %s", error->message);
      g_test_skip (message);
      g_free (message);
      g_clear_error (&error);
      return;
    }

  addr = g_socket_get_local_address (data->server, &error);
  g_assert_no_error (error);

  client = g_socket_new (family,
			 G_SOCKET_TYPE_STREAM,
			 G_SOCKET_PROTOCOL_DEFAULT,
			 &error);
  g_assert_no_error (error);

  g_assert_cmpint (g_socket_get_family (client), ==, family);
  g_assert_cmpint (g_socket_get_socket_type (client), ==, G_SOCKET_TYPE_STREAM);
  g_assert_cmpint (g_socket_get_protocol (client), ==, G_SOCKET_PROTOCOL_DEFAULT);

  g_socket_set_blocking (client, TRUE);
  g_socket_set_timeout (client, 1);

  g_socket_connect (client, addr, NULL, &error);
  g_assert_no_error (error);
  g_assert (g_socket_is_connected (client));
  g_object_unref (addr);

  /* This adds 1 second to "make check", so let's just only do it once. */
  if (family == G_SOCKET_FAMILY_IPV4)
    {
      len = g_socket_receive (client, buf, sizeof (buf), NULL, &error);
      g_assert_cmpint (len, ==, -1);
      g_assert_error (error, G_IO_ERROR, G_IO_ERROR_TIMED_OUT);
      g_clear_error (&error);
    }

  len = g_socket_send (client, testbuf, strlen (testbuf) + 1, NULL, &error);
  g_assert_no_error (error);
  g_assert_cmpint (len, ==, strlen (testbuf) + 1);
  
  len = g_socket_receive (client, buf, sizeof (buf), NULL, &error);
  g_assert_no_error (error);
  g_assert_cmpint (len, ==, strlen (testbuf) + 1);

  g_assert_cmpstr (testbuf, ==, buf);

  {
    GOutputVector v[7] = { { NULL, }, };

    v[0].buffer = testbuf2 + 0;
    v[0].size = 3;
    v[1].buffer = testbuf2 + 3;
    v[1].size = 5;
    v[2].buffer = testbuf2 + 3 + 5;
    v[2].size = 0;
    v[3].buffer = testbuf2 + 3 + 5;
    v[3].size = 6;
    v[4].buffer = testbuf2 + 3 + 5 + 6;
    v[4].size = 2;
    v[5].buffer = testbuf2 + 3 + 5 + 6 + 2;
    v[5].size = 1;
    v[6].buffer = testbuf2 + 3 + 5 + 6 + 2 + 1;
    v[6].size = strlen (testbuf2) - (3 + 5 + 6 + 2 + 1);

    len = g_socket_send_message (client, NULL, v, G_N_ELEMENTS (v), NULL, 0, 0, NULL, &error);
    g_assert_no_error (error);
    g_assert_cmpint (len, ==, strlen (testbuf2));

    memset (buf, 0, sizeof (buf));
    len = g_socket_receive (client, buf, sizeof (buf), NULL, &error);
    g_assert_no_error (error);
    g_assert_cmpint (len, ==, strlen (testbuf2));
    g_assert_cmpstr (testbuf2, ==, buf);
  }

  g_socket_shutdown (client, FALSE, TRUE, &error);
  g_assert_no_error (error);

  g_thread_join (data->thread);

  if (family == G_SOCKET_FAMILY_IPV4)
    {
      /* Test that reading on a remote-closed socket gets back 0 bytes. */
      len = g_socket_receive (client, buf, sizeof (buf), NULL, &error);
      g_assert_no_error (error);
      g_assert_cmpint (len, ==, 0);
    }
  else
    {
      /* Test that writing to a remote-closed socket gets back CONNECTION_CLOSED. */
      len = g_socket_send (client, testbuf, strlen (testbuf) + 1, NULL, &error);
      g_assert_error (error, G_IO_ERROR, G_IO_ERROR_CONNECTION_CLOSED);
      g_assert_cmpint (len, ==, -1);
      g_clear_error (&error);
    }

  g_socket_close (client, &error);
  g_assert_no_error (error);
  g_socket_close (data->server, &error);
  g_assert_no_error (error);

  g_object_unref (data->server);
  g_object_unref (client);

  g_slice_free (IPTestData, data);
}

static void
test_ipv4_sync (void)
{
  test_ip_sync (G_SOCKET_FAMILY_IPV4);
}

static void
test_ipv6_sync (void)
{
  if (!ipv6_supported)
    {
      g_test_skip ("No support for IPv6");
      return;
    }

  test_ip_sync (G_SOCKET_FAMILY_IPV6);
}

static void
test_ip_sync_dgram (GSocketFamily family)
{
  IPTestData *data;
  GError *error = NULL;
  GSocket *client;
  GSocketAddress *dest_addr;
  gssize len;
  gchar buf[128];

  data = create_server_full (family, G_SOCKET_TYPE_DATAGRAM,
                             echo_server_dgram_thread, FALSE, &error);
  if (error != NULL)
    {
      gchar *message = g_strdup_printf ("Failed to create server: %s", error->message);
      g_test_skip (message);
      g_free (message);
      g_clear_error (&error);
      return;
    }

  dest_addr = g_socket_get_local_address (data->server, &error);

  client = g_socket_new (family,
			 G_SOCKET_TYPE_DATAGRAM,
			 G_SOCKET_PROTOCOL_DEFAULT,
			 &error);
  g_assert_no_error (error);

  g_assert_cmpint (g_socket_get_family (client), ==, family);
  g_assert_cmpint (g_socket_get_socket_type (client), ==, G_SOCKET_TYPE_DATAGRAM);
  g_assert_cmpint (g_socket_get_protocol (client), ==, G_SOCKET_PROTOCOL_DEFAULT);

  g_socket_set_blocking (client, TRUE);
  g_socket_set_timeout (client, 1);

  len = g_socket_send_to (client, dest_addr, testbuf, strlen (testbuf) + 1, NULL, &error);
  g_assert_no_error (error);
  g_assert_cmpint (len, ==, strlen (testbuf) + 1);

  len = g_socket_receive_from (client, NULL, buf, sizeof (buf), NULL, &error);
  g_assert_no_error (error);
  g_assert_cmpint (len, ==, strlen (testbuf) + 1);

  g_assert_cmpstr (testbuf, ==, buf);

  {
    GOutputMessage m[3] = { { NULL, }, };
    GInputMessage im[3] = { { NULL, }, };
    GOutputVector v[7] = { { NULL, }, };
    GInputVector iv[7] = { { NULL, }, };

    v[0].buffer = testbuf2 + 0;
    v[0].size = 3;
    v[1].buffer = testbuf2 + 3;
    v[1].size = 5;
    v[2].buffer = testbuf2 + 3 + 5;
    v[2].size = 0;
    v[3].buffer = testbuf2 + 3 + 5;
    v[3].size = 6;
    v[4].buffer = testbuf2 + 3 + 5 + 6;
    v[4].size = 2;
    v[5].buffer = testbuf2 + 3 + 5 + 6 + 2;
    v[5].size = 1;
    v[6].buffer = testbuf2 + 3 + 5 + 6 + 2 + 1;
    v[6].size = strlen (testbuf2) - (3 + 5 + 6 + 2 + 1);

    iv[0].buffer = buf + 0;
    iv[0].size = 3;
    iv[1].buffer = buf + 3;
    iv[1].size = 5;
    iv[2].buffer = buf + 3 + 5;
    iv[2].size = 0;
    iv[3].buffer = buf + 3 + 5;
    iv[3].size = 6;
    iv[4].buffer = buf + 3 + 5 + 6;
    iv[4].size = 2;
    iv[5].buffer = buf + 3 + 5 + 6 + 2;
    iv[5].size = 1;
    iv[6].buffer = buf + 3 + 5 + 6 + 2 + 1;
    iv[6].size = sizeof (buf) - (3 + 5 + 6 + 2 + 1);

    len = g_socket_send_message (client, dest_addr, v, G_N_ELEMENTS (v), NULL, 0, 0, NULL, &error);
    g_assert_no_error (error);
    g_assert_cmpint (len, ==, strlen (testbuf2));

    memset (buf, 0, sizeof (buf));
    len = g_socket_receive_from (client, NULL, buf, sizeof (buf), NULL, &error);
    g_assert_no_error (error);
    g_assert_cmpint (len, ==, strlen (testbuf2));
    g_assert_cmpstr (testbuf2, ==, buf);

    m[0].vectors = &v[0];
    m[0].num_vectors = 1;
    m[0].address = dest_addr;
    m[1].vectors = &v[0];
    m[1].num_vectors = 6;
    m[1].address = dest_addr;
    m[2].vectors = &v[6];
    m[2].num_vectors = 1;
    m[2].address = dest_addr;

    len = g_socket_send_messages (client, m, G_N_ELEMENTS (m), 0, NULL, &error);
    g_assert_no_error (error);
    g_assert_cmpint (len, ==, G_N_ELEMENTS (m));
    g_assert_cmpint (m[0].bytes_sent, ==, 3);
    g_assert_cmpint (m[1].bytes_sent, ==, 17);
    g_assert_cmpint (m[2].bytes_sent, ==, v[6].size);

    memset (buf, 0, sizeof (buf));
    len = g_socket_receive_from (client, NULL, buf, sizeof (buf), NULL, &error);
    g_assert_no_error (error);
    g_assert_cmpint (len, ==, 3);

    memset (buf, 0, sizeof (buf));
    len = g_socket_receive_from (client, NULL, buf, sizeof (buf), NULL, &error);
    g_assert_no_error (error);
    /* v[0].size + v[1].size + v[2].size + v[3].size + v[4].size + v[5].size */
    g_assert_cmpint (len, ==, 17);
    g_assert (memcmp (testbuf2, buf, 17) == 0);

    memset (buf, 0, sizeof (buf));
    len = g_socket_receive_from (client, NULL, buf, sizeof (buf), NULL, &error);
    g_assert_no_error (error);
    g_assert_cmpint (len, ==, v[6].size);
    g_assert_cmpstr (buf, ==, v[6].buffer);

    /* reset since we're re-using the message structs */
    m[0].bytes_sent = 0;
    m[1].bytes_sent = 0;
    m[2].bytes_sent = 0;

    /* now try receiving multiple messages */
    len = g_socket_send_messages (client, m, G_N_ELEMENTS (m), 0, NULL, &error);
    g_assert_no_error (error);
    g_assert_cmpint (len, ==, G_N_ELEMENTS (m));
    g_assert_cmpint (m[0].bytes_sent, ==, 3);
    g_assert_cmpint (m[1].bytes_sent, ==, 17);
    g_assert_cmpint (m[2].bytes_sent, ==, v[6].size);

    im[0].vectors = &iv[0];
    im[0].num_vectors = 1;
    im[1].vectors = &iv[0];
    im[1].num_vectors = 6;
    im[2].vectors = &iv[6];
    im[2].num_vectors = 1;

    memset (buf, 0, sizeof (buf));
    len = g_socket_receive_messages (client, im, G_N_ELEMENTS (im), 0,
                                     NULL, &error);
    g_assert_no_error (error);
    g_assert_cmpint (len, ==, G_N_ELEMENTS (im));

    g_assert_cmpuint (im[0].bytes_received, ==, 3);
    /* v[0].size + v[1].size + v[2].size + v[3].size + v[4].size + v[5].size */
    g_assert_cmpuint (im[1].bytes_received, ==, 17);
    g_assert_cmpuint (im[2].bytes_received, ==, v[6].size);

    /* reset since we're re-using the message structs */
    m[0].bytes_sent = 0;
    m[1].bytes_sent = 0;
    m[2].bytes_sent = 0;

    /* now try to generate an early return by omitting the destination address on [1] */
    m[1].address = NULL;
    len = g_socket_send_messages (client, m, G_N_ELEMENTS (m), 0, NULL, &error);
    g_assert_no_error (error);
    g_assert_cmpint (len, ==, 1);

    g_assert_cmpint (m[0].bytes_sent, ==, 3);
    g_assert_cmpint (m[1].bytes_sent, ==, 0);
    g_assert_cmpint (m[2].bytes_sent, ==, 0);

    /* reset since we're re-using the message structs */
    m[0].bytes_sent = 0;
    m[1].bytes_sent = 0;
    m[2].bytes_sent = 0;

    /* now try to generate an error by omitting all destination addresses */
    m[0].address = NULL;
    m[1].address = NULL;
    m[2].address = NULL;
    len = g_socket_send_messages (client, m, G_N_ELEMENTS (m), 0, NULL, &error);
    g_assert_error (error, G_IO_ERROR, G_IO_ERROR_FAILED);
    g_clear_error (&error);
    g_assert_cmpint (len, ==, -1);

    g_assert_cmpint (m[0].bytes_sent, ==, 0);
    g_assert_cmpint (m[1].bytes_sent, ==, 0);
    g_assert_cmpint (m[2].bytes_sent, ==, 0);

    len = g_socket_receive_from (client, NULL, buf, sizeof (buf), NULL, &error);
    g_assert_cmpint (len, ==, 3);
  }

  g_cancellable_cancel (data->cancellable);

  g_thread_join (data->thread);

  g_socket_close (client, &error);
  g_assert_no_error (error);
  g_socket_close (data->server, &error);
  g_assert_no_error (error);

  g_object_unref (data->server);
  g_object_unref (data->cancellable);
  g_object_unref (client);
  g_object_unref (dest_addr);

  g_slice_free (IPTestData, data);
}

static void
test_ipv4_sync_dgram (void)
{
  test_ip_sync_dgram (G_SOCKET_FAMILY_IPV4);
}

static void
test_ipv6_sync_dgram (void)
{
  if (!ipv6_supported)
    {
      g_test_skip ("No support for IPv6");
      return;
    }

  test_ip_sync_dgram (G_SOCKET_FAMILY_IPV6);
}

static gpointer
cancellable_thread_cb (gpointer data)
{
  GCancellable *cancellable = data;

  g_usleep (0.1 * G_USEC_PER_SEC);
  g_cancellable_cancel (cancellable);
  g_object_unref (cancellable);

  return NULL;
}

static void
test_ip_sync_dgram_timeouts (GSocketFamily family)
{
  GError *error = NULL;
  GSocket *client = NULL;
  GCancellable *cancellable = NULL;
  GThread *cancellable_thread = NULL;
  gssize len;

  client = g_socket_new (family,
                         G_SOCKET_TYPE_DATAGRAM,
                         G_SOCKET_PROTOCOL_DEFAULT,
                         &error);
  g_assert_no_error (error);

  g_assert_cmpint (g_socket_get_family (client), ==, family);
  g_assert_cmpint (g_socket_get_socket_type (client), ==, G_SOCKET_TYPE_DATAGRAM);
  g_assert_cmpint (g_socket_get_protocol (client), ==, G_SOCKET_PROTOCOL_DEFAULT);

  /* No overall timeout: test the per-operation timeouts instead. */
  g_socket_set_timeout (client, 0);

  cancellable = g_cancellable_new ();

  /* Check for timeouts when no server is running. */
  {
    gint64 start_time;
    GInputMessage im = { NULL, };
    GInputVector iv = { NULL, };
    guint8 buf[128];

    iv.buffer = buf;
    iv.size = sizeof (buf);

    im.vectors = &iv;
    im.num_vectors = 1;

    memset (buf, 0, sizeof (buf));

    /* Try a non-blocking read. */
    g_socket_set_blocking (client, FALSE);
    len = g_socket_receive_messages (client, &im, 1, 0  /* flags */,
                                     NULL, &error);
    g_assert_error (error, G_IO_ERROR, G_IO_ERROR_WOULD_BLOCK);
    g_assert_cmpint (len, ==, -1);
    g_clear_error (&error);

    /* Try a timeout read. Can’t really validate the time taken more than
     * checking it’s positive. */
    g_socket_set_timeout (client, 1);
    g_socket_set_blocking (client, TRUE);
    start_time = g_get_monotonic_time ();
    len = g_socket_receive_messages (client, &im, 1, 0  /* flags */,
                                     NULL, &error);
    g_assert_error (error, G_IO_ERROR, G_IO_ERROR_TIMED_OUT);
    g_assert_cmpint (len, ==, -1);
    g_assert_cmpint (g_get_monotonic_time () - start_time, >, 0);
    g_clear_error (&error);

    /* Try a blocking read, cancelled from another thread. */
    g_socket_set_timeout (client, 0);
    cancellable_thread = g_thread_new ("cancellable",
                                       cancellable_thread_cb,
                                       g_object_ref (cancellable));

    start_time = g_get_monotonic_time ();
    len = g_socket_receive_messages (client, &im, 1, 0  /* flags */,
                                     cancellable, &error);
    g_assert_error (error, G_IO_ERROR, G_IO_ERROR_CANCELLED);
    g_assert_cmpint (len, ==, -1);
    g_assert_cmpint (g_get_monotonic_time () - start_time, >, 0);
    g_clear_error (&error);

    g_thread_join (cancellable_thread);
  }

  g_socket_close (client, &error);
  g_assert_no_error (error);

  g_object_unref (client);
  g_object_unref (cancellable);
}

static void
test_ipv4_sync_dgram_timeouts (void)
{
  test_ip_sync_dgram_timeouts (G_SOCKET_FAMILY_IPV4);
}

static void
test_ipv6_sync_dgram_timeouts (void)
{
  if (!ipv6_supported)
    {
      g_test_skip ("No support for IPv6");
      return;
    }

  test_ip_sync_dgram_timeouts (G_SOCKET_FAMILY_IPV6);
}

static gpointer
graceful_server_thread (gpointer user_data)
{
  IPTestData *data = user_data;
  GSocket *sock;
  GError *error = NULL;
  gssize len;

  sock = g_socket_accept (data->server, NULL, &error);
  g_assert_no_error (error);

  len = g_socket_send (sock, testbuf, strlen (testbuf) + 1, NULL, &error);
  g_assert_no_error (error);
  g_assert_cmpint (len, ==, strlen (testbuf) + 1);

  return sock;
}

static void
test_close_graceful (void)
{
  GSocketFamily family = G_SOCKET_FAMILY_IPV4;
  IPTestData *data;
  GError *error = NULL;
  GSocket *client, *server;
  GSocketAddress *addr;
  gssize len;
  gchar buf[128];

  data = create_server (family, graceful_server_thread, FALSE, &error);
  if (error != NULL)
    {
      gchar *message = g_strdup_printf ("Failed to create server: %s", error->message);
      g_test_skip (message);
      g_free (message);
      g_clear_error (&error);
      return;
    }

  addr = g_socket_get_local_address (data->server, &error);
  g_assert_no_error (error);

  client = g_socket_new (family,
			 G_SOCKET_TYPE_STREAM,
			 G_SOCKET_PROTOCOL_DEFAULT,
			 &error);
  g_assert_no_error (error);

  g_assert_cmpint (g_socket_get_family (client), ==, family);
  g_assert_cmpint (g_socket_get_socket_type (client), ==, G_SOCKET_TYPE_STREAM);
  g_assert_cmpint (g_socket_get_protocol (client), ==, G_SOCKET_PROTOCOL_DEFAULT);

  g_socket_set_blocking (client, TRUE);
  g_socket_set_timeout (client, 1);

  g_socket_connect (client, addr, NULL, &error);
  g_assert_no_error (error);
  g_assert (g_socket_is_connected (client));
  g_object_unref (addr);

  server = g_thread_join (data->thread);

  /* similar to g_tcp_connection_set_graceful_disconnect(), but explicit */
  g_socket_shutdown (server, FALSE, TRUE, &error);
  g_assert_no_error (error);

  /* we must timeout */
  g_socket_condition_wait (client, G_IO_HUP, NULL, &error);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_TIMED_OUT);
  g_clear_error (&error);

  /* check that the remaining data is received */
  len = g_socket_receive (client, buf, strlen (testbuf) + 1, NULL, &error);
  g_assert_no_error (error);
  g_assert_cmpint (len, ==, strlen (testbuf) + 1);

  /* and only then the connection is closed */
  len = g_socket_receive (client, buf, sizeof (buf), NULL, &error);
  g_assert_no_error (error);
  g_assert_cmpint (len, ==, 0);

  g_socket_close (server, &error);
  g_assert_no_error (error);

  g_socket_close (client, &error);
  g_assert_no_error (error);

  g_object_unref (server);
  g_object_unref (data->server);
  g_object_unref (client);

  g_slice_free (IPTestData, data);
}

#if defined (IPPROTO_IPV6) && defined (IPV6_V6ONLY)
static gpointer
v4mapped_server_thread (gpointer user_data)
{
  IPTestData *data = user_data;
  GSocket *sock;
  GError *error = NULL;
  GSocketAddress *addr;

  sock = g_socket_accept (data->server, NULL, &error);
  g_assert_no_error (error);

  g_assert_cmpint (g_socket_get_family (sock), ==, G_SOCKET_FAMILY_IPV6);

  addr = g_socket_get_local_address (sock, &error);
  g_assert_no_error (error);
  g_assert_cmpint (g_socket_address_get_family (addr), ==, G_SOCKET_FAMILY_IPV4);
  g_object_unref (addr);

  addr = g_socket_get_remote_address (sock, &error);
  g_assert_no_error (error);
  g_assert_cmpint (g_socket_address_get_family (addr), ==, G_SOCKET_FAMILY_IPV4);
  g_object_unref (addr);

  g_socket_close (sock, &error);
  g_assert_no_error (error);
  g_object_unref (sock);
  return NULL;
}

static void
test_ipv6_v4mapped (void)
{
  IPTestData *data;
  GError *error = NULL;
  GSocket *client;
  GSocketAddress *addr, *v4addr;
  GInetAddress *iaddr;

  if (!ipv6_supported)
    {
      g_test_skip ("No support for IPv6");
      return;
    }

  data = create_server (G_SOCKET_FAMILY_IPV6, v4mapped_server_thread, TRUE, &error);
  if (error != NULL)
    {
      gchar *message = g_strdup_printf ("Failed to create server: %s", error->message);
      g_test_skip (message);
      g_free (message);
      g_clear_error (&error);
      return;
    }

  client = g_socket_new (G_SOCKET_FAMILY_IPV4,
			 G_SOCKET_TYPE_STREAM,
			 G_SOCKET_PROTOCOL_DEFAULT,
			 &error);
  g_assert_no_error (error);

  g_socket_set_blocking (client, TRUE);
  g_socket_set_timeout (client, 1);

  addr = g_socket_get_local_address (data->server, &error);
  g_assert_no_error (error);
  iaddr = g_inet_address_new_loopback (G_SOCKET_FAMILY_IPV4);
  v4addr = g_inet_socket_address_new (iaddr, g_inet_socket_address_get_port (G_INET_SOCKET_ADDRESS (addr)));
  g_object_unref (iaddr);
  g_object_unref (addr);

  g_socket_connect (client, v4addr, NULL, &error);
  g_assert_no_error (error);
  g_assert (g_socket_is_connected (client));

  g_thread_join (data->thread);

  g_socket_close (client, &error);
  g_assert_no_error (error);
  g_socket_close (data->server, &error);
  g_assert_no_error (error);

  g_object_unref (data->server);
  g_object_unref (client);
  g_object_unref (v4addr);

  g_slice_free (IPTestData, data);
}
#endif

static void
test_timed_wait (void)
{
  IPTestData *data;
  GError *error = NULL;
  GSocket *client;
  GSocketAddress *addr;
  gint64 start_time;
  gint poll_duration;

  data = create_server (G_SOCKET_FAMILY_IPV4, echo_server_thread, FALSE, &error);
  if (error != NULL)
    {
      gchar *message = g_strdup_printf ("Failed to create server: %s", error->message);
      g_test_skip (message);
      g_free (message);
      g_clear_error (&error);
      return;
    }

  addr = g_socket_get_local_address (data->server, &error);
  g_assert_no_error (error);

  client = g_socket_new (G_SOCKET_FAMILY_IPV4,
			 G_SOCKET_TYPE_STREAM,
			 G_SOCKET_PROTOCOL_DEFAULT,
			 &error);
  g_assert_no_error (error);

  g_socket_set_blocking (client, TRUE);
  g_socket_set_timeout (client, 1);

  g_socket_connect (client, addr, NULL, &error);
  g_assert_no_error (error);
  g_object_unref (addr);

  start_time = g_get_monotonic_time ();
  g_socket_condition_timed_wait (client, G_IO_IN, 100000 /* 100 ms */,
				 NULL, &error);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_TIMED_OUT);
  g_clear_error (&error);
  poll_duration = g_get_monotonic_time () - start_time;

  g_assert_cmpint (poll_duration, >=, 98000);
  g_assert_cmpint (poll_duration, <, 112000);

  g_socket_close (client, &error);
  g_assert_no_error (error);

  g_thread_join (data->thread);

  g_socket_close (data->server, &error);
  g_assert_no_error (error);

  g_object_unref (data->server);
  g_object_unref (client);

  g_slice_free (IPTestData, data);
}

static int
duplicate_fd (int fd)
{
#ifdef G_OS_WIN32
  HANDLE newfd;

  if (!DuplicateHandle (GetCurrentProcess (),
                        (HANDLE)fd,
                        GetCurrentProcess (),
                        &newfd,
                        0,
                        FALSE,
                        DUPLICATE_SAME_ACCESS))
    {
      return -1;
    }

  return (int)newfd;
#else
  return dup (fd);
#endif
}

static void
test_fd_reuse (void)
{
  IPTestData *data;
  GError *error = NULL;
  GSocket *client;
  GSocket *client2;
  GSocketAddress *addr;
  int fd;
  gssize len;
  gchar buf[128];

  g_test_bug ("741707");

  data = create_server (G_SOCKET_FAMILY_IPV4, echo_server_thread, FALSE, &error);
  if (error != NULL)
    {
      gchar *message = g_strdup_printf ("Failed to create server: %s", error->message);
      g_test_skip (message);
      g_free (message);
      g_clear_error (&error);
      return;
    }

  addr = g_socket_get_local_address (data->server, &error);
  g_assert_no_error (error);

  client = g_socket_new (G_SOCKET_FAMILY_IPV4,
                         G_SOCKET_TYPE_STREAM,
                         G_SOCKET_PROTOCOL_DEFAULT,
                         &error);
  g_assert_no_error (error);

  g_socket_set_blocking (client, TRUE);
  g_socket_set_timeout (client, 1);

  g_socket_connect (client, addr, NULL, &error);
  g_assert_no_error (error);
  g_assert (g_socket_is_connected (client));
  g_object_unref (addr);

  /* we have to dup otherwise the fd gets closed twice on unref */
  fd = duplicate_fd (g_socket_get_fd (client));
  client2 = g_socket_new_from_fd (fd, &error);
  g_assert_no_error (error);

  g_assert_cmpint (g_socket_get_family (client2), ==, g_socket_get_family (client));
  g_assert_cmpint (g_socket_get_socket_type (client2), ==, g_socket_get_socket_type (client));
  g_assert_cmpint (g_socket_get_protocol (client2), ==, G_SOCKET_PROTOCOL_TCP);

  len = g_socket_send (client2, testbuf, strlen (testbuf) + 1, NULL, &error);
  g_assert_no_error (error);
  g_assert_cmpint (len, ==, strlen (testbuf) + 1);

  len = g_socket_receive (client2, buf, sizeof (buf), NULL, &error);
  g_assert_no_error (error);
  g_assert_cmpint (len, ==, strlen (testbuf) + 1);

  g_assert_cmpstr (testbuf, ==, buf);

  g_socket_shutdown (client, FALSE, TRUE, &error);
  g_assert_no_error (error);
  /* The semantics of dup()+shutdown() are ambiguous; this call will succeed
   * on Linux, but return ENOTCONN on OS X.
   */
  g_socket_shutdown (client2, FALSE, TRUE, NULL);

  g_thread_join (data->thread);

  g_socket_close (client, &error);
  g_assert_no_error (error);
  g_socket_close (client2, &error);
  g_assert_no_error (error);
  g_socket_close (data->server, &error);
  g_assert_no_error (error);

  g_assert_cmpint (g_socket_get_fd (client), ==, -1);
  g_assert_cmpint (g_socket_get_fd (client2), ==, -1);
  g_assert_cmpint (g_socket_get_fd (data->server), ==, -1);

  g_object_unref (data->server);
  g_object_unref (client);
  g_object_unref (client2);

  g_slice_free (IPTestData, data);
}

static void
test_sockaddr (void)
{
  struct sockaddr_in6 sin6, gsin6;
  GSocketAddress *saddr;
  GInetSocketAddress *isaddr;
  GInetAddress *iaddr;
  GError *error = NULL;

  memset (&sin6, 0, sizeof (sin6));
  sin6.sin6_family = AF_INET6;
  sin6.sin6_addr = in6addr_loopback;
  sin6.sin6_port = g_htons (42);
  sin6.sin6_scope_id = 17;
  sin6.sin6_flowinfo = 1729;

  saddr = g_socket_address_new_from_native (&sin6, sizeof (sin6));
  g_assert (G_IS_INET_SOCKET_ADDRESS (saddr));

  isaddr = G_INET_SOCKET_ADDRESS (saddr);
  iaddr = g_inet_socket_address_get_address (isaddr);
  g_assert_cmpint (g_inet_address_get_family (iaddr), ==, G_SOCKET_FAMILY_IPV6);
  g_assert (g_inet_address_get_is_loopback (iaddr));

  g_assert_cmpint (g_inet_socket_address_get_port (isaddr), ==, 42);
  g_assert_cmpint (g_inet_socket_address_get_scope_id (isaddr), ==, 17);
  g_assert_cmpint (g_inet_socket_address_get_flowinfo (isaddr), ==, 1729);

  g_socket_address_to_native (saddr, &gsin6, sizeof (gsin6), &error);
  g_assert_no_error (error);

  g_assert (memcmp (&sin6.sin6_addr, &gsin6.sin6_addr, sizeof (struct in6_addr)) == 0);
  g_assert_cmpint (sin6.sin6_port, ==, gsin6.sin6_port);
  g_assert_cmpint (sin6.sin6_scope_id, ==, gsin6.sin6_scope_id);
  g_assert_cmpint (sin6.sin6_flowinfo, ==, gsin6.sin6_flowinfo);

  g_object_unref (saddr);
}

#ifdef G_OS_UNIX
static void
test_unix_from_fd (void)
{
  gint fd;
  GError *error;
  GSocket *s;

  fd = socket (AF_UNIX, SOCK_STREAM, 0);
  g_assert_cmpint (fd, !=, -1);

  error = NULL;
  s = g_socket_new_from_fd (fd, &error);
  g_assert_no_error (error);
  g_assert_cmpint (g_socket_get_family (s), ==, G_SOCKET_FAMILY_UNIX);
  g_assert_cmpint (g_socket_get_socket_type (s), ==, G_SOCKET_TYPE_STREAM);
  g_assert_cmpint (g_socket_get_protocol (s), ==, G_SOCKET_PROTOCOL_DEFAULT);
  g_object_unref (s);
}

static void
test_unix_connection (void)
{
  gint fd;
  GError *error;
  GSocket *s;
  GSocketConnection *c;

  fd = socket (AF_UNIX, SOCK_STREAM, 0);
  g_assert_cmpint (fd, !=, -1);

  error = NULL;
  s = g_socket_new_from_fd (fd, &error);
  g_assert_no_error (error);
  c = g_socket_connection_factory_create_connection (s);
  g_assert (G_IS_UNIX_CONNECTION (c));
  g_object_unref (c);
  g_object_unref (s);
}

static GSocketConnection *
create_connection_for_fd (int fd)
{
  GError *err = NULL;
  GSocket *socket;
  GSocketConnection *connection;

  socket = g_socket_new_from_fd (fd, &err);
  g_assert_no_error (err);
  g_assert (G_IS_SOCKET (socket));
  connection = g_socket_connection_factory_create_connection (socket);
  g_assert (G_IS_UNIX_CONNECTION (connection));
  g_object_unref (socket);
  return connection;
}

#define TEST_DATA "failure to say failure to say 'i love gnome-panel!'."

static void
test_unix_connection_ancillary_data (void)
{
  GError *err = NULL;
  gint pv[2], sv[3];
  gint status, fd, len;
  char buffer[1024];
  pid_t pid;

  status = pipe (pv);
  g_assert_cmpint (status, ==, 0);

  status = socketpair (PF_UNIX, SOCK_STREAM, 0, sv);
  g_assert_cmpint (status, ==, 0);

  pid = fork ();
  g_assert_cmpint (pid, >=, 0);

  /* Child: close its copy of the write end of the pipe, receive it
   * again from the parent over the socket, and write some text to it.
   *
   * Parent: send the write end of the pipe (still open for the
   * parent) over the socket, close it, and read some text from the
   * read end of the pipe.
   */
  if (pid == 0)
    {
      GSocketConnection *connection;

      close (sv[1]);
      connection = create_connection_for_fd (sv[0]);

      status = close (pv[1]);
      g_assert_cmpint (status, ==, 0);

      err = NULL;
      fd = g_unix_connection_receive_fd (G_UNIX_CONNECTION (connection), NULL,
					 &err);
      g_assert_no_error (err);
      g_assert_cmpint (fd, >, -1);
      g_object_unref (connection);

      do
	len = write (fd, TEST_DATA, sizeof (TEST_DATA));
      while (len == -1 && errno == EINTR);
      g_assert_cmpint (len, ==, sizeof (TEST_DATA));
      exit (0);
    }
  else
    {
      GSocketConnection *connection;

      close (sv[0]);
      connection = create_connection_for_fd (sv[1]);

      err = NULL;
      g_unix_connection_send_fd (G_UNIX_CONNECTION (connection), pv[1], NULL,
				 &err);
      g_assert_no_error (err);
      g_object_unref (connection);

      status = close (pv[1]);
      g_assert_cmpint (status, ==, 0);

      memset (buffer, 0xff, sizeof buffer);
      do
	len = read (pv[0], buffer, sizeof buffer);
      while (len == -1 && errno == EINTR);

      g_assert_cmpint (len, ==, sizeof (TEST_DATA));
      g_assert_cmpstr (buffer, ==, TEST_DATA);

      waitpid (pid, &status, 0);
      g_assert (WIFEXITED (status));
      g_assert_cmpint (WEXITSTATUS (status), ==, 0);
    }

  /* TODO: add test for g_unix_connection_send_credentials() and
   * g_unix_connection_receive_credentials().
   */
}

static gboolean
postmortem_source_cb (GSocket      *socket,
                      GIOCondition  condition,
                      gpointer      user_data)
{
  gboolean *been_here = user_data;

  g_assert_cmpint (condition, ==, G_IO_NVAL);

  *been_here = TRUE;
  return FALSE;
}

static void
test_source_postmortem (void)
{
  GMainContext *context;
  GSocket *socket;
  GSource *source;
  GError *error = NULL;
  gboolean callback_visited = FALSE;

  socket = g_socket_new (G_SOCKET_FAMILY_UNIX, G_SOCKET_TYPE_STREAM, G_SOCKET_PROTOCOL_DEFAULT, &error);
  g_assert_no_error (error);

  context = g_main_context_new ();

  source = g_socket_create_source (socket, G_IO_IN, NULL);
  g_source_set_callback (source, (GSourceFunc) postmortem_source_cb,
                         &callback_visited, NULL);
  g_source_attach (source, context);
  g_source_unref (source);

  g_socket_close (socket, &error);
  g_assert_no_error (error);
  g_object_unref (socket);

  /* Test that, after a socket is closed, its source callback should be called
   * exactly once. */
  g_main_context_iteration (context, FALSE);
  g_assert (callback_visited);
  g_assert (!g_main_context_pending (context));

  g_main_context_unref (context);
}

#endif /* G_OS_UNIX */

static void
test_reuse_tcp (void)
{
  GSocket *sock1, *sock2;
  GError *error = NULL;
  GInetAddress *iaddr;
  GSocketAddress *addr;

  sock1 = g_socket_new (G_SOCKET_FAMILY_IPV4,
                        G_SOCKET_TYPE_STREAM,
                        G_SOCKET_PROTOCOL_DEFAULT,
                        &error);
  g_assert_no_error (error);

  iaddr = g_inet_address_new_loopback (G_SOCKET_FAMILY_IPV4);
  addr = g_inet_socket_address_new (iaddr, 0);
  g_object_unref (iaddr);
  g_socket_bind (sock1, addr, TRUE, &error);
  g_object_unref (addr);
  g_assert_no_error (error);

  g_socket_listen (sock1, &error);
  g_assert_no_error (error);

  sock2 = g_socket_new (G_SOCKET_FAMILY_IPV4,
                        G_SOCKET_TYPE_STREAM,
                        G_SOCKET_PROTOCOL_DEFAULT,
                        &error);
  g_assert_no_error (error);

  addr = g_socket_get_local_address (sock1, &error);
  g_assert_no_error (error);
  g_socket_bind (sock2, addr, TRUE, &error);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_ADDRESS_IN_USE);
  g_clear_error (&error);
  g_object_unref (addr);

  g_object_unref (sock1);
  g_object_unref (sock2);
}

static void
test_reuse_udp (void)
{
  GSocket *sock1, *sock2;
  GError *error = NULL;
  GInetAddress *iaddr;
  GSocketAddress *addr;

  sock1 = g_socket_new (G_SOCKET_FAMILY_IPV4,
                        G_SOCKET_TYPE_DATAGRAM,
                        G_SOCKET_PROTOCOL_DEFAULT,
                        &error);
  g_assert_no_error (error);

  iaddr = g_inet_address_new_loopback (G_SOCKET_FAMILY_IPV4);
  addr = g_inet_socket_address_new (iaddr, 0);
  g_object_unref (iaddr);
  g_socket_bind (sock1, addr, TRUE, &error);
  g_object_unref (addr);
  g_assert_no_error (error);

  sock2 = g_socket_new (G_SOCKET_FAMILY_IPV4,
                        G_SOCKET_TYPE_DATAGRAM,
                        G_SOCKET_PROTOCOL_DEFAULT,
                        &error);
  g_assert_no_error (error);

  addr = g_socket_get_local_address (sock1, &error);
  g_assert_no_error (error);
  g_socket_bind (sock2, addr, TRUE, &error);
  g_object_unref (addr);
  g_assert_no_error (error);

  g_object_unref (sock1);
  g_object_unref (sock2);
}

static void
test_get_available (gconstpointer user_data)
{
  GSocketType socket_type = GPOINTER_TO_UINT (user_data);
  GError *err = NULL;
  GSocket *listener, *server, *client;
  GInetAddress *addr;
  GSocketAddress *saddr;
  gchar data[] = "0123456789abcdef";
  gchar buf[34];
  gssize nread;

  listener = g_socket_new (G_SOCKET_FAMILY_IPV4,
                           socket_type,
                           G_SOCKET_PROTOCOL_DEFAULT,
                           &err);
  g_assert_no_error (err);
  g_assert (G_IS_SOCKET (listener));

  client = g_socket_new (G_SOCKET_FAMILY_IPV4,
                         socket_type,
                         G_SOCKET_PROTOCOL_DEFAULT,
                         &err);
  g_assert_no_error (err);
  g_assert (G_IS_SOCKET (client));

  if (socket_type == G_SOCKET_TYPE_STREAM)
    {
      g_socket_set_option (client, IPPROTO_TCP, TCP_NODELAY, TRUE, &err);
      g_assert_no_error (err);
    }

  addr = g_inet_address_new_any (G_SOCKET_FAMILY_IPV4);
  saddr = g_inet_socket_address_new (addr, 0);

  g_socket_bind (listener, saddr, TRUE, &err);
  g_assert_no_error (err);
  g_object_unref (saddr);
  g_object_unref (addr);

  saddr = g_socket_get_local_address (listener, &err);
  g_assert_no_error (err);

  if (socket_type == G_SOCKET_TYPE_STREAM)
    {
      g_socket_listen (listener, &err);
      g_assert_no_error (err);
      g_socket_connect (client, saddr, NULL, &err);
      g_assert_no_error (err);

      server = g_socket_accept (listener, NULL, &err);
      g_assert_no_error (err);
      g_socket_set_blocking (server, FALSE);
      g_object_unref (listener);
    }
  else
    server = listener;

  g_socket_send_to (client, saddr, data, sizeof (data), NULL, &err);
  g_assert_no_error (err);

  while (!g_socket_condition_wait (server, G_IO_IN, NULL, NULL))
    ;
  g_assert_cmpint (g_socket_get_available_bytes (server), ==, sizeof (data));

  g_socket_send_to (client, saddr, data, sizeof (data), NULL, &err);
  g_assert_no_error (err);

  /* We need to wait until the data has actually been copied into the
   * server socket's buffers, but g_socket_condition_wait() won't help
   * here since the socket is definitely already readable. So there's
   * a race condition in checking its available bytes. In the TCP
   * case, we poll for a bit until the new data shows up. In the UDP
   * case, there's not much we can do, but at least the failure mode
   * is passes-when-it-shouldn't, not fails-when-it-shouldn't.
   */
  if (socket_type == G_SOCKET_TYPE_STREAM)
    {
      int tries;

      for (tries = 0; tries < 100; tries++)
        {
          if (g_socket_get_available_bytes (server) > sizeof (data))
            break;
          g_usleep (100000);
        }

      g_assert_cmpint (g_socket_get_available_bytes (server), ==, 2 * sizeof (data));
    }
  else
    {
      g_usleep (100000);
      g_assert_cmpint (g_socket_get_available_bytes (server), ==, sizeof (data));
    }

  g_assert_cmpint (sizeof (buf), >=, 2 * sizeof (data));
  nread = g_socket_receive (server, buf, sizeof (buf), NULL, &err);
  g_assert_no_error (err);

  if (socket_type == G_SOCKET_TYPE_STREAM)
    {
      g_assert_cmpint (nread, ==, 2 * sizeof (data));
      g_assert_cmpint (g_socket_get_available_bytes (server), ==, 0);
    }
  else
    {
      g_assert_cmpint (nread, ==, sizeof (data));
      g_assert_cmpint (g_socket_get_available_bytes (server), ==, sizeof (data));
    }

  nread = g_socket_receive (server, buf, sizeof (buf), NULL, &err);
  if (socket_type == G_SOCKET_TYPE_STREAM)
    {
      g_assert_cmpint (nread, ==, -1);
      g_assert_error (err, G_IO_ERROR, G_IO_ERROR_WOULD_BLOCK);
      g_clear_error (&err);
    }
  else
    {
      g_assert_cmpint (nread, ==, sizeof (data));
      g_assert_no_error (err);
    }

  g_assert_cmpint (g_socket_get_available_bytes (server), ==, 0);

  g_socket_close (server, &err);
  g_assert_no_error (err);

  g_object_unref (saddr);
  g_object_unref (server);
  g_object_unref (client);
}

int
main (int   argc,
      char *argv[])
{
  GSocket *sock;
  GError *error = NULL;

  g_test_init (&argc, &argv, NULL);
  g_test_bug_base ("https://bugzilla.gnome.org/");

  sock = g_socket_new (G_SOCKET_FAMILY_IPV6,
                       G_SOCKET_TYPE_STREAM,
                       G_SOCKET_PROTOCOL_DEFAULT,
                       &error);
  if (sock != NULL)
    {
      ipv6_supported = TRUE;
      g_object_unref (sock);
    }
  else
    {
      g_assert_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED);
      g_clear_error (&error);
    }

  g_test_add_func ("/socket/ipv4_sync", test_ipv4_sync);
  g_test_add_func ("/socket/ipv4_async", test_ipv4_async);
  g_test_add_func ("/socket/ipv6_sync", test_ipv6_sync);
  g_test_add_func ("/socket/ipv6_async", test_ipv6_async);
  g_test_add_func ("/socket/ipv4_sync/datagram", test_ipv4_sync_dgram);
  g_test_add_func ("/socket/ipv4_sync/datagram/timeouts", test_ipv4_sync_dgram_timeouts);
  g_test_add_func ("/socket/ipv6_sync/datagram", test_ipv6_sync_dgram);
  g_test_add_func ("/socket/ipv6_sync/datagram/timeouts", test_ipv6_sync_dgram_timeouts);
#if defined (IPPROTO_IPV6) && defined (IPV6_V6ONLY)
  g_test_add_func ("/socket/ipv6_v4mapped", test_ipv6_v4mapped);
#endif
  g_test_add_func ("/socket/close_graceful", test_close_graceful);
  g_test_add_func ("/socket/timed_wait", test_timed_wait);
  g_test_add_func ("/socket/fd_reuse", test_fd_reuse);
  g_test_add_func ("/socket/address", test_sockaddr);
#ifdef G_OS_UNIX
  g_test_add_func ("/socket/unix-from-fd", test_unix_from_fd);
  g_test_add_func ("/socket/unix-connection", test_unix_connection);
  g_test_add_func ("/socket/unix-connection-ancillary-data", test_unix_connection_ancillary_data);
  g_test_add_func ("/socket/source-postmortem", test_source_postmortem);
#endif
  g_test_add_func ("/socket/reuse/tcp", test_reuse_tcp);
  g_test_add_func ("/socket/reuse/udp", test_reuse_udp);
  g_test_add_data_func ("/socket/get_available/datagram", GUINT_TO_POINTER (G_SOCKET_TYPE_DATAGRAM),
                        test_get_available);
  g_test_add_data_func ("/socket/get_available/stream", GUINT_TO_POINTER (G_SOCKET_TYPE_STREAM),
                        test_get_available);

  return g_test_run();
}

