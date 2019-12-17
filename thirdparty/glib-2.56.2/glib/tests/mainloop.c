/* Unit tests for GMainLoop
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

#include <glib.h>
#include "glib-private.h"
#include <stdio.h>
#include <string.h>

static gboolean cb (gpointer data)
{
  return FALSE;
}

static gboolean prepare (GSource *source, gint *time)
{
  return FALSE;
}
static gboolean check (GSource *source)
{
  return FALSE;
}
static gboolean dispatch (GSource *source, GSourceFunc cb, gpointer date)
{
  return FALSE;
}

GSourceFuncs funcs = {
  prepare,
  check,
  dispatch,
  NULL
};

static void
test_maincontext_basic (void)
{
  GMainContext *ctx;
  GSource *source;
  guint id;
  gpointer data = &funcs;

  ctx = g_main_context_new ();

  g_assert (!g_main_context_pending (ctx));
  g_assert (!g_main_context_iteration (ctx, FALSE));

  source = g_source_new (&funcs, sizeof (GSource));
  g_assert_cmpint (g_source_get_priority (source), ==, G_PRIORITY_DEFAULT);
  g_assert (!g_source_is_destroyed (source));

  g_assert (!g_source_get_can_recurse (source));
  g_assert (g_source_get_name (source) == NULL);

  g_source_set_can_recurse (source, TRUE);
  g_source_set_name (source, "d");

  g_assert (g_source_get_can_recurse (source));
  g_assert_cmpstr (g_source_get_name (source), ==, "d");

  g_assert (g_main_context_find_source_by_user_data (ctx, NULL) == NULL);
  g_assert (g_main_context_find_source_by_funcs_user_data (ctx, &funcs, NULL) == NULL);

  id = g_source_attach (source, ctx);
  g_assert_cmpint (g_source_get_id (source), ==, id);
  g_assert (g_main_context_find_source_by_id (ctx, id) == source);

  g_source_set_priority (source, G_PRIORITY_HIGH);
  g_assert_cmpint (g_source_get_priority (source), ==, G_PRIORITY_HIGH);

  g_source_destroy (source);
  g_assert (g_source_get_context (source) == ctx);
  g_assert (g_main_context_find_source_by_id (ctx, id) == NULL);

  g_main_context_unref (ctx);

  if (g_test_undefined ())
    {
      g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL,
                             "*assertion*source->context != NULL*failed*");
      g_assert (g_source_get_context (source) == NULL);
      g_test_assert_expected_messages ();
    }

  g_source_unref (source);

  ctx = g_main_context_default ();
  source = g_source_new (&funcs, sizeof (GSource));
  g_source_set_funcs (source, &funcs);
  g_source_set_callback (source, cb, data, NULL);
  id = g_source_attach (source, ctx);
  g_source_unref (source);
  g_source_set_name_by_id (id, "e");
  g_assert_cmpstr (g_source_get_name (source), ==, "e");
  g_assert (g_source_get_context (source) == ctx);
  g_assert (g_source_remove_by_funcs_user_data (&funcs, data));

  source = g_source_new (&funcs, sizeof (GSource));
  g_source_set_funcs (source, &funcs);
  g_source_set_callback (source, cb, data, NULL);
  id = g_source_attach (source, ctx);
  g_source_unref (source);
  g_assert (g_source_remove_by_user_data (data));
  g_assert (!g_source_remove_by_user_data ((gpointer)0x1234));

  g_idle_add (cb, data);
  g_assert (g_idle_remove_by_data (data));
}

static void
test_mainloop_basic (void)
{
  GMainLoop *loop;
  GMainContext *ctx;

  loop = g_main_loop_new (NULL, FALSE);

  g_assert (!g_main_loop_is_running (loop));

  g_main_loop_ref (loop);

  ctx = g_main_loop_get_context (loop);
  g_assert (ctx == g_main_context_default ());

  g_main_loop_unref (loop);

  g_assert_cmpint (g_main_depth (), ==, 0);

  g_main_loop_unref (loop);
}

static gint a;
static gint b;
static gint c;

static gboolean
count_calls (gpointer data)
{
  gint *i = data;

  (*i)++;

  return TRUE;
}

static void
test_timeouts (void)
{
  GMainContext *ctx;
  GMainLoop *loop;
  GSource *source;

  a = b = c = 0;

  ctx = g_main_context_new ();
  loop = g_main_loop_new (ctx, FALSE);

  source = g_timeout_source_new (100);
  g_source_set_callback (source, count_calls, &a, NULL);
  g_source_attach (source, ctx);
  g_source_unref (source);

  source = g_timeout_source_new (250);
  g_source_set_callback (source, count_calls, &b, NULL);
  g_source_attach (source, ctx);
  g_source_unref (source);

  source = g_timeout_source_new (330);
  g_source_set_callback (source, count_calls, &c, NULL);
  g_source_attach (source, ctx);
  g_source_unref (source);

  source = g_timeout_source_new (1050);
  g_source_set_callback (source, (GSourceFunc)g_main_loop_quit, loop, NULL);
  g_source_attach (source, ctx);
  g_source_unref (source);

  g_main_loop_run (loop);

  /* We may be delayed for an arbitrary amount of time - for example,
   * it's possible for all timeouts to fire exactly once.
   */
  g_assert_cmpint (a, >, 0);
  g_assert_cmpint (a, >=, b);
  g_assert_cmpint (b, >=, c);

  g_assert_cmpint (a, <=, 10);
  g_assert_cmpint (b, <=, 4);
  g_assert_cmpint (c, <=, 3);

  g_main_loop_unref (loop);
  g_main_context_unref (ctx);
}

static void
test_priorities (void)
{
  GMainContext *ctx;
  GSource *sourcea;
  GSource *sourceb;

  a = b = c = 0;

  ctx = g_main_context_new ();

  sourcea = g_idle_source_new ();
  g_source_set_callback (sourcea, count_calls, &a, NULL);
  g_source_set_priority (sourcea, 1);
  g_source_attach (sourcea, ctx);
  g_source_unref (sourcea);

  sourceb = g_idle_source_new ();
  g_source_set_callback (sourceb, count_calls, &b, NULL);
  g_source_set_priority (sourceb, 0);
  g_source_attach (sourceb, ctx);
  g_source_unref (sourceb);

  g_assert (g_main_context_pending (ctx));
  g_assert (g_main_context_iteration (ctx, FALSE));
  g_assert_cmpint (a, ==, 0);
  g_assert_cmpint (b, ==, 1);

  g_assert (g_main_context_iteration (ctx, FALSE));
  g_assert_cmpint (a, ==, 0);
  g_assert_cmpint (b, ==, 2);

  g_source_destroy (sourceb);

  g_assert (g_main_context_iteration (ctx, FALSE));
  g_assert_cmpint (a, ==, 1);
  g_assert_cmpint (b, ==, 2);

  g_assert (g_main_context_pending (ctx));
  g_source_destroy (sourcea);
  g_assert (!g_main_context_pending (ctx));

  g_main_context_unref (ctx);
}

