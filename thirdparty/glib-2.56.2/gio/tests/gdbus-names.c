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

#include "gdbus-tests.h"

/* all tests rely on a shared mainloop */
static GMainLoop *loop;

/* ---------------------------------------------------------------------------------------------------- */
/* Test that g_bus_own_name() works correctly */
/* ---------------------------------------------------------------------------------------------------- */

typedef struct
{
  GMainLoop *loop;
  gboolean expect_null_connection;
  guint num_bus_acquired;
  guint num_acquired;
  guint num_lost;
  guint num_free_func;
} OwnNameData;

static void
own_name_data_free_func (OwnNameData *data)
{
  data->num_free_func++;
  g_main_loop_quit (loop);
}

static void
bus_acquired_handler (GDBusConnection *connection,
                      const gchar     *name,
                      gpointer         user_data)
{
  OwnNameData *data = user_data;
  g_dbus_connection_set_exit_on_close (connection, FALSE);
  data->num_bus_acquired += 1;
  g_main_loop_quit (loop);
}

static void
name_acquired_handler (GDBusConnection *connection,
                       const gchar     *name,
                       gpointer         user_data)
{
  OwnNameData *data = user_data;
  data->num_acquired += 1;
  g_main_loop_quit (loop);
}

static void
name_lost_handler (GDBusConnection *connection,
                   const gchar     *name,
                   gpointer         user_data)
{
  OwnNameData *data = user_data;
  if (data->expect_null_connection)
    {
      g_assert (connection == NULL);
    }
  else
    {
      g_assert (connection != NULL);
      g_dbus_connection_set_exit_on_close (connection, FALSE);
    }
  data->num_lost += 1;
  g_main_loop_quit (loop);
}

