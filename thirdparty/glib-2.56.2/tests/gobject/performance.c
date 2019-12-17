/* GObject - GLib Type, Object, Parameter and Signal Library
 * Copyright (C) 2009 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include <math.h>
#include <string.h>
#include <glib-object.h>
#include "testcommon.h"

#define WARM_UP_N_RUNS 50
#define ESTIMATE_ROUND_TIME_N_RUNS 5
#define DEFAULT_TEST_TIME 15 /* seconds */
 /* The time we want each round to take, in seconds, this should
  * be large enough compared to the timer resolution, but small
  * enought that the risk of any random slowness will miss the
  * running window */
#define TARGET_ROUND_TIME 0.008

static gboolean verbose = FALSE;
static int test_length = DEFAULT_TEST_TIME;

static GOptionEntry cmd_entries[] = {
  {"verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose,
   "Print extra information", NULL},
  {"seconds", 's', 0, G_OPTION_ARG_INT, &test_length,
   "Time to run each test in seconds", NULL},
  {NULL}
};

typedef struct _PerformanceTest PerformanceTest;
struct _PerformanceTest {
  const char *name;
  gpointer extra_data;

  gpointer (*setup) (PerformanceTest *test);
  void (*init) (PerformanceTest *test,
		gpointer data,
		double factor);
  void (*run) (PerformanceTest *test,
	       gpointer data);
  void (*finish) (PerformanceTest *test,
		  gpointer data);
  void (*teardown) (PerformanceTest *test,
		    gpointer data);
  void (*print_result) (PerformanceTest *test,
			gpointer data,
			double time);
};

static void
run_test (PerformanceTest *test)
{
  gpointer data = NULL;
  guint64 i, num_rounds;
  double elapsed, min_elapsed, max_elapsed, avg_elapsed, factor;
  GTimer *timer;

  g_print ("Running test %s\n", test->name);

  /* Set up test */
  timer = g_timer_new ();
  data = test->setup (test);

  if (verbose)
    g_print ("Warming up\n");

  g_timer_start (timer);

  /* Warm up the test by doing a few runs */
  for (i = 0; i < WARM_UP_N_RUNS; i++)
    {
      test->init (test, data, 1.0);
      test->run (test, data);
      test->finish (test, data);
    }

  g_timer_stop (timer);
  elapsed = g_timer_elapsed (timer, NULL);

  if (verbose)
    {
      g_print ("Warm up time: %.2f secs\n", elapsed);
      g_print ("Estimating round time\n");
    }

  /* Estimate time for one run by doing a few test rounds */
  min_elapsed = 0;
  for (i = 0; i < ESTIMATE_ROUND_TIME_N_RUNS; i++)
    {
      test->init (test, data, 1.0);
      g_timer_start (timer);
      test->run (test, data);
      g_timer_stop (timer);
      test->finish (test, data);

      elapsed = g_timer_elapsed (timer, NULL);
      if (i == 0)
	min_elapsed = elapsed;
      else
	min_elapsed = MIN (min_elapsed, elapsed);
    }

  factor = TARGET_ROUND_TIME / min_elapsed;

  if (verbose)
    g_print ("Uncorrected round time: %.4f msecs, correction factor %.2f\n", 1000*min_elapsed, factor);

  /* Calculate number of rounds needed */
  num_rounds = (test_length / TARGET_ROUND_TIME) + 1;

  if (verbose)
    g_print ("Running %"G_GINT64_MODIFIER"d rounds\n", num_rounds);

  /* Run the test */
  for (i = 0; i < num_rounds; i++)
    {
      test->init (test, data, factor);
      g_timer_start (timer);
      test->run (test, data);
      g_timer_stop (timer);
      test->finish (test, data);
      elapsed = g_timer_elapsed (timer, NULL);

      if (i == 0)
	max_elapsed = min_elapsed = avg_elapsed = elapsed;
      else
        {
          min_elapsed = MIN (min_elapsed, elapsed);
          max_elapsed = MAX (max_elapsed, elapsed);
          avg_elapsed += elapsed;
        }
    }

  avg_elapsed = avg_elapsed / num_rounds;

  if (verbose)
    {
      g_print ("Minimum corrected round time: %.2f msecs\n", min_elapsed * 1000);
      g_print ("Maximum corrected round time: %.2f msecs\n", max_elapsed * 1000);
      g_print ("Average corrected round time: %.2f msecs\n", avg_elapsed * 1000);
    }
  /* Print the results */
  test->print_result (test, data, min_elapsed);

  /* Tear down */
  test->teardown (test, data);
  g_timer_destroy (timer);
}

