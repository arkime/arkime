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
 * Author: Nicolas Dufresne <nicolas.dufresne@collabora.co.uk>
 */

#include "config.h"
#include "gproxyaddressenumerator.h"

#include <string.h>

#include "gasyncresult.h"
#include "ginetaddress.h"
#include "glibintl.h"
#include "gnetworkaddress.h"
#include "gnetworkingprivate.h"
#include "gproxy.h"
#include "gproxyaddress.h"
#include "gproxyresolver.h"
#include "gtask.h"
#include "gresolver.h"
#include "gsocketaddress.h"
#include "gsocketaddressenumerator.h"
#include "gsocketconnectable.h"

#define GET_PRIVATE(o) (G_PROXY_ADDRESS_ENUMERATOR (o)->priv)

enum
{
  PROP_0,
  PROP_URI,
  PROP_DEFAULT_PORT,
  PROP_CONNECTABLE,
  PROP_PROXY_RESOLVER
};

struct _GProxyAddressEnumeratorPrivate
{
  /* Destination address */
  GSocketConnectable *connectable;
  gchar              *dest_uri;
  guint16             default_port;
  gchar              *dest_hostname;
  guint16             dest_port;
  GList              *dest_ips;

  /* Proxy enumeration */
  GProxyResolver           *proxy_resolver;
  gchar                   **proxies;
  gchar                   **next_proxy;
  GSocketAddressEnumerator *addr_enum;
  GSocketAddress           *proxy_address;
  const gchar              *proxy_uri;
  gchar                    *proxy_type;
  gchar                    *proxy_username;
  gchar                    *proxy_password;
  gboolean                  supports_hostname;
  GList                    *next_dest_ip;
  GError                   *last_error;
};

G_DEFINE_TYPE_WITH_PRIVATE (GProxyAddressEnumerator, g_proxy_address_enumerator, G_TYPE_SOCKET_ADDRESS_ENUMERATOR)

static void
save_userinfo (GProxyAddressEnumeratorPrivate *priv,
	       const gchar *proxy)
{
  gchar *userinfo;

  if (priv->proxy_username)
    {
      g_free (priv->proxy_username);
      priv->proxy_username = NULL;
    }

  if (priv->proxy_password)
    {
      g_free (priv->proxy_password);
      priv->proxy_password = NULL;
    }
  
  if (_g_uri_parse_authority (proxy, NULL, NULL, &userinfo, NULL))
    {
      if (userinfo)
	{
	  gchar **split = g_strsplit (userinfo, ":", 2);

	  if (split[0] != NULL)
	    {
	      priv->proxy_username = g_uri_unescape_string (split[0], NULL);
	      if (split[1] != NULL)
		priv->proxy_password = g_uri_unescape_string (split[1], NULL);
	    }

	  g_strfreev (split);
	  g_free (userinfo);
	}
    }
}

static void
next_enumerator (GProxyAddressEnumeratorPrivate *priv)
{
  if (priv->proxy_address)
    return;

  while (priv->addr_enum == NULL && *priv->next_proxy)
    {
      GSocketConnectable *connectable = NULL;
      GProxy *proxy;

      priv->proxy_uri = *priv->next_proxy++;
      g_free (priv->proxy_type);
      priv->proxy_type = g_uri_parse_scheme (priv->proxy_uri);

      if (priv->proxy_type == NULL)
	continue;

      /* Assumes hostnames are supported for unknown protocols */
      priv->supports_hostname = TRUE;
      proxy = g_proxy_get_default_for_protocol (priv->proxy_type);
      if (proxy)
        {
	  priv->supports_hostname = g_proxy_supports_hostname (proxy);
	  g_object_unref (proxy);
        }

      if (strcmp ("direct", priv->proxy_type) == 0)
	{
	  if (priv->connectable)
	    connectable = g_object_ref (priv->connectable);
	  else
	    connectable = g_network_address_new (priv->dest_hostname,
						 priv->dest_port);
	}
      else
	{
	  GError *error = NULL;

	  connectable = g_network_address_parse_uri (priv->proxy_uri, 0, &error);

	  if (error)
	    {
	      g_warning ("Invalid proxy URI '%s': %s",
			 priv->proxy_uri, error->message);
	      g_error_free (error);
	    }

	  save_userinfo (priv, priv->proxy_uri);
	}

      if (connectable)
	{
	  priv->addr_enum = g_socket_connectable_enumerate (connectable);
	  g_object_unref (connectable);
	}
    }
}

