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

#define DEFAULT_TEST_TIME 2 /* seconds */

static GType
simple_register_class (const char *name, GType parent, ...)
{
  GInterfaceInfo interface_info = { NULL, NULL, NULL };
  va_list args;
  GType type, interface;

  va_start (args, parent);
  type = g_type_register_static_simple (parent, name, sizeof (GObjectClass),
      NULL, parent == G_TYPE_INTERFACE ? 0 : sizeof (GObject), NULL, 0);
  for (;;)
    {
      interface = va_arg (args, GType);
      if (interface == 0)
        break;
      g_type_add_interface_static (type, interface, &interface_info);
    }
  va_end (args);

  return type;
}

/* test emulating liststore behavior for interface lookups */

static GType liststore;
static GType liststore_interfaces[6];

static gpointer 
register_types (void)
{
  static volatile gsize inited = 0;
  if (g_once_init_enter (&inited))
    {
      liststore_interfaces[0] = simple_register_class ("GtkBuildable", G_TYPE_INTERFACE, 0);
      liststore_interfaces[1] = simple_register_class ("GtkTreeDragDest", G_TYPE_INTERFACE, 0);
      liststore_interfaces[2] = simple_register_class ("GtkTreeModel", G_TYPE_INTERFACE, 0);
      liststore_interfaces[3] = simple_register_class ("GtkTreeDragSource", G_TYPE_INTERFACE, 0);
      liststore_interfaces[4] = simple_register_class ("GtkTreeSortable", G_TYPE_INTERFACE, 0);
      liststore_interfaces[5] = simple_register_class ("UnrelatedInterface", G_TYPE_INTERFACE, 0);

      liststore = simple_register_class ("GtkListStore", G_TYPE_OBJECT, 
          liststore_interfaces[0], liststore_interfaces[1], liststore_interfaces[2],
          liststore_interfaces[3], liststore_interfaces[4], (GType) 0);

      g_once_init_leave (&inited, 1);
    }
  return NULL;
}

static void 
liststore_is_a_run (gpointer data)
{
  guint i;

  for (i = 0; i < 1000; i++)
    {
      g_assert (g_type_is_a (liststore, liststore_interfaces[0]));
      g_assert (g_type_is_a (liststore, liststore_interfaces[1]));
      g_assert (g_type_is_a (liststore, liststore_interfaces[2]));
      g_assert (g_type_is_a (liststore, liststore_interfaces[3]));
      g_assert (g_type_is_a (liststore, liststore_interfaces[4]));
      g_assert (!g_type_is_a (liststore, liststore_interfaces[5]));
    }
}

static gpointer
liststore_get_class (void)
{
  register_types ();
  return g_type_class_ref (liststore);
}

static void 
liststore_interface_peek_run (gpointer klass)
{
  guint i;
  gpointer iface;

  for (i = 0; i < 1000; i++)
    {
      iface = g_type_interface_peek (klass, liststore_interfaces[0]);
      g_assert (iface);
      iface = g_type_interface_peek (klass, liststore_interfaces[1]);
      g_assert (iface);
      iface = g_type_interface_peek (klass, liststore_interfaces[2]);
      g_assert (iface);
      iface = g_type_interface_peek (klass, liststore_interfaces[3]);
      g_assert (iface);
      iface = g_type_interface_peek (klass, liststore_interfaces[4]);
      g_assert (iface);
    }
}

static void 
liststore_interface_peek_same_run (gpointer klass)
{
  guint i;
  gpointer iface;

  for (i = 0; i < 1000; i++)
    {
      iface = g_type_interface_peek (klass, liststore_interfaces[0]);
      g_assert (iface);
      iface = g_type_interface_peek (klass, liststore_interfaces[0]);
      g_assert (iface);
      iface = g_type_interface_peek (klass, liststore_interfaces[0]);
      g_assert (iface);
      iface = g_type_interface_peek (klass, liststore_interfaces[0]);
      g_assert (iface);
      iface = g_type_interface_peek (klass, liststore_interfaces[0]);
      g_assert (iface);
    }
}

#if 0
/* DUMB test doing nothing */

static gpointer 
no_setup (void)
{
  return NULL;
}

static void 
no_run (gpointer data)
{
}
#endif

static void 
no_reset (gpointer data)
{
}

static void 
no_teardown (gpointer data)
{
}

