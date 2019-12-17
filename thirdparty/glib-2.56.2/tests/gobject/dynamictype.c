/* GObject - GLib Type, Object, Parameter and Signal Library
 * Copyright (C) 2001, 2003 Red Hat, Inc.
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

#undef	G_LOG_DOMAIN
#define	G_LOG_DOMAIN "TestDynamicType"

#undef G_DISABLE_ASSERT
#undef G_DISABLE_CHECKS
#undef G_DISABLE_CAST_CHECKS

#include <glib-object.h>

#include "testcommon.h"
#include "testmodule.h"

/* This test tests the macros for defining dynamic types.
 */

static gboolean loaded = FALSE;

struct _TestIfaceClass
{
  GTypeInterface base_iface;
  guint val;
};

static GType test_iface_get_type (void);
#define TEST_TYPE_IFACE           (test_iface_get_type ())
#define TEST_IFACE_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_INTERFACE ((obj), TEST_TYPE_IFACE, TestIfaceClass))
typedef struct _TestIface      TestIface;
typedef struct _TestIfaceClass TestIfaceClass;

static void test_iface_base_init    (TestIfaceClass *iface);
static void test_iface_default_init (TestIfaceClass *iface, gpointer class_data);

static DEFINE_IFACE(TestIface, test_iface, test_iface_base_init, test_iface_default_init)

static void
test_iface_default_init (TestIfaceClass *iface,
			 gpointer        class_data)
{
}

static void
test_iface_base_init (TestIfaceClass *iface)
{
}

GType dynamic_object_get_type (void);
#define DYNAMIC_OBJECT_TYPE (dynamic_object_get_type ())

typedef GObject DynamicObject;
typedef struct _DynamicObjectClass DynamicObjectClass;

struct _DynamicObjectClass
{
  GObjectClass parent_class;
  guint val;
};

static void dynamic_object_iface_init (TestIface *iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED(DynamicObject, dynamic_object, G_TYPE_OBJECT, 0,
			       G_IMPLEMENT_INTERFACE_DYNAMIC (TEST_TYPE_IFACE,
							      dynamic_object_iface_init));

static void 
dynamic_object_class_init (DynamicObjectClass *class)
{
  class->val = 42;
  loaded = TRUE;
}

static void
dynamic_object_class_finalize (DynamicObjectClass *class)
{
  loaded = FALSE;
}

static void
dynamic_object_iface_init (TestIface *iface)
{
}

static void
dynamic_object_init (DynamicObject *dynamic_object)
{
}

static void
module_register (GTypeModule *module)
{
  dynamic_object_register_type (module);
}

static void
test_dynamic_type (void)
{
  DynamicObjectClass *class;

  test_module_new (module_register);

  /* Not loaded until we call ref for the first time */
  class = g_type_class_peek (DYNAMIC_OBJECT_TYPE);
  g_assert (class == NULL);
  g_assert (!loaded);

  /* Make sure interfaces work */
  g_assert (g_type_is_a (DYNAMIC_OBJECT_TYPE,
			 TEST_TYPE_IFACE));

  /* Ref loads */
  class = g_type_class_ref (DYNAMIC_OBJECT_TYPE);
  g_assert (class && class->val == 42);
  g_assert (loaded);

  /* Peek then works */
  class = g_type_class_peek (DYNAMIC_OBJECT_TYPE);
  g_assert (class && class->val == 42);
  g_assert (loaded);

  /* Make sure interfaces still work */
  g_assert (g_type_is_a (DYNAMIC_OBJECT_TYPE,
			 TEST_TYPE_IFACE));

  /* Unref causes finalize */
  g_type_class_unref (class);

  /* Peek returns NULL */
  class = g_type_class_peek (DYNAMIC_OBJECT_TYPE);
#if 0
  g_assert (!class);
  g_assert (!loaded);
#endif
  
  /* Ref reloads */
  class = g_type_class_ref (DYNAMIC_OBJECT_TYPE);
  g_assert (class && class->val == 42);
  g_assert (loaded);

  /* And Unref causes finalize once more*/
  g_type_class_unref (class);
  class = g_type_class_peek (DYNAMIC_OBJECT_TYPE);
#if 0
  g_assert (!class);
  g_assert (!loaded);
#endif
}

int
main (int   argc,
      char *argv[])
{
  g_log_set_always_fatal (g_log_set_always_fatal (G_LOG_FATAL_MASK) |
			  G_LOG_LEVEL_WARNING |
			  G_LOG_LEVEL_CRITICAL);

  test_dynamic_type ();
  
  return 0;
}
