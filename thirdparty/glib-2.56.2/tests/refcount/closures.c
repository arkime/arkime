/* Copyright (C) 2005 Imendio AB
 *
 * This software is provided "as is"; redistribution and modification
 * is permitted, provided that the following disclaimer is retained.
 *
 * This software is distributed in the hope that it will be useful,
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
#include <glib-object.h>

#ifdef G_OS_UNIX
#include <unistd.h>
#endif

#define TEST_POINTER1   ((gpointer) 47)
#define TEST_POINTER2   ((gpointer) 49)
#define TEST_INT1       (-77)
#define TEST_INT2       (78)

/* --- GTest class --- */
typedef struct {
  GObject object;
  gint value;
  gpointer test_pointer1;
  gpointer test_pointer2;
} GTest;
typedef struct {
  GObjectClass parent_class;
  void (*test_signal1) (GTest * test, gint an_int);
  void (*test_signal2) (GTest * test, gint an_int);
} GTestClass;

#define G_TYPE_TEST                (my_test_get_type ())
#define MY_TEST(test)              (G_TYPE_CHECK_INSTANCE_CAST ((test), G_TYPE_TEST, GTest))
#define MY_IS_TEST(test)           (G_TYPE_CHECK_INSTANCE_TYPE ((test), G_TYPE_TEST))
#define MY_TEST_CLASS(tclass)      (G_TYPE_CHECK_CLASS_CAST ((tclass), G_TYPE_TEST, GTestClass))
#define MY_IS_TEST_CLASS(tclass)   (G_TYPE_CHECK_CLASS_TYPE ((tclass), G_TYPE_TEST))
#define MY_TEST_GET_CLASS(test)    (G_TYPE_INSTANCE_GET_CLASS ((test), G_TYPE_TEST, GTestClass))

static GType my_test_get_type (void);
G_DEFINE_TYPE (GTest, my_test, G_TYPE_OBJECT)

/* --- variables --- */
static volatile gboolean stopping = FALSE;
static guint             test_signal1 = 0;
static guint             test_signal2 = 0;
static gboolean          seen_signal_handler = FALSE;
static gboolean          seen_cleanup = FALSE;
static gboolean          seen_test_int1 = FALSE;
static gboolean          seen_test_int2 = FALSE;
static gboolean          seen_thread1 = FALSE;
static gboolean          seen_thread2 = FALSE;

/* --- functions --- */
static void
my_test_init (GTest * test)
{
  g_print ("init %p\n", test);

  test->value = 0;
  test->test_pointer1 = TEST_POINTER1;
  test->test_pointer2 = TEST_POINTER2;
}

enum {
  ARG_0,
  ARG_TEST_PROP
};

