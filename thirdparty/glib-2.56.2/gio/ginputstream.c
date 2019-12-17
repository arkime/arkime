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
#include "glibintl.h"

#include "ginputstream.h"
#include "gioprivate.h"
#include "gseekable.h"
#include "gcancellable.h"
#include "gasyncresult.h"
#include "gioerror.h"
#include "gpollableinputstream.h"

/**
 * SECTION:ginputstream
 * @short_description: Base class for implementing streaming input
 * @include: gio/gio.h
 *
 * #GInputStream has functions to read from a stream (g_input_stream_read()),
 * to close a stream (g_input_stream_close()) and to skip some content
 * (g_input_stream_skip()). 
 *
 * To copy the content of an input stream to an output stream without 
 * manually handling the reads and writes, use g_output_stream_splice().
 *
 * See the documentation for #GIOStream for details of thread safety of
 * streaming APIs.
 *
 * All of these functions have async variants too.
 **/

struct _GInputStreamPrivate {
  guint closed : 1;
  guint pending : 1;
  GAsyncReadyCallback outstanding_callback;
};

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (GInputStream, g_input_stream, G_TYPE_OBJECT)

static gssize   g_input_stream_real_skip         (GInputStream         *stream,
						  gsize                 count,
						  GCancellable         *cancellable,
						  GError              **error);
static void     g_input_stream_real_read_async   (GInputStream         *stream,
						  void                 *buffer,
						  gsize                 count,
						  int                   io_priority,
						  GCancellable         *cancellable,
						  GAsyncReadyCallback   callback,
						  gpointer              user_data);
static gssize   g_input_stream_real_read_finish  (GInputStream         *stream,
						  GAsyncResult         *result,
						  GError              **error);
static void     g_input_stream_real_skip_async   (GInputStream         *stream,
						  gsize                 count,
						  int                   io_priority,
						  GCancellable         *cancellable,
						  GAsyncReadyCallback   callback,
						  gpointer              data);
static gssize   g_input_stream_real_skip_finish  (GInputStream         *stream,
						  GAsyncResult         *result,
						  GError              **error);
static void     g_input_stream_real_close_async  (GInputStream         *stream,
						  int                   io_priority,
						  GCancellable         *cancellable,
						  GAsyncReadyCallback   callback,
						  gpointer              data);
static gboolean g_input_stream_real_close_finish (GInputStream         *stream,
						  GAsyncResult         *result,
						  GError              **error);

static void
g_input_stream_dispose (GObject *object)
{
  GInputStream *stream;

  stream = G_INPUT_STREAM (object);
  
  if (!stream->priv->closed)
    g_input_stream_close (stream, NULL, NULL);

  G_OBJECT_CLASS (g_input_stream_parent_class)->dispose (object);
}


static void
g_input_stream_class_init (GInputStreamClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  
  gobject_class->dispose = g_input_stream_dispose;
  
  klass->skip = g_input_stream_real_skip;
  klass->read_async = g_input_stream_real_read_async;
  klass->read_finish = g_input_stream_real_read_finish;
  klass->skip_async = g_input_stream_real_skip_async;
  klass->skip_finish = g_input_stream_real_skip_finish;
  klass->close_async = g_input_stream_real_close_async;
  klass->close_finish = g_input_stream_real_close_finish;
}

static void
g_input_stream_init (GInputStream *stream)
{
  stream->priv = g_input_stream_get_instance_private (stream);
}

/**
 * g_input_stream_read:
 * @stream: a #GInputStream.
 * @buffer: (array length=count) (element-type guint8): a buffer to
 *     read data into (which should be at least count bytes long).
 * @count: the number of bytes that will be read from the stream
 * @cancellable: (nullable): optional #GCancellable object, %NULL to ignore.
 * @error: location to store the error occurring, or %NULL to ignore
 *
 * Tries to read @count bytes from the stream into the buffer starting at
 * @buffer. Will block during this read.
 * 
 * If count is zero returns zero and does nothing. A value of @count
 * larger than %G_MAXSSIZE will cause a %G_IO_ERROR_INVALID_ARGUMENT error.
 *
 * On success, the number of bytes read into the buffer is returned.
 * It is not an error if this is not the same as the requested size, as it
 * can happen e.g. near the end of a file. Zero is returned on end of file
 * (or if @count is zero),  but never otherwise.
 *
 * The returned @buffer is not a nul-terminated string, it can contain nul bytes
 * at any position, and this function doesn't nul-terminate the @buffer.
 *
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. If an
 * operation was partially finished when the operation was cancelled the
 * partial result will be returned, without an error.
 *
 * On error -1 is returned and @error is set accordingly.
 * 
 * Returns: Number of bytes read, or -1 on error, or 0 on end of file.
 **/
gssize
g_input_stream_read  (GInputStream  *stream,
		      void          *buffer,
		      gsize          count,
		      GCancellable  *cancellable,
		      GError       **error)
{
  GInputStreamClass *class;
  gssize res;

  g_return_val_if_fail (G_IS_INPUT_STREAM (stream), -1);
  g_return_val_if_fail (buffer != NULL, 0);

  if (count == 0)
    return 0;
  
  if (((gssize) count) < 0)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
		   _("Too large count value passed to %s"), G_STRFUNC);
      return -1;
    }

  class = G_INPUT_STREAM_GET_CLASS (stream);

  if (class->read_fn == NULL) 
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                           _("Input stream doesn’t implement read"));
      return -1;
    }

  if (!g_input_stream_set_pending (stream, error))
    return -1;

  if (cancellable)
    g_cancellable_push_current (cancellable);
  
  res = class->read_fn (stream, buffer, count, cancellable, error);

  if (cancellable)
    g_cancellable_pop_current (cancellable);
  
  g_input_stream_clear_pending (stream);

  return res;
}

