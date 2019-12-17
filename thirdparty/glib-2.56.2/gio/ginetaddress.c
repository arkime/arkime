/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright (C) 2008 Christian Kellner, Samuel Cormier-Iijima
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
 * Authors: Christian Kellner <gicmo@gnome.org>
 *          Samuel Cormier-Iijima <sciyoshi@gmail.com>
 */

#include <config.h>

#include <string.h>

#include <glib.h>

#include "ginetaddress.h"
#include "gioenums.h"
#include "gioenumtypes.h"
#include "glibintl.h"
#include "gnetworkingprivate.h"

#ifdef G_OS_WIN32
/* Ensure Windows XP runtime compatibility, while using
 * inet_pton() and inet_ntop() if available
 */
#include "gwin32networking.h"
#endif

struct _GInetAddressPrivate
{
  GSocketFamily family;
  union {
    struct in_addr ipv4;
    struct in6_addr ipv6;
  } addr;
};

/**
 * SECTION:ginetaddress
 * @short_description: An IPv4/IPv6 address
 * @include: gio/gio.h
 *
 * #GInetAddress represents an IPv4 or IPv6 internet address. Use
 * g_resolver_lookup_by_name() or g_resolver_lookup_by_name_async() to
 * look up the #GInetAddress for a hostname. Use
 * g_resolver_lookup_by_address() or
 * g_resolver_lookup_by_address_async() to look up the hostname for a
 * #GInetAddress.
 *
 * To actually connect to a remote host, you will need a
 * #GInetSocketAddress (which includes a #GInetAddress as well as a
 * port number).
 */

/**
 * GInetAddress:
 *
 * An IPv4 or IPv6 internet address.
 */

G_DEFINE_TYPE_WITH_CODE (GInetAddress, g_inet_address, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (GInetAddress)
			 g_networking_init ();)

enum
{
  PROP_0,
  PROP_FAMILY,
  PROP_BYTES,
  PROP_IS_ANY,
  PROP_IS_LOOPBACK,
  PROP_IS_LINK_LOCAL,
  PROP_IS_SITE_LOCAL,
  PROP_IS_MULTICAST,
  PROP_IS_MC_GLOBAL,
  PROP_IS_MC_LINK_LOCAL,
  PROP_IS_MC_NODE_LOCAL,
  PROP_IS_MC_ORG_LOCAL,
  PROP_IS_MC_SITE_LOCAL,
};

static void
g_inet_address_set_property (GObject      *object,
			     guint         prop_id,
			     const GValue *value,
			     GParamSpec   *pspec)
{
  GInetAddress *address = G_INET_ADDRESS (object);

  switch (prop_id)
    {
    case PROP_FAMILY:
      address->priv->family = g_value_get_enum (value);
      break;

    case PROP_BYTES:
      memcpy (&address->priv->addr, g_value_get_pointer (value),
	      address->priv->family == AF_INET ?
	      sizeof (address->priv->addr.ipv4) :
	      sizeof (address->priv->addr.ipv6));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }

}

