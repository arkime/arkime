/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright 2011 Red Hat, Inc.
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

#include <config.h>

#include <stdlib.h>
#include <string.h>

#include "ginetaddressmask.h"
#include "ginetaddress.h"
#include "ginitable.h"
#include "gioerror.h"
#include "gioenumtypes.h"
#include "glibintl.h"

/**
 * SECTION:ginetaddressmask
 * @short_description: An IPv4/IPv6 address mask
 * @include: gio/gio.h
 *
 * #GInetAddressMask represents a range of IPv4 or IPv6 addresses
 * described by a base address and a length indicating how many bits
 * of the base address are relevant for matching purposes. These are
 * often given in string form. Eg, "10.0.0.0/8", or "fe80::/10".
 */

/**
 * GInetAddressMask:
 *
 * A combination of an IPv4 or IPv6 base address and a length,
 * representing a range of IP addresses.
 *
 * Since: 2.32
 */

struct _GInetAddressMaskPrivate
{
  GInetAddress *addr;
  guint         length;
};

static void     g_inet_address_mask_initable_iface_init (GInitableIface  *iface);

G_DEFINE_TYPE_WITH_CODE (GInetAddressMask, g_inet_address_mask, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (GInetAddressMask)
			 G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
						g_inet_address_mask_initable_iface_init))

enum
{
  PROP_0,
  PROP_FAMILY,
  PROP_ADDRESS,
  PROP_LENGTH
};

static void
g_inet_address_mask_set_property (GObject      *object,
				  guint         prop_id,
				  const GValue *value,
				  GParamSpec   *pspec)
{
  GInetAddressMask *mask = G_INET_ADDRESS_MASK (object);

  switch (prop_id)
    {
    case PROP_ADDRESS:
      if (mask->priv->addr)
	g_object_unref (mask->priv->addr);
      mask->priv->addr = g_value_dup_object (value);
      break;

    case PROP_LENGTH:
      mask->priv->length = g_value_get_uint (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }

}

static void
g_inet_address_mask_get_property (GObject    *object,
				  guint       prop_id,
				  GValue     *value,
				  GParamSpec *pspec)
{
  GInetAddressMask *mask = G_INET_ADDRESS_MASK (object);

  switch (prop_id)
    {
    case PROP_FAMILY:
      g_value_set_enum (value, g_inet_address_get_family (mask->priv->addr));
      break;

    case PROP_ADDRESS:
      g_value_set_object (value, mask->priv->addr);
      break;

    case PROP_LENGTH:
      g_value_set_uint (value, mask->priv->length);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
g_inet_address_mask_dispose (GObject *object)
{
  GInetAddressMask *mask = G_INET_ADDRESS_MASK (object);

  g_clear_object (&mask->priv->addr);

  G_OBJECT_CLASS (g_inet_address_mask_parent_class)->dispose (object);
}

static void
g_inet_address_mask_class_init (GInetAddressMaskClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = g_inet_address_mask_set_property;
  gobject_class->get_property = g_inet_address_mask_get_property;
  gobject_class->dispose = g_inet_address_mask_dispose;

  g_object_class_install_property (gobject_class, PROP_FAMILY,
                                   g_param_spec_enum ("family",
						      P_("Address family"),
						      P_("The address family (IPv4 or IPv6)"),
						      G_TYPE_SOCKET_FAMILY,
						      G_SOCKET_FAMILY_INVALID,
						      G_PARAM_READABLE |
                                                      G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_ADDRESS,
                                   g_param_spec_object ("address",
							P_("Address"),
							P_("The base address"),
							G_TYPE_INET_ADDRESS,
							G_PARAM_READWRITE |
							G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_LENGTH,
                                   g_param_spec_uint ("length",
						      P_("Length"),
						      P_("The prefix length"),
						      0, 128, 0,
						      G_PARAM_READWRITE |
						      G_PARAM_STATIC_STRINGS));
}

static gboolean
g_inet_address_mask_initable_init (GInitable     *initable,
				   GCancellable  *cancellable,
				   GError       **error)
{
  GInetAddressMask *mask = G_INET_ADDRESS_MASK (initable);
  guint addrlen, nbytes, nbits;
  const guint8 *bytes;
  gboolean ok;

  if (!mask->priv->addr)
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
			   _("No address specified"));
      return FALSE;
    }

  addrlen = g_inet_address_get_native_size (mask->priv->addr);
  if (mask->priv->length > addrlen * 8)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
		   _("Length %u is too long for address"),
		   mask->priv->length);
      return FALSE;
    }

  /* Make sure all the bits after @length are 0 */
  bytes = g_inet_address_to_bytes (mask->priv->addr);
  ok = TRUE;

  nbytes = mask->priv->length / 8;
  bytes += nbytes;
  addrlen -= nbytes;

  nbits = mask->priv->length % 8;
  if (nbits)
    {
      if (bytes[0] & (0xFF >> nbits))
	ok = FALSE;
      bytes++;
      addrlen--;
    }

  while (addrlen)
    {
      if (bytes[0])
	ok = FALSE;
      bytes++;
      addrlen--;
    }

  if (!ok)
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
			   _("Address has bits set beyond prefix length"));
      return FALSE;
    }

  return TRUE;
}

