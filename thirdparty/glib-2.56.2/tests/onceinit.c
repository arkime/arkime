/* g_once_init_*() test
 * Copyright (C) 2007 Tim Janik
 *
 * This work is provided "as is"; redistribution and modification
 * in whole or in part, in any medium, physical or electronic is
 * permitted without restriction.

 * This work is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

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
#include <stdlib.h>

#define N_THREADS               (13)

static GMutex       tmutex;
static GCond        tcond;
static volatile int thread_call_count = 0;
static char         dummy_value = 'x';

static void
assert_singleton_execution1 (void)
{
  static volatile int seen_execution = 0;
  int old_seen_execution = g_atomic_int_add (&seen_execution, 1);
  if (old_seen_execution != 0)
    g_error ("%s: function executed more than once", G_STRFUNC);
}

static void
assert_singleton_execution2 (void)
{
  static volatile int seen_execution = 0;
  int old_seen_execution = g_atomic_int_add (&seen_execution, 1);
  if (old_seen_execution != 0)
    g_error ("%s: function executed more than once", G_STRFUNC);
}

static void
assert_singleton_execution3 (void)
{
  static volatile int seen_execution = 0;
  int old_seen_execution = g_atomic_int_add (&seen_execution, 1);
  if (old_seen_execution != 0)
    g_error ("%s: function executed more than once", G_STRFUNC);
}

static void
initializer1 (void)
{
  static volatile gsize initialized = 0;
  if (g_once_init_enter (&initialized))
    {
      gsize initval = 42;
      assert_singleton_execution1();
      g_once_init_leave (&initialized, initval);
    }
}

static gpointer
initializer2 (void)
{
  static volatile gsize initialized = 0;
  if (g_once_init_enter (&initialized))
    {
      void *pointer_value = &dummy_value;
      assert_singleton_execution2();
      g_once_init_leave (&initialized, (gsize) pointer_value);
    }
  return (void*) initialized;
}

static void
initializer3 (void)
{
  static volatile gsize initialized = 0;
  if (g_once_init_enter (&initialized))
    {
      gsize initval = 42;
      assert_singleton_execution3();
      g_usleep (25 * 1000);     /* waste time for multiple threads to wait */
      g_once_init_leave (&initialized, initval);
    }
}

static gpointer
tmain_call_initializer3 (gpointer user_data)
{
  g_mutex_lock (&tmutex);
  g_cond_wait (&tcond, &tmutex);
  g_mutex_unlock (&tmutex);
  //g_printf ("[");
  initializer3();
  //g_printf ("]\n");
  g_atomic_int_add (&thread_call_count, 1);
  return NULL;
}

static void*     stress_concurrent_initializers (void*);

int
main (int   argc,
      char *argv[])
{
  G_GNUC_UNUSED GThread *threads[N_THREADS];
  int i;
  void *p;

  /* test simple initializer */
  initializer1();
  initializer1();
  /* test pointer initializer */
  p = initializer2();
  g_assert (p == &dummy_value);
  p = initializer2();
  g_assert (p == &dummy_value);
  /* start multiple threads for initializer3() */
  g_mutex_lock (&tmutex);
  for (i = 0; i < N_THREADS; i++)
    threads[i] = g_thread_create (tmain_call_initializer3, 0, FALSE, NULL);
  g_mutex_unlock (&tmutex);
  /* concurrently call initializer3() */
  g_cond_broadcast (&tcond);
  /* loop until all threads passed the call to initializer3() */
  while (g_atomic_int_get (&thread_call_count) < i)
    {
      if (rand() % 2)
        g_thread_yield();   /* concurrent shuffling for single core */
      else
        g_usleep (1000);    /* concurrent shuffling for multi core */
      g_cond_broadcast (&tcond);
    }
  /* call multiple (unoptimized) initializers from multiple threads */
  g_mutex_lock (&tmutex);
  g_atomic_int_set (&thread_call_count, 0);
  for (i = 0; i < N_THREADS; i++)
    g_thread_create (stress_concurrent_initializers, 0, FALSE, NULL);
  g_mutex_unlock (&tmutex);
  while (g_atomic_int_get (&thread_call_count) < 256 * 4 * N_THREADS)
    g_usleep (50 * 1000);       /* wait for all 5 threads to complete */
  return 0;
}

/* get rid of g_once_init_enter-optimizations in the below definitions
 * to uncover possible races in the g_once_init_enter_impl()/
 * g_once_init_leave() implementations
 */
#undef g_once_init_enter
#undef g_once_init_leave

/* define 16 * 16 simple initializers */
#define DEFINE_TEST_INITIALIZER(N)                      \
      static void                                       \
      test_initializer_##N (void)                       \
      {                                                 \
        static volatile gsize initialized = 0;          \
        if (g_once_init_enter (&initialized))           \
          {                                             \
            g_free (g_strdup_printf ("cpuhog%5d", 1));  \
            g_free (g_strdup_printf ("cpuhog%6d", 2));  \
            g_free (g_strdup_printf ("cpuhog%7d", 3));  \
            g_once_init_leave (&initialized, 1);        \
          }                                             \
      }
