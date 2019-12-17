/* GLIB - Library of useful routines for C programming
 * Copyright (C) 2005 Matthias Clasen
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
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>

#include "glib.h"
#include "gstdio.h"

#ifdef G_OS_UNIX
#include <unistd.h>
#endif

static gchar *dir, *filename, *displayname, *childname;

static gboolean stop = FALSE;

static gint parent_pid;

#ifndef G_OS_WIN32

static void
handle_usr1 (int signum)
{
  stop = TRUE;
}

#endif

static gboolean
check_stop (gpointer data)
{
  GMainLoop *loop = data;

#ifdef G_OS_WIN32
  stop = g_file_test ("STOP", G_FILE_TEST_EXISTS);
#endif

  if (stop)
    g_main_loop_quit (loop);

  return TRUE;
}

static void
write_or_die (const gchar *filename,
	      const gchar *contents,
	      gssize       length)
{
  GError *error = NULL;
  gchar *displayname;    

  if (!g_file_set_contents (filename, contents, length, &error)) 
    {
      displayname = g_filename_display_name (childname);
      g_print ("failed to write '%s': %s\n", 
	       displayname, error->message);
      exit (1);
    }
}

static GMappedFile *
map_or_die (const gchar *filename,
	    gboolean     writable)
{
  GError *error = NULL;
  GMappedFile *map;
  gchar *displayname;

  map = g_mapped_file_new (filename, writable, &error);
  if (!map)
    {
      displayname = g_filename_display_name (childname);
      g_print ("failed to map '%s' non-writable, shared: %s\n", 
	       displayname, error->message);
      exit (1);
    }

  return map;
}
    
static gboolean
signal_parent (gpointer data)
{
#ifndef G_OS_WIN32
  kill (parent_pid, SIGUSR1);
#endif
  return G_SOURCE_REMOVE;
}

static int
child_main (int argc, char *argv[])
{
  GMappedFile *map;
  GMainLoop *loop;

  parent_pid = atoi (argv[2]);
  map = map_or_die (filename, FALSE);

#ifndef G_OS_WIN32
  signal (SIGUSR1, handle_usr1);
#endif
  loop = g_main_loop_new (NULL, FALSE);
  g_idle_add (check_stop, loop);
  g_idle_add (signal_parent, NULL);
  g_main_loop_run (loop);

 g_message ("test_child_private: received parent signal");

  write_or_die (childname, 
		g_mapped_file_get_contents (map),
		g_mapped_file_get_length (map));

  signal_parent (NULL);

  return 0;
}

static void
test_mapping (void)
{
  GMappedFile *map;

  write_or_die (filename, "ABC", -1);

  map = map_or_die (filename, FALSE);
  g_assert (g_mapped_file_get_length (map) == 3);
  g_mapped_file_free (map);

  map = map_or_die (filename, TRUE);
  g_assert (g_mapped_file_get_length (map) == 3);
  g_mapped_file_free (map);
  g_message ("test_mapping: ok");
}

static void 
test_private (void)
{
  GError *error = NULL;
  GMappedFile *map;
  gchar *buffer;
  gsize len;

  write_or_die (filename, "ABC", -1);
  map = map_or_die (filename, TRUE);

  buffer = (gchar *)g_mapped_file_get_contents (map);
  buffer[0] = '1';
  buffer[1] = '2';
  buffer[2] = '3';
  g_mapped_file_free (map);

  if (!g_file_get_contents (filename, &buffer, &len, &error))
    {
      g_print ("failed to read '%s': %s\n", 
	       displayname, error->message);
      exit (1);
      
    }
  g_assert (len == 3);
  g_assert (strcmp (buffer, "ABC") == 0);
  g_free (buffer);

  g_message ("test_private: ok");
}

static void
test_child_private (gchar *argv0)
{
  GError *error = NULL;
  GMappedFile *map;
  gchar *buffer;
  gsize len;
  gchar *child_argv[4];
  GPid  child_pid;
  GMainLoop *loop;
  gchar pid[100];
  
#ifdef G_OS_WIN32
  g_remove ("STOP");
  g_assert (!g_file_test ("STOP", G_FILE_TEST_EXISTS));
#endif

  write_or_die (filename, "ABC", -1);
  map = map_or_die (filename, TRUE);

#ifndef G_OS_WIN32
  signal (SIGUSR1, handle_usr1);
#endif

  g_snprintf (pid, sizeof(pid), "%d", getpid ());
  child_argv[0] = argv0;
  child_argv[1] = "mapchild";
  child_argv[2] = pid;
  child_argv[3] = NULL;
  if (!g_spawn_async (dir, child_argv, NULL,
		      0, NULL, NULL, &child_pid, &error))
    {
      g_print ("failed to spawn child: %s\n", 
	       error->message);
      exit (1);            
    }
 g_message ("test_child_private: child spawned");

#ifndef G_OS_WIN32
  loop = g_main_loop_new (NULL, FALSE);
  g_idle_add (check_stop, loop);
  g_main_loop_run (loop);
  stop = FALSE;
#else
  g_usleep (2000000);
#endif

 g_message ("test_child_private: received first child signal");

  buffer = (gchar *)g_mapped_file_get_contents (map);
  buffer[0] = '1';
  buffer[1] = '2';
  buffer[2] = '3';
  g_mapped_file_free (map);

#ifndef G_OS_WIN32
  kill (child_pid, SIGUSR1);
#else
  g_file_set_contents ("STOP", "Hey there\n", -1, NULL);
#endif

#ifndef G_OS_WIN32
  g_idle_add (check_stop, loop);
  g_main_loop_run (loop);
#else
  g_usleep (2000000);
#endif

 g_message ("test_child_private: received second child signal");

  if (!g_file_get_contents (childname, &buffer, &len, &error))
    {
      gchar *name;

      name = g_filename_display_name (childname);
      g_print ("failed to read '%s': %s\n", name, error->message);
      exit (1);      
    }
  g_assert (len == 3);
  g_assert (strcmp (buffer, "ABC") == 0);
  g_free (buffer);

  g_message ("test_child_private: ok");
}

static int 
parent_main (int   argc,
	     char *argv[])
{
  /* test mapping with various flag combinations */
  test_mapping ();

  /* test private modification */
  test_private ();

  /* test multiple clients, non-shared */
  test_child_private (argv[0]);

  return 0;
}

int
main (int argc, 
      char *argv[])
{
  int ret;
#ifndef G_OS_WIN32
  sigset_t sig_mask, old_mask;

  sigemptyset (&sig_mask);
  sigaddset (&sig_mask, SIGUSR1);
  if (sigprocmask (SIG_UNBLOCK, &sig_mask, &old_mask) == 0)
    {
      if (sigismember (&old_mask, SIGUSR1))
        g_message ("SIGUSR1 was blocked, unblocking it");
    }
#endif

  dir = g_get_current_dir ();
  filename = g_build_filename (dir, "maptest", NULL);
  displayname = g_filename_display_name (filename);
  childname = g_build_filename (dir, "mapchild", NULL);

  if (argc > 1)
    ret = child_main (argc, argv);
  else 
    ret = parent_main (argc, argv);

  g_free (childname);
  g_free (filename);
  g_free (displayname);
  g_free (dir);

  return ret;
}
