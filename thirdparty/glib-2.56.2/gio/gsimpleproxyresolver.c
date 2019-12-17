/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright 2010, 2013 Red Hat, Inc.
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
 * Public License along with this library; if not, see
 * <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>

#include "gsimpleproxyresolver.h"
#include "ginetaddress.h"
#include "ginetaddressmask.h"
#include "gnetworkingprivate.h"
#include "gtask.h"

#include "glibintl.h"

/**
 * SECTION:gsimpleproxyresolver
 * @short_description: Simple proxy resolver implementation
 * @include: gio/gio.h
 * @see_also: g_socket_client_set_proxy_resolver()
 *
 * #GSimpleProxyResolver is a simple #GProxyResolver implementation
 * that handles a single default proxy, multiple URI-scheme-specific
 * proxies, and a list of hosts that proxies should not be used for.
 *
 * #GSimpleProxyResolver is never the default proxy resolver, but it
 * can be used as the base class for another proxy resolver
 * implementation, or it can be created and used manually, such as
 * with g_socket_client_set_proxy_resolver().
 *
 * Since: 2.36
 */

typedef struct {
  gchar        *name;
  gint          length;
  gushort       port;
} GSimpleProxyResolverDomain;

struct _GSimpleProxyResolverPrivate {
  gchar *default_proxy, **ignore_hosts;
  GHashTable *uri_proxies;

  GPtrArray *ignore_ips;
  GSimpleProxyResolverDomain *ignore_domains;
};

static void g_simple_proxy_resolver_iface_init (GProxyResolverInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GSimpleProxyResolver, g_simple_proxy_resolver, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (GSimpleProxyResolver)
                         G_IMPLEMENT_INTERFACE (G_TYPE_PROXY_RESOLVER,
                                                g_simple_proxy_resolver_iface_init))

enum
{
  PROP_0,
  PROP_DEFAULT_PROXY,
  PROP_IGNORE_HOSTS
};

static void reparse_ignore_hosts (GSimpleProxyResolver *resolver);

static void
g_simple_proxy_resolver_finalize (GObject *object)
{
  GSimpleProxyResolver *resolver = G_SIMPLE_PROXY_RESOLVER (object);
  GSimpleProxyResolverPrivate *priv = resolver->priv;

  g_free (priv->default_proxy);
  g_hash_table_destroy (priv->uri_proxies);

  g_clear_pointer (&priv->ignore_hosts, g_strfreev);
  /* This will free ignore_ips and ignore_domains */
  reparse_ignore_hosts (resolver);

  G_OBJECT_CLASS (g_simple_proxy_resolver_parent_class)->finalize (object);
}

static void
g_simple_proxy_resolver_init (GSimpleProxyResolver *resolver)
{
  resolver->priv = g_simple_proxy_resolver_get_instance_private (resolver);
  resolver->priv->uri_proxies = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                       g_free, g_free);
}

