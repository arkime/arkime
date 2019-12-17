/* Test case for GNOME #651133
 *
 * Copyright (C) 2008-2010 Red Hat, Inc.
 * Copyright (C) 2011 Nokia Corporation
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
 * Author: Simon McVittie <simon.mcvittie@collabora.co.uk>
 */

#include <config.h>

#include <unistd.h>
#include <string.h>

#include <gio/gio.h>

#include "gdbus-tests.h"

#ifdef HAVE_DBUS1
# include <dbus/dbus-shared.h>
#else
# define DBUS_INTERFACE_DBUS "org.freedesktop.DBus"
# define DBUS_PATH_DBUS "/org/freedesktop/DBus"
# define DBUS_SERVICE_DBUS "org.freedesktop.DBus"
# define DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER 1
# define DBUS_RELEASE_NAME_REPLY_RELEASED 1
#endif

#define MY_NAME "com.example.Test.Myself"
/* This many threads create and destroy GDBusProxy instances, in addition
 * to the main thread processing their NameOwnerChanged signals.
 * N_THREADS_MAX is used with "-m slow", N_THREADS otherwise.
 */
#define N_THREADS_MAX 10
#define N_THREADS 2
/* This many GDBusProxy instances are created by each thread. */
#define N_REPEATS 100
/* The main thread requests/releases a name this many times as rapidly as
 * possible, before performing one "slow" cycle that waits for each method
 * call result (and therefore, due to D-Bus total ordering, all previous
 * method calls) to prevent requests from piling up infinitely. The more calls
 * are made rapidly, the better we reproduce bugs.
 */
#define N_RAPID_CYCLES 50

static GMainLoop *loop;

static gpointer
run_proxy_thread (gpointer data)
{
  GDBusConnection *connection = data;
  int i;

  g_assert (g_main_context_get_thread_default () == NULL);

  for (i = 0; i < N_REPEATS; i++)
    {
      GDBusProxy *proxy;
      GError *error = NULL;
      GVariant *ret;

      if (g_test_verbose ())
        g_printerr (".");

      proxy = g_dbus_proxy_new_sync (connection,
                                     G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START |
                                     G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES,
                                     NULL,
                                     MY_NAME,
                                     "/com/example/TestObject",
                                     "com.example.Frob",
                                     NULL,
                                     &error);
      g_assert_no_error (error);
      g_assert (proxy != NULL);
      g_dbus_proxy_set_default_timeout (proxy, G_MAXINT);

      ret = g_dbus_proxy_call_sync (proxy, "StupidMethod", NULL,
                                    G_DBUS_CALL_FLAGS_NO_AUTO_START, -1,
                                    NULL, NULL);
      /*
       * we expect this to fail - if we have the name at the moment, we called
       * an unimplemented method, and if not, there was nothing to call
       */
      g_assert (ret == NULL);

      /*
       * this races with the NameOwnerChanged signal being emitted in an
       * idle
       */
      g_object_unref (proxy);
    }

  g_main_loop_quit (loop);
  return NULL;
}

static void release_name (GDBusConnection *connection, gboolean wait);

static void
request_name_cb (GObject *source,
                 GAsyncResult *res,
                 gpointer user_data)
{
  GDBusConnection *connection = G_DBUS_CONNECTION (source);
  GError *error = NULL;
  GVariant *var;

  var = g_dbus_connection_call_finish (connection, res, &error);
  g_assert_no_error (error);
  g_assert_cmpuint (g_variant_get_uint32 (g_variant_get_child_value (var, 0)),
                    ==, DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER);

  release_name (connection, TRUE);
}

static void
request_name (GDBusConnection *connection,
              gboolean         wait)
{
  g_dbus_connection_call (connection,
                          DBUS_SERVICE_DBUS,
                          DBUS_PATH_DBUS,
                          DBUS_INTERFACE_DBUS,
                          "RequestName",
                          g_variant_new ("(su)", MY_NAME, 0),
                          G_VARIANT_TYPE ("(u)"),
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          NULL,
                          wait ? request_name_cb : NULL,
                          NULL);
}

static void
release_name_cb (GObject *source,
                 GAsyncResult *res,
                 gpointer user_data)
{
  GDBusConnection *connection = G_DBUS_CONNECTION (source);
  GError *error = NULL;
  GVariant *var;
  int i;

  var = g_dbus_connection_call_finish (connection, res, &error);
  g_assert_no_error (error);
  g_assert_cmpuint (g_variant_get_uint32 (g_variant_get_child_value (var, 0)),
                    ==, DBUS_RELEASE_NAME_REPLY_RELEASED);

  /* generate some rapid NameOwnerChanged signals to try to trigger crashes */
  for (i = 0; i < N_RAPID_CYCLES; i++)
    {
      request_name (connection, FALSE);
      release_name (connection, FALSE);
    }

  /* wait for dbus-daemon to catch up */
  request_name (connection, TRUE);
}

static void
release_name (GDBusConnection *connection,
              gboolean         wait)
{
  g_dbus_connection_call (connection,
                          DBUS_SERVICE_DBUS,
                          DBUS_PATH_DBUS,
                          DBUS_INTERFACE_DBUS,
                          "ReleaseName",
                          g_variant_new ("(s)", MY_NAME),
                          G_VARIANT_TYPE ("(u)"),
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          NULL,
                          wait ? release_name_cb : NULL,
                          NULL);
}

static void
test_proxy (void)
{
  GDBusConnection *connection;
  GError *error = NULL;
  GThread *proxy_threads[N_THREADS_MAX];
  int i;
  int n_threads;

  if (g_test_slow ())
    n_threads = N_THREADS_MAX;
  else
    n_threads = N_THREADS;

  session_bus_up ();

  loop = g_main_loop_new (NULL, TRUE);

  connection = g_bus_get_sync (G_BUS_TYPE_SESSION,
                               NULL,
                               &error);
  g_assert_no_error (error);

  request_name (connection, TRUE);

  for (i = 0; i < n_threads; i++)
    {
      proxy_threads[i] = g_thread_new ("run-proxy",
                                       run_proxy_thread, connection);
    }

  g_main_loop_run (loop);

  for (i = 0; i < n_threads; i++)
    {
      g_thread_join (proxy_threads[i]);
    }

  g_object_unref (connection);
  g_main_loop_unref (loop);

  /* TODO: should call session_bus_down() but that requires waiting
   * for all the oustanding method calls to complete...
   */
  if (g_test_verbose ())
    g_printerr ("\n");
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_dbus_unset ();

  g_test_add_func ("/gdbus/proxy/vs-threads", test_proxy);

  return g_test_run();
}
