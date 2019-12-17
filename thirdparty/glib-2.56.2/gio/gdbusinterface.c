/* GDBus - GLib D-Bus Library
 *
 * Copyright (C) 2008-2010 Red Hat, Inc.
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
 *
 * Author: David Zeuthen <davidz@redhat.com>
 */

#include "config.h"

#include "gdbusobject.h"
#include "gdbusinterface.h"

#include "glibintl.h"

/**
 * SECTION:gdbusinterface
 * @short_description: Base type for D-Bus interfaces
 * @include: gio/gio.h
 *
 * The #GDBusInterface type is the base type for D-Bus interfaces both
 * on the service side (see #GDBusInterfaceSkeleton) and client side
 * (see #GDBusProxy).
 */

typedef GDBusInterfaceIface GDBusInterfaceInterface;
G_DEFINE_INTERFACE (GDBusInterface, g_dbus_interface, G_TYPE_OBJECT)

static void
g_dbus_interface_default_init (GDBusInterfaceIface *iface)
{
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_dbus_interface_get_info:
 * @interface_: An exported D-Bus interface.
 *
 * Gets D-Bus introspection information for the D-Bus interface
 * implemented by @interface_.
 *
 * Returns: (transfer none): A #GDBusInterfaceInfo. Do not free.
 *
 * Since: 2.30
 */
GDBusInterfaceInfo *
g_dbus_interface_get_info (GDBusInterface *interface_)
{
  g_return_val_if_fail (G_IS_DBUS_INTERFACE (interface_), NULL);
  return G_DBUS_INTERFACE_GET_IFACE (interface_)->get_info (interface_);
}

/**
 * g_dbus_interface_get_object: (skip)
 * @interface_: An exported D-Bus interface
 *
 * Gets the #GDBusObject that @interface_ belongs to, if any.
 *
 * It is not safe to use the returned object if @interface_ or
 * the returned object is being used from other threads. See
 * g_dbus_interface_dup_object() for a thread-safe alternative.
 *
 * Returns: (transfer none): A #GDBusObject or %NULL. The returned
 *     reference belongs to @interface_ and should not be freed.
 *
 * Since: 2.30
 */
GDBusObject *
g_dbus_interface_get_object (GDBusInterface *interface_)
{
  g_return_val_if_fail (G_IS_DBUS_INTERFACE (interface_), NULL);
  return G_DBUS_INTERFACE_GET_IFACE (interface_)->get_object (interface_);
}

/**
 * g_dbus_interface_dup_object: (rename-to g_dbus_interface_get_object)
 * @interface_: An exported D-Bus interface.
 *
 * Gets the #GDBusObject that @interface_ belongs to, if any.
 *
 * Returns: (transfer full): A #GDBusObject or %NULL. The returned
 * reference should be freed with g_object_unref().
 *
 * Since: 2.32
 */
GDBusObject *
g_dbus_interface_dup_object (GDBusInterface *interface_)
{
  GDBusObject *ret;
  g_return_val_if_fail (G_IS_DBUS_INTERFACE (interface_), NULL);
  if (G_LIKELY (G_DBUS_INTERFACE_GET_IFACE (interface_)->dup_object != NULL))
    {
      ret = G_DBUS_INTERFACE_GET_IFACE (interface_)->dup_object (interface_);
    }
  else
    {
      g_warning ("No dup_object() vfunc on type %s - using get_object() in a way that is not thread-safe.",
                 g_type_name_from_instance ((GTypeInstance *) interface_));
      ret = G_DBUS_INTERFACE_GET_IFACE (interface_)->get_object (interface_);
      if (ret != NULL)
        g_object_ref (ret);
    }
  return ret;
}

/**
 * g_dbus_interface_set_object:
 * @interface_: An exported D-Bus interface.
 * @object: (nullable): A #GDBusObject or %NULL.
 *
 * Sets the #GDBusObject for @interface_ to @object.
 *
 * Note that @interface_ will hold a weak reference to @object.
 *
 * Since: 2.30
 */
void
g_dbus_interface_set_object (GDBusInterface    *interface_,
                             GDBusObject       *object)
{
  g_return_if_fail (G_IS_DBUS_INTERFACE (interface_));
  g_return_if_fail (object == NULL || G_IS_DBUS_OBJECT (object));
  G_DBUS_INTERFACE_GET_IFACE (interface_)->set_object (interface_, object);
}
