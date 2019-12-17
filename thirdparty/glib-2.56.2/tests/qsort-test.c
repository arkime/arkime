#undef G_DISABLE_ASSERT
#undef G_LOG_DOMAIN

#include <glib.h>

#define SIZE 100000

guint32 array[SIZE];

static gint
sort (gconstpointer a, gconstpointer b, gpointer user_data)
{
  return *(guint32*)a < *(guint32*)b ? -1 : 1;
}

int
main (int argc, char **argv)
{
  int i;

  for (i = 0; i < SIZE; i++)
    array[i] = g_random_int ();

  g_qsort_with_data (array, SIZE, sizeof (guint32), sort, NULL);

  for (i = 0; i < SIZE - 1; i++)
    g_assert (array[i] <= array[i+1]);

  /* 0 elements is a valid case */
  g_qsort_with_data (array, 0, sizeof (guint32), sort, NULL);

  return 0;
}
