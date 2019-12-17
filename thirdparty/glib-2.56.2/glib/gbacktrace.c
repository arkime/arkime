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

/*
 * MT safe ; except for g_on_error_stack_trace, but who wants thread safety
 * then
 */

#include "config.h"
#include "glibconfig.h"

#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#include <sys/types.h>

#include <time.h>

#ifdef G_OS_UNIX
#include <unistd.h>
#include <sys/wait.h>
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif /* HAVE_SYS_SELECT_H */
#endif

#include <string.h>

#ifdef G_OS_WIN32
#  define STRICT                /* Strict typing, please */
#  define _WIN32_WINDOWS 0x0401 /* to get IsDebuggerPresent */
#  include <windows.h>
#  undef STRICT
#else
#  include <fcntl.h>
#endif

#include "gbacktrace.h"

#include "gtypes.h"
#include "gmain.h"
#include "gprintfint.h"
#include "gutils.h"


#ifndef NO_FD_SET
#  define SELECT_MASK fd_set
#else
#  if defined(_IBMR2)
#    define SELECT_MASK void
#  else
#    define SELECT_MASK int
#  endif
#endif


#ifndef G_OS_WIN32
static void stack_trace (char **args);
#endif

/* People want to hit this from their debugger... */
GLIB_AVAILABLE_IN_ALL volatile gboolean glib_on_error_halt;
volatile gboolean glib_on_error_halt = TRUE;

/**
 * g_on_error_query:
 * @prg_name: the program name, needed by gdb for the "[S]tack trace"
 *     option. If @prg_name is %NULL, g_get_prgname() is called to get
 *     the program name (which will work correctly if gdk_init() or
 *     gtk_init() has been called)
 *
 * Prompts the user with
 * `[E]xit, [H]alt, show [S]tack trace or [P]roceed`.
 * This function is intended to be used for debugging use only.
 * The following example shows how it can be used together with
 * the g_log() functions.
 *
 * |[<!-- language="C" -->
 * #include <glib.h>
 *
 * static void
 * log_handler (const gchar   *log_domain,
 *              GLogLevelFlags log_level,
 *              const gchar   *message,
 *              gpointer       user_data)
 * {
 *   g_log_default_handler (log_domain, log_level, message, user_data);
 *
 *   g_on_error_query (MY_PROGRAM_NAME);
 * }
 *
 * int
 * main (int argc, char *argv[])
 * {
 *   g_log_set_handler (MY_LOG_DOMAIN,
 *                      G_LOG_LEVEL_WARNING |
 *                      G_LOG_LEVEL_ERROR |
 *                      G_LOG_LEVEL_CRITICAL,
 *                      log_handler,
 *                      NULL);
 *   ...
 * ]|
 *
 * If "[E]xit" is selected, the application terminates with a call
 * to _exit(0).
 *
 * If "[S]tack" trace is selected, g_on_error_stack_trace() is called.
 * This invokes gdb, which attaches to the current process and shows
 * a stack trace. The prompt is then shown again.
 *
 * If "[P]roceed" is selected, the function returns.
 *
 * This function may cause different actions on non-UNIX platforms.
 */
void
g_on_error_query (const gchar *prg_name)
{
#ifndef G_OS_WIN32
  static const gchar * const query1 = "[E]xit, [H]alt";
  static const gchar * const query2 = ", show [S]tack trace";
  static const gchar * const query3 = " or [P]roceed";
  gchar buf[16];

  if (!prg_name)
    prg_name = g_get_prgname ();

 retry:

  if (prg_name)
    _g_fprintf (stdout,
                "%s (pid:%u): %s%s%s: ",
                prg_name,
                (guint) getpid (),
                query1,
                query2,
                query3);
  else
    _g_fprintf (stdout,
                "(process:%u): %s%s: ",
                (guint) getpid (),
                query1,
                query3);
  fflush (stdout);

  if (isatty(0) && isatty(1))
    fgets (buf, 8, stdin);
  else
    strcpy (buf, "E\n");

  if ((buf[0] == 'E' || buf[0] == 'e')
      && buf[1] == '\n')
    _exit (0);
  else if ((buf[0] == 'P' || buf[0] == 'p')
           && buf[1] == '\n')
    return;
  else if (prg_name
           && (buf[0] == 'S' || buf[0] == 's')
           && buf[1] == '\n')
    {
      g_on_error_stack_trace (prg_name);
      goto retry;
    }
  else if ((buf[0] == 'H' || buf[0] == 'h')
           && buf[1] == '\n')
    {
      while (glib_on_error_halt)
        ;
      glib_on_error_halt = TRUE;
      return;
    }
  else
    goto retry;
#else
  if (!prg_name)
    prg_name = g_get_prgname ();

  MessageBox (NULL, "g_on_error_query called, program terminating",
              (prg_name && *prg_name) ? prg_name : NULL,
              MB_OK|MB_ICONERROR);
  _exit(0);
#endif
}