static GSocketAddress *
g_proxy_address_enumerator_next (GSocketAddressEnumerator  *enumerator,
				 GCancellable              *cancellable,
				 GError                   **error)
{
  GProxyAddressEnumeratorPrivate *priv = GET_PRIVATE (enumerator);
  GSocketAddress *result = NULL;
  GError *first_error = NULL;

  if (priv->proxies == NULL)
    {
      priv->proxies = g_proxy_resolver_lookup (priv->proxy_resolver,
					       priv->dest_uri,
					       cancellable,
					       error);
      priv->next_proxy = priv->proxies;

      if (priv->proxies == NULL)
	return NULL;
    }

  while (result == NULL && (*priv->next_proxy || priv->addr_enum))
    {
      gchar *dest_hostname;
      gchar *dest_protocol;
      GInetSocketAddress *inetsaddr;
      GInetAddress *inetaddr;
      guint16 port;

      next_enumerator (priv);

      if (!priv->addr_enum)
	continue;

      if (priv->proxy_address == NULL)
	{
	  priv->proxy_address = g_socket_address_enumerator_next (
				    priv->addr_enum,
				    cancellable,
				    first_error ? NULL : &first_error);
	}

      if (priv->proxy_address == NULL)
	{
	  g_object_unref (priv->addr_enum);
	  priv->addr_enum = NULL;

	  if (priv->dest_ips)
	    {
	      g_resolver_free_addresses (priv->dest_ips);
	      priv->dest_ips = NULL;
	    }

	  continue;
	}

      if (strcmp ("direct", priv->proxy_type) == 0)
	{
	  result = priv->proxy_address;
	  priv->proxy_address = NULL;
	  continue;
	}

      if (!priv->supports_hostname)
	{
	  GInetAddress *dest_ip;

	  if (!priv->dest_ips)
	    {
	      GResolver *resolver;

	      resolver = g_resolver_get_default();
	      priv->dest_ips = g_resolver_lookup_by_name (resolver,
							  priv->dest_hostname,
							  cancellable,
							  first_error ? NULL : &first_error);
	      g_object_unref (resolver);

	      if (!priv->dest_ips)
		{
		  g_object_unref (priv->proxy_address);
		  priv->proxy_address = NULL;
		  continue;
		}
	    }

	  if (!priv->next_dest_ip)
	    priv->next_dest_ip = priv->dest_ips;
	
	  dest_ip = G_INET_ADDRESS (priv->next_dest_ip->data);
	  dest_hostname = g_inet_address_to_string (dest_ip);

	  priv->next_dest_ip = g_list_next (priv->next_dest_ip);
	}
      else
	{
	  dest_hostname = g_strdup (priv->dest_hostname);
	}
      dest_protocol = g_uri_parse_scheme (priv->dest_uri);
		 		  
      g_return_val_if_fail (G_IS_INET_SOCKET_ADDRESS (priv->proxy_address),
			    NULL);

      inetsaddr = G_INET_SOCKET_ADDRESS (priv->proxy_address);
      inetaddr = g_inet_socket_address_get_address (inetsaddr);
      port = g_inet_socket_address_get_port (inetsaddr);

      result = g_object_new (G_TYPE_PROXY_ADDRESS,
			     "address", inetaddr,
			     "port", port,
			     "protocol", priv->proxy_type,
			     "destination-protocol", dest_protocol,
			     "destination-hostname", dest_hostname,
			     "destination-port", priv->dest_port,
			     "username", priv->proxy_username,
			     "password", priv->proxy_password,
			     "uri", priv->proxy_uri,
			     NULL);
      g_free (dest_hostname);
      g_free (dest_protocol);

      if (priv->supports_hostname || priv->next_dest_ip == NULL)
	{
	  g_object_unref (priv->proxy_address);
	  priv->proxy_address = NULL;
	}
    }

  if (result == NULL && first_error)
    g_propagate_error (error, first_error);
  else if (first_error)
    g_error_free (first_error);

  return result;
}



