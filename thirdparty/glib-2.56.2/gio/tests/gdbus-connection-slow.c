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

#include <gio/gio.h>
#include <unistd.h>
#include <string.h>

#include <sys/types.h>

#include "gdbus-tests.h"

/* all tests rely on a shared mainloop */
static GMainLoop *loop = NULL;

/* ---------------------------------------------------------------------------------------------------- */

static void
test_connection_flush_signal_handler (GDBusConnection  *connection,
                                      const gchar      *sender_name,
                                      const gchar      *object_path,
                                      const gchar      *interface_name,
                                      const gchar      *signal_name,
                                      GVariant         *parameters,
                                      gpointer         user_data)
{
  g_main_loop_quit (loop);
}

static gboolean
test_connection_flush_on_timeout (gpointer user_data)
{
  guint iteration = GPOINTER_TO_UINT (user_data);
  g_printerr ("Timeout waiting 1000 msec on iteration %d\n", iteration);
  g_assert_not_reached ();
  return FALSE;
}

static void
test_connection_flush (void)
{
  GDBusConnection *connection;
  GError *error;
  guint n;
  guint signal_handler_id;
  const gchar *flush_helper;

  session_bus_up ();

  error = NULL;
  connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
  g_assert_no_error (error);
  g_assert (connection != NULL);

  signal_handler_id = g_dbus_connection_signal_subscribe (connection,
                                                          NULL, /* sender */
                                                          "org.gtk.GDBus.FlushInterface",
                                                          "SomeSignal",
                                                          "/org/gtk/GDBus/FlushObject",
                                                          NULL,
                                                          G_DBUS_SIGNAL_FLAGS_NONE,
                                                          test_connection_flush_signal_handler,
                                                          NULL,
                                                          NULL);
  g_assert_cmpint (signal_handler_id, !=, 0);

  flush_helper = g_test_get_filename (G_TEST_BUILT, "gdbus-connection-flush-helper", NULL);
  for (n = 0; n < 50; n++)
    {
      gboolean ret;
      gint exit_status;
      guint timeout_mainloop_id;

      error = NULL;
      ret = g_spawn_command_line_sync (flush_helper,
                                       NULL, /* stdout */
                                       NULL, /* stderr */
                                       &exit_status,
                                       &error);
      g_assert_no_error (error);
      g_spawn_check_exit_status (exit_status, &error);
      g_assert_no_error (error);
      g_assert (ret);

      timeout_mainloop_id = g_timeout_add (1000, test_connection_flush_on_timeout, GUINT_TO_POINTER (n));
      g_main_loop_run (loop);
      g_source_remove (timeout_mainloop_id);
    }

  g_dbus_connection_signal_unsubscribe (connection, signal_handler_id);
  g_object_unref (connection);

  session_bus_down ();
}

/* ---------------------------------------------------------------------------------------------------- */

/* Message size > 20MiB ... should be enough to make sure the message
 * is fragmented when shoved across any transport
 */
#define LARGE_MESSAGE_STRING_LENGTH (20*1024*1024)
/* the test will fail if the service name has not appeared after this amount of seconds */
#define LARGE_MESSAGE_TIMEOUT_SECONDS 10

static gboolean
large_message_timeout_cb (gpointer data)
{
  (void)data;

  g_error ("Error: timeout waiting for dbus name to appear\n");

  return FALSE;
}

static void
large_message_on_name_appeared (GDBusConnection *connection,
                                const gchar *name,
                                const gchar *name_owner,
                                gpointer user_data)
{
  GError *error;
  gchar *request;
  const gchar *reply;
  GVariant *result;
  guint n;

  g_assert (g_source_remove (GPOINTER_TO_UINT (user_data)));

  request = g_new (gchar, LARGE_MESSAGE_STRING_LENGTH + 1);
  for (n = 0; n < LARGE_MESSAGE_STRING_LENGTH; n++)
    request[n] = '0' + (n%10);
  request[n] = '\0';

  error = NULL;
  result = g_dbus_connection_call_sync (connection,
                                        "com.example.TestService",      /* bus name */
                                        "/com/example/TestObject",      /* object path */
                                        "com.example.Frob",             /* interface name */
                                        "HelloWorld",                   /* method name */
                                        g_variant_new ("(s)", request), /* parameters */
                                        G_VARIANT_TYPE ("(s)"),         /* return type */
                                        G_DBUS_CALL_FLAGS_NONE,
                                        G_MAXINT,
                                        NULL,
                                        &error);
  g_assert_no_error (error);
  g_assert (result != NULL);
  g_variant_get (result, "(&s)", &reply);
  g_assert_cmpint (strlen (reply), >, LARGE_MESSAGE_STRING_LENGTH);
  g_assert (g_str_has_prefix (reply, "You greeted me with '01234567890123456789012"));
  g_assert (g_str_has_suffix (reply, "6789'. Thanks!"));
  g_variant_unref (result);

  g_free (request);

  g_main_loop_quit (loop);
}

static void
large_message_on_name_vanished (GDBusConnection *connection,
                                const gchar *name,
                                gpointer user_data)
{
}

static void
test_connection_large_message (void)
{
  guint watcher_id;
  guint timeout_id;

  session_bus_up ();

  /* this is safe; testserver will exit once the bus goes away */
  g_assert (g_spawn_command_line_async (g_test_get_filename (G_TEST_BUILT, "gdbus-testserver", NULL), NULL));

  timeout_id = g_timeout_add_seconds (LARGE_MESSAGE_TIMEOUT_SECONDS,
                                      large_message_timeout_cb,
                                      NULL);

  watcher_id = g_bus_watch_name (G_BUS_TYPE_SESSION,
                                 "com.example.TestService",
                                 G_BUS_NAME_WATCHER_FLAGS_NONE,
                                 large_message_on_name_appeared,
                                 large_message_on_name_vanished,
                                 GUINT_TO_POINTER (timeout_id),  /* user_data */
                                 NULL); /* GDestroyNotify */
  g_main_loop_run (loop);
  g_bus_unwatch_name (watcher_id);

  session_bus_down ();
}

/* ---------------------------------------------------------------------------------------------------- */

int
main (int   argc,
      char *argv[])
{
  gint ret;

  g_test_init (&argc, &argv, NULL);

  /* all the tests rely on a shared main loop */
  loop = g_main_loop_new (NULL, FALSE);

  g_test_dbus_unset ();

  g_test_add_func ("/gdbus/connection/flush", test_connection_flush);
  g_test_add_func ("/gdbus/connection/large_message", test_connection_large_message);

  ret = g_test_run();
  g_main_loop_unref (loop);
  return ret;
}
