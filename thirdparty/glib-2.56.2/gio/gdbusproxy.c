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

#include <stdlib.h>
#include <string.h>

#include "gdbusutils.h"
#include "gdbusproxy.h"
#include "gioenumtypes.h"
#include "gdbusconnection.h"
#include "gdbuserror.h"
#include "gdbusprivate.h"
#include "ginitable.h"
#include "gasyncinitable.h"
#include "gioerror.h"
#include "gtask.h"
#include "gcancellable.h"
#include "gdbusinterface.h"
#include "gasyncresult.h"

#ifdef G_OS_UNIX
#include "gunixfdlist.h"
#endif

#include "glibintl.h"

/**
 * SECTION:gdbusproxy
 * @short_description: Client-side D-Bus interface proxy
 * @include: gio/gio.h
 *
 * #GDBusProxy is a base class used for proxies to access a D-Bus
 * interface on a remote object. A #GDBusProxy can be constructed for
 * both well-known and unique names.
 *
 * By default, #GDBusProxy will cache all properties (and listen to
 * changes) of the remote object, and proxy all signals that get
 * emitted. This behaviour can be changed by passing suitable
 * #GDBusProxyFlags when the proxy is created. If the proxy is for a
 * well-known name, the property cache is flushed when the name owner
 * vanishes and reloaded when a name owner appears.
 *
 * If a #GDBusProxy is used for a well-known name, the owner of the
 * name is tracked and can be read from
 * #GDBusProxy:g-name-owner. Connect to the #GObject::notify signal to
 * get notified of changes. Additionally, only signals and property
 * changes emitted from the current name owner are considered and
 * calls are always sent to the current name owner. This avoids a
 * number of race conditions when the name is lost by one owner and
 * claimed by another. However, if no name owner currently exists,
 * then calls will be sent to the well-known name which may result in
 * the message bus launching an owner (unless
 * %G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START is set).
 *
 * The generic #GDBusProxy::g-properties-changed and
 * #GDBusProxy::g-signal signals are not very convenient to work with.
 * Therefore, the recommended way of working with proxies is to subclass
 * #GDBusProxy, and have more natural properties and signals in your derived
 * class. This [example][gdbus-example-gdbus-codegen] shows how this can
 * easily be done using the [gdbus-codegen][gdbus-codegen] tool.
 *
 * A #GDBusProxy instance can be used from multiple threads but note
 * that all signals (e.g. #GDBusProxy::g-signal, #GDBusProxy::g-properties-changed
 * and #GObject::notify) are emitted in the
 * [thread-default main context][g-main-context-push-thread-default]
 * of the thread where the instance was constructed.
 *
 * An example using a proxy for a well-known name can be found in
 * [gdbus-example-watch-proxy.c](https://git.gnome.org/browse/glib/tree/gio/tests/gdbus-example-watch-proxy.c)
 */

/* lock protecting the mutable properties: name_owner, timeout_msec,
 * expected_interface, and the properties hash table
 */
G_LOCK_DEFINE_STATIC (properties_lock);

/* ---------------------------------------------------------------------------------------------------- */

G_LOCK_DEFINE_STATIC (signal_subscription_lock);

typedef struct
{
  volatile gint ref_count;
  GDBusProxy *proxy;
} SignalSubscriptionData;

static SignalSubscriptionData *
signal_subscription_ref (SignalSubscriptionData *data)
{
  g_atomic_int_inc (&data->ref_count);
  return data;
}

static void
signal_subscription_unref (SignalSubscriptionData *data)
{
  if (g_atomic_int_dec_and_test (&data->ref_count))
    {
      g_slice_free (SignalSubscriptionData, data);
    }
}

/* ---------------------------------------------------------------------------------------------------- */

struct _GDBusProxyPrivate
{
  GBusType bus_type;
  GDBusProxyFlags flags;
  GDBusConnection *connection;

  gchar *name;
  /* mutable, protected by properties_lock */
  gchar *name_owner;
  gchar *object_path;
  gchar *interface_name;
  /* mutable, protected by properties_lock */
  gint timeout_msec;

  guint name_owner_changed_subscription_id;

  GCancellable *get_all_cancellable;

  /* gchar* -> GVariant*, protected by properties_lock */
  GHashTable *properties;

  /* mutable, protected by properties_lock */
  GDBusInterfaceInfo *expected_interface;

  guint properties_changed_subscription_id;
  guint signals_subscription_id;

  gboolean initialized;

  /* mutable, protected by properties_lock */
  GDBusObject *object;

  SignalSubscriptionData *signal_subscription_data;
};

enum
{
  PROP_0,
  PROP_G_CONNECTION,
  PROP_G_BUS_TYPE,
  PROP_G_NAME,
  PROP_G_NAME_OWNER,
  PROP_G_FLAGS,
  PROP_G_OBJECT_PATH,
  PROP_G_INTERFACE_NAME,
  PROP_G_DEFAULT_TIMEOUT,
  PROP_G_INTERFACE_INFO
};

enum
{
  PROPERTIES_CHANGED_SIGNAL,
  SIGNAL_SIGNAL,
  LAST_SIGNAL,
};

static guint signals[LAST_SIGNAL] = {0};

static void dbus_interface_iface_init (GDBusInterfaceIface *dbus_interface_iface);
static void initable_iface_init       (GInitableIface *initable_iface);
static void async_initable_iface_init (GAsyncInitableIface *async_initable_iface);

G_DEFINE_TYPE_WITH_CODE (GDBusProxy, g_dbus_proxy, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (GDBusProxy)
                         G_IMPLEMENT_INTERFACE (G_TYPE_DBUS_INTERFACE, dbus_interface_iface_init)
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, initable_iface_init)
                         G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE, async_initable_iface_init))

static void
g_dbus_proxy_dispose (GObject *object)
{
  GDBusProxy *proxy = G_DBUS_PROXY (object);
  G_LOCK (signal_subscription_lock);
  if (proxy->priv->signal_subscription_data != NULL)
    {
      proxy->priv->signal_subscription_data->proxy = NULL;
      signal_subscription_unref (proxy->priv->signal_subscription_data);
      proxy->priv->signal_subscription_data = NULL;
    }
  G_UNLOCK (signal_subscription_lock);

  G_OBJECT_CLASS (g_dbus_proxy_parent_class)->dispose (object);
}

static void
g_dbus_proxy_finalize (GObject *object)
{
  GDBusProxy *proxy = G_DBUS_PROXY (object);

  g_warn_if_fail (proxy->priv->get_all_cancellable == NULL);

  if (proxy->priv->name_owner_changed_subscription_id > 0)
    g_dbus_connection_signal_unsubscribe (proxy->priv->connection,
                                          proxy->priv->name_owner_changed_subscription_id);

  if (proxy->priv->properties_changed_subscription_id > 0)
    g_dbus_connection_signal_unsubscribe (proxy->priv->connection,
                                          proxy->priv->properties_changed_subscription_id);

  if (proxy->priv->signals_subscription_id > 0)
    g_dbus_connection_signal_unsubscribe (proxy->priv->connection,
                                          proxy->priv->signals_subscription_id);

  if (proxy->priv->connection != NULL)
    g_object_unref (proxy->priv->connection);
  g_free (proxy->priv->name);
  g_free (proxy->priv->name_owner);
  g_free (proxy->priv->object_path);
  g_free (proxy->priv->interface_name);
  if (proxy->priv->properties != NULL)
    g_hash_table_unref (proxy->priv->properties);

  if (proxy->priv->expected_interface != NULL)
    {
      g_dbus_interface_info_cache_release (proxy->priv->expected_interface);
      g_dbus_interface_info_unref (proxy->priv->expected_interface);
    }

  if (proxy->priv->object != NULL)
    g_object_remove_weak_pointer (G_OBJECT (proxy->priv->object), (gpointer *) &proxy->priv->object);

  G_OBJECT_CLASS (g_dbus_proxy_parent_class)->finalize (object);
}

