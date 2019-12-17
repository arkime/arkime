/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

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
#include <glib.h>
#include "glibintl.h"

#include <stdlib.h>
#include "gnetworkaddress.h"
#include "gasyncresult.h"
#include "ginetaddress.h"
#include "ginetsocketaddress.h"
#include "gnetworkingprivate.h"
#include "gproxyaddressenumerator.h"
#include "gresolver.h"
#include "gtask.h"
#include "gsocketaddressenumerator.h"
#include "gioerror.h"
#include "gsocketconnectable.h"

#include <string.h>


/**
 * SECTION:gnetworkaddress
 * @short_description: A GSocketConnectable for resolving hostnames
 * @include: gio/gio.h
 *
 * #GNetworkAddress provides an easy way to resolve a hostname and
 * then attempt to connect to that host, handling the possibility of
 * multiple IP addresses and multiple address families.
 *
 * See #GSocketConnectable for and example of using the connectable
 * interface.
 */

/**
 * GNetworkAddress:
 *
 * A #GSocketConnectable for resolving a hostname and connecting to
 * that host.
 */

struct _GNetworkAddressPrivate {
  gchar *hostname;
  guint16 port;
  GList *sockaddrs;
  gchar *scheme;

  gint64 resolver_serial;
};

enum {
  PROP_0,
  PROP_HOSTNAME,
  PROP_PORT,
  PROP_SCHEME,
};

static void g_network_address_set_property (GObject      *object,
                                            guint         prop_id,
                                            const GValue *value,
                                            GParamSpec   *pspec);
static void g_network_address_get_property (GObject      *object,
                                            guint         prop_id,
                                            GValue       *value,
                                            GParamSpec   *pspec);

static void                      g_network_address_connectable_iface_init       (GSocketConnectableIface *iface);
static GSocketAddressEnumerator *g_network_address_connectable_enumerate        (GSocketConnectable      *connectable);
static GSocketAddressEnumerator	*g_network_address_connectable_proxy_enumerate  (GSocketConnectable      *connectable);
static gchar                    *g_network_address_connectable_to_string        (GSocketConnectable      *connectable);

G_DEFINE_TYPE_WITH_CODE (GNetworkAddress, g_network_address, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (GNetworkAddress)
                         G_IMPLEMENT_INTERFACE (G_TYPE_SOCKET_CONNECTABLE,
                                                g_network_address_connectable_iface_init))

static void
g_network_address_finalize (GObject *object)
{
  GNetworkAddress *addr = G_NETWORK_ADDRESS (object);

  g_free (addr->priv->hostname);
  g_free (addr->priv->scheme);
  g_list_free_full (addr->priv->sockaddrs, g_object_unref);

  G_OBJECT_CLASS (g_network_address_parent_class)->finalize (object);
}

static void
g_network_address_class_init (GNetworkAddressClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = g_network_address_set_property;
  gobject_class->get_property = g_network_address_get_property;
  gobject_class->finalize = g_network_address_finalize;

  g_object_class_install_property (gobject_class, PROP_HOSTNAME,
                                   g_param_spec_string ("hostname",
                                                        P_("Hostname"),
                                                        P_("Hostname to resolve"),
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_PORT,
                                   g_param_spec_uint ("port",
                                                      P_("Port"),
                                                      P_("Network port"),
                                                      0, 65535, 0,
                                                      G_PARAM_READWRITE |
                                                      G_PARAM_CONSTRUCT_ONLY |
                                                      G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_SCHEME,
                                   g_param_spec_string ("scheme",
                                                        P_("Scheme"),
                                                        P_("URI Scheme"),
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));
}

static void
g_network_address_connectable_iface_init (GSocketConnectableIface *connectable_iface)
{
  connectable_iface->enumerate  = g_network_address_connectable_enumerate;
  connectable_iface->proxy_enumerate = g_network_address_connectable_proxy_enumerate;
  connectable_iface->to_string = g_network_address_connectable_to_string;
}

static void
g_network_address_init (GNetworkAddress *addr)
{
  addr->priv = g_network_address_get_instance_private (addr);
}

static void
g_network_address_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GNetworkAddress *addr = G_NETWORK_ADDRESS (object);

  switch (prop_id)
    {
    case PROP_HOSTNAME:
      g_free (addr->priv->hostname);
      addr->priv->hostname = g_value_dup_string (value);
      break;

    case PROP_PORT:
      addr->priv->port = g_value_get_uint (value);
      break;

    case PROP_SCHEME:
      g_free (addr->priv->scheme);
      addr->priv->scheme = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }

}

static void
g_network_address_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  GNetworkAddress *addr = G_NETWORK_ADDRESS (object);

  switch (prop_id)
    {
    case PROP_HOSTNAME:
      g_value_set_string (value, addr->priv->hostname);
      break;

    case PROP_PORT:
      g_value_set_uint (value, addr->priv->port);
      break;

    case PROP_SCHEME:
      g_value_set_string (value, addr->priv->scheme);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }

}