static void
g_inet_address_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  GInetAddress *address = G_INET_ADDRESS (object);

  switch (prop_id)
    {
    case PROP_FAMILY:
      g_value_set_enum (value, address->priv->family);
      break;

    case PROP_BYTES:
      g_value_set_pointer (value, &address->priv->addr);
      break;

    case PROP_IS_ANY:
      g_value_set_boolean (value, g_inet_address_get_is_any (address));
      break;

    case PROP_IS_LOOPBACK:
      g_value_set_boolean (value, g_inet_address_get_is_loopback (address));
      break;

    case PROP_IS_LINK_LOCAL:
      g_value_set_boolean (value, g_inet_address_get_is_link_local (address));
      break;

    case PROP_IS_SITE_LOCAL:
      g_value_set_boolean (value, g_inet_address_get_is_site_local (address));
      break;

    case PROP_IS_MULTICAST:
      g_value_set_boolean (value, g_inet_address_get_is_multicast (address));
      break;

    case PROP_IS_MC_GLOBAL:
      g_value_set_boolean (value, g_inet_address_get_is_mc_global (address));
      break;

    case PROP_IS_MC_LINK_LOCAL:
      g_value_set_boolean (value, g_inet_address_get_is_mc_link_local (address));
      break;

    case PROP_IS_MC_NODE_LOCAL:
      g_value_set_boolean (value, g_inet_address_get_is_mc_node_local (address));
      break;

    case PROP_IS_MC_ORG_LOCAL:
      g_value_set_boolean (value, g_inet_address_get_is_mc_org_local (address));
      break;

    case PROP_IS_MC_SITE_LOCAL:
      g_value_set_boolean (value, g_inet_address_get_is_mc_site_local (address));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
g_inet_address_class_init (GInetAddressClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = g_inet_address_set_property;
  gobject_class->get_property = g_inet_address_get_property;

  g_object_class_install_property (gobject_class, PROP_FAMILY,
                                   g_param_spec_enum ("family",
						      P_("Address family"),
						      P_("The address family (IPv4 or IPv6)"),
						      G_TYPE_SOCKET_FAMILY,
						      G_SOCKET_FAMILY_INVALID,
						      G_PARAM_READWRITE |
                                                      G_PARAM_CONSTRUCT_ONLY |
                                                      G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_BYTES,
                                   g_param_spec_pointer ("bytes",
							 P_("Bytes"),
							 P_("The raw address data"),
							 G_PARAM_READWRITE |
                                                         G_PARAM_CONSTRUCT_ONLY |
                                                         G_PARAM_STATIC_STRINGS));

  /**
   * GInetAddress:is-any:
   *
   * Whether this is the "any" address for its family.
   * See g_inet_address_get_is_any().
   *
   * Since: 2.22
   */
  g_object_class_install_property (gobject_class, PROP_IS_ANY,
                                   g_param_spec_boolean ("is-any",
                                                         P_("Is any"),
                                                         P_("Whether this is the \"any\" address for its family"),
                                                         FALSE,
                                                         G_PARAM_READABLE |
                                                         G_PARAM_STATIC_STRINGS));

  /**
   * GInetAddress:is-link-local:
   *
   * Whether this is a link-local address.
   * See g_inet_address_get_is_link_local().
   *
   * Since: 2.22
   */
  g_object_class_install_property (gobject_class, PROP_IS_LINK_LOCAL,
                                   g_param_spec_boolean ("is-link-local",
                                                         P_("Is link-local"),
                                                         P_("Whether this is a link-local address"),
                                                         FALSE,
                                                         G_PARAM_READABLE |
                                                         G_PARAM_STATIC_STRINGS));

  /**
   * GInetAddress:is-loopback:
   *
   * Whether this is the loopback address for its family.
   * See g_inet_address_get_is_loopback().
   *
   * Since: 2.22
   */
  g_object_class_install_property (gobject_class, PROP_IS_LOOPBACK,
                                   g_param_spec_boolean ("is-loopback",
                                                         P_("Is loopback"),
                                                         P_("Whether this is the loopback address for its family"),
                                                         FALSE,
                                                         G_PARAM_READABLE |
                                                         G_PARAM_STATIC_STRINGS));

  /**
   * GInetAddress:is-site-local:
   *
   * Whether this is a site-local address.
   * See g_inet_address_get_is_loopback().
   *
   * Since: 2.22
   */
  g_object_class_install_property (gobject_class, PROP_IS_SITE_LOCAL,
                                   g_param_spec_boolean ("is-site-local",
                                                         P_("Is site-local"),
                                                         P_("Whether this is a site-local address"),
                                                         FALSE,
                                                         G_PARAM_READABLE |
                                                         G_PARAM_STATIC_STRINGS));

  /**
   * GInetAddress:is-multicast:
   *
   * Whether this is a multicast address.
   * See g_inet_address_get_is_multicast().
   *
   * Since: 2.22
   */
  g_object_class_install_property (gobject_class, PROP_IS_MULTICAST,
                                   g_param_spec_boolean ("is-multicast",
                                                         P_("Is multicast"),
                                                         P_("Whether this is a multicast address"),
                                                         FALSE,
                                                         G_PARAM_READABLE |
                                                         G_PARAM_STATIC_STRINGS));

  /**
   * GInetAddress:is-mc-global:
   *
   * Whether this is a global multicast address.
   * See g_inet_address_get_is_mc_global().
   *
   * Since: 2.22
   */
  g_object_class_install_property (gobject_class, PROP_IS_MC_GLOBAL,
                                   g_param_spec_boolean ("is-mc-global",
                                                         P_("Is multicast global"),
                                                         P_("Whether this is a global multicast address"),
                                                         FALSE,
                                                         G_PARAM_READABLE |
                                                         G_PARAM_STATIC_STRINGS));


  /**
   * GInetAddress:is-mc-link-local:
   *
   * Whether this is a link-local multicast address.
   * See g_inet_address_get_is_mc_link_local().
   *
   * Since: 2.22
   */
  g_object_class_install_property (gobject_class, PROP_IS_MC_LINK_LOCAL,
                                   g_param_spec_boolean ("is-mc-link-local",
                                                         P_("Is multicast link-local"),
                                                         P_("Whether this is a link-local multicast address"),
                                                         FALSE,
                                                         G_PARAM_READABLE |
                                                         G_PARAM_STATIC_STRINGS));

  /**
   * GInetAddress:is-mc-node-local:
   *
   * Whether this is a node-local multicast address.
   * See g_inet_address_get_is_mc_node_local().
   *
   * Since: 2.22
   */
  g_object_class_install_property (gobject_class, PROP_IS_MC_NODE_LOCAL,
                                   g_param_spec_boolean ("is-mc-node-local",
                                                         P_("Is multicast node-local"),
                                                         P_("Whether this is a node-local multicast address"),
                                                         FALSE,
                                                         G_PARAM_READABLE |
                                                         G_PARAM_STATIC_STRINGS));

  /**
   * GInetAddress:is-mc-org-local:
   *
   * Whether this is an organization-local multicast address.
   * See g_inet_address_get_is_mc_org_local().
   *
   * Since: 2.22
   */
  g_object_class_install_property (gobject_class, PROP_IS_MC_ORG_LOCAL,
                                   g_param_spec_boolean ("is-mc-org-local",
                                                         P_("Is multicast org-local"),
                                                         P_("Whether this is an organization-local multicast address"),
                                                         FALSE,
                                                         G_PARAM_READABLE |
                                                         G_PARAM_STATIC_STRINGS));

  /**
   * GInetAddress:is-mc-site-local:
   *
   * Whether this is a site-local multicast address.
   * See g_inet_address_get_is_mc_site_local().
   *
   * Since: 2.22
   */
  g_object_class_install_property (gobject_class, PROP_IS_MC_SITE_LOCAL,
                                   g_param_spec_boolean ("is-mc-site-local",
                                                         P_("Is multicast site-local"),
                                                         P_("Whether this is a site-local multicast address"),
                                                         FALSE,
                                                         G_PARAM_READABLE |
                                                         G_PARAM_STATIC_STRINGS));
}

static void
g_inet_address_init (GInetAddress *address)
{
  address->priv = g_inet_address_get_instance_private (address);
}

/* These are provided so that we can use inet_pton() and inet_ntop() on Windows
 * if they are available (i.e. Vista and later), and use the existing code path
 * on Windows XP/Server 2003.  We can drop this portion when we drop support for
 * XP/Server 2003.
 */
#if defined(G_OS_WIN32) && _WIN32_WINNT < 0x0600
static gint
inet_pton (gint family,
           const gchar *addr_string,
           gpointer addr)
{
  /* For Vista/Server 2008 and later, there is native inet_pton() in Winsock2 */
  if (ws2funcs.pInetPton != NULL)
    return ws2funcs.pInetPton (family, addr_string, addr);
  else
    {
      /* Fallback codepath for XP/Server 2003 */
      struct sockaddr_storage sa;
      struct sockaddr_in *sin = (struct sockaddr_in *)&sa;
      struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)&sa;
      gint len = sizeof (sa);

      if (family != AF_INET && family != AF_INET6)
        {
          WSASetLastError (WSAEAFNOSUPPORT);
          return -1;
        }

      /* WSAStringToAddress() will accept various not-an-IP-address
       * strings like "127.0.0.1:80", "[1234::5678]:80", "127.1", etc.
       */
      if (!g_hostname_is_ip_address (addr_string))
        return 0;

      if (WSAStringToAddress ((LPTSTR) addr_string, family, NULL, (LPSOCKADDR) &sa, &len) != 0)
        return 0;

      if (family == AF_INET)
        *(IN_ADDR *)addr = sin->sin_addr;
      else
        *(IN6_ADDR *)addr = sin6->sin6_addr;

      return 1;
    }
}

static const gchar *
inet_ntop (gint family,
           const gpointer addr,
           gchar *addr_str,
           socklen_t size)
{
  /* On Vista/Server 2008 and later, there is native inet_ntop() in Winsock2 */
  if (ws2funcs.pInetNtop != NULL)
    return ws2funcs.pInetNtop (family, addr, addr_str, size);
  else
    {
      /* Fallback codepath for XP/Server 2003 */
      DWORD buflen = size, addrlen;
      struct sockaddr_storage sa;
      struct sockaddr_in *sin = (struct sockaddr_in *)&sa;
      struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)&sa;

      memset (&sa, 0, sizeof (sa));
      sa.ss_family = family;
      if (sa.ss_family == AF_INET)
        {
          struct in_addr *addrv4 = (struct in_addr *) addr;

          addrlen = sizeof (*sin);
          memcpy (&sin->sin_addr, addrv4, sizeof (sin->sin_addr));
        }
      else if (sa.ss_family == AF_INET6)
        {
          struct in6_addr *addrv6 = (struct in6_addr *) addr;

          addrlen = sizeof (*sin6);
          memcpy (&sin6->sin6_addr, addrv6, sizeof (sin6->sin6_addr));
        }
      else
        {
          WSASetLastError (WSAEAFNOSUPPORT);
          return NULL;
        }
      if (WSAAddressToString ((LPSOCKADDR) &sa, addrlen, NULL, addr_str, &buflen) == 0)
        return addr_str;
      else
        return NULL;
    }
}
#endif

