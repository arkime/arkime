/* GIO - GLib Input, Output and Streaming Library
 * 
 * Copyright (C) 2006-2007 Red Hat, Inc.
 * Copyright (C) 2007 Jürg Billeter
 * Copyright © 2009 Codethink Limited
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
#include "gdatainputstream.h"
#include "gtask.h"
#include "gcancellable.h"
#include "gioenumtypes.h"
#include "gioerror.h"
#include "glibintl.h"

#include <string.h>

/**
 * SECTION:gdatainputstream
 * @short_description: Data Input Stream
 * @include: gio/gio.h
 * @see_also: #GInputStream
 * 
 * Data input stream implements #GInputStream and includes functions for 
 * reading structured data directly from a binary input stream.
 *
 **/

struct _GDataInputStreamPrivate {
  GDataStreamByteOrder byte_order;
  GDataStreamNewlineType newline_type;
};

enum {
  PROP_0,
  PROP_BYTE_ORDER,
  PROP_NEWLINE_TYPE
};

static void g_data_input_stream_set_property (GObject      *object,
					      guint         prop_id,
					      const GValue *value,
					      GParamSpec   *pspec);
static void g_data_input_stream_get_property (GObject      *object,
					      guint         prop_id,
					      GValue       *value,
					      GParamSpec   *pspec);

G_DEFINE_TYPE_WITH_PRIVATE (GDataInputStream,
                            g_data_input_stream,
                            G_TYPE_BUFFERED_INPUT_STREAM)


static void
g_data_input_stream_class_init (GDataInputStreamClass *klass)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->get_property = g_data_input_stream_get_property;
  object_class->set_property = g_data_input_stream_set_property;

  /**
   * GDataStream:byte-order:
   *
   * The ::byte-order property determines the byte ordering that
   * is used when reading multi-byte entities (such as integers)
   * from the stream.
   */ 
  g_object_class_install_property (object_class,
                                   PROP_BYTE_ORDER,
                                   g_param_spec_enum ("byte-order",
                                                      P_("Byte order"),
                                                      P_("The byte order"),
                                                      G_TYPE_DATA_STREAM_BYTE_ORDER,
                                                      G_DATA_STREAM_BYTE_ORDER_BIG_ENDIAN,
                                                      G_PARAM_READWRITE|G_PARAM_STATIC_NAME|G_PARAM_STATIC_BLURB));

  /**
   * GDataStream:newline-type:
   *
   * The :newline-type property determines what is considered
   * as a line ending when reading complete lines from the stream.
   */ 
  g_object_class_install_property (object_class,
                                   PROP_NEWLINE_TYPE,
                                   g_param_spec_enum ("newline-type",
                                                      P_("Newline type"),
                                                      P_("The accepted types of line ending"),
                                                      G_TYPE_DATA_STREAM_NEWLINE_TYPE,
                                                      G_DATA_STREAM_NEWLINE_TYPE_LF,
                                                      G_PARAM_READWRITE|G_PARAM_STATIC_NAME|G_PARAM_STATIC_BLURB));
}