/*************************************************************
 * Simple object is a very simple small GObject subclass
 * with no properties, no signals, implementing no interfaces
 *************************************************************/

static GType simple_object_get_type (void);
#define SIMPLE_TYPE_OBJECT        (simple_object_get_type ())
typedef struct _SimpleObject      SimpleObject;
typedef struct _SimpleObjectClass   SimpleObjectClass;

struct _SimpleObject
{
  GObject parent_instance;
  int val;
};

struct _SimpleObjectClass
{
  GObjectClass parent_class;
};

G_DEFINE_TYPE (SimpleObject, simple_object, G_TYPE_OBJECT)

static void
simple_object_finalize (GObject *object)
{
  G_OBJECT_CLASS (simple_object_parent_class)->finalize (object);
}

static void
simple_object_class_init (SimpleObjectClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = simple_object_finalize;
}

static void
simple_object_init (SimpleObject *simple_object)
{
  simple_object->val = 42;
}

typedef struct _TestIfaceClass TestIfaceClass;
typedef struct _TestIfaceClass TestIface1Class;
typedef struct _TestIfaceClass TestIface2Class;
typedef struct _TestIfaceClass TestIface3Class;
typedef struct _TestIfaceClass TestIface4Class;
typedef struct _TestIfaceClass TestIface5Class;
typedef struct _TestIface TestIface;

struct _TestIfaceClass
{
  GTypeInterface base_iface;
  void (*method) (TestIface *obj);
};

static GType test_iface1_get_type (void);
static GType test_iface2_get_type (void);
static GType test_iface3_get_type (void);
static GType test_iface4_get_type (void);
static GType test_iface5_get_type (void);

#define TEST_TYPE_IFACE1 (test_iface1_get_type ())
#define TEST_TYPE_IFACE2 (test_iface2_get_type ())
#define TEST_TYPE_IFACE3 (test_iface3_get_type ())
#define TEST_TYPE_IFACE4 (test_iface4_get_type ())
#define TEST_TYPE_IFACE5 (test_iface5_get_type ())

static DEFINE_IFACE (TestIface1, test_iface1,  NULL, NULL)
static DEFINE_IFACE (TestIface2, test_iface2,  NULL, NULL)
static DEFINE_IFACE (TestIface3, test_iface3,  NULL, NULL)
static DEFINE_IFACE (TestIface4, test_iface4,  NULL, NULL)
static DEFINE_IFACE (TestIface5, test_iface5,  NULL, NULL)

/*************************************************************
 * Complex object is a GObject subclass with a properties,
 * construct properties, signals and implementing an interface.
 *************************************************************/

static GType complex_object_get_type (void);
#define COMPLEX_TYPE_OBJECT        (complex_object_get_type ())
typedef struct _ComplexObject      ComplexObject;
typedef struct _ComplexObjectClass ComplexObjectClass;

struct _ComplexObject
{
  GObject parent_instance;
  int val1;
  int val2;
};

struct _ComplexObjectClass
{
  GObjectClass parent_class;

  void (*signal) (ComplexObject *obj);
  void (*signal_empty) (ComplexObject *obj);
};

static void complex_test_iface_init (gpointer         g_iface,
				     gpointer         iface_data);

