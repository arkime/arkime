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
#include <gfileoutputstream.h>
#include <gseekable.h>
#include "gasyncresult.h"
#include "gtask.h"
#include "gcancellable.h"
#include "gioerror.h"
#include "glibintl.h"


/**
 * SECTION:gfileoutputstream
 * @short_description: File output streaming operations
 * @include: gio/gio.h
 * @see_also: #GOutputStream, #GDataOutputStream, #GSeekable
 * 
 * GFileOutputStream provides output streams that write their
 * content to a file.
 *
 * GFileOutputStream implements #GSeekable, which allows the output 
 * stream to jump to arbitrary positions in the file and to truncate
 * the file, provided the filesystem of the file supports these 
 * operations.
 *
 * To find the position of a file output stream, use g_seekable_tell().
 * To find out if a file output stream supports seeking, use
 * g_seekable_can_seek().To position a file output stream, use
 * g_seekable_seek(). To find out if a file output stream supports
 * truncating, use g_seekable_can_truncate(). To truncate a file output
 * stream, use g_seekable_truncate().
 **/

static void       g_file_output_stream_seekable_iface_init    (GSeekableIface       *iface);
static goffset    g_file_output_stream_seekable_tell          (GSeekable            *seekable);
static gboolean   g_file_output_stream_seekable_can_seek      (GSeekable            *seekable);
static gboolean   g_file_output_stream_seekable_seek          (GSeekable            *seekable,
							       goffset               offset,
							       GSeekType             type,
							       GCancellable         *cancellable,
							       GError              **error);
static gboolean   g_file_output_stream_seekable_can_truncate  (GSeekable            *seekable);
static gboolean   g_file_output_stream_seekable_truncate      (GSeekable            *seekable,
							       goffset               offset,
							       GCancellable         *cancellable,
							       GError              **error);
static void       g_file_output_stream_real_query_info_async  (GFileOutputStream    *stream,
							       const char           *attributes,
							       int                   io_priority,
							       GCancellable         *cancellable,
							       GAsyncReadyCallback   callback,
							       gpointer              user_data);
static GFileInfo *g_file_output_stream_real_query_info_finish (GFileOutputStream    *stream,
							       GAsyncResult         *result,
							       GError              **error);

struct _GFileOutputStreamPrivate {
  GAsyncReadyCallback outstanding_callback;
};

G_DEFINE_TYPE_WITH_CODE (GFileOutputStream, g_file_output_stream, G_TYPE_OUTPUT_STREAM,
                         G_ADD_PRIVATE (GFileOutputStream)
			 G_IMPLEMENT_INTERFACE (G_TYPE_SEEKABLE,
						g_file_output_stream_seekable_iface_init));

static void
g_file_output_stream_class_init (GFileOutputStreamClass *klass)
{
  klass->query_info_async = g_file_output_stream_real_query_info_async;
  klass->query_info_finish = g_file_output_stream_real_query_info_finish;
}

static void
g_file_output_stream_seekable_iface_init (GSeekableIface *iface)
{
  iface->tell = g_file_output_stream_seekable_tell;
  iface->can_seek = g_file_output_stream_seekable_can_seek;
  iface->seek = g_file_output_stream_seekable_seek;
  iface->can_truncate = g_file_output_stream_seekable_can_truncate;
  iface->truncate_fn = g_file_output_stream_seekable_truncate;
}

static void
g_file_output_stream_init (GFileOutputStream *stream)
{
  stream->priv = g_file_output_stream_get_instance_private (stream);
}