static void
my_test_set_property (GObject      *object,
                     guint         prop_id,
                     const GValue *value,
                     GParamSpec   *pspec)
{
  GTest *test = MY_TEST (object);
  switch (prop_id)
    {
    case ARG_TEST_PROP:
      test->value = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
my_test_get_property (GObject    *object,
                     guint       prop_id,
                     GValue     *value,
                     GParamSpec *pspec)
{
  GTest *test = MY_TEST (object);
  switch (prop_id)
    {
    case ARG_TEST_PROP:
      g_value_set_int (value, test->value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
my_test_test_signal2 (GTest *test,
                      gint   an_int)
{
}

static void
my_test_emit_test_signal1 (GTest *test,
                           gint   vint)
{
  g_signal_emit (G_OBJECT (test), test_signal1, 0, vint);
}

static void
my_test_emit_test_signal2 (GTest *test,
                           gint   vint)
{
  g_signal_emit (G_OBJECT (test), test_signal2, 0, vint);
}

static void
my_test_class_init (GTestClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = my_test_set_property;
  gobject_class->get_property = my_test_get_property;

  test_signal1 = g_signal_new ("test-signal1", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
                               G_STRUCT_OFFSET (GTestClass, test_signal1), NULL, NULL,
                               g_cclosure_marshal_VOID__INT, G_TYPE_NONE, 1, G_TYPE_INT);
  test_signal2 = g_signal_new ("test-signal2", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
                               G_STRUCT_OFFSET (GTestClass, test_signal2), NULL, NULL,
                               g_cclosure_marshal_VOID__INT, G_TYPE_NONE, 1, G_TYPE_INT);

  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_TEST_PROP,
                                   g_param_spec_int ("test-prop", "Test Prop", "Test property",
                                                     0, 1, 0, G_PARAM_READWRITE));
  klass->test_signal2 = my_test_test_signal2;
}

static inline guint32
quick_rand32 (void)
{
  static guint32 accu = 2147483563;
  accu = 1664525 * accu + 1013904223;
  return accu;
}

static void
test_closure (GClosure *closure)
{
  /* try to produce high contention in closure->ref_count */
  guint i = 0, n = quick_rand32() % 199;
  for (i = 0; i < n; i++)
    g_closure_ref (closure);
  g_closure_sink (closure); /* NOP */
  for (i = 0; i < n; i++)
    g_closure_unref (closure);
}

static gpointer
thread1_main (gpointer data)
{
  GClosure *closure = data;
  while (!stopping)
    {
      static guint count = 0;
      test_closure (closure);
      if (++count % 10000 == 0)
        {
          g_printerr ("c");
          g_thread_yield(); /* force context switch */
          seen_thread1 = TRUE;
        }
    }
  return NULL;
}

static gpointer
thread2_main (gpointer data)
{
  GClosure *closure = data;
  while (!stopping)
    {
      static guint count = 0;
      test_closure (closure);
      if (++count % 10000 == 0)
        {
          g_printerr ("C");
          g_thread_yield(); /* force context switch */
          seen_thread2 = TRUE;
        }
    }
  return NULL;
}

static void
test_signal_handler (GTest   *test,
                     gint     vint,
                     gpointer data)
{
  g_assert (data == TEST_POINTER2);
  g_assert (test->test_pointer1 == TEST_POINTER1);
  seen_signal_handler = TRUE;
  seen_test_int1 |= vint == TEST_INT1;
  seen_test_int2 |= vint == TEST_INT2;
}

static void
destroy_data (gpointer  data,
              GClosure *closure)
{
  seen_cleanup = data == TEST_POINTER2;
  g_assert (closure->ref_count == 0);
}

static void
test_emissions (GTest *test)
{
  my_test_emit_test_signal1 (test, TEST_INT1);
  my_test_emit_test_signal2 (test, TEST_INT2);
}

int
main (int    argc,
      char **argv)
{
  GThread *thread1, *thread2;
  GClosure *closure;
  GTest *object;
  guint i;

  g_print ("START: %s\n", argv[0]);
  g_log_set_always_fatal (G_LOG_LEVEL_WARNING | G_LOG_LEVEL_CRITICAL | g_log_set_always_fatal (G_LOG_FATAL_MASK));

  object = g_object_new (G_TYPE_TEST, NULL);
  closure = g_cclosure_new (G_CALLBACK (test_signal_handler), TEST_POINTER2, destroy_data);

  g_signal_connect_closure (object, "test-signal1", closure, FALSE);
  g_signal_connect_closure (object, "test-signal2", closure, FALSE);

  stopping = FALSE;

  thread1 = g_thread_create (thread1_main, closure, TRUE, NULL);
  thread2 = g_thread_create (thread2_main, closure, TRUE, NULL);

  for (i = 0; i < 1000000; i++)
    {
      static guint count = 0;
      test_emissions (object);
      if (++count % 10000 == 0)
        {
          g_printerr (".\n");
          g_thread_yield(); /* force context switch */
        }
    }

  stopping = TRUE;
  g_print ("\nstopping\n");

  /* wait for thread shutdown */
  g_thread_join (thread1);
  g_thread_join (thread2);

  /* finalize object, destroy signals, run cleanup code */
  g_object_unref (object);

  g_print ("stopped\n");

  g_assert (seen_thread1 != FALSE);
  g_assert (seen_thread2 != FALSE);
  g_assert (seen_test_int1 != FALSE);
  g_assert (seen_test_int2 != FALSE);
  g_assert (seen_signal_handler != FALSE);
  g_assert (seen_cleanup != FALSE);

  return 0;
}