static void
complete_async (GTask *task)
{
  GProxyAddressEnumeratorPrivate *priv = g_task_get_task_data (task);

  if (priv->last_error)
    {
      g_task_return_error (task, priv->last_error);
      priv->last_error = NULL;
    }
  else
    g_task_return_pointer (task, NULL, NULL);

  g_object_unref (task);
}

static void
return_result (GTask *task)
{
  GProxyAddressEnumeratorPrivate *priv = g_task_get_task_data (task);
  GSocketAddress *result;

  if (strcmp ("direct", priv->proxy_type) == 0)
    {
      result = priv->proxy_address;
      priv->proxy_address = NULL;
    }
  else
    {
      gchar *dest_hostname, *dest_protocol;
      GInetSocketAddress *inetsaddr;
      GInetAddress *inetaddr;
      guint16 port;

      if (!priv->supports_hostname)
	{
	  GInetAddress *dest_ip;

	  if (!priv->next_dest_ip)
	    priv->next_dest_ip = priv->dest_ips;

	  dest_ip = G_INET_ADDRESS (priv->next_dest_ip->data);
	  dest_hostname = g_inet_address_to_string (dest_ip);

	  priv->next_dest_ip = g_list_next (priv->next_dest_ip);
	}
      else
	{
	  dest_hostname = g_strdup (priv->dest_hostname);
	}
      dest_protocol = g_uri_parse_scheme (priv->dest_uri);

      g_return_if_fail (G_IS_INET_SOCKET_ADDRESS (priv->proxy_address));

      inetsaddr = G_INET_SOCKET_ADDRESS (priv->proxy_address);
      inetaddr = g_inet_socket_address_get_address (inetsaddr);
      port = g_inet_socket_address_get_port (inetsaddr);

      result = g_object_new (G_TYPE_PROXY_ADDRESS,
			     "address", inetaddr,
			     "port", port,
			     "protocol", priv->proxy_type,
			     "destination-protocol", dest_protocol,
			     "destination-hostname", dest_hostname,
			     "destination-port", priv->dest_port,
			     "username", priv->proxy_username,
			     "password", priv->proxy_password,
			     "uri", priv->proxy_uri,
			     NULL);
      g_free (dest_hostname);
      g_free (dest_protocol);

      if (priv->supports_hostname || priv->next_dest_ip == NULL)
	{
	  g_object_unref (priv->proxy_address);
	  priv->proxy_address = NULL;
	}
    }

  g_task_return_pointer (task, result, g_object_unref);
  g_object_unref (task);
}

static void address_enumerate_cb (GObject      *object,
				  GAsyncResult *result,
				  gpointer	user_data);

static void
next_proxy (GTask *task)
{
  GProxyAddressEnumeratorPrivate *priv = g_task_get_task_data (task);

  if (*priv->next_proxy)
    {
      g_object_unref (priv->addr_enum);
      priv->addr_enum = NULL;

      if (priv->dest_ips)
	{
	  g_resolver_free_addresses (priv->dest_ips);
	  priv->dest_ips = NULL;
	}

      next_enumerator (priv);

      if (priv->addr_enum)
	{
	  g_socket_address_enumerator_next_async (priv->addr_enum,
						  g_task_get_cancellable (task),
						  address_enumerate_cb,
						  task);
	  return;
	}
    }

  complete_async (task);
}

static void
dest_hostname_lookup_cb (GObject           *object,
			 GAsyncResult      *result,
			 gpointer           user_data)
{
  GTask *task = user_data;
  GProxyAddressEnumeratorPrivate *priv = g_task_get_task_data (task);

  g_clear_error (&priv->last_error);
  priv->dest_ips = g_resolver_lookup_by_name_finish (G_RESOLVER (object),
						     result,
						     &priv->last_error);
  if (priv->dest_ips)
    return_result (task);
  else
    {
      g_clear_object (&priv->proxy_address);
      next_proxy (task);
    }
}

static void
address_enumerate_cb (GObject	   *object,
		      GAsyncResult *result,
		      gpointer	    user_data)
{
  GTask *task = user_data;
  GProxyAddressEnumeratorPrivate *priv = g_task_get_task_data (task);

  g_clear_error (&priv->last_error);
  priv->proxy_address =
    g_socket_address_enumerator_next_finish (priv->addr_enum,
					     result,
					     &priv->last_error);
  if (priv->proxy_address)
    {
      if (!priv->supports_hostname && !priv->dest_ips)
	{
	  GResolver *resolver;
	  resolver = g_resolver_get_default();
	  g_resolver_lookup_by_name_async (resolver,
					   priv->dest_hostname,
					   g_task_get_cancellable (task),
					   dest_hostname_lookup_cb,
					   task);
	  g_object_unref (resolver);
	  return;
	}

      return_result (task);
    }
  else
    next_proxy (task);
}

