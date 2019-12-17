/* Unit tests for GMutex
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

#include <stdio.h>

static void
test_mutex1 (void)
{
  GMutex mutex;

  g_mutex_init (&mutex);
  g_mutex_lock (&mutex);
  g_mutex_unlock (&mutex);
  g_mutex_lock (&mutex);
  g_mutex_unlock (&mutex);
  g_mutex_clear (&mutex);
}

static void
test_mutex2 (void)
{
  static GMutex mutex;

  g_mutex_lock (&mutex);
  g_mutex_unlock (&mutex);
  g_mutex_lock (&mutex);
  g_mutex_unlock (&mutex);
}

static void
test_mutex3 (void)
{
  GMutex *mutex;

  mutex = g_mutex_new ();
  g_mutex_lock (mutex);
  g_mutex_unlock (mutex);
  g_mutex_lock (mutex);
  g_mutex_unlock (mutex);
  g_mutex_free (mutex);
}

static void
test_mutex4 (void)
{
  static GMutex mutex;
  gboolean ret;

  ret = g_mutex_trylock (&mutex);
  g_assert (ret);

  /* no guarantees that mutex is recursive, so could return 0 or 1 */
  if (g_mutex_trylock (&mutex))
    g_mutex_unlock (&mutex);

  g_mutex_unlock (&mutex);
}

#define LOCKS      48
#define ITERATIONS 10000
#define THREADS    100


GThread *owners[LOCKS];
GMutex   locks[LOCKS];

static void
acquire (gint nr)
{
  GThread *self;

  self = g_thread_self ();

  if (!g_mutex_trylock (&locks[nr]))
    {
      if (g_test_verbose ())
        g_printerr ("thread %p going to block on lock %d\n", self, nr);

      g_mutex_lock (&locks[nr]);
    }

  g_assert (owners[nr] == NULL);   /* hopefully nobody else is here */
  owners[nr] = self;

  /* let some other threads try to ruin our day */
  g_thread_yield ();
  g_thread_yield ();
  g_thread_yield ();

  g_assert (owners[nr] == self);   /* hopefully this is still us... */
  owners[nr] = NULL;               /* make way for the next guy */

  g_mutex_unlock (&locks[nr]);
}

static gpointer
thread_func (gpointer data)
{
  gint i;
  GRand *rand;

  rand = g_rand_new ();

  for (i = 0; i < ITERATIONS; i++)
    acquire (g_rand_int_range (rand, 0, LOCKS));

  g_rand_free (rand);

  return NULL;
}

static void
test_mutex5 (void)
{
  gint i;
  GThread *threads[THREADS];

  for (i = 0; i < LOCKS; i++)
    g_mutex_init (&locks[i]);

  for (i = 0; i < THREADS; i++)
    threads[i] = g_thread_create (thread_func, NULL, TRUE, NULL);

  for (i = 0; i < THREADS; i++)
    g_thread_join (threads[i]);

  for (i = 0; i < LOCKS; i++)
    g_mutex_clear (&locks[i]);

  for (i = 0; i < LOCKS; i++)
    g_assert (owners[i] == NULL);
}

#define COUNT_TO 100000000

static gboolean
do_addition (gint *value)
{
  static GMutex lock;
  gboolean more;

  /* test performance of "good" cases (ie: short critical sections) */
  g_mutex_lock (&lock);
  if ((more = *value != COUNT_TO))
    if (*value != -1)
      (*value)++;
  g_mutex_unlock (&lock);

  return more;
}

static gpointer
addition_thread (gpointer value)
{
  while (do_addition (value));

  return NULL;
}

static void
test_mutex_perf (gconstpointer data)
{
  gint n_threads = GPOINTER_TO_INT (data);
  GThread *threads[THREADS];
  gint64 start_time;
  gdouble rate;
  gint x = -1;
  gint i;

  for (i = 0; i < n_threads - 1; i++)
    threads[i] = g_thread_create (addition_thread, &x, TRUE, NULL);

  /* avoid measuring thread setup/teardown time */
  start_time = g_get_monotonic_time ();
  g_atomic_int_set (&x, 0);
  addition_thread (&x);
  g_assert_cmpint (g_atomic_int_get (&x), ==, COUNT_TO);
  rate = g_get_monotonic_time () - start_time;
  rate = x / rate;

  for (i = 0; i < n_threads - 1; i++)
    g_thread_join (threads[i]);

  g_test_maximized_result (rate, "%f mips", rate);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/thread/mutex1", test_mutex1);
  g_test_add_func ("/thread/mutex2", test_mutex2);
  g_test_add_func ("/thread/mutex3", test_mutex3);
  g_test_add_func ("/thread/mutex4", test_mutex4);
  g_test_add_func ("/thread/mutex5", test_mutex5);

  if (g_test_perf ())
    {
      gint i;

      g_test_add_data_func ("/thread/mutex/perf/uncontended", NULL, test_mutex_perf);

      for (i = 1; i <= 10; i++)
        {
          gchar name[80];
          sprintf (name, "/thread/mutex/perf/contended/%d", i);
          g_test_add_data_func (name, GINT_TO_POINTER (i), test_mutex_perf);
        }
    }

  return g_test_run ();
}