static void
test_bus_own_name (void)
{
  guint id;
  guint id2;
  OwnNameData data;
  OwnNameData data2;
  const gchar *name;
  GDBusConnection *c;
  GError *error;
  gboolean name_has_owner_reply;
  GDBusConnection *c2;
  GVariant *result;

  error = NULL;
  name = "org.gtk.GDBus.Name1";

  /*
   * First check that name_lost_handler() is invoked if there is no bus.
   *
   * Also make sure name_lost_handler() isn't invoked when unowning the name.
   */
  data.num_bus_acquired = 0;
  data.num_free_func = 0;
  data.num_acquired = 0;
  data.num_lost = 0;
  data.expect_null_connection = TRUE;
  id = g_bus_own_name (G_BUS_TYPE_SESSION,
                       name,
                       G_BUS_NAME_OWNER_FLAGS_NONE,
                       bus_acquired_handler,
                       name_acquired_handler,
                       name_lost_handler,
                       &data,
                       (GDestroyNotify) own_name_data_free_func);
  g_assert_cmpint (data.num_bus_acquired, ==, 0);
  g_assert_cmpint (data.num_acquired, ==, 0);
  g_assert_cmpint (data.num_lost,     ==, 0);
  g_main_loop_run (loop);
  g_assert_cmpint (data.num_bus_acquired, ==, 0);
  g_assert_cmpint (data.num_acquired, ==, 0);
  g_assert_cmpint (data.num_lost,     ==, 1);
  g_bus_unown_name (id);
  g_assert_cmpint (data.num_acquired, ==, 0);
  g_assert_cmpint (data.num_lost,     ==, 1);
  g_assert_cmpint (data.num_free_func, ==, 1);

  /*
   * Bring up a bus, then own a name and check bus_acquired_handler() then name_acquired_handler() is invoked.
   */
  session_bus_up ();
  data.num_bus_acquired = 0;
  data.num_acquired = 0;
  data.num_lost = 0;
  data.expect_null_connection = FALSE;
  id = g_bus_own_name (G_BUS_TYPE_SESSION,
                       name,
                       G_BUS_NAME_OWNER_FLAGS_NONE,
                       bus_acquired_handler,
                       name_acquired_handler,
                       name_lost_handler,
                       &data,
                       (GDestroyNotify) own_name_data_free_func);
  g_assert_cmpint (data.num_bus_acquired, ==, 0);
  g_assert_cmpint (data.num_acquired, ==, 0);
  g_assert_cmpint (data.num_lost,     ==, 0);
  g_main_loop_run (loop);
  g_assert_cmpint (data.num_bus_acquired, ==, 1);
  g_assert_cmpint (data.num_acquired, ==, 0);
  g_assert_cmpint (data.num_lost,     ==, 0);
  g_main_loop_run (loop);
  g_assert_cmpint (data.num_bus_acquired, ==, 1);
  g_assert_cmpint (data.num_acquired, ==, 1);
  g_assert_cmpint (data.num_lost,     ==, 0);

  /*
   * Check that the name was actually acquired.
   */
  c = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
  g_assert (c != NULL);
  g_assert (!g_dbus_connection_is_closed (c));
  result = g_dbus_connection_call_sync (c,
                                        "org.freedesktop.DBus",  /* bus name */
                                        "/org/freedesktop/DBus", /* object path */
                                        "org.freedesktop.DBus",  /* interface name */
                                        "NameHasOwner",          /* method name */
                                        g_variant_new ("(s)", name),
                                        G_VARIANT_TYPE ("(b)"),
                                        G_DBUS_CALL_FLAGS_NONE,
                                        -1,
                                        NULL,
                                        &error);
  g_assert_no_error (error);
  g_assert (result != NULL);
  g_variant_get (result, "(b)", &name_has_owner_reply);
  g_assert (name_has_owner_reply);
  g_variant_unref (result);

  /*
   * Stop owning the name - this should invoke our free func
   */
  g_bus_unown_name (id);
  g_assert_cmpint (data.num_free_func, ==, 2);

  /*
   * Check that the name was actually released.
   */
  result = g_dbus_connection_call_sync (c,
                                        "org.freedesktop.DBus",  /* bus name */
                                        "/org/freedesktop/DBus", /* object path */
                                        "org.freedesktop.DBus",  /* interface name */
                                        "NameHasOwner",          /* method name */
                                        g_variant_new ("(s)", name),
                                        G_VARIANT_TYPE ("(b)"),
                                        G_DBUS_CALL_FLAGS_NONE,
                                        -1,
                                        NULL,
                                        &error);
  g_assert_no_error (error);
  g_assert (result != NULL);
  g_variant_get (result, "(b)", &name_has_owner_reply);
  g_assert (!name_has_owner_reply);
  g_variant_unref (result);

  /* Now try owning the name and then immediately decide to unown the name */
  g_assert_cmpint (data.num_bus_acquired, ==, 1);
  g_assert_cmpint (data.num_acquired, ==, 1);
  g_assert_cmpint (data.num_lost,     ==, 0);
  g_assert_cmpint (data.num_free_func, ==, 2);
  id = g_bus_own_name (G_BUS_TYPE_SESSION,
                       name,
                       G_BUS_NAME_OWNER_FLAGS_NONE,
                       bus_acquired_handler,
                       name_acquired_handler,
                       name_lost_handler,
                       &data,
                       (GDestroyNotify) own_name_data_free_func);
  g_assert_cmpint (data.num_bus_acquired, ==, 1);
  g_assert_cmpint (data.num_acquired, ==, 1);
  g_assert_cmpint (data.num_lost,     ==, 0);
  g_assert_cmpint (data.num_free_func, ==, 2);
  g_bus_unown_name (id);
  g_assert_cmpint (data.num_bus_acquired, ==, 1);
  g_assert_cmpint (data.num_acquired, ==, 1);
  g_assert_cmpint (data.num_lost,     ==, 0);
  g_assert_cmpint (data.num_free_func, ==, 2);
  g_main_loop_run (loop); /* the GDestroyNotify is called in idle because the bus is acquired in idle */
  g_assert_cmpint (data.num_free_func, ==, 3);

  /*
   * Own the name again.
   */
  data.num_bus_acquired = 0;
  data.num_acquired = 0;
  data.num_lost = 0;
  data.expect_null_connection = FALSE;
  id = g_bus_own_name_with_closures (G_BUS_TYPE_SESSION,
                                     name,
                                     G_BUS_NAME_OWNER_FLAGS_NONE,
                                     g_cclosure_new (G_CALLBACK (bus_acquired_handler),
                                                     &data,
                                                     NULL),
                                     g_cclosure_new (G_CALLBACK (name_acquired_handler),
                                                     &data,
                                                     NULL),
                                     g_cclosure_new (G_CALLBACK (name_lost_handler),
                                                     &data,
                                                     (GClosureNotify) own_name_data_free_func));
  g_assert_cmpint (data.num_bus_acquired, ==, 0);
  g_assert_cmpint (data.num_acquired, ==, 0);
  g_assert_cmpint (data.num_lost,     ==, 0);
  g_main_loop_run (loop);
  g_assert_cmpint (data.num_bus_acquired, ==, 1);
  g_assert_cmpint (data.num_acquired, ==, 0);
  g_assert_cmpint (data.num_lost,     ==, 0);
  g_main_loop_run (loop);
  g_assert_cmpint (data.num_bus_acquired, ==, 1);
  g_assert_cmpint (data.num_acquired, ==, 1);
  g_assert_cmpint (data.num_lost,     ==, 0);

  /*
   * Try owning the name with another object on the same connection  - this should
   * fail because we already own the name.
   */
  data2.num_free_func = 0;
  data2.num_bus_acquired = 0;
  data2.num_acquired = 0;
  data2.num_lost = 0;
  data2.expect_null_connection = FALSE;
  id2 = g_bus_own_name (G_BUS_TYPE_SESSION,
                        name,
                        G_BUS_NAME_OWNER_FLAGS_NONE,
                        bus_acquired_handler,
                        name_acquired_handler,
                        name_lost_handler,
                        &data2,
                        (GDestroyNotify) own_name_data_free_func);
  g_assert_cmpint (data2.num_bus_acquired, ==, 0);
  g_assert_cmpint (data2.num_acquired, ==, 0);
  g_assert_cmpint (data2.num_lost,     ==, 0);
  g_main_loop_run (loop);
  g_assert_cmpint (data2.num_bus_acquired, ==, 1);
  g_assert_cmpint (data2.num_acquired, ==, 0);
  g_assert_cmpint (data2.num_lost,     ==, 0);
  g_main_loop_run (loop);
  g_assert_cmpint (data2.num_bus_acquired, ==, 1);
  g_assert_cmpint (data2.num_acquired, ==, 0);
  g_assert_cmpint (data2.num_lost,     ==, 1);
  g_bus_unown_name (id2);
  g_assert_cmpint (data2.num_bus_acquired, ==, 1);
  g_assert_cmpint (data2.num_acquired, ==, 0);
  g_assert_cmpint (data2.num_lost,     ==, 1);
  g_assert_cmpint (data2.num_free_func, ==, 1);

  /*
   * Create a secondary (e.g. private) connection and try owning the name on that
   * connection. This should fail both with and without _REPLACE because we
   * didn't specify ALLOW_REPLACEMENT.
   */
  c2 = _g_bus_get_priv (G_BUS_TYPE_SESSION, NULL, NULL);
  g_assert (c2 != NULL);
  g_assert (!g_dbus_connection_is_closed (c2));
  /* first without _REPLACE */
  data2.num_bus_acquired = 0;
  data2.num_acquired = 0;
  data2.num_lost = 0;
  data2.expect_null_connection = FALSE;
  data2.num_free_func = 0;
  id2 = g_bus_own_name_on_connection (c2,
                                      name,
                                      G_BUS_NAME_OWNER_FLAGS_NONE,
                                      name_acquired_handler,
                                      name_lost_handler,
                                      &data2,
                                      (GDestroyNotify) own_name_data_free_func);
  g_assert_cmpint (data2.num_bus_acquired, ==, 0);
  g_assert_cmpint (data2.num_acquired, ==, 0);
  g_assert_cmpint (data2.num_lost,     ==, 0);
  g_main_loop_run (loop);
  g_assert_cmpint (data2.num_bus_acquired, ==, 0);
  g_assert_cmpint (data2.num_acquired, ==, 0);
  g_assert_cmpint (data2.num_lost,     ==, 1);
  g_bus_unown_name (id2);
  g_assert_cmpint (data2.num_bus_acquired, ==, 0);
  g_assert_cmpint (data2.num_acquired, ==, 0);
  g_assert_cmpint (data2.num_lost,     ==, 1);
  g_assert_cmpint (data2.num_free_func, ==, 1);
  /* then with _REPLACE */
  data2.num_bus_acquired = 0;
  data2.num_acquired = 0;
  data2.num_lost = 0;
  data2.expect_null_connection = FALSE;
  data2.num_free_func = 0;
  id2 = g_bus_own_name_on_connection (c2,
                                      name,
                                      G_BUS_NAME_OWNER_FLAGS_REPLACE,
                                      name_acquired_handler,
                                      name_lost_handler,
                                      &data2,
                                      (GDestroyNotify) own_name_data_free_func);
  g_assert_cmpint (data2.num_bus_acquired, ==, 0);
  g_assert_cmpint (data2.num_acquired, ==, 0);
  g_assert_cmpint (data2.num_lost,     ==, 0);
  g_main_loop_run (loop);
  g_assert_cmpint (data2.num_bus_acquired, ==, 0);
  g_assert_cmpint (data2.num_acquired, ==, 0);
  g_assert_cmpint (data2.num_lost,     ==, 1);
  g_bus_unown_name (id2);
  g_assert_cmpint (data2.num_bus_acquired, ==, 0);
  g_assert_cmpint (data2.num_acquired, ==, 0);
  g_assert_cmpint (data2.num_lost,     ==, 1);
  g_assert_cmpint (data2.num_free_func, ==, 1);

  /*
   * Stop owning the name and grab it again with _ALLOW_REPLACEMENT.
   */
  data.expect_null_connection = FALSE;
  g_bus_unown_name (id);
  g_assert_cmpint (data.num_bus_acquired, ==, 1);
  g_assert_cmpint (data.num_acquired, ==, 1);
  g_assert_cmpint (data.num_free_func, ==, 4);
  /* grab it again */
  data.num_bus_acquired = 0;
  data.num_acquired = 0;
  data.num_lost = 0;
  data.expect_null_connection = FALSE;
  id = g_bus_own_name (G_BUS_TYPE_SESSION,
                       name,
                       G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT,
                       bus_acquired_handler,
                       name_acquired_handler,
                       name_lost_handler,
                       &data,
                       (GDestroyNotify) own_name_data_free_func);
  g_assert_cmpint (data.num_bus_acquired, ==, 0);
  g_assert_cmpint (data.num_acquired, ==, 0);
  g_assert_cmpint (data.num_lost,     ==, 0);
  g_main_loop_run (loop);
  g_assert_cmpint (data.num_bus_acquired, ==, 1);
  g_assert_cmpint (data.num_acquired, ==, 0);
  g_assert_cmpint (data.num_lost,     ==, 0);
  g_main_loop_run (loop);
  g_assert_cmpint (data.num_bus_acquired, ==, 1);
  g_assert_cmpint (data.num_acquired, ==, 1);
  g_assert_cmpint (data.num_lost,     ==, 0);

  /*
   * Now try to grab the name from the secondary connection.
   *
   */
  /* first without _REPLACE - this won't make us acquire the name */
  data2.num_bus_acquired = 0;
  data2.num_acquired = 0;
  data2.num_lost = 0;
  data2.expect_null_connection = FALSE;
  data2.num_free_func = 0;
  id2 = g_bus_own_name_on_connection (c2,
                                      name,
                                      G_BUS_NAME_OWNER_FLAGS_NONE,
                                      name_acquired_handler,
                                      name_lost_handler,
                                      &data2,
                                      (GDestroyNotify) own_name_data_free_func);
  g_assert_cmpint (data2.num_bus_acquired, ==, 0);
  g_assert_cmpint (data2.num_acquired, ==, 0);
  g_assert_cmpint (data2.num_lost,     ==, 0);
  g_main_loop_run (loop);
  g_assert_cmpint (data2.num_bus_acquired, ==, 0);
  g_assert_cmpint (data2.num_acquired, ==, 0);
  g_assert_cmpint (data2.num_lost,     ==, 1);
  g_bus_unown_name (id2);
  g_assert_cmpint (data2.num_bus_acquired, ==, 0);
  g_assert_cmpint (data2.num_acquired, ==, 0);
  g_assert_cmpint (data2.num_lost,     ==, 1);
  g_assert_cmpint (data2.num_free_func, ==, 1);
  /* then with _REPLACE - here we should acquire the name - e.g. owner should lose it
   * and owner2 should acquire it  */
  data2.num_bus_acquired = 0;
  data2.num_acquired = 0;
  data2.num_lost = 0;
  data2.expect_null_connection = FALSE;
  data2.num_free_func = 0;
  id2 = g_bus_own_name_on_connection (c2,
                                      name,
                                      G_BUS_NAME_OWNER_FLAGS_REPLACE,
                                      name_acquired_handler,
                                      name_lost_handler,
                                      &data2,
                                      (GDestroyNotify) own_name_data_free_func);
  g_assert_cmpint (data.num_acquired, ==, 1);
  g_assert_cmpint (data.num_lost,     ==, 0);
  g_assert_cmpint (data2.num_acquired, ==, 0);
  g_assert_cmpint (data2.num_lost,     ==, 0);
  /* wait for handlers for both owner and owner2 to fire */
  while (data.num_lost == 0 || data2.num_acquired == 0)
    g_main_loop_run (loop);
  g_assert_cmpint (data.num_acquired, ==, 1);
  g_assert_cmpint (data.num_lost,     ==, 1);
  g_assert_cmpint (data2.num_acquired, ==, 1);
  g_assert_cmpint (data2.num_lost,     ==, 0);
  g_assert_cmpint (data2.num_bus_acquired, ==, 0);
  /* ok, make owner2 release the name - then wait for owner to automagically reacquire it */
  g_bus_unown_name (id2);
  g_assert_cmpint (data2.num_free_func, ==, 1);
  g_main_loop_run (loop);
  g_assert_cmpint (data.num_acquired, ==, 2);
  g_assert_cmpint (data.num_lost,     ==, 1);

  /*
   * Finally, nuke the bus and check name_lost_handler() is invoked.
   *
   */
  data.expect_null_connection = TRUE;
  session_bus_stop ();
  while (data.num_lost != 2)
    g_main_loop_run (loop);
  g_assert_cmpint (data.num_acquired, ==, 2);
  g_assert_cmpint (data.num_lost,     ==, 2);
  g_bus_unown_name (id);
  g_assert_cmpint (data.num_free_func, ==, 5);

  g_object_unref (c);
  g_object_unref (c2);

  session_bus_down ();

  /* See https://bugzilla.gnome.org/show_bug.cgi?id=711807 */
  g_usleep (1000000);
}

