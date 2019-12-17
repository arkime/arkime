/* GObject - GLib Type, Object, Parameter and Signal Library
 * testmodule.c: Dummy dynamic type module
 * Copyright (C) 2003 Red Hat, Inc.
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
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "testmodule.h"
#include "testcommon.h"

static gboolean test_module_load   (GTypeModule *module);
static void     test_module_unload (GTypeModule *module);

static void
test_module_class_init (TestModuleClass *class)
{
  GTypeModuleClass *module_class = G_TYPE_MODULE_CLASS (class);

  module_class->load = test_module_load;
  module_class->unload = test_module_unload;
}

DEFINE_TYPE (TestModule, test_module,
	     test_module_class_init, NULL, NULL,
	     G_TYPE_TYPE_MODULE)

static gboolean
test_module_load (GTypeModule *module)
{
  TestModule *test_module = TEST_MODULE (module);

  test_module->register_func (module);
  
  return TRUE;
}

static void
test_module_unload (GTypeModule *module)
{
}

GTypeModule *
test_module_new (TestModuleRegisterFunc register_func)
{
  TestModule *test_module = g_object_new (TEST_TYPE_MODULE, NULL);
  GTypeModule *module = G_TYPE_MODULE (test_module);
  
  test_module->register_func = register_func;

  /* Register the types initially */
  g_type_module_use (module);
  g_type_module_unuse (module);

  return G_TYPE_MODULE (module);
}