/**
 * g_inet_address_new_from_string:
 * @string: a string representation of an IP address
 *
 * Parses @string as an IP address and creates a new #GInetAddress.
 *
 * Returns: a new #GInetAddress corresponding to @string, or %NULL if
 * @string could not be parsed.
 *     Free the returned object with g_object_unref().
 *
 * Since: 2.22
 */
GInetAddress *
g_inet_address_new_from_string (const gchar *string)
{
  struct in_addr in_addr;
  struct in6_addr in6_addr;

  g_return_val_if_fail (string != NULL, NULL);

  /* If this GInetAddress is the first networking-related object to be
   * created, then we won't have called g_networking_init() yet at
   * this point.
   */
  g_networking_init ();

  if (inet_pton (AF_INET, string, &in_addr) > 0)
    return g_inet_address_new_from_bytes ((guint8 *)&in_addr, AF_INET);
  else if (inet_pton (AF_INET6, string, &in6_addr) > 0)
    return g_inet_address_new_from_bytes ((guint8 *)&in6_addr, AF_INET6);

  return NULL;
}

#define G_INET_ADDRESS_FAMILY_IS_VALID(family) ((family) == AF_INET || (family) == AF_INET6)

/**
 * g_inet_address_new_from_bytes:
 * @bytes: (array) (element-type guint8): raw address data
 * @family: the address family of @bytes
 *
 * Creates a new #GInetAddress from the given @family and @bytes.
 * @bytes should be 4 bytes for %G_SOCKET_FAMILY_IPV4 and 16 bytes for
 * %G_SOCKET_FAMILY_IPV6.
 *
 * Returns: a new #GInetAddress corresponding to @family and @bytes.
 *     Free the returned object with g_object_unref().
 *
 * Since: 2.22
 */
