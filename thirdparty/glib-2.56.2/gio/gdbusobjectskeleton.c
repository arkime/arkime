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
#include "gdbusobjectskeleton.h"
#include "gdbusinterfaceskeleton.h"
#include "gdbusprivate.h"
#include "gdbusmethodinvocation.h"
#include "gdbusintrospection.h"
#include "gdbusinterface.h"
#include "gdbusutils.h"

#include "glibintl.h"

/**
 * SECTION:gdbusobjectskeleton
 * @short_description: Service-side D-Bus object
 * @include: gio/gio.h
 *
 * A #GDBusObjectSkeleton instance is essentially a group of D-Bus
 * interfaces. The set of exported interfaces on the object may be
 * dynamic and change at runtime.
 *
 * This type is intended to be used with #GDBusObjectManager.
 */

struct _GDBusObjectSkeletonPrivate
{
  GMutex lock;
  gchar *object_path;
  GHashTable *map_name_to_iface;
};

enum
{
  PROP_0,
  PROP_G_OBJECT_PATH
};

enum
{
  AUTHORIZE_METHOD_SIGNAL,
  LAST_SIGNAL,
};

static guint signals[LAST_SIGNAL] = {0};

static void dbus_object_interface_init (GDBusObjectIface *iface);

G_DEFINE_TYPE_WITH_CODE (GDBusObjectSkeleton, g_dbus_object_skeleton, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (GDBusObjectSkeleton)
                         G_IMPLEMENT_INTERFACE (G_TYPE_DBUS_OBJECT, dbus_object_interface_init))


static void
g_dbus_object_skeleton_finalize (GObject *_object)
{
  GDBusObjectSkeleton *object = G_DBUS_OBJECT_SKELETON (_object);

  g_free (object->priv->object_path);
  g_hash_table_unref (object->priv->map_name_to_iface);

  g_mutex_clear (&object->priv->lock);

  if (G_OBJECT_CLASS (g_dbus_object_skeleton_parent_class)->finalize != NULL)
    G_OBJECT_CLASS (g_dbus_object_skeleton_parent_class)->finalize (_object);
}

