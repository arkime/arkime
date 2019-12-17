/* 
 * Copyright (C) 2011 Red Hat, Inc.
 *
 * This work is provided "as is"; redistribution and modification
 * in whole or in part, in any medium, physical or electronic is
 * permitted without restriction.
 *
 * This work is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * In no event shall the authors or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even
 * if advised of the possibility of such damage.
 *
 * Author: Colin Walters <walters@verbum.org> 
 */

#include "config.h"

#include "glib-unix.h"
#include <string.h>

static void
test_pipe (void)
{
  GError *error = NULL;
  int pipefd[2];
  char buf[1024];
  gssize bytes_read;
  gboolean res;

  res = g_unix_open_pipe (pipefd, FD_CLOEXEC, &error);
  g_assert (res);
  g_assert_no_error (error);

  write (pipefd[1], "hello", sizeof ("hello"));
  memset (buf, 0, sizeof (buf));
  bytes_read = read (pipefd[0], buf, sizeof(buf) - 1);
  g_assert_cmpint (bytes_read, >, 0);
  buf[bytes_read] = '\0';

  close (pipefd[0]);
  close (pipefd[1]);

  g_assert (g_str_has_prefix (buf, "hello"));
}

static void
test_error (void)
{
  GError *error = NULL;
  gboolean res;

  res = g_unix_set_fd_nonblocking (123456, TRUE, &error);
  g_assert_cmpint (errno, ==, EBADF);
  g_assert (!res);
  g_assert_error (error, G_UNIX_ERROR, 0);
  g_clear_error (&error);
}

static void
test_nonblocking (void)
{
  GError *error = NULL;
  int pipefd[2];
  gboolean res;
  int flags;

  res = g_unix_open_pipe (pipefd, FD_CLOEXEC, &error);
  g_assert (res);
  g_assert_no_error (error);

  res = g_unix_set_fd_nonblocking (pipefd[0], TRUE, &error);
  g_assert (res);
  g_assert_no_error (error);
 
  flags = fcntl (pipefd[0], F_GETFL);
  g_assert_cmpint (flags, !=, -1);
  g_assert (flags & O_NONBLOCK);

  res = g_unix_set_fd_nonblocking (pipefd[0], FALSE, &error);
  g_assert (res);
  g_assert_no_error (error);
 
  flags = fcntl (pipefd[0], F_GETFL);
  g_assert_cmpint (flags, !=, -1);
  g_assert (!(flags & O_NONBLOCK));

  close (pipefd[0]);
  close (pipefd[1]);
}

static gboolean sig_received = FALSE;
static gboolean sig_timeout = FALSE;
static int sig_counter = 0;

static gboolean
on_sig_received (gpointer user_data)
{
  GMainLoop *loop = user_data;
  g_main_loop_quit (loop);
  sig_received = TRUE;
  sig_counter ++;
  return G_SOURCE_REMOVE;
}

static gboolean
on_sig_timeout (gpointer data)
{
  GMainLoop *loop = data;
  g_main_loop_quit (loop);
  sig_timeout = TRUE;
  return G_SOURCE_REMOVE;
}

static gboolean
exit_mainloop (gpointer data)
{
  GMainLoop *loop = data;
  g_main_loop_quit (loop);
  return G_SOURCE_REMOVE;
}

static gboolean
on_sig_received_2 (gpointer data)
{
  GMainLoop *loop = data;

  sig_counter ++;
  if (sig_counter == 2)
    g_main_loop_quit (loop);
  return G_SOURCE_REMOVE;
}

static void
test_signal (int signum)
{
  GMainLoop *mainloop;
  int id;

  mainloop = g_main_loop_new (NULL, FALSE);

  sig_received = FALSE;
  sig_counter = 0;
  g_unix_signal_add (signum, on_sig_received, mainloop);
  kill (getpid (), signum);
  g_assert (!sig_received);
  id = g_timeout_add (5000, on_sig_timeout, mainloop);
  g_main_loop_run (mainloop);
  g_assert (sig_received);
  sig_received = FALSE;
  g_source_remove (id);

  /* Ensure we don't get double delivery */
  g_timeout_add (500, exit_mainloop, mainloop);
  g_main_loop_run (mainloop);
  g_assert (!sig_received);

  /* Ensure that two sources for the same signal get it */
  sig_counter = 0;
  g_unix_signal_add (signum, on_sig_received_2, mainloop);
  g_unix_signal_add (signum, on_sig_received_2, mainloop);
  id = g_timeout_add (5000, on_sig_timeout, mainloop);

  kill (getpid (), signum);
  g_main_loop_run (mainloop);
  g_assert_cmpint (sig_counter, ==, 2);
  g_source_remove (id);

  g_main_loop_unref (mainloop);
}

static void
test_sighup (void)
{
  test_signal (SIGHUP);
}

static void
test_sigterm (void)
{
  test_signal (SIGTERM);
}

static void
test_sighup_add_remove (void)
{
  guint id;
  struct sigaction action;

  sig_received = FALSE;
  id = g_unix_signal_add (SIGHUP, on_sig_received, NULL);
  g_source_remove (id);

  sigaction (SIGHUP, NULL, &action);
  g_assert (action.sa_handler == SIG_DFL);
}

static gboolean
nested_idle (gpointer data)
{
  GMainLoop *nested;
  GMainContext *context;
  GSource *source;

  context = g_main_context_new ();
  nested = g_main_loop_new (context, FALSE);

  source = g_unix_signal_source_new (SIGHUP);
  g_source_set_callback (source, on_sig_received, nested, NULL);
  g_source_attach (source, context);
  g_source_unref (source);

  kill (getpid (), SIGHUP);
  g_main_loop_run (nested);
  g_assert_cmpint (sig_counter, ==, 1);

  g_main_loop_unref (nested);
  g_main_context_unref (context);

  return G_SOURCE_REMOVE;
}

static void
test_sighup_nested (void)
{
  GMainLoop *mainloop;

  mainloop = g_main_loop_new (NULL, FALSE);

  sig_counter = 0;
  sig_received = FALSE;
  g_unix_signal_add (SIGHUP, on_sig_received, mainloop);
  g_idle_add (nested_idle, mainloop);

  g_main_loop_run (mainloop);
  g_assert_cmpint (sig_counter, ==, 2);

  g_main_loop_unref (mainloop);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/glib-unix/pipe", test_pipe);
  g_test_add_func ("/glib-unix/error", test_error);
  g_test_add_func ("/glib-unix/nonblocking", test_nonblocking);
  g_test_add_func ("/glib-unix/sighup", test_sighup);
  g_test_add_func ("/glib-unix/sigterm", test_sigterm);
  g_test_add_func ("/glib-unix/sighup_again", test_sighup);
  g_test_add_func ("/glib-unix/sighup_add_remove", test_sighup_add_remove);
  g_test_add_func ("/glib-unix/sighup_nested", test_sighup_nested);

  return g_test_run();
}