GInetAddress *
g_inet_address_new_from_bytes (const guint8         *bytes,
			       GSocketFamily  family)
{
  g_return_val_if_fail (G_INET_ADDRESS_FAMILY_IS_VALID (family), NULL);

  return g_object_new (G_TYPE_INET_ADDRESS,
		       "family", family,
		       "bytes", bytes,
		       NULL);
}

/**
 * g_inet_address_new_loopback:
 * @family: the address family
 *
 * Creates a #GInetAddress for the loopback address for @family.
 *
 * Returns: a new #GInetAddress corresponding to the loopback address
 * for @family.
 *     Free the returned object with g_object_unref().
 *
 * Since: 2.22
 */
GInetAddress *
g_inet_address_new_loopback (GSocketFamily family)
{
  g_return_val_if_fail (G_INET_ADDRESS_FAMILY_IS_VALID (family), NULL);

  if (family == AF_INET)
    {    
      guint8 addr[4] = {127, 0, 0, 1};

      return g_inet_address_new_from_bytes (addr, family);
    }
  else
    return g_inet_address_new_from_bytes (in6addr_loopback.s6_addr, family);
}

/**
 * g_inet_address_new_any:
 * @family: the address family
 *
 * Creates a #GInetAddress for the "any" address (unassigned/"don't
 * care") for @family.
 *
 * Returns: a new #GInetAddress corresponding to the "any" address
 * for @family.
 *     Free the returned object with g_object_unref().
 *
 * Since: 2.22
 */
