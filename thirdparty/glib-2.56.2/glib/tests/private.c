/* Unit tests for GPrivate and friends
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

/* We are testing some deprecated APIs here */
#define GLIB_DISABLE_DEPRECATION_WARNINGS

#include <glib.h>

/* test basics:
 * - initial value is NULL
 * - set/get work repeatedly
 */
static void
test_private1 (void)
{
  static GPrivate private = G_PRIVATE_INIT (NULL);
  gpointer value;

  value = g_private_get (&private);
  g_assert (value == NULL);

  g_private_set (&private, GINT_TO_POINTER(1));
  value = g_private_get (&private);
  g_assert_cmpint (GPOINTER_TO_INT (value), ==, 1);

  g_private_set (&private, GINT_TO_POINTER(2));
  value = g_private_get (&private);
  g_assert_cmpint (GPOINTER_TO_INT (value), ==, 2);
}

static gint private2_destroy_count;

static void
private2_destroy (gpointer data)
{
  g_atomic_int_inc (&private2_destroy_count);
}

static GPrivate private2 = G_PRIVATE_INIT (private2_destroy);

static gpointer
private2_func (gpointer data)
{
  gint value = GPOINTER_TO_INT (data);
  gint i;
  gint v, v2;

  for (i = 0; i < 1000; i++)
    {
      v = value + (i % 5);
      g_private_set (&private2, GINT_TO_POINTER (v));
      g_usleep (1000);
      v2 = GPOINTER_TO_INT (g_private_get (&private2));
      g_assert_cmpint (v, ==, v2);
    }

  if (value % 2 == 0)
    g_thread_exit (NULL);

  return NULL;
}

/* test that
 * - threads do not interfere with each other
 * - destroy notifies are called for each thread exit
 * - destroy notifies are called for g_thread_exit() too
 * - destroy notifies are not called on g_private_set()
 * - destroy notifies are called on g_private_replace()
 */
static void
test_private2 (void)
{
  GThread *thread[10];
  gint i;

  g_private_set (&private2, GINT_TO_POINTER (234));
  g_private_replace (&private2, GINT_TO_POINTER (123));

  for (i = 0; i < 10; i++)
    thread[i] = g_thread_create (private2_func, GINT_TO_POINTER (i), TRUE, NULL);

  for (i = 0; i < 10; i++)
    g_thread_join (thread[i]);

  g_assert_cmpint (private2_destroy_count, ==, 11);
}

static gboolean private3_freed;

static void
private3_free (gpointer data)
{
  g_assert (data == (void*) 0x1234);
  private3_freed = TRUE;
}

#ifdef G_OS_WIN32
#include <windows.h>
#include <process.h>

static guint __stdcall
#else
#include <pthread.h>

static gpointer
#endif
private3_func (gpointer data)
{
  static GPrivate key = G_PRIVATE_INIT (private3_free);

  g_private_set (&key, (void *) 0x1234);

  return 0;
}

static void
test_private3 (void)
{
  g_assert (!private3_freed);

#ifdef G_OS_WIN32
  {
    HANDLE thread;
    guint ignore;
    thread = (HANDLE) _beginthreadex (NULL, 0, private3_func, NULL, 0, &ignore);
    WaitForSingleObject (thread, INFINITE);
    CloseHandle (thread);
  }
#else
  {
    pthread_t thread;

    pthread_create (&thread, NULL, private3_func, NULL);
    pthread_join (thread, NULL);
  }
#endif
  g_assert (private3_freed);
}

/* test basics:
 * - static initialization works
 * - initial value is NULL
 * - get/set works repeatedly
 */
static GStaticPrivate sp1 = G_STATIC_PRIVATE_INIT;

static void
test_static_private1 (void)
{
  gpointer value;

  value = g_static_private_get (&sp1);
  g_assert (value == NULL);

  g_static_private_set (&sp1, GINT_TO_POINTER(1), NULL);
  value = g_static_private_get (&sp1);
  g_assert_cmpint (GPOINTER_TO_INT(value), ==, 1);

  g_static_private_set (&sp1, GINT_TO_POINTER(2), NULL);
  value = g_static_private_get (&sp1);
  g_assert_cmpint (GPOINTER_TO_INT(value), ==, 2);

  g_static_private_free (&sp1);

  value = g_static_private_get (&sp1);
  g_assert (value == NULL);
}

static gint sp2_destroy_count;

static void
sp2_destroy (gpointer data)
{
  sp2_destroy_count++;
}

static void
sp2_destroy2 (gpointer data)
{
  gint value = GPOINTER_TO_INT (data);

  g_assert_cmpint (value, ==, 2);
}

/* test that destroy notifies are called as expected
 * and on the right values
 */