/**
 * g_input_stream_read_all:
 * @stream: a #GInputStream.
 * @buffer: (array length=count) (element-type guint8): a buffer to
 *     read data into (which should be at least count bytes long).
 * @count: the number of bytes that will be read from the stream
 * @bytes_read: (out): location to store the number of bytes that was read from the stream
 * @cancellable: (nullable): optional #GCancellable object, %NULL to ignore.
 * @error: location to store the error occurring, or %NULL to ignore
 *
 * Tries to read @count bytes from the stream into the buffer starting at
 * @buffer. Will block during this read.
 *
 * This function is similar to g_input_stream_read(), except it tries to
 * read as many bytes as requested, only stopping on an error or end of stream.
 *
 * On a successful read of @count bytes, or if we reached the end of the
 * stream,  %TRUE is returned, and @bytes_read is set to the number of bytes
 * read into @buffer.
 * 
 * If there is an error during the operation %FALSE is returned and @error
 * is set to indicate the error status.
 *
 * As a special exception to the normal conventions for functions that
 * use #GError, if this function returns %FALSE (and sets @error) then
 * @bytes_read will be set to the number of bytes that were successfully
 * read before the error was encountered.  This functionality is only
 * available from C.  If you need it from another language then you must
 * write your own loop around g_input_stream_read().
 *
 * Returns: %TRUE on success, %FALSE if there was an error
 **/
gboolean
g_input_stream_read_all (GInputStream  *stream,
			 void          *buffer,
			 gsize          count,
			 gsize         *bytes_read,
			 GCancellable  *cancellable,
			 GError       **error)
{
  gsize _bytes_read;
  gssize res;

  g_return_val_if_fail (G_IS_INPUT_STREAM (stream), FALSE);
  g_return_val_if_fail (buffer != NULL, FALSE);

  _bytes_read = 0;
  while (_bytes_read < count)
    {
      res = g_input_stream_read (stream, (char *)buffer + _bytes_read, count - _bytes_read,
				 cancellable, error);
      if (res == -1)
	{
	  if (bytes_read)
	    *bytes_read = _bytes_read;
	  return FALSE;
	}
      
      if (res == 0)
	break;

      _bytes_read += res;
    }

  if (bytes_read)
    *bytes_read = _bytes_read;
  return TRUE;
}

/**
 * g_input_stream_read_bytes:
 * @stream: a #GInputStream.
 * @count: maximum number of bytes that will be read from the stream. Common
 * values include 4096 and 8192.
 * @cancellable: (nullable): optional #GCancellable object, %NULL to ignore.
 * @error: location to store the error occurring, or %NULL to ignore
 *
 * Like g_input_stream_read(), this tries to read @count bytes from
 * the stream in a blocking fashion. However, rather than reading into
 * a user-supplied buffer, this will create a new #GBytes containing
 * the data that was read. This may be easier to use from language
 * bindings.
 *
 * If count is zero, returns a zero-length #GBytes and does nothing. A
 * value of @count larger than %G_MAXSSIZE will cause a
 * %G_IO_ERROR_INVALID_ARGUMENT error.
 *
 * On success, a new #GBytes is returned. It is not an error if the
 * size of this object is not the same as the requested size, as it
 * can happen e.g. near the end of a file. A zero-length #GBytes is
 * returned on end of file (or if @count is zero), but never
 * otherwise.
 *
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. If an
 * operation was partially finished when the operation was cancelled the
 * partial result will be returned, without an error.
 *
 * On error %NULL is returned and @error is set accordingly.
 *
 * Returns: (transfer full): a new #GBytes, or %NULL on error
 *
 * Since: 2.34
 **/
GBytes *
g_input_stream_read_bytes (GInputStream  *stream,
			   gsize          count,
			   GCancellable  *cancellable,
			   GError       **error)
{
  guchar *buf;
  gssize nread;

  buf = g_malloc (count);
  nread = g_input_stream_read (stream, buf, count, cancellable, error);
  if (nread == -1)
    {
      g_free (buf);
      return NULL;
    }
  else if (nread == 0)
    {
      g_free (buf);
      return g_bytes_new_static ("", 0);
    }
  else
    return g_bytes_new_take (buf, nread);
}

/**
 * g_input_stream_skip:
 * @stream: a #GInputStream.
 * @count: the number of bytes that will be skipped from the stream
 * @cancellable: (nullable): optional #GCancellable object, %NULL to ignore. 
 * @error: location to store the error occurring, or %NULL to ignore
 *
 * Tries to skip @count bytes from the stream. Will block during the operation.
 *
 * This is identical to g_input_stream_read(), from a behaviour standpoint,
 * but the bytes that are skipped are not returned to the user. Some
 * streams have an implementation that is more efficient than reading the data.
 *
 * This function is optional for inherited classes, as the default implementation
 * emulates it using read.
 *
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. If an
 * operation was partially finished when the operation was cancelled the
 * partial result will be returned, without an error.
 *
 * Returns: Number of bytes skipped, or -1 on error
 **/
gssize
g_input_stream_skip (GInputStream  *stream,
		     gsize          count,
		     GCancellable  *cancellable,
		     GError       **error)
{
  GInputStreamClass *class;
  gssize res;

  g_return_val_if_fail (G_IS_INPUT_STREAM (stream), -1);

  if (count == 0)
    return 0;

  if (((gssize) count) < 0)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
		   _("Too large count value passed to %s"), G_STRFUNC);
      return -1;
    }
  
  class = G_INPUT_STREAM_GET_CLASS (stream);

  if (!g_input_stream_set_pending (stream, error))
    return -1;

  if (cancellable)
    g_cancellable_push_current (cancellable);
  
  res = class->skip (stream, count, cancellable, error);

  if (cancellable)
    g_cancellable_pop_current (cancellable);
  
  g_input_stream_clear_pending (stream);

  return res;
}