GInetAddress *
g_inet_address_new_any (GSocketFamily family)
{
  g_return_val_if_fail (G_INET_ADDRESS_FAMILY_IS_VALID (family), NULL);

  if (family == AF_INET)
    {    
      guint8 addr[4] = {0, 0, 0, 0};

      return g_inet_address_new_from_bytes (addr, family);
    }
  else
    return g_inet_address_new_from_bytes (in6addr_any.s6_addr, family);
}


/**
 * g_inet_address_to_string:
 * @address: a #GInetAddress
 *
 * Converts @address to string form.
 *
 * Returns: a representation of @address as a string, which should be
 * freed after use.
 *
 * Since: 2.22
 */
gchar *
g_inet_address_to_string (GInetAddress *address)
{
  gchar buffer[INET6_ADDRSTRLEN];

  g_return_val_if_fail (G_IS_INET_ADDRESS (address), NULL);

  if (address->priv->family == AF_INET)
    inet_ntop (AF_INET, &address->priv->addr.ipv4, buffer, sizeof (buffer));
  else
    inet_ntop (AF_INET6, &address->priv->addr.ipv6, buffer, sizeof (buffer));

  return g_strdup (buffer);
}

/**
 * g_inet_address_to_bytes: (skip)
 * @address: a #GInetAddress
 *
 * Gets the raw binary address data from @address.
 *
 * Returns: a pointer to an internal array of the bytes in @address,
 * which should not be modified, stored, or freed. The size of this
 * array can be gotten with g_inet_address_get_native_size().
 *
 * Since: 2.22
 */
const guint8 *
g_inet_address_to_bytes (GInetAddress *address)
{
  g_return_val_if_fail (G_IS_INET_ADDRESS (address), NULL);

  return (guint8 *)&address->priv->addr;
}

/**
 * g_inet_address_get_native_size:
 * @address: a #GInetAddress
 *
 * Gets the size of the native raw binary address for @address. This
 * is the size of the data that you get from g_inet_address_to_bytes().
 *
 * Returns: the number of bytes used for the native version of @address.
 *
 * Since: 2.22
 */
gsize
g_inet_address_get_native_size (GInetAddress *address)
{
  if (address->priv->family == AF_INET)
    return sizeof (address->priv->addr.ipv4);
  return sizeof (address->priv->addr.ipv6);
}

/**
 * g_inet_address_get_family:
 * @address: a #GInetAddress
 *
 * Gets @address's family
 *
 * Returns: @address's family
 *
 * Since: 2.22
 */
GSocketFamily
g_inet_address_get_family (GInetAddress *address)
{
  g_return_val_if_fail (G_IS_INET_ADDRESS (address), FALSE);

  return address->priv->family;
}

