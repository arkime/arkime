/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright (C) 2006-2007 Red Hat, Inc.
 * Copyright (C) 2007 Jürg Billeter
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
#include "gbufferedinputstream.h"
#include "ginputstream.h"
#include "gcancellable.h"
#include "gasyncresult.h"
#include "gtask.h"
#include "gseekable.h"
#include "gioerror.h"
#include <string.h>
#include "glibintl.h"


/**
 * SECTION:gbufferedinputstream
 * @short_description: Buffered Input Stream
 * @include: gio/gio.h
 * @see_also: #GFilterInputStream, #GInputStream
 *
 * Buffered input stream implements #GFilterInputStream and provides
 * for buffered reads.
 *
 * By default, #GBufferedInputStream's buffer size is set at 4 kilobytes.
 *
 * To create a buffered input stream, use g_buffered_input_stream_new(),
 * or g_buffered_input_stream_new_sized() to specify the buffer's size at
 * construction.
 *
 * To get the size of a buffer within a buffered input stream, use
 * g_buffered_input_stream_get_buffer_size(). To change the size of a
 * buffered input stream's buffer, use
 * g_buffered_input_stream_set_buffer_size(). Note that the buffer's size
 * cannot be reduced below the size of the data within the buffer.
 */


#define DEFAULT_BUFFER_SIZE 4096

struct _GBufferedInputStreamPrivate {
  guint8 *buffer;
  gsize   len;
  gsize   pos;
  gsize   end;
  GAsyncReadyCallback outstanding_callback;
};

enum {
  PROP_0,
  PROP_BUFSIZE
};

static void g_buffered_input_stream_set_property  (GObject      *object,
                                                   guint         prop_id,
                                                   const GValue *value,
                                                   GParamSpec   *pspec);

static void g_buffered_input_stream_get_property  (GObject      *object,
                                                   guint         prop_id,
                                                   GValue       *value,
                                                   GParamSpec   *pspec);
static void g_buffered_input_stream_finalize      (GObject *object);


static gssize g_buffered_input_stream_skip             (GInputStream          *stream,
                                                        gsize                  count,
                                                        GCancellable          *cancellable,
                                                        GError               **error);
static void   g_buffered_input_stream_skip_async       (GInputStream          *stream,
                                                        gsize                  count,
                                                        int                    io_priority,
                                                        GCancellable          *cancellable,
                                                        GAsyncReadyCallback    callback,
                                                        gpointer               user_data);
static gssize g_buffered_input_stream_skip_finish      (GInputStream          *stream,
                                                        GAsyncResult          *result,
                                                        GError               **error);
static gssize g_buffered_input_stream_read             (GInputStream          *stream,
                                                        void                  *buffer,
                                                        gsize                  count,
                                                        GCancellable          *cancellable,
                                                        GError               **error);
static gssize g_buffered_input_stream_real_fill        (GBufferedInputStream  *stream,
                                                        gssize                 count,
                                                        GCancellable          *cancellable,
                                                        GError               **error);
static void   g_buffered_input_stream_real_fill_async  (GBufferedInputStream  *stream,
                                                        gssize                 count,
                                                        int                    io_priority,
                                                        GCancellable          *cancellable,
                                                        GAsyncReadyCallback    callback,
                                                        gpointer               user_data);
static gssize g_buffered_input_stream_real_fill_finish (GBufferedInputStream  *stream,
                                                        GAsyncResult          *result,
                                                        GError               **error);

static void     g_buffered_input_stream_seekable_iface_init (GSeekableIface  *iface);
static goffset  g_buffered_input_stream_tell                (GSeekable       *seekable);
static gboolean g_buffered_input_stream_can_seek            (GSeekable       *seekable);
static gboolean g_buffered_input_stream_seek                (GSeekable       *seekable,
							     goffset          offset,
							     GSeekType        type,
							     GCancellable    *cancellable,
							     GError         **error);
static gboolean g_buffered_input_stream_can_truncate        (GSeekable       *seekable);
static gboolean g_buffered_input_stream_truncate            (GSeekable       *seekable,
							     goffset          offset,
							     GCancellable    *cancellable,
							     GError         **error);

static void compact_buffer (GBufferedInputStream *stream);

G_DEFINE_TYPE_WITH_CODE (GBufferedInputStream,
			 g_buffered_input_stream,
			 G_TYPE_FILTER_INPUT_STREAM,
                         G_ADD_PRIVATE (GBufferedInputStream)
			 G_IMPLEMENT_INTERFACE (G_TYPE_SEEKABLE,
						g_buffered_input_stream_seekable_iface_init))