static void
g_data_input_stream_set_property (GObject      *object,
				  guint         prop_id,
				  const GValue *value,
				  GParamSpec   *pspec)
{
  GDataInputStream        *dstream;

  dstream = G_DATA_INPUT_STREAM (object);

   switch (prop_id)
    {
    case PROP_BYTE_ORDER:
      g_data_input_stream_set_byte_order (dstream, g_value_get_enum (value));
      break;

    case PROP_NEWLINE_TYPE:
      g_data_input_stream_set_newline_type (dstream, g_value_get_enum (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }

}

static void
g_data_input_stream_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  GDataInputStreamPrivate *priv;
  GDataInputStream        *dstream;

  dstream = G_DATA_INPUT_STREAM (object);
  priv = dstream->priv;

  switch (prop_id)
    { 
    case PROP_BYTE_ORDER:
      g_value_set_enum (value, priv->byte_order);
      break;

    case PROP_NEWLINE_TYPE:
      g_value_set_enum (value, priv->newline_type);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }

}
static void
g_data_input_stream_init (GDataInputStream *stream)
{
  stream->priv = g_data_input_stream_get_instance_private (stream);
  stream->priv->byte_order = G_DATA_STREAM_BYTE_ORDER_BIG_ENDIAN;
  stream->priv->newline_type = G_DATA_STREAM_NEWLINE_TYPE_LF;
}

/**
 * g_data_input_stream_new:
 * @base_stream: a #GInputStream.
 * 
 * Creates a new data input stream for the @base_stream.
 * 
 * Returns: a new #GDataInputStream.
 **/
GDataInputStream *
g_data_input_stream_new (GInputStream *base_stream)
{
  GDataInputStream *stream;

  g_return_val_if_fail (G_IS_INPUT_STREAM (base_stream), NULL);

  stream = g_object_new (G_TYPE_DATA_INPUT_STREAM,
                         "base-stream", base_stream,
                         NULL);

  return stream;
}

/**
 * g_data_input_stream_set_byte_order:
 * @stream: a given #GDataInputStream.
 * @order: a #GDataStreamByteOrder to set.
 * 
 * This function sets the byte order for the given @stream. All subsequent
 * reads from the @stream will be read in the given @order.
 *  
 **/
void
g_data_input_stream_set_byte_order (GDataInputStream     *stream,
				    GDataStreamByteOrder  order)
{
  GDataInputStreamPrivate *priv;

  g_return_if_fail (G_IS_DATA_INPUT_STREAM (stream));

  priv = stream->priv;

  if (priv->byte_order != order)
    {
      priv->byte_order = order;
      
      g_object_notify (G_OBJECT (stream), "byte-order");
    }
}

/**
 * g_data_input_stream_get_byte_order:
 * @stream: a given #GDataInputStream.
 * 
 * Gets the byte order for the data input stream.
 * 
 * Returns: the @stream's current #GDataStreamByteOrder. 
 **/
GDataStreamByteOrder
g_data_input_stream_get_byte_order (GDataInputStream *stream)
{
  g_return_val_if_fail (G_IS_DATA_INPUT_STREAM (stream), G_DATA_STREAM_BYTE_ORDER_HOST_ENDIAN);

  return stream->priv->byte_order;
}

/**
 * g_data_input_stream_set_newline_type:
 * @stream: a #GDataInputStream.
 * @type: the type of new line return as #GDataStreamNewlineType.
 * 
 * Sets the newline type for the @stream.
 * 
 * Note that using G_DATA_STREAM_NEWLINE_TYPE_ANY is slightly unsafe. If a read
 * chunk ends in "CR" we must read an additional byte to know if this is "CR" or
 * "CR LF", and this might block if there is no more data available.
 *  
 **/
void
g_data_input_stream_set_newline_type (GDataInputStream       *stream,
				      GDataStreamNewlineType  type)
{
  GDataInputStreamPrivate *priv;

  g_return_if_fail (G_IS_DATA_INPUT_STREAM (stream));

  priv = stream->priv;
  
  if (priv->newline_type != type)
    {
      priv->newline_type = type;

      g_object_notify (G_OBJECT (stream), "newline-type");
    }
}

/**
 * g_data_input_stream_get_newline_type:
 * @stream: a given #GDataInputStream.
 * 
 * Gets the current newline type for the @stream.
 * 
 * Returns: #GDataStreamNewlineType for the given @stream.
 **/
GDataStreamNewlineType
g_data_input_stream_get_newline_type (GDataInputStream *stream)
{
  g_return_val_if_fail (G_IS_DATA_INPUT_STREAM (stream), G_DATA_STREAM_NEWLINE_TYPE_ANY);

  return stream->priv->newline_type;
}

static gboolean
read_data (GDataInputStream  *stream,
           void              *buffer,
           gsize              size,
           GCancellable      *cancellable,
           GError           **error)
{
  gsize available;
  gssize res;

  while ((available = g_buffered_input_stream_get_available (G_BUFFERED_INPUT_STREAM (stream))) < size)
    {
      res = g_buffered_input_stream_fill (G_BUFFERED_INPUT_STREAM (stream),
					  size - available,
					  cancellable, error);
      if (res < 0)
	return FALSE;
      if (res == 0)
	{
	  g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                               _("Unexpected early end-of-stream"));
	  return FALSE;
	}
    }
  
  /* This should always succeed, since it's in the buffer */
  res = g_input_stream_read (G_INPUT_STREAM (stream),
			     buffer, size,
			     NULL, NULL);
  g_warn_if_fail (res == size);
  return TRUE;
}


/**
 * g_data_input_stream_read_byte:
 * @stream: a given #GDataInputStream.
 * @cancellable: (nullable): optional #GCancellable object, %NULL to ignore.
 * @error: #GError for error reporting.
 * 
 * Reads an unsigned 8-bit/1-byte value from @stream.
 *
 * Returns: an unsigned 8-bit/1-byte value read from the @stream or %0 
 * if an error occurred.
 **/
guchar
g_data_input_stream_read_byte (GDataInputStream  *stream,
			       GCancellable       *cancellable,
			       GError            **error)
{
  guchar c;
  
  g_return_val_if_fail (G_IS_DATA_INPUT_STREAM (stream), '\0');
  
  if (read_data (stream, &c, 1, cancellable, error))
      return c;
  
  return 0;
}


/**
 * g_data_input_stream_read_int16:
 * @stream: a given #GDataInputStream.
 * @cancellable: (nullable): optional #GCancellable object, %NULL to ignore.
 * @error: #GError for error reporting.
 * 
 * Reads a 16-bit/2-byte value from @stream.
 *
 * In order to get the correct byte order for this read operation, 
 * see g_data_input_stream_get_byte_order() and g_data_input_stream_set_byte_order().
 * 
 * Returns: a signed 16-bit/2-byte value read from @stream or %0 if 
 * an error occurred.
 **/
gint16
g_data_input_stream_read_int16 (GDataInputStream  *stream,
			       GCancellable       *cancellable,
			       GError            **error)
{
  gint16 v;
  
  g_return_val_if_fail (G_IS_DATA_INPUT_STREAM (stream), 0);
  
  if (read_data (stream, &v, 2, cancellable, error))
    {
      switch (stream->priv->byte_order)
	{
	case G_DATA_STREAM_BYTE_ORDER_BIG_ENDIAN:
	  v = GINT16_FROM_BE (v);
	  break;
	case G_DATA_STREAM_BYTE_ORDER_LITTLE_ENDIAN:
	  v = GINT16_FROM_LE (v);
	  break;
	case G_DATA_STREAM_BYTE_ORDER_HOST_ENDIAN:
	default:
	  break;
	}
      return v;
    }
  
  return 0;
}


/**
 * g_data_input_stream_read_uint16:
 * @stream: a given #GDataInputStream.
 * @cancellable: (nullable): optional #GCancellable object, %NULL to ignore.
 * @error: #GError for error reporting.
 *
 * Reads an unsigned 16-bit/2-byte value from @stream.
 *
 * In order to get the correct byte order for this read operation, 
 * see g_data_input_stream_get_byte_order() and g_data_input_stream_set_byte_order(). 
 * 
 * Returns: an unsigned 16-bit/2-byte value read from the @stream or %0 if 
 * an error occurred. 
 **/
