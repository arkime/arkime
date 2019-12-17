/* GLib testing framework examples and tests
 *
 * Copyright 2014 Red Hat, Inc.
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
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see
 * <http://www.gnu.org/licenses/>.
 */

#include <gio/gio.h>

static void
event_cb (GSocketListener      *listener,
          GSocketListenerEvent  event,
          GSocket              *socket,
          gpointer              data)
{
  static GSocketListenerEvent expected_event = G_SOCKET_LISTENER_BINDING;
  gboolean *success = (gboolean *)data;

  g_assert (G_IS_SOCKET_LISTENER (listener));
  g_assert (G_IS_SOCKET (socket));
  g_assert (event == expected_event);

  switch (event)
    {
      case G_SOCKET_LISTENER_BINDING:
        expected_event = G_SOCKET_LISTENER_BOUND;
        break;
      case G_SOCKET_LISTENER_BOUND:
        expected_event = G_SOCKET_LISTENER_LISTENING;
        break;
      case G_SOCKET_LISTENER_LISTENING:
        expected_event = G_SOCKET_LISTENER_LISTENED;
        break;
      case G_SOCKET_LISTENER_LISTENED:
        *success = TRUE;
        break;
    }
}

static void
test_event_signal (void)
{
  gboolean success = FALSE;
  GInetAddress *iaddr;
  GSocketAddress *saddr;
  GSocketListener *listener;
  GError *error = NULL;

  iaddr = g_inet_address_new_loopback (G_SOCKET_FAMILY_IPV4);
  saddr = g_inet_socket_address_new (iaddr, 0);
  g_object_unref (iaddr);

  listener = g_socket_listener_new ();

  g_signal_connect (listener, "event", G_CALLBACK (event_cb), &success);

  g_socket_listener_add_address (listener,
                                 saddr,
                                 G_SOCKET_TYPE_STREAM,
                                 G_SOCKET_PROTOCOL_TCP,
                                 NULL,
                                 NULL,
                                 &error);
  g_assert_no_error (error);
  g_assert_true (success);

  g_object_unref (saddr);
  g_object_unref (listener);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_bug_base ("http://bugzilla.gnome.org/");

  g_test_add_func ("/socket-listener/event-signal", test_event_signal);

  return g_test_run();
}