static gssize
g_input_stream_real_skip (GInputStream  *stream,
			  gsize          count,
			  GCancellable  *cancellable,
			  GError       **error)
{
  GInputStreamClass *class;
  gssize ret, read_bytes;
  char buffer[8192];
  GError *my_error;

  if (G_IS_SEEKABLE (stream) && g_seekable_can_seek (G_SEEKABLE (stream)))
    {
      if (g_seekable_seek (G_SEEKABLE (stream),
			   count,
			   G_SEEK_CUR,
			   cancellable,
			   NULL))
	return count;
    }

  /* If not seekable, or seek failed, fall back to reading data: */

  class = G_INPUT_STREAM_GET_CLASS (stream);

  read_bytes = 0;
  while (1)
    {
      my_error = NULL;

      ret = class->read_fn (stream, buffer, MIN (sizeof (buffer), count),
                            cancellable, &my_error);
      if (ret == -1)
	{
	  if (read_bytes > 0 &&
	      my_error->domain == G_IO_ERROR &&
	      my_error->code == G_IO_ERROR_CANCELLED)
	    {
	      g_error_free (my_error);
	      return read_bytes;
	    }

	  g_propagate_error (error, my_error);
	  return -1;
	}

      count -= ret;
      read_bytes += ret;

      if (ret == 0 || count == 0)
        return read_bytes;
    }
}

/**
 * g_input_stream_close:
 * @stream: A #GInputStream.
 * @cancellable: (nullable): optional #GCancellable object, %NULL to ignore.
 * @error: location to store the error occurring, or %NULL to ignore
 *
 * Closes the stream, releasing resources related to it.
 *
 * Once the stream is closed, all other operations will return %G_IO_ERROR_CLOSED.
 * Closing a stream multiple times will not return an error.
 *
 * Streams will be automatically closed when the last reference
 * is dropped, but you might want to call this function to make sure 
 * resources are released as early as possible.
 *
 * Some streams might keep the backing store of the stream (e.g. a file descriptor)
 * open after the stream is closed. See the documentation for the individual
 * stream for details.
 *
 * On failure the first error that happened will be reported, but the close
 * operation will finish as much as possible. A stream that failed to
 * close will still return %G_IO_ERROR_CLOSED for all operations. Still, it
 * is important to check and report the error to the user.
 *
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned.
 * Cancelling a close will still leave the stream closed, but some streams
 * can use a faster close that doesn't block to e.g. check errors. 
 *
 * Returns: %TRUE on success, %FALSE on failure
 **/
gboolean
g_input_stream_close (GInputStream  *stream,
		      GCancellable  *cancellable,
		      GError       **error)
{
  GInputStreamClass *class;
  gboolean res;

  g_return_val_if_fail (G_IS_INPUT_STREAM (stream), FALSE);

  class = G_INPUT_STREAM_GET_CLASS (stream);

  if (stream->priv->closed)
    return TRUE;

  res = TRUE;

  if (!g_input_stream_set_pending (stream, error))
    return FALSE;

  if (cancellable)
    g_cancellable_push_current (cancellable);

  if (class->close_fn)
    res = class->close_fn (stream, cancellable, error);

  if (cancellable)
    g_cancellable_pop_current (cancellable);

  g_input_stream_clear_pending (stream);
  
  stream->priv->closed = TRUE;
  
  return res;
}

static void
async_ready_callback_wrapper (GObject      *source_object,
			      GAsyncResult *res,
			      gpointer      user_data)
{
  GInputStream *stream = G_INPUT_STREAM (source_object);

  g_input_stream_clear_pending (stream);
  if (stream->priv->outstanding_callback)
    (*stream->priv->outstanding_callback) (source_object, res, user_data);
  g_object_unref (stream);
}

static void
async_ready_close_callback_wrapper (GObject      *source_object,
				    GAsyncResult *res,
				    gpointer      user_data)
{
  GInputStream *stream = G_INPUT_STREAM (source_object);

  g_input_stream_clear_pending (stream);
  stream->priv->closed = TRUE;
  if (stream->priv->outstanding_callback)
    (*stream->priv->outstanding_callback) (source_object, res, user_data);
  g_object_unref (stream);
}

/**
 * g_input_stream_read_async:
 * @stream: A #GInputStream.
 * @buffer: (array length=count) (element-type guint8): a buffer to
 *     read data into (which should be at least count bytes long).
 * @count: the number of bytes that will be read from the stream
 * @io_priority: the [I/O priority][io-priority]
 * of the request. 
 * @cancellable: (nullable): optional #GCancellable object, %NULL to ignore.
 * @callback: (scope async): callback to call when the request is satisfied
 * @user_data: (closure): the data to pass to callback function
 *
 * Request an asynchronous read of @count bytes from the stream into the buffer
 * starting at @buffer. When the operation is finished @callback will be called. 
 * You can then call g_input_stream_read_finish() to get the result of the 
 * operation.
 *
 * During an async request no other sync and async calls are allowed on @stream, and will
 * result in %G_IO_ERROR_PENDING errors. 
 *
 * A value of @count larger than %G_MAXSSIZE will cause a %G_IO_ERROR_INVALID_ARGUMENT error.
 *
 * On success, the number of bytes read into the buffer will be passed to the
 * callback. It is not an error if this is not the same as the requested size, as it
 * can happen e.g. near the end of a file, but generally we try to read
 * as many bytes as requested. Zero is returned on end of file
 * (or if @count is zero),  but never otherwise.
 *
 * Any outstanding i/o request with higher priority (lower numerical value) will
 * be executed before an outstanding request with lower priority. Default
 * priority is %G_PRIORITY_DEFAULT.
 *
 * The asynchronous methods have a default fallback that uses threads to implement
 * asynchronicity, so they are optional for inheriting classes. However, if you
 * override one you must override all.
 **/
