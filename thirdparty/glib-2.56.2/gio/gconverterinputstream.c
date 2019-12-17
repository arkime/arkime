/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright (C) 2009 Red Hat, Inc.
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

#include <string.h>

#include "gconverterinputstream.h"
#include "gpollableinputstream.h"
#include "gcancellable.h"
#include "gioenumtypes.h"
#include "gioerror.h"
#include "glibintl.h"


/**
 * SECTION:gconverterinputstream
 * @short_description: Converter Input Stream
 * @include: gio/gio.h
 * @see_also: #GInputStream, #GConverter
 *
 * Converter input stream implements #GInputStream and allows
 * conversion of data of various types during reading.
 *
 * As of GLib 2.34, #GConverterInputStream implements
 * #GPollableInputStream.
 **/

#define INITIAL_BUFFER_SIZE 4096

typedef struct {
  char *data;
  gsize start;
  gsize end;
  gsize size;
} Buffer;

struct _GConverterInputStreamPrivate {
  gboolean at_input_end;
  gboolean finished;
  gboolean need_input;
  GConverter *converter;
  Buffer input_buffer;
  Buffer converted_buffer;
};

enum {
  PROP_0,
  PROP_CONVERTER
};

static void   g_converter_input_stream_set_property (GObject       *object,
						     guint          prop_id,
						     const GValue  *value,
						     GParamSpec    *pspec);
static void   g_converter_input_stream_get_property (GObject       *object,
						     guint          prop_id,
						     GValue        *value,
						     GParamSpec    *pspec);
static void   g_converter_input_stream_finalize     (GObject       *object);
static gssize g_converter_input_stream_read         (GInputStream  *stream,
						     void          *buffer,
						     gsize          count,
						     GCancellable  *cancellable,
						     GError       **error);

static gboolean g_converter_input_stream_can_poll         (GPollableInputStream *stream);
static gboolean g_converter_input_stream_is_readable      (GPollableInputStream *stream);
static gssize   g_converter_input_stream_read_nonblocking (GPollableInputStream  *stream,
							   void                  *buffer,
							   gsize                  size,
							   GError               **error);

static GSource *g_converter_input_stream_create_source    (GPollableInputStream *stream,
							   GCancellable          *cancellable);

static void g_converter_input_stream_pollable_iface_init  (GPollableInputStreamInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GConverterInputStream,
			 g_converter_input_stream,
			 G_TYPE_FILTER_INPUT_STREAM,
                         G_ADD_PRIVATE (GConverterInputStream)
			 G_IMPLEMENT_INTERFACE (G_TYPE_POLLABLE_INPUT_STREAM,
						g_converter_input_stream_pollable_iface_init))

static void
g_converter_input_stream_class_init (GConverterInputStreamClass *klass)
{
  GObjectClass *object_class;
  GInputStreamClass *istream_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->get_property = g_converter_input_stream_get_property;
  object_class->set_property = g_converter_input_stream_set_property;
  object_class->finalize     = g_converter_input_stream_finalize;

  istream_class = G_INPUT_STREAM_CLASS (klass);
  istream_class->read_fn = g_converter_input_stream_read;

  g_object_class_install_property (object_class,
				   PROP_CONVERTER,
				   g_param_spec_object ("converter",
							P_("Converter"),
							P_("The converter object"),
							G_TYPE_CONVERTER,
							G_PARAM_READWRITE|
							G_PARAM_CONSTRUCT_ONLY|
							G_PARAM_STATIC_STRINGS));

}

static void
g_converter_input_stream_pollable_iface_init (GPollableInputStreamInterface *iface)
{
  iface->can_poll = g_converter_input_stream_can_poll;
  iface->is_readable = g_converter_input_stream_is_readable;
  iface->read_nonblocking = g_converter_input_stream_read_nonblocking;
  iface->create_source = g_converter_input_stream_create_source;
}

static void
g_converter_input_stream_finalize (GObject *object)
{
  GConverterInputStreamPrivate *priv;
  GConverterInputStream        *stream;

  stream = G_CONVERTER_INPUT_STREAM (object);
  priv = stream->priv;

  g_free (priv->input_buffer.data);
  g_free (priv->converted_buffer.data);
  if (priv->converter)
    g_object_unref (priv->converter);

  G_OBJECT_CLASS (g_converter_input_stream_parent_class)->finalize (object);
}

