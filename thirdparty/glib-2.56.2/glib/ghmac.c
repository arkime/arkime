/* ghmac.h - data hashing functions
 *
 * Copyright (C) 2011  Collabora Ltd.
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
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Stef Walter <stefw@collabora.co.uk>
 */

#include "config.h"

#include <string.h>

#include "ghmac.h"

#include "glib/galloca.h"
#include "gatomic.h"
#include "gslice.h"
#include "gmem.h"
#include "gstrfuncs.h"
#include "gtestutils.h"
#include "gtypes.h"
#include "glibintl.h"


/**
 * SECTION:hmac
 * @title: Secure HMAC Digests
 * @short_description: computes the HMAC for data
 *
 * HMACs should be used when producing a cookie or hash based on data
 * and a key. Simple mechanisms for using SHA1 and other algorithms to
 * digest a key and data together are vulnerable to various security
 * issues.
 * [HMAC](http://en.wikipedia.org/wiki/HMAC)
 * uses algorithms like SHA1 in a secure way to produce a digest of a
 * key and data.
 *
 * Both the key and data are arbitrary byte arrays of bytes or characters.
 *
 * Support for HMAC Digests has been added in GLib 2.30, and support for SHA-512
 * in GLib 2.42. Support for SHA-384 was added in GLib 2.52.
 */

struct _GHmac
{
  int ref_count;
  GChecksumType digest_type;
  GChecksum *digesti;
  GChecksum *digesto;
};

/**
 * g_hmac_new:
 * @digest_type: the desired type of digest
 * @key: (array length=key_len): the key for the HMAC
 * @key_len: the length of the keys
 *
 * Creates a new #GHmac, using the digest algorithm @digest_type.
 * If the @digest_type is not known, %NULL is returned.
 * A #GHmac can be used to compute the HMAC of a key and an
 * arbitrary binary blob, using different hashing algorithms.
 *
 * A #GHmac works by feeding a binary blob through g_hmac_update()
 * until the data is complete; the digest can then be extracted
 * using g_hmac_get_string(), which will return the checksum as a
 * hexadecimal string; or g_hmac_get_digest(), which will return a
 * array of raw bytes. Once either g_hmac_get_string() or
 * g_hmac_get_digest() have been called on a #GHmac, the HMAC
 * will be closed and it won't be possible to call g_hmac_update()
 * on it anymore.
 *
 * Support for digests of type %G_CHECKSUM_SHA512 has been added in GLib 2.42.
 * Support for %G_CHECKSUM_SHA384 was added in GLib 2.52.
 *
 * Returns: the newly created #GHmac, or %NULL.
 *   Use g_hmac_unref() to free the memory allocated by it.
 *
 * Since: 2.30
 */
GHmac *
g_hmac_new (GChecksumType  digest_type,
            const guchar  *key,
            gsize          key_len)
{
  GChecksum *checksum;
  GHmac *hmac;
  guchar *buffer;
  guchar *pad;
  gsize i, len;
  gsize block_size;

  checksum = g_checksum_new (digest_type);
  g_return_val_if_fail (checksum != NULL, NULL);

  switch (digest_type)
    {
    case G_CHECKSUM_MD5:
    case G_CHECKSUM_SHA1:
      block_size = 64; /* RFC 2104 */
      break;
    case G_CHECKSUM_SHA256:
      block_size = 64; /* RFC 4868 */
      break;
    case G_CHECKSUM_SHA384:
    case G_CHECKSUM_SHA512:
      block_size = 128; /* RFC 4868 */
      break;
    default:
      g_return_val_if_reached (NULL);
    }

  hmac = g_slice_new0 (GHmac);
  hmac->ref_count = 1;
  hmac->digest_type = digest_type;
  hmac->digesti = checksum;
  hmac->digesto = g_checksum_new (digest_type);

  buffer = g_alloca (block_size);
  pad = g_alloca (block_size);

  memset (buffer, 0, block_size);

  /* If the key is too long, hash it */
  if (key_len > block_size)
    {
      len = block_size;
      g_checksum_update (hmac->digesti, key, key_len);
      g_checksum_get_digest (hmac->digesti, buffer, &len);
      g_checksum_reset (hmac->digesti);
    }

  /* Otherwise pad it with zeros */
  else
    {
      memcpy (buffer, key, key_len);
    }

  /* First pad */
  for (i = 0; i < block_size; i++)
    pad[i] = 0x36 ^ buffer[i]; /* ipad value */
  g_checksum_update (hmac->digesti, pad, block_size);

  /* Second pad */
  for (i = 0; i < block_size; i++)
    pad[i] = 0x5c ^ buffer[i]; /* opad value */
  g_checksum_update (hmac->digesto, pad, block_size);

  return hmac;
}

