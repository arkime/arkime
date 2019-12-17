#include <string.h>
#include <glib.h>

/* We test deprecated functionality here */
G_GNUC_BEGIN_IGNORE_DEPRECATIONS

#ifdef G_ENABLE_DEBUG
static void
test_slice_nodebug (void)
{
  const gchar *oldval;

  oldval = g_getenv ("G_SLICE");
  g_unsetenv ("G_SLICE");

  if (g_test_subprocess ())
    {
      gpointer p, q;

      p = g_slice_alloc (237);
      q = g_slice_alloc (259);
      g_slice_free1 (237, p);
      g_slice_free1 (259, q);

      g_slice_debug_tree_statistics ();
      return;
    }
  g_test_trap_subprocess (NULL, 1000000, 0);
  g_test_trap_assert_passed ();
  g_test_trap_assert_stderr ("*GSlice: MemChecker: root=NULL*");

  if (oldval)
    g_setenv ("G_SLICE", oldval, TRUE);
}

static void
test_slice_debug (void)
{
  const gchar *oldval;

  oldval = g_getenv ("G_SLICE");
  g_setenv ("G_SLICE", "debug-blocks:always-malloc", TRUE);

  if (g_test_subprocess ())
    {
      gpointer p, q;

      p = g_slice_alloc (237);
      q = g_slice_alloc (259);
      g_slice_free1 (237, p);
      g_slice_free1 (259, q);

      g_slice_debug_tree_statistics ();
      return;
    }
  g_test_trap_subprocess (NULL, 1000000, 0);
  g_test_trap_assert_passed ();
  g_test_trap_assert_stderr ("*GSlice: MemChecker: * trunks, * branches, * old branches*");

  if (oldval)
    g_setenv ("G_SLICE", oldval, TRUE);
  else
    g_unsetenv ("G_SLICE");
}
#endif

static void
test_slice_copy (void)
{
  const gchar *block = "0123456789ABCDEF";
  gpointer p;

  p = g_slice_copy (12, block);
  g_assert (memcmp (p, block, 12) == 0);
  g_slice_free1 (12, p);
}

typedef struct {
  gint int1;
  gint int2;
  gchar byte;
  gpointer next;
  gint64 more;
} TestStruct;

static void
test_chain (void)
{
  TestStruct *ts, *head;

  head = ts = g_slice_new (TestStruct);
  ts->next = g_slice_new (TestStruct);
  ts = ts->next;
  ts->next = g_slice_new (TestStruct);
  ts = ts->next;
  ts->next = NULL;

  g_slice_free_chain (TestStruct, head, next);
}

static gpointer chunks[4096][30];

static gpointer
thread_allocate (gpointer data)
{
  gint i;
  gint b;
  gint size;
  gpointer p;
  volatile gpointer *loc;

  for (i = 0; i < 10000; i++)
    {
      b = g_random_int_range (0, 30);
      size = g_random_int_range (0, 4096);
      loc = &(chunks[size][b]);

      p = g_atomic_pointer_get (loc);
      if (p == NULL)
        {
          p = g_slice_alloc (size + 1);
          if (!g_atomic_pointer_compare_and_exchange (loc, NULL, p))
            g_slice_free1 (size + 1, p);
        }
      else
        {
          if (g_atomic_pointer_compare_and_exchange (loc, p, NULL))
            g_slice_free1 (size + 1, p);
        }
    }

  return NULL;
}

static void
test_allocate (void)
{
  GThread *threads[30];
  gint size;
  gint i;

  for (i = 0; i < 30; i++)
    for (size = 1; size <= 4096; size++)
      chunks[size - 1][i] = NULL;

  for (i = 0; i < G_N_ELEMENTS(threads); i++)
    threads[i] = g_thread_create (thread_allocate, NULL, TRUE, NULL);

  for (i = 0; i < G_N_ELEMENTS(threads); i++)
    g_thread_join (threads[i]);
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);

#ifdef G_ENABLE_DEBUG
  g_test_add_func ("/slice/nodebug", test_slice_nodebug);
  g_test_add_func ("/slice/debug", test_slice_debug);
#endif
  g_test_add_func ("/slice/copy", test_slice_copy);
  g_test_add_func ("/slice/chain", test_chain);
  g_test_add_func ("/slice/allocate", test_allocate);

  return g_test_run ();
}