static void
g_network_address_set_addresses (GNetworkAddress *addr,
                                 GList           *addresses,
                                 guint64          resolver_serial)
{
  GList *a;
  GSocketAddress *sockaddr;

  g_return_if_fail (addresses != NULL && addr->priv->sockaddrs == NULL);

  for (a = addresses; a; a = a->next)
    {
      sockaddr = g_inet_socket_address_new (a->data, addr->priv->port);
      addr->priv->sockaddrs = g_list_prepend (addr->priv->sockaddrs, sockaddr);
      g_object_unref (a->data);
    }
  g_list_free (addresses);
  addr->priv->sockaddrs = g_list_reverse (addr->priv->sockaddrs);

  addr->priv->resolver_serial = resolver_serial;
}

static gboolean
g_network_address_parse_sockaddr (GNetworkAddress *addr)
{
  GSocketAddress *sockaddr;

  sockaddr = g_inet_socket_address_new_from_string (addr->priv->hostname,
                                                    addr->priv->port);
  if (sockaddr)
    {
      addr->priv->sockaddrs = g_list_prepend (addr->priv->sockaddrs, sockaddr);
      return TRUE;
    }
  else
    return FALSE;
}

/**
 * g_network_address_new:
 * @hostname: the hostname
 * @port: the port
 *
 * Creates a new #GSocketConnectable for connecting to the given
 * @hostname and @port.
 *
 * Note that depending on the configuration of the machine, a
 * @hostname of `localhost` may refer to the IPv4 loopback address
 * only, or to both IPv4 and IPv6; use
 * g_network_address_new_loopback() to create a #GNetworkAddress that
 * is guaranteed to resolve to both addresses.
 *
 * Returns: (transfer full) (type GNetworkAddress): the new #GNetworkAddress
 *
 * Since: 2.22
 */
GSocketConnectable *
g_network_address_new (const gchar *hostname,
                       guint16      port)
{
  return g_object_new (G_TYPE_NETWORK_ADDRESS,
                       "hostname", hostname,
                       "port", port,
                       NULL);
}

/**
 * g_network_address_new_loopback:
 * @port: the port
 *
 * Creates a new #GSocketConnectable for connecting to the local host
 * over a loopback connection to the given @port. This is intended for
 * use in connecting to local services which may be running on IPv4 or
 * IPv6.
 *
 * The connectable will return IPv4 and IPv6 loopback addresses,
 * regardless of how the host resolves `localhost`. By contrast,
 * g_network_address_new() will often only return an IPv4 address when
 * resolving `localhost`, and an IPv6 address for `localhost6`.
 *
 * g_network_address_get_hostname() will always return `localhost` for
 * #GNetworkAddresses created with this constructor.
 *
 * Returns: (transfer full) (type GNetworkAddress): the new #GNetworkAddress
 *
 * Since: 2.44
 */