void
g_input_stream_read_async (GInputStream        *stream,
			   void                *buffer,
			   gsize                count,
			   int                  io_priority,
			   GCancellable        *cancellable,
			   GAsyncReadyCallback  callback,
			   gpointer             user_data)
{
  GInputStreamClass *class;
  GError *error = NULL;

  g_return_if_fail (G_IS_INPUT_STREAM (stream));
  g_return_if_fail (buffer != NULL);

  if (count == 0)
    {
      GTask *task;

      task = g_task_new (stream, cancellable, callback, user_data);
      g_task_set_source_tag (task, g_input_stream_read_async);
      g_task_return_int (task, 0);
      g_object_unref (task);
      return;
    }
  
  if (((gssize) count) < 0)
    {
      g_task_report_new_error (stream, callback, user_data,
                               g_input_stream_read_async,
                               G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                               _("Too large count value passed to %s"),
                               G_STRFUNC);
      return;
    }

  if (!g_input_stream_set_pending (stream, &error))
    {
      g_task_report_error (stream, callback, user_data,
                           g_input_stream_read_async,
                           error);
      return;
    }

  class = G_INPUT_STREAM_GET_CLASS (stream);
  stream->priv->outstanding_callback = callback;
  g_object_ref (stream);
  class->read_async (stream, buffer, count, io_priority, cancellable,
		     async_ready_callback_wrapper, user_data);
}

/**
 * g_input_stream_read_finish:
 * @stream: a #GInputStream.
 * @result: a #GAsyncResult.
 * @error: a #GError location to store the error occurring, or %NULL to 
 * ignore.
 * 
 * Finishes an asynchronous stream read operation. 
 * 
 * Returns: number of bytes read in, or -1 on error, or 0 on end of file.
 **/
gssize
g_input_stream_read_finish (GInputStream  *stream,
			    GAsyncResult  *result,
			    GError       **error)
{
  GInputStreamClass *class;
  
  g_return_val_if_fail (G_IS_INPUT_STREAM (stream), -1);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), -1);

  if (g_async_result_legacy_propagate_error (result, error))
    return -1;
  else if (g_async_result_is_tagged (result, g_input_stream_read_async))
    return g_task_propagate_int (G_TASK (result), error);

  class = G_INPUT_STREAM_GET_CLASS (stream);
  return class->read_finish (stream, result, error);
}

typedef struct
{
  gchar *buffer;
  gsize to_read;
  gsize bytes_read;
} AsyncReadAll;

static void
free_async_read_all (gpointer data)
{
  g_slice_free (AsyncReadAll, data);
}

static void
read_all_callback (GObject      *stream,
                   GAsyncResult *result,
                   gpointer      user_data)
{
  GTask *task = user_data;
  AsyncReadAll *data = g_task_get_task_data (task);
  gboolean got_eof = FALSE;

  if (result)
    {
      GError *error = NULL;
      gssize nread;

      nread = g_input_stream_read_finish (G_INPUT_STREAM (stream), result, &error);

      if (nread == -1)
        {
          g_task_return_error (task, error);
          g_object_unref (task);
          return;
        }

      g_assert_cmpint (nread, <=, data->to_read);
      data->to_read -= nread;
      data->bytes_read += nread;
      got_eof = (nread == 0);
    }

  if (got_eof || data->to_read == 0)
    {
      g_task_return_boolean (task, TRUE);
      g_object_unref (task);
    }

  else
    g_input_stream_read_async (G_INPUT_STREAM (stream),
                               data->buffer + data->bytes_read,
                               data->to_read,
                               g_task_get_priority (task),
                               g_task_get_cancellable (task),
                               read_all_callback, task);
}


static void
read_all_async_thread (GTask        *task,
                       gpointer      source_object,
                       gpointer      task_data,
                       GCancellable *cancellable)
{
  GInputStream *stream = source_object;
  AsyncReadAll *data = task_data;
  GError *error = NULL;

  if (g_input_stream_read_all (stream, data->buffer, data->to_read, &data->bytes_read,
                               g_task_get_cancellable (task), &error))
    g_task_return_boolean (task, TRUE);
  else
    g_task_return_error (task, error);
}

/**
 * g_input_stream_read_all_async:
 * @stream: A #GInputStream
 * @buffer: (array length=count) (element-type guint8): a buffer to
 *     read data into (which should be at least count bytes long)
 * @count: the number of bytes that will be read from the stream
 * @io_priority: the [I/O priority][io-priority] of the request
 * @cancellable: (nullable): optional #GCancellable object, %NULL to ignore
 * @callback: (scope async): callback to call when the request is satisfied
 * @user_data: (closure): the data to pass to callback function
 *
 * Request an asynchronous read of @count bytes from the stream into the
 * buffer starting at @buffer.
 *
 * This is the asynchronous equivalent of g_input_stream_read_all().
 *
 * Call g_input_stream_read_all_finish() to collect the result.
 *
 * Any outstanding I/O request with higher priority (lower numerical
 * value) will be executed before an outstanding request with lower
 * priority. Default priority is %G_PRIORITY_DEFAULT.
 *
 * Since: 2.44
 **/
void
g_input_stream_read_all_async (GInputStream        *stream,
                               void                *buffer,
                               gsize                count,
                               int                  io_priority,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
  AsyncReadAll *data;
  GTask *task;

  g_return_if_fail (G_IS_INPUT_STREAM (stream));
  g_return_if_fail (buffer != NULL || count == 0);

  task = g_task_new (stream, cancellable, callback, user_data);
  data = g_slice_new0 (AsyncReadAll);
  data->buffer = buffer;
  data->to_read = count;

  g_task_set_source_tag (task, g_input_stream_read_all_async);
  g_task_set_task_data (task, data, free_async_read_all);
  g_task_set_priority (task, io_priority);

  /* If async reads are going to be handled via the threadpool anyway
   * then we may as well do it with a single dispatch instead of
   * bouncing in and out.
   */
  if (g_input_stream_async_read_is_via_threads (stream))
    {
      g_task_run_in_thread (task, read_all_async_thread);
      g_object_unref (task);
    }
  else
    read_all_callback (G_OBJECT (stream), NULL, task);
}

