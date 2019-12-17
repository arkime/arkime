/* GIO - GLib Input, Output and Streaming Library
 * 
 * Copyright (C) 2006-2007 Red Hat, Inc.
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
 * Author: Alexander Larsson <alexl@redhat.com>
 */

#include "config.h"
#include "gseekable.h"
#include "glibintl.h"


/**
 * SECTION:gseekable
 * @short_description: Stream seeking interface
 * @include: gio/gio.h
 * @see_also: #GInputStream, #GOutputStream
 *
 * #GSeekable is implemented by streams (implementations of
 * #GInputStream or #GOutputStream) that support seeking.
 *
 * Seekable streams largely fall into two categories: resizable and
 * fixed-size.
 *
 * #GSeekable on fixed-sized streams is approximately the same as POSIX
 * lseek() on a block device (for example: attmepting to seek past the
 * end of the device is an error).  Fixed streams typically cannot be
 * truncated.
 *
 * #GSeekable on resizable streams is approximately the same as POSIX
 * lseek() on a normal file.  Seeking past the end and writing data will
 * usually cause the stream to resize by introducing zero bytes.
 **/

typedef GSeekableIface GSeekableInterface;
G_DEFINE_INTERFACE (GSeekable, g_seekable, G_TYPE_OBJECT)

static void
g_seekable_default_init (GSeekableInterface *iface)
{
}

/**
 * g_seekable_tell:
 * @seekable: a #GSeekable.
 * 
 * Tells the current position within the stream.
 * 
 * Returns: the offset from the beginning of the buffer.
 **/
goffset
g_seekable_tell (GSeekable *seekable)
{
  GSeekableIface *iface;

  g_return_val_if_fail (G_IS_SEEKABLE (seekable), 0);

  iface = G_SEEKABLE_GET_IFACE (seekable);

  return (* iface->tell) (seekable);
}

/**
 * g_seekable_can_seek:
 * @seekable: a #GSeekable.
 * 
 * Tests if the stream supports the #GSeekableIface.
 * 
 * Returns: %TRUE if @seekable can be seeked. %FALSE otherwise.
 **/
gboolean
g_seekable_can_seek (GSeekable *seekable)
{
  GSeekableIface *iface;
  
  g_return_val_if_fail (G_IS_SEEKABLE (seekable), FALSE);

  iface = G_SEEKABLE_GET_IFACE (seekable);

  return (* iface->can_seek) (seekable);
}

/**
 * g_seekable_seek:
 * @seekable: a #GSeekable.
 * @offset: a #goffset.
 * @type: a #GSeekType.
 * @cancellable: (nullable): optional #GCancellable object, %NULL to ignore.
 * @error: a #GError location to store the error occurring, or %NULL to
 * ignore.
 *
 * Seeks in the stream by the given @offset, modified by @type.
 *
 * Attempting to seek past the end of the stream will have different
 * results depending on if the stream is fixed-sized or resizable.  If
 * the stream is resizable then seeking past the end and then writing
 * will result in zeros filling the empty space.  Seeking past the end
 * of a resizable stream and reading will result in EOF.  Seeking past
 * the end of a fixed-sized stream will fail.
 *
 * Any operation that would result in a negative offset will fail.
 *
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. 
 * 
 * Returns: %TRUE if successful. If an error
 *     has occurred, this function will return %FALSE and set @error
 *     appropriately if present.
 **/
gboolean
g_seekable_seek (GSeekable     *seekable,
		 goffset        offset,
		 GSeekType      type,
		 GCancellable  *cancellable,
		 GError       **error)
{
  GSeekableIface *iface;
  
  g_return_val_if_fail (G_IS_SEEKABLE (seekable), FALSE);

  iface = G_SEEKABLE_GET_IFACE (seekable);

  return (* iface->seek) (seekable, offset, type, cancellable, error);
}

/**
 * g_seekable_can_truncate:
 * @seekable: a #GSeekable.
 * 
 * Tests if the length of the stream can be adjusted with
 * g_seekable_truncate().
 * 
 * Returns: %TRUE if the stream can be truncated, %FALSE otherwise.
 **/
gboolean
g_seekable_can_truncate (GSeekable *seekable)
{
  GSeekableIface *iface;
  
  g_return_val_if_fail (G_IS_SEEKABLE (seekable), FALSE);

  iface = G_SEEKABLE_GET_IFACE (seekable);

  return (* iface->can_truncate) (seekable);
}

/**
 * g_seekable_truncate: (virtual truncate_fn)
 * @seekable: a #GSeekable.
 * @offset: new length for @seekable, in bytes.
 * @cancellable: (nullable): optional #GCancellable object, %NULL to ignore. 
 * @error: a #GError location to store the error occurring, or %NULL to 
 * ignore.
 * 
 * Sets the length of the stream to @offset. If the stream was previously
 * larger than @offset, the extra data is discarded. If the stream was
 * previouly shorter than @offset, it is extended with NUL ('\0') bytes.
 * 
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. If an
 * operation was partially finished when the operation was cancelled the
 * partial result will be returned, without an error.
 *
 * Returns: %TRUE if successful. If an error
 *     has occurred, this function will return %FALSE and set @error
 *     appropriately if present. 
 **/
gboolean
g_seekable_truncate (GSeekable     *seekable,
		     goffset        offset,
		     GCancellable  *cancellable,
		     GError       **error)
{
  GSeekableIface *iface;
  
  g_return_val_if_fail (G_IS_SEEKABLE (seekable), FALSE);

  iface = G_SEEKABLE_GET_IFACE (seekable);

  return (* iface->truncate_fn) (seekable, offset, cancellable, error);
}