GSocketConnectable *
g_network_address_new_loopback (guint16 port)
{
  GNetworkAddress *addr;
  GList *addrs = NULL;

  addr = g_object_new (G_TYPE_NETWORK_ADDRESS,
                       "hostname", "localhost",
                       "port", port,
                       NULL);

  addrs = g_list_append (addrs, g_inet_address_new_loopback (AF_INET6));
  addrs = g_list_append (addrs, g_inet_address_new_loopback (AF_INET));
  g_network_address_set_addresses (addr, addrs, 0);

  return G_SOCKET_CONNECTABLE (addr);
}

/**
 * g_network_address_parse:
 * @host_and_port: the hostname and optionally a port
 * @default_port: the default port if not in @host_and_port
 * @error: a pointer to a #GError, or %NULL
 *
 * Creates a new #GSocketConnectable for connecting to the given
 * @hostname and @port. May fail and return %NULL in case
 * parsing @host_and_port fails.
 *
 * @host_and_port may be in any of a number of recognised formats; an IPv6
 * address, an IPv4 address, or a domain name (in which case a DNS
 * lookup is performed). Quoting with [] is supported for all address
 * types. A port override may be specified in the usual way with a
 * colon.
 *
 * If no port is specified in @host_and_port then @default_port will be
 * used as the port number to connect to.
 *
 * In general, @host_and_port is expected to be provided by the user
 * (allowing them to give the hostname, and a port override if necessary)
 * and @default_port is expected to be provided by the application.
 *
 * (The port component of @host_and_port can also be specified as a
 * service name rather than as a numeric port, but this functionality
 * is deprecated, because it depends on the contents of /etc/services,
 * which is generally quite sparse on platforms other than Linux.)
 *
 * Returns: (transfer full) (type GNetworkAddress): the new
 *   #GNetworkAddress, or %NULL on error
 *
 * Since: 2.22
 */
GSocketConnectable *
g_network_address_parse (const gchar  *host_and_port,
                         guint16       default_port,
                         GError      **error)
{
  GSocketConnectable *connectable;
  const gchar *port;
  guint16 portnum;
  gchar *name;

  g_return_val_if_fail (host_and_port != NULL, NULL);

  port = NULL;
  if (host_and_port[0] == '[')
    /* escaped host part (to allow, eg. "[2001:db8::1]:888") */
    {
      const gchar *end;

      end = strchr (host_and_port, ']');
      if (end == NULL)
        {
          g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                       _("Hostname “%s” contains “[” but not “]”"), host_and_port);
          return NULL;
        }

      if (end[1] == '\0')
        port = NULL;
      else if (end[1] == ':')
        port = &end[2];
      else
        {
          g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                       "The ']' character (in hostname '%s') must come at the"
                       " end or be immediately followed by ':' and a port",
                       host_and_port);
          return NULL;
        }

      name = g_strndup (host_and_port + 1, end - host_and_port - 1);
    }

  else if ((port = strchr (host_and_port, ':')))
    /* string has a ':' in it */
    {
      /* skip ':' */
      port++;

      if (strchr (port, ':'))
        /* more than one ':' in string */
        {
          /* this is actually an unescaped IPv6 address */
          name = g_strdup (host_and_port);
          port = NULL;
        }
      else
        name = g_strndup (host_and_port, port - host_and_port - 1);
    }

  else
    /* plain hostname, no port */
    name = g_strdup (host_and_port);

  if (port != NULL)
    {
      if (port[0] == '\0')
        {
          g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                       "If a ':' character is given, it must be followed by a "
                       "port (in hostname '%s').", host_and_port);
          g_free (name);
          return NULL;
        }

      else if ('0' <= port[0] && port[0] <= '9')
        {
          char *end;
          long value;

          value = strtol (port, &end, 10);
          if (*end != '\0' || value < 0 || value > G_MAXUINT16)
            {
              g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                           "Invalid numeric port '%s' specified in hostname '%s'",
                           port, host_and_port);
              g_free (name);
              return NULL;
            }

          portnum = value;
        }

      else
        {
          struct servent *entry;

          entry = getservbyname (port, "tcp");
          if (entry == NULL)
            {
              g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                           "Unknown service '%s' specified in hostname '%s'",
                           port, host_and_port);
#ifdef HAVE_ENDSERVENT
              endservent ();
#endif
              g_free (name);
              return NULL;
            }

          portnum = g_ntohs (entry->s_port);

