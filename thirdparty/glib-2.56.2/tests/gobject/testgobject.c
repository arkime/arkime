/* GObject - GLib Type, Object, Parameter and Signal Library
 * Copyright (C) 2001 Red Hat, Inc.
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

#include <string.h>

#undef	G_LOG_DOMAIN
#define	G_LOG_DOMAIN "TestObject"
#include	<glib-object.h>

/* --- TestIface --- */
#define TEST_TYPE_IFACE           (test_iface_get_type ())
#define TEST_IFACE(obj)		  (G_TYPE_CHECK_INSTANCE_CAST ((obj), TEST_TYPE_IFACE, TestIface))
#define TEST_IS_IFACE(obj)	  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TEST_TYPE_IFACE))
#define TEST_IFACE_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_INTERFACE ((obj), TEST_TYPE_IFACE, TestIfaceClass))
typedef struct _TestIface      TestIface;
typedef struct _TestIfaceClass TestIfaceClass;
struct _TestIfaceClass
{
  GTypeInterface base_iface;
  void	(*print_string)	(TestIface	*tiobj,
			 const gchar	*string);
};
static void	iface_base_init		(TestIfaceClass	*iface);
static void	iface_base_finalize	(TestIfaceClass	*iface);
static void	print_foo		(TestIface	*tiobj,
					 const gchar	*string);
static GType
test_iface_get_type (void)
{
  static GType test_iface_type = 0;

  if (!test_iface_type)
    {
      const GTypeInfo test_iface_info =
      {
	sizeof (TestIfaceClass),
	(GBaseInitFunc)	iface_base_init,		/* base_init */
	(GBaseFinalizeFunc) iface_base_finalize,	/* base_finalize */
      };

      test_iface_type = g_type_register_static (G_TYPE_INTERFACE, "TestIface", &test_iface_info, 0);
      g_type_interface_add_prerequisite (test_iface_type, G_TYPE_OBJECT);
    }

  return test_iface_type;
}
static guint iface_base_init_count = 0;
static void
iface_base_init (TestIfaceClass *iface)
{
  iface_base_init_count++;
  if (iface_base_init_count == 1)
    {
      /* add signals here */
    }
}
static void
iface_base_finalize (TestIfaceClass *iface)
{
  iface_base_init_count--;
  if (iface_base_init_count == 0)
    {
      /* destroy signals here */
    }
}
static void
print_foo (TestIface   *tiobj,
	   const gchar *string)
{
  if (!string)
    string = "<NULL>";
  g_print ("Iface-FOO: \"%s\" from %p\n", string, tiobj);
}
static void
test_object_test_iface_init (gpointer giface,
			     gpointer iface_data)
{
  TestIfaceClass *iface = giface;

  g_assert (iface_data == GUINT_TO_POINTER (42));

  g_assert (G_TYPE_FROM_INTERFACE (iface) == TEST_TYPE_IFACE);

  /* assert iface_base_init() was already called */
  g_assert (iface_base_init_count > 0);

  /* initialize stuff */
  iface->print_string = print_foo;
}
static void
iface_print_string (TestIface   *tiobj,
		    const gchar *string)
{
  TestIfaceClass *iface;

  g_return_if_fail (TEST_IS_IFACE (tiobj));
  g_return_if_fail (G_IS_OBJECT (tiobj)); /* ensured through prerequisite */

  iface = TEST_IFACE_GET_CLASS (tiobj);
  g_object_ref (tiobj);
  iface->print_string (tiobj, string);
  g_object_unref (tiobj);
}


/* --- TestObject --- */
#define TEST_TYPE_OBJECT            (test_object_get_type ())
#define TEST_OBJECT(object)         (G_TYPE_CHECK_INSTANCE_CAST ((object), TEST_TYPE_OBJECT, TestObject))
#define TEST_OBJECT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), TEST_TYPE_OBJECT, TestObjectClass))
#define TEST_IS_OBJECT(object)      (G_TYPE_CHECK_INSTANCE_TYPE ((object), TEST_TYPE_OBJECT))
#define TEST_IS_OBJECT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TEST_TYPE_OBJECT))
#define TEST_OBJECT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), TEST_TYPE_OBJECT, TestObjectClass))
#define TEST_OBJECT_GET_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), TEST_TYPE_OBJECT, TestObjectPrivate))
typedef struct _TestObject        TestObject;
typedef struct _TestObjectClass   TestObjectClass;
typedef struct _TestObjectPrivate TestObjectPrivate;
struct _TestObject
{
  GObject parent_instance;
};
struct _TestObjectClass
{
  GObjectClass parent_class;

  gchar* (*test_signal) (TestObject *tobject,
			 TestIface  *iface_object,
			 gpointer    tdata);
};
struct _TestObjectPrivate
{
  int     dummy1;
  gdouble dummy2;
};
static void	test_object_class_init	(TestObjectClass	*class);
static void	test_object_init	(TestObject		*tobject);
static gboolean	test_signal_accumulator	(GSignalInvocationHint	*ihint,
					 GValue            	*return_accu,
					 const GValue       	*handler_return,
					 gpointer                data);
