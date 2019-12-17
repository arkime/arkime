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
#include <glib.h>
#include <string.h>

#include "gsocketaddress.h"
#include "ginetaddress.h"
#include "ginetsocketaddress.h"
#include "gnativesocketaddress.h"
#include "gnetworkingprivate.h"
#include "gproxyaddress.h"
#include "gproxyaddressenumerator.h"
#include "gsocketaddressenumerator.h"
#include "gsocketconnectable.h"
#include "glibintl.h"
#include "gioenumtypes.h"

#ifdef G_OS_UNIX
#include "gunixsocketaddress.h"
#endif


/**
 * SECTION:gsocketaddress
 * @short_description: Abstract base class representing endpoints
 *     for socket communication
 * @include: gio/gio.h
 *
 * #GSocketAddress is the equivalent of struct sockaddr in the BSD
 * sockets API. This is an abstract class; use #GInetSocketAddress
 * for internet sockets, or #GUnixSocketAddress for UNIX domain sockets.
 */

/**
 * GSocketAddress:
 *
 * A socket endpoint address, corresponding to struct sockaddr
 * or one of its subtypes.
 */

enum
{
  PROP_NONE,
  PROP_FAMILY
};

static void                      g_socket_address_connectable_iface_init       (GSocketConnectableIface *iface);
static GSocketAddressEnumerator *g_socket_address_connectable_enumerate	       (GSocketConnectable      *connectable);
static GSocketAddressEnumerator *g_socket_address_connectable_proxy_enumerate  (GSocketConnectable      *connectable);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (GSocketAddress, g_socket_address, G_TYPE_OBJECT,
				  G_IMPLEMENT_INTERFACE (G_TYPE_SOCKET_CONNECTABLE,
							 g_socket_address_connectable_iface_init))

/**
 * g_socket_address_get_family:
 * @address: a #GSocketAddress
 *
 * Gets the socket family type of @address.
 *
 * Returns: the socket family type of @address
 *
 * Since: 2.22
 */
GSocketFamily
g_socket_address_get_family (GSocketAddress *address)
{
  g_return_val_if_fail (G_IS_SOCKET_ADDRESS (address), 0);

  return G_SOCKET_ADDRESS_GET_CLASS (address)->get_family (address);
}

