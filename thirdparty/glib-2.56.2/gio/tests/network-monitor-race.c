/*
 * Copyright (C) 2018 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * licence, or (at your option) any later version.
 *
 * This is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include <glib/glib.h>
#include <gio/gio.h>

#define MAX_RUNS 333

static gboolean
quit_loop (gpointer user_data)
{
  g_main_loop_quit (user_data);

  return FALSE;
}

static gpointer
thread_func (gpointer user_data)
{
  g_network_monitor_get_default ();
  g_timeout_add (100, quit_loop, user_data);

  return NULL;
}

static gboolean
call_func (gpointer user_data)
{
  GThread *thread;

  thread = g_thread_new (NULL, thread_func, user_data);
  g_thread_unref (thread);

  return FALSE;
}

/* Test that calling g_network_monitor_get_default() in a thread doesn’t cause
 * a crash. This is a probabilistic test; since it’s testing a race condition,
 * it can’t deterministically reproduce the problem. The threading has to
 * happen in subprocesses, since the result of g_network_monitor_get_default()
 * is unavoidably cached once created. */
static void
test_network_monitor (void)
{
  guint ii;

  g_test_bug ("793727");

  if (g_test_subprocess ())
    {
       GMainLoop *main_loop;

       main_loop = g_main_loop_new (NULL, FALSE);
       g_timeout_add (1, call_func, main_loop);
       g_main_loop_run (main_loop);
       g_main_loop_unref (main_loop);

       return;
    }

  for (ii = 0; ii < MAX_RUNS; ii++)
    {
       g_test_trap_subprocess (NULL, 0, 0);
       g_test_trap_assert_passed ();
    }
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  g_test_bug_base ("https://bugzilla.gnome.org/show_bug.cgi?id=");

  g_test_add_func ("/network-monitor/create-in-thread",
                   test_network_monitor);

  return g_test_run ();
}
