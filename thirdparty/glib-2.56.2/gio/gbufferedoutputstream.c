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
 * Author: Christian Kellner <gicmo@gnome.org> 
 */

#include "config.h"
#include "gbufferedoutputstream.h"
#include "goutputstream.h"
#include "gseekable.h"
#include "gtask.h"
#include "string.h"
#include "gioerror.h"
#include "glibintl.h"

/**
 * SECTION:gbufferedoutputstream
 * @short_description: Buffered Output Stream
 * @include: gio/gio.h
 * @see_also: #GFilterOutputStream, #GOutputStream
 * 
 * Buffered output stream implements #GFilterOutputStream and provides 
 * for buffered writes. 
 * 
 * By default, #GBufferedOutputStream's buffer size is set at 4 kilobytes.
 * 
 * To create a buffered output stream, use g_buffered_output_stream_new(), 
 * or g_buffered_output_stream_new_sized() to specify the buffer's size 
 * at construction.
 * 
 * To get the size of a buffer within a buffered input stream, use 
 * g_buffered_output_stream_get_buffer_size(). To change the size of a 
 * buffered output stream's buffer, use 
 * g_buffered_output_stream_set_buffer_size(). Note that the buffer's 
 * size cannot be reduced below the size of the data within the buffer.
 **/

#define DEFAULT_BUFFER_SIZE 4096

struct _GBufferedOutputStreamPrivate {
  guint8 *buffer; 
  gsize   len;
  goffset pos;
  gboolean auto_grow;
};

enum {
  PROP_0,
  PROP_BUFSIZE,
  PROP_AUTO_GROW
};

static void     g_buffered_output_stream_set_property (GObject      *object,
                                                       guint         prop_id,
                                                       const GValue *value,
                                                       GParamSpec   *pspec);

static void     g_buffered_output_stream_get_property (GObject    *object,
                                                       guint       prop_id,
                                                       GValue     *value,
                                                       GParamSpec *pspec);
static void     g_buffered_output_stream_finalize     (GObject *object);


static gssize   g_buffered_output_stream_write        (GOutputStream *stream,
                                                       const void    *buffer,
                                                       gsize          count,
                                                       GCancellable  *cancellable,
                                                       GError       **error);
static gboolean g_buffered_output_stream_flush        (GOutputStream    *stream,
                                                       GCancellable  *cancellable,
                                                       GError          **error);
static gboolean g_buffered_output_stream_close        (GOutputStream  *stream,
                                                       GCancellable   *cancellable,
                                                       GError        **error);

static void     g_buffered_output_stream_flush_async  (GOutputStream        *stream,
                                                       int                   io_priority,
                                                       GCancellable         *cancellable,
                                                       GAsyncReadyCallback   callback,
                                                       gpointer              data);
static gboolean g_buffered_output_stream_flush_finish (GOutputStream        *stream,
                                                       GAsyncResult         *result,
                                                       GError              **error);
static void     g_buffered_output_stream_close_async  (GOutputStream        *stream,
                                                       int                   io_priority,
                                                       GCancellable         *cancellable,
                                                       GAsyncReadyCallback   callback,
                                                       gpointer              data);
static gboolean g_buffered_output_stream_close_finish (GOutputStream        *stream,
                                                       GAsyncResult         *result,
                                                       GError              **error);

static void     g_buffered_output_stream_seekable_iface_init (GSeekableIface  *iface);
static goffset  g_buffered_output_stream_tell                (GSeekable       *seekable);
static gboolean g_buffered_output_stream_can_seek            (GSeekable       *seekable);
static gboolean g_buffered_output_stream_seek                (GSeekable       *seekable,
							      goffset          offset,
							      GSeekType        type,
							      GCancellable    *cancellable,
							      GError         **error);
static gboolean g_buffered_output_stream_can_truncate        (GSeekable       *seekable);
static gboolean g_buffered_output_stream_truncate            (GSeekable       *seekable,
							      goffset          offset,
							      GCancellable    *cancellable,
							      GError         **error);

G_DEFINE_TYPE_WITH_CODE (GBufferedOutputStream,
			 g_buffered_output_stream,
			 G_TYPE_FILTER_OUTPUT_STREAM,
                         G_ADD_PRIVATE (GBufferedOutputStream)
			 G_IMPLEMENT_INTERFACE (G_TYPE_SEEKABLE,
						g_buffered_output_stream_seekable_iface_init))


