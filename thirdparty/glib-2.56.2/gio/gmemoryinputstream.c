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
#include "gmemoryinputstream.h"
#include "gpollableinputstream.h"
#include "ginputstream.h"
#include "gseekable.h"
#include "string.h"
#include "gtask.h"
#include "gioerror.h"
#include "glibintl.h"


/**
 * SECTION:gmemoryinputstream
 * @short_description: Streaming input operations on memory chunks
 * @include: gio/gio.h
 * @see_also: #GMemoryOutputStream
 *
 * #GMemoryInputStream is a class for using arbitrary
 * memory chunks as input for GIO streaming input operations.
 *
 * As of GLib 2.34, #GMemoryInputStream implements
 * #GPollableInputStream.
 */

struct _GMemoryInputStreamPrivate {
  GSList *chunks;
  gsize   len;
  gsize   pos;
};

static gssize   g_memory_input_stream_read         (GInputStream         *stream,
						    void                 *buffer,
						    gsize                 count,
						    GCancellable         *cancellable,
						    GError              **error);
static gssize   g_memory_input_stream_skip         (GInputStream         *stream,
						    gsize                 count,
						    GCancellable         *cancellable,
						    GError              **error);
static gboolean g_memory_input_stream_close        (GInputStream         *stream,
						    GCancellable         *cancellable,
						    GError              **error);
static void     g_memory_input_stream_skip_async   (GInputStream         *stream,
						    gsize                 count,
						    int                   io_priority,
						    GCancellable         *cancellabl,
						    GAsyncReadyCallback   callback,
						    gpointer              datae);
static gssize   g_memory_input_stream_skip_finish  (GInputStream         *stream,
						    GAsyncResult         *result,
						    GError              **error);
static void     g_memory_input_stream_close_async  (GInputStream         *stream,
						    int                   io_priority,
						    GCancellable         *cancellabl,
						    GAsyncReadyCallback   callback,
						    gpointer              data);
static gboolean g_memory_input_stream_close_finish (GInputStream         *stream,
						    GAsyncResult         *result,
						    GError              **error);

static void     g_memory_input_stream_seekable_iface_init (GSeekableIface  *iface);
static goffset  g_memory_input_stream_tell                (GSeekable       *seekable);
static gboolean g_memory_input_stream_can_seek            (GSeekable       *seekable);
static gboolean g_memory_input_stream_seek                (GSeekable       *seekable,
                                                           goffset          offset,
                                                           GSeekType        type,
                                                           GCancellable    *cancellable,
                                                           GError         **error);
static gboolean g_memory_input_stream_can_truncate        (GSeekable       *seekable);
static gboolean g_memory_input_stream_truncate            (GSeekable       *seekable,
                                                           goffset          offset,
                                                           GCancellable    *cancellable,
                                                           GError         **error);

static void     g_memory_input_stream_pollable_iface_init (GPollableInputStreamInterface *iface);
static gboolean g_memory_input_stream_is_readable         (GPollableInputStream *stream);
static GSource *g_memory_input_stream_create_source       (GPollableInputStream *stream,
							   GCancellable          *cancellable);

static void     g_memory_input_stream_finalize            (GObject         *object);

G_DEFINE_TYPE_WITH_CODE (GMemoryInputStream, g_memory_input_stream, G_TYPE_INPUT_STREAM,
                         G_ADD_PRIVATE (GMemoryInputStream)
                         G_IMPLEMENT_INTERFACE (G_TYPE_SEEKABLE,
                                                g_memory_input_stream_seekable_iface_init);
                         G_IMPLEMENT_INTERFACE (G_TYPE_POLLABLE_INPUT_STREAM,
                                                g_memory_input_stream_pollable_iface_init);
			 )


static void
g_memory_input_stream_class_init (GMemoryInputStreamClass *klass)
{
  GObjectClass *object_class;
  GInputStreamClass *istream_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->finalize     = g_memory_input_stream_finalize;
  
  istream_class = G_INPUT_STREAM_CLASS (klass);
  istream_class->read_fn  = g_memory_input_stream_read;
  istream_class->skip  = g_memory_input_stream_skip;
  istream_class->close_fn = g_memory_input_stream_close;

  istream_class->skip_async  = g_memory_input_stream_skip_async;
  istream_class->skip_finish  = g_memory_input_stream_skip_finish;
  istream_class->close_async = g_memory_input_stream_close_async;
  istream_class->close_finish = g_memory_input_stream_close_finish;
}

static void
g_memory_input_stream_finalize (GObject *object)
{
  GMemoryInputStream        *stream;
  GMemoryInputStreamPrivate *priv;

  stream = G_MEMORY_INPUT_STREAM (object);
  priv = stream->priv;

  g_slist_free_full (priv->chunks, (GDestroyNotify)g_bytes_unref);

  G_OBJECT_CLASS (g_memory_input_stream_parent_class)->finalize (object);
}