static void
g_socket_address_get_property (GObject *object, guint prop_id,
			       GValue *value, GParamSpec *pspec)
{
  GSocketAddress *address = G_SOCKET_ADDRESS (object);

  switch (prop_id)
    {
     case PROP_FAMILY:
      g_value_set_enum (value, g_socket_address_get_family (address));
      break;

     default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
g_socket_address_class_init (GSocketAddressClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->get_property = g_socket_address_get_property;

  g_object_class_install_property (gobject_class, PROP_FAMILY,
                                   g_param_spec_enum ("family",
						      P_("Address family"),
						      P_("The family of the socket address"),
						      G_TYPE_SOCKET_FAMILY,
						      G_SOCKET_FAMILY_INVALID,
						      G_PARAM_READABLE |
                                                      G_PARAM_STATIC_STRINGS));
}

static void
g_socket_address_connectable_iface_init (GSocketConnectableIface *connectable_iface)
{
  connectable_iface->enumerate  = g_socket_address_connectable_enumerate;
  connectable_iface->proxy_enumerate  = g_socket_address_connectable_proxy_enumerate;
  /* to_string() is implemented by subclasses */
}

static void
g_socket_address_init (GSocketAddress *address)
{

}

/**
 * g_socket_address_get_native_size:
 * @address: a #GSocketAddress
 *
 * Gets the size of @address's native struct sockaddr.
 * You can use this to allocate memory to pass to
 * g_socket_address_to_native().
 *
 * Returns: the size of the native struct sockaddr that
 *     @address represents
 *
 * Since: 2.22
 */
gssize
g_socket_address_get_native_size (GSocketAddress *address)
{
  g_return_val_if_fail (G_IS_SOCKET_ADDRESS (address), -1);

  return G_SOCKET_ADDRESS_GET_CLASS (address)->get_native_size (address);
}

/**
 * g_socket_address_to_native:
 * @address: a #GSocketAddress
 * @dest: a pointer to a memory location that will contain the native
 * struct sockaddr
 * @destlen: the size of @dest. Must be at least as large as
 *     g_socket_address_get_native_size()
 * @error: #GError for error reporting, or %NULL to ignore
 *
 * Converts a #GSocketAddress to a native struct sockaddr, which can
 * be passed to low-level functions like connect() or bind().
 *
 * If not enough space is available, a %G_IO_ERROR_NO_SPACE error
 * is returned. If the address type is not known on the system
 * then a %G_IO_ERROR_NOT_SUPPORTED error is returned.
 *
 * Returns: %TRUE if @dest was filled in, %FALSE on error
 *
 * Since: 2.22
 */
gboolean
g_socket_address_to_native (GSocketAddress  *address,
			    gpointer         dest,
			    gsize            destlen,
			    GError         **error)
{
  g_return_val_if_fail (G_IS_SOCKET_ADDRESS (address), FALSE);

  return G_SOCKET_ADDRESS_GET_CLASS (address)->to_native (address, dest, destlen, error);
}

/**
 * g_socket_address_new_from_native:
 * @native: (not nullable): a pointer to a struct sockaddr
 * @len: the size of the memory location pointed to by @native
 *
 * Creates a #GSocketAddress subclass corresponding to the native
 * struct sockaddr @native.
 *
 * Returns: a new #GSocketAddress if @native could successfully
 *     be converted, otherwise %NULL
 *
 * Since: 2.22
 */
GSocketAddress *
g_socket_address_new_from_native (gpointer native,
				  gsize    len)
{
  gshort family;

  if (len < sizeof (gshort))
    return NULL;

  family = ((struct sockaddr *) native)->sa_family;

  if (family == AF_UNSPEC)
    return NULL;

  if (family == AF_INET)
    {
      struct sockaddr_in *addr = (struct sockaddr_in *) native;
      GInetAddress *iaddr;
      GSocketAddress *sockaddr;

      if (len < sizeof (*addr))
	return NULL;

      iaddr = g_inet_address_new_from_bytes ((guint8 *) &(addr->sin_addr), AF_INET);
      sockaddr = g_inet_socket_address_new (iaddr, g_ntohs (addr->sin_port));
      g_object_unref (iaddr);
      return sockaddr;
    }

  if (family == AF_INET6)
    {
      struct sockaddr_in6 *addr = (struct sockaddr_in6 *) native;
      GInetAddress *iaddr;
      GSocketAddress *sockaddr;

      if (len < sizeof (*addr))
	return NULL;

      if (IN6_IS_ADDR_V4MAPPED (&(addr->sin6_addr)))
	{
	  struct sockaddr_in sin_addr;

	  sin_addr.sin_family = AF_INET;
	  sin_addr.sin_port = addr->sin6_port;
	  memcpy (&(sin_addr.sin_addr.s_addr), addr->sin6_addr.s6_addr + 12, 4);
	  iaddr = g_inet_address_new_from_bytes ((guint8 *) &(sin_addr.sin_addr), AF_INET);
	}
      else
	{
	  iaddr = g_inet_address_new_from_bytes ((guint8 *) &(addr->sin6_addr), AF_INET6);
	}

      sockaddr = g_object_new (G_TYPE_INET_SOCKET_ADDRESS,
			       "address", iaddr,
			       "port", g_ntohs (addr->sin6_port),
			       "flowinfo", addr->sin6_flowinfo,
			       "scope_id", addr->sin6_scope_id,
			       NULL);
      g_object_unref (iaddr);
      return sockaddr;
    }

#ifdef G_OS_UNIX
  if (family == AF_UNIX)
    {
      struct sockaddr_un *addr = (struct sockaddr_un *) native;
      gint path_len = len - G_STRUCT_OFFSET (struct sockaddr_un, sun_path);

      if (path_len == 0)
	{
	  return g_unix_socket_address_new_with_type ("", 0,
						      G_UNIX_SOCKET_ADDRESS_ANONYMOUS);
	}
      else if (addr->sun_path[0] == 0)
	{
	  if (!g_unix_socket_address_abstract_names_supported ())
	    {
	      return g_unix_socket_address_new_with_type ("", 0,
							  G_UNIX_SOCKET_ADDRESS_ANONYMOUS);
	    }
	  else if (len < sizeof (*addr))
	    {
	      return g_unix_socket_address_new_with_type (addr->sun_path + 1,
							  path_len - 1,
							  G_UNIX_SOCKET_ADDRESS_ABSTRACT);
	    }
	  else
	    {
	      return g_unix_socket_address_new_with_type (addr->sun_path + 1,
							  path_len - 1,
							  G_UNIX_SOCKET_ADDRESS_ABSTRACT_PADDED);
	    }
	}
      else
	return g_unix_socket_address_new (addr->sun_path);
    }
#endif

  return g_native_socket_address_new (native, len);
}


#define G_TYPE_SOCKET_ADDRESS_ADDRESS_ENUMERATOR (_g_socket_address_address_enumerator_get_type ())
#define G_SOCKET_ADDRESS_ADDRESS_ENUMERATOR(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), G_TYPE_SOCKET_ADDRESS_ADDRESS_ENUMERATOR, GSocketAddressAddressEnumerator))

typedef struct {
  GSocketAddressEnumerator parent_instance;

  GSocketAddress *sockaddr;
} GSocketAddressAddressEnumerator;

typedef struct {
  GSocketAddressEnumeratorClass parent_class;

} GSocketAddressAddressEnumeratorClass;

static GType _g_socket_address_address_enumerator_get_type (void);
G_DEFINE_TYPE (GSocketAddressAddressEnumerator, _g_socket_address_address_enumerator, G_TYPE_SOCKET_ADDRESS_ENUMERATOR)

static void
g_socket_address_address_enumerator_finalize (GObject *object)
{
  GSocketAddressAddressEnumerator *sockaddr_enum =
    G_SOCKET_ADDRESS_ADDRESS_ENUMERATOR (object);

  if (sockaddr_enum->sockaddr)
    g_object_unref (sockaddr_enum->sockaddr);

  G_OBJECT_CLASS (_g_socket_address_address_enumerator_parent_class)->finalize (object);
}

static GSocketAddress *
g_socket_address_address_enumerator_next (GSocketAddressEnumerator  *enumerator,
					  GCancellable              *cancellable,
					  GError                   **error)
{
  GSocketAddressAddressEnumerator *sockaddr_enum =
    G_SOCKET_ADDRESS_ADDRESS_ENUMERATOR (enumerator);

  if (sockaddr_enum->sockaddr)
    {
      GSocketAddress *ret = sockaddr_enum->sockaddr;

      sockaddr_enum->sockaddr = NULL;
      return ret;
    }
  else
    return NULL;
}

static void
_g_socket_address_address_enumerator_init (GSocketAddressAddressEnumerator *enumerator)
{
}

static void
_g_socket_address_address_enumerator_class_init (GSocketAddressAddressEnumeratorClass *sockaddrenum_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (sockaddrenum_class);
  GSocketAddressEnumeratorClass *enumerator_class =
    G_SOCKET_ADDRESS_ENUMERATOR_CLASS (sockaddrenum_class);

  enumerator_class->next = g_socket_address_address_enumerator_next;
  object_class->finalize = g_socket_address_address_enumerator_finalize;
}

static GSocketAddressEnumerator *
g_socket_address_connectable_enumerate (GSocketConnectable *connectable)
{
  GSocketAddressAddressEnumerator *sockaddr_enum;

  sockaddr_enum = g_object_new (G_TYPE_SOCKET_ADDRESS_ADDRESS_ENUMERATOR, NULL);
  sockaddr_enum->sockaddr = g_object_ref (G_SOCKET_ADDRESS (connectable));

  return (GSocketAddressEnumerator *)sockaddr_enum;
}

static GSocketAddressEnumerator *
g_socket_address_connectable_proxy_enumerate (GSocketConnectable *connectable)
{
  GSocketAddressEnumerator *addr_enum = NULL;

  g_assert (connectable != NULL);

  if (G_IS_INET_SOCKET_ADDRESS (connectable) &&
      !G_IS_PROXY_ADDRESS (connectable))
    {
      GInetAddress *addr;
      guint port;
      gchar *uri;
      gchar *ip;

      g_object_get (connectable, "address", &addr, "port", &port, NULL);

      ip = g_inet_address_to_string (addr);
      uri = _g_uri_from_authority ("none", ip, port, NULL);

      addr_enum = g_object_new (G_TYPE_PROXY_ADDRESS_ENUMERATOR,
      	       	       	       	"connectable", connectable,
      	       	       	       	"uri", uri,
      	       	       	       	NULL);

      g_object_unref (addr);
      g_free (ip);
      g_free (uri);
    }
  else
    {
      addr_enum = g_socket_address_connectable_enumerate (connectable);
    }

  return addr_enum;
}