/**
 * g_file_output_stream_query_info:
 * @stream: a #GFileOutputStream.
 * @attributes: a file attribute query string.
 * @cancellable: optional #GCancellable object, %NULL to ignore. 
 * @error: a #GError, %NULL to ignore.
 *
 * Queries a file output stream for the given @attributes. 
 * This function blocks while querying the stream. For the asynchronous 
 * version of this function, see g_file_output_stream_query_info_async(). 
 * While the stream is blocked, the stream will set the pending flag 
 * internally, and any other operations on the stream will fail with 
 * %G_IO_ERROR_PENDING.
 * 
 * Can fail if the stream was already closed (with @error being set to 
 * %G_IO_ERROR_CLOSED), the stream has pending operations (with @error being
 * set to %G_IO_ERROR_PENDING), or if querying info is not supported for 
 * the stream's interface (with @error being set to %G_IO_ERROR_NOT_SUPPORTED). In
 * all cases of failure, %NULL will be returned.
 * 
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be set, and %NULL will 
 * be returned. 
 * 
 * Returns: (transfer full): a #GFileInfo for the @stream, or %NULL on error.
 **/
GFileInfo *
g_file_output_stream_query_info (GFileOutputStream      *stream,
				    const char             *attributes,
				    GCancellable           *cancellable,
				    GError                **error)
{
  GFileOutputStreamClass *class;
  GOutputStream *output_stream;
  GFileInfo *info;
  
  g_return_val_if_fail (G_IS_FILE_OUTPUT_STREAM (stream), NULL);
  
  output_stream = G_OUTPUT_STREAM (stream);
  
  if (!g_output_stream_set_pending (output_stream, error))
    return NULL;
      
  info = NULL;
  
  if (cancellable)
    g_cancellable_push_current (cancellable);
  
  class = G_FILE_OUTPUT_STREAM_GET_CLASS (stream);
  if (class->query_info)
    info = class->query_info (stream, attributes, cancellable, error);
  else
    g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                         _("Stream doesn’t support query_info"));
  
  if (cancellable)
    g_cancellable_pop_current (cancellable);
  
  g_output_stream_clear_pending (output_stream);
  
  return info;
}

static void
async_ready_callback_wrapper (GObject *source_object,
			      GAsyncResult *res,
			      gpointer      user_data)
{
  GFileOutputStream *stream = G_FILE_OUTPUT_STREAM (source_object);

  g_output_stream_clear_pending (G_OUTPUT_STREAM (stream));
  if (stream->priv->outstanding_callback)
    (*stream->priv->outstanding_callback) (source_object, res, user_data);
  g_object_unref (stream);
}

/**
 * g_file_output_stream_query_info_async:
 * @stream: a #GFileOutputStream.
 * @attributes: a file attribute query string.
 * @io_priority: the [I/O priority][gio-GIOScheduler] of the request
 * @cancellable: optional #GCancellable object, %NULL to ignore. 
 * @callback: callback to call when the request is satisfied
 * @user_data: the data to pass to callback function
 * 
 * Asynchronously queries the @stream for a #GFileInfo. When completed,
 * @callback will be called with a #GAsyncResult which can be used to 
 * finish the operation with g_file_output_stream_query_info_finish().
 * 
 * For the synchronous version of this function, see 
 * g_file_output_stream_query_info().
 *
 **/
void
g_file_output_stream_query_info_async (GFileOutputStream     *stream,
					  const char           *attributes,
					  int                   io_priority,
					  GCancellable         *cancellable,
					  GAsyncReadyCallback   callback,
					  gpointer              user_data)
{
  GFileOutputStreamClass *klass;
  GOutputStream *output_stream;
  GError *error = NULL;

  g_return_if_fail (G_IS_FILE_OUTPUT_STREAM (stream));

  output_stream = G_OUTPUT_STREAM (stream);
 
  if (!g_output_stream_set_pending (output_stream, &error))
    {
      g_task_report_error (stream, callback, user_data,
                           g_file_output_stream_query_info_async,
                           error);
      return;
    }

  klass = G_FILE_OUTPUT_STREAM_GET_CLASS (stream);

  stream->priv->outstanding_callback = callback;
  g_object_ref (stream);
  klass->query_info_async (stream, attributes, io_priority, cancellable,
                           async_ready_callback_wrapper, user_data);
}

