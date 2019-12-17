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
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "gdbus-tests.h"

/* all tests rely on a shared mainloop */
static GMainLoop *loop = NULL;

/* ---------------------------------------------------------------------------------------------------- */

typedef struct {
    const gchar *name;
    const gchar *bug;
    enum {
        EXPLICITLY_FALSE = FALSE,
        EXPLICITLY_TRUE = TRUE,
        IMPLICITLY_TRUE
    } exit_on_close;
    enum {
        LOCAL,
        REMOTE
    } who_closes;
} TestData;

static const TestData cases[] = {
      { "default",  NULL,     IMPLICITLY_TRUE,  REMOTE },
      { "true",     NULL,     EXPLICITLY_TRUE,  REMOTE },
      { "false",    NULL,     EXPLICITLY_FALSE, REMOTE },
      { "we-close", "662100", EXPLICITLY_TRUE,  LOCAL  },
      { NULL }
};

static gboolean
quit_later_cb (gpointer data G_GNUC_UNUSED)
{
  g_main_loop_quit (loop);

  return FALSE;
}

static void
closed_cb (GDBusConnection  *c G_GNUC_UNUSED,
           gboolean          remote_peer_vanished,
           GError           *error,
           gpointer          test_data)
{
  const TestData *td = test_data;

  if (error == NULL)
    g_debug ("closed (%d, no error)", remote_peer_vanished);
  else
    g_debug ("closed (%d, %s %d \"%s\")", remote_peer_vanished,
             g_quark_to_string (error->domain), error->code, error->message);

  g_assert_cmpint (remote_peer_vanished, ==, (td->who_closes == REMOTE));
  g_assert_cmpint ((error == NULL), ==, (td->who_closes == LOCAL));

  /* we delay this so that if exit-on-close was going to happen, it will
   * win the race
   */
  g_timeout_add (50, quit_later_cb, NULL);
}

static void
close_async_cb (GObject *source G_GNUC_UNUSED,
                GAsyncResult *res G_GNUC_UNUSED,
                gpointer nil G_GNUC_UNUSED)
{
  GError *error = NULL;

  if (g_dbus_connection_close_finish (G_DBUS_CONNECTION (source),
                                      res,
                                      &error))
    {
      g_debug ("closed connection");
    }
  else
    {
      g_warning ("failed to close connection: %s (%s #%d)",
                 error->message, g_quark_to_string (error->domain),
                 error->code);
    }
}

static void
test_exit_on_close_subprocess (gconstpointer test_data)
{
  const TestData *td = test_data;
  GDBusConnection *c;

  loop = g_main_loop_new (NULL, FALSE);

  session_bus_up ();
  c = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);

  g_assert (c != NULL);

  /* the default is meant to be TRUE */
  if (td->exit_on_close != IMPLICITLY_TRUE)
    g_dbus_connection_set_exit_on_close (c, td->exit_on_close);

  g_assert_cmpint (g_dbus_connection_get_exit_on_close (c), ==,
                   (td->exit_on_close != EXPLICITLY_FALSE));
  g_assert (!g_dbus_connection_is_closed (c));

  g_timeout_add (50, quit_later_cb, NULL);
  g_main_loop_run (loop);

  g_signal_connect (c, "closed", G_CALLBACK (closed_cb), (gpointer) td);

  if (td->who_closes == LOCAL)
    {
      GVariant *v;
      GError *error = NULL;

      v = g_dbus_connection_call_sync (c, "org.freedesktop.DBus",
                                       "/org/freedesktop/DBus",
                                       "org.freedesktop.DBus",
                                       "ListNames",
                                       NULL,
                                       G_VARIANT_TYPE ("(as)"),
                                       G_DBUS_CALL_FLAGS_NONE,
                                       -1,
                                       NULL,
                                       &error);
      g_assert_no_error (error);
      g_assert (v != NULL);
      g_variant_unref (v);

      g_dbus_connection_close (c, NULL, close_async_cb, NULL);
    }
  else
    {
      session_bus_stop ();
    }

  g_main_loop_run (loop);
  /* this is only reached when we turn off exit-on-close */
  g_main_loop_unref (loop);
  g_object_unref (c);

  session_bus_down ();

  exit (0);
}

static void
test_exit_on_close (gconstpointer test_data)
{
  const TestData *td = test_data;
  GTestSubprocessFlags flags;
  char *child_name;

  g_test_dbus_unset ();

  if (g_test_verbose ())
    flags = G_TEST_SUBPROCESS_INHERIT_STDOUT | G_TEST_SUBPROCESS_INHERIT_STDERR;
  else
    flags = 0;

  child_name = g_strdup_printf ("/gdbus/exit-on-close/%s/subprocess", td->name);
  g_test_trap_subprocess (child_name, 0, flags);
  g_free (child_name);

  if (td->exit_on_close == EXPLICITLY_FALSE ||
      td->who_closes == LOCAL)
    g_test_trap_assert_passed ();
  else
    g_test_trap_assert_failed();
}

/* ---------------------------------------------------------------------------------------------------- */

int
main (int   argc,
      char *argv[])
{
  gint i;

  g_test_init (&argc, &argv, NULL);

  for (i = 0; cases[i].name != NULL; i++)
    {
      gchar *name;

      name = g_strdup_printf ("/gdbus/exit-on-close/%s", cases[i].name);
      g_test_add_data_func (name, &cases[i], test_exit_on_close);
      g_free (name);

      name = g_strdup_printf ("/gdbus/exit-on-close/%s/subprocess", cases[i].name);
      g_test_add_data_func (name, &cases[i], test_exit_on_close_subprocess);
      g_free (name);
    }

  return g_test_run();
}