#ifdef HAVE_ENDSERVENT
          endservent ();
#endif
        }
    }
  else
    {
      /* No port in host_and_port */
      portnum = default_port;
    }

  connectable = g_network_address_new (name, portnum);
  g_free (name);

  return connectable;
}

/* Allowed characters outside alphanumeric for unreserved. */
#define G_URI_OTHER_UNRESERVED "-._~"

/* This or something equivalent will eventually go into glib/guri.h */
gboolean
_g_uri_parse_authority (const char  *uri,
		        char       **host,
		        guint16     *port,
		        char       **userinfo,
		        GError     **error)
{
  char *ascii_uri, *tmp_str;
  const char *start, *p, *at, *delim;
  char c;

  g_return_val_if_fail (uri != NULL, FALSE);

  if (host)
    *host = NULL;

  if (port)
    *port = 0;

  if (userinfo)
    *userinfo = NULL;

  /* Catch broken URIs early by trying to convert to ASCII. */
  ascii_uri = g_hostname_to_ascii (uri);
  if (!ascii_uri)
    goto error;

  /* From RFC 3986 Decodes:
   * URI          = scheme ":" hier-part [ "?" query ] [ "#" fragment ]
   * hier-part    = "//" authority path-abempty
   * path-abempty = *( "/" segment )
   * authority    = [ userinfo "@" ] host [ ":" port ]
   */

  /* Check we have a valid scheme */
  tmp_str = g_uri_parse_scheme (ascii_uri);

  if (tmp_str == NULL)
    goto error;

  g_free (tmp_str);

  /* Decode hier-part:
   *  hier-part   = "//" authority path-abempty
   */
  p = ascii_uri;
  start = strstr (p, "//");

  if (start == NULL)
    goto error;

  start += 2;

  /* check if the @ sign is part of the authority before attempting to
   * decode the userinfo */
  delim = strpbrk (start, "/?#[]");
  at = strchr (start, '@');
  if (at && delim && at > delim)
    at = NULL;

  if (at != NULL)
    {
      /* Decode userinfo:
       * userinfo      = *( unreserved / pct-encoded / sub-delims / ":" )
       * unreserved    = ALPHA / DIGIT / "-" / "." / "_" / "~"
       * pct-encoded   = "%" HEXDIG HEXDIG
       */
      p = start;
      while (1)
	{
	  c = *p++;

	  if (c == '@')
	    break;

	  /* pct-encoded */
	  if (c == '%')
	    {
	      if (!(g_ascii_isxdigit (p[0]) ||
		    g_ascii_isxdigit (p[1])))
          goto error;

	      p++;

	      continue;
	    }

	  /* unreserved /  sub-delims / : */
	  if (!(g_ascii_isalnum (c) ||
		strchr (G_URI_OTHER_UNRESERVED, c) ||
		strchr (G_URI_RESERVED_CHARS_SUBCOMPONENT_DELIMITERS, c) ||
		c == ':'))
      goto error;
	}

      if (userinfo)
	*userinfo = g_strndup (start, p - start - 1);

      start = p;
    }
  else
    {
      p = start;
    }


  /* decode host:
   * host          = IP-literal / IPv4address / reg-name
   * reg-name      = *( unreserved / pct-encoded / sub-delims )
   */

  /* If IPv6 or IPvFuture */
  if (*p == '[')
    {
      gboolean has_scope_id = FALSE, has_bad_scope_id = FALSE;

      start++;
      p++;
      while (1)
	{
	  c = *p++;

	  if (c == ']')
	    break;

          if (c == '%' && !has_scope_id)
            {
              has_scope_id = TRUE;
              if (p[0] != '2' || p[1] != '5')
                has_bad_scope_id = TRUE;
              continue;
            }

	  /* unreserved /  sub-delims */
	  if (!(g_ascii_isalnum (c) ||
		strchr (G_URI_OTHER_UNRESERVED, c) ||
		strchr (G_URI_RESERVED_CHARS_SUBCOMPONENT_DELIMITERS, c) ||
		c == ':' ||
		c == '.'))
      goto error;
	}

      if (host)
        {
          if (has_bad_scope_id)
            *host = g_strndup (start, p - start - 1);
          else
            *host = g_uri_unescape_segment (start, p - 1, NULL);
        }

      c = *p++;
    }
  else
    {
      while (1)
	{
	  c = *p++;

	  if (c == ':' ||
	      c == '/' ||
	      c == '?' ||
	      c == '#' ||
	      c == '\0')
	    break;

	  /* pct-encoded */
	  if (c == '%')
	    {
	      if (!(g_ascii_isxdigit (p[0]) ||
		    g_ascii_isxdigit (p[1])))
          goto error;

	      p++;

	      continue;
	    }

	  /* unreserved /  sub-delims */
	  if (!(g_ascii_isalnum (c) ||
		strchr (G_URI_OTHER_UNRESERVED, c) ||
		strchr (G_URI_RESERVED_CHARS_SUBCOMPONENT_DELIMITERS, c)))
      goto error;
	}

      if (host)
        *host = g_uri_unescape_segment (start, p - 1, NULL);
    }

  if (c == ':')
    {
      /* Decode port:
       *  port          = *DIGIT
       */
      guint tmp = 0;

      while (1)
	{
	  c = *p++;

	  if (c == '/' ||
	      c == '?' ||
	      c == '#' ||
	      c == '\0')
	    break;

	  if (!g_ascii_isdigit (c))
      goto error;

	  tmp = (tmp * 10) + (c - '0');

	  if (tmp > 65535)
	    goto error;
	}
      if (port)
	*port = (guint16) tmp;
    }

  g_free (ascii_uri);

  return TRUE;

error:
  g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
               "Invalid URI ‘%s’", uri);

  if (host && *host)
    {
      g_free (*host);
      *host = NULL;
    }

  if (userinfo && *userinfo)
    {
      g_free (*userinfo);
      *userinfo = NULL;
    }

  g_free (ascii_uri);

  return FALSE;
}