/* ---------------------------------------------------------------------------------------------------- */
/* Test that g_bus_watch_name() works correctly */
/* ---------------------------------------------------------------------------------------------------- */

typedef struct
{
  gboolean expect_null_connection;
  guint num_acquired;
  guint num_lost;
  guint num_appeared;
  guint num_vanished;
  guint num_free_func;
} WatchNameData;

static void
watch_name_data_free_func (WatchNameData *data)
{
  data->num_free_func++;
  g_main_loop_quit (loop);
}

static void
w_bus_acquired_handler (GDBusConnection *connection,
                        const gchar     *name,
                        gpointer         user_data)
{
}

static void
w_name_acquired_handler (GDBusConnection *connection,
                         const gchar     *name,
                         gpointer         user_data)
{
  WatchNameData *data = user_data;
  data->num_acquired += 1;
  g_main_loop_quit (loop);
}

static void
w_name_lost_handler (GDBusConnection *connection,
                     const gchar     *name,
                     gpointer         user_data)
{
  WatchNameData *data = user_data;
  data->num_lost += 1;
  g_main_loop_quit (loop);
}

static void
name_appeared_handler (GDBusConnection *connection,
                       const gchar     *name,
                       const gchar     *name_owner,
                       gpointer         user_data)
{
  WatchNameData *data = user_data;
  if (data->expect_null_connection)
    {
      g_assert (connection == NULL);
    }
  else
    {
      g_assert (connection != NULL);
      g_dbus_connection_set_exit_on_close (connection, FALSE);
    }
  data->num_appeared += 1;
  g_main_loop_quit (loop);
}