/**
 * g_on_error_stack_trace:
 * @prg_name: the program name, needed by gdb for the "[S]tack trace"
 *     option
 *
 * Invokes gdb, which attaches to the current process and shows a
 * stack trace. Called by g_on_error_query() when the "[S]tack trace"
 * option is selected. You can get the current process's program name
 * with g_get_prgname(), assuming that you have called gtk_init() or
 * gdk_init().
 *
 * This function may cause different actions on non-UNIX platforms.
 */
void
g_on_error_stack_trace (const gchar *prg_name)
{
#if defined(G_OS_UNIX)
  pid_t pid;
  gchar buf[16];
  gchar *args[4] = { "gdb", NULL, NULL, NULL };
  int status;

  if (!prg_name)
    return;

  _g_sprintf (buf, "%u", (guint) getpid ());

  args[1] = (gchar*) prg_name;
  args[2] = buf;

  pid = fork ();
  if (pid == 0)
    {
      stack_trace (args);
      _exit (0);
    }
  else if (pid == (pid_t) -1)
    {
      perror ("unable to fork gdb");
      return;
    }

  waitpid (pid, &status, 0);
#else
  if (IsDebuggerPresent ())
    G_BREAKPOINT ();
  else
    g_abort ();
#endif
}

#ifndef G_OS_WIN32

static gboolean stack_trace_done = FALSE;

static void
stack_trace_sigchld (int signum)
{
  stack_trace_done = TRUE;
}

static void
stack_trace (char **args)
{
  pid_t pid;
  int in_fd[2];
  int out_fd[2];
  SELECT_MASK fdset;
  SELECT_MASK readset;
  struct timeval tv;
  int sel, idx, state;
  char buffer[256];
  char c;

  stack_trace_done = FALSE;
  signal (SIGCHLD, stack_trace_sigchld);

  if ((pipe (in_fd) == -1) || (pipe (out_fd) == -1))
    {
      perror ("unable to open pipe");
      _exit (0);
    }

  pid = fork ();
  if (pid == 0)
    {
      /* Save stderr for printing failure below */
      int old_err = dup (2);
      fcntl (old_err, F_SETFD, fcntl (old_err, F_GETFD) | FD_CLOEXEC);

      close (0); dup (in_fd[0]);   /* set the stdin to the in pipe */
      close (1); dup (out_fd[1]);  /* set the stdout to the out pipe */
      close (2); dup (out_fd[1]);  /* set the stderr to the out pipe */

      execvp (args[0], args);      /* exec gdb */

      /* Print failure to original stderr */
      close (2); dup (old_err);
      perror ("exec gdb failed");
      _exit (0);
    }
  else if (pid == (pid_t) -1)
    {
      perror ("unable to fork");
      _exit (0);
    }

  FD_ZERO (&fdset);
  FD_SET (out_fd[0], &fdset);

  write (in_fd[1], "backtrace\n", 10);
  write (in_fd[1], "p x = 0\n", 8);
  write (in_fd[1], "quit\n", 5);

  idx = 0;
  state = 0;

  while (1)
    {
      readset = fdset;
      tv.tv_sec = 1;
      tv.tv_usec = 0;

      sel = select (FD_SETSIZE, &readset, NULL, NULL, &tv);
      if (sel == -1)
        break;

      if ((sel > 0) && (FD_ISSET (out_fd[0], &readset)))
        {
          if (read (out_fd[0], &c, 1))
            {
              switch (state)
                {
                case 0:
                  if (c == '#')
                    {
                      state = 1;
                      idx = 0;
                      buffer[idx++] = c;
                    }
                  break;
                case 1:
                  buffer[idx++] = c;
                  if ((c == '\n') || (c == '\r'))
                    {
                      buffer[idx] = 0;
                      _g_fprintf (stdout, "%s", buffer);
                      state = 0;
                      idx = 0;
                    }
                  break;
                default:
                  break;
                }
            }
        }
      else if (stack_trace_done)
        break;
    }

  close (in_fd[0]);
  close (in_fd[1]);
  close (out_fd[0]);
  close (out_fd[1]);
  _exit (0);
}

#endif /* !G_OS_WIN32 */