static void
g_buffered_input_stream_class_init (GBufferedInputStreamClass *klass)
{
  GObjectClass *object_class;
  GInputStreamClass *istream_class;
  GBufferedInputStreamClass *bstream_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->get_property = g_buffered_input_stream_get_property;
  object_class->set_property = g_buffered_input_stream_set_property;
  object_class->finalize     = g_buffered_input_stream_finalize;

  istream_class = G_INPUT_STREAM_CLASS (klass);
  istream_class->skip = g_buffered_input_stream_skip;
  istream_class->skip_async  = g_buffered_input_stream_skip_async;
  istream_class->skip_finish = g_buffered_input_stream_skip_finish;
  istream_class->read_fn = g_buffered_input_stream_read;

  bstream_class = G_BUFFERED_INPUT_STREAM_CLASS (klass);
  bstream_class->fill = g_buffered_input_stream_real_fill;
  bstream_class->fill_async = g_buffered_input_stream_real_fill_async;
  bstream_class->fill_finish = g_buffered_input_stream_real_fill_finish;

  g_object_class_install_property (object_class,
                                   PROP_BUFSIZE,
                                   g_param_spec_uint ("buffer-size",
                                                      P_("Buffer Size"),
                                                      P_("The size of the backend buffer"),
                                                      1,
                                                      G_MAXUINT,
                                                      DEFAULT_BUFFER_SIZE,
                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT |
                                                      G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB));


}

/**
 * g_buffered_input_stream_get_buffer_size:
 * @stream: a #GBufferedInputStream
 *
 * Gets the size of the input buffer.
 *
 * Returns: the current buffer size.
 */
gsize
g_buffered_input_stream_get_buffer_size (GBufferedInputStream  *stream)
{
  g_return_val_if_fail (G_IS_BUFFERED_INPUT_STREAM (stream), 0);

  return stream->priv->len;
}

/**
 * g_buffered_input_stream_set_buffer_size:
 * @stream: a #GBufferedInputStream
 * @size: a #gsize
 *
 * Sets the size of the internal buffer of @stream to @size, or to the
 * size of the contents of the buffer. The buffer can never be resized
 * smaller than its current contents.
 */
void
g_buffered_input_stream_set_buffer_size (GBufferedInputStream *stream,
                                         gsize                 size)
{
  GBufferedInputStreamPrivate *priv;
  gsize in_buffer;
  guint8 *buffer;

  g_return_if_fail (G_IS_BUFFERED_INPUT_STREAM (stream));

  priv = stream->priv;

  if (priv->len == size)
    return;

  if (priv->buffer)
    {
      in_buffer = priv->end - priv->pos;

      /* Never resize smaller than current buffer contents */
      size = MAX (size, in_buffer);

      buffer = g_malloc (size);
      memcpy (buffer, priv->buffer + priv->pos, in_buffer);
      priv->len = size;
      priv->pos = 0;
      priv->end = in_buffer;
      g_free (priv->buffer);
      priv->buffer = buffer;
    }
  else
    {
      priv->len = size;
      priv->pos = 0;
      priv->end = 0;
      priv->buffer = g_malloc (size);
    }

  g_object_notify (G_OBJECT (stream), "buffer-size");
}

