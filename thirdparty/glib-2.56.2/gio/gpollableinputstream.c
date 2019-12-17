/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright (C) 2010 Red Hat, Inc.
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

#include <errno.h>

#include "gpollableinputstream.h"
#include "gasynchelper.h"
#include "glibintl.h"

/**
 * SECTION:gpollableinputstream
 * @short_description: Interface for pollable input streams
 * @include: gio/gio.h
 * @see_also: #GInputStream, #GPollableOutputStream, #GFileDescriptorBased
 *
 * #GPollableInputStream is implemented by #GInputStreams that
 * can be polled for readiness to read. This can be used when
 * interfacing with a non-GIO API that expects
 * UNIX-file-descriptor-style asynchronous I/O rather than GIO-style.
 *
 * Since: 2.28
 */

G_DEFINE_INTERFACE (GPollableInputStream, g_pollable_input_stream, G_TYPE_INPUT_STREAM)

static gboolean g_pollable_input_stream_default_can_poll         (GPollableInputStream *stream);
static gssize   g_pollable_input_stream_default_read_nonblocking (GPollableInputStream  *stream,
								  void                  *buffer,
								  gsize                  count,
								  GError               **error);

static void
g_pollable_input_stream_default_init (GPollableInputStreamInterface *iface)
{
  iface->can_poll         = g_pollable_input_stream_default_can_poll;
  iface->read_nonblocking = g_pollable_input_stream_default_read_nonblocking;
}

static gboolean
g_pollable_input_stream_default_can_poll (GPollableInputStream *stream)
{
  return TRUE;
}

/**
 * g_pollable_input_stream_can_poll:
 * @stream: a #GPollableInputStream.
 *
 * Checks if @stream is actually pollable. Some classes may implement
 * #GPollableInputStream but have only certain instances of that class
 * be pollable. If this method returns %FALSE, then the behavior of
 * other #GPollableInputStream methods is undefined.
 *
 * For any given stream, the value returned by this method is constant;
 * a stream cannot switch from pollable to non-pollable or vice versa.
 *
 * Returns: %TRUE if @stream is pollable, %FALSE if not.
 *
 * Since: 2.28
 */
gboolean
g_pollable_input_stream_can_poll (GPollableInputStream *stream)
{
  g_return_val_if_fail (G_IS_POLLABLE_INPUT_STREAM (stream), FALSE);

  return G_POLLABLE_INPUT_STREAM_GET_INTERFACE (stream)->can_poll (stream);
}

/**
 * g_pollable_input_stream_is_readable:
 * @stream: a #GPollableInputStream.
 *
 * Checks if @stream can be read.
 *
 * Note that some stream types may not be able to implement this 100%
 * reliably, and it is possible that a call to g_input_stream_read()
 * after this returns %TRUE would still block. To guarantee
 * non-blocking behavior, you should always use
 * g_pollable_input_stream_read_nonblocking(), which will return a
 * %G_IO_ERROR_WOULD_BLOCK error rather than blocking.
 *
 * Returns: %TRUE if @stream is readable, %FALSE if not. If an error
 *   has occurred on @stream, this will result in
 *   g_pollable_input_stream_is_readable() returning %TRUE, and the
 *   next attempt to read will return the error.
 *
 * Since: 2.28
 */
gboolean
g_pollable_input_stream_is_readable (GPollableInputStream *stream)
{
  g_return_val_if_fail (G_IS_POLLABLE_INPUT_STREAM (stream), FALSE);

  return G_POLLABLE_INPUT_STREAM_GET_INTERFACE (stream)->is_readable (stream);
}

/**
 * g_pollable_input_stream_create_source:
 * @stream: a #GPollableInputStream.
 * @cancellable: (nullable): a #GCancellable, or %NULL
 *
 * Creates a #GSource that triggers when @stream can be read, or
 * @cancellable is triggered or an error occurs. The callback on the
 * source is of the #GPollableSourceFunc type.
 *
 * As with g_pollable_input_stream_is_readable(), it is possible that
 * the stream may not actually be readable even after the source
 * triggers, so you should use g_pollable_input_stream_read_nonblocking()
 * rather than g_input_stream_read() from the callback.
 *
 * Returns: (transfer full): a new #GSource
 *
 * Since: 2.28
 */
GSource *
g_pollable_input_stream_create_source (GPollableInputStream *stream,
				       GCancellable         *cancellable)
{
  g_return_val_if_fail (G_IS_POLLABLE_INPUT_STREAM (stream), NULL);

  return G_POLLABLE_INPUT_STREAM_GET_INTERFACE (stream)->
	  create_source (stream, cancellable);
}

static gssize
g_pollable_input_stream_default_read_nonblocking (GPollableInputStream  *stream,
						  void                  *buffer,
						  gsize                  count,
						  GError               **error)
{
  if (!g_pollable_input_stream_is_readable (stream))
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_WOULD_BLOCK,
                           g_strerror (EAGAIN));
      return -1;
    }

  return G_INPUT_STREAM_GET_CLASS (stream)->
    read_fn (G_INPUT_STREAM (stream), buffer, count, NULL, error);
}

/**
 * g_pollable_input_stream_read_nonblocking:
 * @stream: a #GPollableInputStream
 * @buffer: (array length=count) (element-type guint8): a buffer to
 *     read data into (which should be at least @count bytes long).
 * @count: the number of bytes you want to read
 * @cancellable: (nullable): a #GCancellable, or %NULL
 * @error: #GError for error reporting, or %NULL to ignore.
 *
 * Attempts to read up to @count bytes from @stream into @buffer, as
 * with g_input_stream_read(). If @stream is not currently readable,
 * this will immediately return %G_IO_ERROR_WOULD_BLOCK, and you can
 * use g_pollable_input_stream_create_source() to create a #GSource
 * that will be triggered when @stream is readable.
 *
 * Note that since this method never blocks, you cannot actually
 * use @cancellable to cancel it. However, it will return an error
 * if @cancellable has already been cancelled when you call, which
 * may happen if you call this method after a source triggers due
 * to having been cancelled.
 *
 * Virtual: read_nonblocking
 * Returns: the number of bytes read, or -1 on error (including
 *   %G_IO_ERROR_WOULD_BLOCK).
 */
gssize
g_pollable_input_stream_read_nonblocking (GPollableInputStream  *stream,
					  void                  *buffer,
					  gsize                  count,
					  GCancellable          *cancellable,
					  GError               **error)
{
  gssize res;

  g_return_val_if_fail (G_IS_POLLABLE_INPUT_STREAM (stream), -1);
  g_return_val_if_fail (buffer != NULL, 0);

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return -1;

  if (count == 0)
    return 0;

  if (((gssize) count) < 0)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
		   _("Too large count value passed to %s"), G_STRFUNC);
      return -1;
    }

  if (cancellable)
    g_cancellable_push_current (cancellable);

  res = G_POLLABLE_INPUT_STREAM_GET_INTERFACE (stream)->
    read_nonblocking (stream, buffer, count, error);

  if (cancellable)
    g_cancellable_pop_current (cancellable);

  return res;
}