gchar *
_g_uri_from_authority (const gchar *protocol,
                       const gchar *host,
                       guint        port,
                       const gchar *userinfo)
{
  GString *uri;

  uri = g_string_new (protocol);
  g_string_append (uri, "://");

  if (userinfo)
    {
      g_string_append_uri_escaped (uri, userinfo, G_URI_RESERVED_CHARS_ALLOWED_IN_USERINFO, FALSE);
      g_string_append_c (uri, '@');
    }

  if (g_hostname_is_non_ascii (host))
    {
      gchar *ace_encoded = g_hostname_to_ascii (host);

      if (!ace_encoded)
        {
          g_string_free (uri, TRUE);
          return NULL;
        }
      g_string_append (uri, ace_encoded);
      g_free (ace_encoded);
    }
  else if (strchr (host, ':'))
    g_string_append_printf (uri, "[%s]", host);
  else
    g_string_append (uri, host);

  if (port != 0)
    g_string_append_printf (uri, ":%u", port);

  return g_string_free (uri, FALSE);
}

/**
 * g_network_address_parse_uri:
 * @uri: the hostname and optionally a port
 * @default_port: The default port if none is found in the URI
 * @error: a pointer to a #GError, or %NULL
 *
 * Creates a new #GSocketConnectable for connecting to the given
 * @uri. May fail and return %NULL in case parsing @uri fails.
 *
 * Using this rather than g_network_address_new() or
 * g_network_address_parse() allows #GSocketClient to determine
 * when to use application-specific proxy protocols.
 *
 * Returns: (transfer full) (type GNetworkAddress): the new
 *   #GNetworkAddress, or %NULL on error
 *
 * Since: 2.26
 */