static void
g_buffered_input_stream_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  GBufferedInputStream        *bstream;

  bstream = G_BUFFERED_INPUT_STREAM (object);

  switch (prop_id)
    {
    case PROP_BUFSIZE:
      g_buffered_input_stream_set_buffer_size (bstream, g_value_get_uint (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
g_buffered_input_stream_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  GBufferedInputStreamPrivate *priv;
  GBufferedInputStream        *bstream;

  bstream = G_BUFFERED_INPUT_STREAM (object);
  priv = bstream->priv;

  switch (prop_id)
    {
    case PROP_BUFSIZE:
      g_value_set_uint (value, priv->len);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
g_buffered_input_stream_finalize (GObject *object)
{
  GBufferedInputStreamPrivate *priv;
  GBufferedInputStream        *stream;

  stream = G_BUFFERED_INPUT_STREAM (object);
  priv = stream->priv;

  g_free (priv->buffer);

  G_OBJECT_CLASS (g_buffered_input_stream_parent_class)->finalize (object);
}

static void
g_buffered_input_stream_seekable_iface_init (GSeekableIface *iface)
{
  iface->tell         = g_buffered_input_stream_tell;
  iface->can_seek     = g_buffered_input_stream_can_seek;
  iface->seek         = g_buffered_input_stream_seek;
  iface->can_truncate = g_buffered_input_stream_can_truncate;
  iface->truncate_fn  = g_buffered_input_stream_truncate;
}

static void
g_buffered_input_stream_init (GBufferedInputStream *stream)
{
  stream->priv = g_buffered_input_stream_get_instance_private (stream);
}


/**
 * g_buffered_input_stream_new:
 * @base_stream: a #GInputStream
 *
 * Creates a new #GInputStream from the given @base_stream, with
 * a buffer set to the default size (4 kilobytes).
 *
 * Returns: a #GInputStream for the given @base_stream.
 */
GInputStream *
g_buffered_input_stream_new (GInputStream *base_stream)
{
  GInputStream *stream;

  g_return_val_if_fail (G_IS_INPUT_STREAM (base_stream), NULL);

  stream = g_object_new (G_TYPE_BUFFERED_INPUT_STREAM,
                         "base-stream", base_stream,
                         NULL);

  return stream;
}

/**
 * g_buffered_input_stream_new_sized:
 * @base_stream: a #GInputStream
 * @size: a #gsize
 *
 * Creates a new #GBufferedInputStream from the given @base_stream,
 * with a buffer set to @size.
 *
 * Returns: a #GInputStream.
 */
GInputStream *
g_buffered_input_stream_new_sized (GInputStream *base_stream,
                                   gsize         size)
{
  GInputStream *stream;

  g_return_val_if_fail (G_IS_INPUT_STREAM (base_stream), NULL);

  stream = g_object_new (G_TYPE_BUFFERED_INPUT_STREAM,
                         "base-stream", base_stream,
                         "buffer-size", (guint)size,
                         NULL);

  return stream;
}

/**
 * g_buffered_input_stream_fill:
 * @stream: a #GBufferedInputStream
 * @count: the number of bytes that will be read from the stream
 * @cancellable: (nullable): optional #GCancellable object, %NULL to ignore
 * @error: location to store the error occurring, or %NULL to ignore
 *
 * Tries to read @count bytes from the stream into the buffer.
 * Will block during this read.
 *
 * If @count is zero, returns zero and does nothing. A value of @count
 * larger than %G_MAXSSIZE will cause a %G_IO_ERROR_INVALID_ARGUMENT error.
 *
 * On success, the number of bytes read into the buffer is returned.
 * It is not an error if this is not the same as the requested size, as it
 * can happen e.g. near the end of a file. Zero is returned on end of file
 * (or if @count is zero),  but never otherwise.
 *
 * If @count is -1 then the attempted read size is equal to the number of
 * bytes that are required to fill the buffer.
 *
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. If an
 * operation was partially finished when the operation was cancelled the
 * partial result will be returned, without an error.
 *
 * On error -1 is returned and @error is set accordingly.
 *
 * For the asynchronous, non-blocking, version of this function, see
 * g_buffered_input_stream_fill_async().
 *
 * Returns: the number of bytes read into @stream's buffer, up to @count,
 *     or -1 on error.
 */
gssize
g_buffered_input_stream_fill (GBufferedInputStream  *stream,
                              gssize                 count,
                              GCancellable          *cancellable,
                              GError               **error)
{
  GBufferedInputStreamClass *class;
  GInputStream *input_stream;
  gssize res;

  g_return_val_if_fail (G_IS_BUFFERED_INPUT_STREAM (stream), -1);

  input_stream = G_INPUT_STREAM (stream);

  if (count < -1)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                   _("Too large count value passed to %s"), G_STRFUNC);
      return -1;
    }

  if (!g_input_stream_set_pending (input_stream, error))
    return -1;

  if (cancellable)
    g_cancellable_push_current (cancellable);

  class = G_BUFFERED_INPUT_STREAM_GET_CLASS (stream);
  res = class->fill (stream, count, cancellable, error);

  if (cancellable)
    g_cancellable_pop_current (cancellable);

  g_input_stream_clear_pending (input_stream);

  return res;
}

static void
async_fill_callback_wrapper (GObject      *source_object,
                             GAsyncResult *res,
                             gpointer      user_data)
{
  GBufferedInputStream *stream = G_BUFFERED_INPUT_STREAM (source_object);

  g_input_stream_clear_pending (G_INPUT_STREAM (stream));
  (*stream->priv->outstanding_callback) (source_object, res, user_data);
  g_object_unref (stream);
}

/**
 * g_buffered_input_stream_fill_async:
 * @stream: a #GBufferedInputStream
 * @count: the number of bytes that will be read from the stream
 * @io_priority: the [I/O priority][io-priority] of the request
 * @cancellable: (nullable): optional #GCancellable object
 * @callback: (scope async): a #GAsyncReadyCallback
 * @user_data: (closure): a #gpointer
 *
 * Reads data into @stream's buffer asynchronously, up to @count size.
 * @io_priority can be used to prioritize reads. For the synchronous
 * version of this function, see g_buffered_input_stream_fill().
 *
 * If @count is -1 then the attempted read size is equal to the number
 * of bytes that are required to fill the buffer.
 */
void
g_buffered_input_stream_fill_async (GBufferedInputStream *stream,
                                    gssize                count,
                                    int                   io_priority,
                                    GCancellable         *cancellable,
                                    GAsyncReadyCallback   callback,
                                    gpointer              user_data)
{
  GBufferedInputStreamClass *class;
  GError *error = NULL;

  g_return_if_fail (G_IS_BUFFERED_INPUT_STREAM (stream));

  if (count == 0)
    {
      GTask *task;

      task = g_task_new (stream, cancellable, callback, user_data);
      g_task_set_source_tag (task, g_buffered_input_stream_fill_async);
      g_task_return_int (task, 0);
      g_object_unref (task);
      return;
    }

  if (count < -1)
    {
      g_task_report_new_error (stream, callback, user_data,
                               g_buffered_input_stream_fill_async,
                               G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
			       _("Too large count value passed to %s"),
			       G_STRFUNC);
      return;
    }

  if (!g_input_stream_set_pending (G_INPUT_STREAM (stream), &error))
    {
      g_task_report_error (stream, callback, user_data,
                           g_buffered_input_stream_fill_async,
                           error);
      return;
    }

  class = G_BUFFERED_INPUT_STREAM_GET_CLASS (stream);

  stream->priv->outstanding_callback = callback;
  g_object_ref (stream);
  class->fill_async (stream, count, io_priority, cancellable,
                     async_fill_callback_wrapper, user_data);
}

/**
 * g_buffered_input_stream_fill_finish:
 * @stream: a #GBufferedInputStream
 * @result: a #GAsyncResult
 * @error: a #GError
 *
 * Finishes an asynchronous read.
 *
 * Returns: a #gssize of the read stream, or `-1` on an error.
 */
gssize
g_buffered_input_stream_fill_finish (GBufferedInputStream  *stream,
                                     GAsyncResult          *result,
                                     GError               **error)
{
  GBufferedInputStreamClass *class;

  g_return_val_if_fail (G_IS_BUFFERED_INPUT_STREAM (stream), -1);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), -1);

  if (g_async_result_legacy_propagate_error (result, error))
    return -1;
  else if (g_async_result_is_tagged (result, g_buffered_input_stream_fill_async))
    return g_task_propagate_int (G_TASK (result), error);

  class = G_BUFFERED_INPUT_STREAM_GET_CLASS (stream);
  return class->fill_finish (stream, result, error);
}