static void
g_memory_input_stream_seekable_iface_init (GSeekableIface *iface)
{
  iface->tell         = g_memory_input_stream_tell;
  iface->can_seek     = g_memory_input_stream_can_seek;
  iface->seek         = g_memory_input_stream_seek;
  iface->can_truncate = g_memory_input_stream_can_truncate;
  iface->truncate_fn  = g_memory_input_stream_truncate;
}

static void
g_memory_input_stream_pollable_iface_init (GPollableInputStreamInterface *iface)
{
  iface->is_readable   = g_memory_input_stream_is_readable;
  iface->create_source = g_memory_input_stream_create_source;
}

static void
g_memory_input_stream_init (GMemoryInputStream *stream)
{
  stream->priv = g_memory_input_stream_get_instance_private (stream);
}

/**
 * g_memory_input_stream_new:
 *
 * Creates a new empty #GMemoryInputStream. 
 *
 * Returns: a new #GInputStream
 */
GInputStream *
g_memory_input_stream_new (void)
{
  GInputStream *stream;

  stream = g_object_new (G_TYPE_MEMORY_INPUT_STREAM, NULL);

  return stream;
}

/**
 * g_memory_input_stream_new_from_data:
 * @data: (array length=len) (element-type guint8) (transfer full): input data
 * @len: length of the data, may be -1 if @data is a nul-terminated string
 * @destroy: (nullable): function that is called to free @data, or %NULL
 *
 * Creates a new #GMemoryInputStream with data in memory of a given size.
 * 
 * Returns: new #GInputStream read from @data of @len bytes.
 **/
GInputStream *
g_memory_input_stream_new_from_data (const void     *data, 
                                     gssize          len,
                                     GDestroyNotify  destroy)
{
  GInputStream *stream;

  stream = g_memory_input_stream_new ();

  g_memory_input_stream_add_data (G_MEMORY_INPUT_STREAM (stream),
                                  data, len, destroy);

  return stream;
}

/**
 * g_memory_input_stream_new_from_bytes:
 * @bytes: a #GBytes
 *
 * Creates a new #GMemoryInputStream with data from the given @bytes.
 *
 * Returns: new #GInputStream read from @bytes
 *
 * Since: 2.34
 **/
GInputStream *
g_memory_input_stream_new_from_bytes (GBytes  *bytes)
{
  
  GInputStream *stream;

  stream = g_memory_input_stream_new ();

  g_memory_input_stream_add_bytes (G_MEMORY_INPUT_STREAM (stream),
				   bytes);

  return stream;
}

/**
 * g_memory_input_stream_add_data:
 * @stream: a #GMemoryInputStream
 * @data: (array length=len) (element-type guint8) (transfer full): input data
 * @len: length of the data, may be -1 if @data is a nul-terminated string
 * @destroy: (nullable): function that is called to free @data, or %NULL
 *
 * Appends @data to data that can be read from the input stream
 */
void
g_memory_input_stream_add_data (GMemoryInputStream *stream,
                                const void         *data,
                                gssize              len,
                                GDestroyNotify      destroy)
{
  GBytes *bytes;

  if (len == -1)
    len = strlen (data);

  /* It's safe to discard the const here because we're chaining the
   * destroy callback.
   */
  bytes = g_bytes_new_with_free_func (data, len, destroy, (void*)data);

  g_memory_input_stream_add_bytes (stream, bytes);
  
  g_bytes_unref (bytes);
}

/**
 * g_memory_input_stream_add_bytes:
 * @stream: a #GMemoryInputStream
 * @bytes: input data
 *
 * Appends @bytes to data that can be read from the input stream.
 *
 * Since: 2.34
 */
void
g_memory_input_stream_add_bytes (GMemoryInputStream *stream,
				 GBytes             *bytes)
{
  GMemoryInputStreamPrivate *priv;
 
  g_return_if_fail (G_IS_MEMORY_INPUT_STREAM (stream));
  g_return_if_fail (bytes != NULL);

  priv = stream->priv;

  priv->chunks = g_slist_append (priv->chunks, g_bytes_ref (bytes));
  priv->len += g_bytes_get_size (bytes);
}

static gssize
g_memory_input_stream_read (GInputStream  *stream,
                            void          *buffer,
                            gsize          count,
                            GCancellable  *cancellable,
                            GError       **error)
{
  GMemoryInputStream *memory_stream;
  GMemoryInputStreamPrivate *priv;
  GSList *l;
  GBytes *chunk;
  gsize len;
  gsize offset, start, rest, size;

  memory_stream = G_MEMORY_INPUT_STREAM (stream);
  priv = memory_stream->priv;

  count = MIN (count, priv->len - priv->pos);

  offset = 0;
  for (l = priv->chunks; l; l = l->next) 
    {
      chunk = (GBytes *)l->data;
      len = g_bytes_get_size (chunk);

      if (offset + len > priv->pos)
        break;

      offset += len;
    }
  
  start = priv->pos - offset;
  rest = count;

  for (; l && rest > 0; l = l->next)
    {
      const guint8* chunk_data;
      chunk = (GBytes *)l->data;

      chunk_data = g_bytes_get_data (chunk, &len);

      size = MIN (rest, len - start);

      memcpy ((guint8 *)buffer + (count - rest), chunk_data + start, size);
      rest -= size;

      start = 0;
    }

  priv->pos += count;

  return count;
}