static void
g_inet_address_mask_initable_iface_init (GInitableIface  *iface)
{
  iface->init = g_inet_address_mask_initable_init;
}

static void
g_inet_address_mask_init (GInetAddressMask *mask)
{
  mask->priv = g_inet_address_mask_get_instance_private (mask);
}

/**
 * g_inet_address_mask_new:
 * @addr: a #GInetAddress
 * @length: number of bits of @addr to use
 * @error: return location for #GError, or %NULL
 *
 * Creates a new #GInetAddressMask representing all addresses whose
 * first @length bits match @addr.
 *
 * Returns: a new #GInetAddressMask, or %NULL on error
 *
 * Since: 2.32
 */
GInetAddressMask *
g_inet_address_mask_new (GInetAddress  *addr,
			 guint          length,
			 GError       **error)
{
  return g_initable_new (G_TYPE_INET_ADDRESS_MASK, NULL, error,
			 "address", addr,
			 "length", length,
			 NULL);
}

/**
 * g_inet_address_mask_new_from_string:
 * @mask_string: an IP address or address/length string
 * @error: return location for #GError, or %NULL
 *
 * Parses @mask_string as an IP address and (optional) length, and
 * creates a new #GInetAddressMask. The length, if present, is
 * delimited by a "/". If it is not present, then the length is
 * assumed to be the full length of the address.
 *
 * Returns: a new #GInetAddressMask corresponding to @string, or %NULL
 * on error.
 *
 * Since: 2.32
 */
GInetAddressMask *
g_inet_address_mask_new_from_string (const gchar  *mask_string,
				     GError      **error)
{
  GInetAddressMask *mask;
  GInetAddress *addr;
  gchar *slash;
  guint length;

  slash = strchr (mask_string, '/');
  if (slash)
    {
      gchar *address, *end;

      length = strtoul (slash + 1, &end, 10);
      if (*end || !*(slash + 1))
	{
	parse_error:
	  g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
		       _("Could not parse “%s” as IP address mask"),
		       mask_string);
	  return NULL;
	}

      address = g_strndup (mask_string, slash - mask_string);
      addr = g_inet_address_new_from_string (address);
      g_free (address);

      if (!addr)
	goto parse_error;
    }
  else
    {
      addr = g_inet_address_new_from_string (mask_string);
      if (!addr)
	goto parse_error;

      length = g_inet_address_get_native_size (addr) * 8;
    }

  mask = g_inet_address_mask_new (addr, length, error);
  g_object_unref (addr);

  return mask;
}

/**
 * g_inet_address_mask_to_string:
 * @mask: a #GInetAddressMask
 *
 * Converts @mask back to its corresponding string form.
 *
 * Returns: a string corresponding to @mask.
 *
 * Since: 2.32
 */