guint16
g_data_input_stream_read_uint16 (GDataInputStream  *stream,
				 GCancellable       *cancellable,
				 GError            **error)
{
  guint16 v;
  
  g_return_val_if_fail (G_IS_DATA_INPUT_STREAM (stream), 0);
  
  if (read_data (stream, &v, 2, cancellable, error))
    {
      switch (stream->priv->byte_order)
	{
	case G_DATA_STREAM_BYTE_ORDER_BIG_ENDIAN:
	  v = GUINT16_FROM_BE (v);
	  break;
	case G_DATA_STREAM_BYTE_ORDER_LITTLE_ENDIAN:
	  v = GUINT16_FROM_LE (v);
	  break;
	case G_DATA_STREAM_BYTE_ORDER_HOST_ENDIAN:
	default:
	  break;
	}
      return v;
    }
  
  return 0;
}


/**
 * g_data_input_stream_read_int32:
 * @stream: a given #GDataInputStream.
 * @cancellable: (nullable): optional #GCancellable object, %NULL to ignore.
 * @error: #GError for error reporting.
 * 
 * Reads a signed 32-bit/4-byte value from @stream.
 *
 * In order to get the correct byte order for this read operation, 
 * see g_data_input_stream_get_byte_order() and g_data_input_stream_set_byte_order().
 *
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. 
 *   
 * Returns: a signed 32-bit/4-byte value read from the @stream or %0 if 
 * an error occurred. 
 **/
gint32
g_data_input_stream_read_int32 (GDataInputStream  *stream,
				GCancellable       *cancellable,
				GError            **error)
{
  gint32 v;
  
  g_return_val_if_fail (G_IS_DATA_INPUT_STREAM (stream), 0);
  
  if (read_data (stream, &v, 4, cancellable, error))
    {
      switch (stream->priv->byte_order)
	{
	case G_DATA_STREAM_BYTE_ORDER_BIG_ENDIAN:
	  v = GINT32_FROM_BE (v);
	  break;
	case G_DATA_STREAM_BYTE_ORDER_LITTLE_ENDIAN:
	  v = GINT32_FROM_LE (v);
	  break;
	case G_DATA_STREAM_BYTE_ORDER_HOST_ENDIAN:
	default:
	  break;
	}
      return v;
    }
  
  return 0;
}


/**
 * g_data_input_stream_read_uint32:
 * @stream: a given #GDataInputStream.
 * @cancellable: (nullable): optional #GCancellable object, %NULL to ignore.
 * @error: #GError for error reporting.
 * 
 * Reads an unsigned 32-bit/4-byte value from @stream.
 *
 * In order to get the correct byte order for this read operation, 
 * see g_data_input_stream_get_byte_order() and g_data_input_stream_set_byte_order().
 *
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. 
 * 
 * Returns: an unsigned 32-bit/4-byte value read from the @stream or %0 if 
 * an error occurred. 
 **/
guint32
g_data_input_stream_read_uint32 (GDataInputStream  *stream,
				 GCancellable       *cancellable,
				 GError            **error)
{
  guint32 v;
  
  g_return_val_if_fail (G_IS_DATA_INPUT_STREAM (stream), 0);
  
  if (read_data (stream, &v, 4, cancellable, error))
    {
      switch (stream->priv->byte_order)
	{
	case G_DATA_STREAM_BYTE_ORDER_BIG_ENDIAN:
	  v = GUINT32_FROM_BE (v);
	  break;
	case G_DATA_STREAM_BYTE_ORDER_LITTLE_ENDIAN:
	  v = GUINT32_FROM_LE (v);
	  break;
	case G_DATA_STREAM_BYTE_ORDER_HOST_ENDIAN:
	default:
	  break;
	}
      return v;
    }
  
  return 0;
}


/**
 * g_data_input_stream_read_int64:
 * @stream: a given #GDataInputStream.
 * @cancellable: (nullable): optional #GCancellable object, %NULL to ignore.
 * @error: #GError for error reporting.
 * 
 * Reads a 64-bit/8-byte value from @stream.
 *
 * In order to get the correct byte order for this read operation, 
 * see g_data_input_stream_get_byte_order() and g_data_input_stream_set_byte_order().
 *
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. 
 * 
 * Returns: a signed 64-bit/8-byte value read from @stream or %0 if 
 * an error occurred.  
 **/
gint64
g_data_input_stream_read_int64 (GDataInputStream  *stream,
			       GCancellable       *cancellable,
			       GError            **error)
{
  gint64 v;
  
  g_return_val_if_fail (G_IS_DATA_INPUT_STREAM (stream), 0);
  
  if (read_data (stream, &v, 8, cancellable, error))
    {
      switch (stream->priv->byte_order)
	{
	case G_DATA_STREAM_BYTE_ORDER_BIG_ENDIAN:
	  v = GINT64_FROM_BE (v);
	  break;
	case G_DATA_STREAM_BYTE_ORDER_LITTLE_ENDIAN:
	  v = GINT64_FROM_LE (v);
	  break;
	case G_DATA_STREAM_BYTE_ORDER_HOST_ENDIAN:
	default:
	  break;
	}
      return v;
    }
  
  return 0;
}