static void
name_vanished_handler (GDBusConnection *connection,
                       const gchar     *name,
                       gpointer         user_data)
{
  WatchNameData *data = user_data;
  if (data->expect_null_connection)
    {
      g_assert (connection == NULL);
    }
  else
    {
      g_assert (connection != NULL);
      g_dbus_connection_set_exit_on_close (connection, FALSE);
    }
  data->num_vanished += 1;
  g_main_loop_quit (loop);
}

static void
test_bus_watch_name (void)
{
  WatchNameData data;
  guint id;
  guint owner_id;
  GDBusConnection *connection;

  /*
   * First check that name_vanished_handler() is invoked if there is no bus.
   *
   * Also make sure name_vanished_handler() isn't invoked when unwatching the name.
   */
  data.num_free_func = 0;
  data.num_appeared = 0;
  data.num_vanished = 0;
  data.expect_null_connection = TRUE;
  id = g_bus_watch_name (G_BUS_TYPE_SESSION,
                         "org.gtk.GDBus.Name1",
                         G_BUS_NAME_WATCHER_FLAGS_NONE,
                         name_appeared_handler,
                         name_vanished_handler,
                         &data,
                         (GDestroyNotify) watch_name_data_free_func);
  g_assert_cmpint (data.num_appeared, ==, 0);
  g_assert_cmpint (data.num_vanished, ==, 0);
  g_main_loop_run (loop);
  g_assert_cmpint (data.num_appeared, ==, 0);
  g_assert_cmpint (data.num_vanished, ==, 1);
  g_bus_unwatch_name (id);
  g_assert_cmpint (data.num_appeared, ==, 0);
  g_assert_cmpint (data.num_vanished, ==, 1);
  g_assert_cmpint (data.num_free_func, ==, 1);

  /*
   * Now bring up a bus, own a name, and then start watching it.
   */
  session_bus_up ();
  /* own the name */
  data.num_free_func = 0;
  data.num_acquired = 0;
  data.num_lost = 0;
  data.expect_null_connection = FALSE;
  owner_id = g_bus_own_name (G_BUS_TYPE_SESSION,
                             "org.gtk.GDBus.Name1",
                             G_BUS_NAME_OWNER_FLAGS_NONE,
                             w_bus_acquired_handler,
                             w_name_acquired_handler,
                             w_name_lost_handler,
                             &data,
                             (GDestroyNotify) watch_name_data_free_func);
  g_main_loop_run (loop);
  g_assert_cmpint (data.num_acquired, ==, 1);
  g_assert_cmpint (data.num_lost,     ==, 0);

  connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
  g_assert (connection != NULL);

  /* now watch the name */
  data.num_appeared = 0;
  data.num_vanished = 0;
  id = g_bus_watch_name_on_connection (connection,
                                       "org.gtk.GDBus.Name1",
                                       G_BUS_NAME_WATCHER_FLAGS_NONE,
                                       name_appeared_handler,
                                       name_vanished_handler,
                                       &data,
                                       (GDestroyNotify) watch_name_data_free_func);
  g_assert_cmpint (data.num_appeared, ==, 0);
  g_assert_cmpint (data.num_vanished, ==, 0);
  g_main_loop_run (loop);
  g_assert_cmpint (data.num_appeared, ==, 1);
  g_assert_cmpint (data.num_vanished, ==, 0);

  /*
   * Unwatch the name.
   */
  g_bus_unwatch_name (id);
  g_assert_cmpint (data.num_free_func, ==, 1);

  g_object_unref (connection);

  /* unown the name */
  g_bus_unown_name (owner_id);
  g_assert_cmpint (data.num_acquired, ==, 1);
  g_assert_cmpint (data.num_free_func, ==, 2);

  /*
   * Create a watcher and then make a name be owned.
   *
   * This should trigger name_appeared_handler() ...
   */
  /* watch the name */
  data.num_appeared = 0;
  data.num_vanished = 0;
  data.num_free_func = 0;
  id = g_bus_watch_name_with_closures (G_BUS_TYPE_SESSION,
                                       "org.gtk.GDBus.Name1",
                                       G_BUS_NAME_WATCHER_FLAGS_NONE,
                                       g_cclosure_new (G_CALLBACK (name_appeared_handler),
                                                       &data,
                                                       NULL),
                                       g_cclosure_new (G_CALLBACK (name_vanished_handler),
                                                       &data,
                                                       (GClosureNotify) watch_name_data_free_func));
  g_assert_cmpint (data.num_appeared, ==, 0);
  g_assert_cmpint (data.num_vanished, ==, 0);
  g_main_loop_run (loop);
  g_assert_cmpint (data.num_appeared, ==, 0);
  g_assert_cmpint (data.num_vanished, ==, 1);

  /* own the name */
  data.num_acquired = 0;
  data.num_lost = 0;
  data.expect_null_connection = FALSE;
  owner_id = g_bus_own_name (G_BUS_TYPE_SESSION,
                             "org.gtk.GDBus.Name1",
                             G_BUS_NAME_OWNER_FLAGS_NONE,
                             w_bus_acquired_handler,
                             w_name_acquired_handler,
                             w_name_lost_handler,
                             &data,
                             (GDestroyNotify) watch_name_data_free_func);
  while (data.num_acquired == 0 || data.num_appeared == 0)
    g_main_loop_run (loop);
  g_assert_cmpint (data.num_acquired, ==, 1);
  g_assert_cmpint (data.num_lost,     ==, 0);
  g_assert_cmpint (data.num_appeared, ==, 1);
  g_assert_cmpint (data.num_vanished, ==, 1);

  /*
   * Nuke the bus and check that the name vanishes and is lost.
   */
  data.expect_null_connection = TRUE;
  session_bus_stop ();
  g_main_loop_run (loop);
  g_assert_cmpint (data.num_lost,     ==, 1);
  g_assert_cmpint (data.num_vanished, ==, 2);

  g_bus_unwatch_name (id);
  g_assert_cmpint (data.num_free_func, ==, 1);

  g_bus_unown_name (owner_id);
  g_assert_cmpint (data.num_free_func, ==, 2);

  session_bus_down ();
}