GSocketConnectable *
g_network_address_parse_uri (const gchar  *uri,
    			     guint16       default_port,
			     GError      **error)
{
  GSocketConnectable *conn;
  gchar *scheme;
  gchar *hostname;
  guint16 port;

  if (!_g_uri_parse_authority (uri, &hostname, &port, NULL, error))
    return NULL;

  if (port == 0)
    port = default_port;

  scheme = g_uri_parse_scheme (uri);

  conn = g_object_new (G_TYPE_NETWORK_ADDRESS,
                       "hostname", hostname,
                       "port", port,
                       "scheme", scheme,
                       NULL);

  g_free (scheme);
  g_free (hostname);

  return conn;
}

/**
 * g_network_address_get_hostname:
 * @addr: a #GNetworkAddress
 *
 * Gets @addr's hostname. This might be either UTF-8 or ASCII-encoded,
 * depending on what @addr was created with.
 *
 * Returns: @addr's hostname
 *
 * Since: 2.22
 */
const gchar *
g_network_address_get_hostname (GNetworkAddress *addr)
{
  g_return_val_if_fail (G_IS_NETWORK_ADDRESS (addr), NULL);

  return addr->priv->hostname;
}

/**
 * g_network_address_get_port:
 * @addr: a #GNetworkAddress
 *
 * Gets @addr's port number
 *
 * Returns: @addr's port (which may be 0)
 *
 * Since: 2.22
 */
guint16
g_network_address_get_port (GNetworkAddress *addr)
{
  g_return_val_if_fail (G_IS_NETWORK_ADDRESS (addr), 0);

  return addr->priv->port;
}

/**
 * g_network_address_get_scheme:
 * @addr: a #GNetworkAddress
 *
 * Gets @addr's scheme
 *
 * Returns: @addr's scheme (%NULL if not built from URI)
 *
 * Since: 2.26
 */
const gchar *
g_network_address_get_scheme (GNetworkAddress *addr)
{
  g_return_val_if_fail (G_IS_NETWORK_ADDRESS (addr), NULL);

  return addr->priv->scheme;
}

#define G_TYPE_NETWORK_ADDRESS_ADDRESS_ENUMERATOR (_g_network_address_address_enumerator_get_type ())
#define G_NETWORK_ADDRESS_ADDRESS_ENUMERATOR(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), G_TYPE_NETWORK_ADDRESS_ADDRESS_ENUMERATOR, GNetworkAddressAddressEnumerator))

typedef struct {
  GSocketAddressEnumerator parent_instance;

  GNetworkAddress *addr;
  GList *addresses;
  GList *next;
} GNetworkAddressAddressEnumerator;

typedef struct {
  GSocketAddressEnumeratorClass parent_class;

} GNetworkAddressAddressEnumeratorClass;

static GType _g_network_address_address_enumerator_get_type (void);
G_DEFINE_TYPE (GNetworkAddressAddressEnumerator, _g_network_address_address_enumerator, G_TYPE_SOCKET_ADDRESS_ENUMERATOR)

static void
g_network_address_address_enumerator_finalize (GObject *object)
{
  GNetworkAddressAddressEnumerator *addr_enum =
    G_NETWORK_ADDRESS_ADDRESS_ENUMERATOR (object);

  g_object_unref (addr_enum->addr);

  G_OBJECT_CLASS (_g_network_address_address_enumerator_parent_class)->finalize (object);
}

