/* GObject - GLib Type, Object, Parameter and Signal Library
 * Copyright (C) 2000 Red Hat, Inc.
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

#include "config.h"

#include <stdlib.h>

#include "gtypeplugin.h"
#include "gtypemodule.h"


/**
 * SECTION:gtypemodule
 * @short_description: Type loading modules
 * @see_also: #GTypePlugin, #GModule
 * @title: GTypeModule
 *
 * #GTypeModule provides a simple implementation of the #GTypePlugin
 * interface. The model of #GTypeModule is a dynamically loaded module
 * which implements some number of types and interface implementations.
 * When the module is loaded, it registers its types and interfaces
 * using g_type_module_register_type() and g_type_module_add_interface().
 * As long as any instances of these types and interface implementations
 * are in use, the module is kept loaded. When the types and interfaces
 * are gone, the module may be unloaded. If the types and interfaces
 * become used again, the module will be reloaded. Note that the last
 * unref cannot happen in module code, since that would lead to the
 * caller's code being unloaded before g_object_unref() returns to it.
 *
 * Keeping track of whether the module should be loaded or not is done by
 * using a use count - it starts at zero, and whenever it is greater than
 * zero, the module is loaded. The use count is maintained internally by
 * the type system, but also can be explicitly controlled by
 * g_type_module_use() and g_type_module_unuse(). Typically, when loading
 * a module for the first type, g_type_module_use() will be used to load
 * it so that it can initialize its types. At some later point, when the
 * module no longer needs to be loaded except for the type
 * implementations it contains, g_type_module_unuse() is called.
 *
 * #GTypeModule does not actually provide any implementation of module
 * loading and unloading. To create a particular module type you must
 * derive from #GTypeModule and implement the load and unload functions
 * in #GTypeModuleClass.
 */


typedef struct _ModuleTypeInfo ModuleTypeInfo;
typedef struct _ModuleInterfaceInfo ModuleInterfaceInfo;

struct _ModuleTypeInfo 
{
  gboolean  loaded;
  GType     type;
  GType     parent_type;
  GTypeInfo info;
};

struct _ModuleInterfaceInfo 
{
  gboolean       loaded;
  GType          instance_type;
  GType          interface_type;
  GInterfaceInfo info;
};

static void g_type_module_use_plugin              (GTypePlugin     *plugin);
static void g_type_module_complete_type_info      (GTypePlugin     *plugin,
						   GType            g_type,
						   GTypeInfo       *info,
						   GTypeValueTable *value_table);
static void g_type_module_complete_interface_info (GTypePlugin     *plugin,
						   GType            instance_type,
						   GType            interface_type,
						   GInterfaceInfo  *info);
 
static gpointer parent_class = NULL;