/**
 * g_file_output_stream_query_info_finish:
 * @stream: a #GFileOutputStream.
 * @result: a #GAsyncResult.
 * @error: a #GError, %NULL to ignore.
 * 
 * Finalizes the asynchronous query started 
 * by g_file_output_stream_query_info_async().
 * 
 * Returns: (transfer full): A #GFileInfo for the finished query.
 **/
GFileInfo *
g_file_output_stream_query_info_finish (GFileOutputStream     *stream,
					   GAsyncResult         *result,
					   GError              **error)
{
  GFileOutputStreamClass *class;

  g_return_val_if_fail (G_IS_FILE_OUTPUT_STREAM (stream), NULL);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), NULL);
  
  if (g_async_result_legacy_propagate_error (result, error))
    return NULL;
  else if (g_async_result_is_tagged (result, g_file_output_stream_query_info_async))
    return g_task_propagate_pointer (G_TASK (result), error);

  class = G_FILE_OUTPUT_STREAM_GET_CLASS (stream);
  return class->query_info_finish (stream, result, error);
}

/**
 * g_file_output_stream_get_etag:
 * @stream: a #GFileOutputStream.
 * 
 * Gets the entity tag for the file when it has been written.
 * This must be called after the stream has been written
 * and closed, as the etag can change while writing.
 * 
 * Returns: the entity tag for the stream.
 **/
char *
g_file_output_stream_get_etag (GFileOutputStream  *stream)
{
  GFileOutputStreamClass *class;
  GOutputStream *output_stream;
  char *etag;
  
  g_return_val_if_fail (G_IS_FILE_OUTPUT_STREAM (stream), NULL);
  
  output_stream = G_OUTPUT_STREAM (stream);
  
  if (!g_output_stream_is_closed (output_stream))
    {
      g_warning ("stream is not closed yet, can't get etag");
      return NULL;
    }

  etag = NULL;
  
  class = G_FILE_OUTPUT_STREAM_GET_CLASS (stream);
  if (class->get_etag)
    etag = class->get_etag (stream);
  
  return etag;
}

static goffset
g_file_output_stream_tell (GFileOutputStream  *stream)
{
  GFileOutputStreamClass *class;
  goffset offset;
  
  g_return_val_if_fail (G_IS_FILE_OUTPUT_STREAM (stream), 0);  

  class = G_FILE_OUTPUT_STREAM_GET_CLASS (stream);

  offset = 0;
  if (class->tell)
    offset = class->tell (stream);

  return offset;
}

static goffset
g_file_output_stream_seekable_tell (GSeekable *seekable)
{
  return g_file_output_stream_tell (G_FILE_OUTPUT_STREAM (seekable));
}

static gboolean
g_file_output_stream_can_seek (GFileOutputStream  *stream)
{
  GFileOutputStreamClass *class;
  gboolean can_seek;

  g_return_val_if_fail (G_IS_FILE_OUTPUT_STREAM (stream), FALSE);

  class = G_FILE_OUTPUT_STREAM_GET_CLASS (stream);

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
g_file_output_stream_seekable_can_seek (GSeekable *seekable)
{
  return g_file_output_stream_can_seek (G_FILE_OUTPUT_STREAM (seekable));
}

static gboolean
g_file_output_stream_seek (GFileOutputStream  *stream,
			   goffset             offset,
			   GSeekType           type,
			   GCancellable       *cancellable,
			   GError            **error)
{
  GFileOutputStreamClass *class;
  GOutputStream *output_stream;
  gboolean res;

  g_return_val_if_fail (G_IS_FILE_OUTPUT_STREAM (stream), FALSE);

  output_stream = G_OUTPUT_STREAM (stream);
  class = G_FILE_OUTPUT_STREAM_GET_CLASS (stream);

  if (!class->seek)
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                           _("Seek not supported on stream"));
      return FALSE;
    }

  if (!g_output_stream_set_pending (output_stream, error))
    return FALSE;
  
  if (cancellable)
    g_cancellable_push_current (cancellable);
  
  res = class->seek (stream, offset, type, cancellable, error);
  
  if (cancellable)
    g_cancellable_pop_current (cancellable);

  g_output_stream_clear_pending (output_stream);
  
  return res;
}