static void
g_converter_input_stream_set_property (GObject      *object,
				       guint         prop_id,
				       const GValue *value,
				       GParamSpec   *pspec)
{
  GConverterInputStream *cstream;

  cstream = G_CONVERTER_INPUT_STREAM (object);

   switch (prop_id)
    {
    case PROP_CONVERTER:
      cstream->priv->converter = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }

}

static void
g_converter_input_stream_get_property (GObject    *object,
				       guint       prop_id,
				       GValue     *value,
				       GParamSpec *pspec)
{
  GConverterInputStreamPrivate *priv;
  GConverterInputStream        *cstream;

  cstream = G_CONVERTER_INPUT_STREAM (object);
  priv = cstream->priv;

  switch (prop_id)
    {
    case PROP_CONVERTER:
      g_value_set_object (value, priv->converter);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }

}
static void
g_converter_input_stream_init (GConverterInputStream *stream)
{
  stream->priv = g_converter_input_stream_get_instance_private (stream);
}

/**
 * g_converter_input_stream_new:
 * @base_stream: a #GInputStream
 * @converter: a #GConverter
 *
 * Creates a new converter input stream for the @base_stream.
 *
 * Returns: a new #GInputStream.
 **/
GInputStream *
g_converter_input_stream_new (GInputStream *base_stream,
                              GConverter   *converter)
{
  GInputStream *stream;

  g_return_val_if_fail (G_IS_INPUT_STREAM (base_stream), NULL);

  stream = g_object_new (G_TYPE_CONVERTER_INPUT_STREAM,
                         "base-stream", base_stream,
			 "converter", converter,
			 NULL);

  return stream;
}

static gsize
buffer_data_size (Buffer *buffer)
{
  return buffer->end - buffer->start;
}

static gsize
buffer_tailspace (Buffer *buffer)
{
  return buffer->size - buffer->end;
}

static char *
buffer_data (Buffer *buffer)
{
  return buffer->data + buffer->start;
}

static void
buffer_consumed (Buffer *buffer,
		 gsize count)
{
  buffer->start += count;
  if (buffer->start == buffer->end)
    buffer->start = buffer->end = 0;
}

static void
buffer_read (Buffer *buffer,
	     char *dest,
	     gsize count)
{
  if (count != 0)
    memcpy (dest, buffer->data + buffer->start, count);

  buffer_consumed (buffer, count);
}

static void
compact_buffer (Buffer *buffer)
{
  gsize in_buffer;

  in_buffer = buffer_data_size (buffer);
  memmove (buffer->data,
	   buffer->data + buffer->start,
	   in_buffer);
  buffer->end -= buffer->start;
  buffer->start = 0;
}

static void
grow_buffer (Buffer *buffer)
{
  char *data;
  gsize size, in_buffer;

  if (buffer->size == 0)
    size = INITIAL_BUFFER_SIZE;
  else
    size = buffer->size * 2;

  data = g_malloc (size);
  in_buffer = buffer_data_size (buffer);

  if (in_buffer != 0)
    memcpy (data,
            buffer->data + buffer->start,
            in_buffer);

  g_free (buffer->data);
  buffer->data = data;
  buffer->end -= buffer->start;
  buffer->start = 0;
  buffer->size = size;
}

/* Ensures that the buffer can fit at_least_size bytes,
 * *including* the current in-buffer data */
static void
buffer_ensure_space (Buffer *buffer,
		     gsize at_least_size)
{
  gsize in_buffer, left_to_fill;

  in_buffer = buffer_data_size (buffer);

  if (in_buffer >= at_least_size)
    return;

  left_to_fill = buffer_tailspace (buffer);

  if (in_buffer + left_to_fill >= at_least_size)
    {
      /* We fit in remaining space at end */
      /* If the copy is small, compact now anyway so we can fill more */
      if (in_buffer < 256)
	compact_buffer (buffer);
    }
  else if (buffer->size >= at_least_size)
    {
      /* We fit, but only if we compact */
      compact_buffer (buffer);
    }
  else
    {
      /* Need to grow buffer */
      while (buffer->size < at_least_size)
	grow_buffer (buffer);
    }
}

static gssize
fill_input_buffer (GConverterInputStream  *stream,
		   gsize                   at_least_size,
		   gboolean                blocking,
		   GCancellable           *cancellable,
		   GError                **error)
{
  GConverterInputStreamPrivate *priv;
  GInputStream *base_stream;
  gssize nread;

  priv = stream->priv;

  buffer_ensure_space (&priv->input_buffer, at_least_size);

  base_stream = G_FILTER_INPUT_STREAM (stream)->base_stream;
  nread = g_pollable_stream_read (base_stream,
				  priv->input_buffer.data + priv->input_buffer.end,
				  buffer_tailspace (&priv->input_buffer),
				  blocking,
				  cancellable,
				  error);

  if (nread > 0)
    {
      priv->input_buffer.end += nread;
      priv->need_input = FALSE;
    }

  return nread;
}


