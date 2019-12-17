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

#include "gdbus-tests.h"

/* all tests rely on a global connection */
static GDBusConnection *c = NULL;

/* all tests rely on a shared mainloop */
static GMainLoop *loop = NULL;

/* ---------------------------------------------------------------------------------------------------- */
/* Check that pending calls fail with G_IO_ERROR_CLOSED if the connection is closed  */
/* See https://bugzilla.gnome.org/show_bug.cgi?id=660637 */
/* ---------------------------------------------------------------------------------------------------- */

static void
sleep_cb (GObject      *source_object,
          GAsyncResult *res,
          gpointer      user_data)
{
  GError **error = user_data;
  GVariant *result;

  result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object), res, error);
  g_assert (result == NULL);
  g_main_loop_quit (loop);
}

static gboolean
on_timeout (gpointer user_data)
{
  /* tear down bus */
  session_bus_stop ();
  return FALSE; /* remove source */
}

static void
test_connection_loss (void)
{
  GDBusProxy *proxy;
  GError *error;

  error = NULL;
  proxy = g_dbus_proxy_new_sync (c,
                                 G_DBUS_PROXY_FLAGS_NONE,
                                 NULL,                      /* GDBusInterfaceInfo */
                                 "com.example.TestService", /* name */
                                 "/com/example/TestObject", /* object path */
                                 "com.example.Frob",        /* interface */
                                 NULL, /* GCancellable */
                                 &error);
  g_assert_no_error (error);

  error = NULL;
  g_dbus_proxy_call (proxy,
                     "Sleep",
                     g_variant_new ("(i)", 100 * 1000 /* msec */),
                     G_DBUS_CALL_FLAGS_NONE,
                     10 * 1000 /* msec */,
                     NULL, /* GCancellable */
                     sleep_cb,
                     &error);

  /* Make sure we don't exit when the bus goes away */
  g_dbus_connection_set_exit_on_close (c, FALSE);

  /* Nuke the connection to the bus */
  g_timeout_add (100 /* ms */, on_timeout, NULL);

  g_main_loop_run (loop);

  /* If we didn't act on connection-loss we'd be getting G_IO_ERROR_TIMEOUT
   * generated locally. So if we get G_IO_ERROR_CLOSED it means that we
   * are acting correctly on connection loss.
   */
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_CLOSED);
  g_assert (!g_dbus_error_is_remote_error (error));
  g_clear_error (&error);

  g_object_unref (proxy);
}

/* ---------------------------------------------------------------------------------------------------- */

int
main (int   argc,
      char *argv[])
{
  GError *error;
  gint ret;
  gchar *path;

  g_test_init (&argc, &argv, NULL);

  /* all the tests rely on a shared main loop */
  loop = g_main_loop_new (NULL, FALSE);

  session_bus_up ();

  /* this is safe; testserver will exit once the bus goes away */
  path = g_test_build_filename (G_TEST_BUILT, "gdbus-testserver", NULL);
  g_assert (g_spawn_command_line_async (path, NULL));
  g_free (path);

  ensure_gdbus_testserver_up ();

  /* Create the connection in the main thread */
  error = NULL;
  c = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
  g_assert_no_error (error);
  g_assert (c != NULL);

  g_test_add_func ("/gdbus/connection-loss", test_connection_loss);

  ret = g_test_run();

  g_object_unref (c);

  return ret;
}