static gboolean
g_file_output_stream_seekable_seek (GSeekable  *seekable,
				    goffset     offset,
				    GSeekType   type,
				    GCancellable  *cancellable,
				    GError    **error)
{
  return g_file_output_stream_seek (G_FILE_OUTPUT_STREAM (seekable),
				    offset, type, cancellable, error);
}

static gboolean
g_file_output_stream_can_truncate (GFileOutputStream  *stream)
{
  GFileOutputStreamClass *class;
  gboolean can_truncate;

  g_return_val_if_fail (G_IS_FILE_OUTPUT_STREAM (stream), FALSE);

  class = G_FILE_OUTPUT_STREAM_GET_CLASS (stream);

  can_truncate = FALSE;
  if (class->truncate_fn)
    {
      can_truncate = TRUE;
      if (class->can_truncate)
	can_truncate = class->can_truncate (stream);
    }
  
  return can_truncate;
}

static gboolean
g_file_output_stream_seekable_can_truncate (GSeekable  *seekable)
{
  return g_file_output_stream_can_truncate (G_FILE_OUTPUT_STREAM (seekable));
}

static gboolean
g_file_output_stream_truncate (GFileOutputStream  *stream,
			       goffset             size,
			       GCancellable       *cancellable,
			       GError            **error)
{
  GFileOutputStreamClass *class;
  GOutputStream *output_stream;
  gboolean res;

  g_return_val_if_fail (G_IS_FILE_OUTPUT_STREAM (stream), FALSE);

  output_stream = G_OUTPUT_STREAM (stream);
  class = G_FILE_OUTPUT_STREAM_GET_CLASS (stream);

  if (!class->truncate_fn)
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                           _("Truncate not supported on stream"));
      return FALSE;
    }

  if (!g_output_stream_set_pending (output_stream, error))
    return FALSE;
  
  if (cancellable)
    g_cancellable_push_current (cancellable);
  
  res = class->truncate_fn (stream, size, cancellable, error);
  
  if (cancellable)
    g_cancellable_pop_current (cancellable);

  g_output_stream_clear_pending (output_stream);
  
  return res;
}

static gboolean
g_file_output_stream_seekable_truncate (GSeekable     *seekable,
					goffset        size,
					GCancellable  *cancellable,
					GError       **error)
{
  return g_file_output_stream_truncate (G_FILE_OUTPUT_STREAM (seekable),
					size, cancellable, error);
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
  GFileOutputStream *stream = source_object;
  const char *attributes = task_data;
  GFileOutputStreamClass *class;
  GError *error = NULL;
  GFileInfo *info = NULL;

  class = G_FILE_OUTPUT_STREAM_GET_CLASS (stream);
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
g_file_output_stream_real_query_info_async (GFileOutputStream     *stream,
					       const char           *attributes,
					       int                   io_priority,
					       GCancellable         *cancellable,
					       GAsyncReadyCallback   callback,
					       gpointer              user_data)
{
  GTask *task;

  task = g_task_new (stream, cancellable, callback, user_data);
  g_task_set_source_tag (task, g_file_output_stream_real_query_info_async);
  g_task_set_task_data (task, g_strdup (attributes), g_free);
  g_task_set_priority (task, io_priority);
  
  g_task_run_in_thread (task, query_info_async_thread);
  g_object_unref (task);
}

static GFileInfo *
g_file_output_stream_real_query_info_finish (GFileOutputStream     *stream,
					     GAsyncResult         *res,
					     GError              **error)
{
  g_return_val_if_fail (g_task_is_valid (res, stream), NULL);

  return g_task_propagate_pointer (G_TASK (res), error);
}