static gssize
read_internal (GInputStream *stream,
	       void         *buffer,
	       gsize         count,
	       gboolean      blocking,
	       GCancellable *cancellable,
	       GError      **error)
{
  GConverterInputStream *cstream;
  GConverterInputStreamPrivate *priv;
  gsize available, total_bytes_read;
  gssize nread;
  GConverterResult res;
  gsize bytes_read;
  gsize bytes_written;
  GError *my_error;
  GError *my_error2;

  cstream = G_CONVERTER_INPUT_STREAM (stream);
  priv = cstream->priv;

  available = buffer_data_size (&priv->converted_buffer);

  if (available > 0 &&
      count <= available)
    {
      /* Converted data available, return that */
      buffer_read (&priv->converted_buffer, buffer, count);
      return count;
    }

  /* Full request not available, read all currently available and request
     refill/conversion for more */

  buffer_read (&priv->converted_buffer, buffer, available);

  total_bytes_read = available;
  buffer = (char *) buffer + available;
  count -= available;

  /* If there is no data to convert, and no pre-converted data,
     do some i/o for more input */
  if (buffer_data_size (&priv->input_buffer) == 0 &&
      total_bytes_read == 0 &&
      !priv->at_input_end)
    {
      nread = fill_input_buffer (cstream, count, blocking, cancellable, error);
      if (nread < 0)
	return -1;
      if (nread == 0)
	priv->at_input_end = TRUE;
    }

  /* First try to convert any available data (or state) directly to the user buffer: */
  if (!priv->finished)
    {
      my_error = NULL;
      res = g_converter_convert (priv->converter,
				 buffer_data (&priv->input_buffer),
				 buffer_data_size (&priv->input_buffer),
				 buffer, count,
				 priv->at_input_end ? G_CONVERTER_INPUT_AT_END : 0,
				 &bytes_read,
				 &bytes_written,
				 &my_error);
      if (res != G_CONVERTER_ERROR)
	{
	  total_bytes_read += bytes_written;
	  buffer_consumed (&priv->input_buffer, bytes_read);
	  if (res == G_CONVERTER_FINISHED)
	    priv->finished = TRUE; /* We're done converting */
	}
      else if (total_bytes_read == 0 &&
	       !g_error_matches (my_error,
				 G_IO_ERROR,
				 G_IO_ERROR_PARTIAL_INPUT) &&
	       !g_error_matches (my_error,
				 G_IO_ERROR,
				 G_IO_ERROR_NO_SPACE))
	{
	  /* No previously read data and no "special" error, return error */
	  g_propagate_error (error, my_error);
	  return -1;
	}
      else
	g_error_free (my_error);
    }

  /* We had some pre-converted data and/or we converted directly to the
     user buffer */
  if (total_bytes_read > 0)
    return total_bytes_read;

  /* If there is no more to convert, return EOF */
  if (priv->finished)
    {
      g_assert (buffer_data_size (&priv->converted_buffer) == 0);
      return 0;
    }

  /* There was "complexity" in the straight-to-buffer conversion,
   * convert to our own buffer and write from that.
   * At this point we didn't produce any data into @buffer.
   */

  /* Ensure we have *some* initial target space */
  buffer_ensure_space (&priv->converted_buffer, count);

  while (TRUE)
    {
      g_assert (!priv->finished);

      /* Try to convert to our buffer */
      my_error = NULL;
      res = g_converter_convert (priv->converter,
				 buffer_data (&priv->input_buffer),
				 buffer_data_size (&priv->input_buffer),
				 buffer_data (&priv->converted_buffer),
				 buffer_tailspace (&priv->converted_buffer),
				 priv->at_input_end ? G_CONVERTER_INPUT_AT_END : 0,
				 &bytes_read,
				 &bytes_written,
				 &my_error);
      if (res != G_CONVERTER_ERROR)
	{
	  priv->converted_buffer.end += bytes_written;
	  buffer_consumed (&priv->input_buffer, bytes_read);

	  /* Maybe we consumed without producing any output */
	  if (buffer_data_size (&priv->converted_buffer) == 0 && res != G_CONVERTER_FINISHED)
	    continue; /* Convert more */

	  if (res == G_CONVERTER_FINISHED)
	    priv->finished = TRUE;

	  total_bytes_read = MIN (count, buffer_data_size (&priv->converted_buffer));
	  buffer_read (&priv->converted_buffer, buffer, total_bytes_read);

	  g_assert (priv->finished || total_bytes_read > 0);

	  return total_bytes_read;
	}

      /* There was some kind of error filling our buffer */

      if (g_error_matches (my_error,
			   G_IO_ERROR,
			   G_IO_ERROR_PARTIAL_INPUT) &&
	  !priv->at_input_end)
	{
	  /* Need more data */
	  my_error2 = NULL;
	  nread = fill_input_buffer (cstream,
				     buffer_data_size (&priv->input_buffer) + 4096,
				     blocking,
				     cancellable,
				     &my_error2);
	  if (nread < 0)
	    {
	      /* Can't read any more data, return that error */
	      g_error_free (my_error);
	      g_propagate_error (error, my_error2);
	      priv->need_input = TRUE;
	      return -1;
	    }
	  else if (nread == 0)
	    {
	      /* End of file, try INPUT_AT_END */
	      priv->at_input_end = TRUE;
	    }
	  g_error_free (my_error);
	  continue;
	}

      if (g_error_matches (my_error,
			   G_IO_ERROR,
			   G_IO_ERROR_NO_SPACE))
	{
	  /* Need more destination space, grow it
	   * Note: if we actually grow the buffer (as opposed to compacting it),
	   * this will double the size, not just add one byte. */
	  buffer_ensure_space (&priv->converted_buffer,
			       priv->converted_buffer.size + 1);
	  g_error_free (my_error);
	  continue;
	}

      /* Any other random error, return it */
      g_propagate_error (error, my_error);
      return -1;
    }

  g_assert_not_reached ();
}

