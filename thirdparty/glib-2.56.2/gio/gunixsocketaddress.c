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

#include "gunixsocketaddress.h"
#include "gsocketconnectable.h"
#include "glibintl.h"
#include "gnetworking.h"


/**
 * SECTION:gunixsocketaddress
 * @short_description: UNIX GSocketAddress
 * @include: gio/gunixsocketaddress.h
 *
 * Support for UNIX-domain (also known as local) sockets.
 *
 * UNIX domain sockets are generally visible in the filesystem.
 * However, some systems support abstract socket names which are not
 * visible in the filesystem and not affected by the filesystem
 * permissions, visibility, etc. Currently this is only supported
 * under Linux. If you attempt to use abstract sockets on other
 * systems, function calls may return %G_IO_ERROR_NOT_SUPPORTED
 * errors. You can use g_unix_socket_address_abstract_names_supported()
 * to see if abstract names are supported.
 *
 * Note that `<gio/gunixsocketaddress.h>` belongs to the UNIX-specific GIO
 * interfaces, thus you have to use the `gio-unix-2.0.pc` pkg-config file
 * when using it.
 */

/**
 * GUnixSocketAddress:
 *
 * A UNIX-domain (local) socket address, corresponding to a
 * struct sockaddr_un.
 */

enum
{
  PROP_0,
  PROP_PATH,
  PROP_PATH_AS_ARRAY,
  PROP_ABSTRACT,
  PROP_ADDRESS_TYPE
};

#define UNIX_PATH_MAX sizeof (((struct sockaddr_un *) 0)->sun_path)

struct _GUnixSocketAddressPrivate
{
  char path[UNIX_PATH_MAX]; /* Not including the initial zero in abstract case, so
			       we can guarantee zero termination of abstract
			       pathnames in the get_path() API */
  gsize path_len; /* Not including any terminating zeros */
  GUnixSocketAddressType address_type;
};

static void   g_unix_socket_address_connectable_iface_init (GSocketConnectableIface *iface);
static gchar *g_unix_socket_address_connectable_to_string  (GSocketConnectable      *connectable);

G_DEFINE_TYPE_WITH_CODE (GUnixSocketAddress, g_unix_socket_address, G_TYPE_SOCKET_ADDRESS,
                         G_ADD_PRIVATE (GUnixSocketAddress)
                         G_IMPLEMENT_INTERFACE (G_TYPE_SOCKET_CONNECTABLE,
                                                g_unix_socket_address_connectable_iface_init))