static void
g_simple_proxy_resolver_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  GSimpleProxyResolver *resolver = G_SIMPLE_PROXY_RESOLVER (object);

  switch (prop_id)
    {
      case PROP_DEFAULT_PROXY:
        g_simple_proxy_resolver_set_default_proxy (resolver, g_value_get_string (value));
	break;

      case PROP_IGNORE_HOSTS:
        g_simple_proxy_resolver_set_ignore_hosts (resolver, g_value_get_boxed (value));
	break;

      default:
	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
g_simple_proxy_resolver_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  GSimpleProxyResolver *resolver = G_SIMPLE_PROXY_RESOLVER (object);

  switch (prop_id)
    {
      case PROP_DEFAULT_PROXY:
	g_value_set_string (value, resolver->priv->default_proxy);
	break;

      case PROP_IGNORE_HOSTS:
	g_value_set_boxed (value, resolver->priv->ignore_hosts);
	break;

      default:
	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
reparse_ignore_hosts (GSimpleProxyResolver *resolver)
{
  GSimpleProxyResolverPrivate *priv = resolver->priv;
  GPtrArray *ignore_ips;
  GArray *ignore_domains;
  gchar *host, *tmp, *colon, *bracket;
  GInetAddress *iaddr;
  GInetAddressMask *mask;
  GSimpleProxyResolverDomain domain;
  gushort port;
  int i;

  if (priv->ignore_ips)
    g_ptr_array_free (priv->ignore_ips, TRUE);
  if (priv->ignore_domains)
    {
      for (i = 0; priv->ignore_domains[i].name; i++)
	g_free (priv->ignore_domains[i].name);
      g_free (priv->ignore_domains);
    }
  priv->ignore_ips = NULL;
  priv->ignore_domains = NULL;

  if (!priv->ignore_hosts || !priv->ignore_hosts[0])
    return;

  ignore_ips = g_ptr_array_new_with_free_func (g_object_unref);
  ignore_domains = g_array_new (TRUE, FALSE, sizeof (GSimpleProxyResolverDomain));

  for (i = 0; priv->ignore_hosts[i]; i++)
    {
      host = g_strchomp (priv->ignore_hosts[i]);

      /* See if it's an IP address or IP/length mask */
      mask = g_inet_address_mask_new_from_string (host, NULL);
      if (mask)
        {
          g_ptr_array_add (ignore_ips, mask);
          continue;
        }

      port = 0;

      if (*host == '[')
        {
          /* [IPv6]:port */
          host++;
          bracket = strchr (host, ']');
          if (!bracket || !bracket[1] || bracket[1] != ':')
            goto bad;

          port = strtoul (bracket + 2, &tmp, 10);
          if (*tmp)
            goto bad;

          *bracket = '\0';
        }
      else
        {
          colon = strchr (host, ':');
          if (colon && !strchr (colon + 1, ':'))
            {
              /* hostname:port or IPv4:port */
              port = strtoul (colon + 1, &tmp, 10);
              if (*tmp)
                goto bad;
              *colon = '\0';
            }
        }

      iaddr = g_inet_address_new_from_string (host);
      if (iaddr)
        g_object_unref (iaddr);
      else
        {
          if (g_str_has_prefix (host, "*."))
            host += 2;
          else if (*host == '.')
            host++;
        }

      memset (&domain, 0, sizeof (domain));
      domain.name = g_strdup (host);
      domain.length = strlen (domain.name);
      domain.port = port;
      g_array_append_val (ignore_domains, domain);
      continue;

    bad:
      g_warning ("Ignoring invalid ignore_hosts value '%s'", host);
    }

  if (ignore_ips->len)
    priv->ignore_ips = ignore_ips;
  else
    g_ptr_array_free (ignore_ips, TRUE);

  if (ignore_domains->len)
    priv->ignore_domains = (GSimpleProxyResolverDomain *)ignore_domains->data;
  g_array_free (ignore_domains, ignore_domains->len == 0);
}

static gboolean
ignore_host (GSimpleProxyResolver *resolver,
	     const gchar          *host,
	     gushort               port)
{
  GSimpleProxyResolverPrivate *priv = resolver->priv;
  gchar *ascii_host = NULL;
  gboolean ignore = FALSE;
  gint i, length, offset;

  if (priv->ignore_ips)
    {
      GInetAddress *iaddr;

      iaddr = g_inet_address_new_from_string (host);
      if (iaddr)
	{
	  for (i = 0; i < priv->ignore_ips->len; i++)
	    {
	      GInetAddressMask *mask = priv->ignore_ips->pdata[i];

	      if (g_inet_address_mask_matches (mask, iaddr))
		{
		  ignore = TRUE;
		  break;
		}
	    }

	  g_object_unref (iaddr);
	  if (ignore)
	    return TRUE;
	}
    }

  if (priv->ignore_domains)
    {
      length = 0;
      if (g_hostname_is_non_ascii (host))
        host = ascii_host = g_hostname_to_ascii (host);
      if (host)
        length = strlen (host);

      for (i = 0; length > 0 && priv->ignore_domains[i].length; i++)
	{
	  GSimpleProxyResolverDomain *domain = &priv->ignore_domains[i];

	  offset = length - domain->length;
	  if ((domain->port == 0 || domain->port == port) &&
	      (offset == 0 || (offset > 0 && host[offset - 1] == '.')) &&
	      (g_ascii_strcasecmp (domain->name, host + offset) == 0))
	    {
	      ignore = TRUE;
	      break;
	    }
	}

      g_free (ascii_host);
    }

  return ignore;
}

static gchar **
g_simple_proxy_resolver_lookup (GProxyResolver  *proxy_resolver,
                                const gchar     *uri,
                                GCancellable    *cancellable,
                                GError         **error)
{
  GSimpleProxyResolver *resolver = G_SIMPLE_PROXY_RESOLVER (proxy_resolver);
  GSimpleProxyResolverPrivate *priv = resolver->priv;
  const gchar *proxy = NULL;
  gchar **proxies;

  if (priv->ignore_ips || priv->ignore_domains)
    {
      gchar *host = NULL;
      gushort port;

      if (_g_uri_parse_authority (uri, &host, &port, NULL, NULL) &&
          ignore_host (resolver, host, port))
        proxy = "direct://";

      g_free (host);
    }

  if (!proxy && g_hash_table_size (priv->uri_proxies))
    {
      gchar *scheme = g_ascii_strdown (uri, strcspn (uri, ":"));

      proxy = g_hash_table_lookup (priv->uri_proxies, scheme);
      g_free (scheme);
    }

  if (!proxy)
    proxy = priv->default_proxy;
  if (!proxy)
    proxy = "direct://";

  if (!strncmp (proxy, "socks://", 8))
    {
      proxies = g_new0 (gchar *, 4);
      proxies[0] = g_strdup_printf ("socks5://%s", proxy + 8);
      proxies[1] = g_strdup_printf ("socks4a://%s", proxy + 8);
      proxies[2] = g_strdup_printf ("socks4://%s", proxy + 8);
      proxies[3] = NULL;
    }
  else
    {
      proxies = g_new0 (gchar *, 2);
      proxies[0] = g_strdup (proxy);
    }

  return proxies;
}

static void
g_simple_proxy_resolver_lookup_async (GProxyResolver      *proxy_resolver,
                                      const gchar         *uri,
                                      GCancellable        *cancellable,
                                      GAsyncReadyCallback  callback,
                                      gpointer             user_data)
{
  GSimpleProxyResolver *resolver = G_SIMPLE_PROXY_RESOLVER (proxy_resolver);
  GTask *task;
  GError *error = NULL;
  char **proxies;

  task = g_task_new (resolver, cancellable, callback, user_data);
  g_task_set_source_tag (task, g_simple_proxy_resolver_lookup_async);

  proxies = g_simple_proxy_resolver_lookup (proxy_resolver, uri,
                                            cancellable, &error);
  if (proxies)
    g_task_return_pointer (task, proxies, (GDestroyNotify)g_strfreev);
  else
    g_task_return_error (task, error);
  g_object_unref (task);
}

static gchar **
g_simple_proxy_resolver_lookup_finish (GProxyResolver  *resolver,
                                       GAsyncResult    *result,
                                       GError         **error)
{
  g_return_val_if_fail (g_task_is_valid (result, resolver), NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
g_simple_proxy_resolver_class_init (GSimpleProxyResolverClass *resolver_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (resolver_class);

  object_class->get_property = g_simple_proxy_resolver_get_property;
  object_class->set_property = g_simple_proxy_resolver_set_property;
  object_class->finalize = g_simple_proxy_resolver_finalize;

  /**
   * GSimpleProxyResolver:default-proxy:
   *
   * The default proxy URI that will be used for any URI that doesn't
   * match #GSimpleProxyResolver:ignore-hosts, and doesn't match any
   * of the schemes set with g_simple_proxy_resolver_set_uri_proxy().
   *
   * Note that as a special case, if this URI starts with
   * "socks://", #GSimpleProxyResolver will treat it as referring
   * to all three of the socks5, socks4a, and socks4 proxy types.
   */
  g_object_class_install_property (object_class, PROP_DEFAULT_PROXY,
				   g_param_spec_string ("default-proxy",
                                                        P_("Default proxy"),
                                                        P_("The default proxy URI"),
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));

  /**
   * GSimpleProxyResolver:ignore-hosts:
   *
   * A list of hostnames and IP addresses that the resolver should
   * allow direct connections to.
   *
   * Entries can be in one of 4 formats:
   *
   * - A hostname, such as "example.com", ".example.com", or
   *   "*.example.com", any of which match "example.com" or
   *   any subdomain of it.
   *
   * - An IPv4 or IPv6 address, such as "192.168.1.1",
   *   which matches only that address.
   *
   * - A hostname or IP address followed by a port, such as
   *   "example.com:80", which matches whatever the hostname or IP
   *   address would match, but only for URLs with the (explicitly)
   *   indicated port. In the case of an IPv6 address, the address
   *   part must appear in brackets: "[::1]:443"
   *
   * - An IP address range, given by a base address and prefix length,
   *   such as "fe80::/10", which matches any address in that range.
   *
   * Note that when dealing with Unicode hostnames, the matching is
   * done against the ASCII form of the name.
   *
   * Also note that hostname exclusions apply only to connections made
   * to hosts identified by name, and IP address exclusions apply only
   * to connections made to hosts identified by address. That is, if
   * example.com has an address of 192.168.1.1, and the :ignore-hosts list
   * contains only "192.168.1.1", then a connection to "example.com"
   * (eg, via a #GNetworkAddress) will use the proxy, and a connection to
   * "192.168.1.1" (eg, via a #GInetSocketAddress) will not.
   *
   * These rules match the "ignore-hosts"/"noproxy" rules most
   * commonly used by other applications.
   */
  g_object_class_install_property (object_class, PROP_IGNORE_HOSTS,
				   g_param_spec_boxed ("ignore-hosts",
                                                       P_("Ignore hosts"),
                                                       P_("Hosts that will not use the proxy"),
                                                       G_TYPE_STRV,
                                                       G_PARAM_READWRITE |
                                                       G_PARAM_STATIC_STRINGS));

}

static void
g_simple_proxy_resolver_iface_init (GProxyResolverInterface *iface)
{
  iface->lookup = g_simple_proxy_resolver_lookup;
  iface->lookup_async = g_simple_proxy_resolver_lookup_async;
  iface->lookup_finish = g_simple_proxy_resolver_lookup_finish;
}

/**
 * g_simple_proxy_resolver_new:
 * @default_proxy: (nullable): the default proxy to use, eg
 *     "socks://192.168.1.1"
 * @ignore_hosts: (nullable): an optional list of hosts/IP addresses
 *     to not use a proxy for.
 *
 * Creates a new #GSimpleProxyResolver. See
 * #GSimpleProxyResolver:default-proxy and
 * #GSimpleProxyResolver:ignore-hosts for more details on how the
 * arguments are interpreted.
 *
 * Returns: (transfer full) a new #GSimpleProxyResolver
 *
 * Since: 2.36
 */
GProxyResolver *
g_simple_proxy_resolver_new (const gchar  *default_proxy,
                             gchar       **ignore_hosts)
{
  return g_object_new (G_TYPE_SIMPLE_PROXY_RESOLVER,
                       "default-proxy", default_proxy,
                       "ignore-hosts", ignore_hosts,
                       NULL);
}

/**
 * g_simple_proxy_resolver_set_default_proxy:
 * @resolver: a #GSimpleProxyResolver
 * @default_proxy: the default proxy to use
 *
 * Sets the default proxy on @resolver, to be used for any URIs that
 * don't match #GSimpleProxyResolver:ignore-hosts or a proxy set
 * via g_simple_proxy_resolver_set_uri_proxy().
 *
 * If @default_proxy starts with "socks://",
 * #GSimpleProxyResolver will treat it as referring to all three of
 * the socks5, socks4a, and socks4 proxy types.
 *
 * Since: 2.36
 */
void
g_simple_proxy_resolver_set_default_proxy (GSimpleProxyResolver *resolver,
                                           const gchar          *default_proxy)
{
  g_return_if_fail (G_IS_SIMPLE_PROXY_RESOLVER (resolver));

  g_free (resolver->priv->default_proxy);
  resolver->priv->default_proxy = g_strdup (default_proxy);
  g_object_notify (G_OBJECT (resolver), "default-proxy");
}

/**
 * g_simple_proxy_resolver_set_ignore_hosts:
 * @resolver: a #GSimpleProxyResolver
 * @ignore_hosts: %NULL-terminated list of hosts/IP addresses
 *     to not use a proxy for
 *
 * Sets the list of ignored hosts.
 *
 * See #GSimpleProxyResolver:ignore-hosts for more details on how the
 * @ignore_hosts argument is interpreted.
 *
 * Since: 2.36
 */
void
g_simple_proxy_resolver_set_ignore_hosts (GSimpleProxyResolver  *resolver,
                                          gchar                **ignore_hosts)
{
  g_return_if_fail (G_IS_SIMPLE_PROXY_RESOLVER (resolver));

  g_strfreev (resolver->priv->ignore_hosts);
  resolver->priv->ignore_hosts = g_strdupv (ignore_hosts);
  reparse_ignore_hosts (resolver);
  g_object_notify (G_OBJECT (resolver), "ignore-hosts");
}

/**
 * g_simple_proxy_resolver_set_uri_proxy:
 * @resolver: a #GSimpleProxyResolver
 * @uri_scheme: the URI scheme to add a proxy for
 * @proxy: the proxy to use for @uri_scheme
 *
 * Adds a URI-scheme-specific proxy to @resolver; URIs whose scheme
 * matches @uri_scheme (and which don't match
 * #GSimpleProxyResolver:ignore-hosts) will be proxied via @proxy.
 *
 * As with #GSimpleProxyResolver:default-proxy, if @proxy starts with
 * "socks://", #GSimpleProxyResolver will treat it
 * as referring to all three of the socks5, socks4a, and socks4 proxy
 * types.
 *
 * Since: 2.36
 */
void
g_simple_proxy_resolver_set_uri_proxy (GSimpleProxyResolver *resolver,
                                       const gchar          *uri_scheme,
                                       const gchar          *proxy)
{
  g_return_if_fail (G_IS_SIMPLE_PROXY_RESOLVER (resolver));

  g_hash_table_replace (resolver->priv->uri_proxies,
                        g_ascii_strdown (uri_scheme, -1),
                        g_strdup (proxy));
}
