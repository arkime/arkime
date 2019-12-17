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

#include "gconverteroutputstream.h"
#include "gpollableoutputstream.h"
#include "gcancellable.h"
#include "gioenumtypes.h"
#include "gioerror.h"
#include "glibintl.h"


/**
 * SECTION:gconverteroutputstream
 * @short_description: Converter Output Stream
 * @include: gio/gio.h
 * @see_also: #GOutputStream, #GConverter
 *
 * Converter output stream implements #GOutputStream and allows
 * conversion of data of various types during reading.
 *
 * As of GLib 2.34, #GConverterOutputStream implements
 * #GPollableOutputStream.
 **/

#define INITIAL_BUFFER_SIZE 4096

typedef struct {
  char *data;
  gsize start;
  gsize end;
  gsize size;
} Buffer;

struct _GConverterOutputStreamPrivate {
  gboolean at_output_end;
  gboolean finished;
  GConverter *converter;
  Buffer output_buffer; /* To be converted and written */
  Buffer converted_buffer; /* Already converted */
};

/* Buffering strategy:
 *
 * Each time we write we must at least consume some input, or
 * return an error. Thus we start with writing all already
 * converted data and *then* we start converting (reporting
 * an error at any point in this).
 *
 * Its possible that what the user wrote is not enough data
 * for the converter, so we must then buffer it in output_buffer
 * and ask for more data, but we want to avoid this as much as
 * possible, converting directly from the users buffer.
 */

enum {
  PROP_0,
  PROP_CONVERTER
};

static void   g_converter_output_stream_set_property (GObject        *object,
						      guint           prop_id,
						      const GValue   *value,
						      GParamSpec     *pspec);
static void   g_converter_output_stream_get_property (GObject        *object,
						      guint           prop_id,
						      GValue         *value,
						      GParamSpec     *pspec);
static void   g_converter_output_stream_finalize     (GObject        *object);
static gssize g_converter_output_stream_write        (GOutputStream  *stream,
						      const void     *buffer,
						      gsize           count,
						      GCancellable   *cancellable,
						      GError        **error);
static gboolean g_converter_output_stream_flush      (GOutputStream  *stream,
						      GCancellable   *cancellable,
						      GError        **error);

static gboolean g_converter_output_stream_can_poll          (GPollableOutputStream *stream);
static gboolean g_converter_output_stream_is_writable       (GPollableOutputStream *stream);
static gssize   g_converter_output_stream_write_nonblocking (GPollableOutputStream  *stream,
							     const void             *buffer,
							     gsize                  size,
							     GError               **error);

static GSource *g_converter_output_stream_create_source     (GPollableOutputStream *stream,
							     GCancellable          *cancellable);

static void g_converter_output_stream_pollable_iface_init (GPollableOutputStreamInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GConverterOutputStream,
			 g_converter_output_stream,
			 G_TYPE_FILTER_OUTPUT_STREAM,
                         G_ADD_PRIVATE (GConverterOutputStream)
			 G_IMPLEMENT_INTERFACE (G_TYPE_POLLABLE_OUTPUT_STREAM,
						g_converter_output_stream_pollable_iface_init))

