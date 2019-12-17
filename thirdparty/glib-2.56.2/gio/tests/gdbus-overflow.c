/* GLib testing framework examples and tests
 *
 * Copyright (C) 2008-2010 Red Hat, Inc.
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
 * Author: David Zeuthen <davidz@redhat.com>
 */

#include "config.h"

#include <gio/gio.h>
#include <unistd.h>
#include <string.h>

/* for open(2) */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

/* for g_unlink() */
#include <glib/gstdio.h>

#include <gio/gnetworking.h>
#include <gio/gunixsocketaddress.h>
#include <gio/gunixfdlist.h>

/* used in test_overflow */
#ifdef G_OS_UNIX
#include <gio/gunixconnection.h>
#include <errno.h>
#endif

#ifdef G_OS_UNIX
static gboolean is_unix = TRUE;
#else
static gboolean is_unix = FALSE;
#endif

static gchar *tmp_address = NULL;
static gchar *test_guid = NULL;
static GMainLoop *loop = NULL;

static const gchar *test_interface_introspection_xml =
  "<node>"
  "  <interface name='org.gtk.GDBus.PeerTestInterface'>"
  "    <method name='HelloPeer'>"
  "      <arg type='s' name='greeting' direction='in'/>"
  "      <arg type='s' name='response' direction='out'/>"
  "    </method>"
  "    <method name='EmitSignal'/>"
  "    <method name='EmitSignalWithNameSet'/>"
  "    <method name='OpenFile'>"
  "      <arg type='s' name='path' direction='in'/>"
  "    </method>"
  "    <signal name='PeerSignal'>"
  "      <arg type='s' name='a_string'/>"
  "    </signal>"
  "    <property type='s' name='PeerProperty' access='read'/>"
  "  </interface>"
  "</node>";
static GDBusInterfaceInfo *test_interface_introspection_data = NULL;


#ifdef G_OS_UNIX

/* Chosen to be big enough to overflow the socket buffer */
#define OVERFLOW_NUM_SIGNALS 5000
#define OVERFLOW_TIMEOUT_SEC 10

static GDBusMessage *
overflow_filter_func (GDBusConnection *connection,
                      GDBusMessage    *message,
                      gboolean         incoming,
                      gpointer         user_data)
{
  volatile gint *counter = user_data;
  *counter += 1;
  return message;
}

static gboolean
overflow_on_500ms_later_func (gpointer user_data)
{
  g_main_loop_quit (loop);
  return FALSE; /* don't keep the idle */
}

