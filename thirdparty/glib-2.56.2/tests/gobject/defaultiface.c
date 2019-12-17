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
#define	G_LOG_DOMAIN "TestDefaultIface"

#undef G_DISABLE_ASSERT
#undef G_DISABLE_CHECKS
#undef G_DISABLE_CAST_CHECKS

#include <glib-object.h>

#include "testcommon.h"
#include "testmodule.h"

/* This test tests getting the default vtable for an interface
 * and the initialization and finalization of such default
 * interfaces.
 *
 * We test this both for static and for dynamic interfaces.
 */

/**********************************************************************
 * Static interface tests
 **********************************************************************/

typedef struct _TestStaticIfaceClass TestStaticIfaceClass;

struct _TestStaticIfaceClass
{
  GTypeInterface base_iface;
  guint val;
};

GType test_static_iface_get_type (void);
#define TEST_TYPE_STATIC_IFACE (test_static_iface_get_type ())

static void
test_static_iface_default_init (TestStaticIfaceClass *iface)
{
  iface->val = 42;
}

DEFINE_IFACE (TestStaticIface, test_static_iface,
	      NULL, test_static_iface_default_init)

static void
test_static_iface (void)
{
  TestStaticIfaceClass *static_iface;

  /* Not loaded until we call ref for the first time */
  static_iface = g_type_default_interface_peek (TEST_TYPE_STATIC_IFACE);
  g_assert (static_iface == NULL);

  /* Ref loads */
  static_iface = g_type_default_interface_ref (TEST_TYPE_STATIC_IFACE);
  g_assert (static_iface && static_iface->val == 42);

  /* Peek then works */
  static_iface = g_type_default_interface_peek (TEST_TYPE_STATIC_IFACE);
  g_assert (static_iface && static_iface->val == 42);
  
  /* Unref does nothing */
  g_type_default_interface_unref (static_iface);
  
  /* And peek still works */
  static_iface = g_type_default_interface_peek (TEST_TYPE_STATIC_IFACE);
  g_assert (static_iface && static_iface->val == 42);
}

/**********************************************************************
 * Dynamic interface tests
 **********************************************************************/

typedef struct _TestDynamicIfaceClass TestDynamicIfaceClass;

struct _TestDynamicIfaceClass
{
  GTypeInterface base_iface;
  guint val;
};

static GType test_dynamic_iface_type;
static gboolean dynamic_iface_init = FALSE;

#define TEST_TYPE_DYNAMIC_IFACE (test_dynamic_iface_type)

static void
test_dynamic_iface_default_init (TestStaticIfaceClass *iface)
{
  dynamic_iface_init = TRUE;
  iface->val = 42;
}

static void
test_dynamic_iface_default_finalize (TestStaticIfaceClass *iface)
{
  dynamic_iface_init = FALSE;
}

static void
test_dynamic_iface_register (GTypeModule *module)
{
  const GTypeInfo iface_info =			
    {								
      sizeof (TestDynamicIfaceClass),
      (GBaseInitFunc)	   NULL,
      (GBaseFinalizeFunc)  NULL,				
      (GClassInitFunc)     test_dynamic_iface_default_init,
      (GClassFinalizeFunc) test_dynamic_iface_default_finalize
    };							

  test_dynamic_iface_type = g_type_module_register_type (module, G_TYPE_INTERFACE,
							 "TestDynamicIface", &iface_info, 0);
}

static void
module_register (GTypeModule *module)
{
  test_dynamic_iface_register (module);
}

static void
test_dynamic_iface (void)
{
  TestDynamicIfaceClass *dynamic_iface;

  test_module_new (module_register);

  /* Not loaded until we call ref for the first time */
  dynamic_iface = g_type_default_interface_peek (TEST_TYPE_DYNAMIC_IFACE);
  g_assert (dynamic_iface == NULL);

  /* Ref loads */
  dynamic_iface = g_type_default_interface_ref (TEST_TYPE_DYNAMIC_IFACE);
  g_assert (dynamic_iface_init);
  g_assert (dynamic_iface && dynamic_iface->val == 42);

  /* Peek then works */
  dynamic_iface = g_type_default_interface_peek (TEST_TYPE_DYNAMIC_IFACE);
  g_assert (dynamic_iface && dynamic_iface->val == 42);
  
  /* Unref causes finalize */
  g_type_default_interface_unref (dynamic_iface);
#if 0
  g_assert (!dynamic_iface_init);
#endif

  /* Peek returns NULL */
  dynamic_iface = g_type_default_interface_peek (TEST_TYPE_DYNAMIC_IFACE);
#if 0
  g_assert (dynamic_iface == NULL);
#endif
  
  /* Ref reloads */
  dynamic_iface = g_type_default_interface_ref (TEST_TYPE_DYNAMIC_IFACE);
  g_assert (dynamic_iface_init);
  g_assert (dynamic_iface && dynamic_iface->val == 42);

  /* And Unref causes finalize once more*/
  g_type_default_interface_unref (dynamic_iface);
#if 0
  g_assert (!dynamic_iface_init);
#endif
}

int
main (int   argc,
      char *argv[])
{
  g_log_set_always_fatal (g_log_set_always_fatal (G_LOG_FATAL_MASK) |
			  G_LOG_LEVEL_WARNING |
			  G_LOG_LEVEL_CRITICAL);

  test_static_iface ();
  test_dynamic_iface ();
  
  return 0;
}