/**
 * g_hmac_copy:
 * @hmac: the #GHmac to copy
 *
 * Copies a #GHmac. If @hmac has been closed, by calling
 * g_hmac_get_string() or g_hmac_get_digest(), the copied
 * HMAC will be closed as well.
 *
 * Returns: the copy of the passed #GHmac. Use g_hmac_unref()
 *   when finished using it.
 *
 * Since: 2.30
 */
GHmac *
g_hmac_copy (const GHmac *hmac)
{
  GHmac *copy;

  g_return_val_if_fail (hmac != NULL, NULL);

  copy = g_slice_new (GHmac);
  copy->ref_count = 1;
  copy->digest_type = hmac->digest_type;
  copy->digesti = g_checksum_copy (hmac->digesti);
  copy->digesto = g_checksum_copy (hmac->digesto);

  return copy;
}

/**
 * g_hmac_ref:
 * @hmac: a valid #GHmac
 *
 * Atomically increments the reference count of @hmac by one.
 *
 * This function is MT-safe and may be called from any thread.
 *
 * Returns: the passed in #GHmac.
 *
 * Since: 2.30
 **/
GHmac *
g_hmac_ref (GHmac *hmac)
{
  g_return_val_if_fail (hmac != NULL, NULL);

  g_atomic_int_inc (&hmac->ref_count);

  return hmac;
}

/**
 * g_hmac_unref:
 * @hmac: a #GHmac
 *
 * Atomically decrements the reference count of @hmac by one.
 *
 * If the reference count drops to 0, all keys and values will be
 * destroyed, and all memory allocated by the hash table is released.
 * This function is MT-safe and may be called from any thread.
 * Frees the memory allocated for @hmac.
 *
 * Since: 2.30
 */
void
g_hmac_unref (GHmac *hmac)
{
  g_return_if_fail (hmac != NULL);

  if (g_atomic_int_dec_and_test (&hmac->ref_count))
    {
      g_checksum_free (hmac->digesti);
      g_checksum_free (hmac->digesto);
      g_slice_free (GHmac, hmac);
    }
}

/**
 * g_hmac_update:
 * @hmac: a #GHmac
 * @data: (array length=length): buffer used to compute the checksum
 * @length: size of the buffer, or -1 if it is a nul-terminated string
 *
 * Feeds @data into an existing #GHmac.
 *
 * The HMAC must still be open, that is g_hmac_get_string() or
 * g_hmac_get_digest() must not have been called on @hmac.
 *
 * Since: 2.30
 */
void
g_hmac_update (GHmac        *hmac,
               const guchar *data,
               gssize        length)
{
  g_return_if_fail (hmac != NULL);
  g_return_if_fail (length == 0 || data != NULL);

  g_checksum_update (hmac->digesti, data, length);
}

/**
 * g_hmac_get_string:
 * @hmac: a #GHmac
 *
 * Gets the HMAC as an hexadecimal string.
 *
 * Once this function has been called the #GHmac can no longer be
 * updated with g_hmac_update().
 *
 * The hexadecimal characters will be lower case.
 *
 * Returns: the hexadecimal representation of the HMAC. The
 *   returned string is owned by the HMAC and should not be modified
 *   or freed.
 *
 * Since: 2.30
 */
const gchar *
g_hmac_get_string (GHmac *hmac)
{
  guint8 *buffer;
  gsize digest_len;

  g_return_val_if_fail (hmac != NULL, NULL);

  digest_len = g_checksum_type_get_length (hmac->digest_type);
  buffer = g_alloca (digest_len);

  /* This is only called for its side-effect of updating hmac->digesto... */
  g_hmac_get_digest (hmac, buffer, &digest_len);
  /* ... because we get the string from the checksum rather than
   * stringifying buffer ourselves
   */
  return g_checksum_get_string (hmac->digesto);
}

