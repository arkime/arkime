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
 * SECTION:gpollableutils
 * @short_description: Utilities for pollable streams
 * @include: gio/gio.h
 *
 * Utility functions for #GPollableInputStream and
 * #GPollableOutputStream implementations.
 */

typedef struct {
  GSource       source;

  GObject      *stream;
} GPollableSource;

static gboolean
pollable_source_dispatch (GSource     *source,
			  GSourceFunc  callback,
			  gpointer     user_data)
{
  GPollableSourceFunc func = (GPollableSourceFunc)callback;
  GPollableSource *pollable_source = (GPollableSource *)source;

  return (*func) (pollable_source->stream, user_data);
}

static void
pollable_source_finalize (GSource *source)
{
  GPollableSource *pollable_source = (GPollableSource *)source;

  g_object_unref (pollable_source->stream);
}

static gboolean
pollable_source_closure_callback (GObject  *stream,
				  gpointer  data)
{
  GClosure *closure = data;

  GValue param = G_VALUE_INIT;
  GValue result_value = G_VALUE_INIT;
  gboolean result;

  g_value_init (&result_value, G_TYPE_BOOLEAN);

  g_value_init (&param, G_TYPE_OBJECT);
  g_value_set_object (&param, stream);

  g_closure_invoke (closure, &result_value, 1, &param, NULL);

  result = g_value_get_boolean (&result_value);
  g_value_unset (&result_value);
  g_value_unset (&param);

  return result;
}

static GSourceFuncs pollable_source_funcs =
{
  NULL,
  NULL,
  pollable_source_dispatch,
  pollable_source_finalize,
  (GSourceFunc)pollable_source_closure_callback,
};

/**
 * g_pollable_source_new:
 * @pollable_stream: the stream associated with the new source
 *
 * Utility method for #GPollableInputStream and #GPollableOutputStream
 * implementations. Creates a new #GSource that expects a callback of
 * type #GPollableSourceFunc. The new source does not actually do
 * anything on its own; use g_source_add_child_source() to add other
 * sources to it to cause it to trigger.
 *
 * Returns: (transfer full): the new #GSource.
 *
 * Since: 2.28
 */
GSource *
g_pollable_source_new (GObject *pollable_stream)
{
  GSource *source;
  GPollableSource *pollable_source;

  g_return_val_if_fail (G_IS_POLLABLE_INPUT_STREAM (pollable_stream) ||
			G_IS_POLLABLE_OUTPUT_STREAM (pollable_stream), NULL);

  source = g_source_new (&pollable_source_funcs, sizeof (GPollableSource));
  g_source_set_name (source, "GPollableSource");
  pollable_source = (GPollableSource *)source;
  pollable_source->stream = g_object_ref (pollable_stream);

  return source;
}

/**
 * g_pollable_source_new_full:
 * @pollable_stream: (type GObject): the stream associated with the
 *   new source
 * @child_source: (nullable): optional child source to attach
 * @cancellable: (nullable): optional #GCancellable to attach
 *
 * Utility method for #GPollableInputStream and #GPollableOutputStream
 * implementations. Creates a new #GSource, as with
 * g_pollable_source_new(), but also attaching @child_source (with a
 * dummy callback), and @cancellable, if they are non-%NULL.
 *
 * Returns: (transfer full): the new #GSource.
 *
 * Since: 2.34
 */
GSource *
g_pollable_source_new_full (gpointer      pollable_stream,
			    GSource      *child_source,
			    GCancellable *cancellable)
{
  GSource *source;

  g_return_val_if_fail (G_IS_POLLABLE_INPUT_STREAM (pollable_stream) ||
			G_IS_POLLABLE_OUTPUT_STREAM (pollable_stream), NULL);

  source = g_pollable_source_new (pollable_stream);
  if (child_source)
    {
      g_source_set_dummy_callback (child_source);
      g_source_add_child_source (source, child_source);
    }
  if (cancellable)
    {
      GSource *cancellable_source = g_cancellable_source_new (cancellable);

      g_source_set_dummy_callback (cancellable_source);
      g_source_add_child_source (source, cancellable_source);
      g_source_unref (cancellable_source);
    }

  return source;
}

/**
 * g_pollable_stream_read:
 * @stream: a #GInputStream
 * @buffer: (array length=count) (element-type guint8): a buffer to
 *   read data into
 * @count: the number of bytes to read
 * @blocking: whether to do blocking I/O
 * @cancellable: (nullable): optional #GCancellable object, %NULL to ignore.
 * @error: location to store the error occurring, or %NULL to ignore
 *
 * Tries to read from @stream, as with g_input_stream_read() (if
 * @blocking is %TRUE) or g_pollable_input_stream_read_nonblocking()
 * (if @blocking is %FALSE). This can be used to more easily share
 * code between blocking and non-blocking implementations of a method.
 *
 * If @blocking is %FALSE, then @stream must be a
 * #GPollableInputStream for which g_pollable_input_stream_can_poll()
 * returns %TRUE, or else the behavior is undefined. If @blocking is
 * %TRUE, then @stream does not need to be a #GPollableInputStream.
 *
 * Returns: the number of bytes read, or -1 on error.
 *
 * Since: 2.34
 */