/**
 * g_data_input_stream_read_uint64:
 * @stream: a given #GDataInputStream.
 * @cancellable: (nullable): optional #GCancellable object, %NULL to ignore.
 * @error: #GError for error reporting.
 * 
 * Reads an unsigned 64-bit/8-byte value from @stream.
 *
 * In order to get the correct byte order for this read operation, 
 * see g_data_input_stream_get_byte_order().
 *
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. 
 * 
 * Returns: an unsigned 64-bit/8-byte read from @stream or %0 if 
 * an error occurred. 
 **/
guint64
g_data_input_stream_read_uint64 (GDataInputStream  *stream,
				GCancellable       *cancellable,
				GError            **error)
{
  guint64 v;
  
  g_return_val_if_fail (G_IS_DATA_INPUT_STREAM (stream), 0);
  
  if (read_data (stream, &v, 8, cancellable, error))
    {
      switch (stream->priv->byte_order)
	{
	case G_DATA_STREAM_BYTE_ORDER_BIG_ENDIAN:
	  v = GUINT64_FROM_BE (v);
	  break;
	case G_DATA_STREAM_BYTE_ORDER_LITTLE_ENDIAN:
	  v = GUINT64_FROM_LE (v);
	  break;
	case G_DATA_STREAM_BYTE_ORDER_HOST_ENDIAN:
	default:
	  break;
	}
      return v;
    }
  
  return 0;
}

static gssize
scan_for_newline (GDataInputStream *stream,
		  gsize            *checked_out,
		  gboolean         *last_saw_cr_out,
		  int              *newline_len_out)
{
  GBufferedInputStream *bstream;
  GDataInputStreamPrivate *priv;
  const char *buffer;
  gsize start, end, peeked;
  int i;
  gssize found_pos;
  int newline_len;
  gsize available, checked;
  gboolean last_saw_cr;

  priv = stream->priv;
  
  bstream = G_BUFFERED_INPUT_STREAM (stream);

  checked = *checked_out;
  last_saw_cr = *last_saw_cr_out;
  found_pos = -1;
  newline_len = 0;
  
  start = checked;
  buffer = (const char*)g_buffered_input_stream_peek_buffer (bstream, &available) + start;
  end = available;
  peeked = end - start;

  for (i = 0; checked < available && i < peeked; i++)
    {
      switch (priv->newline_type)
	{
	case G_DATA_STREAM_NEWLINE_TYPE_LF:
	  if (buffer[i] == 10)
	    {
	      found_pos = start + i;
	      newline_len = 1;
	    }
	  break;
	case G_DATA_STREAM_NEWLINE_TYPE_CR:
	  if (buffer[i] == 13)
	    {
	      found_pos = start + i;
	      newline_len = 1;
	    }
	  break;
	case G_DATA_STREAM_NEWLINE_TYPE_CR_LF:
	  if (last_saw_cr && buffer[i] == 10)
	    {
	      found_pos = start + i - 1;
	      newline_len = 2;
	    }
	  break;
	default:
	case G_DATA_STREAM_NEWLINE_TYPE_ANY:
	  if (buffer[i] == 10) /* LF */
	    {
	      if (last_saw_cr)
		{
		  /* CR LF */
		  found_pos = start + i - 1;
		  newline_len = 2;
		}
	      else
		{
		  /* LF */
		  found_pos = start + i;
		  newline_len = 1;
		}
	    }
	  else if (last_saw_cr)
	    {
	      /* Last was cr, this is not LF, end is CR */
	      found_pos = start + i - 1;
	      newline_len = 1;
	    }
	  /* Don't check for CR here, instead look at last_saw_cr on next byte */
	  break;
	}
	
      last_saw_cr = (buffer[i] == 13);

      if (found_pos != -1)
	{
	  *newline_len_out = newline_len;
	  return found_pos;
	}
    }

  checked = end;

  *checked_out = checked;
  *last_saw_cr_out = last_saw_cr;
  return -1;
}
		  

/**
 * g_data_input_stream_read_line:
 * @stream: a given #GDataInputStream.
 * @length: (out) (optional): a #gsize to get the length of the data read in.
 * @cancellable: (nullable): optional #GCancellable object, %NULL to ignore.
 * @error: #GError for error reporting.
 *
 * Reads a line from the data input stream.  Note that no encoding
 * checks or conversion is performed; the input is not guaranteed to
 * be UTF-8, and may in fact have embedded NUL characters.
 *
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned.
 *
 * Returns: (nullable) (transfer full) (array zero-terminated=1) (element-type guint8):
 *  a NUL terminated byte array with the line that was read in
 *  (without the newlines).  Set @length to a #gsize to get the length
 *  of the read line.  On an error, it will return %NULL and @error
 *  will be set. If there's no content to read, it will still return
 *  %NULL, but @error won't be set.
 **/