/**
 * g_input_stream_read_all_finish:
 * @stream: a #GInputStream
 * @result: a #GAsyncResult
 * @bytes_read: (out): location to store the number of bytes that was read from the stream
 * @error: a #GError location to store the error occurring, or %NULL to ignore
 *
 * Finishes an asynchronous stream read operation started with
 * g_input_stream_read_all_async().
 *
 * As a special exception to the normal conventions for functions that
 * use #GError, if this function returns %FALSE (and sets @error) then
 * @bytes_read will be set to the number of bytes that were successfully
 * read before the error was encountered.  This functionality is only
 * available from C.  If you need it from another language then you must
 * write your own loop around g_input_stream_read_async().
 *
 * Returns: %TRUE on success, %FALSE if there was an error
 *
 * Since: 2.44
 **/
gboolean
g_input_stream_read_all_finish (GInputStream  *stream,
                                GAsyncResult  *result,
                                gsize         *bytes_read,
                                GError       **error)
{
  GTask *task;

  g_return_val_if_fail (G_IS_INPUT_STREAM (stream), FALSE);
  g_return_val_if_fail (g_task_is_valid (result, stream), FALSE);

  task = G_TASK (result);

  if (bytes_read)
    {
      AsyncReadAll *data = g_task_get_task_data (task);

      *bytes_read = data->bytes_read;
    }

  return g_task_propagate_boolean (task, error);
}

static void
read_bytes_callback (GObject      *stream,
		     GAsyncResult *result,
		     gpointer      user_data)
{
  GTask *task = user_data;
  guchar *buf = g_task_get_task_data (task);
  GError *error = NULL;
  gssize nread;
  GBytes *bytes = NULL;

  nread = g_input_stream_read_finish (G_INPUT_STREAM (stream),
				      result, &error);
  if (nread == -1)
    {
      g_free (buf);
      g_task_return_error (task, error);
    }
  else if (nread == 0)
    {
      g_free (buf);
      bytes = g_bytes_new_static ("", 0);
    }
  else
    bytes = g_bytes_new_take (buf, nread);

  if (bytes)
    g_task_return_pointer (task, bytes, (GDestroyNotify)g_bytes_unref);

  g_object_unref (task);
}

/**
 * g_input_stream_read_bytes_async:
 * @stream: A #GInputStream.
 * @count: the number of bytes that will be read from the stream
 * @io_priority: the [I/O priority][io-priority] of the request
 * @cancellable: (nullable): optional #GCancellable object, %NULL to ignore.
 * @callback: (scope async): callback to call when the request is satisfied
 * @user_data: (closure): the data to pass to callback function
 *
 * Request an asynchronous read of @count bytes from the stream into a
 * new #GBytes. When the operation is finished @callback will be
 * called. You can then call g_input_stream_read_bytes_finish() to get the
 * result of the operation.
 *
 * During an async request no other sync and async calls are allowed
 * on @stream, and will result in %G_IO_ERROR_PENDING errors.
 *
 * A value of @count larger than %G_MAXSSIZE will cause a
 * %G_IO_ERROR_INVALID_ARGUMENT error.
 *
 * On success, the new #GBytes will be passed to the callback. It is
 * not an error if this is smaller than the requested size, as it can
 * happen e.g. near the end of a file, but generally we try to read as
 * many bytes as requested. Zero is returned on end of file (or if
 * @count is zero), but never otherwise.
 *
 * Any outstanding I/O request with higher priority (lower numerical
 * value) will be executed before an outstanding request with lower
 * priority. Default priority is %G_PRIORITY_DEFAULT.
 *
 * Since: 2.34
 **/
void
g_input_stream_read_bytes_async (GInputStream          *stream,
				 gsize                  count,
				 int                    io_priority,
				 GCancellable          *cancellable,
				 GAsyncReadyCallback    callback,
				 gpointer               user_data)
{
  GTask *task;
  guchar *buf;

  task = g_task_new (stream, cancellable, callback, user_data);
  g_task_set_source_tag (task, g_input_stream_read_bytes_async);

  buf = g_malloc (count);
  g_task_set_task_data (task, buf, NULL);

  g_input_stream_read_async (stream, buf, count,
                             io_priority, cancellable,
                             read_bytes_callback, task);
}

/**
 * g_input_stream_read_bytes_finish:
 * @stream: a #GInputStream.
 * @result: a #GAsyncResult.
 * @error: a #GError location to store the error occurring, or %NULL to
 *   ignore.
 *
 * Finishes an asynchronous stream read-into-#GBytes operation.
 *
 * Returns: (transfer full): the newly-allocated #GBytes, or %NULL on error
 *
 * Since: 2.34
 **/