G_DEFINE_TYPE_EXTENDED (ComplexObject, complex_object,
			G_TYPE_OBJECT, 0,
			G_IMPLEMENT_INTERFACE (TEST_TYPE_IFACE1, complex_test_iface_init)
			G_IMPLEMENT_INTERFACE (TEST_TYPE_IFACE2, complex_test_iface_init)
			G_IMPLEMENT_INTERFACE (TEST_TYPE_IFACE3, complex_test_iface_init)
			G_IMPLEMENT_INTERFACE (TEST_TYPE_IFACE4, complex_test_iface_init)
			G_IMPLEMENT_INTERFACE (TEST_TYPE_IFACE5, complex_test_iface_init))

#define COMPLEX_OBJECT(object) (G_TYPE_CHECK_INSTANCE_CAST ((object), COMPLEX_TYPE_OBJECT, ComplexObject))

enum {
  PROP_0,
  PROP_VAL1,
  PROP_VAL2
};

enum {
  COMPLEX_SIGNAL,
  COMPLEX_SIGNAL_EMPTY,
  COMPLEX_SIGNAL_GENERIC,
  COMPLEX_SIGNAL_GENERIC_EMPTY,
  COMPLEX_SIGNAL_ARGS,
  COMPLEX_LAST_SIGNAL
};

static guint complex_signals[COMPLEX_LAST_SIGNAL] = { 0 };

static void
complex_object_finalize (GObject *object)
{
  G_OBJECT_CLASS (complex_object_parent_class)->finalize (object);
}