char *
g_data_input_stream_read_line (GDataInputStream  *stream,
			       gsize             *length,
			       GCancellable      *cancellable,
			       GError           **error)
{
  GBufferedInputStream *bstream;
  gsize checked;
  gboolean last_saw_cr;
  gssize found_pos;
  gssize res;
  int newline_len;
  char *line;
  
  g_return_val_if_fail (G_IS_DATA_INPUT_STREAM (stream), NULL);  

  bstream = G_BUFFERED_INPUT_STREAM (stream);

  newline_len = 0;
  checked = 0;
  last_saw_cr = FALSE;

  while ((found_pos = scan_for_newline (stream, &checked, &last_saw_cr, &newline_len)) == -1)
    {
      if (g_buffered_input_stream_get_available (bstream) ==
	  g_buffered_input_stream_get_buffer_size (bstream))
	g_buffered_input_stream_set_buffer_size (bstream,
						 2 * g_buffered_input_stream_get_buffer_size (bstream));

      res = g_buffered_input_stream_fill (bstream, -1, cancellable, error);
      if (res < 0)
	return NULL;
      if (res == 0)
	{
	  /* End of stream */
	  if (g_buffered_input_stream_get_available (bstream) == 0)
	    {
	      if (length)
		*length = 0;
	      return NULL;
	    }
	  else
	    {
	      found_pos = checked;
	      newline_len = 0;
	      break;
	    }
	}
    }

  line = g_malloc (found_pos + newline_len + 1);

  res = g_input_stream_read (G_INPUT_STREAM (stream),
			     line,
			     found_pos + newline_len,
			     NULL, NULL);
  if (length)
    *length = (gsize)found_pos;
  g_warn_if_fail (res == found_pos + newline_len);
  line[found_pos] = 0;
  
  return line;
}

/**
 * g_data_input_stream_read_line_utf8:
 * @stream: a given #GDataInputStream.
 * @length: (out) (optional): a #gsize to get the length of the data read in.
 * @cancellable: (nullable): optional #GCancellable object, %NULL to ignore.
 * @error: #GError for error reporting.
 *
 * Reads a UTF-8 encoded line from the data input stream.
 *
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned.
 *
 * Returns: (nullable) (transfer full): a NUL terminated UTF-8 string
 *  with the line that was read in (without the newlines).  Set
 *  @length to a #gsize to get the length of the read line.  On an
 *  error, it will return %NULL and @error will be set.  For UTF-8
 *  conversion errors, the set error domain is %G_CONVERT_ERROR.  If
 *  there's no content to read, it will still return %NULL, but @error
 *  won't be set.
 *
 * Since: 2.30
 **/
char *
g_data_input_stream_read_line_utf8 (GDataInputStream  *stream,
				    gsize             *length,
				    GCancellable      *cancellable,
				    GError           **error)
{
  char *res;

  res = g_data_input_stream_read_line (stream, length, cancellable, error);
  if (!res)
    return NULL;
  
  if (!g_utf8_validate (res, -1, NULL))
    {
      g_set_error_literal (error, G_CONVERT_ERROR,
			   G_CONVERT_ERROR_ILLEGAL_SEQUENCE,
			   _("Invalid byte sequence in conversion input"));
      g_free (res);
      return NULL;
    }
  return res;
}

static gssize
scan_for_chars (GDataInputStream *stream,
		gsize            *checked_out,
		const char       *stop_chars,
                gssize            stop_chars_len)
{
  GBufferedInputStream *bstream;
  const char *buffer;
  gsize start, end, peeked;
  int i;
  gsize available, checked;
  const char *stop_char;
  const char *stop_end;

  bstream = G_BUFFERED_INPUT_STREAM (stream);
  stop_end = stop_chars + stop_chars_len;

  checked = *checked_out;

  start = checked;
  buffer = (const char *)g_buffered_input_stream_peek_buffer (bstream, &available) + start;
  end = available;
  peeked = end - start;

  for (i = 0; checked < available && i < peeked; i++)
    {
      for (stop_char = stop_chars; stop_char != stop_end; stop_char++)
	{
	  if (buffer[i] == *stop_char)
	    return (start + i);
	}
    }

  checked = end;

  *checked_out = checked;
  return -1;
}

/**
 * g_data_input_stream_read_until:
 * @stream: a given #GDataInputStream.
 * @stop_chars: characters to terminate the read.
 * @length: (out) (optional): a #gsize to get the length of the data read in.
 * @cancellable: (nullable): optional #GCancellable object, %NULL to ignore.
 * @error: #GError for error reporting.
 *
 * Reads a string from the data input stream, up to the first
 * occurrence of any of the stop characters.
 *
 * Note that, in contrast to g_data_input_stream_read_until_async(),
 * this function consumes the stop character that it finds.
 *
 * Don't use this function in new code.  Its functionality is
 * inconsistent with g_data_input_stream_read_until_async().  Both
 * functions will be marked as deprecated in a future release.  Use
 * g_data_input_stream_read_upto() instead, but note that that function
 * does not consume the stop character.
 *
 * Returns: (transfer full): a string with the data that was read
 *     before encountering any of the stop characters. Set @length to
 *     a #gsize to get the length of the string. This function will
 *     return %NULL on an error.
 * Deprecated: 2.56: Use g_data_input_stream_read_upto() instead, which has more
 *     consistent behaviour regarding the stop character.
 */
char *
g_data_input_stream_read_until (GDataInputStream  *stream,
			       const gchar        *stop_chars,
			       gsize              *length,
			       GCancellable       *cancellable,
			       GError            **error)
{
  GBufferedInputStream *bstream;
  gchar *result;

  bstream = G_BUFFERED_INPUT_STREAM (stream);

  result = g_data_input_stream_read_upto (stream, stop_chars, -1,
                                          length, cancellable, error);

  /* If we're not at end of stream then we have a stop_char to consume. */
  if (result != NULL && g_buffered_input_stream_get_available (bstream) > 0)
    {
      gsize res;
      gchar b;

      res = g_input_stream_read (G_INPUT_STREAM (stream), &b, 1, NULL, NULL);
      g_assert (res == 1);
    }

  return result;
}

typedef struct
{
  gboolean last_saw_cr;
  gsize checked;

  gchar *stop_chars;
  gssize stop_chars_len;
  gsize length;
} GDataInputStreamReadData;