/**
 * g_inet_address_get_is_any:
 * @address: a #GInetAddress
 *
 * Tests whether @address is the "any" address for its family.
 *
 * Returns: %TRUE if @address is the "any" address for its family.
 *
 * Since: 2.22
 */
gboolean
g_inet_address_get_is_any (GInetAddress *address)
{
  g_return_val_if_fail (G_IS_INET_ADDRESS (address), FALSE);

  if (address->priv->family == AF_INET)
    {
      guint32 addr4 = g_ntohl (address->priv->addr.ipv4.s_addr);

      return addr4 == INADDR_ANY;
    }
  else
    return IN6_IS_ADDR_UNSPECIFIED (&address->priv->addr.ipv6);
}

/**
 * g_inet_address_get_is_loopback:
 * @address: a #GInetAddress
 *
 * Tests whether @address is the loopback address for its family.
 *
 * Returns: %TRUE if @address is the loopback address for its family.
 *
 * Since: 2.22
 */
gboolean
g_inet_address_get_is_loopback (GInetAddress *address)
{
  g_return_val_if_fail (G_IS_INET_ADDRESS (address), FALSE);

  if (address->priv->family == AF_INET)
    {
      guint32 addr4 = g_ntohl (address->priv->addr.ipv4.s_addr);

      /* 127.0.0.0/8 */
      return ((addr4 & 0xff000000) == 0x7f000000);
    }
  else
    return IN6_IS_ADDR_LOOPBACK (&address->priv->addr.ipv6);
}

/**
 * g_inet_address_get_is_link_local:
 * @address: a #GInetAddress
 *
 * Tests whether @address is a link-local address (that is, if it
 * identifies a host on a local network that is not connected to the
 * Internet).
 *
 * Returns: %TRUE if @address is a link-local address.
 *
 * Since: 2.22
 */
gboolean
g_inet_address_get_is_link_local (GInetAddress *address)
{
  g_return_val_if_fail (G_IS_INET_ADDRESS (address), FALSE);

  if (address->priv->family == AF_INET)
    {
      guint32 addr4 = g_ntohl (address->priv->addr.ipv4.s_addr);

      /* 169.254.0.0/16 */
      return ((addr4 & 0xffff0000) == 0xa9fe0000);
    }
  else
    return IN6_IS_ADDR_LINKLOCAL (&address->priv->addr.ipv6);
}

/**
 * g_inet_address_get_is_site_local:
 * @address: a #GInetAddress
 *
 * Tests whether @address is a site-local address such as 10.0.0.1
 * (that is, the address identifies a host on a local network that can
 * not be reached directly from the Internet, but which may have
 * outgoing Internet connectivity via a NAT or firewall).
 *
 * Returns: %TRUE if @address is a site-local address.
 *
 * Since: 2.22
 */
gboolean
g_inet_address_get_is_site_local (GInetAddress *address)
{
  g_return_val_if_fail (G_IS_INET_ADDRESS (address), FALSE);

  if (address->priv->family == AF_INET)
    {
      guint32 addr4 = g_ntohl (address->priv->addr.ipv4.s_addr);

      /* 10.0.0.0/8, 172.16.0.0/12, 192.168.0.0/16 */
      return ((addr4 & 0xff000000) == 0x0a000000 ||
	      (addr4 & 0xfff00000) == 0xac100000 ||
	      (addr4 & 0xffff0000) == 0xc0a80000);
    }
  else
    return IN6_IS_ADDR_SITELOCAL (&address->priv->addr.ipv6);
}

/**
 * g_inet_address_get_is_multicast:
 * @address: a #GInetAddress
 *
 * Tests whether @address is a multicast address.
 *
 * Returns: %TRUE if @address is a multicast address.
 *
 * Since: 2.22
 */
gboolean
g_inet_address_get_is_multicast (GInetAddress *address)
{
  g_return_val_if_fail (G_IS_INET_ADDRESS (address), FALSE);

  if (address->priv->family == AF_INET)
    {
      guint32 addr4 = g_ntohl (address->priv->addr.ipv4.s_addr);

      return IN_MULTICAST (addr4);
    }
  else
    return IN6_IS_ADDR_MULTICAST (&address->priv->addr.ipv6);
}