static GSocketAddress *
g_network_address_address_enumerator_next (GSocketAddressEnumerator  *enumerator,
                                           GCancellable              *cancellable,
                                           GError                   **error)
{
  GNetworkAddressAddressEnumerator *addr_enum =
    G_NETWORK_ADDRESS_ADDRESS_ENUMERATOR (enumerator);
  GSocketAddress *sockaddr;

  if (addr_enum->addresses == NULL)
    {
      GNetworkAddress *addr = addr_enum->addr;
      GResolver *resolver = g_resolver_get_default ();
      gint64 serial = g_resolver_get_serial (resolver);

      if (addr->priv->resolver_serial != 0 &&
          addr->priv->resolver_serial != serial)
        {
          /* Resolver has reloaded, discard cached addresses */
          g_list_free_full (addr->priv->sockaddrs, g_object_unref);
          addr->priv->sockaddrs = NULL;
        }

      if (!addr->priv->sockaddrs)
        g_network_address_parse_sockaddr (addr);
      if (!addr->priv->sockaddrs)
        {
          GList *addresses;

          addresses = g_resolver_lookup_by_name (resolver,
                                                 addr->priv->hostname,
                                                 cancellable, error);
          if (!addresses)
            {
              g_object_unref (resolver);
              return NULL;
            }

          g_network_address_set_addresses (addr, addresses, serial);
        }
          
      addr_enum->addresses = addr->priv->sockaddrs;
      addr_enum->next = addr_enum->addresses;
      g_object_unref (resolver);
    }

  if (addr_enum->next == NULL)
    return NULL;

  sockaddr = addr_enum->next->data;
  addr_enum->next = addr_enum->next->next;
  return g_object_ref (sockaddr);
}

static void
have_addresses (GNetworkAddressAddressEnumerator *addr_enum,
                GTask *task, GError *error)
{
  GSocketAddress *sockaddr;

  addr_enum->addresses = addr_enum->addr->priv->sockaddrs;
  addr_enum->next = addr_enum->addresses;

  if (addr_enum->next)
    {
      sockaddr = g_object_ref (addr_enum->next->data);
      addr_enum->next = addr_enum->next->next;
    }
  else
    sockaddr = NULL;

  if (error)
    g_task_return_error (task, error);
  else
    g_task_return_pointer (task, sockaddr, g_object_unref);
  g_object_unref (task);
}

static void
got_addresses (GObject      *source_object,
               GAsyncResult *result,
               gpointer      user_data)
{
  GTask *task = user_data;
  GNetworkAddressAddressEnumerator *addr_enum = g_task_get_source_object (task);
  GResolver *resolver = G_RESOLVER (source_object);
  GList *addresses;
  GError *error = NULL;

  if (!addr_enum->addr->priv->sockaddrs)
    {
      addresses = g_resolver_lookup_by_name_finish (resolver, result, &error);

      if (!error)
        {
          g_network_address_set_addresses (addr_enum->addr, addresses,
                                           g_resolver_get_serial (resolver));
        }
    }
  have_addresses (addr_enum, task, error);
}