#define DEFINE_16_TEST_INITIALIZERS(P)                  \
                DEFINE_TEST_INITIALIZER (P##0)          \
                DEFINE_TEST_INITIALIZER (P##1)          \
                DEFINE_TEST_INITIALIZER (P##2)          \
                DEFINE_TEST_INITIALIZER (P##3)          \
                DEFINE_TEST_INITIALIZER (P##4)          \
                DEFINE_TEST_INITIALIZER (P##5)          \
                DEFINE_TEST_INITIALIZER (P##6)          \
                DEFINE_TEST_INITIALIZER (P##7)          \
                DEFINE_TEST_INITIALIZER (P##8)          \
                DEFINE_TEST_INITIALIZER (P##9)          \
                DEFINE_TEST_INITIALIZER (P##a)          \
                DEFINE_TEST_INITIALIZER (P##b)          \
                DEFINE_TEST_INITIALIZER (P##c)          \
                DEFINE_TEST_INITIALIZER (P##d)          \
                DEFINE_TEST_INITIALIZER (P##e)          \
                DEFINE_TEST_INITIALIZER (P##f)
#define DEFINE_256_TEST_INITIALIZERS(P)                 \
                DEFINE_16_TEST_INITIALIZERS (P##_0)     \
                DEFINE_16_TEST_INITIALIZERS (P##_1)     \
                DEFINE_16_TEST_INITIALIZERS (P##_2)     \
                DEFINE_16_TEST_INITIALIZERS (P##_3)     \
                DEFINE_16_TEST_INITIALIZERS (P##_4)     \
                DEFINE_16_TEST_INITIALIZERS (P##_5)     \
                DEFINE_16_TEST_INITIALIZERS (P##_6)     \
                DEFINE_16_TEST_INITIALIZERS (P##_7)     \
                DEFINE_16_TEST_INITIALIZERS (P##_8)     \
                DEFINE_16_TEST_INITIALIZERS (P##_9)     \
                DEFINE_16_TEST_INITIALIZERS (P##_a)     \
                DEFINE_16_TEST_INITIALIZERS (P##_b)     \
                DEFINE_16_TEST_INITIALIZERS (P##_c)     \
                DEFINE_16_TEST_INITIALIZERS (P##_d)     \
                DEFINE_16_TEST_INITIALIZERS (P##_e)     \
                DEFINE_16_TEST_INITIALIZERS (P##_f)

/* list 16 * 16 simple initializers */
#define LIST_16_TEST_INITIALIZERS(P)                    \
                test_initializer_##P##0,                \
                test_initializer_##P##1,                \
                test_initializer_##P##2,                \
                test_initializer_##P##3,                \
                test_initializer_##P##4,                \
                test_initializer_##P##5,                \
                test_initializer_##P##6,                \
                test_initializer_##P##7,                \
                test_initializer_##P##8,                \
                test_initializer_##P##9,                \
                test_initializer_##P##a,                \
                test_initializer_##P##b,                \
                test_initializer_##P##c,                \
                test_initializer_##P##d,                \
                test_initializer_##P##e,                \
                test_initializer_##P##f
#define LIST_256_TEST_INITIALIZERS(P)                   \
                LIST_16_TEST_INITIALIZERS (P##_0),      \
                LIST_16_TEST_INITIALIZERS (P##_1),      \
                LIST_16_TEST_INITIALIZERS (P##_2),      \
                LIST_16_TEST_INITIALIZERS (P##_3),      \
                LIST_16_TEST_INITIALIZERS (P##_4),      \
                LIST_16_TEST_INITIALIZERS (P##_5),      \
                LIST_16_TEST_INITIALIZERS (P##_6),      \
                LIST_16_TEST_INITIALIZERS (P##_7),      \
                LIST_16_TEST_INITIALIZERS (P##_8),      \
                LIST_16_TEST_INITIALIZERS (P##_9),      \
                LIST_16_TEST_INITIALIZERS (P##_a),      \
                LIST_16_TEST_INITIALIZERS (P##_b),      \
                LIST_16_TEST_INITIALIZERS (P##_c),      \
                LIST_16_TEST_INITIALIZERS (P##_d),      \
                LIST_16_TEST_INITIALIZERS (P##_e),      \
                LIST_16_TEST_INITIALIZERS (P##_f)

/* define 4 * 256 initializers */
DEFINE_256_TEST_INITIALIZERS (stress1);
DEFINE_256_TEST_INITIALIZERS (stress2);
DEFINE_256_TEST_INITIALIZERS (stress3);
DEFINE_256_TEST_INITIALIZERS (stress4);

/* call the above 1024 initializers */
static void*
stress_concurrent_initializers (void *user_data)
{
  static void (*initializers[]) (void) = {
    LIST_256_TEST_INITIALIZERS (stress1),
    LIST_256_TEST_INITIALIZERS (stress2),
    LIST_256_TEST_INITIALIZERS (stress3),
    LIST_256_TEST_INITIALIZERS (stress4),
  };
  int i;
  /* sync to main thread */
  g_mutex_lock (&tmutex);
  g_mutex_unlock (&tmutex);
  /* initialize concurrently */
  for (i = 0; i < G_N_ELEMENTS (initializers); i++)
    {
      initializers[i]();
      g_atomic_int_add (&thread_call_count, 1);
    }
  return NULL;
}