static gboolean
quit_loop (gpointer data)
{
  GMainLoop *loop = data;

  g_main_loop_quit (loop);

  return G_SOURCE_REMOVE;
}

static gint count;

static gboolean
func (gpointer data)
{
  if (data != NULL)
    g_assert (data == g_thread_self ());

  count++;

  return FALSE;
}

static gboolean
call_func (gpointer data)
{
  func (g_thread_self ());

  return G_SOURCE_REMOVE;
}

static GMutex mutex;
static GCond cond;
static gboolean thread_ready;

static gpointer
thread_func (gpointer data)
{
  GMainContext *ctx = data;
  GMainLoop *loop;
  GSource *source;

  g_main_context_push_thread_default (ctx);
  loop = g_main_loop_new (ctx, FALSE);

  g_mutex_lock (&mutex);
  thread_ready = TRUE;
  g_cond_signal (&cond);
  g_mutex_unlock (&mutex);

  source = g_timeout_source_new (500);
  g_source_set_callback (source, quit_loop, loop, NULL);
  g_source_attach (source, ctx);
  g_source_unref (source);

  g_main_loop_run (loop);

  g_main_context_pop_thread_default (ctx);
  g_main_loop_unref (loop);

  return NULL;
}

static void
test_invoke (void)
{
  GMainContext *ctx;
  GThread *thread;

  count = 0;

  /* this one gets invoked directly */
  g_main_context_invoke (NULL, func, g_thread_self ());
  g_assert_cmpint (count, ==, 1);

  /* invoking out of an idle works too */
  g_idle_add (call_func, NULL);
  g_main_context_iteration (g_main_context_default (), FALSE);
  g_assert_cmpint (count, ==, 2);

  /* test thread-default forcing the invocation to go
   * to another thread
   */
  ctx = g_main_context_new ();
  thread = g_thread_new ("worker", thread_func, ctx);

  g_mutex_lock (&mutex);
  while (!thread_ready)
    g_cond_wait (&cond, &mutex);
  g_mutex_unlock (&mutex);

  g_main_context_invoke (ctx, func, thread);

  g_thread_join (thread);
  g_assert_cmpint (count, ==, 3);

  g_main_context_unref (ctx);
}

/* We can't use timeout sources here because on slow or heavily-loaded
 * machines, the test program might not get enough cycles to hit the
 * timeouts at the expected times. So instead we define a source that
 * is based on the number of GMainContext iterations.
 */

static gint counter;
static gint64 last_counter_update;

typedef struct {
  GSource source;
  gint    interval;
  gint    timeout;
} CounterSource;

static gboolean
counter_source_prepare (GSource *source,
                        gint    *timeout)
{
  CounterSource *csource = (CounterSource *)source;
  gint64 now;

  now = g_source_get_time (source);
  if (now != last_counter_update)
    {
      last_counter_update = now;
      counter++;
    }

  *timeout = 1;
  return counter >= csource->timeout;
}

static gboolean
counter_source_dispatch (GSource    *source,
                         GSourceFunc callback,
                         gpointer    user_data)
{
  CounterSource *csource = (CounterSource *) source;
  gboolean again;

  again = callback (user_data);

  if (again)
    csource->timeout = counter + csource->interval;

  return again;
}

static GSourceFuncs counter_source_funcs = {
  counter_source_prepare,
  NULL,
  counter_source_dispatch,
  NULL,
};

static GSource *
counter_source_new (gint interval)
{
  GSource *source = g_source_new (&counter_source_funcs, sizeof (CounterSource));
  CounterSource *csource = (CounterSource *) source;

  csource->interval = interval;
  csource->timeout = counter + interval;

  return source;
}


static gboolean
run_inner_loop (gpointer user_data)
{
  GMainContext *ctx = user_data;
  GMainLoop *inner;
  GSource *timeout;

  a++;

  inner = g_main_loop_new (ctx, FALSE);
  timeout = counter_source_new (100);
  g_source_set_callback (timeout, quit_loop, inner, NULL);
  g_source_attach (timeout, ctx);
  g_source_unref (timeout);

  g_main_loop_run (inner);
  g_main_loop_unref (inner);

  return G_SOURCE_CONTINUE;
}

static void
test_child_sources (void)
{
  GMainContext *ctx;
  GMainLoop *loop;
  GSource *parent, *child_b, *child_c, *end;

  ctx = g_main_context_new ();
  loop = g_main_loop_new (ctx, FALSE);

  a = b = c = 0;

  parent = counter_source_new (2000);
  g_source_set_callback (parent, run_inner_loop, ctx, NULL);
  g_source_set_priority (parent, G_PRIORITY_LOW);
  g_source_attach (parent, ctx);

  child_b = counter_source_new (250);
  g_source_set_callback (child_b, count_calls, &b, NULL);
  g_source_add_child_source (parent, child_b);

  child_c = counter_source_new (330);
  g_source_set_callback (child_c, count_calls, &c, NULL);
  g_source_set_priority (child_c, G_PRIORITY_HIGH);
  g_source_add_child_source (parent, child_c);

  /* Child sources always have the priority of the parent */
  g_assert_cmpint (g_source_get_priority (parent), ==, G_PRIORITY_LOW);
  g_assert_cmpint (g_source_get_priority (child_b), ==, G_PRIORITY_LOW);
  g_assert_cmpint (g_source_get_priority (child_c), ==, G_PRIORITY_LOW);
  g_source_set_priority (parent, G_PRIORITY_DEFAULT);
  g_assert_cmpint (g_source_get_priority (parent), ==, G_PRIORITY_DEFAULT);
  g_assert_cmpint (g_source_get_priority (child_b), ==, G_PRIORITY_DEFAULT);
  g_assert_cmpint (g_source_get_priority (child_c), ==, G_PRIORITY_DEFAULT);

  end = counter_source_new (1050);
  g_source_set_callback (end, quit_loop, loop, NULL);
  g_source_attach (end, ctx);
  g_source_unref (end);

  g_main_loop_run (loop);

  /* The parent source's own timeout will never trigger, so "a" will
   * only get incremented when "b" or "c" does. And when timeouts get
   * blocked, they still wait the full interval next time rather than
   * "catching up". So the timing is:
   *
   *  250 - b++ -> a++, run_inner_loop
   *  330 - (c is blocked)
   *  350 - inner_loop ends
   *  350 - c++ belatedly -> a++, run_inner_loop
   *  450 - inner loop ends
   *  500 - b++ -> a++, run_inner_loop
   *  600 - inner_loop ends
   *  680 - c++ -> a++, run_inner_loop
   *  750 - (b is blocked)
   *  780 - inner loop ends
   *  780 - b++ belatedly -> a++, run_inner_loop
   *  880 - inner loop ends
   * 1010 - c++ -> a++, run_inner_loop
   * 1030 - (b is blocked)
   * 1050 - end runs, quits outer loop, which has no effect yet
   * 1110 - inner loop ends, a returns, outer loop exits
   */

  g_assert_cmpint (a, ==, 6);
  g_assert_cmpint (b, ==, 3);
  g_assert_cmpint (c, ==, 3);

  g_source_destroy (parent);
  g_source_unref (parent);
  g_source_unref (child_b);
  g_source_unref (child_c);

  g_main_loop_unref (loop);
  g_main_context_unref (ctx);
}