/**
 * g_hmac_get_digest:
 * @hmac: a #GHmac
 * @buffer: output buffer
 * @digest_len: an inout parameter. The caller initializes it to the
 *   size of @buffer. After the call it contains the length of the digest
 *
 * Gets the digest from @checksum as a raw binary array and places it
 * into @buffer. The size of the digest depends on the type of checksum.
 *
 * Once this function has been called, the #GHmac is closed and can
 * no longer be updated with g_checksum_update().
 *
 * Since: 2.30
 */
void
g_hmac_get_digest (GHmac  *hmac,
                   guint8 *buffer,
                   gsize  *digest_len)
{
  gsize len;

  g_return_if_fail (hmac != NULL);

  len = g_checksum_type_get_length (hmac->digest_type);
  g_return_if_fail (*digest_len >= len);

  /* Use the same buffer, because we can :) */
  g_checksum_get_digest (hmac->digesti, buffer, &len);
  g_checksum_update (hmac->digesto, buffer, len);
  g_checksum_get_digest (hmac->digesto, buffer, digest_len);
}

/**
 * g_compute_hmac_for_data:
 * @digest_type: a #GChecksumType to use for the HMAC
 * @key: (array length=key_len): the key to use in the HMAC
 * @key_len: the length of the key
 * @data: (array length=length): binary blob to compute the HMAC of
 * @length: length of @data
 *
 * Computes the HMAC for a binary @data of @length. This is a
 * convenience wrapper for g_hmac_new(), g_hmac_get_string()
 * and g_hmac_unref().
 *
 * The hexadecimal string returned will be in lower case.
 *
 * Returns: the HMAC of the binary data as a string in hexadecimal.
 *   The returned string should be freed with g_free() when done using it.
 *
 * Since: 2.30
 */
gchar *
g_compute_hmac_for_data (GChecksumType  digest_type,
                         const guchar  *key,
                         gsize          key_len,
                         const guchar  *data,
                         gsize          length)
{
  GHmac *hmac;
  gchar *retval;

  g_return_val_if_fail (length == 0 || data != NULL, NULL);

  hmac = g_hmac_new (digest_type, key, key_len);
  if (!hmac)
    return NULL;

  g_hmac_update (hmac, data, length);
  retval = g_strdup (g_hmac_get_string (hmac));
  g_hmac_unref (hmac);

  return retval;
}

/**
 * g_compute_hmac_for_bytes:
 * @digest_type: a #GChecksumType to use for the HMAC
 * @key: the key to use in the HMAC
 * @data: binary blob to compute the HMAC of
 *
 * Computes the HMAC for a binary @data. This is a
 * convenience wrapper for g_hmac_new(), g_hmac_get_string()
 * and g_hmac_unref().
 *
 * The hexadecimal string returned will be in lower case.
 *
 * Returns: the HMAC of the binary data as a string in hexadecimal.
 *   The returned string should be freed with g_free() when done using it.
 *
 * Since: 2.50
 */
gchar *
g_compute_hmac_for_bytes (GChecksumType  digest_type,
                          GBytes        *key,
                          GBytes        *data)
{
  gconstpointer byte_data;
  gsize length;
  gconstpointer key_data;
  gsize key_len;

  g_return_val_if_fail (data != NULL, NULL);
  g_return_val_if_fail (key != NULL, NULL);

  byte_data = g_bytes_get_data (data, &length);
  key_data = g_bytes_get_data (key, &key_len);
  return g_compute_hmac_for_data (digest_type, key_data, key_len, byte_data, length);
}


/**
 * g_compute_hmac_for_string:
 * @digest_type: a #GChecksumType to use for the HMAC
 * @key: (array length=key_len): the key to use in the HMAC
 * @key_len: the length of the key
 * @str: the string to compute the HMAC for
 * @length: the length of the string, or -1 if the string is nul-terminated
 *
 * Computes the HMAC for a string.
 *
 * The hexadecimal string returned will be in lower case.
 *
 * Returns: the HMAC as a hexadecimal string.
 *     The returned string should be freed with g_free()
 *     when done using it.
 *
 * Since: 2.30
 */
gchar *
g_compute_hmac_for_string (GChecksumType  digest_type,
                           const guchar  *key,
                           gsize          key_len,
                           const gchar   *str,
                           gssize         length)
{
  g_return_val_if_fail (length == 0 || str != NULL, NULL);

  if (length < 0)
    length = strlen (str);

  return g_compute_hmac_for_data (digest_type, key, key_len,
                                  (const guchar *) str, length);
}
