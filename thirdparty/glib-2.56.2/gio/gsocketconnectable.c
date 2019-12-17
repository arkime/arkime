/* GIO - GLib Input, Output and Streaming Library
 * 
 * Copyright (C) 2008 Red Hat, Inc.
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
#include "gsocketconnectable.h"
#include "glibintl.h"


/**
 * SECTION:gsocketconnectable
 * @short_description: Interface for potential socket endpoints
 * @include: gio/gio.h
 *
 * Objects that describe one or more potential socket endpoints
 * implement #GSocketConnectable. Callers can then use
 * g_socket_connectable_enumerate() to get a #GSocketAddressEnumerator
 * to try out each socket address in turn until one succeeds, as shown
 * in the sample code below.
 *
 * |[<!-- language="C" -->
 * MyConnectionType *
 * connect_to_host (const char    *hostname,
 *                  guint16        port,
 *                  GCancellable  *cancellable,
 *                  GError       **error)
 * {
 *   MyConnection *conn = NULL;
 *   GSocketConnectable *addr;
 *   GSocketAddressEnumerator *enumerator;
 *   GSocketAddress *sockaddr;
 *   GError *conn_error = NULL;
 *
 *   addr = g_network_address_new (hostname, port);
 *   enumerator = g_socket_connectable_enumerate (addr);
 *   g_object_unref (addr);
 *
 *   // Try each sockaddr until we succeed. Record the first connection error,
 *   // but not any further ones (since they'll probably be basically the same
 *   // as the first).
 *   while (!conn && (sockaddr = g_socket_address_enumerator_next (enumerator, cancellable, error))
 *     {
 *       conn = connect_to_sockaddr (sockaddr, conn_error ? NULL : &conn_error);
 *       g_object_unref (sockaddr);
 *     }
 *   g_object_unref (enumerator);
 *
 *   if (conn)
 *     {
 *       if (conn_error)
 *         {
 *           // We couldn't connect to the first address, but we succeeded
 *           // in connecting to a later address.
 *           g_error_free (conn_error);
 *         }
 *       return conn;
 *     }
 *   else if (error)
 *     {
 *       /// Either initial lookup failed, or else the caller cancelled us.
 *       if (conn_error)
 *         g_error_free (conn_error);
 *       return NULL;
 *     }
 *   else
 *     {
 *       g_error_propagate (error, conn_error);
 *       return NULL;
 *     }
 * }
 * ]|
 */


typedef GSocketConnectableIface GSocketConnectableInterface;
G_DEFINE_INTERFACE (GSocketConnectable, g_socket_connectable, G_TYPE_OBJECT)

static void
g_socket_connectable_default_init (GSocketConnectableInterface *iface)
{
}

/**
 * g_socket_connectable_enumerate:
 * @connectable: a #GSocketConnectable
 *
 * Creates a #GSocketAddressEnumerator for @connectable.
 *
 * Returns: (transfer full): a new #GSocketAddressEnumerator.
 *
 * Since: 2.22
 */
GSocketAddressEnumerator *
g_socket_connectable_enumerate (GSocketConnectable *connectable)
{
  GSocketConnectableIface *iface;

  g_return_val_if_fail (G_IS_SOCKET_CONNECTABLE (connectable), NULL);

  iface = G_SOCKET_CONNECTABLE_GET_IFACE (connectable);

  return (* iface->enumerate) (connectable);
}

/**
 * g_socket_connectable_proxy_enumerate:
 * @connectable: a #GSocketConnectable
 *
 * Creates a #GSocketAddressEnumerator for @connectable that will
 * return #GProxyAddresses for addresses that you must connect
 * to via a proxy.
 *
 * If @connectable does not implement
 * g_socket_connectable_proxy_enumerate(), this will fall back to
 * calling g_socket_connectable_enumerate().
 *
 * Returns: (transfer full): a new #GSocketAddressEnumerator.
 *
 * Since: 2.26
 */
GSocketAddressEnumerator *
g_socket_connectable_proxy_enumerate (GSocketConnectable *connectable)
{
  GSocketConnectableIface *iface;

  g_return_val_if_fail (G_IS_SOCKET_CONNECTABLE (connectable), NULL);

  iface = G_SOCKET_CONNECTABLE_GET_IFACE (connectable);

  if (iface->proxy_enumerate)
    return (* iface->proxy_enumerate) (connectable);
  else
    return (* iface->enumerate) (connectable);
}

/**
 * g_socket_connectable_to_string:
 * @connectable: a #GSocketConnectable
 *
 * Format a #GSocketConnectable as a string. This is a human-readable format for
 * use in debugging output, and is not a stable serialization format. It is not
 * suitable for use in user interfaces as it exposes too much information for a
 * user.
 *
 * If the #GSocketConnectable implementation does not support string formatting,
 * the implementation’s type name will be returned as a fallback.
 *
 * Returns: (transfer full): the formatted string
 *
 * Since: 2.48
 */
gchar *
g_socket_connectable_to_string (GSocketConnectable *connectable)
{
  GSocketConnectableIface *iface;

  g_return_val_if_fail (G_IS_SOCKET_CONNECTABLE (connectable), NULL);

  iface = G_SOCKET_CONNECTABLE_GET_IFACE (connectable);

  if (iface->to_string != NULL)
    return iface->to_string (connectable);
  else
    return g_strdup (G_OBJECT_TYPE_NAME (connectable));
}