static void
g_dbus_object_skeleton_get_property (GObject    *_object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  GDBusObjectSkeleton *object = G_DBUS_OBJECT_SKELETON (_object);

  switch (prop_id)
    {
    case PROP_G_OBJECT_PATH:
      g_mutex_lock (&object->priv->lock);
      g_value_set_string (value, object->priv->object_path);
      g_mutex_unlock (&object->priv->lock);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
g_dbus_object_skeleton_set_property (GObject       *_object,
                                     guint          prop_id,
                                     const GValue  *value,
                                     GParamSpec    *pspec)
{
  GDBusObjectSkeleton *object = G_DBUS_OBJECT_SKELETON (_object);

  switch (prop_id)
    {
    case PROP_G_OBJECT_PATH:
      g_dbus_object_skeleton_set_object_path (object, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static gboolean
g_dbus_object_skeleton_authorize_method_default (GDBusObjectSkeleton    *object,
                                                 GDBusInterfaceSkeleton *interface,
                                                 GDBusMethodInvocation  *invocation)
{
  return TRUE;
}

static void
g_dbus_object_skeleton_class_init (GDBusObjectSkeletonClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize     = g_dbus_object_skeleton_finalize;
  gobject_class->set_property = g_dbus_object_skeleton_set_property;
  gobject_class->get_property = g_dbus_object_skeleton_get_property;

  klass->authorize_method = g_dbus_object_skeleton_authorize_method_default;

  /**
   * GDBusObjectSkeleton:g-object-path:
   *
   * The object path where the object is exported.
   *
   * Since: 2.30
   */
  g_object_class_install_property (gobject_class,
                                   PROP_G_OBJECT_PATH,
                                   g_param_spec_string ("g-object-path",
                                                        "Object Path",
                                                        "The object path where the object is exported",
                                                        NULL,
                                                        G_PARAM_READABLE |
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT |
                                                        G_PARAM_STATIC_STRINGS));

  /**
   * GDBusObjectSkeleton::authorize-method:
   * @object: The #GDBusObjectSkeleton emitting the signal.
   * @interface: The #GDBusInterfaceSkeleton that @invocation is for.
   * @invocation: A #GDBusMethodInvocation.
   *
   * Emitted when a method is invoked by a remote caller and used to
   * determine if the method call is authorized.
   *
   * This signal is like #GDBusInterfaceSkeleton's
   * #GDBusInterfaceSkeleton::g-authorize-method signal,
   * except that it is for the enclosing object.
   *
   * The default class handler just returns %TRUE.
   *
   * Returns: %TRUE if the call is authorized, %FALSE otherwise.
   *
   * Since: 2.30
   */
  signals[AUTHORIZE_METHOD_SIGNAL] =
    g_signal_new (I_("authorize-method"),
                  G_TYPE_DBUS_OBJECT_SKELETON,
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GDBusObjectSkeletonClass, authorize_method),
                  _g_signal_accumulator_false_handled,
                  NULL,
                  NULL,
                  G_TYPE_BOOLEAN,
                  2,
                  G_TYPE_DBUS_INTERFACE_SKELETON,
                  G_TYPE_DBUS_METHOD_INVOCATION);
}

static void
g_dbus_object_skeleton_init (GDBusObjectSkeleton *object)
{
  object->priv = g_dbus_object_skeleton_get_instance_private (object);
  g_mutex_init (&object->priv->lock);
  object->priv->map_name_to_iface = g_hash_table_new_full (g_str_hash,
                                                           g_str_equal,
                                                           g_free,
                                                           (GDestroyNotify) g_object_unref);
}

/**
 * g_dbus_object_skeleton_new:
 * @object_path: An object path.
 *
 * Creates a new #GDBusObjectSkeleton.
 *
 * Returns: A #GDBusObjectSkeleton. Free with g_object_unref().
 *
 * Since: 2.30
 */
GDBusObjectSkeleton *
g_dbus_object_skeleton_new (const gchar *object_path)
{
  g_return_val_if_fail (g_variant_is_object_path (object_path), NULL);
  return G_DBUS_OBJECT_SKELETON (g_object_new (G_TYPE_DBUS_OBJECT_SKELETON,
                                               "g-object-path", object_path,
                                               NULL));
}

/**
 * g_dbus_object_skeleton_set_object_path:
 * @object: A #GDBusObjectSkeleton.
 * @object_path: A valid D-Bus object path.
 *
 * Sets the object path for @object.
 *
 * Since: 2.30
 */
void
g_dbus_object_skeleton_set_object_path (GDBusObjectSkeleton *object,
                                        const gchar     *object_path)
{
  g_return_if_fail (G_IS_DBUS_OBJECT_SKELETON (object));
  g_return_if_fail (object_path == NULL || g_variant_is_object_path (object_path));
  g_mutex_lock (&object->priv->lock);
  /* TODO: fail if object is currently exported */
  if (g_strcmp0 (object->priv->object_path, object_path) != 0)
    {
      g_free (object->priv->object_path);
      object->priv->object_path = g_strdup (object_path);
      g_mutex_unlock (&object->priv->lock);
      g_object_notify (G_OBJECT (object), "g-object-path");
    }
  else
    {
      g_mutex_unlock (&object->priv->lock);
    }
}

static const gchar *
g_dbus_object_skeleton_get_object_path (GDBusObject *_object)
{
  GDBusObjectSkeleton *object = G_DBUS_OBJECT_SKELETON (_object);
  const gchar *ret;
  g_mutex_lock (&object->priv->lock);
  ret = object->priv->object_path;
  g_mutex_unlock (&object->priv->lock);
  return ret;
}

/**
 * g_dbus_object_skeleton_add_interface:
 * @object: A #GDBusObjectSkeleton.
 * @interface_: A #GDBusInterfaceSkeleton.
 *
 * Adds @interface_ to @object.
 *
 * If @object already contains a #GDBusInterfaceSkeleton with the same
 * interface name, it is removed before @interface_ is added.
 *
 * Note that @object takes its own reference on @interface_ and holds
 * it until removed.
 *
 * Since: 2.30
 */
void
g_dbus_object_skeleton_add_interface (GDBusObjectSkeleton     *object,
                                      GDBusInterfaceSkeleton  *interface_)
{
  GDBusInterfaceInfo *info;
  GDBusInterface *interface_to_remove;

  g_return_if_fail (G_IS_DBUS_OBJECT_SKELETON (object));
  g_return_if_fail (G_IS_DBUS_INTERFACE_SKELETON (interface_));

  g_mutex_lock (&object->priv->lock);

  info = g_dbus_interface_skeleton_get_info (interface_);
  g_object_ref (interface_);

  interface_to_remove = g_hash_table_lookup (object->priv->map_name_to_iface, info->name);
  if (interface_to_remove != NULL)
    {
      g_object_ref (interface_to_remove);
      g_warn_if_fail (g_hash_table_remove (object->priv->map_name_to_iface, info->name));
    }
  g_hash_table_insert (object->priv->map_name_to_iface,
                       g_strdup (info->name),
                       g_object_ref (interface_));
  g_dbus_interface_set_object (G_DBUS_INTERFACE (interface_), G_DBUS_OBJECT (object));

  g_mutex_unlock (&object->priv->lock);

  if (interface_to_remove != NULL)
    {
      g_dbus_interface_set_object (interface_to_remove, NULL);
      g_signal_emit_by_name (object,
                             "interface-removed",
                             interface_to_remove);
      g_object_unref (interface_to_remove);
    }

  g_signal_emit_by_name (object,
                         "interface-added",
                         interface_);
  g_object_unref (interface_);
}

/**
 * g_dbus_object_skeleton_remove_interface:
 * @object: A #GDBusObjectSkeleton.
 * @interface_: A #GDBusInterfaceSkeleton.
 *
 * Removes @interface_ from @object.
 *
 * Since: 2.30
 */
void
g_dbus_object_skeleton_remove_interface  (GDBusObjectSkeleton    *object,
                                          GDBusInterfaceSkeleton *interface_)
{
  GDBusInterfaceSkeleton *other_interface;
  GDBusInterfaceInfo *info;

  g_return_if_fail (G_IS_DBUS_OBJECT_SKELETON (object));
  g_return_if_fail (G_IS_DBUS_INTERFACE (interface_));

  g_mutex_lock (&object->priv->lock);

  info = g_dbus_interface_skeleton_get_info (interface_);

  other_interface = g_hash_table_lookup (object->priv->map_name_to_iface, info->name);
  if (other_interface == NULL)
    {
      g_mutex_unlock (&object->priv->lock);
      g_warning ("Tried to remove interface with name %s from object "
                 "at path %s but no such interface exists",
                 info->name,
                 object->priv->object_path);
    }
  else if (other_interface != interface_)
    {
      g_mutex_unlock (&object->priv->lock);
      g_warning ("Tried to remove interface %p with name %s from object "
                 "at path %s but the object has the interface %p",
                 interface_,
                 info->name,
                 object->priv->object_path,
                 other_interface);
    }
  else
    {
      g_object_ref (interface_);
      g_warn_if_fail (g_hash_table_remove (object->priv->map_name_to_iface, info->name));
      g_mutex_unlock (&object->priv->lock);
      g_dbus_interface_set_object (G_DBUS_INTERFACE (interface_), NULL);
      g_signal_emit_by_name (object,
                             "interface-removed",
                             interface_);
      g_object_unref (interface_);
    }
}


/**
 * g_dbus_object_skeleton_remove_interface_by_name:
 * @object: A #GDBusObjectSkeleton.
 * @interface_name: A D-Bus interface name.
 *
 * Removes the #GDBusInterface with @interface_name from @object.
 *
 * If no D-Bus interface of the given interface exists, this function
 * does nothing.
 *
 * Since: 2.30
 */
void
g_dbus_object_skeleton_remove_interface_by_name (GDBusObjectSkeleton *object,
                                                 const gchar         *interface_name)
{
  GDBusInterface *interface;

  g_return_if_fail (G_IS_DBUS_OBJECT_SKELETON (object));
  g_return_if_fail (g_dbus_is_interface_name (interface_name));

  g_mutex_lock (&object->priv->lock);
  interface = g_hash_table_lookup (object->priv->map_name_to_iface, interface_name);
  if (interface != NULL)
    {
      g_object_ref (interface);
      g_warn_if_fail (g_hash_table_remove (object->priv->map_name_to_iface, interface_name));
      g_mutex_unlock (&object->priv->lock);
      g_dbus_interface_set_object (interface, NULL);
      g_signal_emit_by_name (object,
                             "interface-removed",
                             interface);
      g_object_unref (interface);
    }
  else
    {
      g_mutex_unlock (&object->priv->lock);
    }
}

static GDBusInterface *
g_dbus_object_skeleton_get_interface (GDBusObject  *_object,
                                      const gchar  *interface_name)
{
  GDBusObjectSkeleton *object = G_DBUS_OBJECT_SKELETON (_object);
  GDBusInterface *ret;

  g_return_val_if_fail (G_IS_DBUS_OBJECT_SKELETON (object), NULL);
  g_return_val_if_fail (g_dbus_is_interface_name (interface_name), NULL);

  g_mutex_lock (&object->priv->lock);
  ret = g_hash_table_lookup (object->priv->map_name_to_iface, interface_name);
  if (ret != NULL)
    g_object_ref (ret);
  g_mutex_unlock (&object->priv->lock);
  return ret;
}

static GList *
g_dbus_object_skeleton_get_interfaces (GDBusObject *_object)
{
  GDBusObjectSkeleton *object = G_DBUS_OBJECT_SKELETON (_object);
  GList *ret;

  g_return_val_if_fail (G_IS_DBUS_OBJECT_SKELETON (object), NULL);

  ret = NULL;

  g_mutex_lock (&object->priv->lock);
  ret = g_hash_table_get_values (object->priv->map_name_to_iface);
  g_list_foreach (ret, (GFunc) g_object_ref, NULL);
  g_mutex_unlock (&object->priv->lock);

  return ret;
}

/**
 * g_dbus_object_skeleton_flush:
 * @object: A #GDBusObjectSkeleton.
 *
 * This method simply calls g_dbus_interface_skeleton_flush() on all
 * interfaces belonging to @object. See that method for when flushing
 * is useful.
 *
 * Since: 2.30
 */
void
g_dbus_object_skeleton_flush (GDBusObjectSkeleton *object)
{
  GList *to_flush, *l;

  g_mutex_lock (&object->priv->lock);
  to_flush = g_hash_table_get_values (object->priv->map_name_to_iface);
  g_list_foreach (to_flush, (GFunc) g_object_ref, NULL);
  g_mutex_unlock (&object->priv->lock);

  for (l = to_flush; l != NULL; l = l->next)
    g_dbus_interface_skeleton_flush (G_DBUS_INTERFACE_SKELETON (l->data));

  g_list_free_full (to_flush, g_object_unref);
}

static void
dbus_object_interface_init (GDBusObjectIface *iface)
{
  iface->get_object_path = g_dbus_object_skeleton_get_object_path;
  iface->get_interfaces  = g_dbus_object_skeleton_get_interfaces;
  iface->get_interface  = g_dbus_object_skeleton_get_interface;
}

gboolean
_g_dbus_object_skeleton_has_authorize_method_handlers (GDBusObjectSkeleton *object)
{
  gboolean has_handlers;
  gboolean has_default_class_handler;

  has_handlers = g_signal_has_handler_pending (object,
                                               signals[AUTHORIZE_METHOD_SIGNAL],
                                               0,
                                               TRUE);
  has_default_class_handler = (G_DBUS_OBJECT_SKELETON_GET_CLASS (object)->authorize_method ==
                               g_dbus_object_skeleton_authorize_method_default);

  return has_handlers || !has_default_class_handler;
}