static void
complex_object_set_property (GObject         *object,
			     guint            prop_id,
			     const GValue    *value,
			     GParamSpec      *pspec)
{
  ComplexObject *complex = COMPLEX_OBJECT (object);

  switch (prop_id)
    {
    case PROP_VAL1:
      complex->val1 = g_value_get_int (value);
      break;
    case PROP_VAL2:
      complex->val2 = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
complex_object_get_property (GObject         *object,
			     guint            prop_id,
			     GValue          *value,
			     GParamSpec      *pspec)
{
  ComplexObject *complex = COMPLEX_OBJECT (object);

  switch (prop_id)
    {
    case PROP_VAL1:
      g_value_set_int (value, complex->val1);
      break;
    case PROP_VAL2:
      g_value_set_int (value, complex->val2);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
complex_object_real_signal (ComplexObject *obj)
{
}

static void
complex_object_class_init (ComplexObjectClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = complex_object_finalize;
  object_class->set_property = complex_object_set_property;
  object_class->get_property = complex_object_get_property;

  class->signal = complex_object_real_signal;

  complex_signals[COMPLEX_SIGNAL] =
    g_signal_new ("signal",
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (ComplexObjectClass, signal),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  complex_signals[COMPLEX_SIGNAL_EMPTY] =
    g_signal_new ("signal-empty",
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (ComplexObjectClass, signal_empty),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);

  complex_signals[COMPLEX_SIGNAL_GENERIC] =
    g_signal_new ("signal-generic",
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (ComplexObjectClass, signal),
		  NULL, NULL,
		  NULL,
		  G_TYPE_NONE, 0);
  complex_signals[COMPLEX_SIGNAL_GENERIC_EMPTY] =
    g_signal_new ("signal-generic-empty",
		  G_TYPE_FROM_CLASS (object_class),
		  G_SIGNAL_RUN_FIRST,
		  G_STRUCT_OFFSET (ComplexObjectClass, signal_empty),
		  NULL, NULL,
		  NULL,
		  G_TYPE_NONE, 0);

  complex_signals[COMPLEX_SIGNAL_ARGS] =
    g_signal_new ("signal-args",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (ComplexObjectClass, signal),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__UINT_POINTER,
                  G_TYPE_NONE, 2, G_TYPE_UINT, G_TYPE_POINTER);

  g_object_class_install_property (object_class,
				   PROP_VAL1,
				   g_param_spec_int ("val1",
 						     "val1",
 						     "val1",
 						     0,
 						     G_MAXINT,
 						     42,
 						     G_PARAM_CONSTRUCT | G_PARAM_READWRITE));
  g_object_class_install_property (object_class,
				   PROP_VAL2,
				   g_param_spec_int ("val2",
 						     "val2",
 						     "val2",
 						     0,
 						     G_MAXINT,
 						     43,
 						     G_PARAM_READWRITE));


}

static void
complex_object_iface_method (TestIface *obj)
{
  ComplexObject *complex = COMPLEX_OBJECT (obj);
  complex->val1++;
}

static void
complex_test_iface_init (gpointer         g_iface,
			 gpointer         iface_data)
{
  TestIfaceClass *iface = g_iface;
  iface->method = complex_object_iface_method;
}

static void
complex_object_init (ComplexObject *complex_object)
{
  complex_object->val2 = 43;
}

/*************************************************************
 * Test object construction performance
 *************************************************************/

#define NUM_OBJECT_TO_CONSTRUCT 10000

struct ConstructionTest {
  GObject **objects;
  int n_objects;
  GType type;
};

static gpointer
test_construction_setup (PerformanceTest *test)
{
  struct ConstructionTest *data;

  data = g_new0 (struct ConstructionTest, 1);
  data->type = ((GType (*)(void))test->extra_data)();

  return data;
}

static void
test_construction_init (PerformanceTest *test,
			gpointer _data,
			double count_factor)
{
  struct ConstructionTest *data = _data;
  int n;

  n = NUM_OBJECT_TO_CONSTRUCT * count_factor;
  if (data->n_objects != n)
    {
      data->n_objects = n;
      data->objects = g_new (GObject *, n);
    }
}

static void
test_construction_run (PerformanceTest *test,
		       gpointer _data)
{
  struct ConstructionTest *data = _data;
  GObject **objects = data->objects;
  GType type = data->type;
  int i, n_objects;

  n_objects = data->n_objects;
  for (i = 0; i < n_objects; i++)
    objects[i] = g_object_new (type, NULL);
}

static void
test_construction_finish (PerformanceTest *test,
			  gpointer _data)
{
  struct ConstructionTest *data = _data;
  int i;

  for (i = 0; i < data->n_objects; i++)
    g_object_unref (data->objects[i]);
}

static void
test_construction_teardown (PerformanceTest *test,
			    gpointer _data)
{
  struct ConstructionTest *data = _data;
  g_free (data->objects);
  g_free (data);
}

static void
test_construction_print_result (PerformanceTest *test,
				gpointer _data,
				double time)
{
  struct ConstructionTest *data = _data;

  g_print ("Millions of constructed objects per second: %.3f\n",
	   data->n_objects / (time * 1000000));
}

/*************************************************************
 * Test runtime type check performance
 *************************************************************/

#define NUM_KILO_CHECKS_PER_ROUND 50

struct TypeCheckTest {
  GObject *object;
  int n_checks;
};

static gpointer
test_type_check_setup (PerformanceTest *test)
{
  struct TypeCheckTest *data;

  data = g_new0 (struct TypeCheckTest, 1);
  data->object = g_object_new (COMPLEX_TYPE_OBJECT, NULL);

  return data;
}

static void
test_type_check_init (PerformanceTest *test,
		      gpointer _data,
		      double factor)
{
  struct TypeCheckTest *data = _data;

  data->n_checks = factor * NUM_KILO_CHECKS_PER_ROUND;
}


/* Work around g_type_check_instance_is_a being marked "pure",
   and thus only called once for the loop. */
gboolean (*my_type_check_instance_is_a) (GTypeInstance *type_instance,
					 GType          iface_type) = &g_type_check_instance_is_a;

static void
test_type_check_run (PerformanceTest *test,
		     gpointer _data)
{
  struct TypeCheckTest *data = _data;
  volatile GObject *object = data->object;
  volatile GType type, types[5];
  int i, j;

  types[0] = test_iface1_get_type ();
  types[1] = test_iface2_get_type ();
  types[2] = test_iface3_get_type ();
  types[3] = test_iface4_get_type ();
  types[4] = test_iface5_get_type ();

  for (i = 0; i < data->n_checks; i++)
    {
      type = types[i%5];
      for (j = 0; j < 1000; j++)
	{
	  my_type_check_instance_is_a ((GTypeInstance *)object,
				       type);
	}
    }
}

static void
test_type_check_finish (PerformanceTest *test,
			gpointer data)
{
}

static void
test_type_check_print_result (PerformanceTest *test,
			      gpointer _data,
			      double time)
{
  struct TypeCheckTest *data = _data;
  g_print ("Million type checks per second: %.2f\n",
	   data->n_checks / (1000*time));
}

static void
test_type_check_teardown (PerformanceTest *test,
			  gpointer _data)
{
  struct TypeCheckTest *data = _data;

  g_object_unref (data->object);
  g_free (data);
}

/*************************************************************
 * Test signal emissions performance (common code)
 *************************************************************/

#define NUM_EMISSIONS_PER_ROUND 10000

struct EmissionTest {
  GObject *object;
  int n_checks;
  int signal_id;
};

static void
test_emission_run (PerformanceTest *test,
                             gpointer _data)
{
  struct EmissionTest *data = _data;
  GObject *object = data->object;
  int i;

  for (i = 0; i < data->n_checks; i++)
    g_signal_emit (object, data->signal_id, 0);
}

static void
test_emission_run_args (PerformanceTest *test,
                        gpointer _data)
{
  struct EmissionTest *data = _data;
  GObject *object = data->object;
  int i;

  for (i = 0; i < data->n_checks; i++)
    g_signal_emit (object, data->signal_id, 0, 0, NULL);
}

/*************************************************************
 * Test signal unhandled emissions performance
 *************************************************************/

static gpointer
test_emission_unhandled_setup (PerformanceTest *test)
{
  struct EmissionTest *data;

  data = g_new0 (struct EmissionTest, 1);
  data->object = g_object_new (COMPLEX_TYPE_OBJECT, NULL);
  data->signal_id = complex_signals[GPOINTER_TO_INT (test->extra_data)];
  return data;
}

static void
test_emission_unhandled_init (PerformanceTest *test,
                              gpointer _data,
                              double factor)
{
  struct EmissionTest *data = _data;

  data->n_checks = factor * NUM_EMISSIONS_PER_ROUND;
}

static void
test_emission_unhandled_finish (PerformanceTest *test,
                                gpointer data)
{
}

static void
test_emission_unhandled_print_result (PerformanceTest *test,
                                      gpointer _data,
                                      double time)
{
  struct EmissionTest *data = _data;

  g_print ("Emissions per second: %.0f\n",
	   data->n_checks / time);
}

static void
test_emission_unhandled_teardown (PerformanceTest *test,
                                  gpointer _data)
{
  struct EmissionTest *data = _data;

  g_object_unref (data->object);
  g_free (data);
}

/*************************************************************
 * Test signal handled emissions performance
 *************************************************************/

static void
test_emission_handled_handler (ComplexObject *obj, gpointer data)
{
}

static gpointer
test_emission_handled_setup (PerformanceTest *test)
{
  struct EmissionTest *data;

  data = g_new0 (struct EmissionTest, 1);
  data->object = g_object_new (COMPLEX_TYPE_OBJECT, NULL);
  data->signal_id = complex_signals[GPOINTER_TO_INT (test->extra_data)];
  g_signal_connect (data->object, "signal",
                    G_CALLBACK (test_emission_handled_handler),
                    NULL);
  g_signal_connect (data->object, "signal-empty",
                    G_CALLBACK (test_emission_handled_handler),
                    NULL);
  g_signal_connect (data->object, "signal-generic",
                    G_CALLBACK (test_emission_handled_handler),
                    NULL);
  g_signal_connect (data->object, "signal-generic-empty",
                    G_CALLBACK (test_emission_handled_handler),
                    NULL);
  g_signal_connect (data->object, "signal-args",
                    G_CALLBACK (test_emission_handled_handler),
                    NULL);

  return data;
}

static void
test_emission_handled_init (PerformanceTest *test,
                            gpointer _data,
                            double factor)
{
  struct EmissionTest *data = _data;

  data->n_checks = factor * NUM_EMISSIONS_PER_ROUND;
}

static void
test_emission_handled_finish (PerformanceTest *test,
                              gpointer data)
{
}

static void
test_emission_handled_print_result (PerformanceTest *test,
                                    gpointer _data,
                                    double time)
{
  struct EmissionTest *data = _data;

  g_print ("Emissions per second: %.0f\n",
	   data->n_checks / time);
}

static void
test_emission_handled_teardown (PerformanceTest *test,
                                gpointer _data)
{
  struct EmissionTest *data = _data;

  g_object_unref (data->object);
  g_free (data);
}

/*************************************************************
 * Test object refcount performance
 *************************************************************/

#define NUM_KILO_REFS_PER_ROUND 100000

struct RefcountTest {
  GObject *object;
  int n_checks;
};

static gpointer
test_refcount_setup (PerformanceTest *test)
{
  struct RefcountTest *data;

  data = g_new0 (struct RefcountTest, 1);
  data->object = g_object_new (COMPLEX_TYPE_OBJECT, NULL);

  return data;
}

static void
test_refcount_init (PerformanceTest *test,
                    gpointer _data,
                    double factor)
{
  struct RefcountTest *data = _data;

  data->n_checks = factor * NUM_KILO_REFS_PER_ROUND;
}

static void
test_refcount_run (PerformanceTest *test,
                   gpointer _data)
{
  struct RefcountTest *data = _data;
  GObject *object = data->object;
  int i;

  for (i = 0; i < data->n_checks; i++)
    {
      g_object_ref (object);
      g_object_ref (object);
      g_object_ref (object);
      g_object_unref (object);
      g_object_unref (object);

      g_object_ref (object);
      g_object_ref (object);
      g_object_unref (object);
      g_object_unref (object);
      g_object_unref (object);
    }
}

static void
test_refcount_finish (PerformanceTest *test,
                      gpointer _data)
{
}

static void
test_refcount_print_result (PerformanceTest *test,
			      gpointer _data,
			      double time)
{
  struct RefcountTest *data = _data;
  g_print ("Million refs+unref per second: %.2f\n",
	   data->n_checks * 5 / (time * 1000000 ));
}

static void
test_refcount_teardown (PerformanceTest *test,
			  gpointer _data)
{
  struct RefcountTest *data = _data;

  g_object_unref (data->object);
  g_free (data);
}

/*************************************************************
 * Main test code
 *************************************************************/

static PerformanceTest tests[] = {
  {
    "simple-construction",
    simple_object_get_type,
    test_construction_setup,
    test_construction_init,
    test_construction_run,
    test_construction_finish,
    test_construction_teardown,
    test_construction_print_result
  },
  {
    "complex-construction",
    complex_object_get_type,
    test_construction_setup,
    test_construction_init,
    test_construction_run,
    test_construction_finish,
    test_construction_teardown,
    test_construction_print_result
  },
  {
    "type-check",
    NULL,
    test_type_check_setup,
    test_type_check_init,
    test_type_check_run,
    test_type_check_finish,
    test_type_check_teardown,
    test_type_check_print_result
  },
  {
    "emit-unhandled",
    GINT_TO_POINTER (COMPLEX_SIGNAL),
    test_emission_unhandled_setup,
    test_emission_unhandled_init,
    test_emission_run,
    test_emission_unhandled_finish,
    test_emission_unhandled_teardown,
    test_emission_unhandled_print_result
  },
  {
    "emit-unhandled-empty",
    GINT_TO_POINTER (COMPLEX_SIGNAL_EMPTY),
    test_emission_unhandled_setup,
    test_emission_unhandled_init,
    test_emission_run,
    test_emission_unhandled_finish,
    test_emission_unhandled_teardown,
    test_emission_unhandled_print_result
  },
  {
    "emit-unhandled-generic",
    GINT_TO_POINTER (COMPLEX_SIGNAL_GENERIC),
    test_emission_unhandled_setup,
    test_emission_unhandled_init,
    test_emission_run,
    test_emission_unhandled_finish,
    test_emission_unhandled_teardown,
    test_emission_unhandled_print_result
  },
  {
    "emit-unhandled-generic-empty",
    GINT_TO_POINTER (COMPLEX_SIGNAL_GENERIC_EMPTY),
    test_emission_unhandled_setup,
    test_emission_unhandled_init,
    test_emission_run,
    test_emission_unhandled_finish,
    test_emission_unhandled_teardown,
    test_emission_unhandled_print_result
  },
  {
    "emit-unhandled-args",
    GINT_TO_POINTER (COMPLEX_SIGNAL_ARGS),
    test_emission_unhandled_setup,
    test_emission_unhandled_init,
    test_emission_run_args,
    test_emission_unhandled_finish,
    test_emission_unhandled_teardown,
    test_emission_unhandled_print_result
  },
  {
    "emit-handled",
    GINT_TO_POINTER (COMPLEX_SIGNAL),
    test_emission_handled_setup,
    test_emission_handled_init,
    test_emission_run,
    test_emission_handled_finish,
    test_emission_handled_teardown,
    test_emission_handled_print_result
  },
  {
    "emit-handled-empty",
    GINT_TO_POINTER (COMPLEX_SIGNAL_EMPTY),
    test_emission_handled_setup,
    test_emission_handled_init,
    test_emission_run,
    test_emission_handled_finish,
    test_emission_handled_teardown,
    test_emission_handled_print_result
  },
  {
    "emit-handled-generic",
    GINT_TO_POINTER (COMPLEX_SIGNAL_GENERIC),
    test_emission_handled_setup,
    test_emission_handled_init,
    test_emission_run,
    test_emission_handled_finish,
    test_emission_handled_teardown,
    test_emission_handled_print_result
  },
  {
    "emit-handled-generic-empty",
    GINT_TO_POINTER (COMPLEX_SIGNAL_GENERIC_EMPTY),
    test_emission_handled_setup,
    test_emission_handled_init,
    test_emission_run,
    test_emission_handled_finish,
    test_emission_handled_teardown,
    test_emission_handled_print_result
  },
  {
    "emit-handled-args",
    GINT_TO_POINTER (COMPLEX_SIGNAL_ARGS),
    test_emission_handled_setup,
    test_emission_handled_init,
    test_emission_run_args,
    test_emission_handled_finish,
    test_emission_handled_teardown,
    test_emission_handled_print_result
  },
  {
    "refcount",
    NULL,
    test_refcount_setup,
    test_refcount_init,
    test_refcount_run,
    test_refcount_finish,
    test_refcount_teardown,
    test_refcount_print_result
  }
};

static PerformanceTest *
find_test (const char *name)
{
  int i;
  for (i = 0; i < G_N_ELEMENTS (tests); i++)
    {
      if (strcmp (tests[i].name, name) == 0)
	return &tests[i];
    }
  return NULL;
}
int
main (int   argc,
      char *argv[])
{
  PerformanceTest *test;
  GOptionContext *context;
  GError *error = NULL;
  int i;

  context = g_option_context_new ("GObject performance tests");
  g_option_context_add_main_entries (context, cmd_entries, NULL);
  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_printerr ("%s: %s\n", argv[0], error->message);
      return 1;
    }

  if (argc > 1)
    {
      for (i = 1; i < argc; i++)
	{
	  test = find_test (argv[i]);
	  if (test)
	    run_test (test);
	}
    }
  else
    {
      for (i = 0; i < G_N_ELEMENTS (tests); i++)
	run_test (&tests[i]);
    }

  return 0;
}