static void
test_recursive_child_sources (void)
{
  GMainContext *ctx;
  GMainLoop *loop;
  GSource *parent, *child_b, *child_c, *end;

  ctx = g_main_context_new ();
  loop = g_main_loop_new (ctx, FALSE);

  a = b = c = 0;

  parent = counter_source_new (500);
  g_source_set_callback (parent, count_calls, &a, NULL);

  child_b = counter_source_new (220);
  g_source_set_callback (child_b, count_calls, &b, NULL);
  g_source_add_child_source (parent, child_b);

  child_c = counter_source_new (430);
  g_source_set_callback (child_c, count_calls, &c, NULL);
  g_source_add_child_source (child_b, child_c);

  g_source_attach (parent, ctx);

  end = counter_source_new (2010);
  g_source_set_callback (end, (GSourceFunc)g_main_loop_quit, loop, NULL);
  g_source_attach (end, ctx);
  g_source_unref (end);

  g_main_loop_run (loop);

  /* Sequence of events:
   *  220 b (b -> 440, a -> 720)
   *  430 c (c -> 860, b -> 650, a -> 930)
   *  650 b (b -> 870, a -> 1150)
   *  860 c (c -> 1290, b -> 1080, a -> 1360)
   * 1080 b (b -> 1300, a -> 1580)
   * 1290 c (c -> 1720, b -> 1510, a -> 1790)
   * 1510 b (b -> 1730, a -> 2010)
   * 1720 c (c -> 2150, b -> 1940, a -> 2220)
   * 1940 b (b -> 2160, a -> 2440)
   */

  g_assert_cmpint (a, ==, 9);
  g_assert_cmpint (b, ==, 9);
  g_assert_cmpint (c, ==, 4);

  g_source_destroy (parent);
  g_source_unref (parent);
  g_source_unref (child_b);
  g_source_unref (child_c);

  g_main_loop_unref (loop);
  g_main_context_unref (ctx);
}

typedef struct {
  GSource *parent, *old_child, *new_child;
  GMainLoop *loop;
} SwappingTestData;

static gboolean
swap_sources (gpointer user_data)
{
  SwappingTestData *data = user_data;

  if (data->old_child)
    {
      g_source_remove_child_source (data->parent, data->old_child);
      g_clear_pointer (&data->old_child, g_source_unref);
    }

  if (!data->new_child)
    {
      data->new_child = g_timeout_source_new (0);
      g_source_set_callback (data->new_child, quit_loop, data->loop, NULL);
      g_source_add_child_source (data->parent, data->new_child);
    }

  return G_SOURCE_CONTINUE;
}

static gboolean
assert_not_reached_callback (gpointer user_data)
{
  g_assert_not_reached ();

  return G_SOURCE_REMOVE;
}

static void
test_swapping_child_sources (void)
{
  GMainContext *ctx;
  GMainLoop *loop;
  SwappingTestData data;

  ctx = g_main_context_new ();
  loop = g_main_loop_new (ctx, FALSE);

  data.parent = counter_source_new (50);
  data.loop = loop;
  g_source_set_callback (data.parent, swap_sources, &data, NULL);
  g_source_attach (data.parent, ctx);

  data.old_child = counter_source_new (100);
  g_source_add_child_source (data.parent, data.old_child);
  g_source_set_callback (data.old_child, assert_not_reached_callback, NULL, NULL);

  data.new_child = NULL;
  g_main_loop_run (loop);

  g_source_destroy (data.parent);
  g_source_unref (data.parent);
  g_source_unref (data.new_child);

  g_main_loop_unref (loop);
  g_main_context_unref (ctx);
}

static gboolean
add_source_callback (gpointer user_data)
{
  GMainLoop *loop = user_data;
  GSource *self = g_main_current_source (), *child;
  GIOChannel *io;

  /* It doesn't matter whether this is a valid fd or not; it never
   * actually gets polled; the test is just checking that
   * g_source_add_child_source() doesn't crash.
   */
  io = g_io_channel_unix_new (0);
  child = g_io_create_watch (io, G_IO_IN);
  g_source_add_child_source (self, child);
  g_source_unref (child);
  g_io_channel_unref (io);

  g_main_loop_quit (loop);
  return FALSE;
}

static void
test_blocked_child_sources (void)
{
  GMainContext *ctx;
  GMainLoop *loop;
  GSource *source;

  g_test_bug ("701283");

  ctx = g_main_context_new ();
  loop = g_main_loop_new (ctx, FALSE);

  source = g_idle_source_new ();
  g_source_set_callback (source, add_source_callback, loop, NULL);
  g_source_attach (source, ctx);

  g_main_loop_run (loop);

  g_source_destroy (source);
  g_source_unref (source);

  g_main_loop_unref (loop);
  g_main_context_unref (ctx);
}

typedef struct {
  GMainContext *ctx;
  GMainLoop *loop;

  GSource *timeout1, *timeout2;
  gint64 time1;
  GTimeVal tv;
} TimeTestData;

