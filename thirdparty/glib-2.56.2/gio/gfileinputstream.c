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

#include <glib.h>
#include <gfileinputstream.h>
#include <gseekable.h>
#include "gcancellable.h"
#include "gasyncresult.h"
#include "gtask.h"
#include "gioerror.h"
#include "glibintl.h"


/**
 * SECTION:gfileinputstream
 * @short_description: File input streaming operations
 * @include: gio/gio.h
 * @see_also: #GInputStream, #GDataInputStream, #GSeekable
 *
 * GFileInputStream provides input streams that take their
 * content from a file.
 *
 * GFileInputStream implements #GSeekable, which allows the input 
 * stream to jump to arbitrary positions in the file, provided the 
 * filesystem of the file allows it. To find the position of a file
 * input stream, use g_seekable_tell(). To find out if a file input
 * stream supports seeking, use g_seekable_can_seek().
 * To position a file input stream, use g_seekable_seek().
 **/

static void       g_file_input_stream_seekable_iface_init    (GSeekableIface       *iface);
static goffset    g_file_input_stream_seekable_tell          (GSeekable            *seekable);
static gboolean   g_file_input_stream_seekable_can_seek      (GSeekable            *seekable);
static gboolean   g_file_input_stream_seekable_seek          (GSeekable            *seekable,
							      goffset               offset,
							      GSeekType             type,
							      GCancellable         *cancellable,
							      GError              **error);
static gboolean   g_file_input_stream_seekable_can_truncate  (GSeekable            *seekable);
static gboolean   g_file_input_stream_seekable_truncate      (GSeekable            *seekable,
							      goffset               offset,
							      GCancellable         *cancellable,
							      GError              **error);
static void       g_file_input_stream_real_query_info_async  (GFileInputStream     *stream,
							      const char           *attributes,
							      int                   io_priority,
							      GCancellable         *cancellable,
							      GAsyncReadyCallback   callback,
							      gpointer              user_data);
static GFileInfo *g_file_input_stream_real_query_info_finish (GFileInputStream     *stream,
							      GAsyncResult         *result,
							      GError              **error);


struct _GFileInputStreamPrivate {
  GAsyncReadyCallback outstanding_callback;
};

G_DEFINE_TYPE_WITH_CODE (GFileInputStream, g_file_input_stream, G_TYPE_INPUT_STREAM,
                         G_ADD_PRIVATE (GFileInputStream)
			 G_IMPLEMENT_INTERFACE (G_TYPE_SEEKABLE,
						g_file_input_stream_seekable_iface_init))

static void
g_file_input_stream_class_init (GFileInputStreamClass *klass)
{
  klass->query_info_async = g_file_input_stream_real_query_info_async;
  klass->query_info_finish = g_file_input_stream_real_query_info_finish;
}

static void
g_file_input_stream_seekable_iface_init (GSeekableIface *iface)
{
  iface->tell = g_file_input_stream_seekable_tell;
  iface->can_seek = g_file_input_stream_seekable_can_seek;
  iface->seek = g_file_input_stream_seekable_seek;
  iface->can_truncate = g_file_input_stream_seekable_can_truncate;
  iface->truncate_fn = g_file_input_stream_seekable_truncate;
}

static void
g_file_input_stream_init (GFileInputStream *stream)
{
  stream->priv = g_file_input_stream_get_instance_private (stream);
}

/**
 * g_file_input_stream_query_info:
 * @stream: a #GFileInputStream.
 * @attributes: a file attribute query string.
 * @cancellable: (nullable): optional #GCancellable object, %NULL to ignore. 
 * @error: a #GError location to store the error occurring, or %NULL to 
 * ignore.
 *
 * Queries a file input stream the given @attributes. This function blocks 
 * while querying the stream. For the asynchronous (non-blocking) version 
 * of this function, see g_file_input_stream_query_info_async(). While the 
 * stream is blocked, the stream will set the pending flag internally, and 
 * any other operations on the stream will fail with %G_IO_ERROR_PENDING.
 *
 * Returns: (transfer full): a #GFileInfo, or %NULL on error.
 **/