/**
 * g_buffered_input_stream_get_available:
 * @stream: #GBufferedInputStream
 *
 * Gets the size of the available data within the stream.
 *
 * Returns: size of the available stream.
 */
gsize
g_buffered_input_stream_get_available (GBufferedInputStream *stream)
{
  g_return_val_if_fail (G_IS_BUFFERED_INPUT_STREAM (stream), -1);

  return stream->priv->end - stream->priv->pos;
}

/**
 * g_buffered_input_stream_peek:
 * @stream: a #GBufferedInputStream
 * @buffer: (array length=count) (element-type guint8): a pointer to
 *   an allocated chunk of memory
 * @offset: a #gsize
 * @count: a #gsize
 *
 * Peeks in the buffer, copying data of size @count into @buffer,
 * offset @offset bytes.
 *
 * Returns: a #gsize of the number of bytes peeked, or -1 on error.
 */
gsize
g_buffered_input_stream_peek (GBufferedInputStream *stream,
                              void                 *buffer,
                              gsize                 offset,
                              gsize                 count)
{
  gsize available;
  gsize end;

  g_return_val_if_fail (G_IS_BUFFERED_INPUT_STREAM (stream), -1);
  g_return_val_if_fail (buffer != NULL, -1);

  available = g_buffered_input_stream_get_available (stream);

  if (offset > available)
    return 0;

  end = MIN (offset + count, available);
  count = end - offset;

  memcpy (buffer, stream->priv->buffer + stream->priv->pos + offset, count);
  return count;
}