static gboolean
timeout1_callback (gpointer user_data)
{
  TimeTestData *data = user_data;
  GSource *source;
  gint64 mtime1, mtime2, time2;

  source = g_main_current_source ();
  g_assert (source == data->timeout1);

  if (data->time1 == -1)
    {
      /* First iteration */
      g_assert (!g_source_is_destroyed (data->timeout2));

      mtime1 = g_get_monotonic_time ();
      data->time1 = g_source_get_time (source);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      g_source_get_current_time (source, &data->tv);
G_GNUC_END_IGNORE_DEPRECATIONS

      /* g_source_get_time() does not change during a single callback */
      g_usleep (1000000);
      mtime2 = g_get_monotonic_time ();
      time2 = g_source_get_time (source);

      g_assert_cmpint (mtime1, <, mtime2);
      g_assert_cmpint (data->time1, ==, time2);
    }
  else
    {
      GTimeVal tv;

      /* Second iteration */
      g_assert (g_source_is_destroyed (data->timeout2));

      /* g_source_get_time() MAY change between iterations; in this
       * case we know for sure that it did because of the g_usleep()
       * last time.
       */
      time2 = g_source_get_time (source);
      g_assert_cmpint (data->time1, <, time2);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      g_source_get_current_time (source, &tv);
G_GNUC_END_IGNORE_DEPRECATIONS

      g_assert (tv.tv_sec > data->tv.tv_sec ||
                (tv.tv_sec == data->tv.tv_sec &&
                 tv.tv_usec > data->tv.tv_usec));

      g_main_loop_quit (data->loop);
    }

  return TRUE;
}

static gboolean
timeout2_callback (gpointer user_data)
{
  TimeTestData *data = user_data;
  GSource *source;
  gint64 time2, time3;

  source = g_main_current_source ();
  g_assert (source == data->timeout2);

  g_assert (!g_source_is_destroyed (data->timeout1));

  /* g_source_get_time() does not change between different sources in
   * a single iteration of the mainloop.
   */
  time2 = g_source_get_time (source);
  g_assert_cmpint (data->time1, ==, time2);

  /* The source should still have a valid time even after being
   * destroyed, since it's currently running.
   */
  g_source_destroy (source);
  time3 = g_source_get_time (source);
  g_assert_cmpint (time2, ==, time3);

  return FALSE;
}

static void
test_source_time (void)
{
  TimeTestData data;

  data.ctx = g_main_context_new ();
  data.loop = g_main_loop_new (data.ctx, FALSE);

  data.timeout1 = g_timeout_source_new (0);
  g_source_set_callback (data.timeout1, timeout1_callback, &data, NULL);
  g_source_attach (data.timeout1, data.ctx);

  data.timeout2 = g_timeout_source_new (0);
  g_source_set_callback (data.timeout2, timeout2_callback, &data, NULL);
  g_source_attach (data.timeout2, data.ctx);

  data.time1 = -1;

  g_main_loop_run (data.loop);

  g_assert (!g_source_is_destroyed (data.timeout1));
  g_assert (g_source_is_destroyed (data.timeout2));

  g_source_destroy (data.timeout1);
  g_source_unref (data.timeout1);
  g_source_unref (data.timeout2);

  g_main_loop_unref (data.loop);
  g_main_context_unref (data.ctx);
}

typedef struct {
  guint outstanding_ops;
  GMainLoop *loop;
} TestOverflowData;

static gboolean
on_source_fired_cb (gpointer user_data)
{
  TestOverflowData *data = user_data;
  GSource *current_source;
  GMainContext *current_context;
  guint source_id;

  data->outstanding_ops--;

  current_source = g_main_current_source ();
  current_context = g_source_get_context (current_source);
  source_id = g_source_get_id (current_source);
  g_assert (g_main_context_find_source_by_id (current_context, source_id) != NULL);
  g_source_destroy (current_source);
  g_assert (g_main_context_find_source_by_id (current_context, source_id) == NULL);

  if (data->outstanding_ops == 0)
    g_main_loop_quit (data->loop);
  return FALSE;
}

static GSource *
add_idle_source (GMainContext *ctx,
                 TestOverflowData *data)
{
  GSource *source;

  source = g_idle_source_new ();
  g_source_set_callback (source, on_source_fired_cb, data, NULL);
  g_source_attach (source, ctx);
  g_source_unref (source);
  data->outstanding_ops++;

  return source;
}

static void
test_mainloop_overflow (void)
{
  GMainContext *ctx;
  GMainLoop *loop;
  GSource *source;
  TestOverflowData data;
  guint i;

  g_test_bug ("687098");

  memset (&data, 0, sizeof (data));

  ctx = GLIB_PRIVATE_CALL (g_main_context_new_with_next_id) (G_MAXUINT-1);

  loop = g_main_loop_new (ctx, TRUE);
  data.outstanding_ops = 0;
  data.loop = loop;

  source = add_idle_source (ctx, &data);
  g_assert_cmpint (source->source_id, ==, G_MAXUINT-1);

  source = add_idle_source (ctx, &data);
  g_assert_cmpint (source->source_id, ==, G_MAXUINT);

  source = add_idle_source (ctx, &data);
  g_assert_cmpint (source->source_id, !=, 0);

  /* Now, a lot more sources */
  for (i = 0; i < 50; i++)
    {
      source = add_idle_source (ctx, &data);
      g_assert_cmpint (source->source_id, !=, 0);
    }

  g_main_loop_run (loop);
  g_assert_cmpint (data.outstanding_ops, ==, 0);

  g_main_loop_unref (loop);
  g_main_context_unref (ctx);
}

static volatile gint ready_time_dispatched;

static gboolean
ready_time_dispatch (GSource     *source,
                     GSourceFunc  callback,
                     gpointer     user_data)
{
  g_atomic_int_set (&ready_time_dispatched, TRUE);

  g_source_set_ready_time (source, -1);

  return TRUE;
}

static gpointer
run_context (gpointer user_data)
{
  g_main_loop_run (user_data);

  return NULL;
}