static gssize
g_memory_input_stream_skip (GInputStream  *stream,
                            gsize          count,
                            GCancellable  *cancellable,
                            GError       **error)
{
  GMemoryInputStream *memory_stream;
  GMemoryInputStreamPrivate *priv;

  memory_stream = G_MEMORY_INPUT_STREAM (stream);
  priv = memory_stream->priv;

  count = MIN (count, priv->len - priv->pos);
  priv->pos += count;

  return count;
}

static gboolean
g_memory_input_stream_close (GInputStream  *stream,
                             GCancellable  *cancellable,
                             GError       **error)
{
  return TRUE;
}

static void
g_memory_input_stream_skip_async (GInputStream        *stream,
                                  gsize                count,
                                  int                  io_priority,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data)
{
  GTask *task;
  gssize nskipped;
  GError *error = NULL;

  nskipped = G_INPUT_STREAM_GET_CLASS (stream)->skip (stream, count, cancellable, &error);
  task = g_task_new (stream, cancellable, callback, user_data);
  g_task_set_source_tag (task, g_memory_input_stream_skip_async);

  if (error)
    g_task_return_error (task, error);
  else
    g_task_return_int (task, nskipped);
  g_object_unref (task);
}

static gssize
g_memory_input_stream_skip_finish (GInputStream  *stream,
                                   GAsyncResult  *result,
                                   GError       **error)
{
  g_return_val_if_fail (g_task_is_valid (result, stream), -1);

  return g_task_propagate_int (G_TASK (result), error);
}

static void
g_memory_input_stream_close_async (GInputStream        *stream,
                                   int                  io_priority,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data)
{
  GTask *task;

  task = g_task_new (stream, cancellable, callback, user_data);
  g_task_set_source_tag (task, g_memory_input_stream_close_async);
  g_task_return_boolean (task, TRUE);
  g_object_unref (task);
}

static gboolean
g_memory_input_stream_close_finish (GInputStream  *stream,
                                    GAsyncResult  *result,
                                    GError       **error)
{
  return TRUE;
}

static goffset
g_memory_input_stream_tell (GSeekable *seekable)
{
  GMemoryInputStream *memory_stream;
  GMemoryInputStreamPrivate *priv;

  memory_stream = G_MEMORY_INPUT_STREAM (seekable);
  priv = memory_stream->priv;

  return priv->pos;
}

static
gboolean g_memory_input_stream_can_seek (GSeekable *seekable)
{
  return TRUE;
}

static gboolean
g_memory_input_stream_seek (GSeekable     *seekable,
                            goffset        offset,
                            GSeekType      type,
                            GCancellable  *cancellable,
                            GError       **error)
{
  GMemoryInputStream *memory_stream;
  GMemoryInputStreamPrivate *priv;
  goffset absolute;

  memory_stream = G_MEMORY_INPUT_STREAM (seekable);
  priv = memory_stream->priv;

  switch (type) 
    {
    case G_SEEK_CUR:
      absolute = priv->pos + offset;
      break;

    case G_SEEK_SET:
      absolute = offset;
      break;

    case G_SEEK_END:
      absolute = priv->len + offset;
      break;
  
    default:
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_INVALID_ARGUMENT,
                           _("Invalid GSeekType supplied"));

      return FALSE;
    }

  if (absolute < 0 || absolute > priv->len)
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_INVALID_ARGUMENT,
                           _("Invalid seek request"));
      return FALSE;
    }

  priv->pos = absolute;

  return TRUE;
}

static gboolean
g_memory_input_stream_can_truncate (GSeekable *seekable)
{
  return FALSE;
}

static gboolean
g_memory_input_stream_truncate (GSeekable     *seekable,
                                goffset        offset,
                                GCancellable  *cancellable,
                                GError       **error)
{
  g_set_error_literal (error,
                       G_IO_ERROR,
                       G_IO_ERROR_NOT_SUPPORTED,
                       _("Cannot truncate GMemoryInputStream"));
  return FALSE;
}

static gboolean
g_memory_input_stream_is_readable (GPollableInputStream *stream)
{
  return TRUE;
}

static GSource *
g_memory_input_stream_create_source (GPollableInputStream *stream,
				     GCancellable         *cancellable)
{
  GSource *base_source, *pollable_source;

  base_source = g_timeout_source_new (0);
  pollable_source = g_pollable_source_new_full (stream, base_source,
						cancellable);
  g_source_unref (base_source);

  return pollable_source;
}
