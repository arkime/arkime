/* Unit tests for GAsyncQueue
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

static gint
compare_func (gconstpointer d1, gconstpointer d2, gpointer data)
{
  gint i1, i2;

  i1 = GPOINTER_TO_INT (d1);
  i2 = GPOINTER_TO_INT (d2);

  return i1 - i2;
}

static
void test_async_queue_sort (void)
{
  GAsyncQueue *q;

  q = g_async_queue_new ();

  g_async_queue_push (q, GINT_TO_POINTER (10));
  g_async_queue_push (q, GINT_TO_POINTER (2));
  g_async_queue_push (q, GINT_TO_POINTER (7));

  g_async_queue_sort (q, compare_func, NULL);

  g_async_queue_push_sorted (q, GINT_TO_POINTER (1), compare_func, NULL);
  g_async_queue_push_sorted (q, GINT_TO_POINTER (8), compare_func, NULL);

  g_assert_cmpint (GPOINTER_TO_INT (g_async_queue_pop (q)), ==, 1);
  g_assert_cmpint (GPOINTER_TO_INT (g_async_queue_pop (q)), ==, 2);
  g_assert_cmpint (GPOINTER_TO_INT (g_async_queue_pop (q)), ==, 7);
  g_assert_cmpint (GPOINTER_TO_INT (g_async_queue_pop (q)), ==, 8);
  g_assert_cmpint (GPOINTER_TO_INT (g_async_queue_pop (q)), ==, 10);

  g_assert (g_async_queue_try_pop (q) == NULL);

  g_async_queue_unref (q);
}

static gint destroy_count;

static void
destroy_notify (gpointer item)
{
  destroy_count++;
}

static void
test_async_queue_destroy (void)
{
  GAsyncQueue *q;

  q = g_async_queue_new_full (destroy_notify);

  g_assert (destroy_count == 0);

  g_async_queue_push (q, GINT_TO_POINTER (1));
  g_async_queue_push (q, GINT_TO_POINTER (1));
  g_async_queue_push (q, GINT_TO_POINTER (1));
  g_async_queue_push (q, GINT_TO_POINTER (1));

  g_assert (g_async_queue_length (q) == 4);

  g_async_queue_unref (q);

  g_assert (destroy_count == 4);
}

static GAsyncQueue *q;

static GThread *threads[10];
static gint counts[10];
static gint sums[10];
static gint total;

static gpointer
thread_func (gpointer data)
{
  gint pos = GPOINTER_TO_INT (data);
  gint value;

  while (1)
    {
      value = GPOINTER_TO_INT (g_async_queue_pop (q));

      if (value == -1)
        break;

      counts[pos]++;
      sums[pos] += value;

      g_usleep (1000);
    }

  return NULL;
}

static void
test_async_queue_threads (void)
{
  gint i, j;
  gint s, c;
  gint value;

  q = g_async_queue_new ();

  for (i = 0; i < 10; i++)
    threads[i] = g_thread_new ("test", thread_func, GINT_TO_POINTER (i));

  for (i = 0; i < 100; i++)
    {
      g_async_queue_lock (q);
      for (j = 0; j < 10; j++)
        {
          value = g_random_int_range (1, 100);
          total += value;
          g_async_queue_push_unlocked (q, GINT_TO_POINTER (value));
        }
      g_async_queue_unlock (q);

      g_usleep (1000);
    }

  for (i = 0; i < 10; i++)
    g_async_queue_push (q, GINT_TO_POINTER(-1));

  for (i = 0; i < 10; i++)
    g_thread_join (threads[i]);

  g_assert_cmpint (g_async_queue_length (q), ==, 0);

  s = c = 0;

  for (i = 0; i < 10; i++)
    {
      g_assert_cmpint (sums[i], >, 0);
      g_assert_cmpint (counts[i], >, 0);
      s += sums[i];
      c += counts[i];
    }

  g_assert_cmpint (s, ==, total);
  g_assert_cmpint (c, ==, 1000);

  g_async_queue_unref (q);
}

static void
test_async_queue_timed (void)
{
  GAsyncQueue *q;
  GTimeVal tv;
  gint64 start, end, diff;
  gpointer val;

  q = g_async_queue_new ();

  start = g_get_monotonic_time ();
  val = g_async_queue_timeout_pop (q, G_USEC_PER_SEC / 10);
  g_assert (val == NULL);

  end = g_get_monotonic_time ();
  diff = end - start;
  g_assert_cmpint (diff, >=, G_USEC_PER_SEC / 10);
  /* diff should be only a little bit more than G_USEC_PER_SEC/10, but
   * we have to leave some wiggle room for heavily-loaded machines...
   */
  g_assert_cmpint (diff, <, G_USEC_PER_SEC);

  start = end;
  g_get_current_time (&tv);
  g_time_val_add (&tv, G_USEC_PER_SEC / 10);
  val = g_async_queue_timed_pop (q, &tv);
  g_assert (val == NULL);

  end = g_get_monotonic_time ();
  diff = end - start;
  g_assert_cmpint (diff, >=, G_USEC_PER_SEC / 10);
  g_assert_cmpint (diff, <, G_USEC_PER_SEC);

  start = end;
  g_get_current_time (&tv);
  g_time_val_add (&tv, G_USEC_PER_SEC / 10);
  g_async_queue_lock (q);
  val = g_async_queue_timed_pop_unlocked (q, &tv);
  g_async_queue_unlock (q);
  g_assert (val == NULL);

  end = g_get_monotonic_time ();
  diff = end - start;
  g_assert_cmpint (diff, >=, G_USEC_PER_SEC / 10);
  g_assert_cmpint (diff, <, G_USEC_PER_SEC);

  g_async_queue_unref (q);
}