static void
test_ready_time (void)
{
  GThread *thread;
  GSource *source;
  GSourceFuncs source_funcs = {
    NULL, NULL, ready_time_dispatch
  };
  GMainLoop *loop;

  source = g_source_new (&source_funcs, sizeof (GSource));
  g_source_attach (source, NULL);
  g_source_unref (source);

  /* Unfortunately we can't do too many things with respect to timing
   * without getting into trouble on slow systems or heavily loaded
   * builders.
   *
   * We can test that the basics are working, though.
   */

  /* A source with no ready time set should not fire */
  g_assert_cmpint (g_source_get_ready_time (source), ==, -1);
  while (g_main_context_iteration (NULL, FALSE));
  g_assert (!ready_time_dispatched);

  /* The ready time should not have been changed */
  g_assert_cmpint (g_source_get_ready_time (source), ==, -1);

  /* Of course this shouldn't change anything either */
  g_source_set_ready_time (source, -1);
  g_assert_cmpint (g_source_get_ready_time (source), ==, -1);

  /* A source with a ready time set to tomorrow should not fire on any
   * builder, no matter how badly loaded...
   */
  g_source_set_ready_time (source, g_get_monotonic_time () + G_TIME_SPAN_DAY);
  while (g_main_context_iteration (NULL, FALSE));
  g_assert (!ready_time_dispatched);
  /* Make sure it didn't get reset */
  g_assert_cmpint (g_source_get_ready_time (source), !=, -1);

  /* Ready time of -1 -> don't fire */
  g_source_set_ready_time (source, -1);
  while (g_main_context_iteration (NULL, FALSE));
  g_assert (!ready_time_dispatched);
  /* Not reset, but should still be -1 from above */
  g_assert_cmpint (g_source_get_ready_time (source), ==, -1);

  /* A ready time of the current time should fire immediately */
  g_source_set_ready_time (source, g_get_monotonic_time ());
  while (g_main_context_iteration (NULL, FALSE));
  g_assert (ready_time_dispatched);
  ready_time_dispatched = FALSE;
  /* Should have gotten reset by the handler function */
  g_assert_cmpint (g_source_get_ready_time (source), ==, -1);

  /* As well as one in the recent past... */
  g_source_set_ready_time (source, g_get_monotonic_time () - G_TIME_SPAN_SECOND);
  while (g_main_context_iteration (NULL, FALSE));
  g_assert (ready_time_dispatched);
  ready_time_dispatched = FALSE;
  g_assert_cmpint (g_source_get_ready_time (source), ==, -1);

  /* Zero is the 'official' way to get a source to fire immediately */
  g_source_set_ready_time (source, 0);
  while (g_main_context_iteration (NULL, FALSE));
  g_assert (ready_time_dispatched);
  ready_time_dispatched = FALSE;
  g_assert_cmpint (g_source_get_ready_time (source), ==, -1);

  /* Now do some tests of cross-thread wakeups.
   *
   * Make sure it wakes up right away from the start.
   */
  g_source_set_ready_time (source, 0);
  loop = g_main_loop_new (NULL, FALSE);
  thread = g_thread_new ("context thread", run_context, loop);
  while (!g_atomic_int_get (&ready_time_dispatched));

  /* Now let's see if it can wake up from sleeping. */
  g_usleep (G_TIME_SPAN_SECOND / 2);
  g_atomic_int_set (&ready_time_dispatched, FALSE);
  g_source_set_ready_time (source, 0);
  while (!g_atomic_int_get (&ready_time_dispatched));

  /* kill the thread */
  g_main_loop_quit (loop);
  g_thread_join (thread);
  g_main_loop_unref (loop);

  g_source_destroy (source);
}

static void
test_wakeup(void)
{
  GMainContext *ctx;
  int i;

  ctx = g_main_context_new ();

  /* run a random large enough number of times because 
   * main contexts tend to wake up a few times after creation.
   */
  for (i = 0; i < 100; i++)
    {
      /* This is the invariant we care about:
       * g_main_context_wakeup(ctx,) ensures that the next call to
       * g_main_context_iteration (ctx, TRUE) returns and doesn't
       * block.
       * This is important in threaded apps where we might not know
       * if the thread calls g_main_context_wakeup() before or after
       * we enter g_main_context_iteration().
       */
      g_main_context_wakeup (ctx);
      g_main_context_iteration (ctx, TRUE);
    }

  g_main_context_unref (ctx);
}

static void
test_remove_invalid (void)
{
  g_test_expect_message ("GLib", G_LOG_LEVEL_CRITICAL, "Source ID 3000000000 was not found*");
  g_source_remove (3000000000u);
  g_test_assert_expected_messages ();
}

static gboolean
trivial_prepare (GSource *source,
                 gint    *timeout)
{
  *timeout = 0;
  return TRUE;
}

static gint n_finalized;

static void
trivial_finalize (GSource *source)
{
  n_finalized++;
}

static void
test_unref_while_pending (void)
{
  static GSourceFuncs funcs = { trivial_prepare, NULL, NULL, trivial_finalize };
  GMainContext *context;
  GSource *source;

  context = g_main_context_new ();

  source = g_source_new (&funcs, sizeof (GSource));
  g_source_attach (source, context);
  g_source_unref (source);

  /* Do incomplete main iteration -- get a pending source but don't dispatch it. */
  g_main_context_prepare (context, NULL);
  g_main_context_query (context, 0, NULL, NULL, 0);
  g_main_context_check (context, 1000, NULL, 0);

  /* Destroy the context */
  g_main_context_unref (context);

  /* Make sure we didn't leak the source */
  g_assert_cmpint (n_finalized, ==, 1);
}

#ifdef G_OS_UNIX

#include <glib-unix.h>
#include <unistd.h>

static gchar zeros[1024];

static gsize
fill_a_pipe (gint fd)
{
  gsize written = 0;
  GPollFD pfd;

  pfd.fd = fd;
  pfd.events = G_IO_OUT;
  while (g_poll (&pfd, 1, 0) == 1)
    /* we should never see -1 here */
    written += write (fd, zeros, sizeof zeros);

  return written;
}

static gboolean
write_bytes (gint         fd,
             GIOCondition condition,
             gpointer     user_data)
{
  gssize *to_write = user_data;
  gint limit;

  if (*to_write == 0)
    return FALSE;

  /* Detect if we run before we should */
  g_assert (*to_write >= 0);

  limit = MIN (*to_write, sizeof zeros);
  *to_write -= write (fd, zeros, limit);

  return TRUE;
}

static gboolean
read_bytes (gint         fd,
            GIOCondition condition,
            gpointer     user_data)
{
  static gchar buffer[1024];
  gssize *to_read = user_data;

  *to_read -= read (fd, buffer, sizeof buffer);

  /* The loop will exit when there is nothing else to read, then we will
   * use g_source_remove() to destroy this source.
   */
  return TRUE;
}

