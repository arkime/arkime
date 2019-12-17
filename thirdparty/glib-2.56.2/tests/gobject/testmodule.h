/* GObject - GLib Type, Object, Parameter and Signal Library
 * testmodule.h: Dummy dynamic type module
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

#ifndef __TEST_MODULE_H__
#define __TEST_MODULE_H__

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _TestModule      TestModule;
typedef struct _TestModuleClass TestModuleClass;

#define TEST_TYPE_MODULE              (test_module_get_type ())
#define TEST_MODULE(module)           (G_TYPE_CHECK_INSTANCE_CAST ((module), TEST_TYPE_MODULE, TestModule))
#define TEST_MODULE_CLASS(class)      (G_TYPE_CHECK_CLASS_CAST ((class), TEST_TYPE_MODULE, TestModuleClass))
#define TEST_IS_MODULE(module)        (G_TYPE_CHECK_INSTANCE_TYPE ((module), TEST_TYPE_MODULE))
#define TEST_IS_MODULE_CLASS(class)   (G_TYPE_CHECK_CLASS_TYPE ((class), TEST_TYPE_MODULE))
#define TEST_MODULE_GET_CLASS(module) (G_TYPE_INSTANCE_GET_CLASS ((module), TEST_TYPE_MODULE, TestModuleClass))

typedef void (*TestModuleRegisterFunc) (GTypeModule *module);

struct _TestModule 
{
  GTypeModule parent_instance;

  TestModuleRegisterFunc register_func;
};

struct _TestModuleClass
{
  GTypeModuleClass parent_class;
};

GType        test_module_get_type      (void);
GTypeModule *test_module_new           (TestModuleRegisterFunc register_func);

G_END_DECLS

#endif /* __TEST_MODULE_H__ */
