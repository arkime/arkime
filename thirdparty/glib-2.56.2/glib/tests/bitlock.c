#include <glib.h>

#define ITERATIONS 100000000

static void
test_bitlocks (void)
{
  guint64 start = g_get_monotonic_time ();
  gint lock = 0;
  gint i;

  for (i = 0; i < ITERATIONS; i++)
    {
      g_bit_lock (&lock, 0);
      g_bit_unlock (&lock, 0);
    }

  {
    gdouble elapsed;
    gdouble rate;

    elapsed = g_get_monotonic_time () - start;
    elapsed /= 1000000;
    rate = ITERATIONS / elapsed;

    g_test_maximized_result (rate, "iterations per second");
  }
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);

  if (g_test_perf ())
    g_test_add_func ("/bitlock/performance/uncontended", test_bitlocks);

  return g_test_run ();
}
