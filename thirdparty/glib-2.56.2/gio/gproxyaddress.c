/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright (C) 2010 Collabora, Ltd.
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
 * Authors: Nicolas Dufresne <nicolas.dufresne@collabora.co.uk>
 */

#include <config.h>
#include <glib.h>
#include <string.h>

#include <gio/gsocketaddress.h>

#include "gproxyaddress.h"
#include "glibintl.h"

/**
 * SECTION:gproxyaddress
 * @short_description: An internet address with proxy information
 * @include: gio/gio.h
 *
 * Support for proxied #GInetSocketAddress.
 */

/**
 * GProxyAddress:
 *
 * A #GInetSocketAddress representing a connection via a proxy server
 *
 * Since: 2.26
 **/

/**
 * GProxyAddressClass:
 *
 * Class structure for #GProxyAddress.
 *
 * Since: 2.26
 **/

enum
{
  PROP_0,
  PROP_PROTOCOL,
  PROP_DESTINATION_PROTOCOL,
  PROP_DESTINATION_HOSTNAME,
  PROP_DESTINATION_PORT,
  PROP_USERNAME,
  PROP_PASSWORD,
  PROP_URI
};

struct _GProxyAddressPrivate
{
  gchar 	 *uri;
  gchar 	 *protocol;
  gchar		 *username;
  gchar		 *password;
  gchar 	 *dest_protocol;
  gchar 	 *dest_hostname;
  guint16 	  dest_port;
};

G_DEFINE_TYPE_WITH_PRIVATE (GProxyAddress, g_proxy_address, G_TYPE_INET_SOCKET_ADDRESS)

static void
g_proxy_address_finalize (GObject *object)
{
  GProxyAddress *proxy = G_PROXY_ADDRESS (object);

  g_free (proxy->priv->uri);
  g_free (proxy->priv->protocol);
  g_free (proxy->priv->username);
  g_free (proxy->priv->password);
  g_free (proxy->priv->dest_hostname);
  g_free (proxy->priv->dest_protocol);

  G_OBJECT_CLASS (g_proxy_address_parent_class)->finalize (object);
}