/**
 * g_buffered_input_stream_peek_buffer:
 * @stream: a #GBufferedInputStream
 * @count: (out): a #gsize to get the number of bytes available in the buffer
 *
 * Returns the buffer with the currently available bytes. The returned
 * buffer must not be modified and will become invalid when reading from
 * the stream or filling the buffer.
 *
 * Returns: (array length=count) (element-type guint8) (transfer none):
 *          read-only buffer
 */
const void*
g_buffered_input_stream_peek_buffer (GBufferedInputStream *stream,
                                     gsize                *count)
{
  GBufferedInputStreamPrivate *priv;

  g_return_val_if_fail (G_IS_BUFFERED_INPUT_STREAM (stream), NULL);

  priv = stream->priv;

  if (count)
    *count = priv->end - priv->pos;

  return priv->buffer + priv->pos;
}

static void
compact_buffer (GBufferedInputStream *stream)
{
  GBufferedInputStreamPrivate *priv;
  gsize current_size;

  priv = stream->priv;

  current_size = priv->end - priv->pos;

  memmove (priv->buffer, priv->buffer + priv->pos, current_size);

  priv->pos = 0;
  priv->end = current_size;
}

static gssize
g_buffered_input_stream_real_fill (GBufferedInputStream  *stream,
                                   gssize                 count,
                                   GCancellable          *cancellable,
                                   GError               **error)
{
  GBufferedInputStreamPrivate *priv;
  GInputStream *base_stream;
  gssize nread;
  gsize in_buffer;

  priv = stream->priv;

  if (count == -1)
    count = priv->len;

  in_buffer = priv->end - priv->pos;

  /* Never fill more than can fit in the buffer */
  count = MIN (count, priv->len - in_buffer);

  /* If requested length does not fit at end, compact */
  if (priv->len - priv->end < count)
    compact_buffer (stream);

  base_stream = G_FILTER_INPUT_STREAM (stream)->base_stream;
  nread = g_input_stream_read (base_stream,
                               priv->buffer + priv->end,
                               count,
                               cancellable,
                               error);

  if (nread > 0)
    priv->end += nread;

  return nread;
}

static gssize
g_buffered_input_stream_skip (GInputStream  *stream,
                              gsize          count,
                              GCancellable  *cancellable,
                              GError       **error)
{
  GBufferedInputStream        *bstream;
  GBufferedInputStreamPrivate *priv;
  GBufferedInputStreamClass *class;
  GInputStream *base_stream;
  gsize available, bytes_skipped;
  gssize nread;

  bstream = G_BUFFERED_INPUT_STREAM (stream);
  priv = bstream->priv;

  available = priv->end - priv->pos;

  if (count <= available)
    {
      priv->pos += count;
      return count;
    }

  /* Full request not available, skip all currently available and
   * request refill for more
   */

  priv->pos = 0;
  priv->end = 0;
  bytes_skipped = available;
  count -= available;

  if (bytes_skipped > 0)
    error = NULL; /* Ignore further errors if we already read some data */

  if (count > priv->len)
    {
      /* Large request, shortcut buffer */

      base_stream = G_FILTER_INPUT_STREAM (stream)->base_stream;

      nread = g_input_stream_skip (base_stream,
                                   count,
                                   cancellable,
                                   error);

      if (nread < 0 && bytes_skipped == 0)
        return -1;

      if (nread > 0)
        bytes_skipped += nread;

      return bytes_skipped;
    }

  class = G_BUFFERED_INPUT_STREAM_GET_CLASS (stream);
  nread = class->fill (bstream, priv->len, cancellable, error);

  if (nread < 0)
    {
      if (bytes_skipped == 0)
        return -1;
      else
        return bytes_skipped;
    }

  available = priv->end - priv->pos;
  count = MIN (count, available);

  bytes_skipped += count;
  priv->pos += count;

  return bytes_skipped;
}