static void
g_buffered_output_stream_class_init (GBufferedOutputStreamClass *klass)
{
  GObjectClass *object_class;
  GOutputStreamClass *ostream_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->get_property = g_buffered_output_stream_get_property;
  object_class->set_property = g_buffered_output_stream_set_property;
  object_class->finalize     = g_buffered_output_stream_finalize;

  ostream_class = G_OUTPUT_STREAM_CLASS (klass);
  ostream_class->write_fn = g_buffered_output_stream_write;
  ostream_class->flush = g_buffered_output_stream_flush;
  ostream_class->close_fn = g_buffered_output_stream_close;
  ostream_class->flush_async  = g_buffered_output_stream_flush_async;
  ostream_class->flush_finish = g_buffered_output_stream_flush_finish;
  ostream_class->close_async  = g_buffered_output_stream_close_async;
  ostream_class->close_finish = g_buffered_output_stream_close_finish;

  g_object_class_install_property (object_class,
                                   PROP_BUFSIZE,
                                   g_param_spec_uint ("buffer-size",
                                                      P_("Buffer Size"),
                                                      P_("The size of the backend buffer"),
                                                      1,
                                                      G_MAXUINT,
                                                      DEFAULT_BUFFER_SIZE,
                                                      G_PARAM_READWRITE|G_PARAM_CONSTRUCT|
                                                      G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB));

  g_object_class_install_property (object_class,
                                   PROP_AUTO_GROW,
                                   g_param_spec_boolean ("auto-grow",
                                                         P_("Auto-grow"),
                                                         P_("Whether the buffer should automatically grow"),
                                                         FALSE,
                                                         G_PARAM_READWRITE|
                                                         G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB));

}

/**
 * g_buffered_output_stream_get_buffer_size:
 * @stream: a #GBufferedOutputStream.
 * 
 * Gets the size of the buffer in the @stream.
 * 
 * Returns: the current size of the buffer.
 **/
gsize
g_buffered_output_stream_get_buffer_size (GBufferedOutputStream *stream)
{
  g_return_val_if_fail (G_IS_BUFFERED_OUTPUT_STREAM (stream), -1);

  return stream->priv->len;
}

/**
 * g_buffered_output_stream_set_buffer_size:
 * @stream: a #GBufferedOutputStream.
 * @size: a #gsize.
 *
 * Sets the size of the internal buffer to @size.
 **/    
void
g_buffered_output_stream_set_buffer_size (GBufferedOutputStream *stream,
                                          gsize                  size)
{
  GBufferedOutputStreamPrivate *priv;
  guint8 *buffer;
  
  g_return_if_fail (G_IS_BUFFERED_OUTPUT_STREAM (stream));

  priv = stream->priv;
  
  if (size == priv->len)
    return;

  if (priv->buffer)
    {
      size = MAX (size, priv->pos);

      buffer = g_malloc (size);
      memcpy (buffer, priv->buffer, priv->pos);
      g_free (priv->buffer);
      priv->buffer = buffer;
      priv->len = size;
      /* Keep old pos */
    }
  else
    {
      priv->buffer = g_malloc (size);
      priv->len = size;
      priv->pos = 0;
    }

  g_object_notify (G_OBJECT (stream), "buffer-size");
}

/**
 * g_buffered_output_stream_get_auto_grow:
 * @stream: a #GBufferedOutputStream.
 * 
 * Checks if the buffer automatically grows as data is added.
 * 
 * Returns: %TRUE if the @stream's buffer automatically grows,
 * %FALSE otherwise.
 **/  
gboolean
g_buffered_output_stream_get_auto_grow (GBufferedOutputStream *stream)
{
  g_return_val_if_fail (G_IS_BUFFERED_OUTPUT_STREAM (stream), FALSE);

  return stream->priv->auto_grow;
}

/**
 * g_buffered_output_stream_set_auto_grow:
 * @stream: a #GBufferedOutputStream.
 * @auto_grow: a #gboolean.
 *
 * Sets whether or not the @stream's buffer should automatically grow.
 * If @auto_grow is true, then each write will just make the buffer
 * larger, and you must manually flush the buffer to actually write out
 * the data to the underlying stream.
 **/
void
g_buffered_output_stream_set_auto_grow (GBufferedOutputStream *stream,
                                        gboolean               auto_grow)
{
  GBufferedOutputStreamPrivate *priv;
  g_return_if_fail (G_IS_BUFFERED_OUTPUT_STREAM (stream));
  priv = stream->priv;
  auto_grow = auto_grow != FALSE;
  if (priv->auto_grow != auto_grow)
    {
      priv->auto_grow = auto_grow;
      g_object_notify (G_OBJECT (stream), "auto-grow");
    }
}

