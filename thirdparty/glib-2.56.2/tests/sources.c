/* This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Copyright 2012 Red Hat, Inc
 */

#undef G_DISABLE_ASSERT
#undef G_LOG_DOMAIN

#include <glib.h>

#define NSOURCES 50000

static gboolean
callback (gpointer user_data)
{
  g_assert_not_reached ();
  return FALSE;
}

static void
shuffle (GSource **sources, int num)
{
  int i, a, b;
  GSource *tmp;

  for (i = 0; i < num * 10; i++)
    {
      a = g_random_int_range (0, num);
      b = g_random_int_range (0, num);
      tmp = sources[a];
      sources[a] = sources[b];
      sources[b] = tmp;
    }
}

static void
thread_pool_attach_func (gpointer data,
                         gpointer user_data)
{
  GMainContext *context = user_data;
  GSource *source = data;

  g_source_attach (source, context);
  g_source_unref (source);
}

static void
thread_pool_destroy_func (gpointer data,
                          gpointer user_data)
{
  GSource *source = data;

  g_source_destroy (source);
}

int
main (int argc, char **argv)
{
  int i;
  gint64 start;
  gint64 end;
  GMainContext *context;
  GSource **sources;
  GThreadPool *pool;
  GError *error = NULL;

  context = g_main_context_default ();
  sources = g_new0 (GSource *, NSOURCES);

  start = g_get_monotonic_time ();
  for (i = 0; i < NSOURCES; i++)
    {
      sources[i] = g_idle_source_new ();
      g_source_set_callback (sources[i], callback, NULL, NULL);
      g_source_attach (sources[i], context);
    }
  end = g_get_monotonic_time ();
  g_print ("Add same-priority sources: %" G_GINT64_FORMAT "\n",
           (end - start) / 1000);

#ifdef SLOW
  start = g_get_monotonic_time ();
  for (i = 0; i < NSOURCES; i++)
    g_assert (sources[i] == g_main_context_find_source_by_id (context, g_source_get_id (sources[i])));
  end = g_get_monotonic_time ();
  g_print ("Find each source: %" G_GINT64_FORMAT "\n",
           (end - start) / 1000);
#endif

  shuffle (sources, NSOURCES);

  start = g_get_monotonic_time ();
  for (i = 0; i < NSOURCES; i++)
    {
      g_source_destroy (sources[i]);
      g_source_unref (sources[i]);
    }
  end = g_get_monotonic_time ();
  g_print ("Remove in random order: %" G_GINT64_FORMAT "\n",
           (end - start) / 1000);

  /* Make sure they really did get removed */
  g_main_context_iteration (context, FALSE);

  start = g_get_monotonic_time ();
  for (i = 0; i < NSOURCES; i++)
    {
      sources[i] = g_idle_source_new ();
      g_source_set_callback (sources[i], callback, NULL, NULL);
      g_source_set_priority (sources[i], i % 100);
      g_source_attach (sources[i], context);
    }
  end = g_get_monotonic_time ();
  g_print ("Add different-priority sources: %" G_GINT64_FORMAT "\n",
           (end - start) / 1000);

#ifdef SLOW
  start = g_get_monotonic_time ();
  for (i = 0; i < NSOURCES; i++)
    g_assert (sources[i] == g_main_context_find_source_by_id (context, g_source_get_id (sources[i])));
  end = g_get_monotonic_time ();
  g_print ("Find each source: %" G_GINT64_FORMAT "\n",
           (end - start) / 1000);
#endif

  shuffle (sources, NSOURCES);

  start = g_get_monotonic_time ();
  for (i = 0; i < NSOURCES; i++)
    {
      g_source_destroy (sources[i]);
      g_source_unref (sources[i]);
    }
  end = g_get_monotonic_time ();
  g_print ("Remove in random order: %" G_GINT64_FORMAT "\n",
           (end - start) / 1000);

  /* Make sure they really did get removed */
  g_main_context_iteration (context, FALSE);

  pool = g_thread_pool_new (thread_pool_attach_func, context,
                            20, TRUE, NULL);
  start = g_get_monotonic_time ();
  for (i = 0; i < NSOURCES; i++)
    {
      sources[i] = g_idle_source_new ();
      g_source_set_callback (sources[i], callback, NULL, NULL);
      g_thread_pool_push (pool, sources[i], &error);
      g_assert_no_error (error);
    }
  g_thread_pool_free (pool, FALSE, TRUE);
  end = g_get_monotonic_time ();
  g_print ("Add sources from threads: %" G_GINT64_FORMAT "\n",
           (end - start) / 1000);

  pool = g_thread_pool_new (thread_pool_destroy_func, context,
                            20, TRUE, NULL);
  start = g_get_monotonic_time ();
  for (i = 0; i < NSOURCES; i++)
    {
      g_thread_pool_push (pool, sources[i], &error);
      g_assert_no_error (error);
    }
  g_thread_pool_free (pool, FALSE, TRUE);
  end = g_get_monotonic_time ();
  g_print ("Remove sources from threads: %" G_GINT64_FORMAT "\n",
           (end - start) / 1000);

  /* Make sure they really did get removed */
  g_main_context_iteration (context, FALSE);

  g_free (sources);
  return 0;
}
