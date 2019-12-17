#include <glib.h>
#include <glib-object.h>

#ifdef G_OS_UNIX
#include <unistd.h>
#endif

#define G_TYPE_TEST                (my_test_get_type ())
#define MY_TEST(test)              (G_TYPE_CHECK_INSTANCE_CAST ((test), G_TYPE_TEST, GTest))
#define MY_IS_TEST(test)           (G_TYPE_CHECK_INSTANCE_TYPE ((test), G_TYPE_TEST))
#define MY_TEST_CLASS(tclass)      (G_TYPE_CHECK_CLASS_CAST ((tclass), G_TYPE_TEST, GTestClass))
#define MY_IS_TEST_CLASS(tclass)   (G_TYPE_CHECK_CLASS_TYPE ((tclass), G_TYPE_TEST))
#define MY_TEST_GET_CLASS(test)    (G_TYPE_INSTANCE_GET_CLASS ((test), G_TYPE_TEST, GTestClass))

static GRand *grand;

typedef struct _GTest GTest;
typedef struct _GTestClass GTestClass;

struct _GTest
{
  GObject object;

  gint value;
};

struct _GTestClass
{
  GObjectClass parent_class;

  void (*test_signal1) (GTest * test, gint an_int);
  void (*test_signal2) (GTest * test, gint an_int);
  gchar * (*test_signal3) (GTest * test, gint an_int);
};

static GType my_test_get_type (void);
static volatile gboolean stopping;

/* Element signals and args */
enum
{
  TEST_SIGNAL1,
  TEST_SIGNAL2,
  TEST_SIGNAL3,
  /* add more above */
  LAST_SIGNAL
};

enum
{
  ARG_0,
  ARG_TEST_PROP
};

static void my_test_class_init (GTestClass * klass);
static void my_test_init (GTest * test);
static void my_test_dispose (GObject * object);

static void signal2_handler (GTest * test, gint anint);
static gchar * signal3_handler (GTest * test, gint anint);

static void my_test_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void my_test_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static GObjectClass *parent_class = NULL;

static guint my_test_signals[LAST_SIGNAL] = { 0 };

static GType
my_test_get_type (void)
{
  static GType test_type = 0;

  if (!test_type) {
    const GTypeInfo test_info = {
      sizeof (GTestClass),
      NULL,
      NULL,
      (GClassInitFunc) my_test_class_init,
      NULL,
      NULL,
      sizeof (GTest),
      0,
      (GInstanceInitFunc) my_test_init,
      NULL
    };

    grand = g_rand_new();

    test_type = g_type_register_static (G_TYPE_OBJECT, "GTest",
        &test_info, 0);
  }
  return test_type;
}

static void
my_test_class_init (GTestClass * klass)
{
  GObjectClass *gobject_class;

  gobject_class = (GObjectClass *) klass;

  parent_class = g_type_class_ref (G_TYPE_OBJECT);

  gobject_class->dispose = my_test_dispose;
  gobject_class->set_property = my_test_set_property;
  gobject_class->get_property = my_test_get_property;

  my_test_signals[TEST_SIGNAL1] =
      g_signal_new ("test-signal1", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (GTestClass, test_signal1), NULL,
      NULL, g_cclosure_marshal_VOID__INT, G_TYPE_NONE, 1, G_TYPE_INT);
  my_test_signals[TEST_SIGNAL2] =
      g_signal_new ("test-signal2", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (GTestClass, test_signal2), NULL,
      NULL, g_cclosure_marshal_VOID__INT, G_TYPE_NONE, 1, G_TYPE_INT);
  my_test_signals[TEST_SIGNAL3] =
      g_signal_new ("test-signal3", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (GTestClass, test_signal3), NULL,
      NULL, g_cclosure_marshal_generic, G_TYPE_STRING, 1, G_TYPE_INT);

  g_object_class_install_property (G_OBJECT_CLASS (klass), ARG_TEST_PROP,
      g_param_spec_int ("test-prop", "Test Prop", "Test property",
          0, 1, 0, G_PARAM_READWRITE));

  klass->test_signal2 = signal2_handler;
  klass->test_signal3 = signal3_handler;
}