GFileInfo *
g_file_input_stream_query_info (GFileInputStream  *stream,
                                const char        *attributes,
                                GCancellable      *cancellable,
                                GError           **error)
{
  GFileInputStreamClass *class;
  GInputStream *input_stream;
  GFileInfo *info;
  
  g_return_val_if_fail (G_IS_FILE_INPUT_STREAM (stream), NULL);
  
  input_stream = G_INPUT_STREAM (stream);
  
  if (!g_input_stream_set_pending (input_stream, error))
    return NULL;
      
  info = NULL;
  
  if (cancellable)
    g_cancellable_push_current (cancellable);
  
  class = G_FILE_INPUT_STREAM_GET_CLASS (stream);
  if (class->query_info)
    info = class->query_info (stream, attributes, cancellable, error);
  else
    g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                         _("Stream doesn’t support query_info"));

  if (cancellable)
    g_cancellable_pop_current (cancellable);
  
  g_input_stream_clear_pending (input_stream);
  
  return info;
}

static void
async_ready_callback_wrapper (GObject      *source_object,
                              GAsyncResult *res,
                              gpointer      user_data)
{
  GFileInputStream *stream = G_FILE_INPUT_STREAM (source_object);

  g_input_stream_clear_pending (G_INPUT_STREAM (stream));
  if (stream->priv->outstanding_callback)
    (*stream->priv->outstanding_callback) (source_object, res, user_data);
  g_object_unref (stream);
}

/**
 * g_file_input_stream_query_info_async:
 * @stream: a #GFileInputStream.
 * @attributes: a file attribute query string.
 * @io_priority: the [I/O priority][io-priority] of the request
 * @cancellable: (nullable): optional #GCancellable object, %NULL to ignore. 
 * @callback: (scope async): callback to call when the request is satisfied
 * @user_data: (closure): the data to pass to callback function
 * 
 * Queries the stream information asynchronously.
 * When the operation is finished @callback will be called. 
 * You can then call g_file_input_stream_query_info_finish() 
 * to get the result of the operation.
 *
 * For the synchronous version of this function, 
 * see g_file_input_stream_query_info(). 
 * 
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be set
 *  
 **/
void
g_file_input_stream_query_info_async (GFileInputStream    *stream,
                                      const char          *attributes,
                                      int                  io_priority,
                                      GCancellable        *cancellable,
                                      GAsyncReadyCallback  callback,
                                      gpointer             user_data)
{
  GFileInputStreamClass *klass;
  GInputStream *input_stream;
  GError *error = NULL;

  g_return_if_fail (G_IS_FILE_INPUT_STREAM (stream));

  input_stream = G_INPUT_STREAM (stream);
  
  if (!g_input_stream_set_pending (input_stream, &error))
    {
      g_task_report_error (stream, callback, user_data,
                           g_file_input_stream_query_info_async,
                           error);
      return;
    }

  klass = G_FILE_INPUT_STREAM_GET_CLASS (stream);

  stream->priv->outstanding_callback = callback;
  g_object_ref (stream);
  klass->query_info_async (stream, attributes, io_priority, cancellable,
                           async_ready_callback_wrapper, user_data);
}

/**
 * g_file_input_stream_query_info_finish:
 * @stream: a #GFileInputStream.
 * @result: a #GAsyncResult.
 * @error: a #GError location to store the error occurring, 
 *     or %NULL to ignore.
 * 
 * Finishes an asynchronous info query operation.
 * 
 * Returns: (transfer full): #GFileInfo. 
 **/
GFileInfo *
g_file_input_stream_query_info_finish (GFileInputStream  *stream,
                                       GAsyncResult      *result,
                                       GError           **error)
{
  GFileInputStreamClass *class;

  g_return_val_if_fail (G_IS_FILE_INPUT_STREAM (stream), NULL);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), NULL);

  if (g_async_result_legacy_propagate_error (result, error))
    return NULL;
  else if (g_async_result_is_tagged (result, g_file_input_stream_query_info_async))
    return g_task_propagate_pointer (G_TASK (result), error);

  class = G_FILE_INPUT_STREAM_GET_CLASS (stream);
  return class->query_info_finish (stream, result, error);
}

static goffset
g_file_input_stream_tell (GFileInputStream *stream)
{
  GFileInputStreamClass *class;
  goffset offset;

  g_return_val_if_fail (G_IS_FILE_INPUT_STREAM (stream), 0);

  class = G_FILE_INPUT_STREAM_GET_CLASS (stream);

  offset = 0;
  if (class->tell)
    offset = class->tell (stream);

  return offset;
}