static void
g_dbus_proxy_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  GDBusProxy *proxy = G_DBUS_PROXY (object);

  switch (prop_id)
    {
    case PROP_G_CONNECTION:
      g_value_set_object (value, proxy->priv->connection);
      break;

    case PROP_G_FLAGS:
      g_value_set_flags (value, proxy->priv->flags);
      break;

    case PROP_G_NAME:
      g_value_set_string (value, proxy->priv->name);
      break;

    case PROP_G_NAME_OWNER:
      g_value_take_string (value, g_dbus_proxy_get_name_owner (proxy));
      break;

    case PROP_G_OBJECT_PATH:
      g_value_set_string (value, proxy->priv->object_path);
      break;

    case PROP_G_INTERFACE_NAME:
      g_value_set_string (value, proxy->priv->interface_name);
      break;

    case PROP_G_DEFAULT_TIMEOUT:
      g_value_set_int (value, g_dbus_proxy_get_default_timeout (proxy));
      break;

    case PROP_G_INTERFACE_INFO:
      g_value_set_boxed (value, g_dbus_proxy_get_interface_info (proxy));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
g_dbus_proxy_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  GDBusProxy *proxy = G_DBUS_PROXY (object);

  switch (prop_id)
    {
    case PROP_G_CONNECTION:
      proxy->priv->connection = g_value_dup_object (value);
      break;

    case PROP_G_FLAGS:
      proxy->priv->flags = g_value_get_flags (value);
      break;

    case PROP_G_NAME:
      proxy->priv->name = g_value_dup_string (value);
      break;

    case PROP_G_OBJECT_PATH:
      proxy->priv->object_path = g_value_dup_string (value);
      break;

    case PROP_G_INTERFACE_NAME:
      proxy->priv->interface_name = g_value_dup_string (value);
      break;

    case PROP_G_DEFAULT_TIMEOUT:
      g_dbus_proxy_set_default_timeout (proxy, g_value_get_int (value));
      break;

    case PROP_G_INTERFACE_INFO:
      g_dbus_proxy_set_interface_info (proxy, g_value_get_boxed (value));
      break;

    case PROP_G_BUS_TYPE:
      proxy->priv->bus_type = g_value_get_enum (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
g_dbus_proxy_class_init (GDBusProxyClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose      = g_dbus_proxy_dispose;
  gobject_class->finalize     = g_dbus_proxy_finalize;
  gobject_class->set_property = g_dbus_proxy_set_property;
  gobject_class->get_property = g_dbus_proxy_get_property;

  /* Note that all property names are prefixed to avoid collisions with D-Bus property names
   * in derived classes */

  /**
   * GDBusProxy:g-interface-info:
   *
   * Ensure that interactions with this proxy conform to the given
   * interface. This is mainly to ensure that malformed data received
   * from the other peer is ignored. The given #GDBusInterfaceInfo is
   * said to be the "expected interface".
   *
   * The checks performed are:
   * - When completing a method call, if the type signature of
   *   the reply message isn't what's expected, the reply is
   *   discarded and the #GError is set to %G_IO_ERROR_INVALID_ARGUMENT.
   *
   * - Received signals that have a type signature mismatch are dropped and
   *   a warning is logged via g_warning().
   *
   * - Properties received via the initial `GetAll()` call or via the 
   *   `::PropertiesChanged` signal (on the
   *   [org.freedesktop.DBus.Properties](http://dbus.freedesktop.org/doc/dbus-specification.html#standard-interfaces-properties)
   *   interface) or set using g_dbus_proxy_set_cached_property()
   *   with a type signature mismatch are ignored and a warning is
   *   logged via g_warning().
   *
   * Note that these checks are never done on methods, signals and
   * properties that are not referenced in the given
   * #GDBusInterfaceInfo, since extending a D-Bus interface on the
   * service-side is not considered an ABI break.
   *
   * Since: 2.26
   */
  g_object_class_install_property (gobject_class,
                                   PROP_G_INTERFACE_INFO,
                                   g_param_spec_boxed ("g-interface-info",
                                                       P_("Interface Information"),
                                                       P_("Interface Information"),
                                                       G_TYPE_DBUS_INTERFACE_INFO,
                                                       G_PARAM_READABLE |
                                                       G_PARAM_WRITABLE |
                                                       G_PARAM_STATIC_NAME |
                                                       G_PARAM_STATIC_BLURB |
                                                       G_PARAM_STATIC_NICK));

  /**
   * GDBusProxy:g-connection:
   *
   * The #GDBusConnection the proxy is for.
   *
   * Since: 2.26
   */
  g_object_class_install_property (gobject_class,
                                   PROP_G_CONNECTION,
                                   g_param_spec_object ("g-connection",
                                                        P_("g-connection"),
                                                        P_("The connection the proxy is for"),
                                                        G_TYPE_DBUS_CONNECTION,
                                                        G_PARAM_READABLE |
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_BLURB |
                                                        G_PARAM_STATIC_NICK));

  /**
   * GDBusProxy:g-bus-type:
   *
   * If this property is not %G_BUS_TYPE_NONE, then
   * #GDBusProxy:g-connection must be %NULL and will be set to the
   * #GDBusConnection obtained by calling g_bus_get() with the value
   * of this property.
   *
   * Since: 2.26
   */
  g_object_class_install_property (gobject_class,
                                   PROP_G_BUS_TYPE,
                                   g_param_spec_enum ("g-bus-type",
                                                      P_("Bus Type"),
                                                      P_("The bus to connect to, if any"),
                                                      G_TYPE_BUS_TYPE,
                                                      G_BUS_TYPE_NONE,
                                                      G_PARAM_WRITABLE |
                                                      G_PARAM_CONSTRUCT_ONLY |
                                                      G_PARAM_STATIC_NAME |
                                                      G_PARAM_STATIC_BLURB |
                                                      G_PARAM_STATIC_NICK));

  /**
   * GDBusProxy:g-flags:
   *
   * Flags from the #GDBusProxyFlags enumeration.
   *
   * Since: 2.26
   */
  g_object_class_install_property (gobject_class,
                                   PROP_G_FLAGS,
                                   g_param_spec_flags ("g-flags",
                                                       P_("g-flags"),
                                                       P_("Flags for the proxy"),
                                                       G_TYPE_DBUS_PROXY_FLAGS,
                                                       G_DBUS_PROXY_FLAGS_NONE,
                                                       G_PARAM_READABLE |
                                                       G_PARAM_WRITABLE |
                                                       G_PARAM_CONSTRUCT_ONLY |
                                                       G_PARAM_STATIC_NAME |
                                                       G_PARAM_STATIC_BLURB |
                                                       G_PARAM_STATIC_NICK));

  /**
   * GDBusProxy:g-name:
   *
   * The well-known or unique name that the proxy is for.
   *
   * Since: 2.26
   */
  g_object_class_install_property (gobject_class,
                                   PROP_G_NAME,
                                   g_param_spec_string ("g-name",
                                                        P_("g-name"),
                                                        P_("The well-known or unique name that the proxy is for"),
                                                        NULL,
                                                        G_PARAM_READABLE |
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_BLURB |
                                                        G_PARAM_STATIC_NICK));

  /**
   * GDBusProxy:g-name-owner:
   *
   * The unique name that owns #GDBusProxy:g-name or %NULL if no-one
   * currently owns that name. You may connect to #GObject::notify signal to
   * track changes to this property.
   *
   * Since: 2.26
   */
  g_object_class_install_property (gobject_class,
                                   PROP_G_NAME_OWNER,
                                   g_param_spec_string ("g-name-owner",
                                                        P_("g-name-owner"),
                                                        P_("The unique name for the owner"),
                                                        NULL,
                                                        G_PARAM_READABLE |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_BLURB |
                                                        G_PARAM_STATIC_NICK));

  /**
   * GDBusProxy:g-object-path:
   *
   * The object path the proxy is for.
   *
   * Since: 2.26
   */
  g_object_class_install_property (gobject_class,
                                   PROP_G_OBJECT_PATH,
                                   g_param_spec_string ("g-object-path",
                                                        P_("g-object-path"),
                                                        P_("The object path the proxy is for"),
                                                        NULL,
                                                        G_PARAM_READABLE |
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_BLURB |
                                                        G_PARAM_STATIC_NICK));

  /**
   * GDBusProxy:g-interface-name:
   *
   * The D-Bus interface name the proxy is for.
   *
   * Since: 2.26
   */
  g_object_class_install_property (gobject_class,
                                   PROP_G_INTERFACE_NAME,
                                   g_param_spec_string ("g-interface-name",
                                                        P_("g-interface-name"),
                                                        P_("The D-Bus interface name the proxy is for"),
                                                        NULL,
                                                        G_PARAM_READABLE |
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_BLURB |
                                                        G_PARAM_STATIC_NICK));

  /**
   * GDBusProxy:g-default-timeout:
   *
   * The timeout to use if -1 (specifying default timeout) is passed
   * as @timeout_msec in the g_dbus_proxy_call() and
   * g_dbus_proxy_call_sync() functions.
   *
   * This allows applications to set a proxy-wide timeout for all
   * remote method invocations on the proxy. If this property is -1,
   * the default timeout (typically 25 seconds) is used. If set to
   * %G_MAXINT, then no timeout is used.
   *
   * Since: 2.26
   */
  g_object_class_install_property (gobject_class,
                                   PROP_G_DEFAULT_TIMEOUT,
                                   g_param_spec_int ("g-default-timeout",
                                                     P_("Default Timeout"),
                                                     P_("Timeout for remote method invocation"),
                                                     -1,
                                                     G_MAXINT,
                                                     -1,
                                                     G_PARAM_READABLE |
                                                     G_PARAM_WRITABLE |
                                                     G_PARAM_CONSTRUCT |
                                                     G_PARAM_STATIC_NAME |
                                                     G_PARAM_STATIC_BLURB |
                                                     G_PARAM_STATIC_NICK));

  /**
   * GDBusProxy::g-properties-changed:
   * @proxy: The #GDBusProxy emitting the signal.
   * @changed_properties: A #GVariant containing the properties that changed
   * @invalidated_properties: A %NULL terminated array of properties that was invalidated
   *
   * Emitted when one or more D-Bus properties on @proxy changes. The
   * local cache has already been updated when this signal fires. Note
   * that both @changed_properties and @invalidated_properties are
   * guaranteed to never be %NULL (either may be empty though).
   *
   * If the proxy has the flag
   * %G_DBUS_PROXY_FLAGS_GET_INVALIDATED_PROPERTIES set, then
   * @invalidated_properties will always be empty.
   *
   * This signal corresponds to the
   * `PropertiesChanged` D-Bus signal on the
   * `org.freedesktop.DBus.Properties` interface.
   *
   * Since: 2.26
   */
  signals[PROPERTIES_CHANGED_SIGNAL] = g_signal_new (I_("g-properties-changed"),
                                                     G_TYPE_DBUS_PROXY,
                                                     G_SIGNAL_RUN_LAST | G_SIGNAL_MUST_COLLECT,
                                                     G_STRUCT_OFFSET (GDBusProxyClass, g_properties_changed),
                                                     NULL,
                                                     NULL,
                                                     NULL,
                                                     G_TYPE_NONE,
                                                     2,
                                                     G_TYPE_VARIANT,
                                                     G_TYPE_STRV | G_SIGNAL_TYPE_STATIC_SCOPE);

  /**
   * GDBusProxy::g-signal:
   * @proxy: The #GDBusProxy emitting the signal.
   * @sender_name: (nullable): The sender of the signal or %NULL if the connection is not a bus connection.
   * @signal_name: The name of the signal.
   * @parameters: A #GVariant tuple with parameters for the signal.
   *
   * Emitted when a signal from the remote object and interface that @proxy is for, has been received.
   *
   * Since: 2.26
   */
  signals[SIGNAL_SIGNAL] = g_signal_new (I_("g-signal"),
                                         G_TYPE_DBUS_PROXY,
                                         G_SIGNAL_RUN_LAST | G_SIGNAL_MUST_COLLECT,
                                         G_STRUCT_OFFSET (GDBusProxyClass, g_signal),
                                         NULL,
                                         NULL,
                                         NULL,
                                         G_TYPE_NONE,
                                         3,
                                         G_TYPE_STRING,
                                         G_TYPE_STRING,
                                         G_TYPE_VARIANT);

}

static void
g_dbus_proxy_init (GDBusProxy *proxy)
{
  proxy->priv = g_dbus_proxy_get_instance_private (proxy);
  proxy->priv->signal_subscription_data = g_slice_new0 (SignalSubscriptionData);
  proxy->priv->signal_subscription_data->ref_count = 1;
  proxy->priv->signal_subscription_data->proxy = proxy;
  proxy->priv->properties = g_hash_table_new_full (g_str_hash,
                                                   g_str_equal,
                                                   g_free,
                                                   (GDestroyNotify) g_variant_unref);
}

/* ---------------------------------------------------------------------------------------------------- */

static gint
property_name_sort_func (const gchar **a,
                         const gchar **b)
{
  return g_strcmp0 (*a, *b);
}

/**
 * g_dbus_proxy_get_cached_property_names:
 * @proxy: A #GDBusProxy.
 *
 * Gets the names of all cached properties on @proxy.
 *
 * Returns: (transfer full) (nullable) (array zero-terminated=1): A
 *          %NULL-terminated array of strings or %NULL if
 *          @proxy has no cached properties. Free the returned array with
 *          g_strfreev().
 *
 * Since: 2.26
 */
gchar **
g_dbus_proxy_get_cached_property_names (GDBusProxy  *proxy)
{
  gchar **names;
  GPtrArray *p;
  GHashTableIter iter;
  const gchar *key;

  g_return_val_if_fail (G_IS_DBUS_PROXY (proxy), NULL);

  G_LOCK (properties_lock);

  names = NULL;
  if (g_hash_table_size (proxy->priv->properties) == 0)
    goto out;

  p = g_ptr_array_new ();

  g_hash_table_iter_init (&iter, proxy->priv->properties);
  while (g_hash_table_iter_next (&iter, (gpointer) &key, NULL))
    g_ptr_array_add (p, g_strdup (key));
  g_ptr_array_sort (p, (GCompareFunc) property_name_sort_func);
  g_ptr_array_add (p, NULL);

  names = (gchar **) g_ptr_array_free (p, FALSE);

 out:
  G_UNLOCK (properties_lock);
  return names;
}

/* properties_lock must be held for as long as you will keep the
 * returned value
 */
static const GDBusPropertyInfo *
lookup_property_info (GDBusProxy  *proxy,
                      const gchar *property_name)
{
  const GDBusPropertyInfo *info = NULL;

  if (proxy->priv->expected_interface == NULL)
    goto out;

  info = g_dbus_interface_info_lookup_property (proxy->priv->expected_interface, property_name);

 out:
  return info;
}

/**
 * g_dbus_proxy_get_cached_property:
 * @proxy: A #GDBusProxy.
 * @property_name: Property name.
 *
 * Looks up the value for a property from the cache. This call does no
 * blocking IO.
 *
 * If @proxy has an expected interface (see
 * #GDBusProxy:g-interface-info) and @property_name is referenced by
 * it, then @value is checked against the type of the property.
 *
 * Returns: (transfer full) (nullable): A reference to the #GVariant instance
 *    that holds the value for @property_name or %NULL if the value is not in
 *    the cache. The returned reference must be freed with g_variant_unref().
 *
 * Since: 2.26
 */
GVariant *
g_dbus_proxy_get_cached_property (GDBusProxy   *proxy,
                                  const gchar  *property_name)
{
  const GDBusPropertyInfo *info;
  GVariant *value;

  g_return_val_if_fail (G_IS_DBUS_PROXY (proxy), NULL);
  g_return_val_if_fail (property_name != NULL, NULL);

  G_LOCK (properties_lock);

  value = g_hash_table_lookup (proxy->priv->properties, property_name);
  if (value == NULL)
    goto out;

  info = lookup_property_info (proxy, property_name);
  if (info != NULL)
    {
      const gchar *type_string = g_variant_get_type_string (value);
      if (g_strcmp0 (type_string, info->signature) != 0)
        {
          g_warning ("Trying to get property %s with type %s but according to the expected "
                     "interface the type is %s",
                     property_name,
                     type_string,
                     info->signature);
          value = NULL;
          goto out;
        }
    }

  g_variant_ref (value);

 out:
  G_UNLOCK (properties_lock);
  return value;
}

/**
 * g_dbus_proxy_set_cached_property:
 * @proxy: A #GDBusProxy
 * @property_name: Property name.
 * @value: (nullable): Value for the property or %NULL to remove it from the cache.
 *
 * If @value is not %NULL, sets the cached value for the property with
 * name @property_name to the value in @value.
 *
 * If @value is %NULL, then the cached value is removed from the
 * property cache.
 *
 * If @proxy has an expected interface (see
 * #GDBusProxy:g-interface-info) and @property_name is referenced by
 * it, then @value is checked against the type of the property.
 *
 * If the @value #GVariant is floating, it is consumed. This allows
 * convenient 'inline' use of g_variant_new(), e.g.
 * |[<!-- language="C" -->
 *  g_dbus_proxy_set_cached_property (proxy,
 *                                    "SomeProperty",
 *                                    g_variant_new ("(si)",
 *                                                  "A String",
 *                                                  42));
 * ]|
 *
 * Normally you will not need to use this method since @proxy
 * is tracking changes using the
 * `org.freedesktop.DBus.Properties.PropertiesChanged`
 * D-Bus signal. However, for performance reasons an object may
 * decide to not use this signal for some properties and instead
 * use a proprietary out-of-band mechanism to transmit changes.
 *
 * As a concrete example, consider an object with a property
 * `ChatroomParticipants` which is an array of strings. Instead of
 * transmitting the same (long) array every time the property changes,
 * it is more efficient to only transmit the delta using e.g. signals
 * `ChatroomParticipantJoined(String name)` and
 * `ChatroomParticipantParted(String name)`.
 *
 * Since: 2.26
 */
void
g_dbus_proxy_set_cached_property (GDBusProxy   *proxy,
                                  const gchar  *property_name,
                                  GVariant     *value)
{
  const GDBusPropertyInfo *info;

  g_return_if_fail (G_IS_DBUS_PROXY (proxy));
  g_return_if_fail (property_name != NULL);

  G_LOCK (properties_lock);

  if (value != NULL)
    {
      info = lookup_property_info (proxy, property_name);
      if (info != NULL)
        {
          if (g_strcmp0 (info->signature, g_variant_get_type_string (value)) != 0)
            {
              g_warning ("Trying to set property %s of type %s but according to the expected "
			 "interface the type is %s",
                         property_name,
                         g_variant_get_type_string (value),
                         info->signature);
              goto out;
            }
        }
      g_hash_table_insert (proxy->priv->properties,
                           g_strdup (property_name),
                           g_variant_ref_sink (value));
    }
  else
    {
      g_hash_table_remove (proxy->priv->properties, property_name);
    }

 out:
  G_UNLOCK (properties_lock);
}

/* ---------------------------------------------------------------------------------------------------- */

static void
on_signal_received (GDBusConnection *connection,
                    const gchar     *sender_name,
                    const gchar     *object_path,
                    const gchar     *interface_name,
                    const gchar     *signal_name,
                    GVariant        *parameters,
                    gpointer         user_data)
{
  SignalSubscriptionData *data = user_data;
  GDBusProxy *proxy;

  G_LOCK (signal_subscription_lock);
  proxy = data->proxy;
  if (proxy == NULL)
    {
      G_UNLOCK (signal_subscription_lock);
      return;
    }
  else
    {
      g_object_ref (proxy);
      G_UNLOCK (signal_subscription_lock);
    }

  if (!proxy->priv->initialized)
    goto out;

  G_LOCK (properties_lock);

  if (proxy->priv->name_owner != NULL && g_strcmp0 (sender_name, proxy->priv->name_owner) != 0)
    {
      G_UNLOCK (properties_lock);
      goto out;
    }

  if (proxy->priv->expected_interface != NULL)
    {
      const GDBusSignalInfo *info;
      info = g_dbus_interface_info_lookup_signal (proxy->priv->expected_interface, signal_name);
      if (info != NULL)
        {
          GVariantType *expected_type;
          expected_type = _g_dbus_compute_complete_signature (info->args);
          if (!g_variant_type_equal (expected_type, g_variant_get_type (parameters)))
            {
              gchar *expected_type_string = g_variant_type_dup_string (expected_type);
              g_warning ("Dropping signal %s of type %s since the type from the expected interface is %s",
                         info->name,
                         g_variant_get_type_string (parameters),
                         expected_type_string);
              g_free (expected_type_string);
              g_variant_type_free (expected_type);
              G_UNLOCK (properties_lock);
              goto out;
            }
          g_variant_type_free (expected_type);
        }
    }

  G_UNLOCK (properties_lock);

  g_signal_emit (proxy,
                 signals[SIGNAL_SIGNAL],
                 0,
                 sender_name,
                 signal_name,
                 parameters);

 out:
  if (proxy != NULL)
    g_object_unref (proxy);
}

/* ---------------------------------------------------------------------------------------------------- */

/* must hold properties_lock */
static void
insert_property_checked (GDBusProxy  *proxy,
			 gchar *property_name,
			 GVariant *value)
{
  if (proxy->priv->expected_interface != NULL)
    {
      const GDBusPropertyInfo *info;
      info = g_dbus_interface_info_lookup_property (proxy->priv->expected_interface, property_name);
      /* Only check known properties */
      if (info != NULL)
        {
          /* Warn about properties with the wrong type */
          if (g_strcmp0 (info->signature, g_variant_get_type_string (value)) != 0)
            {
              g_warning ("Received property %s with type %s does not match expected type "
                         "%s in the expected interface",
                         property_name,
                         g_variant_get_type_string (value),
                         info->signature);
              goto invalid;
            }
        }
    }

  g_hash_table_insert (proxy->priv->properties,
		       property_name, /* adopts string */
		       value); /* adopts value */

  return;

 invalid:
  g_variant_unref (value);
  g_free (property_name);
}

typedef struct
{
  GDBusProxy *proxy;
  gchar *prop_name;
} InvalidatedPropGetData;

static void
invalidated_property_get_cb (GDBusConnection *connection,
                             GAsyncResult    *res,
                             gpointer         user_data)
{
  InvalidatedPropGetData *data = user_data;
  const gchar *invalidated_properties[] = {NULL};
  GVariantBuilder builder;
  GVariant *value = NULL;
  GVariant *unpacked_value = NULL;

  /* errors are fine, the other end could have disconnected */
  value = g_dbus_connection_call_finish (connection, res, NULL);
  if (value == NULL)
    {
      goto out;
    }

  if (!g_variant_is_of_type (value, G_VARIANT_TYPE ("(v)")))
    {
      g_warning ("Expected type '(v)' for Get() reply, got '%s'", g_variant_get_type_string (value));
      goto out;
    }

  g_variant_get (value, "(v)", &unpacked_value);

  /* synthesize the a{sv} in the PropertiesChanged signal */
  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a{sv}"));
  g_variant_builder_add (&builder, "{sv}", data->prop_name, unpacked_value);

  G_LOCK (properties_lock);
  insert_property_checked (data->proxy,
                           data->prop_name,  /* adopts string */
                           unpacked_value);  /* adopts value */
  data->prop_name = NULL;
  G_UNLOCK (properties_lock);

  g_signal_emit (data->proxy,
                 signals[PROPERTIES_CHANGED_SIGNAL], 0,
                 g_variant_builder_end (&builder), /* consumed */
                 invalidated_properties);


 out:
  if (value != NULL)
    g_variant_unref (value);
  g_object_unref (data->proxy);
  g_free (data->prop_name);
  g_slice_free (InvalidatedPropGetData, data);
}

static void
on_properties_changed (GDBusConnection *connection,
                       const gchar     *sender_name,
                       const gchar     *object_path,
                       const gchar     *interface_name,
                       const gchar     *signal_name,
                       GVariant        *parameters,
                       gpointer         user_data)
{
  SignalSubscriptionData *data = user_data;
  gboolean emit_g_signal = FALSE;
  GDBusProxy *proxy;
  const gchar *interface_name_for_signal;
  GVariant *changed_properties;
  gchar **invalidated_properties;
  GVariantIter iter;
  gchar *key;
  GVariant *value;
  guint n;

  changed_properties = NULL;
  invalidated_properties = NULL;

  G_LOCK (signal_subscription_lock);
  proxy = data->proxy;
  if (proxy == NULL)
    {
      G_UNLOCK (signal_subscription_lock);
      goto out;
    }
  else
    {
      g_object_ref (proxy);
      G_UNLOCK (signal_subscription_lock);
    }

  if (!proxy->priv->initialized)
    goto out;

  G_LOCK (properties_lock);

  if (proxy->priv->name_owner != NULL && g_strcmp0 (sender_name, proxy->priv->name_owner) != 0)
    {
      G_UNLOCK (properties_lock);
      goto out;
    }

  if (!g_variant_is_of_type (parameters, G_VARIANT_TYPE ("(sa{sv}as)")))
    {
      g_warning ("Value for PropertiesChanged signal with type '%s' does not match '(sa{sv}as)'",
                 g_variant_get_type_string (parameters));
      G_UNLOCK (properties_lock);
      goto out;
    }

  g_variant_get (parameters,
                 "(&s@a{sv}^a&s)",
                 &interface_name_for_signal,
                 &changed_properties,
                 &invalidated_properties);

  if (g_strcmp0 (interface_name_for_signal, proxy->priv->interface_name) != 0)
    {
      G_UNLOCK (properties_lock);
      goto out;
    }

  g_variant_iter_init (&iter, changed_properties);
  while (g_variant_iter_next (&iter, "{sv}", &key, &value))
    {
      insert_property_checked (proxy,
			       key, /* adopts string */
			       value); /* adopts value */
      emit_g_signal = TRUE;
    }

  if (proxy->priv->flags & G_DBUS_PROXY_FLAGS_GET_INVALIDATED_PROPERTIES)
    {
      if (proxy->priv->name_owner != NULL)
        {
          for (n = 0; invalidated_properties[n] != NULL; n++)
            {
              InvalidatedPropGetData *data;
              data = g_slice_new0 (InvalidatedPropGetData);
              data->proxy = g_object_ref (proxy);
              data->prop_name = g_strdup (invalidated_properties[n]);
              g_dbus_connection_call (proxy->priv->connection,
                                      proxy->priv->name_owner,
                                      proxy->priv->object_path,
                                      "org.freedesktop.DBus.Properties",
                                      "Get",
                                      g_variant_new ("(ss)", proxy->priv->interface_name, data->prop_name),
                                      G_VARIANT_TYPE ("(v)"),
                                      G_DBUS_CALL_FLAGS_NONE,
                                      -1,           /* timeout */
                                      NULL,         /* GCancellable */
                                      (GAsyncReadyCallback) invalidated_property_get_cb,
                                      data);
            }
        }
    }
  else
    {
      emit_g_signal = TRUE;
      for (n = 0; invalidated_properties[n] != NULL; n++)
        {
          g_hash_table_remove (proxy->priv->properties, invalidated_properties[n]);
        }
    }

  G_UNLOCK (properties_lock);

  if (emit_g_signal)
    {
      g_signal_emit (proxy, signals[PROPERTIES_CHANGED_SIGNAL],
                     0,
                     changed_properties,
                     invalidated_properties);
    }

 out:
  if (changed_properties != NULL)
    g_variant_unref (changed_properties);
  g_free (invalidated_properties);
  if (proxy != NULL)
    g_object_unref (proxy);
}

/* ---------------------------------------------------------------------------------------------------- */

static void
process_get_all_reply (GDBusProxy *proxy,
                       GVariant   *result)
{
  GVariantIter *iter;
  gchar *key;
  GVariant *value;
  guint num_properties;

  if (!g_variant_is_of_type (result, G_VARIANT_TYPE ("(a{sv})")))
    {
      g_warning ("Value for GetAll reply with type '%s' does not match '(a{sv})'",
                 g_variant_get_type_string (result));
      goto out;
    }

  G_LOCK (properties_lock);

  g_variant_get (result, "(a{sv})", &iter);
  while (g_variant_iter_next (iter, "{sv}", &key, &value))
    {
      insert_property_checked (proxy,
			       key, /* adopts string */
			       value); /* adopts value */
    }
  g_variant_iter_free (iter);

  num_properties = g_hash_table_size (proxy->priv->properties);
  G_UNLOCK (properties_lock);

  /* Synthesize ::g-properties-changed changed */
  if (num_properties > 0)
    {
      GVariant *changed_properties;
      const gchar *invalidated_properties[1] = {NULL};

      g_variant_get (result,
                     "(@a{sv})",
                     &changed_properties);
      g_signal_emit (proxy, signals[PROPERTIES_CHANGED_SIGNAL],
                     0,
                     changed_properties,
                     invalidated_properties);
      g_variant_unref (changed_properties);
    }

 out:
  ;
}

typedef struct
{
  GDBusProxy *proxy;
  GCancellable *cancellable;
  gchar *name_owner;
} LoadPropertiesOnNameOwnerChangedData;

static void
on_name_owner_changed_get_all_cb (GDBusConnection *connection,
                                  GAsyncResult    *res,
                                  gpointer         user_data)
{
  LoadPropertiesOnNameOwnerChangedData *data = user_data;
  GVariant *result;
  GError *error;
  gboolean cancelled;

  cancelled = FALSE;

  error = NULL;
  result = g_dbus_connection_call_finish (connection,
                                          res,
                                          &error);
  if (result == NULL)
    {
      if (error->domain == G_IO_ERROR && error->code == G_IO_ERROR_CANCELLED)
        cancelled = TRUE;
      /* We just ignore if GetAll() is failing. Because this might happen
       * if the object has no properties at all. Or if the caller is
       * not authorized to see the properties.
       *
       * Either way, apps can know about this by using
       * get_cached_property_names() or get_cached_property().
       *
       * TODO: handle G_DBUS_DEBUG flag 'proxy' and, if enabled, log the
       * fact that GetAll() failed
       */
      //g_debug ("error: %d %d %s", error->domain, error->code, error->message);
      g_error_free (error);
    }

  /* and finally we can notify */
  if (!cancelled)
    {
      G_LOCK (properties_lock);
      g_free (data->proxy->priv->name_owner);
      data->proxy->priv->name_owner = data->name_owner;
      data->name_owner = NULL; /* to avoid an extra copy, we steal the string */
      g_hash_table_remove_all (data->proxy->priv->properties);
      G_UNLOCK (properties_lock);
      if (result != NULL)
        {
          process_get_all_reply (data->proxy, result);
          g_variant_unref (result);
        }

      g_object_notify (G_OBJECT (data->proxy), "g-name-owner");
    }

  if (data->cancellable == data->proxy->priv->get_all_cancellable)
    data->proxy->priv->get_all_cancellable = NULL;

  g_object_unref (data->proxy);
  g_object_unref (data->cancellable);
  g_free (data->name_owner);
  g_free (data);
}

static void
on_name_owner_changed (GDBusConnection *connection,
                       const gchar      *sender_name,
                       const gchar      *object_path,
                       const gchar      *interface_name,
                       const gchar      *signal_name,
                       GVariant         *parameters,
                       gpointer          user_data)
{
  SignalSubscriptionData *data = user_data;
  GDBusProxy *proxy;
  const gchar *old_owner;
  const gchar *new_owner;

  G_LOCK (signal_subscription_lock);
  proxy = data->proxy;
  if (proxy == NULL)
    {
      G_UNLOCK (signal_subscription_lock);
      goto out;
    }
  else
    {
      g_object_ref (proxy);
      G_UNLOCK (signal_subscription_lock);
    }

  /* if we are already trying to load properties, cancel that */
  if (proxy->priv->get_all_cancellable != NULL)
    {
      g_cancellable_cancel (proxy->priv->get_all_cancellable);
      proxy->priv->get_all_cancellable = NULL;
    }

  g_variant_get (parameters,
                 "(&s&s&s)",
                 NULL,
                 &old_owner,
                 &new_owner);

  if (strlen (new_owner) == 0)
    {
      G_LOCK (properties_lock);
      g_free (proxy->priv->name_owner);
      proxy->priv->name_owner = NULL;

      /* Synthesize ::g-properties-changed changed */
      if (!(proxy->priv->flags & G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES) &&
          g_hash_table_size (proxy->priv->properties) > 0)
        {
          GVariantBuilder builder;
          GPtrArray *invalidated_properties;
          GHashTableIter iter;
          const gchar *key;

          /* Build changed_properties (always empty) and invalidated_properties ... */
          g_variant_builder_init (&builder, G_VARIANT_TYPE ("a{sv}"));

          invalidated_properties = g_ptr_array_new_with_free_func (g_free);
          g_hash_table_iter_init (&iter, proxy->priv->properties);
          while (g_hash_table_iter_next (&iter, (gpointer) &key, NULL))
            g_ptr_array_add (invalidated_properties, g_strdup (key));
          g_ptr_array_add (invalidated_properties, NULL);

          /* ... throw out the properties ... */
          g_hash_table_remove_all (proxy->priv->properties);

          G_UNLOCK (properties_lock);

          /* ... and finally emit the ::g-properties-changed signal */
          g_signal_emit (proxy, signals[PROPERTIES_CHANGED_SIGNAL],
                         0,
                         g_variant_builder_end (&builder) /* consumed */,
                         (const gchar* const *) invalidated_properties->pdata);
          g_ptr_array_unref (invalidated_properties);
        }
      else
        {
          G_UNLOCK (properties_lock);
        }
      g_object_notify (G_OBJECT (proxy), "g-name-owner");
    }
  else
    {
      G_LOCK (properties_lock);

      /* ignore duplicates - this can happen when activating the service */
      if (g_strcmp0 (new_owner, proxy->priv->name_owner) == 0)
        {
          G_UNLOCK (properties_lock);
          goto out;
        }

      if (proxy->priv->flags & G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES)
        {
          g_free (proxy->priv->name_owner);
          proxy->priv->name_owner = g_strdup (new_owner);

          g_hash_table_remove_all (proxy->priv->properties);
          G_UNLOCK (properties_lock);
          g_object_notify (G_OBJECT (proxy), "g-name-owner");
        }
      else
        {
          LoadPropertiesOnNameOwnerChangedData *data;

          G_UNLOCK (properties_lock);

          /* start loading properties.. only then emit notify::g-name-owner .. we
           * need to be able to cancel this in the event another NameOwnerChanged
           * signal suddenly happens
           */

          g_assert (proxy->priv->get_all_cancellable == NULL);
          proxy->priv->get_all_cancellable = g_cancellable_new ();
          data = g_new0 (LoadPropertiesOnNameOwnerChangedData, 1);
          data->proxy = g_object_ref (proxy);
          data->cancellable = proxy->priv->get_all_cancellable;
          data->name_owner = g_strdup (new_owner);
          g_dbus_connection_call (proxy->priv->connection,
                                  data->name_owner,
                                  proxy->priv->object_path,
                                  "org.freedesktop.DBus.Properties",
                                  "GetAll",
                                  g_variant_new ("(s)", proxy->priv->interface_name),
                                  G_VARIANT_TYPE ("(a{sv})"),
                                  G_DBUS_CALL_FLAGS_NONE,
                                  -1,           /* timeout */
                                  proxy->priv->get_all_cancellable,
                                  (GAsyncReadyCallback) on_name_owner_changed_get_all_cb,
                                  data);
        }
    }

 out:
  if (proxy != NULL)
    g_object_unref (proxy);
}

/* ---------------------------------------------------------------------------------------------------- */

static void
async_init_get_all_cb (GDBusConnection *connection,
                       GAsyncResult    *res,
                       gpointer         user_data)
{
  GTask *task = user_data;
  GVariant *result;
  GError *error;

  error = NULL;
  result = g_dbus_connection_call_finish (connection,
                                          res,
                                          &error);
  if (result == NULL)
    {
      /* We just ignore if GetAll() is failing. Because this might happen
       * if the object has no properties at all. Or if the caller is
       * not authorized to see the properties.
       *
       * Either way, apps can know about this by using
       * get_cached_property_names() or get_cached_property().
       *
       * TODO: handle G_DBUS_DEBUG flag 'proxy' and, if enabled, log the
       * fact that GetAll() failed
       */
      //g_debug ("error: %d %d %s", error->domain, error->code, error->message);
      g_error_free (error);
    }

  g_task_return_pointer (task, result,
                         (GDestroyNotify) g_variant_unref);
  g_object_unref (task);
}

static void
async_init_data_set_name_owner (GTask       *task,
                                const gchar *name_owner)
{
  GDBusProxy *proxy = g_task_get_source_object (task);
  gboolean get_all;

  if (name_owner != NULL)
    {
      G_LOCK (properties_lock);
      /* Must free first, since on_name_owner_changed() could run before us */
      g_free (proxy->priv->name_owner);
      proxy->priv->name_owner = g_strdup (name_owner);
      G_UNLOCK (properties_lock);
    }

  get_all = TRUE;

  if (proxy->priv->flags & G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES)
    {
      /* Don't load properties if the API user doesn't want them */
      get_all = FALSE;
    }
  else if (name_owner == NULL && proxy->priv->name != NULL)
    {
      /* Don't attempt to load properties if the name_owner is NULL (which
       * usually means the name isn't owned), unless name is also NULL (which
       * means we actually wanted to talk to the directly-connected process -
       * either dbus-daemon or a peer - instead of going via dbus-daemon)
       */
        get_all = FALSE;
    }

  if (get_all)
    {
      /* load all properties asynchronously */
      g_dbus_connection_call (proxy->priv->connection,
                              name_owner,
                              proxy->priv->object_path,
                              "org.freedesktop.DBus.Properties",
                              "GetAll",
                              g_variant_new ("(s)", proxy->priv->interface_name),
                              G_VARIANT_TYPE ("(a{sv})"),
                              G_DBUS_CALL_FLAGS_NONE,
                              -1,           /* timeout */
                              g_task_get_cancellable (task),
                              (GAsyncReadyCallback) async_init_get_all_cb,
                              task);
    }
  else
    {
      g_task_return_pointer (task, NULL, NULL);
      g_object_unref (task);
    }
}

static void
async_init_get_name_owner_cb (GDBusConnection *connection,
                              GAsyncResult    *res,
                              gpointer         user_data)
{
  GTask *task = user_data;
  GError *error;
  GVariant *result;

  error = NULL;
  result = g_dbus_connection_call_finish (connection,
                                          res,
                                          &error);
  if (result == NULL)
    {
      if (error->domain == G_DBUS_ERROR &&
          error->code == G_DBUS_ERROR_NAME_HAS_NO_OWNER)
        {
          g_error_free (error);
          async_init_data_set_name_owner (task, NULL);
        }
      else
        {
          g_task_return_error (task, error);
          g_object_unref (task);
        }
    }
  else
    {
      /* borrowed from result to avoid an extra copy */
      const gchar *name_owner;

      g_variant_get (result, "(&s)", &name_owner);
      async_init_data_set_name_owner (task, name_owner);
      g_variant_unref (result);
    }
}

static void
async_init_call_get_name_owner (GTask *task)
{
  GDBusProxy *proxy = g_task_get_source_object (task);

  g_dbus_connection_call (proxy->priv->connection,
                          "org.freedesktop.DBus",  /* name */
                          "/org/freedesktop/DBus", /* object path */
                          "org.freedesktop.DBus",  /* interface */
                          "GetNameOwner",
                          g_variant_new ("(s)",
                                         proxy->priv->name),
                          G_VARIANT_TYPE ("(s)"),
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,           /* timeout */
                          g_task_get_cancellable (task),
                          (GAsyncReadyCallback) async_init_get_name_owner_cb,
                          task);
}

static void
async_init_start_service_by_name_cb (GDBusConnection *connection,
                                     GAsyncResult    *res,
                                     gpointer         user_data)
{
  GTask *task = user_data;
  GDBusProxy *proxy = g_task_get_source_object (task);
  GError *error;
  GVariant *result;

  error = NULL;
  result = g_dbus_connection_call_finish (connection,
                                          res,
                                          &error);
  if (result == NULL)
    {
      /* Errors are not unexpected; the bus will reply e.g.
       *
       *   org.freedesktop.DBus.Error.ServiceUnknown: The name org.gnome.Epiphany2
       *   was not provided by any .service files
       *
       * or (see #677718)
       *
       *   org.freedesktop.systemd1.Masked: Unit polkit.service is masked.
       *
       * This doesn't mean that the name doesn't have an owner, just
       * that it's not provided by a .service file or can't currently
       * be started.
       *
       * In particular, in both cases, it could be that a service
       * owner will actually appear later. So instead of erroring out,
       * we just proceed to invoke GetNameOwner() if dealing with the
       * kind of errors above.
       */
      if (error->domain == G_DBUS_ERROR && error->code == G_DBUS_ERROR_SERVICE_UNKNOWN)
        {
          g_error_free (error);
        }
      else
        {
          gchar *remote_error = g_dbus_error_get_remote_error (error);
          if (g_strcmp0 (remote_error, "org.freedesktop.systemd1.Masked") == 0)
            {
              g_error_free (error);
              g_free (remote_error);
            }
          else
            {
              g_prefix_error (&error,
                              _("Error calling StartServiceByName for %s: "),
                              proxy->priv->name);
              g_free (remote_error);
              goto failed;
            }
        }
    }
  else
    {
      guint32 start_service_result;
      g_variant_get (result,
                     "(u)",
                     &start_service_result);
      g_variant_unref (result);
      if (start_service_result == 1 ||  /* DBUS_START_REPLY_SUCCESS */
          start_service_result == 2)    /* DBUS_START_REPLY_ALREADY_RUNNING */
        {
          /* continue to invoke GetNameOwner() */
        }
      else
        {
          error = g_error_new (G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               _("Unexpected reply %d from StartServiceByName(\"%s\") method"),
                               start_service_result,
                               proxy->priv->name);
          goto failed;
        }
    }

  async_init_call_get_name_owner (task);
  return;

 failed:
  g_warn_if_fail (error != NULL);
  g_task_return_error (task, error);
  g_object_unref (task);
}

static void
async_init_call_start_service_by_name (GTask *task)
{
  GDBusProxy *proxy = g_task_get_source_object (task);

  g_dbus_connection_call (proxy->priv->connection,
                          "org.freedesktop.DBus",  /* name */
                          "/org/freedesktop/DBus", /* object path */
                          "org.freedesktop.DBus",  /* interface */
                          "StartServiceByName",
                          g_variant_new ("(su)",
                                         proxy->priv->name,
                                         0),
                          G_VARIANT_TYPE ("(u)"),
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,           /* timeout */
                          g_task_get_cancellable (task),
                          (GAsyncReadyCallback) async_init_start_service_by_name_cb,
                          task);
}

static void
async_initable_init_second_async (GAsyncInitable      *initable,
                                  gint                 io_priority,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data)
{
  GDBusProxy *proxy = G_DBUS_PROXY (initable);
  GTask *task;

  task = g_task_new (proxy, cancellable, callback, user_data);
  g_task_set_source_tag (task, async_initable_init_second_async);
  g_task_set_priority (task, io_priority);

  /* Check name ownership asynchronously - possibly also start the service */
  if (proxy->priv->name == NULL)
    {
      /* Do nothing */
      async_init_data_set_name_owner (task, NULL);
    }
  else if (g_dbus_is_unique_name (proxy->priv->name))
    {
      async_init_data_set_name_owner (task, proxy->priv->name);
    }
  else
    {
      if ((proxy->priv->flags & G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START) ||
          (proxy->priv->flags & G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START_AT_CONSTRUCTION))
        {
          async_init_call_get_name_owner (task);
        }
      else
        {
          async_init_call_start_service_by_name (task);
        }
    }
}

static gboolean
async_initable_init_second_finish (GAsyncInitable  *initable,
                                   GAsyncResult    *res,
                                   GError         **error)
{
  GDBusProxy *proxy = G_DBUS_PROXY (initable);
  GTask *task = G_TASK (res);
  GVariant *result;
  gboolean ret;

  ret = !g_task_had_error (task);

  result = g_task_propagate_pointer (task, error);
  if (result != NULL)
    {
      process_get_all_reply (proxy, result);
      g_variant_unref (result);
    }

  proxy->priv->initialized = TRUE;
  return ret;
}

/* ---------------------------------------------------------------------------------------------------- */

static void
async_initable_init_first (GAsyncInitable *initable)
{
  GDBusProxy *proxy = G_DBUS_PROXY (initable);

  if (!(proxy->priv->flags & G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES))
    {
      /* subscribe to PropertiesChanged() */
      proxy->priv->properties_changed_subscription_id =
        g_dbus_connection_signal_subscribe (proxy->priv->connection,
                                            proxy->priv->name,
                                            "org.freedesktop.DBus.Properties",
                                            "PropertiesChanged",
                                            proxy->priv->object_path,
                                            proxy->priv->interface_name,
                                            G_DBUS_SIGNAL_FLAGS_NONE,
                                            on_properties_changed,
                                            signal_subscription_ref (proxy->priv->signal_subscription_data),
                                            (GDestroyNotify) signal_subscription_unref);
    }

  if (!(proxy->priv->flags & G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS))
    {
      /* subscribe to all signals for the object */
      proxy->priv->signals_subscription_id =
        g_dbus_connection_signal_subscribe (proxy->priv->connection,
                                            proxy->priv->name,
                                            proxy->priv->interface_name,
                                            NULL,                        /* member */
                                            proxy->priv->object_path,
                                            NULL,                        /* arg0 */
                                            G_DBUS_SIGNAL_FLAGS_NONE,
                                            on_signal_received,
                                            signal_subscription_ref (proxy->priv->signal_subscription_data),
                                            (GDestroyNotify) signal_subscription_unref);
    }

  if (proxy->priv->name != NULL && !g_dbus_is_unique_name (proxy->priv->name))
    {
      proxy->priv->name_owner_changed_subscription_id =
        g_dbus_connection_signal_subscribe (proxy->priv->connection,
                                            "org.freedesktop.DBus",  /* name */
                                            "org.freedesktop.DBus",  /* interface */
                                            "NameOwnerChanged",      /* signal name */
                                            "/org/freedesktop/DBus", /* path */
                                            proxy->priv->name,       /* arg0 */
                                            G_DBUS_SIGNAL_FLAGS_NONE,
                                            on_name_owner_changed,
                                            signal_subscription_ref (proxy->priv->signal_subscription_data),
                                            (GDestroyNotify) signal_subscription_unref);
    }
}

/* ---------------------------------------------------------------------------------------------------- */

/* initialization is split into two parts - the first is the
 * non-blocking part that requires the callers GMainContext - the
 * second is a blocking part async part that doesn't require the
 * callers GMainContext.. we do this split so the code can be reused
 * in the GInitable implementation below.
 *
 * Note that obtaining a GDBusConnection is not shared between the two
 * paths.
 */

static void
init_second_async_cb (GObject       *source_object,
		      GAsyncResult  *res,
		      gpointer       user_data)
{
  GTask *task = user_data;
  GError *error = NULL;

  if (async_initable_init_second_finish (G_ASYNC_INITABLE (source_object), res, &error))
    g_task_return_boolean (task, TRUE);
  else
    g_task_return_error (task, error);
  g_object_unref (task);
}

static void
get_connection_cb (GObject       *source_object,
                   GAsyncResult  *res,
                   gpointer       user_data)
{
  GTask *task = user_data;
  GDBusProxy *proxy = g_task_get_source_object (task);
  GError *error;

  error = NULL;
  proxy->priv->connection = g_bus_get_finish (res, &error);
  if (proxy->priv->connection == NULL)
    {
      g_task_return_error (task, error);
      g_object_unref (task);
    }
  else
    {
      async_initable_init_first (G_ASYNC_INITABLE (proxy));
      async_initable_init_second_async (G_ASYNC_INITABLE (proxy),
                                        g_task_get_priority (task),
                                        g_task_get_cancellable (task),
                                        init_second_async_cb,
                                        task);
    }
}

static void
async_initable_init_async (GAsyncInitable      *initable,
                           gint                 io_priority,
                           GCancellable        *cancellable,
                           GAsyncReadyCallback  callback,
                           gpointer             user_data)
{
  GDBusProxy *proxy = G_DBUS_PROXY (initable);
  GTask *task;

  task = g_task_new (proxy, cancellable, callback, user_data);
  g_task_set_source_tag (task, async_initable_init_async);
  g_task_set_priority (task, io_priority);

  if (proxy->priv->bus_type != G_BUS_TYPE_NONE)
    {
      g_assert (proxy->priv->connection == NULL);

      g_bus_get (proxy->priv->bus_type,
                 cancellable,
                 get_connection_cb,
                 task);
    }
  else
    {
      async_initable_init_first (initable);
      async_initable_init_second_async (initable, io_priority, cancellable,
                                        init_second_async_cb, task);
    }
}

static gboolean
async_initable_init_finish (GAsyncInitable  *initable,
                            GAsyncResult    *res,
                            GError         **error)
{
  return g_task_propagate_boolean (G_TASK (res), error);
}

static void
async_initable_iface_init (GAsyncInitableIface *async_initable_iface)
{
  async_initable_iface->init_async = async_initable_init_async;
  async_initable_iface->init_finish = async_initable_init_finish;
}

/* ---------------------------------------------------------------------------------------------------- */

typedef struct
{
  GMainContext *context;
  GMainLoop *loop;
  GAsyncResult *res;
} InitableAsyncInitableData;

static void
async_initable_init_async_cb (GObject      *source_object,
                              GAsyncResult *res,
                              gpointer      user_data)
{
  InitableAsyncInitableData *data = user_data;
  data->res = g_object_ref (res);
  g_main_loop_quit (data->loop);
}

/* Simply reuse the GAsyncInitable implementation but run the first
 * part (that is non-blocking and requires the callers GMainContext)
 * with the callers GMainContext.. and the second with a private
 * GMainContext (bug 621310 is slightly related).
 *
 * Note that obtaining a GDBusConnection is not shared between the two
 * paths.
 */
static gboolean
initable_init (GInitable     *initable,
               GCancellable  *cancellable,
               GError       **error)
{
  GDBusProxy *proxy = G_DBUS_PROXY (initable);
  InitableAsyncInitableData *data;
  gboolean ret;

  ret = FALSE;

  if (proxy->priv->bus_type != G_BUS_TYPE_NONE)
    {
      g_assert (proxy->priv->connection == NULL);
      proxy->priv->connection = g_bus_get_sync (proxy->priv->bus_type,
                                                cancellable,
                                                error);
      if (proxy->priv->connection == NULL)
        goto out;
    }

  async_initable_init_first (G_ASYNC_INITABLE (initable));

  data = g_new0 (InitableAsyncInitableData, 1);
  data->context = g_main_context_new ();
  data->loop = g_main_loop_new (data->context, FALSE);

  g_main_context_push_thread_default (data->context);

  async_initable_init_second_async (G_ASYNC_INITABLE (initable),
                                    G_PRIORITY_DEFAULT,
                                    cancellable,
                                    async_initable_init_async_cb,
                                    data);

  g_main_loop_run (data->loop);

  ret = async_initable_init_second_finish (G_ASYNC_INITABLE (initable),
                                           data->res,
                                           error);

  g_main_context_pop_thread_default (data->context);

  g_main_context_unref (data->context);
  g_main_loop_unref (data->loop);
  g_object_unref (data->res);
  g_free (data);

 out:

  return ret;
}

static void
initable_iface_init (GInitableIface *initable_iface)
{
  initable_iface->init = initable_init;
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_dbus_proxy_new:
 * @connection: A #GDBusConnection.
 * @flags: Flags used when constructing the proxy.
 * @info: (nullable): A #GDBusInterfaceInfo specifying the minimal interface that @proxy conforms to or %NULL.
 * @name: (nullable): A bus name (well-known or unique) or %NULL if @connection is not a message bus connection.
 * @object_path: An object path.
 * @interface_name: A D-Bus interface name.
 * @cancellable: (nullable): A #GCancellable or %NULL.
 * @callback: Callback function to invoke when the proxy is ready.
 * @user_data: User data to pass to @callback.
 *
 * Creates a proxy for accessing @interface_name on the remote object
 * at @object_path owned by @name at @connection and asynchronously
 * loads D-Bus properties unless the
 * %G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES flag is used. Connect to
 * the #GDBusProxy::g-properties-changed signal to get notified about
 * property changes.
 *
 * If the %G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS flag is not set, also sets up
 * match rules for signals. Connect to the #GDBusProxy::g-signal signal
 * to handle signals from the remote object.
 *
 * If @name is a well-known name and the
 * %G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START and %G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START_AT_CONSTRUCTION
 * flags aren't set and no name owner currently exists, the message bus
 * will be requested to launch a name owner for the name.
 *
 * This is a failable asynchronous constructor - when the proxy is
 * ready, @callback will be invoked and you can use
 * g_dbus_proxy_new_finish() to get the result.
 *
 * See g_dbus_proxy_new_sync() and for a synchronous version of this constructor.
 *
 * #GDBusProxy is used in this [example][gdbus-wellknown-proxy].
 *
 * Since: 2.26
 */
void
g_dbus_proxy_new (GDBusConnection     *connection,
                  GDBusProxyFlags      flags,
                  GDBusInterfaceInfo  *info,
                  const gchar         *name,
                  const gchar         *object_path,
                  const gchar         *interface_name,
                  GCancellable        *cancellable,
                  GAsyncReadyCallback  callback,
                  gpointer             user_data)
{
  _g_dbus_initialize ();

  g_return_if_fail (G_IS_DBUS_CONNECTION (connection));
  g_return_if_fail ((name == NULL && g_dbus_connection_get_unique_name (connection) == NULL) || g_dbus_is_name (name));
  g_return_if_fail (g_variant_is_object_path (object_path));
  g_return_if_fail (g_dbus_is_interface_name (interface_name));

  g_async_initable_new_async (G_TYPE_DBUS_PROXY,
                              G_PRIORITY_DEFAULT,
                              cancellable,
                              callback,
                              user_data,
                              "g-flags", flags,
                              "g-interface-info", info,
                              "g-name", name,
                              "g-connection", connection,
                              "g-object-path", object_path,
                              "g-interface-name", interface_name,
                              NULL);
}

/**
 * g_dbus_proxy_new_finish:
 * @res: A #GAsyncResult obtained from the #GAsyncReadyCallback function passed to g_dbus_proxy_new().
 * @error: Return location for error or %NULL.
 *
 * Finishes creating a #GDBusProxy.
 *
 * Returns: (transfer full): A #GDBusProxy or %NULL if @error is set.
 *    Free with g_object_unref().
 *
 * Since: 2.26
 */
GDBusProxy *
g_dbus_proxy_new_finish (GAsyncResult  *res,
                         GError       **error)
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
    return G_DBUS_PROXY (object);
  else
    return NULL;
}

/**
 * g_dbus_proxy_new_sync:
 * @connection: A #GDBusConnection.
 * @flags: Flags used when constructing the proxy.
 * @info: (nullable): A #GDBusInterfaceInfo specifying the minimal interface that @proxy conforms to or %NULL.
 * @name: (nullable): A bus name (well-known or unique) or %NULL if @connection is not a message bus connection.
 * @object_path: An object path.
 * @interface_name: A D-Bus interface name.
 * @cancellable: (nullable): A #GCancellable or %NULL.
 * @error: (nullable): Return location for error or %NULL.
 *
 * Creates a proxy for accessing @interface_name on the remote object
 * at @object_path owned by @name at @connection and synchronously
 * loads D-Bus properties unless the
 * %G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES flag is used.
 *
 * If the %G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS flag is not set, also sets up
 * match rules for signals. Connect to the #GDBusProxy::g-signal signal
 * to handle signals from the remote object.
 *
 * If @name is a well-known name and the
 * %G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START and %G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START_AT_CONSTRUCTION
 * flags aren't set and no name owner currently exists, the message bus
 * will be requested to launch a name owner for the name.
 *
 * This is a synchronous failable constructor. See g_dbus_proxy_new()
 * and g_dbus_proxy_new_finish() for the asynchronous version.
 *
 * #GDBusProxy is used in this [example][gdbus-wellknown-proxy].
 *
 * Returns: (transfer full): A #GDBusProxy or %NULL if error is set.
 *    Free with g_object_unref().
 *
 * Since: 2.26
 */
GDBusProxy *
g_dbus_proxy_new_sync (GDBusConnection     *connection,
                       GDBusProxyFlags      flags,
                       GDBusInterfaceInfo  *info,
                       const gchar         *name,
                       const gchar         *object_path,
                       const gchar         *interface_name,
                       GCancellable        *cancellable,
                       GError             **error)
{
  GInitable *initable;

  g_return_val_if_fail (G_IS_DBUS_CONNECTION (connection), NULL);
  g_return_val_if_fail ((name == NULL && g_dbus_connection_get_unique_name (connection) == NULL) ||
                        g_dbus_is_name (name), NULL);
  g_return_val_if_fail (g_variant_is_object_path (object_path), NULL);
  g_return_val_if_fail (g_dbus_is_interface_name (interface_name), NULL);

  initable = g_initable_new (G_TYPE_DBUS_PROXY,
                             cancellable,
                             error,
                             "g-flags", flags,
                             "g-interface-info", info,
                             "g-name", name,
                             "g-connection", connection,
                             "g-object-path", object_path,
                             "g-interface-name", interface_name,
                             NULL);
  if (initable != NULL)
    return G_DBUS_PROXY (initable);
  else
    return NULL;
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_dbus_proxy_new_for_bus:
 * @bus_type: A #GBusType.
 * @flags: Flags used when constructing the proxy.
 * @info: (nullable): A #GDBusInterfaceInfo specifying the minimal interface that @proxy conforms to or %NULL.
 * @name: A bus name (well-known or unique).
 * @object_path: An object path.
 * @interface_name: A D-Bus interface name.
 * @cancellable: (nullable): A #GCancellable or %NULL.
 * @callback: Callback function to invoke when the proxy is ready.
 * @user_data: User data to pass to @callback.
 *
 * Like g_dbus_proxy_new() but takes a #GBusType instead of a #GDBusConnection.
 *
 * #GDBusProxy is used in this [example][gdbus-wellknown-proxy].
 *
 * Since: 2.26
 */
void
g_dbus_proxy_new_for_bus (GBusType             bus_type,
                          GDBusProxyFlags      flags,
                          GDBusInterfaceInfo  *info,
                          const gchar         *name,
                          const gchar         *object_path,
                          const gchar         *interface_name,
                          GCancellable        *cancellable,
                          GAsyncReadyCallback  callback,
                          gpointer             user_data)
{
  _g_dbus_initialize ();

  g_return_if_fail (g_dbus_is_name (name));
  g_return_if_fail (g_variant_is_object_path (object_path));
  g_return_if_fail (g_dbus_is_interface_name (interface_name));

  g_async_initable_new_async (G_TYPE_DBUS_PROXY,
                              G_PRIORITY_DEFAULT,
                              cancellable,
                              callback,
                              user_data,
                              "g-flags", flags,
                              "g-interface-info", info,
                              "g-name", name,
                              "g-bus-type", bus_type,
                              "g-object-path", object_path,
                              "g-interface-name", interface_name,
                              NULL);
}

/**
 * g_dbus_proxy_new_for_bus_finish:
 * @res: A #GAsyncResult obtained from the #GAsyncReadyCallback function passed to g_dbus_proxy_new_for_bus().
 * @error: Return location for error or %NULL.
 *
 * Finishes creating a #GDBusProxy.
 *
 * Returns: (transfer full): A #GDBusProxy or %NULL if @error is set.
 *    Free with g_object_unref().
 *
 * Since: 2.26
 */
GDBusProxy *
g_dbus_proxy_new_for_bus_finish (GAsyncResult  *res,
                                 GError       **error)
{
  return g_dbus_proxy_new_finish (res, error);
}

/**
 * g_dbus_proxy_new_for_bus_sync:
 * @bus_type: A #GBusType.
 * @flags: Flags used when constructing the proxy.
 * @info: (nullable): A #GDBusInterfaceInfo specifying the minimal interface
 *        that @proxy conforms to or %NULL.
 * @name: A bus name (well-known or unique).
 * @object_path: An object path.
 * @interface_name: A D-Bus interface name.
 * @cancellable: (nullable): A #GCancellable or %NULL.
 * @error: Return location for error or %NULL.
 *
 * Like g_dbus_proxy_new_sync() but takes a #GBusType instead of a #GDBusConnection.
 *
 * #GDBusProxy is used in this [example][gdbus-wellknown-proxy].
 *
 * Returns: (transfer full): A #GDBusProxy or %NULL if error is set.
 *    Free with g_object_unref().
 *
 * Since: 2.26
 */
GDBusProxy *
g_dbus_proxy_new_for_bus_sync (GBusType             bus_type,
                               GDBusProxyFlags      flags,
                               GDBusInterfaceInfo  *info,
                               const gchar         *name,
                               const gchar         *object_path,
                               const gchar         *interface_name,
                               GCancellable        *cancellable,
                               GError             **error)
{
  GInitable *initable;

  _g_dbus_initialize ();

  g_return_val_if_fail (g_dbus_is_name (name), NULL);
  g_return_val_if_fail (g_variant_is_object_path (object_path), NULL);
  g_return_val_if_fail (g_dbus_is_interface_name (interface_name), NULL);

  initable = g_initable_new (G_TYPE_DBUS_PROXY,
                             cancellable,
                             error,
                             "g-flags", flags,
                             "g-interface-info", info,
                             "g-name", name,
                             "g-bus-type", bus_type,
                             "g-object-path", object_path,
                             "g-interface-name", interface_name,
                             NULL);
  if (initable != NULL)
    return G_DBUS_PROXY (initable);
  else
    return NULL;
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_dbus_proxy_get_connection:
 * @proxy: A #GDBusProxy.
 *
 * Gets the connection @proxy is for.
 *
 * Returns: (transfer none): A #GDBusConnection owned by @proxy. Do not free.
 *
 * Since: 2.26
 */
GDBusConnection *
g_dbus_proxy_get_connection (GDBusProxy *proxy)
{
  g_return_val_if_fail (G_IS_DBUS_PROXY (proxy), NULL);
  return proxy->priv->connection;
}

/**
 * g_dbus_proxy_get_flags:
 * @proxy: A #GDBusProxy.
 *
 * Gets the flags that @proxy was constructed with.
 *
 * Returns: Flags from the #GDBusProxyFlags enumeration.
 *
 * Since: 2.26
 */
GDBusProxyFlags
g_dbus_proxy_get_flags (GDBusProxy *proxy)
{
  g_return_val_if_fail (G_IS_DBUS_PROXY (proxy), 0);
  return proxy->priv->flags;
}

/**
 * g_dbus_proxy_get_name:
 * @proxy: A #GDBusProxy.
 *
 * Gets the name that @proxy was constructed for.
 *
 * Returns: A string owned by @proxy. Do not free.
 *
 * Since: 2.26
 */
const gchar *
g_dbus_proxy_get_name (GDBusProxy *proxy)
{
  g_return_val_if_fail (G_IS_DBUS_PROXY (proxy), NULL);
  return proxy->priv->name;
}

/**
 * g_dbus_proxy_get_name_owner:
 * @proxy: A #GDBusProxy.
 *
 * The unique name that owns the name that @proxy is for or %NULL if
 * no-one currently owns that name. You may connect to the
 * #GObject::notify signal to track changes to the
 * #GDBusProxy:g-name-owner property.
 *
 * Returns: (transfer full) (nullable): The name owner or %NULL if no name
 *    owner exists. Free with g_free().
 *
 * Since: 2.26
 */
gchar *
g_dbus_proxy_get_name_owner (GDBusProxy *proxy)
{
  gchar *ret;

  g_return_val_if_fail (G_IS_DBUS_PROXY (proxy), NULL);

  G_LOCK (properties_lock);
  ret = g_strdup (proxy->priv->name_owner);
  G_UNLOCK (properties_lock);
  return ret;
}

/**
 * g_dbus_proxy_get_object_path:
 * @proxy: A #GDBusProxy.
 *
 * Gets the object path @proxy is for.
 *
 * Returns: A string owned by @proxy. Do not free.
 *
 * Since: 2.26
 */
const gchar *
g_dbus_proxy_get_object_path (GDBusProxy *proxy)
{
  g_return_val_if_fail (G_IS_DBUS_PROXY (proxy), NULL);
  return proxy->priv->object_path;
}

/**
 * g_dbus_proxy_get_interface_name:
 * @proxy: A #GDBusProxy.
 *
 * Gets the D-Bus interface name @proxy is for.
 *
 * Returns: A string owned by @proxy. Do not free.
 *
 * Since: 2.26
 */
const gchar *
g_dbus_proxy_get_interface_name (GDBusProxy *proxy)
{
  g_return_val_if_fail (G_IS_DBUS_PROXY (proxy), NULL);
  return proxy->priv->interface_name;
}

/**
 * g_dbus_proxy_get_default_timeout:
 * @proxy: A #GDBusProxy.
 *
 * Gets the timeout to use if -1 (specifying default timeout) is
 * passed as @timeout_msec in the g_dbus_proxy_call() and
 * g_dbus_proxy_call_sync() functions.
 *
 * See the #GDBusProxy:g-default-timeout property for more details.
 *
 * Returns: Timeout to use for @proxy.
 *
 * Since: 2.26
 */
gint
g_dbus_proxy_get_default_timeout (GDBusProxy *proxy)
{
  gint ret;

  g_return_val_if_fail (G_IS_DBUS_PROXY (proxy), -1);

  G_LOCK (properties_lock);
  ret = proxy->priv->timeout_msec;
  G_UNLOCK (properties_lock);
  return ret;
}

/**
 * g_dbus_proxy_set_default_timeout:
 * @proxy: A #GDBusProxy.
 * @timeout_msec: Timeout in milliseconds.
 *
 * Sets the timeout to use if -1 (specifying default timeout) is
 * passed as @timeout_msec in the g_dbus_proxy_call() and
 * g_dbus_proxy_call_sync() functions.
 *
 * See the #GDBusProxy:g-default-timeout property for more details.
 *
 * Since: 2.26
 */
void
g_dbus_proxy_set_default_timeout (GDBusProxy *proxy,
                                  gint        timeout_msec)
{
  g_return_if_fail (G_IS_DBUS_PROXY (proxy));
  g_return_if_fail (timeout_msec == -1 || timeout_msec >= 0);

  G_LOCK (properties_lock);

  if (proxy->priv->timeout_msec != timeout_msec)
    {
      proxy->priv->timeout_msec = timeout_msec;
      G_UNLOCK (properties_lock);

      g_object_notify (G_OBJECT (proxy), "g-default-timeout");
    }
  else
    {
      G_UNLOCK (properties_lock);
    }
}

/**
 * g_dbus_proxy_get_interface_info:
 * @proxy: A #GDBusProxy
 *
 * Returns the #GDBusInterfaceInfo, if any, specifying the interface
 * that @proxy conforms to. See the #GDBusProxy:g-interface-info
 * property for more details.
 *
 * Returns: (transfer none) (nullable): A #GDBusInterfaceInfo or %NULL.
 *    Do not unref the returned object, it is owned by @proxy.
 *
 * Since: 2.26
 */
GDBusInterfaceInfo *
g_dbus_proxy_get_interface_info (GDBusProxy *proxy)
{
  GDBusInterfaceInfo *ret;

  g_return_val_if_fail (G_IS_DBUS_PROXY (proxy), NULL);

  G_LOCK (properties_lock);
  ret = proxy->priv->expected_interface;
  G_UNLOCK (properties_lock);
  /* FIXME: returning a borrowed ref with no guarantee that nobody will
   * call g_dbus_proxy_set_interface_info() and make it invalid...
   */
  return ret;
}

/**
 * g_dbus_proxy_set_interface_info:
 * @proxy: A #GDBusProxy
 * @info: (transfer none) (nullable): Minimum interface this proxy conforms to
 *    or %NULL to unset.
 *
 * Ensure that interactions with @proxy conform to the given
 * interface. See the #GDBusProxy:g-interface-info property for more
 * details.
 *
 * Since: 2.26
 */
void
g_dbus_proxy_set_interface_info (GDBusProxy         *proxy,
                                 GDBusInterfaceInfo *info)
{
  g_return_if_fail (G_IS_DBUS_PROXY (proxy));
  G_LOCK (properties_lock);

  if (proxy->priv->expected_interface != NULL)
    {
      g_dbus_interface_info_cache_release (proxy->priv->expected_interface);
      g_dbus_interface_info_unref (proxy->priv->expected_interface);
    }
  proxy->priv->expected_interface = info != NULL ? g_dbus_interface_info_ref (info) : NULL;
  if (proxy->priv->expected_interface != NULL)
    g_dbus_interface_info_cache_build (proxy->priv->expected_interface);

  G_UNLOCK (properties_lock);
}

/* ---------------------------------------------------------------------------------------------------- */

static gboolean
maybe_split_method_name (const gchar  *method_name,
                         gchar       **out_interface_name,
                         const gchar **out_method_name)
{
  gboolean was_split;

  was_split = FALSE;
  g_assert (out_interface_name != NULL);
  g_assert (out_method_name != NULL);
  *out_interface_name = NULL;
  *out_method_name = NULL;

  if (strchr (method_name, '.') != NULL)
    {
      gchar *p;
      gchar *last_dot;

      p = g_strdup (method_name);
      last_dot = strrchr (p, '.');
      *last_dot = '\0';

      *out_interface_name = p;
      *out_method_name = last_dot + 1;

      was_split = TRUE;
    }

  return was_split;
}

typedef struct
{
  GVariant *value;
#ifdef G_OS_UNIX
  GUnixFDList *fd_list;
#endif
} ReplyData;

static void
reply_data_free (ReplyData *data)
{
  g_variant_unref (data->value);
#ifdef G_OS_UNIX
  if (data->fd_list != NULL)
    g_object_unref (data->fd_list);
#endif
  g_slice_free (ReplyData, data);
}

static void
reply_cb (GDBusConnection *connection,
          GAsyncResult    *res,
          gpointer         user_data)
{
  GTask *task = user_data;
  GVariant *value;
  GError *error;
#ifdef G_OS_UNIX
  GUnixFDList *fd_list;
#endif

  error = NULL;
#ifdef G_OS_UNIX
  value = g_dbus_connection_call_with_unix_fd_list_finish (connection,
                                                           &fd_list,
                                                           res,
                                                           &error);
#else
  value = g_dbus_connection_call_finish (connection,
                                         res,
                                         &error);
#endif
  if (error != NULL)
    {
      g_task_return_error (task, error);
    }
  else
    {
      ReplyData *data;
      data = g_slice_new0 (ReplyData);
      data->value = value;
#ifdef G_OS_UNIX
      data->fd_list = fd_list;
#endif
      g_task_return_pointer (task, data, (GDestroyNotify) reply_data_free);
    }

  g_object_unref (task);
}

/* properties_lock must be held for as long as you will keep the
 * returned value
 */
static const GDBusMethodInfo *
lookup_method_info (GDBusProxy  *proxy,
                    const gchar *method_name)
{
  const GDBusMethodInfo *info = NULL;

  if (proxy->priv->expected_interface == NULL)
    goto out;

  info = g_dbus_interface_info_lookup_method (proxy->priv->expected_interface, method_name);

out:
  return info;
}

/* properties_lock must be held for as long as you will keep the
 * returned value
 */
static const gchar *
get_destination_for_call (GDBusProxy *proxy)
{
  const gchar *ret;

  ret = NULL;

  /* If proxy->priv->name is a unique name, then proxy->priv->name_owner
   * is never NULL and always the same as proxy->priv->name. We use this
   * knowledge to avoid checking if proxy->priv->name is a unique or
   * well-known name.
   */
  ret = proxy->priv->name_owner;
  if (ret != NULL)
    goto out;

  if (proxy->priv->flags & G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START)
    goto out;

  ret = proxy->priv->name;

 out:
  return ret;
}

/* ---------------------------------------------------------------------------------------------------- */

static void
g_dbus_proxy_call_internal (GDBusProxy          *proxy,
                            const gchar         *method_name,
                            GVariant            *parameters,
                            GDBusCallFlags       flags,
                            gint                 timeout_msec,
                            GUnixFDList         *fd_list,
                            GCancellable        *cancellable,
                            GAsyncReadyCallback  callback,
                            gpointer             user_data)
{
  GTask *task;
  gboolean was_split;
  gchar *split_interface_name;
  const gchar *split_method_name;
  const gchar *target_method_name;
  const gchar *target_interface_name;
  gchar *destination;
  GVariantType *reply_type;
  GAsyncReadyCallback my_callback;

  g_return_if_fail (G_IS_DBUS_PROXY (proxy));
  g_return_if_fail (g_dbus_is_member_name (method_name) || g_dbus_is_interface_name (method_name));
  g_return_if_fail (parameters == NULL || g_variant_is_of_type (parameters, G_VARIANT_TYPE_TUPLE));
  g_return_if_fail (timeout_msec == -1 || timeout_msec >= 0);
#ifdef G_OS_UNIX
  g_return_if_fail (fd_list == NULL || G_IS_UNIX_FD_LIST (fd_list));
#else
  g_return_if_fail (fd_list == NULL);
#endif

  reply_type = NULL;
  split_interface_name = NULL;

  /* g_dbus_connection_call() is optimised for the case of a NULL
   * callback.  If we get a NULL callback from our user then make sure
   * we pass along a NULL callback for ourselves as well.
   */
  if (callback != NULL)
    {
      my_callback = (GAsyncReadyCallback) reply_cb;
      task = g_task_new (proxy, cancellable, callback, user_data);
      g_task_set_source_tag (task, g_dbus_proxy_call_internal);
    }
  else
    {
      my_callback = NULL;
      task = NULL;
    }

  G_LOCK (properties_lock);

  was_split = maybe_split_method_name (method_name, &split_interface_name, &split_method_name);
  target_method_name = was_split ? split_method_name : method_name;
  target_interface_name = was_split ? split_interface_name : proxy->priv->interface_name;

  /* Warn if method is unexpected (cf. :g-interface-info) */
  if (!was_split)
    {
      const GDBusMethodInfo *expected_method_info;
      expected_method_info = lookup_method_info (proxy, target_method_name);
      if (expected_method_info != NULL)
        reply_type = _g_dbus_compute_complete_signature (expected_method_info->out_args);
    }

  destination = NULL;
  if (proxy->priv->name != NULL)
    {
      destination = g_strdup (get_destination_for_call (proxy));
      if (destination == NULL)
        {
          if (task != NULL)
            {
              g_task_return_new_error (task,
                                       G_IO_ERROR,
                                       G_IO_ERROR_FAILED,
                                       _("Cannot invoke method; proxy is for a well-known name without an owner and proxy was constructed with the G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START flag"));
              g_object_unref (task);
            }
          G_UNLOCK (properties_lock);
          goto out;
        }
    }

  G_UNLOCK (properties_lock);

#ifdef G_OS_UNIX
  g_dbus_connection_call_with_unix_fd_list (proxy->priv->connection,
                                            destination,
                                            proxy->priv->object_path,
                                            target_interface_name,
                                            target_method_name,
                                            parameters,
                                            reply_type,
                                            flags,
                                            timeout_msec == -1 ? proxy->priv->timeout_msec : timeout_msec,
                                            fd_list,
                                            cancellable,
                                            my_callback,
                                            task);
#else
  g_dbus_connection_call (proxy->priv->connection,
                          destination,
                          proxy->priv->object_path,
                          target_interface_name,
                          target_method_name,
                          parameters,
                          reply_type,
                          flags,
                          timeout_msec == -1 ? proxy->priv->timeout_msec : timeout_msec,
                          cancellable,
                          my_callback,
                          task);
#endif

 out:
  if (reply_type != NULL)
    g_variant_type_free (reply_type);

  g_free (destination);
  g_free (split_interface_name);
}

static GVariant *
g_dbus_proxy_call_finish_internal (GDBusProxy    *proxy,
                                   GUnixFDList  **out_fd_list,
                                   GAsyncResult  *res,
                                   GError       **error)
{
  GVariant *value;
  ReplyData *data;

  g_return_val_if_fail (G_IS_DBUS_PROXY (proxy), NULL);
  g_return_val_if_fail (g_task_is_valid (res, proxy), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  value = NULL;

  data = g_task_propagate_pointer (G_TASK (res), error);
  if (!data)
    goto out;

  value = g_variant_ref (data->value);
#ifdef G_OS_UNIX
  if (out_fd_list != NULL)
    *out_fd_list = data->fd_list != NULL ? g_object_ref (data->fd_list) : NULL;
#endif
  reply_data_free (data);

 out:
  return value;
}

static GVariant *
g_dbus_proxy_call_sync_internal (GDBusProxy      *proxy,
                                 const gchar     *method_name,
                                 GVariant        *parameters,
                                 GDBusCallFlags   flags,
                                 gint             timeout_msec,
                                 GUnixFDList     *fd_list,
                                 GUnixFDList    **out_fd_list,
                                 GCancellable    *cancellable,
                                 GError         **error)
{
  GVariant *ret;
  gboolean was_split;
  gchar *split_interface_name;
  const gchar *split_method_name;
  const gchar *target_method_name;
  const gchar *target_interface_name;
  gchar *destination;
  GVariantType *reply_type;

  g_return_val_if_fail (G_IS_DBUS_PROXY (proxy), NULL);
  g_return_val_if_fail (g_dbus_is_member_name (method_name) || g_dbus_is_interface_name (method_name), NULL);
  g_return_val_if_fail (parameters == NULL || g_variant_is_of_type (parameters, G_VARIANT_TYPE_TUPLE), NULL);
  g_return_val_if_fail (timeout_msec == -1 || timeout_msec >= 0, NULL);
#ifdef G_OS_UNIX
  g_return_val_if_fail (fd_list == NULL || G_IS_UNIX_FD_LIST (fd_list), NULL);
#else
  g_return_val_if_fail (fd_list == NULL, NULL);
#endif
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  reply_type = NULL;

  G_LOCK (properties_lock);

  was_split = maybe_split_method_name (method_name, &split_interface_name, &split_method_name);
  target_method_name = was_split ? split_method_name : method_name;
  target_interface_name = was_split ? split_interface_name : proxy->priv->interface_name;

  /* Warn if method is unexpected (cf. :g-interface-info) */
  if (!was_split)
    {
      const GDBusMethodInfo *expected_method_info;
      expected_method_info = lookup_method_info (proxy, target_method_name);
      if (expected_method_info != NULL)
        reply_type = _g_dbus_compute_complete_signature (expected_method_info->out_args);
    }

  destination = NULL;
  if (proxy->priv->name != NULL)
    {
      destination = g_strdup (get_destination_for_call (proxy));
      if (destination == NULL)
        {
          g_set_error_literal (error,
                               G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               _("Cannot invoke method; proxy is for a well-known name without an owner and proxy was constructed with the G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START flag"));
          ret = NULL;
          G_UNLOCK (properties_lock);
          goto out;
        }
    }

  G_UNLOCK (properties_lock);

#ifdef G_OS_UNIX
  ret = g_dbus_connection_call_with_unix_fd_list_sync (proxy->priv->connection,
                                                       destination,
                                                       proxy->priv->object_path,
                                                       target_interface_name,
                                                       target_method_name,
                                                       parameters,
                                                       reply_type,
                                                       flags,
                                                       timeout_msec == -1 ? proxy->priv->timeout_msec : timeout_msec,
                                                       fd_list,
                                                       out_fd_list,
                                                       cancellable,
                                                       error);
#else
  ret = g_dbus_connection_call_sync (proxy->priv->connection,
                                     destination,
                                     proxy->priv->object_path,
                                     target_interface_name,
                                     target_method_name,
                                     parameters,
                                     reply_type,
                                     flags,
                                     timeout_msec == -1 ? proxy->priv->timeout_msec : timeout_msec,
                                     cancellable,
                                     error);
#endif

 out:
  if (reply_type != NULL)
    g_variant_type_free (reply_type);

  g_free (destination);
  g_free (split_interface_name);

  return ret;
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_dbus_proxy_call:
 * @proxy: A #GDBusProxy.
 * @method_name: Name of method to invoke.
 * @parameters: (nullable): A #GVariant tuple with parameters for the signal or %NULL if not passing parameters.
 * @flags: Flags from the #GDBusCallFlags enumeration.
 * @timeout_msec: The timeout in milliseconds (with %G_MAXINT meaning
 *                "infinite") or -1 to use the proxy default timeout.
 * @cancellable: (nullable): A #GCancellable or %NULL.
 * @callback: (nullable): A #GAsyncReadyCallback to call when the request is satisfied or %NULL if you don't
 * care about the result of the method invocation.
 * @user_data: The data to pass to @callback.
 *
 * Asynchronously invokes the @method_name method on @proxy.
 *
 * If @method_name contains any dots, then @name is split into interface and
 * method name parts. This allows using @proxy for invoking methods on
 * other interfaces.
 *
 * If the #GDBusConnection associated with @proxy is closed then
 * the operation will fail with %G_IO_ERROR_CLOSED. If
 * @cancellable is canceled, the operation will fail with
 * %G_IO_ERROR_CANCELLED. If @parameters contains a value not
 * compatible with the D-Bus protocol, the operation fails with
 * %G_IO_ERROR_INVALID_ARGUMENT.
 *
 * If the @parameters #GVariant is floating, it is consumed. This allows
 * convenient 'inline' use of g_variant_new(), e.g.:
 * |[<!-- language="C" -->
 *  g_dbus_proxy_call (proxy,
 *                     "TwoStrings",
 *                     g_variant_new ("(ss)",
 *                                    "Thing One",
 *                                    "Thing Two"),
 *                     G_DBUS_CALL_FLAGS_NONE,
 *                     -1,
 *                     NULL,
 *                     (GAsyncReadyCallback) two_strings_done,
 *                     &data);
 * ]|
 *
 * If @proxy has an expected interface (see
 * #GDBusProxy:g-interface-info) and @method_name is referenced by it,
 * then the return value is checked against the return type.
 *
 * This is an asynchronous method. When the operation is finished,
 * @callback will be invoked in the
 * [thread-default main context][g-main-context-push-thread-default]
 * of the thread you are calling this method from.
 * You can then call g_dbus_proxy_call_finish() to get the result of
 * the operation. See g_dbus_proxy_call_sync() for the synchronous
 * version of this method.
 *
 * If @callback is %NULL then the D-Bus method call message will be sent with
 * the %G_DBUS_MESSAGE_FLAGS_NO_REPLY_EXPECTED flag set.
 *
 * Since: 2.26
 */
void
g_dbus_proxy_call (GDBusProxy          *proxy,
                   const gchar         *method_name,
                   GVariant            *parameters,
                   GDBusCallFlags       flags,
                   gint                 timeout_msec,
                   GCancellable        *cancellable,
                   GAsyncReadyCallback  callback,
                   gpointer             user_data)
{
  g_dbus_proxy_call_internal (proxy, method_name, parameters, flags, timeout_msec, NULL, cancellable, callback, user_data);
}

/**
 * g_dbus_proxy_call_finish:
 * @proxy: A #GDBusProxy.
 * @res: A #GAsyncResult obtained from the #GAsyncReadyCallback passed to g_dbus_proxy_call().
 * @error: Return location for error or %NULL.
 *
 * Finishes an operation started with g_dbus_proxy_call().
 *
 * Returns: %NULL if @error is set. Otherwise a #GVariant tuple with
 * return values. Free with g_variant_unref().
 *
 * Since: 2.26
 */
GVariant *
g_dbus_proxy_call_finish (GDBusProxy    *proxy,
                          GAsyncResult  *res,
                          GError       **error)
{
  return g_dbus_proxy_call_finish_internal (proxy, NULL, res, error);
}

/**
 * g_dbus_proxy_call_sync:
 * @proxy: A #GDBusProxy.
 * @method_name: Name of method to invoke.
 * @parameters: (nullable): A #GVariant tuple with parameters for the signal
 *              or %NULL if not passing parameters.
 * @flags: Flags from the #GDBusCallFlags enumeration.
 * @timeout_msec: The timeout in milliseconds (with %G_MAXINT meaning
 *                "infinite") or -1 to use the proxy default timeout.
 * @cancellable: (nullable): A #GCancellable or %NULL.
 * @error: Return location for error or %NULL.
 *
 * Synchronously invokes the @method_name method on @proxy.
 *
 * If @method_name contains any dots, then @name is split into interface and
 * method name parts. This allows using @proxy for invoking methods on
 * other interfaces.
 *
 * If the #GDBusConnection associated with @proxy is disconnected then
 * the operation will fail with %G_IO_ERROR_CLOSED. If
 * @cancellable is canceled, the operation will fail with
 * %G_IO_ERROR_CANCELLED. If @parameters contains a value not
 * compatible with the D-Bus protocol, the operation fails with
 * %G_IO_ERROR_INVALID_ARGUMENT.
 *
 * If the @parameters #GVariant is floating, it is consumed. This allows
 * convenient 'inline' use of g_variant_new(), e.g.:
 * |[<!-- language="C" -->
 *  g_dbus_proxy_call_sync (proxy,
 *                          "TwoStrings",
 *                          g_variant_new ("(ss)",
 *                                         "Thing One",
 *                                         "Thing Two"),
 *                          G_DBUS_CALL_FLAGS_NONE,
 *                          -1,
 *                          NULL,
 *                          &error);
 * ]|
 *
 * The calling thread is blocked until a reply is received. See
 * g_dbus_proxy_call() for the asynchronous version of this
 * method.
 *
 * If @proxy has an expected interface (see
 * #GDBusProxy:g-interface-info) and @method_name is referenced by it,
 * then the return value is checked against the return type.
 *
 * Returns: %NULL if @error is set. Otherwise a #GVariant tuple with
 * return values. Free with g_variant_unref().
 *
 * Since: 2.26
 */
GVariant *
g_dbus_proxy_call_sync (GDBusProxy      *proxy,
                        const gchar     *method_name,
                        GVariant        *parameters,
                        GDBusCallFlags   flags,
                        gint             timeout_msec,
                        GCancellable    *cancellable,
                        GError         **error)
{
  return g_dbus_proxy_call_sync_internal (proxy, method_name, parameters, flags, timeout_msec, NULL, NULL, cancellable, error);
}

/* ---------------------------------------------------------------------------------------------------- */

#ifdef G_OS_UNIX

/**
 * g_dbus_proxy_call_with_unix_fd_list:
 * @proxy: A #GDBusProxy.
 * @method_name: Name of method to invoke.
 * @parameters: (nullable): A #GVariant tuple with parameters for the signal or %NULL if not passing parameters.
 * @flags: Flags from the #GDBusCallFlags enumeration.
 * @timeout_msec: The timeout in milliseconds (with %G_MAXINT meaning
 *                "infinite") or -1 to use the proxy default timeout.
 * @fd_list: (nullable): A #GUnixFDList or %NULL.
 * @cancellable: (nullable): A #GCancellable or %NULL.
 * @callback: (nullable): A #GAsyncReadyCallback to call when the request is satisfied or %NULL if you don't
 * care about the result of the method invocation.
 * @user_data: The data to pass to @callback.
 *
 * Like g_dbus_proxy_call() but also takes a #GUnixFDList object.
 *
 * This method is only available on UNIX.
 *
 * Since: 2.30
 */
void
g_dbus_proxy_call_with_unix_fd_list (GDBusProxy          *proxy,
                                     const gchar         *method_name,
                                     GVariant            *parameters,
                                     GDBusCallFlags       flags,
                                     gint                 timeout_msec,
                                     GUnixFDList         *fd_list,
                                     GCancellable        *cancellable,
                                     GAsyncReadyCallback  callback,
                                     gpointer             user_data)
{
  g_dbus_proxy_call_internal (proxy, method_name, parameters, flags, timeout_msec, fd_list, cancellable, callback, user_data);
}

/**
 * g_dbus_proxy_call_with_unix_fd_list_finish:
 * @proxy: A #GDBusProxy.
 * @out_fd_list: (out) (optional): Return location for a #GUnixFDList or %NULL.
 * @res: A #GAsyncResult obtained from the #GAsyncReadyCallback passed to g_dbus_proxy_call_with_unix_fd_list().
 * @error: Return location for error or %NULL.
 *
 * Finishes an operation started with g_dbus_proxy_call_with_unix_fd_list().
 *
 * Returns: %NULL if @error is set. Otherwise a #GVariant tuple with
 * return values. Free with g_variant_unref().
 *
 * Since: 2.30
 */
GVariant *
g_dbus_proxy_call_with_unix_fd_list_finish (GDBusProxy    *proxy,
                                            GUnixFDList  **out_fd_list,
                                            GAsyncResult  *res,
                                            GError       **error)
{
  return g_dbus_proxy_call_finish_internal (proxy, out_fd_list, res, error);
}

/**
 * g_dbus_proxy_call_with_unix_fd_list_sync:
 * @proxy: A #GDBusProxy.
 * @method_name: Name of method to invoke.
 * @parameters: (nullable): A #GVariant tuple with parameters for the signal
 *              or %NULL if not passing parameters.
 * @flags: Flags from the #GDBusCallFlags enumeration.
 * @timeout_msec: The timeout in milliseconds (with %G_MAXINT meaning
 *                "infinite") or -1 to use the proxy default timeout.
 * @fd_list: (nullable): A #GUnixFDList or %NULL.
 * @out_fd_list: (out) (optional): Return location for a #GUnixFDList or %NULL.
 * @cancellable: (nullable): A #GCancellable or %NULL.
 * @error: Return location for error or %NULL.
 *
 * Like g_dbus_proxy_call_sync() but also takes and returns #GUnixFDList objects.
 *
 * This method is only available on UNIX.
 *
 * Returns: %NULL if @error is set. Otherwise a #GVariant tuple with
 * return values. Free with g_variant_unref().
 *
 * Since: 2.30
 */
GVariant *
g_dbus_proxy_call_with_unix_fd_list_sync (GDBusProxy      *proxy,
                                          const gchar     *method_name,
                                          GVariant        *parameters,
                                          GDBusCallFlags   flags,
                                          gint             timeout_msec,
                                          GUnixFDList     *fd_list,
                                          GUnixFDList    **out_fd_list,
                                          GCancellable    *cancellable,
                                          GError         **error)
{
  return g_dbus_proxy_call_sync_internal (proxy, method_name, parameters, flags, timeout_msec, fd_list, out_fd_list, cancellable, error);
}

#endif /* G_OS_UNIX */

/* ---------------------------------------------------------------------------------------------------- */

static GDBusInterfaceInfo *
_g_dbus_proxy_get_info (GDBusInterface *interface)
{
  GDBusProxy *proxy = G_DBUS_PROXY (interface);
  return g_dbus_proxy_get_interface_info (proxy);
}

static GDBusObject *
_g_dbus_proxy_get_object (GDBusInterface *interface)
{
  GDBusProxy *proxy = G_DBUS_PROXY (interface);
  return proxy->priv->object;
}

static GDBusObject *
_g_dbus_proxy_dup_object (GDBusInterface *interface)
{
  GDBusProxy *proxy = G_DBUS_PROXY (interface);
  GDBusObject *ret = NULL;

  G_LOCK (properties_lock);
  if (proxy->priv->object != NULL)
    ret = g_object_ref (proxy->priv->object);
  G_UNLOCK (properties_lock);
  return ret;
}

static void
_g_dbus_proxy_set_object (GDBusInterface *interface,
                          GDBusObject    *object)
{
  GDBusProxy *proxy = G_DBUS_PROXY (interface);
  G_LOCK (properties_lock);
  if (proxy->priv->object != NULL)
    g_object_remove_weak_pointer (G_OBJECT (proxy->priv->object), (gpointer *) &proxy->priv->object);
  proxy->priv->object = object;
  if (proxy->priv->object != NULL)
    g_object_add_weak_pointer (G_OBJECT (proxy->priv->object), (gpointer *) &proxy->priv->object);
  G_UNLOCK (properties_lock);
}

static void
dbus_interface_iface_init (GDBusInterfaceIface *dbus_interface_iface)
{
  dbus_interface_iface->get_info   = _g_dbus_proxy_get_info;
  dbus_interface_iface->get_object = _g_dbus_proxy_get_object;
  dbus_interface_iface->dup_object = _g_dbus_proxy_dup_object;
  dbus_interface_iface->set_object = _g_dbus_proxy_set_object;
}

/* ---------------------------------------------------------------------------------------------------- */