typedef struct _PerformanceTest PerformanceTest;
struct _PerformanceTest {
  const char *name;

  gpointer (*setup) (void);
  void (*run) (gpointer data);
  void (*reset) (gpointer data);
  void (*teardown) (gpointer data);
};

static const PerformanceTest tests[] = {
  { "liststore-is-a",
    register_types,
    liststore_is_a_run,
    no_reset,
    no_teardown },
  { "liststore-interface-peek",
    liststore_get_class,
    liststore_interface_peek_run,
    no_reset,
    g_type_class_unref },
  { "liststore-interface-peek-same",
    liststore_get_class,
    liststore_interface_peek_same_run,
    no_reset,
    g_type_class_unref },
#if 0
  { "nothing",
    no_setup,
    no_run,
    no_reset,
    no_teardown }
#endif
};

static gboolean verbose = FALSE;
static int n_threads = 0;
static gboolean list = FALSE;
static int test_length = DEFAULT_TEST_TIME;

static GOptionEntry cmd_entries[] = {
  {"verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose,
   "Print extra information", NULL},
  {"threads", 't', 0, G_OPTION_ARG_INT, &n_threads,
   "number of threads to run in parrallel", NULL},
  {"seconds", 's', 0, G_OPTION_ARG_INT, &test_length,
   "Time to run each test in seconds", NULL},
  {"list", 'l', 0, G_OPTION_ARG_NONE, &list, 
   "List all available tests and exit", NULL},
  {NULL}
};

static gpointer
run_test_thread (gpointer user_data)
{
  const PerformanceTest *test = user_data;
  gpointer data;
  double elapsed;
  GTimer *timer, *total;
  GArray *results;

  total = g_timer_new ();
  g_timer_start (total);

  /* Set up test */
  timer = g_timer_new ();
  data = test->setup ();
  results = g_array_new (FALSE, FALSE, sizeof (double));

  /* Run the test */
  while (g_timer_elapsed (total, NULL) < test_length)
    {
      g_timer_reset (timer);
      g_timer_start (timer);
      test->run (data);
      g_timer_stop (timer);
      elapsed = g_timer_elapsed (timer, NULL);
      g_array_append_val (results, elapsed);
      test->reset (data);
    }

  /* Tear down */
  test->teardown (data);
  g_timer_destroy (timer);
  g_timer_destroy (total);

  return results;
}

static int
compare_doubles (gconstpointer a, gconstpointer b)
{
  double d = *(double *) a - *(double *) b;

  if (d < 0)
    return -1;
  if (d > 0)
    return 1;
  return 0;
}

static void
print_results (GArray *array)
{
  double min, max, avg;
  guint i;

  g_array_sort (array, compare_doubles);

  /* FIXME: discard outliers */

  min = g_array_index (array, double, 0) * 1000;
  max = g_array_index (array, double, array->len - 1) * 1000;
  avg = 0;
  for (i = 0; i < array->len; i++)
    {
      avg += g_array_index (array, double, i);
    }
  avg = avg / array->len * 1000;

  g_print ("  %u runs, min/avg/max = %.3f/%.3f/%.3f ms\n", array->len, min, avg, max);
}

static void
run_test (const PerformanceTest *test)
{
  GArray *results;

  g_print ("Running test \"%s\"\n", test->name);

  if (n_threads == 0) {
    results = run_test_thread ((gpointer) test);
  } else {
    guint i;
    GThread **threads;
    GArray *thread_results;
      
    threads = g_new (GThread *, n_threads);
    for (i = 0; i < n_threads; i++) {
      threads[i] = g_thread_create (run_test_thread, (gpointer) test, TRUE, NULL);
      g_assert (threads[i] != NULL);
    }

    results = g_array_new (FALSE, FALSE, sizeof (double));
    for (i = 0; i < n_threads; i++) {
      thread_results = g_thread_join (threads[i]);
      g_array_append_vals (results, thread_results->data, thread_results->len);
      g_array_free (thread_results, TRUE);
    }
    g_free (threads);
  }

  print_results (results);
  g_array_free (results, TRUE);
}

static const PerformanceTest *
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
  const PerformanceTest *test;
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

  if (list)
    {
      for (i = 0; i < G_N_ELEMENTS (tests); i++)
        {
          g_print ("%s\n", tests[i].name);
        }
      return 0;
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