static void
g_buffered_output_stream_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  GBufferedOutputStream *stream;

  stream = G_BUFFERED_OUTPUT_STREAM (object);

  switch (prop_id) 
    {
    case PROP_BUFSIZE:
      g_buffered_output_stream_set_buffer_size (stream, g_value_get_uint (value));
      break;    

    case PROP_AUTO_GROW:
      g_buffered_output_stream_set_auto_grow (stream, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }

}

static void
g_buffered_output_stream_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  GBufferedOutputStream *buffered_stream;
  GBufferedOutputStreamPrivate *priv;

  buffered_stream = G_BUFFERED_OUTPUT_STREAM (object);
  priv = buffered_stream->priv;

  switch (prop_id)
    {
    case PROP_BUFSIZE:
      g_value_set_uint (value, priv->len);
      break;

    case PROP_AUTO_GROW:
      g_value_set_boolean (value, priv->auto_grow);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }

}

static void
g_buffered_output_stream_finalize (GObject *object)
{
  GBufferedOutputStream *stream;
  GBufferedOutputStreamPrivate *priv;

  stream = G_BUFFERED_OUTPUT_STREAM (object);
  priv = stream->priv;

  g_free (priv->buffer);

  G_OBJECT_CLASS (g_buffered_output_stream_parent_class)->finalize (object);
}

static void
g_buffered_output_stream_init (GBufferedOutputStream *stream)
{
  stream->priv = g_buffered_output_stream_get_instance_private (stream);
}

static void
g_buffered_output_stream_seekable_iface_init (GSeekableIface *iface)
{
  iface->tell         = g_buffered_output_stream_tell;
  iface->can_seek     = g_buffered_output_stream_can_seek;
  iface->seek         = g_buffered_output_stream_seek;
  iface->can_truncate = g_buffered_output_stream_can_truncate;
  iface->truncate_fn  = g_buffered_output_stream_truncate;
}

/**
 * g_buffered_output_stream_new:
 * @base_stream: a #GOutputStream.
 * 
 * Creates a new buffered output stream for a base stream.
 * 
 * Returns: a #GOutputStream for the given @base_stream.
 **/  
GOutputStream *
g_buffered_output_stream_new (GOutputStream *base_stream)
{
  GOutputStream *stream;

  g_return_val_if_fail (G_IS_OUTPUT_STREAM (base_stream), NULL);

  stream = g_object_new (G_TYPE_BUFFERED_OUTPUT_STREAM,
                         "base-stream", base_stream,
                         NULL);

  return stream;
}

/**
 * g_buffered_output_stream_new_sized:
 * @base_stream: a #GOutputStream.
 * @size: a #gsize.
 * 
 * Creates a new buffered output stream with a given buffer size.
 * 
 * Returns: a #GOutputStream with an internal buffer set to @size.
 **/  
GOutputStream *
g_buffered_output_stream_new_sized (GOutputStream *base_stream,
                                    gsize          size)
{
  GOutputStream *stream;

  g_return_val_if_fail (G_IS_OUTPUT_STREAM (base_stream), NULL);

  stream = g_object_new (G_TYPE_BUFFERED_OUTPUT_STREAM,
                         "base-stream", base_stream,
                         "buffer-size", size,
                         NULL);

  return stream;
}

static gboolean
flush_buffer (GBufferedOutputStream  *stream,
              GCancellable           *cancellable,
              GError                 **error)
{
  GBufferedOutputStreamPrivate *priv;
  GOutputStream                *base_stream;
  gboolean                      res;
  gsize                         bytes_written;
  gsize                         count;

  priv = stream->priv;
  bytes_written = 0;
  base_stream = G_FILTER_OUTPUT_STREAM (stream)->base_stream;

  g_return_val_if_fail (G_IS_OUTPUT_STREAM (base_stream), FALSE);

  res = g_output_stream_write_all (base_stream,
                                   priv->buffer,
                                   priv->pos,
                                   &bytes_written,
                                   cancellable,
                                   error);

  count = priv->pos - bytes_written;

  if (count > 0)
    memmove (priv->buffer, priv->buffer + bytes_written, count);
  
  priv->pos -= bytes_written;

  return res;
}