static void
test_unix_fd (void)
{
  gssize to_write = -1;
  gssize to_read;
  gint fds[2];
  gint a, b;
  gint s;
  GSource *source_a;
  GSource *source_b;

  s = pipe (fds);
  g_assert (s == 0);

  to_read = fill_a_pipe (fds[1]);
  /* write at higher priority to keep the pipe full... */
  a = g_unix_fd_add_full (G_PRIORITY_HIGH, fds[1], G_IO_OUT, write_bytes, &to_write, NULL);
  source_a = g_source_ref (g_main_context_find_source_by_id (NULL, a));
  /* make sure no 'writes' get dispatched yet */
  while (g_main_context_iteration (NULL, FALSE));

  to_read += 128 * 1024 * 1024;
  to_write = 128 * 1024 * 1024;
  b = g_unix_fd_add (fds[0], G_IO_IN, read_bytes, &to_read);
  source_b = g_source_ref (g_main_context_find_source_by_id (NULL, b));

  /* Assuming the kernel isn't internally 'laggy' then there will always
   * be either data to read or room in which to write.  That will keep
   * the loop running until all data has been read and written.
   */
  while (TRUE)
    {
      gssize to_write_was = to_write;
      gssize to_read_was = to_read;

      if (!g_main_context_iteration (NULL, FALSE))
        break;

      /* Since the sources are at different priority, only one of them
       * should possibly have run.
       */
      g_assert (to_write == to_write_was || to_read == to_read_was);
    }

  g_assert (to_write == 0);
  g_assert (to_read == 0);

  /* 'a' is already removed by itself */
  g_assert (g_source_is_destroyed (source_a));
  g_source_unref (source_a);
  g_source_remove (b);
  g_assert (g_source_is_destroyed (source_b));
  g_source_unref (source_b);
  close (fds[1]);
  close (fds[0]);
}

static void
assert_main_context_state (gint n_to_poll,
                           ...)
{
  GMainContext *context;
  gboolean consumed[10] = { };
  GPollFD poll_fds[10];
  gboolean acquired;
  gboolean immediate;
  gint max_priority;
  gint timeout;
  gint n;
  gint i, j;
  va_list ap;

  context = g_main_context_default ();

  acquired = g_main_context_acquire (context);
  g_assert (acquired);

  immediate = g_main_context_prepare (context, &max_priority);
  g_assert (!immediate);
  n = g_main_context_query (context, max_priority, &timeout, poll_fds, 10);
  g_assert_cmpint (n, ==, n_to_poll + 1); /* one will be the gwakeup */

  va_start (ap, n_to_poll);
  for (i = 0; i < n_to_poll; i++)
    {
      gint expected_fd = va_arg (ap, gint);
      GIOCondition expected_events = va_arg (ap, GIOCondition);
      GIOCondition report_events = va_arg (ap, GIOCondition);

      for (j = 0; j < n; j++)
        if (!consumed[j] && poll_fds[j].fd == expected_fd && poll_fds[j].events == expected_events)
          {
            poll_fds[j].revents = report_events;
            consumed[j] = TRUE;
            break;
          }

      if (j == n)
        g_error ("Unable to find fd %d (index %d) with events 0x%x\n", expected_fd, i, (guint) expected_events);
    }
  va_end (ap);

  /* find the gwakeup, flag as non-ready */
  for (i = 0; i < n; i++)
    if (!consumed[i])
      poll_fds[i].revents = 0;

  if (g_main_context_check (context, max_priority, poll_fds, n))
    g_main_context_dispatch (context);

  g_main_context_release (context);
}

static gboolean
flag_bool (gint         fd,
           GIOCondition condition,
           gpointer     user_data)
{
  gboolean *flag = user_data;

  *flag = TRUE;

  return TRUE;
}

static void
test_unix_fd_source (void)
{
  GSource *out_source;
  GSource *in_source;
  GSource *source;
  gboolean out, in;
  gint fds[2];
  gint s;

  assert_main_context_state (0);

  s = pipe (fds);
  g_assert (s == 0);

  source = g_unix_fd_source_new (fds[1], G_IO_OUT);
  g_source_attach (source, NULL);

  /* Check that a source with no callback gets successfully detached
   * with a warning printed.
   */
  g_test_expect_message ("GLib", G_LOG_LEVEL_WARNING, "*GUnixFDSource dispatched without callback*");
  while (g_main_context_iteration (NULL, FALSE));
  g_test_assert_expected_messages ();
  g_assert (g_source_is_destroyed (source));
  g_source_unref (source);

  out = in = FALSE;
  out_source = g_unix_fd_source_new (fds[1], G_IO_OUT);
  g_source_set_callback (out_source, (GSourceFunc) flag_bool, &out, NULL);
  g_source_attach (out_source, NULL);
  assert_main_context_state (1,
                             fds[1], G_IO_OUT, 0);
  g_assert (!in && !out);

  in_source = g_unix_fd_source_new (fds[0], G_IO_IN);
  g_source_set_callback (in_source, (GSourceFunc) flag_bool, &in, NULL);
  g_source_set_priority (in_source, G_PRIORITY_DEFAULT_IDLE);
  g_source_attach (in_source, NULL);
  assert_main_context_state (2,
                             fds[0], G_IO_IN, G_IO_IN,
                             fds[1], G_IO_OUT, G_IO_OUT);
  /* out is higher priority so only it should fire */
  g_assert (!in && out);

  /* raise the priority of the in source to higher than out*/
  in = out = FALSE;
  g_source_set_priority (in_source, G_PRIORITY_HIGH);
  assert_main_context_state (2,
                             fds[0], G_IO_IN, G_IO_IN,
                             fds[1], G_IO_OUT, G_IO_OUT);
  g_assert (in && !out);

  /* now, let them be equal */
  in = out = FALSE;
  g_source_set_priority (in_source, G_PRIORITY_DEFAULT);
  assert_main_context_state (2,
                             fds[0], G_IO_IN, G_IO_IN,
                             fds[1], G_IO_OUT, G_IO_OUT);
  g_assert (in && out);

  g_source_destroy (out_source);
  g_source_unref (out_source);
  g_source_destroy (in_source);
  g_source_unref (in_source);
  close (fds[1]);
  close (fds[0]);
}

typedef struct
{
  GSource parent;
  gboolean flagged;
} FlagSource;

static gboolean
return_true (GSource *source, GSourceFunc callback, gpointer user_data)
{
  FlagSource *flag_source = (FlagSource *) source;

  flag_source->flagged = TRUE;

  return TRUE;
}

#define assert_flagged(s) g_assert (((FlagSource *) (s))->flagged);
#define assert_not_flagged(s) g_assert (!((FlagSource *) (s))->flagged);
#define clear_flag(s) ((FlagSource *) (s))->flagged = 0