static void
g_proxy_address_set_property (GObject      *object,
			      guint         prop_id,
			      const GValue *value,
			      GParamSpec   *pspec)
{
  GProxyAddress *proxy = G_PROXY_ADDRESS (object);

  switch (prop_id)
    {
    case PROP_PROTOCOL:
      g_free (proxy->priv->protocol);
      proxy->priv->protocol = g_value_dup_string (value);
      break;

    case PROP_DESTINATION_PROTOCOL:
      g_free (proxy->priv->dest_protocol);
      proxy->priv->dest_protocol = g_value_dup_string (value);
      break;

    case PROP_DESTINATION_HOSTNAME:
      g_free (proxy->priv->dest_hostname);
      proxy->priv->dest_hostname = g_value_dup_string (value);
      break;

    case PROP_DESTINATION_PORT:
      proxy->priv->dest_port = g_value_get_uint (value);
      break;

    case PROP_USERNAME:
      g_free (proxy->priv->username);
      proxy->priv->username = g_value_dup_string (value);
      break;

    case PROP_PASSWORD:
      g_free (proxy->priv->password);
      proxy->priv->password = g_value_dup_string (value);
      break;

    case PROP_URI:
      g_free (proxy->priv->uri);
      proxy->priv->uri = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
g_proxy_address_get_property (GObject    *object,
			      guint       prop_id,
			      GValue     *value,
			      GParamSpec *pspec)
{
  GProxyAddress *proxy = G_PROXY_ADDRESS (object);

  switch (prop_id)
    {
      case PROP_PROTOCOL:
	g_value_set_string (value, proxy->priv->protocol);
	break;

      case PROP_DESTINATION_PROTOCOL:
	g_value_set_string (value, proxy->priv->dest_protocol);
	break;

      case PROP_DESTINATION_HOSTNAME:
	g_value_set_string (value, proxy->priv->dest_hostname);
	break;

      case PROP_DESTINATION_PORT:
	g_value_set_uint (value, proxy->priv->dest_port);
	break;

      case PROP_USERNAME:
	g_value_set_string (value, proxy->priv->username);
	break;

      case PROP_PASSWORD:
	g_value_set_string (value, proxy->priv->password);
	break;

      case PROP_URI:
	g_value_set_string (value, proxy->priv->uri);
	break;

      default:
	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
g_proxy_address_class_init (GProxyAddressClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = g_proxy_address_finalize;
  gobject_class->set_property = g_proxy_address_set_property;
  gobject_class->get_property = g_proxy_address_get_property;

  g_object_class_install_property (gobject_class,
				   PROP_PROTOCOL,
				   g_param_spec_string ("protocol",
						       P_("Protocol"),
						       P_("The proxy protocol"),
						       NULL,
						       G_PARAM_READWRITE |
						       G_PARAM_CONSTRUCT_ONLY |
						       G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
				   PROP_USERNAME,
				   g_param_spec_string ("username",
						       P_("Username"),
						       P_("The proxy username"),
						       NULL,
						       G_PARAM_READWRITE |
						       G_PARAM_CONSTRUCT_ONLY |
						       G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
				   PROP_PASSWORD,
				   g_param_spec_string ("password",
						       P_("Password"),
						       P_("The proxy password"),
						       NULL,
						       G_PARAM_READWRITE |
						       G_PARAM_CONSTRUCT_ONLY |
						       G_PARAM_STATIC_STRINGS));

  /**
   * GProxyAddress:destination-protocol:
   *
   * The protocol being spoke to the destination host, or %NULL if
   * the #GProxyAddress doesn't know.
   *
   * Since: 2.34
   */
  g_object_class_install_property (gobject_class,
				   PROP_DESTINATION_PROTOCOL,
				   g_param_spec_string ("destination-protocol",
						       P_("Destionation Protocol"),
						       P_("The proxy destination protocol"),
						       NULL,
						       G_PARAM_READWRITE |
						       G_PARAM_CONSTRUCT_ONLY |
						       G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
				   PROP_DESTINATION_HOSTNAME,
				   g_param_spec_string ("destination-hostname",
						       P_("Destination Hostname"),
						       P_("The proxy destination hostname"),
						       NULL,
						       G_PARAM_READWRITE |
						       G_PARAM_CONSTRUCT_ONLY |
						       G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class,
				   PROP_DESTINATION_PORT,
				   g_param_spec_uint ("destination-port",
						      P_("Destination Port"),
						      P_("The proxy destination port"),
						      0, 65535, 0,
						      G_PARAM_READWRITE |
						      G_PARAM_CONSTRUCT_ONLY |
						      G_PARAM_STATIC_STRINGS));

  /**
   * GProxyAddress:uri:
   *
   * The URI string that the proxy was constructed from (or %NULL
   * if the creator didn't specify this).
   *
   * Since: 2.34
   */
  g_object_class_install_property (gobject_class,
				   PROP_URI,
				   g_param_spec_string ("uri",
							P_("URI"),
							P_("The proxy’s URI"),
							NULL,
							G_PARAM_READWRITE |
							G_PARAM_CONSTRUCT_ONLY |
							G_PARAM_STATIC_STRINGS));
}

static void
g_proxy_address_init (GProxyAddress *proxy)
{
  proxy->priv = g_proxy_address_get_instance_private (proxy);
  proxy->priv->protocol = NULL;
  proxy->priv->username = NULL;
  proxy->priv->password = NULL;
  proxy->priv->dest_hostname = NULL;
  proxy->priv->dest_port = 0;
}

/**
 * g_proxy_address_new:
 * @inetaddr: The proxy server #GInetAddress.
 * @port: The proxy server port.
 * @protocol: The proxy protocol to support, in lower case (e.g. socks, http).
 * @dest_hostname: The destination hostname the proxy should tunnel to.
 * @dest_port: The destination port to tunnel to.
 * @username: (nullable): The username to authenticate to the proxy server
 *     (or %NULL).
 * @password: (nullable): The password to authenticate to the proxy server
 *     (or %NULL).
 *
 * Creates a new #GProxyAddress for @inetaddr with @protocol that should
 * tunnel through @dest_hostname and @dest_port.
 *
 * (Note that this method doesn't set the #GProxyAddress:uri or
 * #GProxyAddress:destination-protocol fields; use g_object_new()
 * directly if you want to set those.)
 *
 * Returns: a new #GProxyAddress
 *
 * Since: 2.26
 */
GSocketAddress *
g_proxy_address_new (GInetAddress  *inetaddr,
		     guint16        port,
		     const gchar   *protocol,
		     const gchar   *dest_hostname,
		     guint16        dest_port,
		     const gchar   *username,
		     const gchar   *password)
{
  return g_object_new (G_TYPE_PROXY_ADDRESS,
		       "address", inetaddr,
		       "port", port,
		       "protocol", protocol,
		       "destination-hostname", dest_hostname,
		       "destination-port", dest_port,
		       "username", username,
		       "password", password,
		       NULL);
}


/**
 * g_proxy_address_get_protocol:
 * @proxy: a #GProxyAddress
 *
 * Gets @proxy's protocol. eg, "socks" or "http"
 *
 * Returns: the @proxy's protocol
 *
 * Since: 2.26
 */
const gchar *
g_proxy_address_get_protocol (GProxyAddress *proxy)
{
  return proxy->priv->protocol;
}

/**
 * g_proxy_address_get_destination_protocol:
 * @proxy: a #GProxyAddress
 *
 * Gets the protocol that is being spoken to the destination
 * server; eg, "http" or "ftp".
 *
 * Returns: the @proxy's destination protocol
 *
 * Since: 2.34
 */
const gchar *
g_proxy_address_get_destination_protocol (GProxyAddress *proxy)
{
  return proxy->priv->dest_protocol;
}

/**
 * g_proxy_address_get_destination_hostname:
 * @proxy: a #GProxyAddress
 *
 * Gets @proxy's destination hostname; that is, the name of the host
 * that will be connected to via the proxy, not the name of the proxy
 * itself.
 *
 * Returns: the @proxy's destination hostname
 *
 * Since: 2.26
 */
const gchar *
g_proxy_address_get_destination_hostname (GProxyAddress *proxy)
{
  return proxy->priv->dest_hostname;
}

/**
 * g_proxy_address_get_destination_port:
 * @proxy: a #GProxyAddress
 *
 * Gets @proxy's destination port; that is, the port on the
 * destination host that will be connected to via the proxy, not the
 * port number of the proxy itself.
 *
 * Returns: the @proxy's destination port
 *
 * Since: 2.26
 */
guint16
g_proxy_address_get_destination_port (GProxyAddress *proxy)
{
  return proxy->priv->dest_port;
}

/**
 * g_proxy_address_get_username:
 * @proxy: a #GProxyAddress
 *
 * Gets @proxy's username.
 *
 * Returns: the @proxy's username
 *
 * Since: 2.26
 */
const gchar *
g_proxy_address_get_username (GProxyAddress *proxy)
{
  return proxy->priv->username;
}

/**
 * g_proxy_address_get_password:
 * @proxy: a #GProxyAddress
 *
 * Gets @proxy's password.
 *
 * Returns: the @proxy's password
 *
 * Since: 2.26
 */
const gchar *
g_proxy_address_get_password (GProxyAddress *proxy)
{
  return proxy->priv->password;
}


/**
 * g_proxy_address_get_uri:
 * @proxy: a #GProxyAddress
 *
 * Gets the proxy URI that @proxy was constructed from.
 *
 * Returns: the @proxy's URI, or %NULL if unknown
 *
 * Since: 2.34
 */
const gchar *
g_proxy_address_get_uri (GProxyAddress *proxy)
{
  return proxy->priv->uri;
}