static void
g_unix_socket_address_set_property (GObject      *object,
				    guint         prop_id,
				    const GValue *value,
				    GParamSpec   *pspec)
{
  GUnixSocketAddress *address = G_UNIX_SOCKET_ADDRESS (object);
  const char *str;
  GByteArray *array;
  gsize len;

  switch (prop_id)
    {
    case PROP_PATH:
      str = g_value_get_string (value);
      if (str)
	{
	  g_strlcpy (address->priv->path, str,
		     sizeof (address->priv->path));
	  address->priv->path_len = strlen (address->priv->path);
	}
      break;

    case PROP_PATH_AS_ARRAY:
      array = g_value_get_boxed (value);

      if (array)
	{
	  /* Clip to fit in UNIX_PATH_MAX with zero termination or first byte */
	  len = MIN (array->len, UNIX_PATH_MAX-1);

	  if (len != 0)
	    memcpy (address->priv->path, array->data, len);

	  address->priv->path[len] = 0; /* Ensure null-terminated */
	  address->priv->path_len = len;
	}
      break;

    case PROP_ABSTRACT:
      /* Only set it if it's not the default... */
      if (g_value_get_boolean (value))
       address->priv->address_type = G_UNIX_SOCKET_ADDRESS_ABSTRACT_PADDED;
      break;

    case PROP_ADDRESS_TYPE:
      /* Only set it if it's not the default... */
      if (g_value_get_enum (value) != G_UNIX_SOCKET_ADDRESS_PATH)
        address->priv->address_type = g_value_get_enum (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
g_unix_socket_address_get_property (GObject    *object,
				    guint       prop_id,
				    GValue     *value,
				    GParamSpec *pspec)
{
  GUnixSocketAddress *address = G_UNIX_SOCKET_ADDRESS (object);
  GByteArray *array;

  switch (prop_id)
    {
      case PROP_PATH:
	g_value_set_string (value, address->priv->path);
	break;

      case PROP_PATH_AS_ARRAY:
	array = g_byte_array_sized_new (address->priv->path_len);
	g_byte_array_append (array, (guint8 *)address->priv->path, address->priv->path_len);
	g_value_take_boxed (value, array);
	break;

      case PROP_ABSTRACT:
	g_value_set_boolean (value, (address->priv->address_type == G_UNIX_SOCKET_ADDRESS_ABSTRACT ||
				     address->priv->address_type == G_UNIX_SOCKET_ADDRESS_ABSTRACT_PADDED));

	break;

      case PROP_ADDRESS_TYPE:
	g_value_set_enum (value, address->priv->address_type);
	break;

      default:
	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static GSocketFamily
g_unix_socket_address_get_family (GSocketAddress *address)
{
  g_assert (PF_UNIX == G_SOCKET_FAMILY_UNIX);

  return G_SOCKET_FAMILY_UNIX;
}

static gssize
g_unix_socket_address_get_native_size (GSocketAddress *address)
{
  GUnixSocketAddress *addr = G_UNIX_SOCKET_ADDRESS (address);

  switch (addr->priv->address_type)
    {
    case G_UNIX_SOCKET_ADDRESS_ANONYMOUS:
      return G_STRUCT_OFFSET(struct sockaddr_un, sun_path);
    case G_UNIX_SOCKET_ADDRESS_ABSTRACT:
      return G_STRUCT_OFFSET(struct sockaddr_un, sun_path) + addr->priv->path_len + 1;
    default:
      return sizeof (struct sockaddr_un);
    }
}

static gboolean
g_unix_socket_address_to_native (GSocketAddress *address,
				 gpointer        dest,
				 gsize           destlen,
				 GError        **error)
{
  GUnixSocketAddress *addr = G_UNIX_SOCKET_ADDRESS (address);
  struct sockaddr_un *sock;
  gssize socklen;

  socklen = g_unix_socket_address_get_native_size (address);
  if (destlen < socklen)
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NO_SPACE,
			   _("Not enough space for socket address"));
      return FALSE;
    }

  sock = (struct sockaddr_un *) dest;
  memset (sock, 0, socklen);
  sock->sun_family = AF_UNIX;

  switch (addr->priv->address_type)
    {
    case G_UNIX_SOCKET_ADDRESS_INVALID:
    case G_UNIX_SOCKET_ADDRESS_ANONYMOUS:
      break;

    case G_UNIX_SOCKET_ADDRESS_PATH:
      strcpy (sock->sun_path, addr->priv->path);
      break;

    case G_UNIX_SOCKET_ADDRESS_ABSTRACT:
    case G_UNIX_SOCKET_ADDRESS_ABSTRACT_PADDED:
      if (!g_unix_socket_address_abstract_names_supported ())
	{
	  g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
			       _("Abstract UNIX domain socket addresses not supported on this system"));
	  return FALSE;
	}

      sock->sun_path[0] = 0;
      memcpy (sock->sun_path+1, addr->priv->path, addr->priv->path_len);
      break;
    }

  return TRUE;
}

static void
g_unix_socket_address_class_init (GUnixSocketAddressClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GSocketAddressClass *gsocketaddress_class = G_SOCKET_ADDRESS_CLASS (klass);

  gobject_class->set_property = g_unix_socket_address_set_property;
  gobject_class->get_property = g_unix_socket_address_get_property;

  gsocketaddress_class->get_family = g_unix_socket_address_get_family;
  gsocketaddress_class->to_native = g_unix_socket_address_to_native;
  gsocketaddress_class->get_native_size = g_unix_socket_address_get_native_size;

  g_object_class_install_property (gobject_class,
				   PROP_PATH,
				   g_param_spec_string ("path",
							P_("Path"),
							P_("UNIX socket path"),
							NULL,
							G_PARAM_READWRITE |
							G_PARAM_CONSTRUCT_ONLY |
							G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_PATH_AS_ARRAY,
				   g_param_spec_boxed ("path-as-array",
						       P_("Path array"),
						       P_("UNIX socket path, as byte array"),
						       G_TYPE_BYTE_ARRAY,
						       G_PARAM_READWRITE |
						       G_PARAM_CONSTRUCT_ONLY |
						       G_PARAM_STATIC_STRINGS));
  /**
   * GUnixSocketAddress:abstract:
   *
   * Whether or not this is an abstract address
   *
   * Deprecated: Use #GUnixSocketAddress:address-type, which
   * distinguishes between zero-padded and non-zero-padded
   * abstract addresses.
   */
  g_object_class_install_property (gobject_class, PROP_ABSTRACT,
				   g_param_spec_boolean ("abstract",
							 P_("Abstract"),
							 P_("Whether or not this is an abstract address"),
							 FALSE,
							 G_PARAM_READWRITE |
							 G_PARAM_CONSTRUCT_ONLY |
							 G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_ADDRESS_TYPE,
				   g_param_spec_enum ("address-type",
						      P_("Address type"),
						      P_("The type of UNIX socket address"),
						      G_TYPE_UNIX_SOCKET_ADDRESS_TYPE,
						      G_UNIX_SOCKET_ADDRESS_PATH,
						      G_PARAM_READWRITE |
						      G_PARAM_CONSTRUCT_ONLY |
						      G_PARAM_STATIC_STRINGS));
}

static void
g_unix_socket_address_connectable_iface_init (GSocketConnectableIface *iface)
{
  GSocketConnectableIface *parent_iface = g_type_interface_peek_parent (iface);

  iface->enumerate = parent_iface->enumerate;
  iface->proxy_enumerate = parent_iface->proxy_enumerate;
  iface->to_string = g_unix_socket_address_connectable_to_string;
}

static gchar *
g_unix_socket_address_connectable_to_string (GSocketConnectable *connectable)
{
  GUnixSocketAddress *ua;
  GString *out;
  const gchar *path;
  gsize path_len, i;

  ua = G_UNIX_SOCKET_ADDRESS (connectable);

  /* Anonymous sockets have no path. */
  if (ua->priv->address_type == G_UNIX_SOCKET_ADDRESS_ANONYMOUS)
    return g_strdup ("anonymous");

  path = g_unix_socket_address_get_path (ua);
  path_len = g_unix_socket_address_get_path_len (ua);
  out = g_string_sized_new (path_len);

  /* Return the #GUnixSocketAddress:path, but with all non-printable characters
   * (including nul bytes) escaped to hex. */
  for (i = 0; i < path_len; i++)
    {
      guint8 c = path[i];

      if (g_ascii_isprint (path[i]))
        g_string_append_c (out, c);
      else
        g_string_append_printf (out, "\\x%02x", (guint) c);
    }

  return g_string_free (out, FALSE);
}

static void
g_unix_socket_address_init (GUnixSocketAddress *address)
{
  address->priv = g_unix_socket_address_get_instance_private (address);

  memset (address->priv->path, 0, sizeof (address->priv->path));
  address->priv->path_len = -1;
  address->priv->address_type = G_UNIX_SOCKET_ADDRESS_PATH;
}

/**
 * g_unix_socket_address_new:
 * @path: the socket path
 *
 * Creates a new #GUnixSocketAddress for @path.
 *
 * To create abstract socket addresses, on systems that support that,
 * use g_unix_socket_address_new_abstract().
 *
 * Returns: a new #GUnixSocketAddress
 *
 * Since: 2.22
 */
GSocketAddress *
g_unix_socket_address_new (const gchar *path)
{
  return g_object_new (G_TYPE_UNIX_SOCKET_ADDRESS,
		       "path", path,
		       "abstract", FALSE,
		       NULL);
}

/**
 * g_unix_socket_address_new_abstract:
 * @path: (array length=path_len) (element-type gchar): the abstract name
 * @path_len: the length of @path, or -1
 *
 * Creates a new %G_UNIX_SOCKET_ADDRESS_ABSTRACT_PADDED
 * #GUnixSocketAddress for @path.
 *
 * Returns: a new #GUnixSocketAddress
 *
 * Deprecated: Use g_unix_socket_address_new_with_type().
 */
GSocketAddress *
g_unix_socket_address_new_abstract (const gchar *path,
				    gint         path_len)
{
  return g_unix_socket_address_new_with_type (path, path_len,
					      G_UNIX_SOCKET_ADDRESS_ABSTRACT_PADDED);
}

/**
 * g_unix_socket_address_new_with_type:
 * @path: (array length=path_len) (element-type gchar): the name
 * @path_len: the length of @path, or -1
 * @type: a #GUnixSocketAddressType
 *
 * Creates a new #GUnixSocketAddress of type @type with name @path.
 *
 * If @type is %G_UNIX_SOCKET_ADDRESS_PATH, this is equivalent to
 * calling g_unix_socket_address_new().
 *
 * If @type is %G_UNIX_SOCKET_ADDRESS_ANONYMOUS, @path and @path_len will be
 * ignored.
 *
 * If @path_type is %G_UNIX_SOCKET_ADDRESS_ABSTRACT, then @path_len
 * bytes of @path will be copied to the socket's path, and only those
 * bytes will be considered part of the name. (If @path_len is -1,
 * then @path is assumed to be NUL-terminated.) For example, if @path
 * was "test", then calling g_socket_address_get_native_size() on the
 * returned socket would return 7 (2 bytes of overhead, 1 byte for the
 * abstract-socket indicator byte, and 4 bytes for the name "test").
 *
 * If @path_type is %G_UNIX_SOCKET_ADDRESS_ABSTRACT_PADDED, then
 * @path_len bytes of @path will be copied to the socket's path, the
 * rest of the path will be padded with 0 bytes, and the entire
 * zero-padded buffer will be considered the name. (As above, if
 * @path_len is -1, then @path is assumed to be NUL-terminated.) In
 * this case, g_socket_address_get_native_size() will always return
 * the full size of a `struct sockaddr_un`, although
 * g_unix_socket_address_get_path_len() will still return just the
 * length of @path.
 *
 * %G_UNIX_SOCKET_ADDRESS_ABSTRACT is preferred over
 * %G_UNIX_SOCKET_ADDRESS_ABSTRACT_PADDED for new programs. Of course,
 * when connecting to a server created by another process, you must
 * use the appropriate type corresponding to how that process created
 * its listening socket.
 *
 * Returns: a new #GUnixSocketAddress
 *
 * Since: 2.26
 */
GSocketAddress *
g_unix_socket_address_new_with_type (const gchar            *path,
				     gint                    path_len,
				     GUnixSocketAddressType  type)
{
  GSocketAddress *address;
  GByteArray *array;

  if (type == G_UNIX_SOCKET_ADDRESS_ANONYMOUS)
    path_len = 0;
  else if (path_len == -1)
    path_len = strlen (path);

  array = g_byte_array_sized_new (path_len);

  g_byte_array_append (array, (guint8 *)path, path_len);

  address = g_object_new (G_TYPE_UNIX_SOCKET_ADDRESS,
			  "path-as-array", array,
			  "address-type", type,
			  NULL);

  g_byte_array_unref (array);

  return address;
}

/**
 * g_unix_socket_address_get_path:
 * @address: a #GInetSocketAddress
 *
 * Gets @address's path, or for abstract sockets the "name".
 *
 * Guaranteed to be zero-terminated, but an abstract socket
 * may contain embedded zeros, and thus you should use
 * g_unix_socket_address_get_path_len() to get the true length
 * of this string.
 *
 * Returns: the path for @address
 *
 * Since: 2.22
 */
const char *
g_unix_socket_address_get_path (GUnixSocketAddress *address)
{
  return address->priv->path;
}

/**
 * g_unix_socket_address_get_path_len:
 * @address: a #GInetSocketAddress
 *
 * Gets the length of @address's path.
 *
 * For details, see g_unix_socket_address_get_path().
 *
 * Returns: the length of the path
 *
 * Since: 2.22
 */
gsize
g_unix_socket_address_get_path_len (GUnixSocketAddress *address)
{
  return address->priv->path_len;
}

/**
 * g_unix_socket_address_get_address_type:
 * @address: a #GInetSocketAddress
 *
 * Gets @address's type.
 *
 * Returns: a #GUnixSocketAddressType
 *
 * Since: 2.26
 */
GUnixSocketAddressType
g_unix_socket_address_get_address_type (GUnixSocketAddress *address)
{
  return address->priv->address_type;
}

/**
 * g_unix_socket_address_get_is_abstract:
 * @address: a #GInetSocketAddress
 *
 * Tests if @address is abstract.
 *
 * Returns: %TRUE if the address is abstract, %FALSE otherwise
 *
 * Since: 2.22
 *
 * Deprecated: Use g_unix_socket_address_get_address_type()
 */
gboolean
g_unix_socket_address_get_is_abstract (GUnixSocketAddress *address)
{
  return (address->priv->address_type == G_UNIX_SOCKET_ADDRESS_ABSTRACT ||
	  address->priv->address_type == G_UNIX_SOCKET_ADDRESS_ABSTRACT_PADDED);
}

/**
 * g_unix_socket_address_abstract_names_supported:
 *
 * Checks if abstract UNIX domain socket names are supported.
 *
 * Returns: %TRUE if supported, %FALSE otherwise
 *
 * Since: 2.22
 */
gboolean
g_unix_socket_address_abstract_names_supported (void)
{
#ifdef __linux__
  return TRUE;
#else
  return FALSE;
#endif
}
