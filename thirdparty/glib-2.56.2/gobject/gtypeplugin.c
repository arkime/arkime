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
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtypeplugin.h"


/**
 * SECTION:gtypeplugin
 * @short_description: An interface for dynamically loadable types
 * @see_also: #GTypeModule and g_type_register_dynamic().
 * @title: GTypePlugin
 *
 * The GObject type system supports dynamic loading of types.
 * The #GTypePlugin interface is used to handle the lifecycle
 * of dynamically loaded types. It goes as follows:
 *
 * 1. The type is initially introduced (usually upon loading the module
 *    the first time, or by your main application that knows what modules
 *    introduces what types), like this:
 *    |[<!-- language="C" --> 
 *    new_type_id = g_type_register_dynamic (parent_type_id,
 *                                           "TypeName",
 *                                           new_type_plugin,
 *                                           type_flags);
 *    ]|
 *    where @new_type_plugin is an implementation of the
 *    #GTypePlugin interface.
 *
 * 2. The type's implementation is referenced, e.g. through
 *    g_type_class_ref() or through g_type_create_instance() (this is
 *    being called by g_object_new()) or through one of the above done on
 *    a type derived from @new_type_id.
 *
 * 3. This causes the type system to load the type's implementation by
 *    calling g_type_plugin_use() and g_type_plugin_complete_type_info()
 *    on @new_type_plugin.
 * 
 * 4. At some point the type's implementation isn't required anymore,
 *    e.g. after g_type_class_unref() or g_type_free_instance() (called
 *    when the reference count of an instance drops to zero).
 *
 * 5. This causes the type system to throw away the information retrieved
 *    from g_type_plugin_complete_type_info() and then it calls
 *    g_type_plugin_unuse() on @new_type_plugin.
 * 
 * 6. Things may repeat from the second step.
 *
 * So basically, you need to implement a #GTypePlugin type that
 * carries a use_count, once use_count goes from zero to one, you need
 * to load the implementation to successfully handle the upcoming
 * g_type_plugin_complete_type_info() call. Later, maybe after
 * succeeding use/unuse calls, once use_count drops to zero, you can
 * unload the implementation again. The type system makes sure to call
 * g_type_plugin_use() and g_type_plugin_complete_type_info() again
 * when the type is needed again.
 *
 * #GTypeModule is an implementation of #GTypePlugin that already
 * implements most of this except for the actual module loading and
 * unloading. It even handles multiple registered types per module.
 */


/* --- functions --- */
GType
g_type_plugin_get_type (void)
{
  static GType type_plugin_type = 0;
  
  if (!type_plugin_type)
    {
      const GTypeInfo type_plugin_info = {
	sizeof (GTypePluginClass),
	NULL,           /* base_init */
	NULL,           /* base_finalize */
      };
      
      type_plugin_type = g_type_register_static (G_TYPE_INTERFACE, g_intern_static_string ("GTypePlugin"), &type_plugin_info, 0);
    }
  
  return type_plugin_type;
}

/**
 * g_type_plugin_use:
 * @plugin: a #GTypePlugin
 *
 * Calls the @use_plugin function from the #GTypePluginClass of
 * @plugin.  There should be no need to use this function outside of
 * the GObject type system itself.
 */
void
g_type_plugin_use (GTypePlugin *plugin)
{
  GTypePluginClass *iface;
  
  g_return_if_fail (G_IS_TYPE_PLUGIN (plugin));
  
  iface = G_TYPE_PLUGIN_GET_CLASS (plugin);
  iface->use_plugin (plugin);
}

/**
 * g_type_plugin_unuse:
 * @plugin: a #GTypePlugin
 *
 * Calls the @unuse_plugin function from the #GTypePluginClass of
 * @plugin.  There should be no need to use this function outside of
 * the GObject type system itself.
 */
void
g_type_plugin_unuse (GTypePlugin *plugin)
{
  GTypePluginClass *iface;
  
  g_return_if_fail (G_IS_TYPE_PLUGIN (plugin));
  
  iface = G_TYPE_PLUGIN_GET_CLASS (plugin);
  iface->unuse_plugin (plugin);
}

/**
 * g_type_plugin_complete_type_info:
 * @plugin: a #GTypePlugin
 * @g_type: the #GType whose info is completed
 * @info: the #GTypeInfo struct to fill in
 * @value_table: the #GTypeValueTable to fill in
 * 
 * Calls the @complete_type_info function from the #GTypePluginClass of @plugin.
 * There should be no need to use this function outside of the GObject 
 * type system itself.
 */
void
g_type_plugin_complete_type_info (GTypePlugin     *plugin,
				  GType            g_type,
				  GTypeInfo       *info,
				  GTypeValueTable *value_table)
{
  GTypePluginClass *iface;
  
  g_return_if_fail (G_IS_TYPE_PLUGIN (plugin));
  g_return_if_fail (info != NULL);
  g_return_if_fail (value_table != NULL);
  
  iface = G_TYPE_PLUGIN_GET_CLASS (plugin);
  iface->complete_type_info (plugin,
			     g_type,
			     info,
			     value_table);
}

/**
 * g_type_plugin_complete_interface_info:
 * @plugin: the #GTypePlugin
 * @instance_type: the #GType of an instantiable type to which the interface
 *  is added
 * @interface_type: the #GType of the interface whose info is completed
 * @info: the #GInterfaceInfo to fill in
 *
 * Calls the @complete_interface_info function from the
 * #GTypePluginClass of @plugin. There should be no need to use this
 * function outside of the GObject type system itself.
 */
void
g_type_plugin_complete_interface_info (GTypePlugin    *plugin,
				       GType           instance_type,
				       GType           interface_type,
				       GInterfaceInfo *info)
{
  GTypePluginClass *iface;
  
  g_return_if_fail (G_IS_TYPE_PLUGIN (plugin));
  g_return_if_fail (info != NULL);
  
  iface = G_TYPE_PLUGIN_GET_CLASS (plugin);
  iface->complete_interface_info (plugin,
				  instance_type,
				  interface_type,
				  info);
}