static gssize
g_buffered_input_stream_read (GInputStream *stream,
                              void         *buffer,
                              gsize         count,
                              GCancellable *cancellable,
                              GError      **error)
{
  GBufferedInputStream        *bstream;
  GBufferedInputStreamPrivate *priv;
  GBufferedInputStreamClass *class;
  GInputStream *base_stream;
  gsize available, bytes_read;
  gssize nread;

  bstream = G_BUFFERED_INPUT_STREAM (stream);
  priv = bstream->priv;

  available = priv->end - priv->pos;

  if (count <= available)
    {
      memcpy (buffer, priv->buffer + priv->pos, count);
      priv->pos += count;
      return count;
    }

  /* Full request not available, read all currently available and
   * request refill for more
   */

  memcpy (buffer, priv->buffer + priv->pos, available);
  priv->pos = 0;
  priv->end = 0;
  bytes_read = available;
  count -= available;

  if (bytes_read > 0)
    error = NULL; /* Ignore further errors if we already read some data */

  if (count > priv->len)
    {
      /* Large request, shortcut buffer */

      base_stream = G_FILTER_INPUT_STREAM (stream)->base_stream;

      nread = g_input_stream_read (base_stream,
                                   (char *)buffer + bytes_read,
                                   count,
                                   cancellable,
                                   error);

      if (nread < 0 && bytes_read == 0)
        return -1;

      if (nread > 0)
        bytes_read += nread;

      return bytes_read;
    }

  class = G_BUFFERED_INPUT_STREAM_GET_CLASS (stream);
  nread = class->fill (bstream, priv->len, cancellable, error);
  if (nread < 0)
    {
      if (bytes_read == 0)
        return -1;
      else
        return bytes_read;
    }

  available = priv->end - priv->pos;
  count = MIN (count, available);

  memcpy ((char *)buffer + bytes_read, (char *)priv->buffer + priv->pos, count);
  bytes_read += count;
  priv->pos += count;

  return bytes_read;
}

static goffset
g_buffered_input_stream_tell (GSeekable *seekable)
{
  GBufferedInputStream        *bstream;
  GBufferedInputStreamPrivate *priv;
  GInputStream *base_stream;
  GSeekable    *base_stream_seekable;
  gsize available;
  goffset base_offset;
  
  bstream = G_BUFFERED_INPUT_STREAM (seekable);
  priv = bstream->priv;

  base_stream = G_FILTER_INPUT_STREAM (seekable)->base_stream;
  if (!G_IS_SEEKABLE (base_stream))
    return 0;
  base_stream_seekable = G_SEEKABLE (base_stream);
  
  available = priv->end - priv->pos;
  base_offset = g_seekable_tell (base_stream_seekable);

  return base_offset - available;
}

static gboolean
g_buffered_input_stream_can_seek (GSeekable *seekable)
{
  GInputStream *base_stream;
  
  base_stream = G_FILTER_INPUT_STREAM (seekable)->base_stream;
  return G_IS_SEEKABLE (base_stream) && g_seekable_can_seek (G_SEEKABLE (base_stream));
}

static gboolean
g_buffered_input_stream_seek (GSeekable     *seekable,
			      goffset        offset,
			      GSeekType      type,
			      GCancellable  *cancellable,
			      GError       **error)
{
  GBufferedInputStream        *bstream;
  GBufferedInputStreamPrivate *priv;
  GInputStream *base_stream;
  GSeekable *base_stream_seekable;

  bstream = G_BUFFERED_INPUT_STREAM (seekable);
  priv = bstream->priv;

  base_stream = G_FILTER_INPUT_STREAM (seekable)->base_stream;
  if (!G_IS_SEEKABLE (base_stream))
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                           _("Seek not supported on base stream"));
      return FALSE;
    }

  base_stream_seekable = G_SEEKABLE (base_stream);
  
  if (type == G_SEEK_CUR)
    {
      if (offset <= priv->end - priv->pos && offset >= -priv->pos)
	{
	  priv->pos += offset;
	  return TRUE;
	}
      else
	{
	  offset -= priv->end - priv->pos;
	}
    }

  if (g_seekable_seek (base_stream_seekable, offset, type, cancellable, error))
    {
      priv->pos = 0;
      priv->end = 0;
      return TRUE;
    }
  else
    {
      return FALSE;
    }
}

static gboolean
g_buffered_input_stream_can_truncate (GSeekable *seekable)
{
  return FALSE;
}