GBytes *
g_input_stream_read_bytes_finish (GInputStream  *stream,
				  GAsyncResult  *result,
				  GError       **error)
{
  g_return_val_if_fail (G_IS_INPUT_STREAM (stream), NULL);
  g_return_val_if_fail (g_task_is_valid (result, stream), NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}

/**
 * g_input_stream_skip_async:
 * @stream: A #GInputStream.
 * @count: the number of bytes that will be skipped from the stream
 * @io_priority: the [I/O priority][io-priority] of the request
 * @cancellable: (nullable): optional #GCancellable object, %NULL to ignore.
 * @callback: (scope async): callback to call when the request is satisfied
 * @user_data: (closure): the data to pass to callback function
 *
 * Request an asynchronous skip of @count bytes from the stream.
 * When the operation is finished @callback will be called.
 * You can then call g_input_stream_skip_finish() to get the result
 * of the operation.
 *
 * During an async request no other sync and async calls are allowed,
 * and will result in %G_IO_ERROR_PENDING errors.
 *
 * A value of @count larger than %G_MAXSSIZE will cause a %G_IO_ERROR_INVALID_ARGUMENT error.
 *
 * On success, the number of bytes skipped will be passed to the callback.
 * It is not an error if this is not the same as the requested size, as it
 * can happen e.g. near the end of a file, but generally we try to skip
 * as many bytes as requested. Zero is returned on end of file
 * (or if @count is zero), but never otherwise.
 *
 * Any outstanding i/o request with higher priority (lower numerical value)
 * will be executed before an outstanding request with lower priority.
 * Default priority is %G_PRIORITY_DEFAULT.
 *
 * The asynchronous methods have a default fallback that uses threads to
 * implement asynchronicity, so they are optional for inheriting classes.
 * However, if you override one, you must override all.
 **/
void
g_input_stream_skip_async (GInputStream        *stream,
			   gsize                count,
			   int                  io_priority,
			   GCancellable        *cancellable,
			   GAsyncReadyCallback  callback,
			   gpointer             user_data)
{
  GInputStreamClass *class;
  GError *error = NULL;

  g_return_if_fail (G_IS_INPUT_STREAM (stream));

  if (count == 0)
    {
      GTask *task;

      task = g_task_new (stream, cancellable, callback, user_data);
      g_task_set_source_tag (task, g_input_stream_skip_async);
      g_task_return_int (task, 0);
      g_object_unref (task);
      return;
    }
  
  if (((gssize) count) < 0)
    {
      g_task_report_new_error (stream, callback, user_data,
                               g_input_stream_skip_async,
                               G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                               _("Too large count value passed to %s"),
                               G_STRFUNC);
      return;
    }

  if (!g_input_stream_set_pending (stream, &error))
    {
      g_task_report_error (stream, callback, user_data,
                           g_input_stream_skip_async,
                           error);
      return;
    }

  class = G_INPUT_STREAM_GET_CLASS (stream);
  stream->priv->outstanding_callback = callback;
  g_object_ref (stream);
  class->skip_async (stream, count, io_priority, cancellable,
		     async_ready_callback_wrapper, user_data);
}

/**
 * g_input_stream_skip_finish:
 * @stream: a #GInputStream.
 * @result: a #GAsyncResult.
 * @error: a #GError location to store the error occurring, or %NULL to 
 * ignore.
 * 
 * Finishes a stream skip operation.
 * 
 * Returns: the size of the bytes skipped, or %-1 on error.
 **/
gssize
g_input_stream_skip_finish (GInputStream  *stream,
			    GAsyncResult  *result,
			    GError       **error)
{
  GInputStreamClass *class;

  g_return_val_if_fail (G_IS_INPUT_STREAM (stream), -1);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), -1);

  if (g_async_result_legacy_propagate_error (result, error))
    return -1;
  else if (g_async_result_is_tagged (result, g_input_stream_skip_async))
    return g_task_propagate_int (G_TASK (result), error);

  class = G_INPUT_STREAM_GET_CLASS (stream);
  return class->skip_finish (stream, result, error);
}

/**
 * g_input_stream_close_async:
 * @stream: A #GInputStream.
 * @io_priority: the [I/O priority][io-priority] of the request
 * @cancellable: (nullable): optional cancellable object
 * @callback: (scope async): callback to call when the request is satisfied
 * @user_data: (closure): the data to pass to callback function
 *
 * Requests an asynchronous closes of the stream, releasing resources related to it.
 * When the operation is finished @callback will be called. 
 * You can then call g_input_stream_close_finish() to get the result of the 
 * operation.
 *
 * For behaviour details see g_input_stream_close().
 *
 * The asynchronous methods have a default fallback that uses threads to implement
 * asynchronicity, so they are optional for inheriting classes. However, if you
 * override one you must override all.
 **/
void
g_input_stream_close_async (GInputStream        *stream,
			    int                  io_priority,
			    GCancellable        *cancellable,
			    GAsyncReadyCallback  callback,
			    gpointer             user_data)
{
  GInputStreamClass *class;
  GError *error = NULL;

  g_return_if_fail (G_IS_INPUT_STREAM (stream));

  if (stream->priv->closed)
    {
      GTask *task;

      task = g_task_new (stream, cancellable, callback, user_data);
      g_task_set_source_tag (task, g_input_stream_close_async);
      g_task_return_boolean (task, TRUE);
      g_object_unref (task);
      return;
    }

  if (!g_input_stream_set_pending (stream, &error))
    {
      g_task_report_error (stream, callback, user_data,
                           g_input_stream_close_async,
                           error);
      return;
    }
  
  class = G_INPUT_STREAM_GET_CLASS (stream);
  stream->priv->outstanding_callback = callback;
  g_object_ref (stream);
  class->close_async (stream, io_priority, cancellable,
		      async_ready_close_callback_wrapper, user_data);
}

/**
 * g_input_stream_close_finish:
 * @stream: a #GInputStream.
 * @result: a #GAsyncResult.
 * @error: a #GError location to store the error occurring, or %NULL to 
 * ignore.
 * 
 * Finishes closing a stream asynchronously, started from g_input_stream_close_async().
 * 
 * Returns: %TRUE if the stream was closed successfully.
 **/