static void
test_overflow (void)
{
  gint sv[2];
  gint n;
  GSocket *socket;
  GSocketConnection *socket_connection;
  GDBusConnection *producer, *consumer;
  GError *error;
  GTimer *timer;
  volatile gint n_messages_received;
  volatile gint n_messages_sent;

  g_assert_cmpint (socketpair (AF_UNIX, SOCK_STREAM, 0, sv), ==, 0);

  error = NULL;
  socket = g_socket_new_from_fd (sv[0], &error);
  g_assert_no_error (error);
  socket_connection = g_socket_connection_factory_create_connection (socket);
  g_assert (socket_connection != NULL);
  g_object_unref (socket);
  producer = g_dbus_connection_new_sync (G_IO_STREAM (socket_connection),
					 NULL, /* guid */
					 G_DBUS_CONNECTION_FLAGS_NONE,
					 NULL, /* GDBusAuthObserver */
					 NULL, /* GCancellable */

					 &error);
  g_dbus_connection_set_exit_on_close (producer, TRUE);
  g_assert_no_error (error);
  g_object_unref (socket_connection);
  n_messages_sent = 0;
  g_dbus_connection_add_filter (producer, overflow_filter_func, (gpointer) &n_messages_sent, NULL);

  /* send enough data that we get an EAGAIN */
  for (n = 0; n < OVERFLOW_NUM_SIGNALS; n++)
    {
      error = NULL;
      g_dbus_connection_emit_signal (producer,
                                     NULL, /* destination */
                                     "/org/foo/Object",
                                     "org.foo.Interface",
                                     "Member",
                                     g_variant_new ("(s)", "a string"),
                                     &error);
      g_assert_no_error (error);
    }

  /* sleep for 0.5 sec (to allow the GDBus IO thread to fill up the
   * kernel buffers) and verify that n_messages_sent <
   * OVERFLOW_NUM_SIGNALS
   *
   * This is to verify that not all the submitted messages have been
   * sent to the underlying transport.
   */
  g_timeout_add (500, overflow_on_500ms_later_func, NULL);
  g_main_loop_run (loop);
  g_assert_cmpint (n_messages_sent, <, OVERFLOW_NUM_SIGNALS);

  /* now suck it all out as a client, and add it up */
  socket = g_socket_new_from_fd (sv[1], &error);
  g_assert_no_error (error);
  socket_connection = g_socket_connection_factory_create_connection (socket);
  g_assert (socket_connection != NULL);
  g_object_unref (socket);
  consumer = g_dbus_connection_new_sync (G_IO_STREAM (socket_connection),
					 NULL, /* guid */
					 G_DBUS_CONNECTION_FLAGS_DELAY_MESSAGE_PROCESSING,
					 NULL, /* GDBusAuthObserver */
					 NULL, /* GCancellable */
					 &error);
  g_assert_no_error (error);
  g_object_unref (socket_connection);
  n_messages_received = 0;
  g_dbus_connection_add_filter (consumer, overflow_filter_func, (gpointer) &n_messages_received, NULL);
  g_dbus_connection_start_message_processing (consumer);

  timer = g_timer_new ();
  g_timer_start (timer);

  while (n_messages_received < OVERFLOW_NUM_SIGNALS && g_timer_elapsed (timer, NULL) < OVERFLOW_TIMEOUT_SEC)
      g_main_context_iteration (NULL, FALSE);

  g_assert_cmpint (n_messages_sent, ==, OVERFLOW_NUM_SIGNALS);
  g_assert_cmpint (n_messages_received, ==, OVERFLOW_NUM_SIGNALS);

  g_timer_destroy (timer);
  g_object_unref (consumer);
  g_object_unref (producer);
}
#else
static void
test_overflow (void)
{
  /* TODO: test this with e.g. GWin32InputStream/GWin32OutputStream */
}
#endif

/* ---------------------------------------------------------------------------------------------------- */


int
main (int   argc,
      char *argv[])
{
  gint ret;
  GDBusNodeInfo *introspection_data = NULL;
  gchar *tmpdir = NULL;

  g_test_init (&argc, &argv, NULL);

  introspection_data = g_dbus_node_info_new_for_xml (test_interface_introspection_xml, NULL);
  g_assert (introspection_data != NULL);
  test_interface_introspection_data = introspection_data->interfaces[0];

  test_guid = g_dbus_generate_guid ();

  if (is_unix)
    {
      if (g_unix_socket_address_abstract_names_supported ())
	tmp_address = g_strdup ("unix:tmpdir=/tmp/gdbus-test-");
      else
	{
	  tmpdir = g_dir_make_tmp ("gdbus-test-XXXXXX", NULL);
	  tmp_address = g_strdup_printf ("unix:tmpdir=%s", tmpdir);
	}
    }
  else
    tmp_address = g_strdup ("nonce-tcp:");

  /* all the tests rely on a shared main loop */
  loop = g_main_loop_new (NULL, FALSE);

  g_test_add_func ("/gdbus/overflow", test_overflow);

  ret = g_test_run();

  g_main_loop_unref (loop);
  g_free (test_guid);
  g_dbus_node_info_unref (introspection_data);
  if (is_unix)
    g_free (tmp_address);
  if (tmpdir)
    {
      g_rmdir (tmpdir);
      g_free (tmpdir);
    }

  return ret;
}