/**
 * g_inet_address_get_is_mc_global:
 * @address: a #GInetAddress
 *
 * Tests whether @address is a global multicast address.
 *
 * Returns: %TRUE if @address is a global multicast address.
 *
 * Since: 2.22
 */
gboolean
g_inet_address_get_is_mc_global (GInetAddress *address)
{
  g_return_val_if_fail (G_IS_INET_ADDRESS (address), FALSE);

  if (address->priv->family == AF_INET)
    return FALSE;
  else
    return IN6_IS_ADDR_MC_GLOBAL (&address->priv->addr.ipv6);
}

/**
 * g_inet_address_get_is_mc_link_local:
 * @address: a #GInetAddress
 *
 * Tests whether @address is a link-local multicast address.
 *
 * Returns: %TRUE if @address is a link-local multicast address.
 *
 * Since: 2.22
 */
gboolean
g_inet_address_get_is_mc_link_local (GInetAddress *address)
{
  g_return_val_if_fail (G_IS_INET_ADDRESS (address), FALSE);

  if (address->priv->family == AF_INET)
    return FALSE;
  else
    return IN6_IS_ADDR_MC_LINKLOCAL (&address->priv->addr.ipv6);
}

/**
 * g_inet_address_get_is_mc_node_local:
 * @address: a #GInetAddress
 *
 * Tests whether @address is a node-local multicast address.
 *
 * Returns: %TRUE if @address is a node-local multicast address.
 *
 * Since: 2.22
 */
gboolean
g_inet_address_get_is_mc_node_local (GInetAddress *address)
{
  g_return_val_if_fail (G_IS_INET_ADDRESS (address), FALSE);

  if (address->priv->family == AF_INET)
    return FALSE;
  else
    return IN6_IS_ADDR_MC_NODELOCAL (&address->priv->addr.ipv6);
}

/**
 * g_inet_address_get_is_mc_org_local:
 * @address: a #GInetAddress
 *
 * Tests whether @address is an organization-local multicast address.
 *
 * Returns: %TRUE if @address is an organization-local multicast address.
 *
 * Since: 2.22
 */
gboolean
g_inet_address_get_is_mc_org_local  (GInetAddress *address)
{
  g_return_val_if_fail (G_IS_INET_ADDRESS (address), FALSE);

  if (address->priv->family == AF_INET)
    return FALSE;
  else
    return IN6_IS_ADDR_MC_ORGLOCAL (&address->priv->addr.ipv6);
}

/**
 * g_inet_address_get_is_mc_site_local:
 * @address: a #GInetAddress
 *
 * Tests whether @address is a site-local multicast address.
 *
 * Returns: %TRUE if @address is a site-local multicast address.
 *
 * Since: 2.22
 */
gboolean
g_inet_address_get_is_mc_site_local (GInetAddress *address)
{
  g_return_val_if_fail (G_IS_INET_ADDRESS (address), FALSE);

  if (address->priv->family == AF_INET)
    return FALSE;
  else
    return IN6_IS_ADDR_MC_SITELOCAL (&address->priv->addr.ipv6);
}

/**
 * g_inet_address_equal:
 * @address: A #GInetAddress.
 * @other_address: Another #GInetAddress.
 *
 * Checks if two #GInetAddress instances are equal, e.g. the same address.
 *
 * Returns: %TRUE if @address and @other_address are equal, %FALSE otherwise.
 *
 * Since: 2.30
 */
gboolean
g_inet_address_equal (GInetAddress *address,
                      GInetAddress *other_address)
{
  g_return_val_if_fail (G_IS_INET_ADDRESS (address), FALSE);
  g_return_val_if_fail (G_IS_INET_ADDRESS (other_address), FALSE);

  if (g_inet_address_get_family (address) != g_inet_address_get_family (other_address))
    return FALSE;

  if (memcmp (g_inet_address_to_bytes (address),
              g_inet_address_to_bytes (other_address),
              g_inet_address_get_native_size (address)) != 0)
    return FALSE;

  return TRUE;
}