static gssize
g_buffered_output_stream_write  (GOutputStream *stream,
                                 const void    *buffer,
                                 gsize          count,
                                 GCancellable  *cancellable,
                                 GError       **error)
{
  GBufferedOutputStream        *bstream;
  GBufferedOutputStreamPrivate *priv;
  gboolean res;
  gsize    n;
  gsize new_size;

  bstream = G_BUFFERED_OUTPUT_STREAM (stream);
  priv = bstream->priv;

  n = priv->len - priv->pos;

  if (priv->auto_grow && n < count)
    {
      new_size = MAX (priv->len * 2, priv->len + count);
      g_buffered_output_stream_set_buffer_size (bstream, new_size);
    }
  else if (n == 0)
    {
      res = flush_buffer (bstream, cancellable, error);
      
      if (res == FALSE)
	return -1;
    }

  n = priv->len - priv->pos;
  
  count = MIN (count, n);
  memcpy (priv->buffer + priv->pos, buffer, count);
  priv->pos += count;

  return count;
}

static gboolean
g_buffered_output_stream_flush (GOutputStream  *stream,
                                GCancellable   *cancellable,
                                GError        **error)
{
  GBufferedOutputStream *bstream;
  GOutputStream                *base_stream;
  gboolean res;

  bstream = G_BUFFERED_OUTPUT_STREAM (stream);
  base_stream = G_FILTER_OUTPUT_STREAM (stream)->base_stream;

  res = flush_buffer (bstream, cancellable, error);

  if (res == FALSE)
    return FALSE;

  res = g_output_stream_flush (base_stream, cancellable, error);

  return res;
}

static gboolean
g_buffered_output_stream_close (GOutputStream  *stream,
                                GCancellable   *cancellable,
                                GError        **error)
{
  GBufferedOutputStream        *bstream;
  GOutputStream                *base_stream;
  gboolean                      res;

  bstream = G_BUFFERED_OUTPUT_STREAM (stream);
  base_stream = G_FILTER_OUTPUT_STREAM (bstream)->base_stream;
  res = flush_buffer (bstream, cancellable, error);

  if (g_filter_output_stream_get_close_base_stream (G_FILTER_OUTPUT_STREAM (stream)))
    {
      /* report the first error but still close the stream */
      if (res)
        res = g_output_stream_close (base_stream, cancellable, error);
      else
        g_output_stream_close (base_stream, cancellable, NULL);
    }

  return res;
}

static goffset
g_buffered_output_stream_tell (GSeekable *seekable)
{
  GBufferedOutputStream        *bstream;
  GBufferedOutputStreamPrivate *priv;
  GOutputStream *base_stream;
  GSeekable    *base_stream_seekable;
  goffset base_offset;
  
  bstream = G_BUFFERED_OUTPUT_STREAM (seekable);
  priv = bstream->priv;

  base_stream = G_FILTER_OUTPUT_STREAM (seekable)->base_stream;
  if (!G_IS_SEEKABLE (base_stream))
    return 0;

  base_stream_seekable = G_SEEKABLE (base_stream);
  
  base_offset = g_seekable_tell (base_stream_seekable);
  return base_offset + priv->pos;
}

static gboolean
g_buffered_output_stream_can_seek (GSeekable *seekable)
{
  GOutputStream *base_stream;
  
  base_stream = G_FILTER_OUTPUT_STREAM (seekable)->base_stream;
  return G_IS_SEEKABLE (base_stream) && g_seekable_can_seek (G_SEEKABLE (base_stream));
}

static gboolean
g_buffered_output_stream_seek (GSeekable     *seekable,
			       goffset        offset,
			       GSeekType      type,
			       GCancellable  *cancellable,
			       GError       **error)
{
  GBufferedOutputStream *bstream;
  GOutputStream *base_stream;
  GSeekable *base_stream_seekable;
  gboolean flushed;

  bstream = G_BUFFERED_OUTPUT_STREAM (seekable);

  base_stream = G_FILTER_OUTPUT_STREAM (seekable)->base_stream;
  if (!G_IS_SEEKABLE (base_stream))
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                           _("Seek not supported on base stream"));
      return FALSE;
    }

  base_stream_seekable = G_SEEKABLE (base_stream);
  flushed = flush_buffer (bstream, cancellable, error);
  if (!flushed)
    return FALSE;

  return g_seekable_seek (base_stream_seekable, offset, type, cancellable, error);
}

static gboolean
g_buffered_output_stream_can_truncate (GSeekable *seekable)
{
  GOutputStream *base_stream;
  
  base_stream = G_FILTER_OUTPUT_STREAM (seekable)->base_stream;
  return G_IS_SEEKABLE (base_stream) && g_seekable_can_truncate (G_SEEKABLE (base_stream));
}