static void
g_type_module_dispose (GObject *object)
{
  GTypeModule *module = G_TYPE_MODULE (object);
  
  if (module->type_infos || module->interface_infos)
    {
      g_warning (G_STRLOC ": unsolicitated invocation of g_object_run_dispose() on GTypeModule");

      g_object_ref (object);
    }

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
g_type_module_finalize (GObject *object)
{
  GTypeModule *module = G_TYPE_MODULE (object);

  g_free (module->name);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
g_type_module_class_init (GTypeModuleClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  parent_class = G_OBJECT_CLASS (g_type_class_peek_parent (class));
  
  gobject_class->dispose = g_type_module_dispose;
  gobject_class->finalize = g_type_module_finalize;
}

static void
g_type_module_iface_init (GTypePluginClass *iface)
{
  iface->use_plugin = g_type_module_use_plugin;
  iface->unuse_plugin = (void (*) (GTypePlugin *))g_type_module_unuse;
  iface->complete_type_info = g_type_module_complete_type_info;
  iface->complete_interface_info = g_type_module_complete_interface_info;
}

GType
g_type_module_get_type (void)
{
  static GType type_module_type = 0;

  if (!type_module_type)
    {
      const GTypeInfo type_module_info = {
        sizeof (GTypeModuleClass),
        NULL,           /* base_init */
        NULL,           /* base_finalize */
        (GClassInitFunc) g_type_module_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (GTypeModule),
        0,              /* n_preallocs */
        NULL,           /* instance_init */
      };
      const GInterfaceInfo iface_info = {
        (GInterfaceInitFunc) g_type_module_iface_init,
        NULL,               /* interface_finalize */
        NULL,               /* interface_data */
      };

      type_module_type = g_type_register_static (G_TYPE_OBJECT, g_intern_static_string ("GTypeModule"), &type_module_info, G_TYPE_FLAG_ABSTRACT);

      g_type_add_interface_static (type_module_type, G_TYPE_TYPE_PLUGIN, &iface_info);
    }
  
  return type_module_type;
}

/**
 * g_type_module_set_name:
 * @module: a #GTypeModule.
 * @name: a human-readable name to use in error messages.
 * 
 * Sets the name for a #GTypeModule 
 */
void
g_type_module_set_name (GTypeModule  *module,
			const gchar  *name)
{
  g_return_if_fail (G_IS_TYPE_MODULE (module));

  g_free (module->name);
  module->name = g_strdup (name);
}

static ModuleTypeInfo *
g_type_module_find_type_info (GTypeModule *module,
			      GType        type)
{
  GSList *tmp_list = module->type_infos;
  while (tmp_list)
    {
      ModuleTypeInfo *type_info = tmp_list->data;
      if (type_info->type == type)
	return type_info;
      
      tmp_list = tmp_list->next;
    }

  return NULL;
}

static ModuleInterfaceInfo *
g_type_module_find_interface_info (GTypeModule *module,
				   GType        instance_type,
				   GType        interface_type)
{
  GSList *tmp_list = module->interface_infos;
  while (tmp_list)
    {
      ModuleInterfaceInfo *interface_info = tmp_list->data;
      if (interface_info->instance_type == instance_type &&
	  interface_info->interface_type == interface_type)
	return interface_info;
      
      tmp_list = tmp_list->next;
    }

  return NULL;
}

/**
 * g_type_module_use:
 * @module: a #GTypeModule
 * 
 * Increases the use count of a #GTypeModule by one. If the
 * use count was zero before, the plugin will be loaded.
 * If loading the plugin fails, the use count is reset to 
 * its prior value. 
 * 
 * Returns: %FALSE if the plugin needed to be loaded and
 *  loading the plugin failed.
 */
gboolean
g_type_module_use (GTypeModule *module)
{
  g_return_val_if_fail (G_IS_TYPE_MODULE (module), FALSE);

  module->use_count++;
  if (module->use_count == 1)
    {
      GSList *tmp_list;
      
      if (!G_TYPE_MODULE_GET_CLASS (module)->load (module))
	{
	  module->use_count--;
	  return FALSE;
	}

      tmp_list = module->type_infos;
      while (tmp_list)
	{
	  ModuleTypeInfo *type_info = tmp_list->data;
	  if (!type_info->loaded)
	    {
	      g_warning ("plugin '%s' failed to register type '%s'\n",
			 module->name ? module->name : "(unknown)",
			 g_type_name (type_info->type));
	      module->use_count--;
	      return FALSE;
	    }
	  
	  tmp_list = tmp_list->next;
	}
    }
 
  return TRUE;
}

/**
 * g_type_module_unuse:
 * @module: a #GTypeModule
 *
 * Decreases the use count of a #GTypeModule by one. If the
 * result is zero, the module will be unloaded. (However, the
 * #GTypeModule will not be freed, and types associated with the
 * #GTypeModule are not unregistered. Once a #GTypeModule is
 * initialized, it must exist forever.)
 */
void
g_type_module_unuse (GTypeModule *module)
{
  g_return_if_fail (G_IS_TYPE_MODULE (module));
  g_return_if_fail (module->use_count > 0);

  module->use_count--;

  if (module->use_count == 0)
    {
      GSList *tmp_list;

      G_TYPE_MODULE_GET_CLASS (module)->unload (module);

      tmp_list = module->type_infos;
      while (tmp_list)
	{
	  ModuleTypeInfo *type_info = tmp_list->data;
	  type_info->loaded = FALSE;

	  tmp_list = tmp_list->next;
	}
    }
}
	
static void
g_type_module_use_plugin (GTypePlugin *plugin)
{
  GTypeModule *module = G_TYPE_MODULE (plugin);

  if (!g_type_module_use (module))
    {
      g_warning ("Fatal error - Could not reload previously loaded plugin '%s'\n",
		 module->name ? module->name : "(unknown)");
      exit (1);
    }
}

static void
g_type_module_complete_type_info (GTypePlugin     *plugin,
				  GType            g_type,
				  GTypeInfo       *info,
				  GTypeValueTable *value_table)
{
  GTypeModule *module = G_TYPE_MODULE (plugin);
  ModuleTypeInfo *module_type_info = g_type_module_find_type_info (module, g_type);

  *info = module_type_info->info;
  
  if (module_type_info->info.value_table)
    *value_table = *module_type_info->info.value_table;
}

static void 
g_type_module_complete_interface_info (GTypePlugin    *plugin,
				       GType           instance_type,
				       GType           interface_type,
				       GInterfaceInfo *info)
{
  GTypeModule *module = G_TYPE_MODULE (plugin);
  ModuleInterfaceInfo *module_interface_info = g_type_module_find_interface_info (module, instance_type, interface_type);

  *info = module_interface_info->info;
}

/**
 * g_type_module_register_type:
 * @module: (nullable): a #GTypeModule
 * @parent_type: the type for the parent class
 * @type_name: name for the type
 * @type_info: type information structure
 * @flags: flags field providing details about the type
 *
 * Looks up or registers a type that is implemented with a particular
 * type plugin. If a type with name @type_name was previously registered,
 * the #GType identifier for the type is returned, otherwise the type
 * is newly registered, and the resulting #GType identifier returned.
 *
 * When reregistering a type (typically because a module is unloaded
 * then reloaded, and reinitialized), @module and @parent_type must
 * be the same as they were previously.
 *
 * As long as any instances of the type exist, the type plugin will
 * not be unloaded.
 *
 * Since 2.56 if @module is %NULL this will call g_type_register_static()
 * instead. This can be used when making a static build of the module.
 *
 * Returns: the new or existing type ID
 */
GType
g_type_module_register_type (GTypeModule     *module,
			     GType            parent_type,
			     const gchar     *type_name,
			     const GTypeInfo *type_info,
			     GTypeFlags       flags)
{
  ModuleTypeInfo *module_type_info = NULL;
  GType type;
  
  g_return_val_if_fail (type_name != NULL, 0);
  g_return_val_if_fail (type_info != NULL, 0);

  if (module == NULL)
    {
      /* Cannot pass type_info directly to g_type_register_static() here because
       * it has class_finalize != NULL and that's forbidden for static types */
      return g_type_register_static_simple (parent_type,
                                            type_name,
                                            type_info->class_size,
                                            type_info->class_init,
                                            type_info->instance_size,
                                            type_info->instance_init,
                                            flags);
    }

  type = g_type_from_name (type_name);
  if (type)
    {
      GTypePlugin *old_plugin = g_type_get_plugin (type);

      if (old_plugin != G_TYPE_PLUGIN (module))
	{
	  g_warning ("Two different plugins tried to register '%s'.", type_name);
	  return 0;
	}
    }

  if (type)
    {
      module_type_info = g_type_module_find_type_info (module, type);

      if (module_type_info->parent_type != parent_type)
	{
	  const gchar *parent_type_name = g_type_name (parent_type);
	  
	  g_warning ("Type '%s' recreated with different parent type.\n"
		     "(was '%s', now '%s')", type_name,
		     g_type_name (module_type_info->parent_type),
		     parent_type_name ? parent_type_name : "(unknown)");
	  return 0;
	}

      if (module_type_info->info.value_table)
	g_free ((GTypeValueTable *) module_type_info->info.value_table);
    }
  else
    {
      module_type_info = g_new (ModuleTypeInfo, 1);
      
      module_type_info->parent_type = parent_type;
      module_type_info->type = g_type_register_dynamic (parent_type, type_name, G_TYPE_PLUGIN (module), flags);
      
      module->type_infos = g_slist_prepend (module->type_infos, module_type_info);
    }

  module_type_info->loaded = TRUE;
  module_type_info->info = *type_info;
  if (type_info->value_table)
    module_type_info->info.value_table = g_memdup (type_info->value_table,
						   sizeof (GTypeValueTable));

  return module_type_info->type;
}

/**
 * g_type_module_add_interface:
 * @module: (nullable): a #GTypeModule
 * @instance_type: type to which to add the interface.
 * @interface_type: interface type to add
 * @interface_info: type information structure
 *
 * Registers an additional interface for a type, whose interface lives
 * in the given type plugin. If the interface was already registered
 * for the type in this plugin, nothing will be done.
 *
 * As long as any instances of the type exist, the type plugin will
 * not be unloaded.
 *
 * Since 2.56 if @module is %NULL this will call g_type_add_interface_static()
 * instead. This can be used when making a static build of the module.
 */
void
g_type_module_add_interface (GTypeModule          *module,
			     GType                 instance_type,
			     GType                 interface_type,
			     const GInterfaceInfo *interface_info)
{
  ModuleInterfaceInfo *module_interface_info = NULL;

  g_return_if_fail (interface_info != NULL);

  if (module == NULL)
    {
      g_type_add_interface_static (instance_type, interface_type, interface_info);
      return;
    }

  if (g_type_is_a (instance_type, interface_type))
    {
      GTypePlugin *old_plugin = g_type_interface_get_plugin (instance_type,
							     interface_type);

      if (!old_plugin)
	{
	  g_warning ("Interface '%s' for '%s' was previously registered statically or for a parent type.",
		     g_type_name (interface_type), g_type_name (instance_type));
	  return;
	}
      else if (old_plugin != G_TYPE_PLUGIN (module))
	{
	  g_warning ("Two different plugins tried to register interface '%s' for '%s'.",
		     g_type_name (interface_type), g_type_name (instance_type));
	  return;
	}
      
      module_interface_info = g_type_module_find_interface_info (module, instance_type, interface_type);

      g_assert (module_interface_info);
    }
  else
    {
      module_interface_info = g_new (ModuleInterfaceInfo, 1);
      
      module_interface_info->instance_type = instance_type;
      module_interface_info->interface_type = interface_type;
      
      g_type_add_interface_dynamic (instance_type, interface_type, G_TYPE_PLUGIN (module));
      
      module->interface_infos = g_slist_prepend (module->interface_infos, module_interface_info);
    }
  
  module_interface_info->loaded = TRUE;
  module_interface_info->info = *interface_info;
}

/**
 * g_type_module_register_enum:
 * @module: (nullable): a #GTypeModule
 * @name: name for the type
 * @const_static_values: an array of #GEnumValue structs for the
 *                       possible enumeration values. The array is
 *                       terminated by a struct with all members being
 *                       0.
 *
 * Looks up or registers an enumeration that is implemented with a particular
 * type plugin. If a type with name @type_name was previously registered,
 * the #GType identifier for the type is returned, otherwise the type
 * is newly registered, and the resulting #GType identifier returned.
 *
 * As long as any instances of the type exist, the type plugin will
 * not be unloaded.
 *
 * Since 2.56 if @module is %NULL this will call g_type_register_static()
 * instead. This can be used when making a static build of the module.
 *
 * Since: 2.6
 *
 * Returns: the new or existing type ID
 */
GType
g_type_module_register_enum (GTypeModule      *module,
                             const gchar      *name,
                             const GEnumValue *const_static_values)
{
  GTypeInfo enum_type_info = { 0, };

  g_return_val_if_fail (module == NULL || G_IS_TYPE_MODULE (module), 0);
  g_return_val_if_fail (name != NULL, 0);
  g_return_val_if_fail (const_static_values != NULL, 0);

  g_enum_complete_type_info (G_TYPE_ENUM,
                             &enum_type_info, const_static_values);

  return g_type_module_register_type (G_TYPE_MODULE (module),
                                      G_TYPE_ENUM, name, &enum_type_info, 0);
}

/**
 * g_type_module_register_flags:
 * @module: (nullable): a #GTypeModule
 * @name: name for the type
 * @const_static_values: an array of #GFlagsValue structs for the
 *                       possible flags values. The array is
 *                       terminated by a struct with all members being
 *                       0.
 *
 * Looks up or registers a flags type that is implemented with a particular
 * type plugin. If a type with name @type_name was previously registered,
 * the #GType identifier for the type is returned, otherwise the type
 * is newly registered, and the resulting #GType identifier returned.
 *
 * As long as any instances of the type exist, the type plugin will
 * not be unloaded.
 *
 * Since 2.56 if @module is %NULL this will call g_type_register_static()
 * instead. This can be used when making a static build of the module.
 *
 * Since: 2.6
 *
 * Returns: the new or existing type ID
 */
GType
g_type_module_register_flags (GTypeModule      *module,
                             const gchar       *name,
                             const GFlagsValue *const_static_values)
{
  GTypeInfo flags_type_info = { 0, };

  g_return_val_if_fail (module == NULL || G_IS_TYPE_MODULE (module), 0);
  g_return_val_if_fail (name != NULL, 0);
  g_return_val_if_fail (const_static_values != NULL, 0);

  g_flags_complete_type_info (G_TYPE_FLAGS,
                             &flags_type_info, const_static_values);

  return g_type_module_register_type (G_TYPE_MODULE (module),
                                      G_TYPE_FLAGS, name, &flags_type_info, 0);
}