gchar *
g_inet_address_mask_to_string (GInetAddressMask *mask)
{
  gchar *addr_string, *mask_string;

  g_return_val_if_fail (G_IS_INET_ADDRESS_MASK (mask), NULL);

  addr_string = g_inet_address_to_string (mask->priv->addr);

  if (mask->priv->length == (g_inet_address_get_native_size (mask->priv->addr) * 8))
    return addr_string;

  mask_string = g_strdup_printf ("%s/%u", addr_string, mask->priv->length);
  g_free (addr_string);

  return mask_string;
}

/**
 * g_inet_address_mask_get_family:
 * @mask: a #GInetAddressMask
 *
 * Gets the #GSocketFamily of @mask's address
 *
 * Returns: the #GSocketFamily of @mask's address
 *
 * Since: 2.32
 */
GSocketFamily
g_inet_address_mask_get_family (GInetAddressMask *mask)
{
  g_return_val_if_fail (G_IS_INET_ADDRESS_MASK (mask), G_SOCKET_FAMILY_INVALID);

  return g_inet_address_get_family (mask->priv->addr);
}

/**
 * g_inet_address_mask_get_address:
 * @mask: a #GInetAddressMask
 *
 * Gets @mask's base address
 *
 * Returns: (transfer none): @mask's base address
 *
 * Since: 2.32
 */
GInetAddress *
g_inet_address_mask_get_address (GInetAddressMask *mask)
{
  g_return_val_if_fail (G_IS_INET_ADDRESS_MASK (mask), NULL);

  return mask->priv->addr;
}

/**
 * g_inet_address_mask_get_length:
 * @mask: a #GInetAddressMask
 *
 * Gets @mask's length
 *
 * Returns: @mask's length
 *
 * Since: 2.32
 */
guint
g_inet_address_mask_get_length (GInetAddressMask *mask)
{
  g_return_val_if_fail (G_IS_INET_ADDRESS_MASK (mask), 0);

  return mask->priv->length;
}

/**
 * g_inet_address_mask_matches:
 * @mask: a #GInetAddressMask
 * @address: a #GInetAddress
 *
 * Tests if @address falls within the range described by @mask.
 *
 * Returns: whether @address falls within the range described by
 * @mask.
 *
 * Since: 2.32
 */
gboolean
g_inet_address_mask_matches (GInetAddressMask *mask,
			     GInetAddress     *address)
{
  const guint8 *maskbytes, *addrbytes;
  int nbytes, nbits;

  g_return_val_if_fail (G_IS_INET_ADDRESS_MASK (mask), FALSE);
  g_return_val_if_fail (G_IS_INET_ADDRESS (address), FALSE);

  if (g_inet_address_get_family (mask->priv->addr) !=
      g_inet_address_get_family (address))
    return FALSE;

  if (mask->priv->length == 0)
    return TRUE;

  maskbytes = g_inet_address_to_bytes (mask->priv->addr);
  addrbytes = g_inet_address_to_bytes (address);

  nbytes = mask->priv->length / 8;
  if (nbytes != 0 && memcmp (maskbytes, addrbytes, nbytes) != 0)
    return FALSE;

  nbits = mask->priv->length % 8;
  if (nbits == 0)
    return TRUE;

  return maskbytes[nbytes] == (addrbytes[nbytes] & (0xFF << (8 - nbits)));
}


/**
 * g_inet_address_mask_equal:
 * @mask: a #GInetAddressMask
 * @mask2: another #GInetAddressMask
 *
 * Tests if @mask and @mask2 are the same mask.
 *
 * Returns: whether @mask and @mask2 are the same mask
 *
 * Since: 2.32
 */
gboolean
g_inet_address_mask_equal (GInetAddressMask  *mask,
			   GInetAddressMask  *mask2)
{
  g_return_val_if_fail (G_IS_INET_ADDRESS_MASK (mask), FALSE);
  g_return_val_if_fail (G_IS_INET_ADDRESS_MASK (mask2), FALSE);

  return ((mask->priv->length == mask2->priv->length) &&
	  g_inet_address_equal (mask->priv->addr, mask2->priv->addr));
}
