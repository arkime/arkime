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

#include "gdbusobjectmanager.h"
#include "gdbusobjectmanagerclient.h"
#include "gdbusobject.h"
#include "gdbusprivate.h"
#include "gioenumtypes.h"
#include "ginitable.h"
#include "gasyncresult.h"
#include "gasyncinitable.h"
#include "gdbusconnection.h"
#include "gdbusutils.h"
#include "gdbusobject.h"
#include "gdbusobjectproxy.h"
#include "gdbusproxy.h"
#include "gdbusinterface.h"

#include "glibintl.h"

/**
 * SECTION:gdbusobjectmanagerclient
 * @short_description: Client-side object manager
 * @include: gio/gio.h
 *
 * #GDBusObjectManagerClient is used to create, monitor and delete object
 * proxies for remote objects exported by a #GDBusObjectManagerServer (or any
 * code implementing the
 * [org.freedesktop.DBus.ObjectManager](http://dbus.freedesktop.org/doc/dbus-specification.html#standard-interfaces-objectmanager)
 * interface).
 *
 * Once an instance of this type has been created, you can connect to
 * the #GDBusObjectManager::object-added and
 * #GDBusObjectManager::object-removed signals and inspect the
 * #GDBusObjectProxy objects returned by
 * g_dbus_object_manager_get_objects().
 *
 * If the name for a #GDBusObjectManagerClient is not owned by anyone at
 * object construction time, the default behavior is to request the
 * message bus to launch an owner for the name. This behavior can be
 * disabled using the %G_DBUS_OBJECT_MANAGER_CLIENT_FLAGS_DO_NOT_AUTO_START
 * flag. It's also worth noting that this only works if the name of
 * interest is activatable in the first place. E.g. in some cases it
 * is not possible to launch an owner for the requested name. In this
 * case, #GDBusObjectManagerClient object construction still succeeds but
 * there will be no object proxies
 * (e.g. g_dbus_object_manager_get_objects() returns the empty list) and
 * the #GDBusObjectManagerClient:name-owner property is %NULL.
 *
 * The owner of the requested name can come and go (for example
 * consider a system service being restarted) – #GDBusObjectManagerClient
 * handles this case too; simply connect to the #GObject::notify
 * signal to watch for changes on the #GDBusObjectManagerClient:name-owner
 * property. When the name owner vanishes, the behavior is that
 * #GDBusObjectManagerClient:name-owner is set to %NULL (this includes
 * emission of the #GObject::notify signal) and then
 * #GDBusObjectManager::object-removed signals are synthesized
 * for all currently existing object proxies. Since
 * #GDBusObjectManagerClient:name-owner is %NULL when this happens, you can
 * use this information to disambiguate a synthesized signal from a
 * genuine signal caused by object removal on the remote
 * #GDBusObjectManager. Similarly, when a new name owner appears,
 * #GDBusObjectManager::object-added signals are synthesized
 * while #GDBusObjectManagerClient:name-owner is still %NULL. Only when all
 * object proxies have been added, the #GDBusObjectManagerClient:name-owner
 * is set to the new name owner (this includes emission of the
 * #GObject::notify signal).  Furthermore, you are guaranteed that
 * #GDBusObjectManagerClient:name-owner will alternate between a name owner
 * (e.g. `:1.42`) and %NULL even in the case where
 * the name of interest is atomically replaced
 *
 * Ultimately, #GDBusObjectManagerClient is used to obtain #GDBusProxy
 * instances. All signals (including the
 * org.freedesktop.DBus.Properties::PropertiesChanged signal)
 * delivered to #GDBusProxy instances are guaranteed to originate
 * from the name owner. This guarantee along with the behavior
 * described above, means that certain race conditions including the
 * "half the proxy is from the old owner and the other half is from
 * the new owner" problem cannot happen.
 *
 * To avoid having the application connect to signals on the returned
 * #GDBusObjectProxy and #GDBusProxy objects, the
 * #GDBusObject::interface-added,
 * #GDBusObject::interface-removed,
 * #GDBusProxy::g-properties-changed and
 * #GDBusProxy::g-signal signals
 * are also emitted on the #GDBusObjectManagerClient instance managing these
 * objects. The signals emitted are
 * #GDBusObjectManager::interface-added,
 * #GDBusObjectManager::interface-removed,
 * #GDBusObjectManagerClient::interface-proxy-properties-changed and
 * #GDBusObjectManagerClient::interface-proxy-signal.
 *
 * Note that all callbacks and signals are emitted in the
 * [thread-default main context][g-main-context-push-thread-default]
 * that the #GDBusObjectManagerClient object was constructed
 * in. Additionally, the #GDBusObjectProxy and #GDBusProxy objects
 * originating from the #GDBusObjectManagerClient object will be created in
 * the same context and, consequently, will deliver signals in the
 * same main loop.
 */

struct _GDBusObjectManagerClientPrivate
{
  GMutex lock;

  GBusType bus_type;
  GDBusConnection *connection;
  gchar *object_path;
  gchar *name;
  gchar *name_owner;
  GDBusObjectManagerClientFlags flags;

  GDBusProxy *control_proxy;

  GHashTable *map_object_path_to_object_proxy;

  guint signal_subscription_id;
  gchar *match_rule;

  GDBusProxyTypeFunc get_proxy_type_func;
  gpointer get_proxy_type_user_data;
  GDestroyNotify get_proxy_type_destroy_notify;
};

enum
{
  PROP_0,
  PROP_BUS_TYPE,
  PROP_CONNECTION,
  PROP_FLAGS,
  PROP_OBJECT_PATH,
  PROP_NAME,
  PROP_NAME_OWNER,
  PROP_GET_PROXY_TYPE_FUNC,
  PROP_GET_PROXY_TYPE_USER_DATA,
  PROP_GET_PROXY_TYPE_DESTROY_NOTIFY
};

enum
{
  INTERFACE_PROXY_SIGNAL_SIGNAL,
  INTERFACE_PROXY_PROPERTIES_CHANGED_SIGNAL,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static void initable_iface_init       (GInitableIface *initable_iface);
static void async_initable_iface_init (GAsyncInitableIface *async_initable_iface);
static void dbus_object_manager_interface_init (GDBusObjectManagerIface *iface);

G_DEFINE_TYPE_WITH_CODE (GDBusObjectManagerClient, g_dbus_object_manager_client, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (GDBusObjectManagerClient)
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, initable_iface_init)
                         G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE, async_initable_iface_init)
                         G_IMPLEMENT_INTERFACE (G_TYPE_DBUS_OBJECT_MANAGER, dbus_object_manager_interface_init))

static void maybe_unsubscribe_signals (GDBusObjectManagerClient *manager);

static void on_control_proxy_g_signal (GDBusProxy   *proxy,
                                       const gchar  *sender_name,
                                       const gchar  *signal_name,
                                       GVariant     *parameters,
                                       gpointer      user_data);

static void process_get_all_result (GDBusObjectManagerClient *manager,
                                    GVariant          *value,
                                    const gchar       *name_owner);

static void
g_dbus_object_manager_client_finalize (GObject *object)
{
  GDBusObjectManagerClient *manager = G_DBUS_OBJECT_MANAGER_CLIENT (object);

  maybe_unsubscribe_signals (manager);

  g_hash_table_unref (manager->priv->map_object_path_to_object_proxy);

  if (manager->priv->control_proxy != NULL)
    {
      g_signal_handlers_disconnect_by_func (manager->priv->control_proxy,
                                            on_control_proxy_g_signal,
                                            manager);
      g_object_unref (manager->priv->control_proxy);
    }
  if (manager->priv->connection != NULL)
    g_object_unref (manager->priv->connection);
  g_free (manager->priv->object_path);
  g_free (manager->priv->name);
  g_free (manager->priv->name_owner);

  if (manager->priv->get_proxy_type_destroy_notify != NULL)
    manager->priv->get_proxy_type_destroy_notify (manager->priv->get_proxy_type_user_data);

  g_mutex_clear (&manager->priv->lock);

  if (G_OBJECT_CLASS (g_dbus_object_manager_client_parent_class)->finalize != NULL)
    G_OBJECT_CLASS (g_dbus_object_manager_client_parent_class)->finalize (object);
}