gssize
g_pollable_stream_read (GInputStream   *stream,
			void           *buffer,
			gsize           count,
			gboolean        blocking,
			GCancellable   *cancellable,
			GError        **error)
{
  if (blocking)
    {
      return g_input_stream_read (stream,
				  buffer, count,
				  cancellable, error);
    }
  else
    {
      return g_pollable_input_stream_read_nonblocking (G_POLLABLE_INPUT_STREAM (stream),
						       buffer, count,
						       cancellable, error);
    }
}

/**
 * g_pollable_stream_write:
 * @stream: a #GOutputStream.
 * @buffer: (array length=count) (element-type guint8): the buffer
 *   containing the data to write.
 * @count: the number of bytes to write
 * @blocking: whether to do blocking I/O
 * @cancellable: (nullable): optional #GCancellable object, %NULL to ignore.
 * @error: location to store the error occurring, or %NULL to ignore
 *
 * Tries to write to @stream, as with g_output_stream_write() (if
 * @blocking is %TRUE) or g_pollable_output_stream_write_nonblocking()
 * (if @blocking is %FALSE). This can be used to more easily share
 * code between blocking and non-blocking implementations of a method.
 *
 * If @blocking is %FALSE, then @stream must be a
 * #GPollableOutputStream for which
 * g_pollable_output_stream_can_poll() returns %TRUE or else the
 * behavior is undefined. If @blocking is %TRUE, then @stream does not
 * need to be a #GPollableOutputStream.
 *
 * Returns: the number of bytes written, or -1 on error.
 *
 * Since: 2.34
 */
gssize
g_pollable_stream_write (GOutputStream   *stream,
			 const void      *buffer,
			 gsize            count,
			 gboolean         blocking,
			 GCancellable    *cancellable,
			 GError         **error)
{
  if (blocking)
    {
      return g_output_stream_write (stream,
				    buffer, count,
				    cancellable, error);
    }
  else
    {
      return g_pollable_output_stream_write_nonblocking (G_POLLABLE_OUTPUT_STREAM (stream),
							 buffer, count,
							 cancellable, error);
    }
}

/**
 * g_pollable_stream_write_all:
 * @stream: a #GOutputStream.
 * @buffer: (array length=count) (element-type guint8): the buffer
 *   containing the data to write.
 * @count: the number of bytes to write
 * @blocking: whether to do blocking I/O
 * @bytes_written: (out): location to store the number of bytes that was 
 *   written to the stream
 * @cancellable: (nullable): optional #GCancellable object, %NULL to ignore.
 * @error: location to store the error occurring, or %NULL to ignore
 *
 * Tries to write @count bytes to @stream, as with
 * g_output_stream_write_all(), but using g_pollable_stream_write()
 * rather than g_output_stream_write().
 *
 * On a successful write of @count bytes, %TRUE is returned, and
 * @bytes_written is set to @count.
 * 
 * If there is an error during the operation (including
 * %G_IO_ERROR_WOULD_BLOCK in the non-blocking case), %FALSE is
 * returned and @error is set to indicate the error status,
 * @bytes_written is updated to contain the number of bytes written
 * into the stream before the error occurred.
 *
 * As with g_pollable_stream_write(), if @blocking is %FALSE, then
 * @stream must be a #GPollableOutputStream for which
 * g_pollable_output_stream_can_poll() returns %TRUE or else the
 * behavior is undefined. If @blocking is %TRUE, then @stream does not
 * need to be a #GPollableOutputStream.
 *
 * Returns: %TRUE on success, %FALSE if there was an error
 *
 * Since: 2.34
 */
gboolean
g_pollable_stream_write_all (GOutputStream  *stream,
			     const void     *buffer,
			     gsize           count,
			     gboolean        blocking,
			     gsize          *bytes_written,
			     GCancellable   *cancellable,
			     GError        **error)
{
  gsize _bytes_written;
  gssize res;

  _bytes_written = 0;
  while (_bytes_written < count)
    {
      res = g_pollable_stream_write (stream,
				     (char *)buffer + _bytes_written,
				     count - _bytes_written,
				     blocking,
				     cancellable, error);
      if (res == -1)
	{
	  if (bytes_written)
	    *bytes_written = _bytes_written;
	  return FALSE;
	}

      if (res == 0)
	g_warning ("Write returned zero without error");

      _bytes_written += res;
    }

  if (bytes_written)
    *bytes_written = _bytes_written;

  return TRUE;
}