static void
test_static_private2 (void)
{
  GStaticPrivate sp2;
  gpointer value;

  g_static_private_init (&sp2);

  value = g_static_private_get (&sp2);
  g_assert (value == NULL);

  g_static_private_set (&sp2, GINT_TO_POINTER(1), sp2_destroy);
  g_assert_cmpint (sp2_destroy_count, ==, 0);
  value = g_static_private_get (&sp2);
  g_assert_cmpint (GPOINTER_TO_INT(value), ==, 1);

  g_static_private_set (&sp2, GINT_TO_POINTER(2), sp2_destroy2);
  g_assert_cmpint (sp2_destroy_count, ==, 1);
  value = g_static_private_get (&sp2);
  g_assert_cmpint (GPOINTER_TO_INT(value), ==, 2);

  g_static_private_set (&sp2, GINT_TO_POINTER(3), sp2_destroy);
  g_assert_cmpint (sp2_destroy_count, ==, 1);
  value = g_static_private_get (&sp2);
  g_assert_cmpint (GPOINTER_TO_INT(value), ==, 3);

  g_static_private_free (&sp2);

  value = g_static_private_get (&sp2);
  g_assert (value == NULL);
}

/* test that freeing and reinitializing a static private
 * drops previous value
 */
static void
test_static_private3 (void)
{
  GStaticPrivate sp3;
  gpointer value;

  g_static_private_init (&sp3);

  value = g_static_private_get (&sp3);
  g_assert (value == NULL);

  g_static_private_set (&sp3, GINT_TO_POINTER(1), NULL);
  value = g_static_private_get (&sp3);
  g_assert_cmpint (GPOINTER_TO_INT(value), ==, 1);

  g_static_private_free (&sp3);
  g_static_private_init (&sp3);

  value = g_static_private_get (&sp3);
  g_assert (value == NULL);

  g_static_private_set (&sp3, GINT_TO_POINTER(2), NULL);
  value = g_static_private_get (&sp3);
  g_assert_cmpint (GPOINTER_TO_INT(value), ==, 2);

  g_static_private_free (&sp3);
}

static GStaticPrivate sp4 = G_STATIC_PRIVATE_INIT;

static gpointer
sp4_func (gpointer data)
{
  gint value = GPOINTER_TO_INT (data);
  gint i;
  gint v, v2;

  for (i = 0; i < 1000; i++)
    {
      v = value + (i % 5);
      g_static_private_set (&sp4, GINT_TO_POINTER(v), NULL);
      g_usleep (1000);
      v2 = GPOINTER_TO_INT(g_static_private_get (&sp4));
      g_assert_cmpint (v, ==, v2);
    }

  if (value % 2 == 0)
    g_thread_exit (NULL);

  return NULL;
}

/* test that threads do not interfere with each other
 */
static void
test_static_private4 (void)
{
  GThread *thread[10];
  gint i;

  for (i = 0; i < 10; i++)
    thread[i] = g_thread_create (sp4_func, GINT_TO_POINTER (i), TRUE, NULL);

  for (i = 0; i < 10; i++)
    g_thread_join (thread[i]);

  g_static_private_free (&sp4);
}

static GStaticPrivate sp5 = G_STATIC_PRIVATE_INIT;
static GMutex m5;
static GCond c5a;
static GCond c5b;
static gint count5;

static gpointer
sp5_func (gpointer data)
{
  gint v = GPOINTER_TO_INT (data);
  gpointer value;

  value = g_static_private_get (&sp5);
  g_assert (value == NULL);

  g_static_private_set (&sp5, GINT_TO_POINTER (v), NULL);
  value = g_static_private_get (&sp5);
  g_assert_cmpint (GPOINTER_TO_INT (value), ==, v);

  if (g_test_verbose ())
    g_printerr ("thread %d set sp5\n", v);
  g_mutex_lock (&m5);
  g_atomic_int_inc (&count5);
  g_cond_signal (&c5a);
  g_cond_wait (&c5b, &m5);
  g_mutex_unlock (&m5);

  if (g_test_verbose ())
    g_printerr ("thread %d get sp5\n", v);
  value = g_static_private_get (&sp5);
  g_assert (value == NULL);

  return NULL;
}

static void
test_static_private5 (void)
{
  GThread *thread[10];
  gint i;

  g_atomic_int_set (&count5, 0);

  for (i = 0; i < 10; i++)
    thread[i] = g_thread_create (sp5_func, GINT_TO_POINTER (i), TRUE, NULL);

  g_mutex_lock (&m5);
  while (g_atomic_int_get (&count5) < 10)
    g_cond_wait (&c5a, &m5);

  if (g_test_verbose ())
    g_printerr ("sp5 gets nuked\n");

  g_static_private_free (&sp5);

  g_cond_broadcast (&c5b);
  g_mutex_unlock (&m5);

  for (i = 0; i < 10; i++)
    g_thread_join (thread[i]);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/thread/private1", test_private1);
  g_test_add_func ("/thread/private2", test_private2);
  g_test_add_func ("/thread/private3", test_private3);
  g_test_add_func ("/thread/staticprivate1", test_static_private1);
  g_test_add_func ("/thread/staticprivate2", test_static_private2);
  g_test_add_func ("/thread/staticprivate3", test_static_private3);
  g_test_add_func ("/thread/staticprivate4", test_static_private4);
  g_test_add_func ("/thread/staticprivate5", test_static_private5);

  return g_test_run ();
}