gboolean
g_input_stream_close_finish (GInputStream  *stream,
			     GAsyncResult  *result,
			     GError       **error)
{
  GInputStreamClass *class;

  g_return_val_if_fail (G_IS_INPUT_STREAM (stream), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  if (g_async_result_legacy_propagate_error (result, error))
    return FALSE;
  else if (g_async_result_is_tagged (result, g_input_stream_close_async))
    return g_task_propagate_boolean (G_TASK (result), error);

  class = G_INPUT_STREAM_GET_CLASS (stream);
  return class->close_finish (stream, result, error);
}

/**
 * g_input_stream_is_closed:
 * @stream: input stream.
 * 
 * Checks if an input stream is closed.
 * 
 * Returns: %TRUE if the stream is closed.
 **/
gboolean
g_input_stream_is_closed (GInputStream *stream)
{
  g_return_val_if_fail (G_IS_INPUT_STREAM (stream), TRUE);
  
  return stream->priv->closed;
}
 
/**
 * g_input_stream_has_pending:
 * @stream: input stream.
 * 
 * Checks if an input stream has pending actions.
 * 
 * Returns: %TRUE if @stream has pending actions.
 **/  
gboolean
g_input_stream_has_pending (GInputStream *stream)
{
  g_return_val_if_fail (G_IS_INPUT_STREAM (stream), TRUE);
  
  return stream->priv->pending;
}

/**
 * g_input_stream_set_pending:
 * @stream: input stream
 * @error: a #GError location to store the error occurring, or %NULL to 
 * ignore.
 * 
 * Sets @stream to have actions pending. If the pending flag is
 * already set or @stream is closed, it will return %FALSE and set
 * @error.
 *
 * Returns: %TRUE if pending was previously unset and is now set.
 **/
gboolean
g_input_stream_set_pending (GInputStream *stream, GError **error)
{
  g_return_val_if_fail (G_IS_INPUT_STREAM (stream), FALSE);
  
  if (stream->priv->closed)
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_CLOSED,
                           _("Stream is already closed"));
      return FALSE;
    }
  
  if (stream->priv->pending)
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_PENDING,
		/* Translators: This is an error you get if there is already an
		 * operation running against this stream when you try to start
		 * one */
		 _("Stream has outstanding operation"));
      return FALSE;
    }
  
  stream->priv->pending = TRUE;
  return TRUE;
}

/**
 * g_input_stream_clear_pending:
 * @stream: input stream
 * 
 * Clears the pending flag on @stream.
 **/
void
g_input_stream_clear_pending (GInputStream *stream)
{
  g_return_if_fail (G_IS_INPUT_STREAM (stream));
  
  stream->priv->pending = FALSE;
}

/*< internal >
 * g_input_stream_async_read_is_via_threads:
 * @stream: input stream
 *
 * Checks if an input stream's read_async function uses threads.
 *
 * Returns: %TRUE if @stream's read_async function uses threads.
 **/
gboolean
g_input_stream_async_read_is_via_threads (GInputStream *stream)
{
  GInputStreamClass *class;

  g_return_val_if_fail (G_IS_INPUT_STREAM (stream), FALSE);

  class = G_INPUT_STREAM_GET_CLASS (stream);

  return (class->read_async == g_input_stream_real_read_async &&
      !(G_IS_POLLABLE_INPUT_STREAM (stream) &&
        g_pollable_input_stream_can_poll (G_POLLABLE_INPUT_STREAM (stream))));
}

/*< internal >
 * g_input_stream_async_close_is_via_threads:
 * @stream: input stream
 *
 * Checks if an input stream's close_async function uses threads.
 *
 * Returns: %TRUE if @stream's close_async function uses threads.
 **/
gboolean
g_input_stream_async_close_is_via_threads (GInputStream *stream)
{
  GInputStreamClass *class;

  g_return_val_if_fail (G_IS_INPUT_STREAM (stream), FALSE);

  class = G_INPUT_STREAM_GET_CLASS (stream);

  return class->close_async == g_input_stream_real_close_async;
}

/********************************************
 *   Default implementation of async ops    *
 ********************************************/

typedef struct {
  void   *buffer;
  gsize   count;
} ReadData;

static void
free_read_data (ReadData *op)
{
  g_slice_free (ReadData, op);
}

static void
read_async_thread (GTask        *task,
                   gpointer      source_object,
                   gpointer      task_data,
                   GCancellable *cancellable)
{
  GInputStream *stream = source_object;
  ReadData *op = task_data;
  GInputStreamClass *class;
  GError *error = NULL;
  gssize nread;
 
  class = G_INPUT_STREAM_GET_CLASS (stream);

  nread = class->read_fn (stream,
                          op->buffer, op->count,
                          g_task_get_cancellable (task),
                          &error);
  if (nread == -1)
    g_task_return_error (task, error);
  else
    g_task_return_int (task, nread);
}

static void read_async_pollable (GPollableInputStream *stream,
                                 GTask                *task);

static gboolean
read_async_pollable_ready (GPollableInputStream *stream,
			   gpointer              user_data)
{
  GTask *task = user_data;

  read_async_pollable (stream, task);
  return FALSE;
}

static void
read_async_pollable (GPollableInputStream *stream,
                     GTask                *task)
{
  ReadData *op = g_task_get_task_data (task);
  GError *error = NULL;
  gssize nread;

  if (g_task_return_error_if_cancelled (task))
    return;

  nread = G_POLLABLE_INPUT_STREAM_GET_INTERFACE (stream)->
    read_nonblocking (stream, op->buffer, op->count, &error);

  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_WOULD_BLOCK))
    {
      GSource *source;

      g_error_free (error);

      source = g_pollable_input_stream_create_source (stream,
                                                      g_task_get_cancellable (task));
      g_task_attach_source (task, source,
                            (GSourceFunc) read_async_pollable_ready);
      g_source_unref (source);
      return;
    }

  if (nread == -1)
    g_task_return_error (task, error);
  else
    g_task_return_int (task, nread);
  /* g_input_stream_real_read_async() unrefs task */
}


