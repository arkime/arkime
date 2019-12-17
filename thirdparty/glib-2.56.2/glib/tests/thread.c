/* Unit tests for GThread
 * Copyright (C) 2011 Red Hat, Inc
 * Author: Matthias Clasen
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
 */

#include <config.h>

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#include <sys/types.h>
#ifdef HAVE_SYS_PRCTL_H
#include <sys/prctl.h>
#endif

#include <glib.h>

#ifdef G_OS_UNIX
#include <unistd.h>
#include <sys/resource.h>
#endif

#ifdef THREADS_POSIX
#include <pthread.h>
#endif

static gpointer
thread1_func (gpointer data)
{
  g_thread_exit (GINT_TO_POINTER (1));

  g_assert_not_reached ();

  return NULL;
}

/* test that g_thread_exit() works */
static void
test_thread1 (void)
{
  gpointer result;
  GThread *thread;
  GError *error = NULL;

  thread = g_thread_try_new ("test", thread1_func, NULL, &error);
  g_assert_no_error (error);

  result = g_thread_join (thread);

  g_assert_cmpint (GPOINTER_TO_INT (result), ==, 1);
}

static gpointer
thread2_func (gpointer data)
{
  return g_thread_self ();
}

/* test that g_thread_self() works */
static void
test_thread2 (void)
{
  gpointer result;
  GThread *thread;

  thread = g_thread_new ("test", thread2_func, NULL);

  g_assert (g_thread_self () != thread);

  result = g_thread_join (thread);

  g_assert (result == thread);
}

static gpointer
thread3_func (gpointer data)
{
  GThread *peer = data;
  gint retval;

  retval = 3;

  if (peer)
    {
      gpointer result;

      result = g_thread_join (peer);

      retval += GPOINTER_TO_INT (result);
    }

  return GINT_TO_POINTER (retval);
}

/* test that g_thread_join() works across peers */
static void
test_thread3 (void)
{
  gpointer result;
  GThread *thread1, *thread2, *thread3;

  thread1 = g_thread_new ("a", thread3_func, NULL);
  thread2 = g_thread_new ("b", thread3_func, thread1);
  thread3 = g_thread_new ("c", thread3_func, thread2);

  result = g_thread_join (thread3);

  g_assert_cmpint (GPOINTER_TO_INT(result), ==, 9);
}

/* test that thread creation fails as expected,
 * by setting RLIMIT_NPROC ridiculously low
 */
static void
test_thread4 (void)
{
#ifdef HAVE_PRLIMIT
  struct rlimit ol, nl;
  GThread *thread;
  GError *error;
  gint ret;

  /* Linux CAP_SYS_RESOURCE overrides RLIMIT_NPROC, and probably similar
   * things are true on other systems.
   */
  if (getuid () == 0 || geteuid () == 0)
    return;

  getrlimit (RLIMIT_NPROC, &nl);
  nl.rlim_cur = 1;

  if ((ret = prlimit (getpid(), RLIMIT_NPROC, &nl, &ol)) != 0)
    g_error ("prlimit failed: %s\n", g_strerror (ret));

  error = NULL;
  thread = g_thread_try_new ("a", thread1_func, NULL, &error);
  g_assert (thread == NULL);
  g_assert_error (error, G_THREAD_ERROR, G_THREAD_ERROR_AGAIN);
  g_error_free (error);

  if ((ret = prlimit (getpid (), RLIMIT_NPROC, &ol, NULL)) != 0)
    g_error ("resetting RLIMIT_NPROC failed: %s\n", g_strerror (ret));
#endif
}

static void
test_thread5 (void)
{
  GThread *thread;

  thread = g_thread_new ("a", thread3_func, NULL);
  g_thread_ref (thread);
  g_thread_join (thread);
  g_thread_unref (thread);
}

static gpointer
thread6_func (gpointer data)
{
#if defined (HAVE_PTHREAD_SETNAME_NP_WITH_TID) && defined (HAVE_PTHREAD_GETNAME_NP)
  char name[16];

  pthread_getname_np (pthread_self(), name, 16);

  g_assert_cmpstr (name, ==, data);
#endif

  return NULL;
}

static void
test_thread6 (void)
{
  GThread *thread;

  thread = g_thread_new ("abc", thread6_func, "abc");
  g_thread_join (thread);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/thread/thread1", test_thread1);
  g_test_add_func ("/thread/thread2", test_thread2);
  g_test_add_func ("/thread/thread3", test_thread3);
  g_test_add_func ("/thread/thread4", test_thread4);
  g_test_add_func ("/thread/thread5", test_thread5);
  g_test_add_func ("/thread/thread6", test_thread6);

  return g_test_run ();
}