static void
proxy_lookup_cb (GObject      *object,
		 GAsyncResult *result,
		 gpointer      user_data)
{
  GTask *task = user_data;
  GProxyAddressEnumeratorPrivate *priv = g_task_get_task_data (task);

  g_clear_error (&priv->last_error);
  priv->proxies = g_proxy_resolver_lookup_finish (G_PROXY_RESOLVER (object),
						  result,
						  &priv->last_error);
  priv->next_proxy = priv->proxies;

  if (priv->last_error)
    {
      complete_async (task);
      return;
    }
  else
    {
      next_enumerator (priv);
      if (priv->addr_enum)
	{
	  g_socket_address_enumerator_next_async (priv->addr_enum,
						  g_task_get_cancellable (task),
						  address_enumerate_cb,
						  task);
	  return;
	}
    }

  complete_async (task);
}

static void
g_proxy_address_enumerator_next_async (GSocketAddressEnumerator *enumerator,
				       GCancellable             *cancellable,
				       GAsyncReadyCallback       callback,
				       gpointer                  user_data)
{
  GProxyAddressEnumeratorPrivate *priv = GET_PRIVATE (enumerator);
  GTask *task;

  task = g_task_new (enumerator, cancellable, callback, user_data);
  g_task_set_source_tag (task, g_proxy_address_enumerator_next_async);
  g_task_set_task_data (task, priv, NULL);

  if (priv->proxies == NULL)
    {
      g_proxy_resolver_lookup_async (priv->proxy_resolver,
				     priv->dest_uri,
				     cancellable,
				     proxy_lookup_cb,
				     task);
      return;
    }

  if (priv->addr_enum)
    {
      if (priv->proxy_address)
	{
	  return_result (task);
	  return;
	}
      else
	{
	  g_socket_address_enumerator_next_async (priv->addr_enum,
						  cancellable,
						  address_enumerate_cb,
						  task);
	  return;
	}
    }

  complete_async (task);
}