static void
g_dbus_object_manager_client_get_property (GObject    *_object,
                                           guint       prop_id,
                                           GValue     *value,
                                           GParamSpec *pspec)
{
  GDBusObjectManagerClient *manager = G_DBUS_OBJECT_MANAGER_CLIENT (_object);

  switch (prop_id)
    {
    case PROP_CONNECTION:
      g_value_set_object (value, g_dbus_object_manager_client_get_connection (manager));
      break;

    case PROP_OBJECT_PATH:
      g_value_set_string (value, g_dbus_object_manager_get_object_path (G_DBUS_OBJECT_MANAGER (manager)));
      break;

    case PROP_NAME:
      g_value_set_string (value, g_dbus_object_manager_client_get_name (manager));
      break;

    case PROP_FLAGS:
      g_value_set_flags (value, g_dbus_object_manager_client_get_flags (manager));
      break;

    case PROP_NAME_OWNER:
      g_value_take_string (value, g_dbus_object_manager_client_get_name_owner (manager));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (manager, prop_id, pspec);
      break;
    }
}

static void
g_dbus_object_manager_client_set_property (GObject       *_object,
                                           guint          prop_id,
                                           const GValue  *value,
                                           GParamSpec    *pspec)
{
  GDBusObjectManagerClient *manager = G_DBUS_OBJECT_MANAGER_CLIENT (_object);
  const gchar *name;

  switch (prop_id)
    {
    case PROP_BUS_TYPE:
      manager->priv->bus_type = g_value_get_enum (value);
      break;

    case PROP_CONNECTION:
      if (g_value_get_object (value) != NULL)
        {
          g_assert (manager->priv->connection == NULL);
          g_assert (G_IS_DBUS_CONNECTION (g_value_get_object (value)));
          manager->priv->connection = g_value_dup_object (value);
        }
      break;

    case PROP_OBJECT_PATH:
      g_assert (manager->priv->object_path == NULL);
      g_assert (g_variant_is_object_path (g_value_get_string (value)));
      manager->priv->object_path = g_value_dup_string (value);
      break;

    case PROP_NAME:
      g_assert (manager->priv->name == NULL);
      name = g_value_get_string (value);
      g_assert (name == NULL || g_dbus_is_name (name));
      manager->priv->name = g_strdup (name);
      break;

    case PROP_FLAGS:
      manager->priv->flags = g_value_get_flags (value);
      break;

    case PROP_GET_PROXY_TYPE_FUNC:
      manager->priv->get_proxy_type_func = g_value_get_pointer (value);
      break;

    case PROP_GET_PROXY_TYPE_USER_DATA:
      manager->priv->get_proxy_type_user_data = g_value_get_pointer (value);
      break;

    case PROP_GET_PROXY_TYPE_DESTROY_NOTIFY:
      manager->priv->get_proxy_type_destroy_notify = g_value_get_pointer (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (manager, prop_id, pspec);
      break;
    }
}

static void
g_dbus_object_manager_client_class_init (GDBusObjectManagerClientClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize     = g_dbus_object_manager_client_finalize;
  gobject_class->set_property = g_dbus_object_manager_client_set_property;
  gobject_class->get_property = g_dbus_object_manager_client_get_property;

  /**
   * GDBusObjectManagerClient:connection:
   *
   * The #GDBusConnection to use.
   *
   * Since: 2.30
   */
  g_object_class_install_property (gobject_class,
                                   PROP_CONNECTION,
                                   g_param_spec_object ("connection",
                                                        "Connection",
                                                        "The connection to use",
                                                        G_TYPE_DBUS_CONNECTION,
                                                        G_PARAM_READABLE |
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));

  /**
   * GDBusObjectManagerClient:bus-type:
   *
   * If this property is not %G_BUS_TYPE_NONE, then
   * #GDBusObjectManagerClient:connection must be %NULL and will be set to the
   * #GDBusConnection obtained by calling g_bus_get() with the value
   * of this property.
   *
   * Since: 2.30
   */
  g_object_class_install_property (gobject_class,
                                   PROP_BUS_TYPE,
                                   g_param_spec_enum ("bus-type",
                                                      "Bus Type",
                                                      "The bus to connect to, if any",
                                                      G_TYPE_BUS_TYPE,
                                                      G_BUS_TYPE_NONE,
                                                      G_PARAM_WRITABLE |
                                                      G_PARAM_CONSTRUCT_ONLY |
                                                      G_PARAM_STATIC_NAME |
                                                      G_PARAM_STATIC_BLURB |
                                                      G_PARAM_STATIC_NICK));

  /**
   * GDBusObjectManagerClient:flags:
   *
   * Flags from the #GDBusObjectManagerClientFlags enumeration.
   *
   * Since: 2.30
   */
  g_object_class_install_property (gobject_class,
                                   PROP_FLAGS,
                                   g_param_spec_flags ("flags",
                                                       "Flags",
                                                       "Flags for the proxy manager",
                                                       G_TYPE_DBUS_OBJECT_MANAGER_CLIENT_FLAGS,
                                                       G_DBUS_OBJECT_MANAGER_CLIENT_FLAGS_NONE,
                                                       G_PARAM_READABLE |
                                                       G_PARAM_WRITABLE |
                                                       G_PARAM_CONSTRUCT_ONLY |
                                                       G_PARAM_STATIC_NAME |
                                                       G_PARAM_STATIC_BLURB |
                                                       G_PARAM_STATIC_NICK));

  /**
   * GDBusObjectManagerClient:object-path:
   *
   * The object path the manager is for.
   *
   * Since: 2.30
   */
  g_object_class_install_property (gobject_class,
                                   PROP_OBJECT_PATH,
                                   g_param_spec_string ("object-path",
                                                        "Object Path",
                                                        "The object path of the control object",
                                                        NULL,
                                                        G_PARAM_READABLE |
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));

  /**
   * GDBusObjectManagerClient:name:
   *
   * The well-known name or unique name that the manager is for.
   *
   * Since: 2.30
   */
  g_object_class_install_property (gobject_class,
                                   PROP_NAME,
                                   g_param_spec_string ("name",
                                                        "Name",
                                                        "Name that the manager is for",
                                                        NULL,
                                                        G_PARAM_READABLE |
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));

  /**
   * GDBusObjectManagerClient:name-owner:
   *
   * The unique name that owns #GDBusObjectManagerClient:name or %NULL if
   * no-one is currently owning the name. Connect to the
   * #GObject::notify signal to track changes to this property.
   *
   * Since: 2.30
   */
  g_object_class_install_property (gobject_class,
                                   PROP_NAME_OWNER,
                                   g_param_spec_string ("name-owner",
                                                        "Name Owner",
                                                        "The owner of the name we are watching",
                                                        NULL,
                                                        G_PARAM_READABLE |
                                                        G_PARAM_STATIC_STRINGS));

  /**
   * GDBusObjectManagerClient:get-proxy-type-func:
   *
   * The #GDBusProxyTypeFunc to use when determining what #GType to
   * use for interface proxies or %NULL.
   *
   * Since: 2.30
   */
  g_object_class_install_property (gobject_class,
                                   PROP_GET_PROXY_TYPE_FUNC,
                                   g_param_spec_pointer ("get-proxy-type-func",
                                                         "GDBusProxyTypeFunc Function Pointer",
                                                         "The GDBusProxyTypeFunc pointer to use",
                                                         G_PARAM_READABLE |
                                                         G_PARAM_WRITABLE |
                                                         G_PARAM_CONSTRUCT_ONLY |
                                                         G_PARAM_STATIC_STRINGS));

  /**
   * GDBusObjectManagerClient:get-proxy-type-user-data:
   *
   * The #gpointer user_data to pass to #GDBusObjectManagerClient:get-proxy-type-func.
   *
   * Since: 2.30
   */
  g_object_class_install_property (gobject_class,
                                   PROP_GET_PROXY_TYPE_USER_DATA,
                                   g_param_spec_pointer ("get-proxy-type-user-data",
                                                         "GDBusProxyTypeFunc User Data",
                                                         "The GDBusProxyTypeFunc user_data",
                                                         G_PARAM_READABLE |
                                                         G_PARAM_WRITABLE |
                                                         G_PARAM_CONSTRUCT_ONLY |
                                                         G_PARAM_STATIC_STRINGS));

  /**
   * GDBusObjectManagerClient:get-proxy-type-destroy-notify:
   *
   * A #GDestroyNotify for the #gpointer user_data in #GDBusObjectManagerClient:get-proxy-type-user-data.
   *
   * Since: 2.30
   */
  g_object_class_install_property (gobject_class,
                                   PROP_GET_PROXY_TYPE_DESTROY_NOTIFY,
                                   g_param_spec_pointer ("get-proxy-type-destroy-notify",
                                                         "GDBusProxyTypeFunc user data free function",
                                                         "The GDBusProxyTypeFunc user data free function",
                                                         G_PARAM_READABLE |
                                                         G_PARAM_WRITABLE |
                                                         G_PARAM_CONSTRUCT_ONLY |
                                                         G_PARAM_STATIC_STRINGS));

  /**
   * GDBusObjectManagerClient::interface-proxy-signal:
   * @manager: The #GDBusObjectManagerClient emitting the signal.
   * @object_proxy: The #GDBusObjectProxy on which an interface is emitting a D-Bus signal.
   * @interface_proxy: The #GDBusProxy that is emitting a D-Bus signal.
   * @sender_name: The sender of the signal or NULL if the connection is not a bus connection.
   * @signal_name: The signal name.
   * @parameters: A #GVariant tuple with parameters for the signal.
   *
   * Emitted when a D-Bus signal is received on @interface_proxy.
   *
   * This signal exists purely as a convenience to avoid having to
   * connect signals to all interface proxies managed by @manager.
   *
   * This signal is emitted in the
   * [thread-default main context][g-main-context-push-thread-default]
   * that @manager was constructed in.
   *
   * Since: 2.30
   */
  signals[INTERFACE_PROXY_SIGNAL_SIGNAL] =
    g_signal_new (I_("interface-proxy-signal"),
                  G_TYPE_DBUS_OBJECT_MANAGER_CLIENT,
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GDBusObjectManagerClientClass, interface_proxy_signal),
                  NULL,
                  NULL,
                  NULL,
                  G_TYPE_NONE,
                  5,
                  G_TYPE_DBUS_OBJECT_PROXY,
                  G_TYPE_DBUS_PROXY,
                  G_TYPE_STRING,
                  G_TYPE_STRING,
                  G_TYPE_VARIANT);

  /**
   * GDBusObjectManagerClient::interface-proxy-properties-changed:
   * @manager: The #GDBusObjectManagerClient emitting the signal.
   * @object_proxy: The #GDBusObjectProxy on which an interface has properties that are changing.
   * @interface_proxy: The #GDBusProxy that has properties that are changing.
   * @changed_properties: A #GVariant containing the properties that changed.
   * @invalidated_properties: (array zero-terminated=1) (element-type utf8): A %NULL terminated
   *   array of properties that were invalidated.
   *
   * Emitted when one or more D-Bus properties on proxy changes. The
   * local cache has already been updated when this signal fires. Note
   * that both @changed_properties and @invalidated_properties are
   * guaranteed to never be %NULL (either may be empty though).
   *
   * This signal exists purely as a convenience to avoid having to
   * connect signals to all interface proxies managed by @manager.
   *
   * This signal is emitted in the
   * [thread-default main context][g-main-context-push-thread-default]
   * that @manager was constructed in.
   *
   * Since: 2.30
   */
  signals[INTERFACE_PROXY_PROPERTIES_CHANGED_SIGNAL] =
    g_signal_new (I_("interface-proxy-properties-changed"),
                  G_TYPE_DBUS_OBJECT_MANAGER_CLIENT,
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GDBusObjectManagerClientClass, interface_proxy_properties_changed),
                  NULL,
                  NULL,
                  NULL,
                  G_TYPE_NONE,
                  4,
                  G_TYPE_DBUS_OBJECT_PROXY,
                  G_TYPE_DBUS_PROXY,
                  G_TYPE_VARIANT,
                  G_TYPE_STRV);
}

