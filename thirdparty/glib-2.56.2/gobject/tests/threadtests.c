/* GLib testing framework examples and tests
 * Copyright (C) 2008 Imendio AB
 * Authors: Tim Janik
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
#define GLIB_DISABLE_DEPRECATION_WARNINGS
#include <glib.h>
#include <glib-object.h>

static volatile int mtsafe_call_counter = 0; /* multi thread safe call counter */
static int          unsafe_call_counter = 0; /* single-threaded call counter */
static GCond sync_cond;
static GMutex sync_mutex;

#define NUM_COUNTER_INCREMENTS 100000

static void
call_counter_init (gpointer tclass)
{
  int i;
  for (i = 0; i < NUM_COUNTER_INCREMENTS; i++)
    {
      int saved_unsafe_call_counter = unsafe_call_counter;
      g_atomic_int_add (&mtsafe_call_counter, 1); /* real call count update */
      g_thread_yield(); /* let concurrent threads corrupt the unsafe_call_counter state */
      unsafe_call_counter = 1 + saved_unsafe_call_counter; /* non-atomic counter update */
    }
}

static void interface_per_class_init (void) { call_counter_init (NULL); }

/* define 3 test interfaces */
typedef GTypeInterface MyFace0Interface;
static GType my_face0_get_type (void);
G_DEFINE_INTERFACE (MyFace0, my_face0, G_TYPE_OBJECT)
static void my_face0_default_init (MyFace0Interface *iface) { call_counter_init (iface); }
typedef GTypeInterface MyFace1Interface;
static GType my_face1_get_type (void);
G_DEFINE_INTERFACE (MyFace1, my_face1, G_TYPE_OBJECT)
static void my_face1_default_init (MyFace1Interface *iface) { call_counter_init (iface); }

/* define 3 test objects, adding interfaces 0 & 1, and adding interface 2 after class initialization */
typedef GObject         MyTester0;
typedef GObjectClass    MyTester0Class;
static GType my_tester0_get_type (void);
G_DEFINE_TYPE_WITH_CODE (MyTester0, my_tester0, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (my_face0_get_type(), interface_per_class_init)
                         G_IMPLEMENT_INTERFACE (my_face1_get_type(), interface_per_class_init))
static void my_tester0_init (MyTester0*t) {}
static void my_tester0_class_init (MyTester0Class*c) { call_counter_init (c); }
typedef GObject         MyTester1;
typedef GObjectClass    MyTester1Class;

/* Disabled for now (see https://bugzilla.gnome.org/show_bug.cgi?id=687659) */
#if 0
typedef GTypeInterface MyFace2Interface;
static GType my_face2_get_type (void);
G_DEFINE_INTERFACE (MyFace2, my_face2, G_TYPE_OBJECT)
static void my_face2_default_init (MyFace2Interface *iface) { call_counter_init (iface); }

static GType my_tester1_get_type (void);
G_DEFINE_TYPE_WITH_CODE (MyTester1, my_tester1, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (my_face0_get_type(), interface_per_class_init)
                         G_IMPLEMENT_INTERFACE (my_face1_get_type(), interface_per_class_init))
static void my_tester1_init (MyTester1*t) {}
static void my_tester1_class_init (MyTester1Class*c) { call_counter_init (c); }
typedef GObject         MyTester2;
typedef GObjectClass    MyTester2Class;
static GType my_tester2_get_type (void);
G_DEFINE_TYPE_WITH_CODE (MyTester2, my_tester2, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (my_face0_get_type(), interface_per_class_init)
                         G_IMPLEMENT_INTERFACE (my_face1_get_type(), interface_per_class_init))
static void my_tester2_init (MyTester2*t) {}
static void my_tester2_class_init (MyTester2Class*c) { call_counter_init (c); }

