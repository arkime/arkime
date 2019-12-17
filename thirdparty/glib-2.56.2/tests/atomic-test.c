#undef G_DISABLE_ASSERT
#undef G_LOG_DOMAIN

#include <glib.h>

/* Obviously we can't test that the operations are atomic, but we can
 * at least test, that they do, what they ought to do */

int 
main (int   argc,
      char *argv[])
{
  gint i;
  gint atomic = -5;
  gpointer atomic_pointer = NULL;
  gpointer biggest_pointer = (gpointer)((gsize)atomic_pointer - 1);

  for (i = 0; i < 15; i++)
    g_atomic_int_inc (&atomic);
  g_assert (atomic == 10);
  for (i = 0; i < 9; i++)
    g_assert (!g_atomic_int_dec_and_test (&atomic));
  g_assert (g_atomic_int_dec_and_test (&atomic));
  g_assert (atomic == 0);

  g_assert (g_atomic_int_add (&atomic, 5) == 0);
  g_assert (atomic == 5);

  g_assert (g_atomic_int_add (&atomic, -10) == 5);
  g_assert (atomic == -5);

  g_atomic_int_add (&atomic, 20);
  g_assert (atomic == 15);

  g_atomic_int_add (&atomic, -35);
  g_assert (atomic == -20);

  g_assert (atomic == g_atomic_int_get (&atomic));

  g_assert (g_atomic_int_compare_and_exchange (&atomic, -20, 20));
  g_assert (atomic == 20);
  
  g_assert (!g_atomic_int_compare_and_exchange (&atomic, 42, 12));
  g_assert (atomic == 20);
  
  g_assert (g_atomic_int_compare_and_exchange (&atomic, 20, G_MAXINT));
  g_assert (atomic == G_MAXINT);

  g_assert (g_atomic_int_compare_and_exchange (&atomic, G_MAXINT, G_MININT));
  g_assert (atomic == G_MININT);

  g_assert (g_atomic_pointer_compare_and_exchange (&atomic_pointer, 
						   NULL, biggest_pointer));
  g_assert (atomic_pointer == biggest_pointer);

  g_assert (atomic_pointer == g_atomic_pointer_get (&atomic_pointer));

  g_assert (g_atomic_pointer_compare_and_exchange (&atomic_pointer, 
						   biggest_pointer, NULL));
  g_assert (atomic_pointer == NULL);
  
  return 0;
}