static void
g_dbus_object_manager_client_init (GDBusObjectManagerClient *manager)
{
  manager->priv = g_dbus_object_manager_client_get_instance_private (manager);
  g_mutex_init (&manager->priv->lock);
  manager->priv->map_object_path_to_object_proxy = g_hash_table_new_full (g_str_hash,
                                                                          g_str_equal,
                                                                          g_free,
                                                                          (GDestroyNotify) g_object_unref);
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_dbus_object_manager_client_new_sync:
 * @connection: A #GDBusConnection.
 * @flags: Zero or more flags from the #GDBusObjectManagerClientFlags enumeration.
 * @name: (nullable): The owner of the control object (unique or well-known name), or %NULL when not using a message bus connection.
 * @object_path: The object path of the control object.
 * @get_proxy_type_func: (nullable): A #GDBusProxyTypeFunc function or %NULL to always construct #GDBusProxy proxies.
 * @get_proxy_type_user_data: User data to pass to @get_proxy_type_func.
 * @get_proxy_type_destroy_notify: (nullable): Free function for @get_proxy_type_user_data or %NULL.
 * @cancellable: (nullable): A #GCancellable or %NULL
 * @error: Return location for error or %NULL.
 *
 * Creates a new #GDBusObjectManagerClient object.
 *
 * This is a synchronous failable constructor - the calling thread is
 * blocked until a reply is received. See g_dbus_object_manager_client_new()
 * for the asynchronous version.
 *
 * Returns: (transfer full) (type GDBusObjectManagerClient): A
 *   #GDBusObjectManagerClient object or %NULL if @error is set. Free
 *   with g_object_unref().
 *
 * Since: 2.30
 */
GDBusObjectManager *
g_dbus_object_manager_client_new_sync (GDBusConnection               *connection,
                                       GDBusObjectManagerClientFlags  flags,
                                       const gchar                   *name,
                                       const gchar                   *object_path,
                                       GDBusProxyTypeFunc             get_proxy_type_func,
                                       gpointer                       get_proxy_type_user_data,
                                       GDestroyNotify                 get_proxy_type_destroy_notify,
                                       GCancellable                  *cancellable,
                                       GError                       **error)
{
  GInitable *initable;

  g_return_val_if_fail (G_IS_DBUS_CONNECTION (connection), NULL);
  g_return_val_if_fail ((name == NULL && g_dbus_connection_get_unique_name (connection) == NULL) ||
                        g_dbus_is_name (name), NULL);
  g_return_val_if_fail (g_variant_is_object_path (object_path), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  initable = g_initable_new (G_TYPE_DBUS_OBJECT_MANAGER_CLIENT,
                             cancellable,
                             error,
                             "connection", connection,
                             "flags", flags,
                             "name", name,
                             "object-path", object_path,
                             "get-proxy-type-func", get_proxy_type_func,
                             "get-proxy-type-user-data", get_proxy_type_user_data,
                             "get-proxy-type-destroy-notify", get_proxy_type_destroy_notify,
                             NULL);
  if (initable != NULL)
    return G_DBUS_OBJECT_MANAGER (initable);
  else
    return NULL;
}

/**
 * g_dbus_object_manager_client_new:
 * @connection: A #GDBusConnection.
 * @flags: Zero or more flags from the #GDBusObjectManagerClientFlags enumeration.
 * @name: The owner of the control object (unique or well-known name).
 * @object_path: The object path of the control object.
 * @get_proxy_type_func: (nullable): A #GDBusProxyTypeFunc function or %NULL to always construct #GDBusProxy proxies.
 * @get_proxy_type_user_data: User data to pass to @get_proxy_type_func.
 * @get_proxy_type_destroy_notify: (nullable): Free function for @get_proxy_type_user_data or %NULL.
 * @cancellable: (nullable): A #GCancellable or %NULL
 * @callback: A #GAsyncReadyCallback to call when the request is satisfied.
 * @user_data: The data to pass to @callback.
 *
 * Asynchronously creates a new #GDBusObjectManagerClient object.
 *
 * This is an asynchronous failable constructor. When the result is
 * ready, @callback will be invoked in the
 * [thread-default main context][g-main-context-push-thread-default]
 * of the thread you are calling this method from. You can
 * then call g_dbus_object_manager_client_new_finish() to get the result. See
 * g_dbus_object_manager_client_new_sync() for the synchronous version.
 *
 * Since: 2.30
 */
void
g_dbus_object_manager_client_new (GDBusConnection               *connection,
                                  GDBusObjectManagerClientFlags  flags,
                                  const gchar                   *name,
                                  const gchar                   *object_path,
                                  GDBusProxyTypeFunc             get_proxy_type_func,
                                  gpointer                       get_proxy_type_user_data,
                                  GDestroyNotify                 get_proxy_type_destroy_notify,
                                  GCancellable                  *cancellable,
                                  GAsyncReadyCallback            callback,
                                  gpointer                       user_data)
{
  g_return_if_fail (G_IS_DBUS_CONNECTION (connection));
  g_return_if_fail ((name == NULL && g_dbus_connection_get_unique_name (connection) == NULL) ||
                        g_dbus_is_name (name));
  g_return_if_fail (g_variant_is_object_path (object_path));

  g_async_initable_new_async (G_TYPE_DBUS_OBJECT_MANAGER_CLIENT,
                              G_PRIORITY_DEFAULT,
                              cancellable,
                              callback,
                              user_data,
                              "connection", connection,
                              "flags", flags,
                              "name", name,
                              "object-path", object_path,
                              "get-proxy-type-func", get_proxy_type_func,
                              "get-proxy-type-user-data", get_proxy_type_user_data,
                              "get-proxy-type-destroy-notify", get_proxy_type_destroy_notify,
                              NULL);
}

/**
 * g_dbus_object_manager_client_new_finish:
 * @res: A #GAsyncResult obtained from the #GAsyncReadyCallback passed to g_dbus_object_manager_client_new().
 * @error: Return location for error or %NULL.
 *
 * Finishes an operation started with g_dbus_object_manager_client_new().
 *
 * Returns: (transfer full) (type GDBusObjectManagerClient): A
 *   #GDBusObjectManagerClient object or %NULL if @error is set. Free
 *   with g_object_unref().
 *
 * Since: 2.30
 */
GDBusObjectManager *
g_dbus_object_manager_client_new_finish (GAsyncResult   *res,
                                         GError        **error)
{
  GObject *object;
  GObject *source_object;

  source_object = g_async_result_get_source_object (res);
  g_assert (source_object != NULL);

  object = g_async_initable_new_finish (G_ASYNC_INITABLE (source_object),
                                        res,
                                        error);
  g_object_unref (source_object);

  if (object != NULL)
    return G_DBUS_OBJECT_MANAGER (object);
  else
    return NULL;
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_dbus_object_manager_client_new_for_bus_sync:
 * @bus_type: A #GBusType.
 * @flags: Zero or more flags from the #GDBusObjectManagerClientFlags enumeration.
 * @name: The owner of the control object (unique or well-known name).
 * @object_path: The object path of the control object.
 * @get_proxy_type_func: (nullable): A #GDBusProxyTypeFunc function or %NULL to always construct #GDBusProxy proxies.
 * @get_proxy_type_user_data: User data to pass to @get_proxy_type_func.
 * @get_proxy_type_destroy_notify: (nullable): Free function for @get_proxy_type_user_data or %NULL.
 * @cancellable: (nullable): A #GCancellable or %NULL
 * @error: Return location for error or %NULL.
 *
 * Like g_dbus_object_manager_client_new_sync() but takes a #GBusType instead
 * of a #GDBusConnection.
 *
 * This is a synchronous failable constructor - the calling thread is
 * blocked until a reply is received. See g_dbus_object_manager_client_new_for_bus()
 * for the asynchronous version.
 *
 * Returns: (transfer full) (type GDBusObjectManagerClient): A
 *   #GDBusObjectManagerClient object or %NULL if @error is set. Free
 *   with g_object_unref().
 *
 * Since: 2.30
 */
GDBusObjectManager *
g_dbus_object_manager_client_new_for_bus_sync (GBusType                       bus_type,
                                               GDBusObjectManagerClientFlags  flags,
                                               const gchar                   *name,
                                               const gchar                   *object_path,
                                               GDBusProxyTypeFunc             get_proxy_type_func,
                                               gpointer                       get_proxy_type_user_data,
                                               GDestroyNotify                 get_proxy_type_destroy_notify,
                                               GCancellable                  *cancellable,
                                               GError                       **error)
{
  GInitable *initable;

  g_return_val_if_fail (bus_type != G_BUS_TYPE_NONE, NULL);
  g_return_val_if_fail (g_dbus_is_name (name), NULL);
  g_return_val_if_fail (g_variant_is_object_path (object_path), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  initable = g_initable_new (G_TYPE_DBUS_OBJECT_MANAGER_CLIENT,
                             cancellable,
                             error,
                             "bus-type", bus_type,
                             "flags", flags,
                             "name", name,
                             "object-path", object_path,
                             "get-proxy-type-func", get_proxy_type_func,
                             "get-proxy-type-user-data", get_proxy_type_user_data,
                             "get-proxy-type-destroy-notify", get_proxy_type_destroy_notify,
                             NULL);
  if (initable != NULL)
    return G_DBUS_OBJECT_MANAGER (initable);
  else
    return NULL;
}

/**
 * g_dbus_object_manager_client_new_for_bus:
 * @bus_type: A #GBusType.
 * @flags: Zero or more flags from the #GDBusObjectManagerClientFlags enumeration.
 * @name: The owner of the control object (unique or well-known name).
 * @object_path: The object path of the control object.
 * @get_proxy_type_func: (nullable): A #GDBusProxyTypeFunc function or %NULL to always construct #GDBusProxy proxies.
 * @get_proxy_type_user_data: User data to pass to @get_proxy_type_func.
 * @get_proxy_type_destroy_notify: (nullable): Free function for @get_proxy_type_user_data or %NULL.
 * @cancellable: (nullable): A #GCancellable or %NULL
 * @callback: A #GAsyncReadyCallback to call when the request is satisfied.
 * @user_data: The data to pass to @callback.
 *
 * Like g_dbus_object_manager_client_new() but takes a #GBusType instead of a
 * #GDBusConnection.
 *
 * This is an asynchronous failable constructor. When the result is
 * ready, @callback will be invoked in the
 * [thread-default main loop][g-main-context-push-thread-default]
 * of the thread you are calling this method from. You can
 * then call g_dbus_object_manager_client_new_for_bus_finish() to get the result. See
 * g_dbus_object_manager_client_new_for_bus_sync() for the synchronous version.
 *
 * Since: 2.30
 */
void
g_dbus_object_manager_client_new_for_bus (GBusType                       bus_type,
                                          GDBusObjectManagerClientFlags  flags,
                                          const gchar                   *name,
                                          const gchar                   *object_path,
                                          GDBusProxyTypeFunc             get_proxy_type_func,
                                          gpointer                       get_proxy_type_user_data,
                                          GDestroyNotify                 get_proxy_type_destroy_notify,
                                          GCancellable                  *cancellable,
                                          GAsyncReadyCallback            callback,
                                          gpointer                       user_data)
{
  g_return_if_fail (bus_type != G_BUS_TYPE_NONE);
  g_return_if_fail (g_dbus_is_name (name));
  g_return_if_fail (g_variant_is_object_path (object_path));

  g_async_initable_new_async (G_TYPE_DBUS_OBJECT_MANAGER_CLIENT,
                              G_PRIORITY_DEFAULT,
                              cancellable,
                              callback,
                              user_data,
                              "bus-type", bus_type,
                              "flags", flags,
                              "name", name,
                              "object-path", object_path,
                              "get-proxy-type-func", get_proxy_type_func,
                              "get-proxy-type-user-data", get_proxy_type_user_data,
                              "get-proxy-type-destroy-notify", get_proxy_type_destroy_notify,
                              NULL);
}

/**
 * g_dbus_object_manager_client_new_for_bus_finish:
 * @res: A #GAsyncResult obtained from the #GAsyncReadyCallback passed to g_dbus_object_manager_client_new_for_bus().
 * @error: Return location for error or %NULL.
 *
 * Finishes an operation started with g_dbus_object_manager_client_new_for_bus().
 *
 * Returns: (transfer full) (type GDBusObjectManagerClient): A
 *   #GDBusObjectManagerClient object or %NULL if @error is set. Free
 *   with g_object_unref().
 *
 * Since: 2.30
 */
GDBusObjectManager *
g_dbus_object_manager_client_new_for_bus_finish (GAsyncResult   *res,
                                                 GError        **error)
{
  GObject *object;
  GObject *source_object;

  source_object = g_async_result_get_source_object (res);
  g_assert (source_object != NULL);

  object = g_async_initable_new_finish (G_ASYNC_INITABLE (source_object),
                                        res,
                                        error);
  g_object_unref (source_object);

  if (object != NULL)
    return G_DBUS_OBJECT_MANAGER (object);
  else
    return NULL;
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_dbus_object_manager_client_get_connection:
 * @manager: A #GDBusObjectManagerClient
 *
 * Gets the #GDBusConnection used by @manager.
 *
 * Returns: (transfer none): A #GDBusConnection object. Do not free,
 *   the object belongs to @manager.
 *
 * Since: 2.30
 */
GDBusConnection *
g_dbus_object_manager_client_get_connection (GDBusObjectManagerClient *manager)
{
  GDBusConnection *ret;
  g_return_val_if_fail (G_IS_DBUS_OBJECT_MANAGER_CLIENT (manager), NULL);
  g_mutex_lock (&manager->priv->lock);
  ret = manager->priv->connection;
  g_mutex_unlock (&manager->priv->lock);
  return ret;
}

/**
 * g_dbus_object_manager_client_get_name:
 * @manager: A #GDBusObjectManagerClient
 *
 * Gets the name that @manager is for, or %NULL if not a message bus
 * connection.
 *
 * Returns: A unique or well-known name. Do not free, the string
 * belongs to @manager.
 *
 * Since: 2.30
 */
const gchar *
g_dbus_object_manager_client_get_name (GDBusObjectManagerClient *manager)
{
  const gchar *ret;
  g_return_val_if_fail (G_IS_DBUS_OBJECT_MANAGER_CLIENT (manager), NULL);
  g_mutex_lock (&manager->priv->lock);
  ret = manager->priv->name;
  g_mutex_unlock (&manager->priv->lock);
  return ret;
}

/**
 * g_dbus_object_manager_client_get_flags:
 * @manager: A #GDBusObjectManagerClient
 *
 * Gets the flags that @manager was constructed with.
 *
 * Returns: Zero of more flags from the #GDBusObjectManagerClientFlags
 * enumeration.
 *
 * Since: 2.30
 */
GDBusObjectManagerClientFlags
g_dbus_object_manager_client_get_flags (GDBusObjectManagerClient *manager)
{
  GDBusObjectManagerClientFlags ret;
  g_return_val_if_fail (G_IS_DBUS_OBJECT_MANAGER_CLIENT (manager), G_DBUS_OBJECT_MANAGER_CLIENT_FLAGS_NONE);
  g_mutex_lock (&manager->priv->lock);
  ret = manager->priv->flags;
  g_mutex_unlock (&manager->priv->lock);
  return ret;
}

/**
 * g_dbus_object_manager_client_get_name_owner:
 * @manager: A #GDBusObjectManagerClient.
 *
 * The unique name that owns the name that @manager is for or %NULL if
 * no-one currently owns that name. You can connect to the
 * #GObject::notify signal to track changes to the
 * #GDBusObjectManagerClient:name-owner property.
 *
 * Returns: (nullable): The name owner or %NULL if no name owner
 * exists. Free with g_free().
 *
 * Since: 2.30
 */
gchar *
g_dbus_object_manager_client_get_name_owner (GDBusObjectManagerClient *manager)
{
  gchar *ret;
  g_return_val_if_fail (G_IS_DBUS_OBJECT_MANAGER_CLIENT (manager), NULL);
  g_mutex_lock (&manager->priv->lock);
  ret = g_strdup (manager->priv->name_owner);
  g_mutex_unlock (&manager->priv->lock);
  return ret;
}

/* ---------------------------------------------------------------------------------------------------- */

/* signal handler for all objects we manage - we dispatch signals
 * from here to the objects
 */
static void
signal_cb (GDBusConnection *connection,
           const gchar     *sender_name,
           const gchar     *object_path,
           const gchar     *interface_name,
           const gchar     *signal_name,
           GVariant        *parameters,
           gpointer         user_data)
{
  GDBusObjectManagerClient *manager = G_DBUS_OBJECT_MANAGER_CLIENT (user_data);
  GDBusObjectProxy *object_proxy;
  GDBusInterface *interface;

  g_mutex_lock (&manager->priv->lock);
  object_proxy = g_hash_table_lookup (manager->priv->map_object_path_to_object_proxy, object_path);
  if (object_proxy == NULL)
    {
      g_mutex_unlock (&manager->priv->lock);
      goto out;
    }
  g_object_ref (object_proxy);
  g_mutex_unlock (&manager->priv->lock);

  //g_debug ("yay, signal_cb %s %s: %s\n", signal_name, object_path, g_variant_print (parameters, TRUE));

  g_object_ref (manager);
  if (g_strcmp0 (interface_name, "org.freedesktop.DBus.Properties") == 0)
    {
      if (g_strcmp0 (signal_name, "PropertiesChanged") == 0)
        {
          const gchar *interface_name;
          GVariant *changed_properties;
          const gchar **invalidated_properties;

          g_variant_get (parameters,
                         "(&s@a{sv}^a&s)",
                         &interface_name,
                         &changed_properties,
                         &invalidated_properties);

          interface = g_dbus_object_get_interface (G_DBUS_OBJECT (object_proxy), interface_name);
          if (interface != NULL)
            {
              GVariantIter property_iter;
              const gchar *property_name;
              GVariant *property_value;
              guint n;

              /* update caches... */
              g_variant_iter_init (&property_iter, changed_properties);
              while (g_variant_iter_next (&property_iter,
                                          "{&sv}",
                                          &property_name,
                                          &property_value))
                {
                  g_dbus_proxy_set_cached_property (G_DBUS_PROXY (interface),
                                                    property_name,
                                                    property_value);
                  g_variant_unref (property_value);
                }

              for (n = 0; invalidated_properties[n] != NULL; n++)
                {
                  g_dbus_proxy_set_cached_property (G_DBUS_PROXY (interface),
                                                    invalidated_properties[n],
                                                    NULL);
                }
              /* ... and then synthesize the signal */
              g_signal_emit_by_name (interface,
                                     "g-properties-changed",
                                     changed_properties,
                                     invalidated_properties);
              g_signal_emit (manager,
                             signals[INTERFACE_PROXY_PROPERTIES_CHANGED_SIGNAL],
                             0,
                             object_proxy,
                             interface,
                             changed_properties,
                             invalidated_properties);
              g_object_unref (interface);
            }
          g_variant_unref (changed_properties);
          g_free (invalidated_properties);
        }
    }
  else
    {
      /* regular signal - just dispatch it */
      interface = g_dbus_object_get_interface (G_DBUS_OBJECT (object_proxy), interface_name);
      if (interface != NULL)
        {
          g_signal_emit_by_name (interface,
                                 "g-signal",
                                 sender_name,
                                 signal_name,
                                 parameters);
          g_signal_emit (manager,
                         signals[INTERFACE_PROXY_SIGNAL_SIGNAL],
                         0,
                         object_proxy,
                         interface,
                         sender_name,
                         signal_name,
                         parameters);
          g_object_unref (interface);
        }
    }
  g_object_unref (manager);

 out:
  g_clear_object (&object_proxy);
}

static void
subscribe_signals (GDBusObjectManagerClient *manager,
                   const gchar *name_owner)
{
  GError *error = NULL;
  GVariant *ret;

  g_return_if_fail (G_IS_DBUS_OBJECT_MANAGER_CLIENT (manager));
  g_return_if_fail (manager->priv->signal_subscription_id == 0);
  g_return_if_fail (name_owner == NULL || g_dbus_is_unique_name (name_owner));

  if (name_owner != NULL)
    {
      /* Only add path_namespace if it's non-'/'. This removes a no-op key from
       * the match rule, and also works around a D-Bus bug where
       * path_namespace='/' matches nothing in D-Bus versions < 1.6.18.
       *
       * See: https://bugs.freedesktop.org/show_bug.cgi?id=70799 */
      if (g_str_equal (manager->priv->object_path, "/"))
        {
          manager->priv->match_rule = g_strdup_printf ("type='signal',sender='%s'",
                                                       name_owner);
        }
      else
        {
          manager->priv->match_rule = g_strdup_printf ("type='signal',sender='%s',path_namespace='%s'",
                                                       name_owner, manager->priv->object_path);
        }

      /* The bus daemon may not implement path_namespace so gracefully
       * handle this by using a fallback triggered if @error is set. */
      ret = g_dbus_connection_call_sync (manager->priv->connection,
                                         "org.freedesktop.DBus",
                                         "/org/freedesktop/DBus",
                                         "org.freedesktop.DBus",
                                         "AddMatch",
                                         g_variant_new ("(s)",
                                                        manager->priv->match_rule),
                                         NULL, /* reply_type */
                                         G_DBUS_CALL_FLAGS_NONE,
                                         -1, /* default timeout */
                                         NULL, /* TODO: Cancellable */
                                         &error);

      /* yay, bus daemon supports path_namespace */
      if (ret != NULL)
        g_variant_unref (ret);
    }

  if (error == NULL)
    {
      /* still need to ask GDBusConnection for the callbacks */
      manager->priv->signal_subscription_id =
        g_dbus_connection_signal_subscribe (manager->priv->connection,
                                            name_owner,
                                            NULL, /* interface */
                                            NULL, /* member */
                                            NULL, /* path - TODO: really want wilcard support here */
                                            NULL, /* arg0 */
                                            G_DBUS_SIGNAL_FLAGS_NONE |
                                            G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE,
                                            signal_cb,
                                            manager,
                                            NULL); /* user_data_free_func */

    }
  else
    {
      /* TODO: we could report this to the user
      g_warning ("Message bus daemon does not support path_namespace: %s (%s %d)",
                 error->message,
                 g_quark_to_string (error->domain),
                 error->code);
      */

      g_error_free (error);

      /* no need to call RemoveMatch when done since it didn't work */
      g_free (manager->priv->match_rule);
      manager->priv->match_rule = NULL;

      /* Fallback is to subscribe to *all* signals from the name owner which
       * is rather wasteful. It's probably not a big practical problem because
       * users typically want all objects that the name owner supplies.
       */
      manager->priv->signal_subscription_id =
        g_dbus_connection_signal_subscribe (manager->priv->connection,
                                            name_owner,
                                            NULL, /* interface */
                                            NULL, /* member */
                                            NULL, /* path - TODO: really want wilcard support here */
                                            NULL, /* arg0 */
                                            G_DBUS_SIGNAL_FLAGS_NONE,
                                            signal_cb,
                                            manager,
                                            NULL); /* user_data_free_func */
    }
}

static void
maybe_unsubscribe_signals (GDBusObjectManagerClient *manager)
{
  g_return_if_fail (G_IS_DBUS_OBJECT_MANAGER_CLIENT (manager));

  if (manager->priv->signal_subscription_id > 0)
    {
      g_dbus_connection_signal_unsubscribe (manager->priv->connection,
                                            manager->priv->signal_subscription_id);
      manager->priv->signal_subscription_id = 0;
    }

  if (manager->priv->match_rule != NULL)
    {
      /* Since the AddMatch call succeeded this is guaranteed to not
       * fail - therefore, don't bother checking the return value
       */
      g_dbus_connection_call (manager->priv->connection,
                              "org.freedesktop.DBus",
                              "/org/freedesktop/DBus",
                              "org.freedesktop.DBus",
                              "RemoveMatch",
                              g_variant_new ("(s)",
                                             manager->priv->match_rule),
                              NULL, /* reply_type */
                              G_DBUS_CALL_FLAGS_NONE,
                              -1, /* default timeout */
                              NULL, /* GCancellable */
                              NULL, /* GAsyncReadyCallback */
                              NULL); /* user data */
      g_free (manager->priv->match_rule);
      manager->priv->match_rule = NULL;
    }

}

/* ---------------------------------------------------------------------------------------------------- */

static void
on_notify_g_name_owner (GObject    *object,
                        GParamSpec *pspec,
                        gpointer    user_data)
{
  GDBusObjectManagerClient *manager = G_DBUS_OBJECT_MANAGER_CLIENT (user_data);
  gchar *old_name_owner;
  gchar *new_name_owner;

  g_mutex_lock (&manager->priv->lock);
  old_name_owner = manager->priv->name_owner;
  new_name_owner = g_dbus_proxy_get_name_owner (manager->priv->control_proxy);
  manager->priv->name_owner = NULL;

  g_object_ref (manager);
  if (g_strcmp0 (old_name_owner, new_name_owner) != 0)
    {
      GList *l;
      GList *proxies;

      /* remote manager changed; nuke all local proxies  */
      proxies = g_hash_table_get_values (manager->priv->map_object_path_to_object_proxy);
      g_list_foreach (proxies, (GFunc) g_object_ref, NULL);
      g_hash_table_remove_all (manager->priv->map_object_path_to_object_proxy);

      g_mutex_unlock (&manager->priv->lock);

      /* do the :name-owner notify with a NULL name - this way the user knows
       * the ::object-proxy-removed following is because the name owner went
       * away
       */
      g_object_notify (G_OBJECT (manager), "name-owner");

      for (l = proxies; l != NULL; l = l->next)
        {
          GDBusObjectProxy *object_proxy = G_DBUS_OBJECT_PROXY (l->data);
          g_signal_emit_by_name (manager, "object-removed", object_proxy);
        }
      g_list_free_full (proxies, g_object_unref);

      /* nuke local filter */
      maybe_unsubscribe_signals (manager);
    }
  else
    {
      g_mutex_unlock (&manager->priv->lock);
    }

  if (new_name_owner != NULL)
    {
      GError *error;
      GVariant *value;

      //g_debug ("repopulating for %s", new_name_owner);

      /* TODO: do this async! */
      subscribe_signals (manager,
                         new_name_owner);
      error = NULL;
      value = g_dbus_proxy_call_sync (manager->priv->control_proxy,
                                      "GetManagedObjects",
                                      NULL, /* parameters */
                                      G_DBUS_CALL_FLAGS_NONE,
                                      -1,
                                      NULL,
                                      &error);
      if (value == NULL)
        {
          maybe_unsubscribe_signals (manager);
          g_warning ("Error calling GetManagedObjects() when name owner %s for name %s came back: %s",
                     new_name_owner,
                     manager->priv->name,
                     error->message);
          g_error_free (error);
        }
      else
        {
          process_get_all_result (manager, value, new_name_owner);
          g_variant_unref (value);
        }

      /* do the :name-owner notify *AFTER* emitting ::object-proxy-added signals - this
       * way the user knows that the signals were emitted because the name owner came back
       */
      g_mutex_lock (&manager->priv->lock);
      manager->priv->name_owner = new_name_owner;
      g_mutex_unlock (&manager->priv->lock);
      g_object_notify (G_OBJECT (manager), "name-owner");

    }
  g_free (old_name_owner);
  g_object_unref (manager);
}

static gboolean
initable_init (GInitable     *initable,
               GCancellable  *cancellable,
               GError       **error)
{
  GDBusObjectManagerClient *manager = G_DBUS_OBJECT_MANAGER_CLIENT (initable);
  gboolean ret;
  GVariant *value;
  GDBusProxyFlags proxy_flags;

  ret = FALSE;

  if (manager->priv->bus_type != G_BUS_TYPE_NONE)
    {
      g_assert (manager->priv->connection == NULL);
      manager->priv->connection = g_bus_get_sync (manager->priv->bus_type, cancellable, error);
      if (manager->priv->connection == NULL)
        goto out;
    }

  proxy_flags = G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES;
  if (manager->priv->flags & G_DBUS_OBJECT_MANAGER_CLIENT_FLAGS_DO_NOT_AUTO_START)
    proxy_flags |= G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START;

  manager->priv->control_proxy = g_dbus_proxy_new_sync (manager->priv->connection,
                                                        proxy_flags,
                                                        NULL, /* GDBusInterfaceInfo* */
                                                        manager->priv->name,
                                                        manager->priv->object_path,
                                                        "org.freedesktop.DBus.ObjectManager",
                                                        cancellable,
                                                        error);
  if (manager->priv->control_proxy == NULL)
    goto out;

  g_signal_connect (G_OBJECT (manager->priv->control_proxy),
                    "notify::g-name-owner",
                    G_CALLBACK (on_notify_g_name_owner),
                    manager);

  g_signal_connect (manager->priv->control_proxy,
                    "g-signal",
                    G_CALLBACK (on_control_proxy_g_signal),
                    manager);

  manager->priv->name_owner = g_dbus_proxy_get_name_owner (manager->priv->control_proxy);
  if (manager->priv->name_owner == NULL && manager->priv->name != NULL)
    {
      /* it's perfectly fine if there's no name owner.. we're just going to
       * wait until one is ready
       */
    }
  else
    {
      /* yay, we can get the objects */
      subscribe_signals (manager,
                         manager->priv->name_owner);
      value = g_dbus_proxy_call_sync (manager->priv->control_proxy,
                                      "GetManagedObjects",
                                      NULL, /* parameters */
                                      G_DBUS_CALL_FLAGS_NONE,
                                      -1,
                                      cancellable,
                                      error);
      if (value == NULL)
        {
          maybe_unsubscribe_signals (manager);
          g_warn_if_fail (g_signal_handlers_disconnect_by_func (manager->priv->control_proxy,
                                                                on_control_proxy_g_signal,
                                                                manager) == 1);
          g_object_unref (manager->priv->control_proxy);
          manager->priv->control_proxy = NULL;
          goto out;
        }

      process_get_all_result (manager, value, manager->priv->name_owner);
      g_variant_unref (value);
    }

  ret = TRUE;

 out:
  return ret;
}

static void
initable_iface_init (GInitableIface *initable_iface)
{
  initable_iface->init = initable_init;
}

static void
async_initable_iface_init (GAsyncInitableIface *async_initable_iface)
{
  /* for now, just use default: run GInitable code in thread */
}

/* ---------------------------------------------------------------------------------------------------- */

static void
add_interfaces (GDBusObjectManagerClient *manager,
                const gchar       *object_path,
                GVariant          *ifaces_and_properties,
                const gchar       *name_owner)
{
  GDBusObjectProxy *op;
  gboolean added;
  GVariantIter iter;
  const gchar *interface_name;
  GVariant *properties;
  GList *interface_added_signals, *l;
  GDBusProxy *interface_proxy;

  g_return_if_fail (name_owner == NULL || g_dbus_is_unique_name (name_owner));

  g_mutex_lock (&manager->priv->lock);

  interface_added_signals = NULL;
  added = FALSE;

  op = g_hash_table_lookup (manager->priv->map_object_path_to_object_proxy, object_path);
  if (op == NULL)
    {
      GType object_proxy_type;
      if (manager->priv->get_proxy_type_func != NULL)
        {
          object_proxy_type = manager->priv->get_proxy_type_func (manager,
                                                                  object_path,
                                                                  NULL,
                                                                  manager->priv->get_proxy_type_user_data);
          g_warn_if_fail (g_type_is_a (object_proxy_type, G_TYPE_DBUS_OBJECT_PROXY));
        }
      else
        {
          object_proxy_type = G_TYPE_DBUS_OBJECT_PROXY;
        }
      op = g_object_new (object_proxy_type,
                         "g-connection", manager->priv->connection,
                         "g-object-path", object_path,
                         NULL);
      added = TRUE;
    }
  g_object_ref (op);

  g_variant_iter_init (&iter, ifaces_and_properties);
  while (g_variant_iter_next (&iter,
                              "{&s@a{sv}}",
                              &interface_name,
                              &properties))
    {
      GError *error;
      GType interface_proxy_type;

      if (manager->priv->get_proxy_type_func != NULL)
        {
          interface_proxy_type = manager->priv->get_proxy_type_func (manager,
                                                                     object_path,
                                                                     interface_name,
                                                                     manager->priv->get_proxy_type_user_data);
          g_warn_if_fail (g_type_is_a (interface_proxy_type, G_TYPE_DBUS_PROXY));
        }
      else
        {
          interface_proxy_type = G_TYPE_DBUS_PROXY;
        }

      /* this is fine - there is no blocking IO because we pass DO_NOT_LOAD_PROPERTIES and
       * DO_NOT_CONNECT_SIGNALS and use a unique name
       */
      error = NULL;
      interface_proxy = g_initable_new (interface_proxy_type,
                                        NULL, /* GCancellable */
                                        &error,
                                        "g-connection", manager->priv->connection,
                                        "g-flags", G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES |
                                                   G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS,
                                        "g-name", name_owner,
                                        "g-object-path", object_path,
                                        "g-interface-name", interface_name,
                                        NULL);
      if (interface_proxy == NULL)
        {
          g_warning ("%s: Error constructing proxy for path %s and interface %s: %s",
                     G_STRLOC,
                     object_path,
                     interface_name,
                     error->message);
          g_error_free (error);
        }
      else
        {
          GVariantIter property_iter;
          const gchar *property_name;
          GVariant *property_value;

          /* associate the interface proxy with the object */
          g_dbus_interface_set_object (G_DBUS_INTERFACE (interface_proxy),
                                       G_DBUS_OBJECT (op));

          g_variant_iter_init (&property_iter, properties);
          while (g_variant_iter_next (&property_iter,
                                      "{&sv}",
                                      &property_name,
                                      &property_value))
            {
              g_dbus_proxy_set_cached_property (interface_proxy,
                                                property_name,
                                                property_value);
              g_variant_unref (property_value);
            }

          _g_dbus_object_proxy_add_interface (op, interface_proxy);
          if (!added)
            interface_added_signals = g_list_append (interface_added_signals, g_object_ref (interface_proxy));
          g_object_unref (interface_proxy);
        }
      g_variant_unref (properties);
    }

  if (added)
    {
      g_hash_table_insert (manager->priv->map_object_path_to_object_proxy,
                           g_strdup (object_path),
                           op);
    }

  g_mutex_unlock (&manager->priv->lock);

  /* now that we don't hold the lock any more, emit signals */
  g_object_ref (manager);
  for (l = interface_added_signals; l != NULL; l = l->next)
    {
      interface_proxy = G_DBUS_PROXY (l->data);
      g_signal_emit_by_name (manager, "interface-added", op, interface_proxy);
      g_object_unref (interface_proxy);
    }
  g_list_free (interface_added_signals);

  if (added)
    g_signal_emit_by_name (manager, "object-added", op);

  g_object_unref (manager);
  g_object_unref (op);
}

static void
remove_interfaces (GDBusObjectManagerClient   *manager,
                   const gchar         *object_path,
                   const gchar *const  *interface_names)
{
  GDBusObjectProxy *op;
  GList *interfaces;
  guint n;
  guint num_interfaces;
  guint num_interfaces_to_remove;

  g_mutex_lock (&manager->priv->lock);

  op = g_hash_table_lookup (manager->priv->map_object_path_to_object_proxy, object_path);
  if (op == NULL)
    {
      g_warning ("%s: Processing InterfaceRemoved signal for path %s but no object proxy exists",
                 G_STRLOC,
                 object_path);
      g_mutex_unlock (&manager->priv->lock);
      goto out;
    }

  interfaces = g_dbus_object_get_interfaces (G_DBUS_OBJECT (op));
  num_interfaces = g_list_length (interfaces);
  g_list_free_full (interfaces, g_object_unref);

  num_interfaces_to_remove = g_strv_length ((gchar **) interface_names);

  /* see if we are going to completety remove the object */
  g_object_ref (manager);
  if (num_interfaces_to_remove == num_interfaces)
    {
      g_object_ref (op);
      g_warn_if_fail (g_hash_table_remove (manager->priv->map_object_path_to_object_proxy, object_path));
      g_mutex_unlock (&manager->priv->lock);
      g_signal_emit_by_name (manager, "object-removed", op);
      g_object_unref (op);
    }
  else
    {
      g_object_ref (op);
      g_mutex_unlock (&manager->priv->lock);
      for (n = 0; interface_names != NULL && interface_names[n] != NULL; n++)
        {
          GDBusInterface *interface;
          interface = g_dbus_object_get_interface (G_DBUS_OBJECT (op), interface_names[n]);
          _g_dbus_object_proxy_remove_interface (op, interface_names[n]);
          if (interface != NULL)
            {
              g_signal_emit_by_name (manager, "interface-removed", op, interface);
              g_object_unref (interface);
            }
        }
      g_object_unref (op);
    }
  g_object_unref (manager);
 out:
  ;
}

static void
process_get_all_result (GDBusObjectManagerClient *manager,
                        GVariant          *value,
                        const gchar       *name_owner)
{
  GVariant *arg0;
  const gchar *object_path;
  GVariant *ifaces_and_properties;
  GVariantIter iter;

  g_return_if_fail (name_owner == NULL || g_dbus_is_unique_name (name_owner));

  arg0 = g_variant_get_child_value (value, 0);
  g_variant_iter_init (&iter, arg0);
  while (g_variant_iter_next (&iter,
                              "{&o@a{sa{sv}}}",
                              &object_path,
                              &ifaces_and_properties))
    {
      add_interfaces (manager, object_path, ifaces_and_properties, name_owner);
      g_variant_unref (ifaces_and_properties);
    }
  g_variant_unref (arg0);
}

static void
on_control_proxy_g_signal (GDBusProxy   *proxy,
                           const gchar  *sender_name,
                           const gchar  *signal_name,
                           GVariant     *parameters,
                           gpointer      user_data)
{
  GDBusObjectManagerClient *manager = G_DBUS_OBJECT_MANAGER_CLIENT (user_data);
  const gchar *object_path;

  //g_debug ("yay, g_signal %s: %s\n", signal_name, g_variant_print (parameters, TRUE));

  if (g_strcmp0 (signal_name, "InterfacesAdded") == 0)
    {
      GVariant *ifaces_and_properties;
      g_variant_get (parameters,
                     "(&o@a{sa{sv}})",
                     &object_path,
                     &ifaces_and_properties);
      add_interfaces (manager, object_path, ifaces_and_properties, manager->priv->name_owner);
      g_variant_unref (ifaces_and_properties);
    }
  else if (g_strcmp0 (signal_name, "InterfacesRemoved") == 0)
    {
      const gchar **ifaces;
      g_variant_get (parameters,
                     "(&o^a&s)",
                     &object_path,
                     &ifaces);
      remove_interfaces (manager, object_path, ifaces);
      g_free (ifaces);
    }
}

/* ---------------------------------------------------------------------------------------------------- */

static const gchar *
g_dbus_object_manager_client_get_object_path (GDBusObjectManager *_manager)
{
  GDBusObjectManagerClient *manager = G_DBUS_OBJECT_MANAGER_CLIENT (_manager);
  return manager->priv->object_path;
}

static GDBusObject *
g_dbus_object_manager_client_get_object (GDBusObjectManager *_manager,
                                         const gchar        *object_path)
{
  GDBusObjectManagerClient *manager = G_DBUS_OBJECT_MANAGER_CLIENT (_manager);
  GDBusObject *ret;

  g_mutex_lock (&manager->priv->lock);
  ret = g_hash_table_lookup (manager->priv->map_object_path_to_object_proxy, object_path);
  if (ret != NULL)
    g_object_ref (ret);
  g_mutex_unlock (&manager->priv->lock);
  return ret;
}

static GDBusInterface *
g_dbus_object_manager_client_get_interface  (GDBusObjectManager  *_manager,
                                             const gchar         *object_path,
                                             const gchar         *interface_name)
{
  GDBusInterface *ret;
  GDBusObject *object;

  ret = NULL;

  object = g_dbus_object_manager_get_object (_manager, object_path);
  if (object == NULL)
    goto out;

  ret = g_dbus_object_get_interface (object, interface_name);
  g_object_unref (object);

 out:
  return ret;
}

static GList *
g_dbus_object_manager_client_get_objects (GDBusObjectManager *_manager)
{
  GDBusObjectManagerClient *manager = G_DBUS_OBJECT_MANAGER_CLIENT (_manager);
  GList *ret;

  g_return_val_if_fail (G_IS_DBUS_OBJECT_MANAGER_CLIENT (manager), NULL);

  g_mutex_lock (&manager->priv->lock);
  ret = g_hash_table_get_values (manager->priv->map_object_path_to_object_proxy);
  g_list_foreach (ret, (GFunc) g_object_ref, NULL);
  g_mutex_unlock (&manager->priv->lock);

  return ret;
}


static void
dbus_object_manager_interface_init (GDBusObjectManagerIface *iface)
{
  iface->get_object_path = g_dbus_object_manager_client_get_object_path;
  iface->get_objects     = g_dbus_object_manager_client_get_objects;
  iface->get_object      = g_dbus_object_manager_client_get_object;
  iface->get_interface   = g_dbus_object_manager_client_get_interface;
}

/* ---------------------------------------------------------------------------------------------------- */