static gpointer
tester_init_thread (gpointer data)
{
  const GInterfaceInfo face2_interface_info = { (GInterfaceInitFunc) interface_per_class_init, NULL, NULL };
  gpointer klass;
  /* first, syncronize with other threads,
   * then run interface and class initializers,
   * using unsafe_call_counter concurrently
   */
  g_mutex_lock (&sync_mutex);
  g_mutex_unlock (&sync_mutex);
  /* test default interface initialization for face0 */
  g_type_default_interface_unref (g_type_default_interface_ref (my_face0_get_type()));
  /* test class initialization, face0 per-class initializer, face1 default and per-class initializer */
  klass = g_type_class_ref ((GType) data);
  /* test face2 default and per-class initializer, after class_init */
  g_type_add_interface_static (G_TYPE_FROM_CLASS (klass), my_face2_get_type(), &face2_interface_info);
  /* cleanups */
  g_type_class_unref (klass);
  return NULL;
}

static void
test_threaded_class_init (void)
{
  GThread *t1, *t2, *t3;

  /* pause newly created threads */
  g_mutex_lock (&sync_mutex);

  /* create threads */
  t1 = g_thread_create (tester_init_thread, (gpointer) my_tester0_get_type(), TRUE, NULL);
  t2 = g_thread_create (tester_init_thread, (gpointer) my_tester1_get_type(), TRUE, NULL);
  t3 = g_thread_create (tester_init_thread, (gpointer) my_tester2_get_type(), TRUE, NULL);

  /* execute threads */
  g_mutex_unlock (&sync_mutex);
  while (g_atomic_int_get (&mtsafe_call_counter) < (3 + 3 + 3 * 3) * NUM_COUNTER_INCREMENTS)
    {
      if (g_test_verbose())
        g_printerr ("Initializers counted: %u\n", g_atomic_int_get (&mtsafe_call_counter));
      g_usleep (50 * 1000); /* wait for threads to complete */
    }
  if (g_test_verbose())
    g_printerr ("Total initializers: %u\n", g_atomic_int_get (&mtsafe_call_counter));
  /* ensure non-corrupted counter updates */
  g_assert_cmpint (g_atomic_int_get (&mtsafe_call_counter), ==, unsafe_call_counter);

  g_thread_join (t1);
  g_thread_join (t2);
  g_thread_join (t3);
}
#endif

typedef struct {
  GObject parent;
  char   *name;
} PropTester;
typedef GObjectClass    PropTesterClass;
static GType prop_tester_get_type (void);
G_DEFINE_TYPE (PropTester, prop_tester, G_TYPE_OBJECT)
#define PROP_NAME 1
static void
prop_tester_init (PropTester* t)
{
  if (t->name == NULL)
    ; /* neds unit test framework initialization: g_test_bug ("race initializing properties"); */
}
static void
prop_tester_set_property (GObject        *object,
                          guint           property_id,
                          const GValue   *value,
                          GParamSpec     *pspec)
{}
static void
prop_tester_class_init (PropTesterClass *c)
{
  int i;
  GParamSpec *param;
  GObjectClass *gobject_class = G_OBJECT_CLASS (c);

  gobject_class->set_property = prop_tester_set_property; /* silence GObject checks */

  g_mutex_lock (&sync_mutex);
  g_cond_signal (&sync_cond);
  g_mutex_unlock (&sync_mutex);

  for (i = 0; i < 100; i++) /* wait a bit. */
    g_thread_yield();

  call_counter_init (c);
  param = g_param_spec_string ("name", "name_i18n",
			       "yet-more-wasteful-i18n",
			       NULL,
			       G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE |
			       G_PARAM_STATIC_NAME | G_PARAM_STATIC_BLURB |
			       G_PARAM_STATIC_NICK);
  g_object_class_install_property (gobject_class, PROP_NAME, param);
}

static gpointer
object_create (gpointer data)
{
  GObject *obj = g_object_new (prop_tester_get_type(), "name", "fish", NULL);
  g_object_unref (obj);
  return NULL;
}