static gchar*	test_object_test_signal	(TestObject		*tobject,
					 TestIface		*iface_object,
					 gpointer		 tdata);
static GType
test_object_get_type (void)
{
  static GType test_object_type = 0;

  if (!test_object_type)
    {
      const GTypeInfo test_object_info =
      {
	sizeof (TestObjectClass),
	NULL,           /* base_init */
	NULL,           /* base_finalize */
	(GClassInitFunc) test_object_class_init,
	NULL,           /* class_finalize */
	NULL,           /* class_data */
	sizeof (TestObject),
	5,              /* n_preallocs */
	(GInstanceInitFunc) test_object_init,
      };
      GInterfaceInfo iface_info = { test_object_test_iface_init, NULL, GUINT_TO_POINTER (42) };

      test_object_type = g_type_register_static (G_TYPE_OBJECT, "TestObject", &test_object_info, 0);
      g_type_add_interface_static (test_object_type, TEST_TYPE_IFACE, &iface_info);
    }

  return test_object_type;
}
static void
test_object_class_init (TestObjectClass *class)
{
  /*  GObjectClass *gobject_class = G_OBJECT_CLASS (class); */

  class->test_signal = test_object_test_signal;

  g_signal_new ("test-signal",
		G_OBJECT_CLASS_TYPE (class),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST | G_SIGNAL_RUN_CLEANUP,
		G_STRUCT_OFFSET (TestObjectClass, test_signal),
		test_signal_accumulator, NULL,
		g_cclosure_marshal_STRING__OBJECT_POINTER,
		G_TYPE_STRING, 2, TEST_TYPE_IFACE, G_TYPE_POINTER);

  g_type_class_add_private (class, sizeof (TestObjectPrivate));
}
static void
test_object_init (TestObject *tobject)
{
  TestObjectPrivate *priv;

  priv = TEST_OBJECT_GET_PRIVATE (tobject);

  g_assert (priv);

  priv->dummy1 = 54321;
}
/* Check to see if private data initialization in the
 * instance init function works.
 */
static void
test_object_check_private_init (TestObject *tobject)
{
  TestObjectPrivate *priv;

  priv = TEST_OBJECT_GET_PRIVATE (tobject);

  g_print ("private data during initialization: %u == %u\n", priv->dummy1, 54321);
  g_assert (priv->dummy1 == 54321);
}
static gboolean
test_signal_accumulator (GSignalInvocationHint *ihint,
			 GValue                *return_accu,
			 const GValue          *handler_return,
			 gpointer               data)
{
  const gchar *accu_string = g_value_get_string (return_accu);
  const gchar *new_string = g_value_get_string (handler_return);
  gchar *result_string;

  if (accu_string)
    result_string = g_strconcat (accu_string, new_string, NULL);
  else if (new_string)
    result_string = g_strdup (new_string);
  else
    result_string = NULL;

  g_value_take_string (return_accu, result_string);

  return TRUE;
}
static gchar*
test_object_test_signal (TestObject *tobject,
			 TestIface  *iface_object,
			 gpointer    tdata)
{
  g_message ("::test_signal default_handler called");

  g_return_val_if_fail (TEST_IS_IFACE (iface_object), NULL);
  
  return g_strdup ("<default_handler>");
}


/* --- TestIface for DerivedObject --- */
static void
print_bar (TestIface   *tiobj,
	   const gchar *string)
{
  TestIfaceClass *parent_iface;

  g_return_if_fail (TEST_IS_IFACE (tiobj));

  if (!string)
    string = "<NULL>";
  g_print ("Iface-BAR: \"%s\" from %p\n", string, tiobj);

  g_print ("chaining: ");
  parent_iface = g_type_interface_peek_parent (TEST_IFACE_GET_CLASS (tiobj));
  parent_iface->print_string (tiobj, string);

  g_assert (g_type_interface_peek_parent (parent_iface) == NULL);
}

static void
derived_object_test_iface_init (gpointer giface,
				gpointer iface_data)
{
  TestIfaceClass *iface = giface;

  g_assert (iface_data == GUINT_TO_POINTER (87));

  g_assert (G_TYPE_FROM_INTERFACE (iface) == TEST_TYPE_IFACE);

  /* assert test_object_test_iface_init() was already called */
  g_assert (iface->print_string == print_foo);

  /* override stuff */
  iface->print_string = print_bar;
}