static gboolean
g_buffered_input_stream_truncate (GSeekable     *seekable,
				  goffset        offset,
				  GCancellable  *cancellable,
				  GError       **error)
{
  g_set_error_literal (error,
		       G_IO_ERROR,
		       G_IO_ERROR_NOT_SUPPORTED,
		       _("Cannot truncate GBufferedInputStream"));
  return FALSE;
}

/**
 * g_buffered_input_stream_read_byte:
 * @stream: a #GBufferedInputStream
 * @cancellable: (nullable): optional #GCancellable object, %NULL to ignore
 * @error: location to store the error occurring, or %NULL to ignore
 *
 * Tries to read a single byte from the stream or the buffer. Will block
 * during this read.
 *
 * On success, the byte read from the stream is returned. On end of stream
 * -1 is returned but it's not an exceptional error and @error is not set.
 *
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. If an
 * operation was partially finished when the operation was cancelled the
 * partial result will be returned, without an error.
 *
 * On error -1 is returned and @error is set accordingly.
 *
 * Returns: the byte read from the @stream, or -1 on end of stream or error.
 */
int
g_buffered_input_stream_read_byte (GBufferedInputStream  *stream,
                                   GCancellable          *cancellable,
                                   GError               **error)
{
  GBufferedInputStreamPrivate *priv;
  GBufferedInputStreamClass *class;
  GInputStream *input_stream;
  gsize available;
  gssize nread;

  g_return_val_if_fail (G_IS_BUFFERED_INPUT_STREAM (stream), -1);

  priv = stream->priv;
  input_stream = G_INPUT_STREAM (stream);

  if (g_input_stream_is_closed (input_stream))
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_CLOSED,
                           _("Stream is already closed"));
      return -1;
    }

  if (!g_input_stream_set_pending (input_stream, error))
    return -1;

  available = priv->end - priv->pos;

  if (available != 0)
    {
      g_input_stream_clear_pending (input_stream);
      return priv->buffer[priv->pos++];
    }

  /* Byte not available, request refill for more */

  if (cancellable)
    g_cancellable_push_current (cancellable);

  priv->pos = 0;
  priv->end = 0;

  class = G_BUFFERED_INPUT_STREAM_GET_CLASS (stream);
  nread = class->fill (stream, priv->len, cancellable, error);

  if (cancellable)
    g_cancellable_pop_current (cancellable);

  g_input_stream_clear_pending (input_stream);

  if (nread <= 0)
    return -1; /* error or end of stream */

  return priv->buffer[priv->pos++];
}

/* ************************** */
/* Async stuff implementation */
/* ************************** */

static void
fill_async_callback (GObject      *source_object,
                     GAsyncResult *result,
                     gpointer      user_data)
{
  GError *error;
  gssize res;
  GTask *task = user_data;

  error = NULL;
  res = g_input_stream_read_finish (G_INPUT_STREAM (source_object),
                                    result, &error);
  if (res == -1)
    g_task_return_error (task, error);
  else
    {
      GBufferedInputStream *stream;
      GBufferedInputStreamPrivate *priv;

      stream = g_task_get_source_object (task);
      priv = G_BUFFERED_INPUT_STREAM (stream)->priv;

      g_assert_cmpint (priv->end + res, <=, priv->len);
      priv->end += res;

      g_task_return_int (task, res);
    }

  g_object_unref (task);
}

static void
g_buffered_input_stream_real_fill_async (GBufferedInputStream *stream,
                                         gssize                count,
                                         int                   io_priority,
                                         GCancellable         *cancellable,
                                         GAsyncReadyCallback   callback,
                                         gpointer              user_data)
{
  GBufferedInputStreamPrivate *priv;
  GInputStream *base_stream;
  GTask *task;
  gsize in_buffer;

  priv = stream->priv;

  if (count == -1)
    count = priv->len;

  in_buffer = priv->end - priv->pos;

  /* Never fill more than can fit in the buffer */
  count = MIN (count, priv->len - in_buffer);

  /* If requested length does not fit at end, compact */
  if (priv->len - priv->end < count)
    compact_buffer (stream);

  task = g_task_new (stream, cancellable, callback, user_data);
  g_task_set_source_tag (task, g_buffered_input_stream_real_fill_async);

  base_stream = G_FILTER_INPUT_STREAM (stream)->base_stream;
  g_input_stream_read_async (base_stream,
                             priv->buffer + priv->end,
                             count,
                             io_priority,
                             cancellable,
                             fill_async_callback,
                             task);
}