static void
g_converter_output_stream_class_init (GConverterOutputStreamClass *klass)
{
  GObjectClass *object_class;
  GOutputStreamClass *istream_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->get_property = g_converter_output_stream_get_property;
  object_class->set_property = g_converter_output_stream_set_property;
  object_class->finalize     = g_converter_output_stream_finalize;

  istream_class = G_OUTPUT_STREAM_CLASS (klass);
  istream_class->write_fn = g_converter_output_stream_write;
  istream_class->flush = g_converter_output_stream_flush;

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
g_converter_output_stream_pollable_iface_init (GPollableOutputStreamInterface *iface)
{
  iface->can_poll = g_converter_output_stream_can_poll;
  iface->is_writable = g_converter_output_stream_is_writable;
  iface->write_nonblocking = g_converter_output_stream_write_nonblocking;
  iface->create_source = g_converter_output_stream_create_source;
}

static void
g_converter_output_stream_finalize (GObject *object)
{
  GConverterOutputStreamPrivate *priv;
  GConverterOutputStream        *stream;

  stream = G_CONVERTER_OUTPUT_STREAM (object);
  priv = stream->priv;

  g_free (priv->output_buffer.data);
  g_free (priv->converted_buffer.data);
  if (priv->converter)
    g_object_unref (priv->converter);

  G_OBJECT_CLASS (g_converter_output_stream_parent_class)->finalize (object);
}

static void
g_converter_output_stream_set_property (GObject      *object,
				       guint         prop_id,
				       const GValue *value,
				       GParamSpec   *pspec)
{
  GConverterOutputStream *cstream;

  cstream = G_CONVERTER_OUTPUT_STREAM (object);

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
g_converter_output_stream_get_property (GObject    *object,
				       guint       prop_id,
				       GValue     *value,
				       GParamSpec *pspec)
{
  GConverterOutputStreamPrivate *priv;
  GConverterOutputStream        *cstream;

  cstream = G_CONVERTER_OUTPUT_STREAM (object);
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
g_converter_output_stream_init (GConverterOutputStream *stream)
{
  stream->priv = g_converter_output_stream_get_instance_private (stream);
}

/**
 * g_converter_output_stream_new:
 * @base_stream: a #GOutputStream
 * @converter: a #GConverter
 *
 * Creates a new converter output stream for the @base_stream.
 *
 * Returns: a new #GOutputStream.
 **/
GOutputStream *
g_converter_output_stream_new (GOutputStream *base_stream,
                               GConverter    *converter)
{
  GOutputStream *stream;

  g_return_val_if_fail (G_IS_OUTPUT_STREAM (base_stream), NULL);

  stream = g_object_new (G_TYPE_CONVERTER_OUTPUT_STREAM,
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

static void
buffer_append (Buffer *buffer,
	       const char *data,
	       gsize data_size)
{
  buffer_ensure_space (buffer,
		       buffer_data_size (buffer) + data_size);
  memcpy (buffer->data + buffer->end, data, data_size);
  buffer->end += data_size;
}


static gboolean
flush_buffer (GConverterOutputStream *stream,
	      gboolean                blocking,
	      GCancellable           *cancellable,
	      GError                **error)
{
  GConverterOutputStreamPrivate *priv;
  GOutputStream *base_stream;
  gsize nwritten;
  gsize available;
  gboolean res;

  priv = stream->priv;

  base_stream = G_FILTER_OUTPUT_STREAM (stream)->base_stream;

  available = buffer_data_size (&priv->converted_buffer);
  if (available > 0)
    {
      res = g_pollable_stream_write_all (base_stream,
					 buffer_data (&priv->converted_buffer),
					 available,
					 blocking,
					 &nwritten,
					 cancellable,
					 error);
      buffer_consumed (&priv->converted_buffer, nwritten);
      return res;
    }
  return TRUE;
}


static gssize
write_internal (GOutputStream  *stream,
		const void     *buffer,
		gsize           count,
		gboolean        blocking,
		GCancellable   *cancellable,
		GError        **error)
{
  GConverterOutputStream *cstream;
  GConverterOutputStreamPrivate *priv;
  gssize retval;
  GConverterResult res;
  gsize bytes_read;
  gsize bytes_written;
  GError *my_error;
  const char *to_convert;
  gsize to_convert_size, converted_bytes;
  gboolean converting_from_buffer;

  cstream = G_CONVERTER_OUTPUT_STREAM (stream);
  priv = cstream->priv;

  /* Write out all available pre-converted data and fail if
     not possible */
  if (!flush_buffer (cstream, blocking, cancellable, error))
    return -1;

  if (priv->finished)
    return 0;

  /* Convert as much as possible */
  if (buffer_data_size (&priv->output_buffer) > 0)
    {
      converting_from_buffer = TRUE;
      buffer_append (&priv->output_buffer, buffer, count);
      to_convert = buffer_data (&priv->output_buffer);
      to_convert_size = buffer_data_size (&priv->output_buffer);
    }
  else
    {
      converting_from_buffer = FALSE;
      to_convert = buffer;
      to_convert_size = count;
    }

  /* Ensure we have *some* initial target space */
  buffer_ensure_space (&priv->converted_buffer, to_convert_size);

  converted_bytes = 0;
  while (!priv->finished && converted_bytes < to_convert_size)
    {
      /* Ensure we have *some* target space */
      if (buffer_tailspace (&priv->converted_buffer) == 0)
	grow_buffer (&priv->converted_buffer);

      /* Try to convert to our buffer */
      my_error = NULL;
      res = g_converter_convert (priv->converter,
				 to_convert + converted_bytes,
				 to_convert_size - converted_bytes,
				 buffer_data (&priv->converted_buffer) + buffer_data_size (&priv->converted_buffer),
				 buffer_tailspace (&priv->converted_buffer),
				 0,
				 &bytes_read,
				 &bytes_written,
				 &my_error);

      if (res != G_CONVERTER_ERROR)
	{
	  priv->converted_buffer.end += bytes_written;
	  converted_bytes += bytes_read;

	  if (res == G_CONVERTER_FINISHED)
	    priv->finished = TRUE;
	}
      else
	{
	  /* No-space errors can be handled locally: */
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

	  if (converted_bytes > 0)
	    {
	      /* We got an conversion error, but we did convert some bytes before
		 that, so handle those before reporting the error */
	      g_error_free (my_error);
	      break;
	    }

	  if (g_error_matches (my_error,
			       G_IO_ERROR,
			       G_IO_ERROR_PARTIAL_INPUT))
	    {
	      /* Consume everything to buffer that we append to next time
		 we write */
	      if (!converting_from_buffer)
		buffer_append (&priv->output_buffer, buffer, count);
	      /* in the converting_from_buffer case we already appended this */

              g_error_free (my_error);
	      return count; /* consume everything */
	    }

	  /* Converted no data and got an normal error, return it */
	  g_propagate_error (error, my_error);
	  return -1;
	}
    }

  if (converting_from_buffer)
    {
      buffer_consumed (&priv->output_buffer, converted_bytes);
      retval = count;
    }
  else
    retval = converted_bytes;

  /* We now successfully consumed retval bytes, so we can't return an error,
     even if writing this to the base stream fails. If it does we'll just
     stop early and report this error when we try again on the next
     write call. */
  flush_buffer (cstream, blocking, cancellable, NULL);

  return retval;
}

static gssize
g_converter_output_stream_write (GOutputStream  *stream,
				 const void     *buffer,
				 gsize           count,
				 GCancellable   *cancellable,
				 GError        **error)
{
  return write_internal (stream, buffer, count, TRUE, cancellable, error);
}

static gboolean
g_converter_output_stream_flush (GOutputStream  *stream,
				 GCancellable   *cancellable,
				 GError        **error)
{
  GConverterOutputStream *cstream;
  GConverterOutputStreamPrivate *priv;
  GConverterResult res;
  GError *my_error;
  gboolean is_closing;
  gboolean flushed;
  gsize bytes_read;
  gsize bytes_written;

  cstream = G_CONVERTER_OUTPUT_STREAM (stream);
  priv = cstream->priv;

  is_closing = g_output_stream_is_closing (stream);

  /* Write out all available pre-converted data and fail if
     not possible */
  if (!flush_buffer (cstream, TRUE, cancellable, error))
    return FALSE;

  /* Ensure we have *some* initial target space */
  buffer_ensure_space (&priv->converted_buffer, 1);

  /* Convert whole buffer */
  flushed = FALSE;
  while (!priv->finished && !flushed)
    {
      /* Ensure we have *some* target space */
      if (buffer_tailspace (&priv->converted_buffer) == 0)
	grow_buffer (&priv->converted_buffer);

      /* Try to convert to our buffer */
      my_error = NULL;
      res = g_converter_convert (priv->converter,
				 buffer_data (&priv->output_buffer),
				 buffer_data_size (&priv->output_buffer),
				 buffer_data (&priv->converted_buffer) + buffer_data_size (&priv->converted_buffer),
				 buffer_tailspace (&priv->converted_buffer),
				 is_closing ? G_CONVERTER_INPUT_AT_END : G_CONVERTER_FLUSH,
				 &bytes_read,
				 &bytes_written,
				 &my_error);

      if (res != G_CONVERTER_ERROR)
	{
	  priv->converted_buffer.end += bytes_written;
	  buffer_consumed (&priv->output_buffer, bytes_read);

	  if (res == G_CONVERTER_FINISHED)
	    priv->finished = TRUE;
	  if (!is_closing &&
	      res == G_CONVERTER_FLUSHED)
	    {
	      /* Should not have retured FLUSHED with input left */
	      g_assert (buffer_data_size (&priv->output_buffer) == 0);
	      flushed = TRUE;
	    }
	}
      else
	{
	  /* No-space errors can be handled locally: */
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

	  /* Any other error, including PARTIAL_INPUT can't be fixed by now
	     and is an error */
	  g_propagate_error (error, my_error);
	  return FALSE;
	}
    }

  /* Now write all converted data to base stream */
  if (!flush_buffer (cstream, TRUE, cancellable, error))
    return FALSE;

  return TRUE;
}

static gboolean
g_converter_output_stream_can_poll (GPollableOutputStream *stream)
{
  GOutputStream *base_stream = G_FILTER_OUTPUT_STREAM (stream)->base_stream;

  return (G_IS_POLLABLE_OUTPUT_STREAM (base_stream) &&
	  g_pollable_output_stream_can_poll (G_POLLABLE_OUTPUT_STREAM (base_stream)));
}

static gboolean
g_converter_output_stream_is_writable (GPollableOutputStream *stream)
{
  GOutputStream *base_stream = G_FILTER_OUTPUT_STREAM (stream)->base_stream;

  return g_pollable_output_stream_is_writable (G_POLLABLE_OUTPUT_STREAM (base_stream));
}

static gssize
g_converter_output_stream_write_nonblocking (GPollableOutputStream  *stream,
					     const void             *buffer,
					     gsize                   count,
					     GError                **error)
{
  return write_internal (G_OUTPUT_STREAM (stream), buffer, count, FALSE,
			 NULL, error);
}

static GSource *
g_converter_output_stream_create_source (GPollableOutputStream *stream,
					 GCancellable          *cancellable)
{
  GOutputStream *base_stream = G_FILTER_OUTPUT_STREAM (stream)->base_stream;
  GSource *base_source, *pollable_source;

  base_source = g_pollable_output_stream_create_source (G_POLLABLE_OUTPUT_STREAM (base_stream), NULL);
  pollable_source = g_pollable_source_new_full (stream, base_source,
						cancellable);
  g_source_unref (base_source);

  return pollable_source;
}

/**
 * g_converter_output_stream_get_converter:
 * @converter_stream: a #GConverterOutputStream
 *
 * Gets the #GConverter that is used by @converter_stream.
 *
 * Returns: (transfer none): the converter of the converter output stream
 *
 * Since: 2.24
 */
GConverter *
g_converter_output_stream_get_converter (GConverterOutputStream *converter_stream)
{
  return converter_stream->priv->converter;
}