/* --- DerivedObject --- */
#define DERIVED_TYPE_OBJECT            (derived_object_get_type ())
#define DERIVED_OBJECT(object)         (G_TYPE_CHECK_INSTANCE_CAST ((object), DERIVED_TYPE_OBJECT, DerivedObject))
#define DERIVED_OBJECT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), DERIVED_TYPE_OBJECT, DerivedObjectClass))
#define DERIVED_IS_OBJECT(object)      (G_TYPE_CHECK_INSTANCE_TYPE ((object), DERIVED_TYPE_OBJECT))
#define DERIVED_IS_OBJECT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), DERIVED_TYPE_OBJECT))
#define DERIVED_OBJECT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), DERIVED_TYPE_OBJECT, DerivedObjectClass))
#define DERIVED_OBJECT_GET_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), DERIVED_TYPE_OBJECT, DerivedObjectPrivate))
typedef struct _DerivedObject        DerivedObject;
typedef struct _TestObjectClass      DerivedObjectClass;
typedef struct _DerivedObjectPrivate DerivedObjectPrivate;
struct _DerivedObject
{
  TestObject parent_instance;
  int  dummy1;
  int  dummy2;
};
struct _DerivedObjectPrivate
{
  char dummy;
};
static void derived_object_class_init (DerivedObjectClass *class);
static void derived_object_init       (DerivedObject      *dobject);
static GType
derived_object_get_type (void)
{
  static GType derived_object_type = 0;

  if (!derived_object_type)
    {
      const GTypeInfo derived_object_info =
      {
	sizeof (DerivedObjectClass),
	NULL,           /* base_init */
	NULL,           /* base_finalize */
	(GClassInitFunc) derived_object_class_init,
	NULL,           /* class_finalize */
	NULL,           /* class_data */
	sizeof (DerivedObject),
	5,              /* n_preallocs */
	(GInstanceInitFunc) derived_object_init,
      };
      GInterfaceInfo iface_info = { derived_object_test_iface_init, NULL, GUINT_TO_POINTER (87) };

      derived_object_type = g_type_register_static (TEST_TYPE_OBJECT, "DerivedObject", &derived_object_info, 0);
      g_type_add_interface_static (derived_object_type, TEST_TYPE_IFACE, &iface_info);
    }

  return derived_object_type;
}
static void
derived_object_class_init (DerivedObjectClass *class)
{
  g_type_class_add_private (class, sizeof (DerivedObjectPrivate));
}
static void
derived_object_init (DerivedObject *dobject)
{
  TestObjectPrivate *test_priv;
  DerivedObjectPrivate *derived_priv;

  derived_priv = DERIVED_OBJECT_GET_PRIVATE (dobject);

  g_assert (derived_priv);

  test_priv = TEST_OBJECT_GET_PRIVATE (dobject);
  
  g_assert (test_priv);

}

/* --- main --- */
int
main (int   argc,
      char *argv[])
{
  GTypeInfo info = { 0, };
  GTypeFundamentalInfo finfo = { 0, };
  GType type;
  TestObject *sigarg;
  DerivedObject *dobject;
  TestObjectPrivate *priv;
  gchar *string = NULL;

  g_log_set_always_fatal (g_log_set_always_fatal (G_LOG_FATAL_MASK) |
			  G_LOG_LEVEL_WARNING |
			  G_LOG_LEVEL_CRITICAL);

  /* test new fundamentals */
  g_assert (G_TYPE_MAKE_FUNDAMENTAL (G_TYPE_RESERVED_USER_FIRST) == g_type_fundamental_next ());
  type = g_type_register_fundamental (g_type_fundamental_next (), "FooShadow1", &info, &finfo, 0);
  g_assert (type == G_TYPE_MAKE_FUNDAMENTAL (G_TYPE_RESERVED_USER_FIRST));
  g_assert (G_TYPE_MAKE_FUNDAMENTAL (G_TYPE_RESERVED_USER_FIRST + 1) == g_type_fundamental_next ());
  type = g_type_register_fundamental (g_type_fundamental_next (), "FooShadow2", &info, &finfo, 0);
  g_assert (type == G_TYPE_MAKE_FUNDAMENTAL (G_TYPE_RESERVED_USER_FIRST + 1));
  g_assert (G_TYPE_MAKE_FUNDAMENTAL (G_TYPE_RESERVED_USER_FIRST + 2) == g_type_fundamental_next ());
  g_assert (g_type_from_name ("FooShadow1") == G_TYPE_MAKE_FUNDAMENTAL (G_TYPE_RESERVED_USER_FIRST));
  g_assert (g_type_from_name ("FooShadow2") == G_TYPE_MAKE_FUNDAMENTAL (G_TYPE_RESERVED_USER_FIRST + 1));

  /* to test past class initialization interface setups, create the class here */
  g_type_class_ref (TEST_TYPE_OBJECT);

  dobject = g_object_new (DERIVED_TYPE_OBJECT, NULL);
  test_object_check_private_init (TEST_OBJECT (dobject));

  sigarg = g_object_new (TEST_TYPE_OBJECT, NULL);

  g_print ("MAIN: emit test-signal:\n");
  g_signal_emit_by_name (dobject, "test-signal", sigarg, NULL, &string);
  g_message ("signal return: \"%s\"", string);
  g_assert (strcmp (string, "<default_handler><default_handler>") == 0);
  g_free (string);

  g_print ("MAIN: call iface print-string on test and derived object:\n");
  iface_print_string (TEST_IFACE (sigarg), "iface-string-from-test-type");
  iface_print_string (TEST_IFACE (dobject), "iface-string-from-derived-type");

  priv = TEST_OBJECT_GET_PRIVATE (dobject);
  g_print ("private data after initialization: %u == %u\n", priv->dummy1, 54321);
  g_assert (priv->dummy1 == 54321);
  
  g_object_unref (sigarg);
  g_object_unref (dobject);

  g_message ("%s done", argv[0]);

  return 0;
}