static gssize
g_buffered_input_stream_real_fill_finish (GBufferedInputStream *stream,
                                          GAsyncResult         *result,
                                          GError              **error)
{
  g_return_val_if_fail (g_task_is_valid (result, stream), -1);

  return g_task_propagate_int (G_TASK (result), error);
}

typedef struct
{
  gssize bytes_skipped;
  gssize count;
} SkipAsyncData;

static void
free_skip_async_data (gpointer _data)
{
  SkipAsyncData *data = _data;
  g_slice_free (SkipAsyncData, data);
}

static void
large_skip_callback (GObject      *source_object,
                     GAsyncResult *result,
                     gpointer      user_data)
{
  GTask *task = G_TASK (user_data);
  SkipAsyncData *data;
  GError *error;
  gssize nread;

  data = g_task_get_task_data (task);

  error = NULL;
  nread = g_input_stream_skip_finish (G_INPUT_STREAM (source_object),
                                      result, &error);

  /* Only report the error if we've not already read some data */
  if (nread < 0 && data->bytes_skipped == 0)
    g_task_return_error (task, error);
  else
    {
      if (error)
	g_error_free (error);

      if (nread > 0)
	data->bytes_skipped += nread;

      g_task_return_int (task, data->bytes_skipped);
    }

  g_object_unref (task);
}

static void
skip_fill_buffer_callback (GObject      *source_object,
                           GAsyncResult *result,
                           gpointer      user_data)
{
  GTask *task = G_TASK (user_data);
  GBufferedInputStream *bstream;
  GBufferedInputStreamPrivate *priv;
  SkipAsyncData *data;
  GError *error;
  gssize nread;
  gsize available;

  bstream = G_BUFFERED_INPUT_STREAM (source_object);
  priv = bstream->priv;

  data = g_task_get_task_data (task);

  error = NULL;
  nread = g_buffered_input_stream_fill_finish (bstream,
                                               result, &error);

  if (nread < 0 && data->bytes_skipped == 0)
    g_task_return_error (task, error);
  else
    {
      if (error)
	g_error_free (error);

      if (nread > 0)
	{
	  available = priv->end - priv->pos;
	  data->count = MIN (data->count, available);

	  data->bytes_skipped += data->count;
	  priv->pos += data->count;
	}

      g_task_return_int (task, data->bytes_skipped);
    }

  g_object_unref (task);
}

static void
g_buffered_input_stream_skip_async (GInputStream        *stream,
                                    gsize                count,
                                    int                  io_priority,
                                    GCancellable        *cancellable,
                                    GAsyncReadyCallback  callback,
                                    gpointer             user_data)
{
  GBufferedInputStream *bstream;
  GBufferedInputStreamPrivate *priv;
  GBufferedInputStreamClass *class;
  GInputStream *base_stream;
  gsize available;
  GTask *task;
  SkipAsyncData *data;

  bstream = G_BUFFERED_INPUT_STREAM (stream);
  priv = bstream->priv;

  data = g_slice_new (SkipAsyncData);
  data->bytes_skipped = 0;
  task = g_task_new (stream, cancellable, callback, user_data);
  g_task_set_source_tag (task, g_buffered_input_stream_skip_async);
  g_task_set_task_data (task, data, free_skip_async_data);

  available = priv->end - priv->pos;

  if (count <= available)
    {
      priv->pos += count;

      g_task_return_int (task, count);
      g_object_unref (task);
      return;
    }

  /* Full request not available, skip all currently available
   * and request refill for more
   */

  priv->pos = 0;
  priv->end = 0;

  count -= available;

  data->bytes_skipped = available;
  data->count = count;

  if (count > priv->len)
    {
      /* Large request, shortcut buffer */

      base_stream = G_FILTER_INPUT_STREAM (stream)->base_stream;

      g_input_stream_skip_async (base_stream,
                                 count,
                                 io_priority, cancellable,
                                 large_skip_callback,
                                 task);
    }
  else
    {
      class = G_BUFFERED_INPUT_STREAM_GET_CLASS (stream);
      class->fill_async (bstream, priv->len, io_priority, cancellable,
                         skip_fill_buffer_callback, task);
    }
}

static gssize
g_buffered_input_stream_skip_finish (GInputStream   *stream,
                                     GAsyncResult   *result,
                                     GError        **error)
{
  g_return_val_if_fail (g_task_is_valid (result, stream), -1);

  return g_task_propagate_int (G_TASK (result), error);
}