static void
test_source_unix_fd_api (void)
{
  GSourceFuncs no_funcs = {
    NULL, NULL, return_true
  };
  GSource *source_a;
  GSource *source_b;
  gpointer tag1, tag2;
  gint fds_a[2];
  gint fds_b[2];

  pipe (fds_a);
  pipe (fds_b);

  source_a = g_source_new (&no_funcs, sizeof (FlagSource));
  source_b = g_source_new (&no_funcs, sizeof (FlagSource));

  /* attach a source with more than one fd */
  g_source_add_unix_fd (source_a, fds_a[0], G_IO_IN);
  g_source_add_unix_fd (source_a, fds_a[1], G_IO_OUT);
  g_source_attach (source_a, NULL);
  assert_main_context_state (2,
                             fds_a[0], G_IO_IN, 0,
                             fds_a[1], G_IO_OUT, 0);
  assert_not_flagged (source_a);

  /* attach a higher priority source with no fds */
  g_source_set_priority (source_b, G_PRIORITY_HIGH);
  g_source_attach (source_b, NULL);
  assert_main_context_state (2,
                             fds_a[0], G_IO_IN, G_IO_IN,
                             fds_a[1], G_IO_OUT, 0);
  assert_flagged (source_a);
  assert_not_flagged (source_b);
  clear_flag (source_a);

  /* add some fds to the second source, while attached */
  tag1 = g_source_add_unix_fd (source_b, fds_b[0], G_IO_IN);
  tag2 = g_source_add_unix_fd (source_b, fds_b[1], G_IO_OUT);
  assert_main_context_state (4,
                             fds_a[0], G_IO_IN, 0,
                             fds_a[1], G_IO_OUT, G_IO_OUT,
                             fds_b[0], G_IO_IN, 0,
                             fds_b[1], G_IO_OUT, G_IO_OUT);
  /* only 'b' (higher priority) should have dispatched */
  assert_not_flagged (source_a);
  assert_flagged (source_b);
  clear_flag (source_b);

  /* change our events on b to the same as they were before */
  g_source_modify_unix_fd (source_b, tag1, G_IO_IN);
  g_source_modify_unix_fd (source_b, tag2, G_IO_OUT);
  assert_main_context_state (4,
                             fds_a[0], G_IO_IN, 0,
                             fds_a[1], G_IO_OUT, G_IO_OUT,
                             fds_b[0], G_IO_IN, 0,
                             fds_b[1], G_IO_OUT, G_IO_OUT);
  assert_not_flagged (source_a);
  assert_flagged (source_b);
  clear_flag (source_b);

  /* now reverse them */
  g_source_modify_unix_fd (source_b, tag1, G_IO_OUT);
  g_source_modify_unix_fd (source_b, tag2, G_IO_IN);
  assert_main_context_state (4,
                             fds_a[0], G_IO_IN, 0,
                             fds_a[1], G_IO_OUT, G_IO_OUT,
                             fds_b[0], G_IO_OUT, 0,
                             fds_b[1], G_IO_IN, 0);
  /* 'b' had no events, so 'a' can go this time */
  assert_flagged (source_a);
  assert_not_flagged (source_b);
  clear_flag (source_a);

  /* remove one of the fds from 'b' */
  g_source_remove_unix_fd (source_b, tag1);
  assert_main_context_state (3,
                             fds_a[0], G_IO_IN, 0,
                             fds_a[1], G_IO_OUT, 0,
                             fds_b[1], G_IO_IN, 0);
  assert_not_flagged (source_a);
  assert_not_flagged (source_b);

  /* remove the other */
  g_source_remove_unix_fd (source_b, tag2);
  assert_main_context_state (2,
                             fds_a[0], G_IO_IN, 0,
                             fds_a[1], G_IO_OUT, 0);
  assert_not_flagged (source_a);
  assert_not_flagged (source_b);

  /* destroy the sources */
  g_source_destroy (source_a);
  g_source_destroy (source_b);
  assert_main_context_state (0);

  g_source_unref (source_a);
  g_source_unref (source_b);
  close (fds_a[0]);
  close (fds_a[1]);
  close (fds_b[0]);
  close (fds_b[1]);
}

static gboolean
unixfd_quit_loop (gint         fd,
                  GIOCondition condition,
                  gpointer     user_data)
{
  GMainLoop *loop = user_data;

  g_main_loop_quit (loop);

  return FALSE;
}

static void
test_unix_file_poll (void)
{
  gint fd;
  GSource *source;
  GMainLoop *loop;

  fd = open ("/dev/null", O_RDONLY);
  g_assert (fd >= 0);

  loop = g_main_loop_new (NULL, FALSE);

  source = g_unix_fd_source_new (fd, G_IO_IN);
  g_source_set_callback (source, (GSourceFunc) unixfd_quit_loop, loop, NULL);
  g_source_attach (source, NULL);

  /* Should not block */
  g_main_loop_run (loop);

  g_source_destroy (source);

  assert_main_context_state (0);

  g_source_unref (source);

  g_main_loop_unref (loop);

  close (fd);
}

#endif

static gboolean
timeout_cb (gpointer data)
{
  GMainLoop *loop = data;
  GMainContext *context;

  context = g_main_loop_get_context (loop);
  g_assert (g_main_loop_is_running (loop));
  g_assert (g_main_context_is_owner (context));

  g_main_loop_quit (loop);

  return G_SOURCE_REMOVE;
}

static gpointer
threadf (gpointer data)
{
  GMainContext *context = data;
  GMainLoop *loop;
  GSource *source;

  loop = g_main_loop_new (context, FALSE);
  source = g_timeout_source_new (250);
  g_source_set_callback (source, timeout_cb, loop, NULL);
  g_source_attach (source, context);
  g_source_unref (source);
 
  g_main_loop_run (loop);

  g_main_loop_unref (loop);

  return NULL;
}

static void
test_mainloop_wait (void)
{
  GMainContext *context;
  GThread *t1, *t2;

  context = g_main_context_new ();

  t1 = g_thread_new ("t1", threadf, context);
  t2 = g_thread_new ("t2", threadf, context);

  g_thread_join (t1);
  g_thread_join (t2);

  g_main_context_unref (context);
}

static gboolean
nfds_in_cb (GIOChannel   *io,
            GIOCondition  condition,
            gpointer      user_data)
{
  gboolean *in_cb_ran = user_data;

  *in_cb_ran = TRUE;
  g_assert_cmpint (condition, ==, G_IO_IN);
  return FALSE;
}

static gboolean
nfds_out_cb (GIOChannel   *io,
             GIOCondition  condition,
             gpointer      user_data)
{
  gboolean *out_cb_ran = user_data;

  *out_cb_ran = TRUE;
  g_assert_cmpint (condition, ==, G_IO_OUT);
  return FALSE;
}

static gboolean
nfds_out_low_cb (GIOChannel   *io,
                 GIOCondition  condition,
                 gpointer      user_data)
{
  g_assert_not_reached ();
  return FALSE;
}

