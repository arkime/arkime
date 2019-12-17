/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GLib Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GLib Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GLib at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include <sys/types.h>
#include <stdlib.h>

#include <glib.h>

#ifdef G_OS_UNIX
#include <unistd.h>
#endif

#ifdef G_OS_WIN32
#include <windows.h>
#endif

#ifdef G_OS_WIN32
#define GPID_FORMAT "%p"
#else
#define GPID_FORMAT "%d"
#endif

GMainLoop *main_loop;
gint alive;

#ifdef G_OS_WIN32
char *argv0;
#endif

static GPid
get_a_child (gint ttl)
{
  GPid pid;

#ifdef G_OS_WIN32
  STARTUPINFO si;
  PROCESS_INFORMATION pi;
  gchar *cmdline;

  memset (&si, 0, sizeof (si));
  si.cb = sizeof (&si);
  memset (&pi, 0, sizeof (pi));

  cmdline = g_strdup_printf( "child-test -c%d", ttl);

  if (!CreateProcess (argv0, cmdline, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
    g_error ("CreateProcess failed: %s\n", g_win32_error_message (GetLastError ()));

  g_free(cmdline);

  CloseHandle (pi.hThread);
  pid = pi.hProcess;

  return pid;
#else
  pid = fork ();
  if (pid < 0)
    exit (1);

  if (pid > 0)
    return pid;

  sleep (ttl);
  _exit (0);
#endif /* G_OS_WIN32 */
}

static gboolean
child_watch_callback (GPid pid, gint status, gpointer data)
{
#ifdef VERBOSE
  gint ttl = GPOINTER_TO_INT (data);

  g_print ("child " GPID_FORMAT " (ttl %d) exited, status %d\n", pid, ttl, status);
#endif

  g_spawn_close_pid (pid);

  if (--alive == 0)
    g_main_loop_quit (main_loop);

  return TRUE;
}

static gboolean
quit_loop (gpointer data)
{
  GMainLoop *main_loop = data;

  g_main_loop_quit (main_loop);

  return TRUE;
}

#ifdef TEST_THREAD
static gpointer
test_thread (gpointer data)
{
  GMainLoop *new_main_loop;
  GSource *source;
  GPid pid;
  gint ttl = GPOINTER_TO_INT (data);

  new_main_loop = g_main_loop_new (NULL, FALSE);

  pid = get_a_child (ttl);
  source = g_child_watch_source_new (pid);
  g_source_set_callback (source, (GSourceFunc) child_watch_callback, data, NULL);
  g_source_attach (source, g_main_loop_get_context (new_main_loop));
  g_source_unref (source);

#ifdef VERBOSE
  g_print ("whee! created pid: " GPID_FORMAT " (ttl %d)\n", pid, ttl);
#endif

  g_main_loop_run (new_main_loop);

  return NULL;
}
#endif

int
main (int argc, char *argv[])
{
#ifndef TEST_THREAD
  GPid pid;
#endif
#ifdef G_OS_WIN32
  argv0 = argv[0];
  if (argc > 1 && argv[1][0] == '-' && argv[1][1] == 'c')
    {
      int ttl = atoi (argv[1] + 2);
      Sleep (ttl * 1000);
      /* Exit on purpose with STILL_ACTIVE (which isn't a very common
       * exit status) to verify that g_child_watch_check() in gmain.c
       * doesn't believe a child still to be active if it happens to
       * exit with that status.
       */
      exit (STILL_ACTIVE);
    }
#endif

  main_loop = g_main_loop_new (NULL, FALSE);

#ifdef G_OS_WIN32
  system ("ipconfig /all");
#else
  system ("true");
#endif

  alive = 2;
  g_timeout_add_seconds (30, quit_loop, main_loop);

#ifdef TEST_THREAD
  g_thread_create (test_thread, GINT_TO_POINTER (10), FALSE, NULL);
  g_thread_create (test_thread, GINT_TO_POINTER (20), FALSE, NULL);
#else
  pid = get_a_child (10);
  g_child_watch_add (pid, (GChildWatchFunc) child_watch_callback,
		     GINT_TO_POINTER (10));
  pid = get_a_child (20);
  g_child_watch_add (pid, (GChildWatchFunc) child_watch_callback,
		     GINT_TO_POINTER (20));
#endif
  
  g_main_loop_run (main_loop);

  g_main_loop_unref (main_loop);

  if (alive > 0)
    {
      g_warning ("%d children still alive\n", alive);
      return 1;
    }
    
   return 0;
}