static void
test_async_queue_remove (void)
{
  GAsyncQueue *q;

  q = g_async_queue_new ();

  g_async_queue_push (q, GINT_TO_POINTER (10));
  g_async_queue_push (q, GINT_TO_POINTER (2));
  g_async_queue_push (q, GINT_TO_POINTER (7));
  g_async_queue_push (q, GINT_TO_POINTER (1));

  g_async_queue_remove (q, GINT_TO_POINTER (7));

  g_assert_cmpint (GPOINTER_TO_INT (g_async_queue_pop (q)), ==, 10);
  g_assert_cmpint (GPOINTER_TO_INT (g_async_queue_pop (q)), ==, 2);
  g_assert_cmpint (GPOINTER_TO_INT (g_async_queue_pop (q)), ==, 1);

  g_assert (g_async_queue_try_pop (q) == NULL);

  g_async_queue_unref (q);
}

static void
test_async_queue_push_front (void)
{
  GAsyncQueue *q;

  q = g_async_queue_new ();

  g_async_queue_push (q, GINT_TO_POINTER (10));
  g_async_queue_push (q, GINT_TO_POINTER (2));
  g_async_queue_push (q, GINT_TO_POINTER (7));

  g_async_queue_push_front (q, GINT_TO_POINTER (1));

  g_assert_cmpint (GPOINTER_TO_INT (g_async_queue_pop (q)), ==, 1);
  g_assert_cmpint (GPOINTER_TO_INT (g_async_queue_pop (q)), ==, 10);
  g_assert_cmpint (GPOINTER_TO_INT (g_async_queue_pop (q)), ==, 2);
  g_assert_cmpint (GPOINTER_TO_INT (g_async_queue_pop (q)), ==, 7);

  g_assert (g_async_queue_try_pop (q) == NULL);

  g_async_queue_unref (q);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/asyncqueue/sort", test_async_queue_sort);
  g_test_add_func ("/asyncqueue/destroy", test_async_queue_destroy);
  g_test_add_func ("/asyncqueue/threads", test_async_queue_threads);
  g_test_add_func ("/asyncqueue/timed", test_async_queue_timed);
  g_test_add_func ("/asyncqueue/remove", test_async_queue_remove);
  g_test_add_func ("/asyncqueue/push_front", test_async_queue_push_front);

  return g_test_run ();
}