/* ---------------------------------------------------------------------------------------------------- */

static void
test_validate_names (void)
{
  guint n;
  static const struct
  {
    gboolean name;
    gboolean unique;
    gboolean interface;
    const gchar *string;
  } names[] = {
    { 1, 0, 1, "valid.well_known.name"},
    { 1, 0, 0, "valid.well-known.name"},
    { 1, 1, 0, ":valid.unique.name"},
    { 0, 0, 0, "invalid.5well_known.name"},
    { 0, 0, 0, "4invalid.5well_known.name"},
    { 1, 1, 0, ":4valid.5unique.name"},
    { 0, 0, 0, ""},
    { 1, 0, 1, "very.long.very.long.very.long.very.long.very.long.very.long.very.long.very.long.very.long.very.long.very.long.very.long.very.long.very.long.very.long.very.long.very.long.very.long.very.long.very.long.very.long.very.long.very.long.very.long.very.long.name1"}, /* 255 */
    { 0, 0, 0, "very.long.very.long.very.long.very.long.very.long.very.long.very.long.very.long.very.long.very.long.very.long.very.long.very.long.very.long.very.long.very.long.very.long.very.long.very.long.very.long.very.long.very.long.very.long.very.long.very.long.name12"}, /* 256 - too long! */
    { 0, 0, 0, ".starts.with.a.dot"},
    { 0, 0, 0, "contains.invalid;.characters"},
    { 0, 0, 0, "contains.inva/lid.characters"},
    { 0, 0, 0, "contains.inva[lid.characters"},
    { 0, 0, 0, "contains.inva]lid.characters"},
    { 0, 0, 0, "contains.inva_æøå_lid.characters"},
    { 1, 1, 0, ":1.1"},
  };

  for (n = 0; n < G_N_ELEMENTS (names); n++)
    {
      if (names[n].name)
        g_assert (g_dbus_is_name (names[n].string));
      else
        g_assert (!g_dbus_is_name (names[n].string));

      if (names[n].unique)
        g_assert (g_dbus_is_unique_name (names[n].string));
      else
        g_assert (!g_dbus_is_unique_name (names[n].string));

      if (names[n].interface)
        g_assert (g_dbus_is_interface_name (names[n].string));
      else
        g_assert (!g_dbus_is_interface_name (names[n].string));
    }
}

/* ---------------------------------------------------------------------------------------------------- */

int
main (int   argc,
      char *argv[])
{
  gint ret;

  g_test_init (&argc, &argv, NULL);

  loop = g_main_loop_new (NULL, FALSE);

  g_test_dbus_unset ();

  g_test_add_func ("/gdbus/validate-names", test_validate_names);
  g_test_add_func ("/gdbus/bus-own-name", test_bus_own_name);
  g_test_add_func ("/gdbus/bus-watch-name", test_bus_watch_name);

  ret = g_test_run();

  g_main_loop_unref (loop);

  return ret;
}
