/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright (C) 2010 Collabora Ltd.
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
 * Author: Nicolas Dufresne <nicolas.dufresne@collabora.co.uk>
 */

#include "config.h"

#include "gproxy.h"

#include "giomodule.h"
#include "giomodule-priv.h"
#include "glibintl.h"

/**
 * SECTION:gproxy
 * @short_description: Interface for proxy handling
 * @include: gio/gio.h
 *
 * A #GProxy handles connecting to a remote host via a given type of
 * proxy server. It is implemented by the 'gio-proxy' extension point.
 * The extensions are named after their proxy protocol name. As an
 * example, a SOCKS5 proxy implementation can be retrieved with the
 * name 'socks5' using the function
 * g_io_extension_point_get_extension_by_name().
 *
 * Since: 2.26
 **/

G_DEFINE_INTERFACE (GProxy, g_proxy, G_TYPE_OBJECT)

static void
g_proxy_default_init (GProxyInterface *iface)
{
}

/**
 * g_proxy_get_default_for_protocol:
 * @protocol: the proxy protocol name (e.g. http, socks, etc)
 *
 * Lookup "gio-proxy" extension point for a proxy implementation that supports
 * specified protocol.
 *
 * Returns: (transfer full): return a #GProxy or NULL if protocol
 *               is not supported.
 *
 * Since: 2.26
 **/
GProxy *
g_proxy_get_default_for_protocol (const gchar *protocol)
{
  GIOExtensionPoint *ep;
  GIOExtension *extension;

  /* Ensure proxy modules loaded */
  _g_io_modules_ensure_loaded ();

  ep = g_io_extension_point_lookup (G_PROXY_EXTENSION_POINT_NAME);

  extension = g_io_extension_point_get_extension_by_name (ep, protocol);

  if (extension)
      return g_object_new (g_io_extension_get_type (extension), NULL);

  return NULL;
}

/**
 * g_proxy_connect:
 * @proxy: a #GProxy
 * @connection: a #GIOStream
 * @proxy_address: a #GProxyAddress
 * @cancellable: (nullable): a #GCancellable
 * @error: return #GError
 *
 * Given @connection to communicate with a proxy (eg, a
 * #GSocketConnection that is connected to the proxy server), this
 * does the necessary handshake to connect to @proxy_address, and if
 * required, wraps the #GIOStream to handle proxy payload.
 *
 * Returns: (transfer full): a #GIOStream that will replace @connection. This might
 *               be the same as @connection, in which case a reference
 *               will be added.
 *
 * Since: 2.26
 */
GIOStream *
g_proxy_connect (GProxy            *proxy,
		 GIOStream         *connection,
		 GProxyAddress     *proxy_address,
		 GCancellable      *cancellable,
		 GError           **error)
{
  GProxyInterface *iface;

  g_return_val_if_fail (G_IS_PROXY (proxy), NULL);

  iface = G_PROXY_GET_IFACE (proxy);

  return (* iface->connect) (proxy,
			     connection,
			     proxy_address,
			     cancellable,
			     error);
}

/**
 * g_proxy_connect_async:
 * @proxy: a #GProxy
 * @connection: a #GIOStream
 * @proxy_address: a #GProxyAddress
 * @cancellable: (nullable): a #GCancellable
 * @callback: (scope async): a #GAsyncReadyCallback
 * @user_data: (closure): callback data
 *
 * Asynchronous version of g_proxy_connect().
 *
 * Since: 2.26
 */
void
g_proxy_connect_async (GProxy               *proxy,
		       GIOStream            *connection,
		       GProxyAddress        *proxy_address,
		       GCancellable         *cancellable,
		       GAsyncReadyCallback   callback,
		       gpointer              user_data)
{
  GProxyInterface *iface;

  g_return_if_fail (G_IS_PROXY (proxy));

  iface = G_PROXY_GET_IFACE (proxy);

  (* iface->connect_async) (proxy,
			    connection,
			    proxy_address,
			    cancellable,
			    callback,
			    user_data);
}

/**
 * g_proxy_connect_finish:
 * @proxy: a #GProxy
 * @result: a #GAsyncResult
 * @error: return #GError
 *
 * See g_proxy_connect().
 *
 * Returns: (transfer full): a #GIOStream.
 *
 * Since: 2.26
 */
GIOStream *
g_proxy_connect_finish (GProxy       *proxy,
			GAsyncResult *result,
			GError      **error)
{
  GProxyInterface *iface;

  g_return_val_if_fail (G_IS_PROXY (proxy), NULL);

  iface = G_PROXY_GET_IFACE (proxy);

  return (* iface->connect_finish) (proxy, result, error);
}

/**
 * g_proxy_supports_hostname:
 * @proxy: a #GProxy
 *
 * Some proxy protocols expect to be passed a hostname, which they
 * will resolve to an IP address themselves. Others, like SOCKS4, do
 * not allow this. This function will return %FALSE if @proxy is
 * implementing such a protocol. When %FALSE is returned, the caller
 * should resolve the destination hostname first, and then pass a
 * #GProxyAddress containing the stringified IP address to
 * g_proxy_connect() or g_proxy_connect_async().
 *
 * Returns: %TRUE if hostname resolution is supported.
 *
 * Since: 2.26
 */
gboolean
g_proxy_supports_hostname (GProxy *proxy)
{
  GProxyInterface *iface;

  g_return_val_if_fail (G_IS_PROXY (proxy), FALSE);

  iface = G_PROXY_GET_IFACE (proxy);

  return (* iface->supports_hostname) (proxy);
}