static goffset
g_file_input_stream_seekable_tell (GSeekable *seekable)
{
  return g_file_input_stream_tell (G_FILE_INPUT_STREAM (seekable));
}

static gboolean
g_file_input_stream_can_seek (GFileInputStream *stream)
{
  GFileInputStreamClass *class;
  gboolean can_seek;

  g_return_val_if_fail (G_IS_FILE_INPUT_STREAM (stream), FALSE);

  class = G_FILE_INPUT_STREAM_GET_CLASS (stream);

  can_seek = FALSE;
  if (class->seek)
    {
      can_seek = TRUE;
      if (class->can_seek)
	can_seek = class->can_seek (stream);
    }
  
  return can_seek;
}

static gboolean
g_file_input_stream_seekable_can_seek (GSeekable *seekable)
{
  return g_file_input_stream_can_seek (G_FILE_INPUT_STREAM (seekable));
}

static gboolean
g_file_input_stream_seek (GFileInputStream  *stream,
			  goffset            offset,
			  GSeekType          type,
			  GCancellable      *cancellable,
			  GError           **error)
{
  GFileInputStreamClass *class;
  GInputStream *input_stream;
  gboolean res;

  g_return_val_if_fail (G_IS_FILE_INPUT_STREAM (stream), FALSE);

  input_stream = G_INPUT_STREAM (stream);
  class = G_FILE_INPUT_STREAM_GET_CLASS (stream);

  if (!class->seek)
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                           _("Seek not supported on stream"));
      return FALSE;
    }

  if (!g_input_stream_set_pending (input_stream, error))
    return FALSE;
  
  if (cancellable)
    g_cancellable_push_current (cancellable);
  
  res = class->seek (stream, offset, type, cancellable, error);
  
  if (cancellable)
    g_cancellable_pop_current (cancellable);

  g_input_stream_clear_pending (input_stream);
  
  return res;
}

static gboolean
g_file_input_stream_seekable_seek (GSeekable     *seekable,
				   goffset        offset,
				   GSeekType      type,
				   GCancellable  *cancellable,
				   GError       **error)
{
  return g_file_input_stream_seek (G_FILE_INPUT_STREAM (seekable),
				   offset, type, cancellable, error);
}

static gboolean
g_file_input_stream_seekable_can_truncate (GSeekable *seekable)
{
  return FALSE;
}

static gboolean
g_file_input_stream_seekable_truncate (GSeekable     *seekable,
				       goffset        offset,
				       GCancellable  *cancellable,
				       GError       **error)
{
  g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                       _("Truncate not allowed on input stream"));
  return FALSE;
}

/********************************************
 *   Default implementation of async ops    *
 ********************************************/

static void
query_info_async_thread (GTask        *task,
                         gpointer      source_object,
                         gpointer      task_data,
                         GCancellable *cancellable)
{
  GFileInputStream *stream = source_object;
  const char *attributes = task_data;
  GFileInputStreamClass *class;
  GError *error = NULL;
  GFileInfo *info = NULL;

  class = G_FILE_INPUT_STREAM_GET_CLASS (stream);
  if (class->query_info)
    info = class->query_info (stream, attributes, cancellable, &error);
  else
    g_set_error_literal (&error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                         _("Stream doesn’t support query_info"));

  if (info == NULL)
    g_task_return_error (task, error);
  else
    g_task_return_pointer (task, info, g_object_unref);
}

static void
g_file_input_stream_real_query_info_async (GFileInputStream    *stream,
                                           const char          *attributes,
                                           int                  io_priority,
                                           GCancellable        *cancellable,
                                           GAsyncReadyCallback  callback,
                                           gpointer             user_data)
{
  GTask *task;

  task = g_task_new (stream, cancellable, callback, user_data);
  g_task_set_source_tag (task, g_file_input_stream_real_query_info_async);
  g_task_set_task_data (task, g_strdup (attributes), g_free);
  g_task_set_priority (task, io_priority);
  
  g_task_run_in_thread (task, query_info_async_thread);
  g_object_unref (task);
}

static GFileInfo *
g_file_input_stream_real_query_info_finish (GFileInputStream  *stream,
                                            GAsyncResult      *res,
                                            GError           **error)
{
  g_return_val_if_fail (g_task_is_valid (res, stream), NULL);

  return g_task_propagate_pointer (G_TASK (res), error);
}