static gssize
g_converter_input_stream_read (GInputStream *stream,
			       void         *buffer,
			       gsize         count,
			       GCancellable *cancellable,
			       GError      **error)
{
  return read_internal (stream, buffer, count, TRUE, cancellable, error);
}

static gboolean
g_converter_input_stream_can_poll (GPollableInputStream *stream)
{
  GInputStream *base_stream = G_FILTER_INPUT_STREAM (stream)->base_stream;

  return (G_IS_POLLABLE_INPUT_STREAM (base_stream) &&
	  g_pollable_input_stream_can_poll (G_POLLABLE_INPUT_STREAM (base_stream)));
}

static gboolean
g_converter_input_stream_is_readable (GPollableInputStream *stream)
{
  GInputStream *base_stream = G_FILTER_INPUT_STREAM (stream)->base_stream;
  GConverterInputStream *cstream = G_CONVERTER_INPUT_STREAM (stream);

  if (buffer_data_size (&cstream->priv->converted_buffer))
    return TRUE;
  else if (buffer_data_size (&cstream->priv->input_buffer) &&
	   !cstream->priv->need_input)
    return TRUE;
  else
    return g_pollable_input_stream_is_readable (G_POLLABLE_INPUT_STREAM (base_stream));
}

static gssize
g_converter_input_stream_read_nonblocking (GPollableInputStream  *stream,
					   void                  *buffer,
					   gsize                  count,
					   GError               **error)
{
  return read_internal (G_INPUT_STREAM (stream), buffer, count,
			FALSE, NULL, error);
}

static GSource *
g_converter_input_stream_create_source (GPollableInputStream *stream,
					GCancellable         *cancellable)
{
  GInputStream *base_stream = G_FILTER_INPUT_STREAM (stream)->base_stream;
  GSource *base_source, *pollable_source;

  if (g_pollable_input_stream_is_readable (stream))
    base_source = g_timeout_source_new (0);
  else
    base_source = g_pollable_input_stream_create_source (G_POLLABLE_INPUT_STREAM (base_stream), NULL);

  pollable_source = g_pollable_source_new_full (stream, base_source,
						cancellable);
  g_source_unref (base_source);

  return pollable_source;
}


/**
 * g_converter_input_stream_get_converter:
 * @converter_stream: a #GConverterInputStream
 *
 * Gets the #GConverter that is used by @converter_stream.
 *
 * Returns: (transfer none): the converter of the converter input stream
 *
 * Since: 2.24
 */
GConverter *
g_converter_input_stream_get_converter (GConverterInputStream *converter_stream)
{
  return converter_stream->priv->converter;
}