static void
g_data_input_stream_read_complete (GTask *task,
                                   gsize  read_length,
                                   gsize  skip_length)
{
  GDataInputStreamReadData *data = g_task_get_task_data (task);
  GInputStream *stream = g_task_get_source_object (task);
  char *line = NULL;

  if (read_length || skip_length)
    {
      gssize bytes;

      data->length = read_length;
      line = g_malloc (read_length + 1);
      line[read_length] = '\0';

      /* we already checked the buffer.  this shouldn't fail. */
      bytes = g_input_stream_read (stream, line, read_length, NULL, NULL);
      g_assert_cmpint (bytes, ==, read_length);

      bytes = g_input_stream_skip (stream, skip_length, NULL, NULL);
      g_assert_cmpint (bytes, ==, skip_length);
    }

  g_task_return_pointer (task, line, g_free);
  g_object_unref (task);
}

static void
g_data_input_stream_read_line_ready (GObject      *object,
                                     GAsyncResult *result,
                                     gpointer      user_data)
{
  GTask *task = user_data;
  GDataInputStreamReadData *data = g_task_get_task_data (task);
  GBufferedInputStream *buffer = g_task_get_source_object (task);
  gssize found_pos;
  gint newline_len;

  if (result)
    /* this is a callback.  finish the async call. */
    {
      GError *error = NULL;
      gssize bytes;

      bytes = g_buffered_input_stream_fill_finish (buffer, result, &error);

      if (bytes <= 0)
        {
          if (bytes < 0)
            /* stream error. */
            {
              g_task_return_error (task, error);
              g_object_unref (task);
              return;
            }

          g_data_input_stream_read_complete (task, data->checked, 0);
          return;
        }

      /* only proceed if we got more bytes... */
    }

  if (data->stop_chars)
    {
      found_pos = scan_for_chars (G_DATA_INPUT_STREAM (buffer),
                                  &data->checked,
                                  data->stop_chars,
                                  data->stop_chars_len);
      newline_len = 0;
    }
  else
    found_pos = scan_for_newline (G_DATA_INPUT_STREAM (buffer), &data->checked,
                                  &data->last_saw_cr, &newline_len);

  if (found_pos == -1)
    /* didn't find a full line; need to buffer some more bytes */
    {
      gsize size;

      size = g_buffered_input_stream_get_buffer_size (buffer);

      if (g_buffered_input_stream_get_available (buffer) == size)
        /* need to grow the buffer */
        g_buffered_input_stream_set_buffer_size (buffer, size * 2);

      /* try again */
      g_buffered_input_stream_fill_async (buffer, -1,
                                          g_task_get_priority (task),
                                          g_task_get_cancellable (task),
                                          g_data_input_stream_read_line_ready,
                                          user_data);
    }
  else
    {
      /* read the line and the EOL.  no error is possible. */
      g_data_input_stream_read_complete (task, found_pos, newline_len);
    }
}

static void
g_data_input_stream_read_data_free (gpointer user_data)
{
  GDataInputStreamReadData *data = user_data;

  g_free (data->stop_chars);
  g_slice_free (GDataInputStreamReadData, data);
}

static void
g_data_input_stream_read_async (GDataInputStream    *stream,
                                const gchar         *stop_chars,
                                gssize               stop_chars_len,
                                gint                 io_priority,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  GDataInputStreamReadData *data;
  GTask *task;

  data = g_slice_new0 (GDataInputStreamReadData);
  if (stop_chars_len == -1)
    stop_chars_len = strlen (stop_chars);
  data->stop_chars = g_memdup (stop_chars, stop_chars_len);
  data->stop_chars_len = stop_chars_len;
  data->last_saw_cr = FALSE;

  task = g_task_new (stream, cancellable, callback, user_data);
  g_task_set_source_tag (task, g_data_input_stream_read_async);
  g_task_set_task_data (task, data, g_data_input_stream_read_data_free);
  g_task_set_priority (task, io_priority);

  g_data_input_stream_read_line_ready (NULL, NULL, task);
}

static gchar *
g_data_input_stream_read_finish (GDataInputStream  *stream,
                                 GAsyncResult      *result,
                                 gsize             *length,
                                 GError           **error)
{
  GTask *task = G_TASK (result);
  gchar *line;

  line = g_task_propagate_pointer (task, error);

  if (length && line)
    {
      GDataInputStreamReadData *data = g_task_get_task_data (task);

      *length = data->length;
    }

  return line;
}

/**
 * g_data_input_stream_read_line_async:
 * @stream: a given #GDataInputStream.
 * @io_priority: the [I/O priority][io-priority] of the request
 * @cancellable: (nullable): optional #GCancellable object, %NULL to ignore.
 * @callback: (scope async): callback to call when the request is satisfied.
 * @user_data: (closure): the data to pass to callback function.
 *
 * The asynchronous version of g_data_input_stream_read_line().  It is
 * an error to have two outstanding calls to this function.
 *
 * When the operation is finished, @callback will be called. You
 * can then call g_data_input_stream_read_line_finish() to get
 * the result of the operation.
 *
 * Since: 2.20
 */
void
g_data_input_stream_read_line_async (GDataInputStream    *stream,
                                     gint                 io_priority,
                                     GCancellable        *cancellable,
                                     GAsyncReadyCallback  callback,
                                     gpointer             user_data)
{
  g_return_if_fail (G_IS_DATA_INPUT_STREAM (stream));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  g_data_input_stream_read_async (stream, NULL, 0, io_priority,
                                  cancellable, callback, user_data);
}