static void
test_threaded_object_init (void)
{
  GThread *creator;
  g_mutex_lock (&sync_mutex);

  creator = g_thread_create (object_create, NULL, TRUE, NULL);
  /* really provoke the race */
  g_cond_wait (&sync_cond, &sync_mutex);

  object_create (NULL);
  g_mutex_unlock (&sync_mutex);

  g_thread_join (creator);
}

typedef struct {
    MyTester0 *strong;
    guint unref_delay;
} UnrefInThreadData;

static gpointer
unref_in_thread (gpointer p)
{
  UnrefInThreadData *data = p;

  g_usleep (data->unref_delay);
  g_object_unref (data->strong);

  return NULL;
}

/* undefine to see this test fail without GWeakRef */
#define HAVE_G_WEAK_REF

#define SLEEP_MIN_USEC 1
#define SLEEP_MAX_USEC 10

static void
test_threaded_weak_ref (void)
{
  guint i;
  guint get_wins = 0, unref_wins = 0;
  guint n;

  if (g_test_thorough ())
    n = NUM_COUNTER_INCREMENTS;
  else
    n = NUM_COUNTER_INCREMENTS / 20;

  for (i = 0; i < n; i++)
    {
      UnrefInThreadData data;
#ifdef HAVE_G_WEAK_REF
      /* GWeakRef<MyTester0> in C++ terms */
      GWeakRef weak;
#else
      gpointer weak;
#endif
      MyTester0 *strengthened;
      guint get_delay;
      GThread *thread;
      GError *error = NULL;

      if (g_test_verbose () && (i % (n/20)) == 0)
        g_printerr ("%u%%\n", ((i * 100) / n));

      /* Have an object and a weak ref to it */
      data.strong = g_object_new (my_tester0_get_type (), NULL);

#ifdef HAVE_G_WEAK_REF
      g_weak_ref_init (&weak, data.strong);
#else
      weak = data.strong;
      g_object_add_weak_pointer ((GObject *) weak, &weak);
#endif

      /* Delay for a random time on each side of the race, to perturb the
       * timing. Ideally, we want each side to win half the races; on
       * smcv's laptop, these timings are about right.
       */
      data.unref_delay = g_random_int_range (SLEEP_MIN_USEC / 2, SLEEP_MAX_USEC / 2);
      get_delay = g_random_int_range (SLEEP_MIN_USEC, SLEEP_MAX_USEC);

      /* One half of the race is to unref the shared object */
      thread = g_thread_create (unref_in_thread, &data, TRUE, &error);
      g_assert_no_error (error);

      /* The other half of the race is to get the object from the "global
       * singleton"
       */
      g_usleep (get_delay);

#ifdef HAVE_G_WEAK_REF
      strengthened = g_weak_ref_get (&weak);
#else
      /* Spot the unsafe pointer access! In GDBusConnection this is rather
       * better-hidden, but ends up with essentially the same thing, albeit
       * cleared in dispose() rather than by a traditional weak pointer
       */
      strengthened = weak;

      if (strengthened != NULL)
        g_object_ref (strengthened);
#endif

      if (strengthened != NULL)
        g_assert (G_IS_OBJECT (strengthened));

      /* Wait for the thread to run */
      g_thread_join (thread);

      if (strengthened != NULL)
        {
          get_wins++;
          g_assert (G_IS_OBJECT (strengthened));
          g_object_unref (strengthened);
        }
      else
        {
          unref_wins++;
        }

#ifdef HAVE_G_WEAK_REF
      g_weak_ref_clear (&weak);
#else
      if (weak != NULL)
        g_object_remove_weak_pointer (weak, &weak);
#endif
    }

  if (g_test_verbose ())
    g_printerr ("Race won by get %u times, unref %u times\n",
             get_wins, unref_wins);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  /* g_test_add_func ("/GObject/threaded-class-init", test_threaded_class_init); */
  g_test_add_func ("/GObject/threaded-object-init", test_threaded_object_init);
  g_test_add_func ("/GObject/threaded-weak-ref", test_threaded_weak_ref);

  return g_test_run();
}