static void
my_test_init (GTest * test)
{
  g_print ("init %p\n", test);

  test->value = 0;
}

static void
my_test_dispose (GObject * object)
{
  GTest *test;

  test = MY_TEST (object);

  g_print ("dispose %p!\n", test);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
my_test_set_property (GObject * object, guint prop_id,
                      const GValue * value, GParamSpec * pspec)
{
  GTest *test;

  test = MY_TEST (object);

  switch (prop_id) {
    case ARG_TEST_PROP:
      test->value = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
my_test_get_property (GObject * object, guint prop_id,
                      GValue * value, GParamSpec * pspec)
{
  GTest *test;

  test = MY_TEST (object);

  switch (prop_id) {
    case ARG_TEST_PROP:
      g_value_set_int (value, test->value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
my_test_do_signal1 (GTest * test)
{
  g_signal_emit (G_OBJECT (test), my_test_signals[TEST_SIGNAL1], 0, 0);
}

static void
signal2_handler (GTest * test, gint anint)
{
}

static void
my_test_do_signal2 (GTest * test)
{
  g_signal_emit (G_OBJECT (test), my_test_signals[TEST_SIGNAL2], 0, 0);
}

static gchar *
signal3_handler (GTest * test, gint anint)
{
  return g_strdup ("test");
}

static void
my_test_do_signal3 (GTest * test)
{
  gchar *res;

  g_signal_emit (G_OBJECT (test), my_test_signals[TEST_SIGNAL3], 0, 0, &res);
  g_assert (res);
  g_free (res);
}

static void
my_test_do_prop (GTest * test)
{
  test->value = g_rand_int (grand);
  g_object_notify (G_OBJECT (test), "test-prop");
}

static gpointer
run_thread (GTest * test)
{
  gint i = 1;

  while (!stopping) {
    if (TESTNUM == 1)
      my_test_do_signal1 (test);
    if (TESTNUM == 2)
      my_test_do_signal2 (test);
    if (TESTNUM == 3)
      my_test_do_prop (test);
    if (TESTNUM == 4)
      my_test_do_signal3 (test);
    if ((i++ % 10000) == 0) {
      g_print (".");
      g_thread_yield(); /* force context switch */
    }
  }

  return NULL;
}

static void
notify (GObject *object, GParamSpec *spec, gpointer user_data)
{
  gint value;

  g_object_get (object, "test-prop", &value, NULL);
  /*g_print ("+ %d", value);*/
}

int
main (int argc, char **argv)
{
  gint i;
  GTest *test1, *test2;
  GArray *test_threads;
  const gint n_threads = 1;

  g_print ("START: %s\n", argv[0]);
  g_log_set_always_fatal (G_LOG_LEVEL_WARNING | G_LOG_LEVEL_CRITICAL | g_log_set_always_fatal (G_LOG_FATAL_MASK));

  test1 = g_object_new (G_TYPE_TEST, NULL);
  test2 = g_object_new (G_TYPE_TEST, NULL);

  g_signal_connect (test1, "notify::test-prop", G_CALLBACK (notify), NULL);
  g_signal_connect (test1, "test-signal1", G_CALLBACK (notify), NULL);
  g_signal_connect (test1, "test-signal2", G_CALLBACK (notify), NULL);

  test_threads = g_array_new (FALSE, FALSE, sizeof (GThread *));

  stopping = FALSE;

  for (i = 0; i < n_threads; i++) {
    GThread *thread;

    thread = g_thread_create ((GThreadFunc) run_thread, test1, TRUE, NULL);
    g_array_append_val (test_threads, thread);

    thread = g_thread_create ((GThreadFunc) run_thread, test2, TRUE, NULL);
    g_array_append_val (test_threads, thread);
  }
  g_usleep (5000000);

  stopping = TRUE;

  g_print ("\nstopping\n");

  /* join all threads */
  for (i = 0; i < 2 * n_threads; i++) {
    GThread *thread;

    thread = g_array_index (test_threads, GThread *, i);
    g_thread_join (thread);
  }

  g_print ("stopped\n");

  g_array_free (test_threads, TRUE);
  g_object_unref (test1);
  g_object_unref (test2);

  return 0;
}