static void
g_input_stream_real_read_async (GInputStream        *stream,
				void                *buffer,
				gsize                count,
				int                  io_priority,
				GCancellable        *cancellable,
				GAsyncReadyCallback  callback,
				gpointer             user_data)
{
  GTask *task;
  ReadData *op;
  
  op = g_slice_new0 (ReadData);
  task = g_task_new (stream, cancellable, callback, user_data);
  g_task_set_source_tag (task, g_input_stream_real_read_async);
  g_task_set_task_data (task, op, (GDestroyNotify) free_read_data);
  g_task_set_priority (task, io_priority);
  op->buffer = buffer;
  op->count = count;

  if (!g_input_stream_async_read_is_via_threads (stream))
    read_async_pollable (G_POLLABLE_INPUT_STREAM (stream), task);
  else
    g_task_run_in_thread (task, read_async_thread);
  g_object_unref (task);
}

static gssize
g_input_stream_real_read_finish (GInputStream  *stream,
				 GAsyncResult  *result,
				 GError       **error)
{
  g_return_val_if_fail (g_task_is_valid (result, stream), -1);

  return g_task_propagate_int (G_TASK (result), error);
}


static void
skip_async_thread (GTask        *task,
                   gpointer      source_object,
                   gpointer      task_data,
                   GCancellable *cancellable)
{
  GInputStream *stream = source_object;
  gsize count = GPOINTER_TO_SIZE (task_data);
  GInputStreamClass *class;
  GError *error = NULL;
  gssize ret;

  class = G_INPUT_STREAM_GET_CLASS (stream);
  ret = class->skip (stream, count,
                     g_task_get_cancellable (task),
                     &error);
  if (ret == -1)
    g_task_return_error (task, error);
  else
    g_task_return_int (task, ret);
}

typedef struct {
  char buffer[8192];
  gsize count;
  gsize count_skipped;
} SkipFallbackAsyncData;

static void
skip_callback_wrapper (GObject      *source_object,
		       GAsyncResult *res,
		       gpointer      user_data)
{
  GInputStreamClass *class;
  GTask *task = user_data;
  SkipFallbackAsyncData *data = g_task_get_task_data (task);
  GError *error = NULL;
  gssize ret;

  ret = g_input_stream_read_finish (G_INPUT_STREAM (source_object), res, &error);

  if (ret > 0)
    {
      data->count -= ret;
      data->count_skipped += ret;

      if (data->count > 0)
	{
	  class = G_INPUT_STREAM_GET_CLASS (source_object);
	  class->read_async (G_INPUT_STREAM (source_object),
                             data->buffer, MIN (8192, data->count),
                             g_task_get_priority (task),
                             g_task_get_cancellable (task),
                             skip_callback_wrapper, task);
	  return;
	}
    }

  if (ret == -1 &&
      g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED) &&
      data->count_skipped)
    {
      /* No error, return partial read */
      g_clear_error (&error);
    }

  if (error)
    g_task_return_error (task, error);
  else
    g_task_return_int (task, data->count_skipped);
  g_object_unref (task);
 }

static void
g_input_stream_real_skip_async (GInputStream        *stream,
				gsize                count,
				int                  io_priority,
				GCancellable        *cancellable,
				GAsyncReadyCallback  callback,
				gpointer             user_data)
{
  GInputStreamClass *class;
  SkipFallbackAsyncData *data;
  GTask *task;

  class = G_INPUT_STREAM_GET_CLASS (stream);

  task = g_task_new (stream, cancellable, callback, user_data);
  g_task_set_source_tag (task, g_input_stream_real_skip_async);
  g_task_set_priority (task, io_priority);

  if (g_input_stream_async_read_is_via_threads (stream))
    {
      /* Read is thread-using async fallback.
       * Make skip use threads too, so that we can use a possible sync skip
       * implementation. */
      g_task_set_task_data (task, GSIZE_TO_POINTER (count), NULL);

      g_task_run_in_thread (task, skip_async_thread);
      g_object_unref (task);
    }
  else
    {
      /* TODO: Skip fallback uses too much memory, should do multiple read calls */
      
      /* There is a custom async read function, lets use that. */
      data = g_new (SkipFallbackAsyncData, 1);
      data->count = count;
      data->count_skipped = 0;
      g_task_set_task_data (task, data, g_free);
      g_task_set_check_cancellable (task, FALSE);
      class->read_async (stream, data->buffer, MIN (8192, count), io_priority, cancellable,
			 skip_callback_wrapper, task);
    }

}

static gssize
g_input_stream_real_skip_finish (GInputStream  *stream,
				 GAsyncResult  *result,
				 GError       **error)
{
  g_return_val_if_fail (g_task_is_valid (result, stream), -1);

  return g_task_propagate_int (G_TASK (result), error);
}

static void
close_async_thread (GTask        *task,
                    gpointer      source_object,
                    gpointer      task_data,
                    GCancellable *cancellable)
{
  GInputStream *stream = source_object;
  GInputStreamClass *class;
  GError *error = NULL;
  gboolean result;

  class = G_INPUT_STREAM_GET_CLASS (stream);
  if (class->close_fn)
    {
      result = class->close_fn (stream,
                                g_task_get_cancellable (task),
                                &error);
      if (!result)
        {
          g_task_return_error (task, error);
          return;
        }
    }

  g_task_return_boolean (task, TRUE);
}

static void
g_input_stream_real_close_async (GInputStream        *stream,
				 int                  io_priority,
				 GCancellable        *cancellable,
				 GAsyncReadyCallback  callback,
				 gpointer             user_data)
{
  GTask *task;

  task = g_task_new (stream, cancellable, callback, user_data);
  g_task_set_source_tag (task, g_input_stream_real_close_async);
  g_task_set_check_cancellable (task, FALSE);
  g_task_set_priority (task, io_priority);
  
  g_task_run_in_thread (task, close_async_thread);
  g_object_unref (task);
}

static gboolean
g_input_stream_real_close_finish (GInputStream  *stream,
				  GAsyncResult  *result,
				  GError       **error)
{
  g_return_val_if_fail (g_task_is_valid (result, stream), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}