static GSocketAddress *
g_proxy_address_enumerator_next_finish (GSocketAddressEnumerator  *enumerator,
					GAsyncResult              *result,
					GError                   **error)
{
  g_return_val_if_fail (g_task_is_valid (result, enumerator), NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
g_proxy_address_enumerator_constructed (GObject *object)
{
  GProxyAddressEnumeratorPrivate *priv = GET_PRIVATE (object);
  GSocketConnectable *conn;
  guint port;

  if (priv->dest_uri)
    {
      conn = g_network_address_parse_uri (priv->dest_uri, priv->default_port, NULL);
      if (conn)
        {
          g_object_get (conn,
                        "hostname", &priv->dest_hostname,
                        "port", &port,
                        NULL);
          priv->dest_port = port;

          g_object_unref (conn);
        }
      else
        g_warning ("Invalid URI '%s'", priv->dest_uri);
    }

  G_OBJECT_CLASS (g_proxy_address_enumerator_parent_class)->constructed (object);
}

static void
g_proxy_address_enumerator_get_property (GObject        *object,
                                         guint           property_id,
                                         GValue         *value,
                                         GParamSpec     *pspec)
{
  GProxyAddressEnumeratorPrivate *priv = GET_PRIVATE (object);
  switch (property_id)
    {
    case PROP_URI:
      g_value_set_string (value, priv->dest_uri);
      break;

    case PROP_DEFAULT_PORT:
      g_value_set_uint (value, priv->default_port);
      break;

    case PROP_CONNECTABLE:
      g_value_set_object (value, priv->connectable);
      break;

    case PROP_PROXY_RESOLVER:
      g_value_set_object (value, priv->proxy_resolver);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
g_proxy_address_enumerator_set_property (GObject        *object,
                                         guint           property_id,
                                         const GValue   *value,
                                         GParamSpec     *pspec)
{
  GProxyAddressEnumeratorPrivate *priv = GET_PRIVATE (object);
  switch (property_id)
    {
    case PROP_URI:
      priv->dest_uri = g_value_dup_string (value);
      break;

    case PROP_DEFAULT_PORT:
      priv->default_port = g_value_get_uint (value);
      break;

    case PROP_CONNECTABLE:
      priv->connectable = g_value_dup_object (value);
      break;

    case PROP_PROXY_RESOLVER:
      if (priv->proxy_resolver)
        g_object_unref (priv->proxy_resolver);
      priv->proxy_resolver = g_value_get_object (value);
      if (!priv->proxy_resolver)
        priv->proxy_resolver = g_proxy_resolver_get_default ();
      g_object_ref (priv->proxy_resolver);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
g_proxy_address_enumerator_finalize (GObject *object)
{
  GProxyAddressEnumeratorPrivate *priv = GET_PRIVATE (object);

  if (priv->connectable)
    g_object_unref (priv->connectable);

  if (priv->proxy_resolver)
    g_object_unref (priv->proxy_resolver);

  g_free (priv->dest_uri);
  g_free (priv->dest_hostname);

  if (priv->dest_ips)
    g_resolver_free_addresses (priv->dest_ips);

  g_strfreev (priv->proxies);

  if (priv->addr_enum)
    g_object_unref (priv->addr_enum);

  g_free (priv->proxy_type);
  g_free (priv->proxy_username);
  g_free (priv->proxy_password);

  g_clear_error (&priv->last_error);

  G_OBJECT_CLASS (g_proxy_address_enumerator_parent_class)->finalize (object);
}

static void
g_proxy_address_enumerator_init (GProxyAddressEnumerator *self)
{
  self->priv = g_proxy_address_enumerator_get_instance_private (self);
}

static void
g_proxy_address_enumerator_class_init (GProxyAddressEnumeratorClass *proxy_enumerator_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (proxy_enumerator_class);
  GSocketAddressEnumeratorClass *enumerator_class = G_SOCKET_ADDRESS_ENUMERATOR_CLASS (proxy_enumerator_class);

  object_class->constructed = g_proxy_address_enumerator_constructed;
  object_class->set_property = g_proxy_address_enumerator_set_property;
  object_class->get_property = g_proxy_address_enumerator_get_property;
  object_class->finalize = g_proxy_address_enumerator_finalize;

  enumerator_class->next = g_proxy_address_enumerator_next;
  enumerator_class->next_async = g_proxy_address_enumerator_next_async;
  enumerator_class->next_finish = g_proxy_address_enumerator_next_finish;

  g_object_class_install_property (object_class,
				   PROP_URI,
				   g_param_spec_string ("uri",
							P_("URI"),
							P_("The destination URI, use none:// for generic socket"),
							NULL,
							G_PARAM_READWRITE |
							G_PARAM_CONSTRUCT_ONLY |
							G_PARAM_STATIC_STRINGS));

  /**
   * GProxyAddressEnumerator:default-port:
   *
   * The default port to use if #GProxyAddressEnumerator:uri does not
   * specify one.
   *
   * Since: 2.38
   */
  g_object_class_install_property (object_class,
				   PROP_DEFAULT_PORT,
				   g_param_spec_uint ("default-port",
                                                      P_("Default port"),
                                                      P_("The default port to use if uri does not specify one"),
                                                      0, 65535, 0,
                                                      G_PARAM_READWRITE |
                                                      G_PARAM_CONSTRUCT_ONLY |
                                                      G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class,
				   PROP_CONNECTABLE,
				   g_param_spec_object ("connectable",
							P_("Connectable"),
							P_("The connectable being enumerated."),
							G_TYPE_SOCKET_CONNECTABLE,
							G_PARAM_READWRITE |
							G_PARAM_CONSTRUCT_ONLY |
							G_PARAM_STATIC_STRINGS));

  /**
   * GProxyAddressEnumerator:proxy-resolver:
   *
   * The proxy resolver to use.
   *
   * Since: 2.36
   */
  g_object_class_install_property (object_class,
                                   PROP_PROXY_RESOLVER,
                                   g_param_spec_object ("proxy-resolver",
                                                        P_("Proxy resolver"),
                                                        P_("The proxy resolver to use."),
                                                        G_TYPE_PROXY_RESOLVER,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT |
                                                        G_PARAM_STATIC_STRINGS));
}