static void
test_nfds (void)
{
  GMainContext *ctx;
  GPollFD out_fds[3];
  gint fd, nfds;
  GIOChannel *io;
  GSource *source1, *source2, *source3;
  gboolean source1_ran = FALSE, source3_ran = FALSE;
  gchar *tmpfile;
  GError *error = NULL;

  ctx = g_main_context_new ();
  nfds = g_main_context_query (ctx, G_MAXINT, NULL,
                               out_fds, G_N_ELEMENTS (out_fds));
  /* An "empty" GMainContext will have a single GPollFD, for its
   * internal GWakeup.
   */
  g_assert_cmpint (nfds, ==, 1);

  fd = g_file_open_tmp (NULL, &tmpfile, &error);
  g_assert_no_error (error);

  io = g_io_channel_unix_new (fd);
#ifdef G_OS_WIN32
  /* The fd in the pollfds won't be the same fd we passed in */
  g_io_channel_win32_make_pollfd (io, G_IO_IN, out_fds);
  fd = out_fds[0].fd;
#endif

  /* Add our first pollfd */
  source1 = g_io_create_watch (io, G_IO_IN);
  g_source_set_priority (source1, G_PRIORITY_DEFAULT);
  g_source_set_callback (source1, (GSourceFunc) nfds_in_cb,
                         &source1_ran, NULL);
  g_source_attach (source1, ctx);

  nfds = g_main_context_query (ctx, G_MAXINT, NULL,
                               out_fds, G_N_ELEMENTS (out_fds));
  g_assert_cmpint (nfds, ==, 2);
  if (out_fds[0].fd == fd)
    g_assert_cmpint (out_fds[0].events, ==, G_IO_IN);
  else if (out_fds[1].fd == fd)
    g_assert_cmpint (out_fds[1].events, ==, G_IO_IN);
  else
    g_assert_not_reached ();

  /* Add a second pollfd with the same fd but different event, and
   * lower priority.
   */
  source2 = g_io_create_watch (io, G_IO_OUT);
  g_source_set_priority (source2, G_PRIORITY_LOW);
  g_source_set_callback (source2, (GSourceFunc) nfds_out_low_cb,
                         NULL, NULL);
  g_source_attach (source2, ctx);

  /* g_main_context_query() should still return only 2 pollfds,
   * one of which has our fd, and a combined events field.
   */
  nfds = g_main_context_query (ctx, G_MAXINT, NULL,
                               out_fds, G_N_ELEMENTS (out_fds));
  g_assert_cmpint (nfds, ==, 2);
  if (out_fds[0].fd == fd)
    g_assert_cmpint (out_fds[0].events, ==, G_IO_IN | G_IO_OUT);
  else if (out_fds[1].fd == fd)
    g_assert_cmpint (out_fds[1].events, ==, G_IO_IN | G_IO_OUT);
  else
    g_assert_not_reached ();

  /* But if we query with a max priority, we won't see the
   * lower-priority one.
   */
  nfds = g_main_context_query (ctx, G_PRIORITY_DEFAULT, NULL,
                               out_fds, G_N_ELEMENTS (out_fds));
  g_assert_cmpint (nfds, ==, 2);
  if (out_fds[0].fd == fd)
    g_assert_cmpint (out_fds[0].events, ==, G_IO_IN);
  else if (out_fds[1].fd == fd)
    g_assert_cmpint (out_fds[1].events, ==, G_IO_IN);
  else
    g_assert_not_reached ();

  /* Third pollfd */
  source3 = g_io_create_watch (io, G_IO_OUT);
  g_source_set_priority (source3, G_PRIORITY_DEFAULT);
  g_source_set_callback (source3, (GSourceFunc) nfds_out_cb,
                         &source3_ran, NULL);
  g_source_attach (source3, ctx);

  nfds = g_main_context_query (ctx, G_MAXINT, NULL,
                               out_fds, G_N_ELEMENTS (out_fds));
  g_assert_cmpint (nfds, ==, 2);
  if (out_fds[0].fd == fd)
    g_assert_cmpint (out_fds[0].events, ==, G_IO_IN | G_IO_OUT);
  else if (out_fds[1].fd == fd)
    g_assert_cmpint (out_fds[1].events, ==, G_IO_IN | G_IO_OUT);
  else
    g_assert_not_reached ();

  /* Now actually iterate the loop; the fd should be readable and
   * writable, so source1 and source3 should be triggered, but *not*
   * source2, since it's lower priority than them. (Though on
   * G_OS_WIN32, source3 doesn't get triggered, probably because of
   * giowin32 weirdness...)
   */
  g_main_context_iteration (ctx, FALSE);

  g_assert (source1_ran);
#ifndef G_OS_WIN32
  g_assert (source3_ran);
#endif

  g_source_destroy (source1);
  g_source_unref (source1);
  g_source_destroy (source2);
  g_source_unref (source2);
  g_source_destroy (source3);
  g_source_unref (source3);

  g_io_channel_unref (io);
  remove (tmpfile);
  g_free (tmpfile);

  g_main_context_unref (ctx);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  g_test_bug_base ("http://bugzilla.gnome.org/");

  g_test_add_func ("/maincontext/basic", test_maincontext_basic);
  g_test_add_func ("/mainloop/basic", test_mainloop_basic);
  g_test_add_func ("/mainloop/timeouts", test_timeouts);
  g_test_add_func ("/mainloop/priorities", test_priorities);
  g_test_add_func ("/mainloop/invoke", test_invoke);
  g_test_add_func ("/mainloop/child_sources", test_child_sources);
  g_test_add_func ("/mainloop/recursive_child_sources", test_recursive_child_sources);
  g_test_add_func ("/mainloop/swapping_child_sources", test_swapping_child_sources);
  g_test_add_func ("/mainloop/blocked_child_sources", test_blocked_child_sources);
  g_test_add_func ("/mainloop/source_time", test_source_time);
  g_test_add_func ("/mainloop/overflow", test_mainloop_overflow);
  g_test_add_func ("/mainloop/ready-time", test_ready_time);
  g_test_add_func ("/mainloop/wakeup", test_wakeup);
  g_test_add_func ("/mainloop/remove-invalid", test_remove_invalid);
  g_test_add_func ("/mainloop/unref-while-pending", test_unref_while_pending);
#ifdef G_OS_UNIX
  g_test_add_func ("/mainloop/unix-fd", test_unix_fd);
  g_test_add_func ("/mainloop/unix-fd-source", test_unix_fd_source);
  g_test_add_func ("/mainloop/source-unix-fd-api", test_source_unix_fd_api);
  g_test_add_func ("/mainloop/wait", test_mainloop_wait);
  g_test_add_func ("/mainloop/unix-file-poll", test_unix_file_poll);
#endif
  g_test_add_func ("/mainloop/nfds", test_nfds);

  return g_test_run ();
}