/**
 * g_data_input_stream_read_until_async:
 * @stream: a given #GDataInputStream.
 * @stop_chars: characters to terminate the read.
 * @io_priority: the [I/O priority][io-priority] of the request
 * @cancellable: (nullable): optional #GCancellable object, %NULL to ignore.
 * @callback: (scope async): callback to call when the request is satisfied.
 * @user_data: (closure): the data to pass to callback function.
 *
 * The asynchronous version of g_data_input_stream_read_until().
 * It is an error to have two outstanding calls to this function.
 *
 * Note that, in contrast to g_data_input_stream_read_until(),
 * this function does not consume the stop character that it finds.  You
 * must read it for yourself.
 *
 * When the operation is finished, @callback will be called. You
 * can then call g_data_input_stream_read_until_finish() to get
 * the result of the operation.
 *
 * Don't use this function in new code.  Its functionality is
 * inconsistent with g_data_input_stream_read_until().  Both functions
 * will be marked as deprecated in a future release.  Use
 * g_data_input_stream_read_upto_async() instead.
 *
 * Since: 2.20
 * Deprecated: 2.56: Use g_data_input_stream_read_upto_async() instead, which
 *     has more consistent behaviour regarding the stop character.
 */
void
g_data_input_stream_read_until_async (GDataInputStream    *stream,
                                      const gchar         *stop_chars,
                                      gint                 io_priority,
                                      GCancellable        *cancellable,
                                      GAsyncReadyCallback  callback,
                                      gpointer             user_data)
{
  g_return_if_fail (G_IS_DATA_INPUT_STREAM (stream));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (stop_chars != NULL);

  g_data_input_stream_read_async (stream, stop_chars, -1, io_priority,
                                  cancellable, callback, user_data);
}

/**
 * g_data_input_stream_read_line_finish:
 * @stream: a given #GDataInputStream.
 * @result: the #GAsyncResult that was provided to the callback.
 * @length: (out) (optional): a #gsize to get the length of the data read in.
 * @error: #GError for error reporting.
 *
 * Finish an asynchronous call started by
 * g_data_input_stream_read_line_async().  Note the warning about
 * string encoding in g_data_input_stream_read_line() applies here as
 * well.
 *
 * Returns: (nullable) (transfer full) (array zero-terminated=1) (element-type guint8):
 *  a NUL-terminated byte array with the line that was read in
 *  (without the newlines).  Set @length to a #gsize to get the length
 *  of the read line.  On an error, it will return %NULL and @error
 *  will be set. If there's no content to read, it will still return
 *  %NULL, but @error won't be set.
 *
 * Since: 2.20
 */
gchar *
g_data_input_stream_read_line_finish (GDataInputStream  *stream,
                                      GAsyncResult      *result,
                                      gsize             *length,
                                      GError           **error)
{
  g_return_val_if_fail (g_task_is_valid (result, stream), NULL);

  return g_data_input_stream_read_finish (stream, result, length, error);
}

/**
 * g_data_input_stream_read_line_finish_utf8:
 * @stream: a given #GDataInputStream.
 * @result: the #GAsyncResult that was provided to the callback.
 * @length: (out) (optional): a #gsize to get the length of the data read in.
 * @error: #GError for error reporting.
 *
 * Finish an asynchronous call started by
 * g_data_input_stream_read_line_async().
 *
 * Returns: (nullable) (transfer full): a string with the line that
 *  was read in (without the newlines).  Set @length to a #gsize to
 *  get the length of the read line.  On an error, it will return
 *  %NULL and @error will be set. For UTF-8 conversion errors, the set
 *  error domain is %G_CONVERT_ERROR.  If there's no content to read,
 *  it will still return %NULL, but @error won't be set.
 *
 * Since: 2.30
 */
gchar *
g_data_input_stream_read_line_finish_utf8 (GDataInputStream  *stream,
					   GAsyncResult      *result,
					   gsize             *length,
					   GError           **error)
{
  gchar *res;

  res = g_data_input_stream_read_line_finish (stream, result, length, error);
  if (!res)
    return NULL;

  if (!g_utf8_validate (res, -1, NULL))
    {
      g_set_error_literal (error, G_CONVERT_ERROR,
			   G_CONVERT_ERROR_ILLEGAL_SEQUENCE,
			   _("Invalid byte sequence in conversion input"));
      g_free (res);
      return NULL;
    }
  return res;
}

/**
 * g_data_input_stream_read_until_finish:
 * @stream: a given #GDataInputStream.
 * @result: the #GAsyncResult that was provided to the callback.
 * @length: (out) (optional): a #gsize to get the length of the data read in.
 * @error: #GError for error reporting.
 *
 * Finish an asynchronous call started by
 * g_data_input_stream_read_until_async().
 *
 * Since: 2.20
 *
 * Returns: (transfer full): a string with the data that was read
 *     before encountering any of the stop characters. Set @length to
 *     a #gsize to get the length of the string. This function will
 *     return %NULL on an error.
 * Deprecated: 2.56: Use g_data_input_stream_read_upto_finish() instead, which
 *     has more consistent behaviour regarding the stop character.
 */
gchar *
g_data_input_stream_read_until_finish (GDataInputStream  *stream,
                                       GAsyncResult      *result,
                                       gsize             *length,
                                       GError           **error)
{
  g_return_val_if_fail (g_task_is_valid (result, stream), NULL);

  return g_data_input_stream_read_finish (stream, result, length, error);
}