static void
g_network_address_address_enumerator_next_async (GSocketAddressEnumerator  *enumerator,
                                                 GCancellable              *cancellable,
                                                 GAsyncReadyCallback        callback,
                                                 gpointer                   user_data)
{
  GNetworkAddressAddressEnumerator *addr_enum =
    G_NETWORK_ADDRESS_ADDRESS_ENUMERATOR (enumerator);
  GSocketAddress *sockaddr;
  GTask *task;

  task = g_task_new (addr_enum, cancellable, callback, user_data);
  g_task_set_source_tag (task, g_network_address_address_enumerator_next_async);

  if (addr_enum->addresses == NULL)
    {
      GNetworkAddress *addr = addr_enum->addr;
      GResolver *resolver = g_resolver_get_default ();
      gint64 serial = g_resolver_get_serial (resolver);

      if (addr->priv->resolver_serial != 0 &&
          addr->priv->resolver_serial != serial)
        {
          /* Resolver has reloaded, discard cached addresses */
          g_list_free_full (addr->priv->sockaddrs, g_object_unref);
          addr->priv->sockaddrs = NULL;
        }

      if (!addr->priv->sockaddrs)
        {
          if (g_network_address_parse_sockaddr (addr))
            have_addresses (addr_enum, task, NULL);
          else
            {
              g_resolver_lookup_by_name_async (resolver,
                                               addr->priv->hostname,
                                               cancellable,
                                               got_addresses, task);
            }
          g_object_unref (resolver);
          return;
        }

      addr_enum->addresses = addr->priv->sockaddrs;
      addr_enum->next = addr_enum->addresses;
      g_object_unref (resolver);
    }

  if (addr_enum->next)
    {
      sockaddr = g_object_ref (addr_enum->next->data);
      addr_enum->next = addr_enum->next->next;
    }
  else
    sockaddr = NULL;

  g_task_return_pointer (task, sockaddr, g_object_unref);
  g_object_unref (task);
}

static GSocketAddress *
g_network_address_address_enumerator_next_finish (GSocketAddressEnumerator  *enumerator,
                                                  GAsyncResult              *result,
                                                  GError                   **error)
{
  g_return_val_if_fail (g_task_is_valid (result, enumerator), NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
_g_network_address_address_enumerator_init (GNetworkAddressAddressEnumerator *enumerator)
{
}

static void
_g_network_address_address_enumerator_class_init (GNetworkAddressAddressEnumeratorClass *addrenum_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (addrenum_class);
  GSocketAddressEnumeratorClass *enumerator_class =
    G_SOCKET_ADDRESS_ENUMERATOR_CLASS (addrenum_class);

  enumerator_class->next = g_network_address_address_enumerator_next;
  enumerator_class->next_async = g_network_address_address_enumerator_next_async;
  enumerator_class->next_finish = g_network_address_address_enumerator_next_finish;
  object_class->finalize = g_network_address_address_enumerator_finalize;
}

static GSocketAddressEnumerator *
g_network_address_connectable_enumerate (GSocketConnectable *connectable)
{
  GNetworkAddressAddressEnumerator *addr_enum;

  addr_enum = g_object_new (G_TYPE_NETWORK_ADDRESS_ADDRESS_ENUMERATOR, NULL);
  addr_enum->addr = g_object_ref (G_NETWORK_ADDRESS (connectable));

  return (GSocketAddressEnumerator *)addr_enum;
}

static GSocketAddressEnumerator *
g_network_address_connectable_proxy_enumerate (GSocketConnectable *connectable)
{
  GNetworkAddress *self = G_NETWORK_ADDRESS (connectable);
  GSocketAddressEnumerator *proxy_enum;
  gchar *uri;

  uri = _g_uri_from_authority (self->priv->scheme ? self->priv->scheme : "none",
                               self->priv->hostname,
                               self->priv->port,
                               NULL);

  proxy_enum = g_object_new (G_TYPE_PROXY_ADDRESS_ENUMERATOR,
                             "connectable", connectable,
      	       	       	     "uri", uri,
      	       	       	     NULL);

  g_free (uri);

  return proxy_enum;
}

static gchar *
g_network_address_connectable_to_string (GSocketConnectable *connectable)
{
  GNetworkAddress *addr;
  const gchar *scheme;
  guint16 port;
  GString *out;  /* owned */

  addr = G_NETWORK_ADDRESS (connectable);
  out = g_string_new ("");

  scheme = g_network_address_get_scheme (addr);
  if (scheme != NULL)
    g_string_append_printf (out, "%s:", scheme);

  g_string_append (out, g_network_address_get_hostname (addr));

  port = g_network_address_get_port (addr);
  if (port != 0)
    g_string_append_printf (out, ":%u", port);

  return g_string_free (out, FALSE);
}