static gboolean
g_buffered_output_stream_truncate (GSeekable     *seekable,
				   goffset        offset,
				   GCancellable  *cancellable,
				   GError       **error)
{
  GBufferedOutputStream        *bstream;
  GOutputStream *base_stream;
  GSeekable *base_stream_seekable;
  gboolean flushed;

  bstream = G_BUFFERED_OUTPUT_STREAM (seekable);
  base_stream = G_FILTER_OUTPUT_STREAM (seekable)->base_stream;
  if (!G_IS_SEEKABLE (base_stream))
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                           _("Truncate not supported on base stream"));
      return FALSE;
    }

  base_stream_seekable = G_SEEKABLE (base_stream);

  flushed = flush_buffer (bstream, cancellable, error);
  if (!flushed)
    return FALSE;
  return g_seekable_truncate (base_stream_seekable, offset, cancellable, error);
}

/* ************************** */
/* Async stuff implementation */
/* ************************** */

/* TODO: This should be using the base class async ops, not threads */

typedef struct {

  guint flush_stream : 1;
  guint close_stream : 1;

} FlushData;

static void
free_flush_data (gpointer data)
{
  g_slice_free (FlushData, data);
}

/* This function is used by all three (i.e. 
 * _write, _flush, _close) functions since
 * all of them will need to flush the buffer
 * and so closing and writing is just a special
 * case of flushing + some addition stuff */
static void
flush_buffer_thread (GTask        *task,
                     gpointer      object,
                     gpointer      task_data,
                     GCancellable *cancellable)
{
  GBufferedOutputStream *stream;
  GOutputStream *base_stream;
  FlushData     *fdata;
  gboolean       res;
  GError        *error = NULL;

  stream = G_BUFFERED_OUTPUT_STREAM (object);
  fdata = task_data;
  base_stream = G_FILTER_OUTPUT_STREAM (stream)->base_stream;

  res = flush_buffer (stream, cancellable, &error);

  /* if flushing the buffer didn't work don't even bother
   * to flush the stream but just report that error */
  if (res && fdata->flush_stream)
    res = g_output_stream_flush (base_stream, cancellable, &error);

  if (fdata->close_stream) 
    {
     
      /* if flushing the buffer or the stream returned 
       * an error report that first error but still try 
       * close the stream */
      if (g_filter_output_stream_get_close_base_stream (G_FILTER_OUTPUT_STREAM (stream)))
        {
          if (res == FALSE)
            g_output_stream_close (base_stream, cancellable, NULL);
          else 
            res = g_output_stream_close (base_stream, cancellable, &error);
        }
    }

  if (res == FALSE)
    g_task_return_error (task, error);
  else
    g_task_return_boolean (task, TRUE);
}

static void
g_buffered_output_stream_flush_async (GOutputStream        *stream,
                                      int                   io_priority,
                                      GCancellable         *cancellable,
                                      GAsyncReadyCallback   callback,
                                      gpointer              data)
{
  GTask *task;
  FlushData *fdata;

  fdata = g_slice_new (FlushData);
  fdata->flush_stream = TRUE;
  fdata->close_stream = FALSE;

  task = g_task_new (stream, cancellable, callback, data);
  g_task_set_source_tag (task, g_buffered_output_stream_flush_async);
  g_task_set_task_data (task, fdata, free_flush_data);
  g_task_set_priority (task, io_priority);

  g_task_run_in_thread (task, flush_buffer_thread);
  g_object_unref (task);
}

static gboolean
g_buffered_output_stream_flush_finish (GOutputStream        *stream,
                                       GAsyncResult         *result,
                                       GError              **error)
{
  g_return_val_if_fail (g_task_is_valid (result, stream), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
g_buffered_output_stream_close_async (GOutputStream        *stream,
                                      int                   io_priority,
                                      GCancellable         *cancellable,
                                      GAsyncReadyCallback   callback,
                                      gpointer              data)
{
  GTask *task;
  FlushData *fdata;

  fdata = g_slice_new (FlushData);
  fdata->close_stream = TRUE;

  task = g_task_new (stream, cancellable, callback, data);
  g_task_set_source_tag (task, g_buffered_output_stream_close_async);
  g_task_set_task_data (task, fdata, free_flush_data);
  g_task_set_priority (task, io_priority);

  g_task_run_in_thread (task, flush_buffer_thread);
  g_object_unref (task);
}

static gboolean
g_buffered_output_stream_close_finish (GOutputStream        *stream,
                                       GAsyncResult         *result,
                                       GError              **error)
{
  g_return_val_if_fail (g_task_is_valid (result, stream), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}