/**
 * g_data_input_stream_read_upto:
 * @stream: a #GDataInputStream
 * @stop_chars: characters to terminate the read
 * @stop_chars_len: length of @stop_chars. May be -1 if @stop_chars is
 *     nul-terminated
 * @length: (out) (optional): a #gsize to get the length of the data read in
 * @cancellable: (nullable): optional #GCancellable object, %NULL to ignore
 * @error: #GError for error reporting
 *
 * Reads a string from the data input stream, up to the first
 * occurrence of any of the stop characters.
 *
 * In contrast to g_data_input_stream_read_until(), this function
 * does not consume the stop character. You have to use
 * g_data_input_stream_read_byte() to get it before calling
 * g_data_input_stream_read_upto() again.
 *
 * Note that @stop_chars may contain '\0' if @stop_chars_len is
 * specified.
 *
 * The returned string will always be nul-terminated on success.
 *
 * Returns: (transfer full): a string with the data that was read
 *     before encountering any of the stop characters. Set @length to
 *     a #gsize to get the length of the string. This function will
 *     return %NULL on an error
 *
 * Since: 2.26
 */
char *
g_data_input_stream_read_upto (GDataInputStream  *stream,
                               const gchar       *stop_chars,
                               gssize             stop_chars_len,
                               gsize             *length,
                               GCancellable      *cancellable,
                               GError           **error)
{
  GBufferedInputStream *bstream;
  gsize checked;
  gssize found_pos;
  gssize res;
  char *data_until;

  g_return_val_if_fail (G_IS_DATA_INPUT_STREAM (stream), NULL);

  if (stop_chars_len < 0)
    stop_chars_len = strlen (stop_chars);

  bstream = G_BUFFERED_INPUT_STREAM (stream);

  checked = 0;

  while ((found_pos = scan_for_chars (stream, &checked, stop_chars, stop_chars_len)) == -1)
    {
      if (g_buffered_input_stream_get_available (bstream) ==
          g_buffered_input_stream_get_buffer_size (bstream))
        g_buffered_input_stream_set_buffer_size (bstream,
                                                 2 * g_buffered_input_stream_get_buffer_size (bstream));

      res = g_buffered_input_stream_fill (bstream, -1, cancellable, error);
      if (res < 0)
        return NULL;
      if (res == 0)
        {
          /* End of stream */
          if (g_buffered_input_stream_get_available (bstream) == 0)
            {
              if (length)
                *length = 0;
              return NULL;
            }
          else
            {
              found_pos = checked;
              break;
            }
        }
    }

  data_until = g_malloc (found_pos + 1);

  res = g_input_stream_read (G_INPUT_STREAM (stream),
                             data_until,
                             found_pos,
                             NULL, NULL);
  if (length)
    *length = (gsize)found_pos;
  g_warn_if_fail (res == found_pos);
  data_until[found_pos] = 0;

  return data_until;
}

/**
 * g_data_input_stream_read_upto_async:
 * @stream: a #GDataInputStream
 * @stop_chars: characters to terminate the read
 * @stop_chars_len: length of @stop_chars. May be -1 if @stop_chars is
 *     nul-terminated
 * @io_priority: the [I/O priority][io-priority] of the request
 * @cancellable: (nullable): optional #GCancellable object, %NULL to ignore
 * @callback: (scope async): callback to call when the request is satisfied
 * @user_data: (closure): the data to pass to callback function
 *
 * The asynchronous version of g_data_input_stream_read_upto().
 * It is an error to have two outstanding calls to this function.
 *
 * In contrast to g_data_input_stream_read_until(), this function
 * does not consume the stop character. You have to use
 * g_data_input_stream_read_byte() to get it before calling
 * g_data_input_stream_read_upto() again.
 *
 * Note that @stop_chars may contain '\0' if @stop_chars_len is
 * specified.
 *
 * When the operation is finished, @callback will be called. You
 * can then call g_data_input_stream_read_upto_finish() to get
 * the result of the operation.
 *
 * Since: 2.26
 */
void
g_data_input_stream_read_upto_async (GDataInputStream    *stream,
                                     const gchar         *stop_chars,
                                     gssize               stop_chars_len,
                                     gint                 io_priority,
                                     GCancellable        *cancellable,
                                     GAsyncReadyCallback  callback,
                                     gpointer             user_data)
{
  g_return_if_fail (G_IS_DATA_INPUT_STREAM (stream));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (stop_chars != NULL);

  g_data_input_stream_read_async (stream, stop_chars, stop_chars_len, io_priority,
                                  cancellable, callback, user_data);
}

/**
 * g_data_input_stream_read_upto_finish:
 * @stream: a #GDataInputStream
 * @result: the #GAsyncResult that was provided to the callback
 * @length: (out) (optional): a #gsize to get the length of the data read in
 * @error: #GError for error reporting
 *
 * Finish an asynchronous call started by
 * g_data_input_stream_read_upto_async().
 *
 * Note that this function does not consume the stop character. You
 * have to use g_data_input_stream_read_byte() to get it before calling
 * g_data_input_stream_read_upto_async() again.
 *
 * The returned string will always be nul-terminated on success.
 *
 * Returns: (transfer full): a string with the data that was read
 *     before encountering any of the stop characters. Set @length to
 *     a #gsize to get the length of the string. This function will
 *     return %NULL on an error.
 *
 * Since: 2.24
 */
gchar *
g_data_input_stream_read_upto_finish (GDataInputStream  *stream,
                                      GAsyncResult      *result,
                                      gsize             *length,
                                      GError           **error)
{
  g_return_val_if_fail (g_task_is_valid (result, stream), NULL);

  return g_data_input_stream_read_finish (stream, result, length, error);
}
