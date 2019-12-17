#include <glib.h>
#ifdef G_OS_UNIX
#include <unistd.h>
#endif

static GMainLoop *loop;

static gboolean
stop_waiting (gpointer data)
{
  g_main_loop_quit (loop);

  return G_SOURCE_REMOVE;
}

static gboolean
function (gpointer data)
{
  g_assert_not_reached ();

  return G_SOURCE_REMOVE;
}

static void
test_seconds (void)
{
  guint id;

  /* Bug 642052 mentions that g_timeout_add_seconds(21475) schedules a
   * job that runs once per second.
   *
   * Test that that isn't true anymore by scheduling two jobs:
   *   - one, as above
   *   - another that runs in 2100ms
   *
   * If everything is working properly, the 2100ms one should run first
   * (and exit the mainloop).  If we ever see the 21475 second job run
   * then we have trouble (since it ran in less than 2 seconds).
   *
   * We need a timeout of at least 2 seconds because
   * g_timeout_add_second can add as much as an additional second of
   * latency.
   */
  loop = g_main_loop_new (NULL, FALSE);

  g_timeout_add (2100, stop_waiting, NULL);
  id = g_timeout_add_seconds (21475, function, NULL);

  g_main_loop_run (loop);
  g_main_loop_unref (loop);

  g_source_remove (id);
}

static gint64 last_time;
static gint count;

static gboolean
test_func (gpointer data)
{
  gint64 current_time;

  current_time = g_get_monotonic_time ();

  /* We accept 2 on the first iteration because _add_seconds() can
   * have an initial latency of 1 second, see its documentation.
   */
  if (count == 0)
    g_assert (current_time / 1000000 - last_time / 1000000 <= 2);
  else
    g_assert (current_time / 1000000 - last_time / 1000000 == 1);

  last_time = current_time;
  count++;

  /* Make the timeout take up to 0.1 seconds.
   * We should still get scheduled for the next second.
   */
  g_usleep (count * 10000);

  if (count < 10)
    return TRUE;

  g_main_loop_quit (loop);

  return FALSE;
}

static void
test_rounding (void)
{
  loop = g_main_loop_new (NULL, FALSE);

  last_time = g_get_monotonic_time ();
  g_timeout_add_seconds (1, test_func, NULL);

  g_main_loop_run (loop);
  g_main_loop_unref (loop);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/timeout/seconds", test_seconds);
  g_test_add_func ("/timeout/rounding", test_rounding);

  return g_test_run ();
}
