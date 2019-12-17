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
#include <string.h>
#include "goutputstream.h"
#include "gcancellable.h"
#include "gasyncresult.h"
#include "gtask.h"
#include "ginputstream.h"
#include "gioerror.h"
#include "gioprivate.h"
#include "glibintl.h"
#include "gpollableoutputstream.h"

/**
 * SECTION:goutputstream
 * @short_description: Base class for implementing streaming output
 * @include: gio/gio.h
 *
 * #GOutputStream has functions to write to a stream (g_output_stream_write()),
 * to close a stream (g_output_stream_close()) and to flush pending writes
 * (g_output_stream_flush()). 
 *
 * To copy the content of an input stream to an output stream without 
 * manually handling the reads and writes, use g_output_stream_splice().
 *
 * See the documentation for #GIOStream for details of thread safety of
 * streaming APIs.
 *
 * All of these functions have async variants too.
 **/

struct _GOutputStreamPrivate {
  guint closed : 1;
  guint pending : 1;
  guint closing : 1;
  GAsyncReadyCallback outstanding_callback;
};

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (GOutputStream, g_output_stream, G_TYPE_OBJECT)

static gssize   g_output_stream_real_splice        (GOutputStream             *stream,
						    GInputStream              *source,
						    GOutputStreamSpliceFlags   flags,
						    GCancellable              *cancellable,
						    GError                   **error);
static void     g_output_stream_real_write_async   (GOutputStream             *stream,
						    const void                *buffer,
						    gsize                      count,
						    int                        io_priority,
						    GCancellable              *cancellable,
						    GAsyncReadyCallback        callback,
						    gpointer                   data);
static gssize   g_output_stream_real_write_finish  (GOutputStream             *stream,
						    GAsyncResult              *result,
						    GError                   **error);
static void     g_output_stream_real_splice_async  (GOutputStream             *stream,
						    GInputStream              *source,
						    GOutputStreamSpliceFlags   flags,
						    int                        io_priority,
						    GCancellable              *cancellable,
						    GAsyncReadyCallback        callback,
						    gpointer                   data);
static gssize   g_output_stream_real_splice_finish (GOutputStream             *stream,
						    GAsyncResult              *result,
						    GError                   **error);
static void     g_output_stream_real_flush_async   (GOutputStream             *stream,
						    int                        io_priority,
						    GCancellable              *cancellable,
						    GAsyncReadyCallback        callback,
						    gpointer                   data);
static gboolean g_output_stream_real_flush_finish  (GOutputStream             *stream,
						    GAsyncResult              *result,
						    GError                   **error);
static void     g_output_stream_real_close_async   (GOutputStream             *stream,
						    int                        io_priority,
						    GCancellable              *cancellable,
						    GAsyncReadyCallback        callback,
						    gpointer                   data);
static gboolean g_output_stream_real_close_finish  (GOutputStream             *stream,
						    GAsyncResult              *result,
						    GError                   **error);
static gboolean g_output_stream_internal_close     (GOutputStream             *stream,
                                                    GCancellable              *cancellable,
                                                    GError                   **error);
static void     g_output_stream_internal_close_async (GOutputStream           *stream,
                                                      int                      io_priority,
                                                      GCancellable            *cancellable,
                                                      GAsyncReadyCallback      callback,
                                                      gpointer                 data);
static gboolean g_output_stream_internal_close_finish (GOutputStream          *stream,
                                                       GAsyncResult           *result,
                                                       GError                **error);

static void
g_output_stream_dispose (GObject *object)
{
  GOutputStream *stream;

  stream = G_OUTPUT_STREAM (object);
  
  if (!stream->priv->closed)
    g_output_stream_close (stream, NULL, NULL);

  G_OBJECT_CLASS (g_output_stream_parent_class)->dispose (object);
}

static void
g_output_stream_class_init (GOutputStreamClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = g_output_stream_dispose;

  klass->splice = g_output_stream_real_splice;
  
  klass->write_async = g_output_stream_real_write_async;
  klass->write_finish = g_output_stream_real_write_finish;
  klass->splice_async = g_output_stream_real_splice_async;
  klass->splice_finish = g_output_stream_real_splice_finish;
  klass->flush_async = g_output_stream_real_flush_async;
  klass->flush_finish = g_output_stream_real_flush_finish;
  klass->close_async = g_output_stream_real_close_async;
  klass->close_finish = g_output_stream_real_close_finish;
}

static void
g_output_stream_init (GOutputStream *stream)
{
  stream->priv = g_output_stream_get_instance_private (stream);
}

/**
 * g_output_stream_write:
 * @stream: a #GOutputStream.
 * @buffer: (array length=count) (element-type guint8): the buffer containing the data to write. 
 * @count: the number of bytes to write
 * @cancellable: (nullable): optional cancellable object
 * @error: location to store the error occurring, or %NULL to ignore
 *
 * Tries to write @count bytes from @buffer into the stream. Will block
 * during the operation.
 * 
 * If count is 0, returns 0 and does nothing. A value of @count
 * larger than %G_MAXSSIZE will cause a %G_IO_ERROR_INVALID_ARGUMENT error.
 *
 * On success, the number of bytes written to the stream is returned.
 * It is not an error if this is not the same as the requested size, as it
 * can happen e.g. on a partial I/O error, or if there is not enough
 * storage in the stream. All writes block until at least one byte
 * is written or an error occurs; 0 is never returned (unless
 * @count is 0).
 * 
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. If an
 * operation was partially finished when the operation was cancelled the
 * partial result will be returned, without an error.
 *
 * On error -1 is returned and @error is set accordingly.
 * 
 * Virtual: write_fn
 *
 * Returns: Number of bytes written, or -1 on error
 **/
gssize
g_output_stream_write (GOutputStream  *stream,
		       const void     *buffer,
		       gsize           count,
		       GCancellable   *cancellable,
		       GError        **error)
{
  GOutputStreamClass *class;
  gssize res;

  g_return_val_if_fail (G_IS_OUTPUT_STREAM (stream), -1);
  g_return_val_if_fail (buffer != NULL, 0);

  if (count == 0)
    return 0;
  
  if (((gssize) count) < 0)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
		   _("Too large count value passed to %s"), G_STRFUNC);
      return -1;
    }

  class = G_OUTPUT_STREAM_GET_CLASS (stream);

  if (class->write_fn == NULL) 
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                           _("Output stream doesn’t implement write"));
      return -1;
    }
  
  if (!g_output_stream_set_pending (stream, error))
    return -1;
  
  if (cancellable)
    g_cancellable_push_current (cancellable);
  
  res = class->write_fn (stream, buffer, count, cancellable, error);
  
  if (cancellable)
    g_cancellable_pop_current (cancellable);
  
  g_output_stream_clear_pending (stream);

  return res; 
}

/**
 * g_output_stream_write_all:
 * @stream: a #GOutputStream.
 * @buffer: (array length=count) (element-type guint8): the buffer containing the data to write. 
 * @count: the number of bytes to write
 * @bytes_written: (out) (optional): location to store the number of bytes that was
 *     written to the stream
 * @cancellable: (nullable): optional #GCancellable object, %NULL to ignore.
 * @error: location to store the error occurring, or %NULL to ignore
 *
 * Tries to write @count bytes from @buffer into the stream. Will block
 * during the operation.
 * 
 * This function is similar to g_output_stream_write(), except it tries to
 * write as many bytes as requested, only stopping on an error.
 *
 * On a successful write of @count bytes, %TRUE is returned, and @bytes_written
 * is set to @count.
 * 
 * If there is an error during the operation %FALSE is returned and @error
 * is set to indicate the error status.
 *
 * As a special exception to the normal conventions for functions that
 * use #GError, if this function returns %FALSE (and sets @error) then
 * @bytes_written will be set to the number of bytes that were
 * successfully written before the error was encountered.  This
 * functionality is only available from C.  If you need it from another
 * language then you must write your own loop around
 * g_output_stream_write().
 *
 * Returns: %TRUE on success, %FALSE if there was an error
 **/
gboolean
g_output_stream_write_all (GOutputStream  *stream,
			   const void     *buffer,
			   gsize           count,
			   gsize          *bytes_written,
			   GCancellable   *cancellable,
			   GError        **error)
{
  gsize _bytes_written;
  gssize res;

  g_return_val_if_fail (G_IS_OUTPUT_STREAM (stream), FALSE);
  g_return_val_if_fail (buffer != NULL, FALSE);

  _bytes_written = 0;
  while (_bytes_written < count)
    {
      res = g_output_stream_write (stream, (char *)buffer + _bytes_written, count - _bytes_written,
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

/**
 * g_output_stream_printf:
 * @stream: a #GOutputStream.
 * @bytes_written: (out) (optional): location to store the number of bytes that was
 *     written to the stream
 * @cancellable: (nullable): optional #GCancellable object, %NULL to ignore.
 * @error: location to store the error occurring, or %NULL to ignore
 * @format: the format string. See the printf() documentation
 * @...: the parameters to insert into the format string
 *
 * This is a utility function around g_output_stream_write_all(). It
 * uses g_strdup_vprintf() to turn @format and @... into a string that
 * is then written to @stream.
 *
 * See the documentation of g_output_stream_write_all() about the
 * behavior of the actual write operation.
 *
 * Note that partial writes cannot be properly checked with this
 * function due to the variable length of the written string, if you
 * need precise control over partial write failures, you need to
 * create you own printf()-like wrapper around g_output_stream_write()
 * or g_output_stream_write_all().
 *
 * Since: 2.40
 *
 * Returns: %TRUE on success, %FALSE if there was an error
 **/
gboolean
g_output_stream_printf (GOutputStream  *stream,
                        gsize          *bytes_written,
                        GCancellable   *cancellable,
                        GError        **error,
                        const gchar    *format,
                        ...)
{
  va_list  args;
  gboolean success;

  va_start (args, format);
  success = g_output_stream_vprintf (stream, bytes_written, cancellable,
                                     error, format, args);
  va_end (args);

  return success;
}

/**
 * g_output_stream_vprintf:
 * @stream: a #GOutputStream.
 * @bytes_written: (out) (optional): location to store the number of bytes that was
 *     written to the stream
 * @cancellable: (nullable): optional #GCancellable object, %NULL to ignore.
 * @error: location to store the error occurring, or %NULL to ignore
 * @format: the format string. See the printf() documentation
 * @args: the parameters to insert into the format string
 *
 * This is a utility function around g_output_stream_write_all(). It
 * uses g_strdup_vprintf() to turn @format and @args into a string that
 * is then written to @stream.
 *
 * See the documentation of g_output_stream_write_all() about the
 * behavior of the actual write operation.
 *
 * Note that partial writes cannot be properly checked with this
 * function due to the variable length of the written string, if you
 * need precise control over partial write failures, you need to
 * create you own printf()-like wrapper around g_output_stream_write()
 * or g_output_stream_write_all().
 *
 * Since: 2.40
 *
 * Returns: %TRUE on success, %FALSE if there was an error
 **/
gboolean
g_output_stream_vprintf (GOutputStream  *stream,
                         gsize          *bytes_written,
                         GCancellable   *cancellable,
                         GError        **error,
                         const gchar    *format,
                         va_list         args)
{
  gchar    *text;
  gboolean  success;

  g_return_val_if_fail (G_IS_OUTPUT_STREAM (stream), FALSE);
  g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  g_return_val_if_fail (format != NULL, FALSE);

  text = g_strdup_vprintf (format, args);
  success = g_output_stream_write_all (stream,
                                       text, strlen (text),
                                       bytes_written, cancellable, error);
  g_free (text);

  return success;
}

/**
 * g_output_stream_write_bytes:
 * @stream: a #GOutputStream.
 * @bytes: the #GBytes to write
 * @cancellable: (nullable): optional cancellable object
 * @error: location to store the error occurring, or %NULL to ignore
 *
 * A wrapper function for g_output_stream_write() which takes a
 * #GBytes as input.  This can be more convenient for use by language
 * bindings or in other cases where the refcounted nature of #GBytes
 * is helpful over a bare pointer interface.
 *
 * However, note that this function may still perform partial writes,
 * just like g_output_stream_write().  If that occurs, to continue
 * writing, you will need to create a new #GBytes containing just the
 * remaining bytes, using g_bytes_new_from_bytes(). Passing the same
 * #GBytes instance multiple times potentially can result in duplicated
 * data in the output stream.
 *
 * Returns: Number of bytes written, or -1 on error
 **/
gssize
g_output_stream_write_bytes (GOutputStream  *stream,
			     GBytes         *bytes,
			     GCancellable   *cancellable,
			     GError        **error)
{
  gsize size;
  gconstpointer data;

  data = g_bytes_get_data (bytes, &size);

  return g_output_stream_write (stream,
                                data, size,
				cancellable,
				error);
}

/**
 * g_output_stream_flush:
 * @stream: a #GOutputStream.
 * @cancellable: (nullable): optional cancellable object
 * @error: location to store the error occurring, or %NULL to ignore
 *
 * Forces a write of all user-space buffered data for the given
 * @stream. Will block during the operation. Closing the stream will
 * implicitly cause a flush.
 *
 * This function is optional for inherited classes.
 * 
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned.
 *
 * Returns: %TRUE on success, %FALSE on error
 **/
gboolean
g_output_stream_flush (GOutputStream  *stream,
                       GCancellable   *cancellable,
                       GError        **error)
{
  GOutputStreamClass *class;
  gboolean res;

  g_return_val_if_fail (G_IS_OUTPUT_STREAM (stream), FALSE);

  if (!g_output_stream_set_pending (stream, error))
    return FALSE;
  
  class = G_OUTPUT_STREAM_GET_CLASS (stream);

  res = TRUE;
  if (class->flush)
    {
      if (cancellable)
	g_cancellable_push_current (cancellable);
      
      res = class->flush (stream, cancellable, error);
      
      if (cancellable)
	g_cancellable_pop_current (cancellable);
    }
  
  g_output_stream_clear_pending (stream);

  return res;
}

/**
 * g_output_stream_splice:
 * @stream: a #GOutputStream.
 * @source: a #GInputStream.
 * @flags: a set of #GOutputStreamSpliceFlags.
 * @cancellable: (nullable): optional #GCancellable object, %NULL to ignore.
 * @error: a #GError location to store the error occurring, or %NULL to
 * ignore.
 *
 * Splices an input stream into an output stream.
 *
 * Returns: a #gssize containing the size of the data spliced, or
 *     -1 if an error occurred. Note that if the number of bytes
 *     spliced is greater than %G_MAXSSIZE, then that will be
 *     returned, and there is no way to determine the actual number
 *     of bytes spliced.
 **/
gssize
g_output_stream_splice (GOutputStream             *stream,
			GInputStream              *source,
			GOutputStreamSpliceFlags   flags,
			GCancellable              *cancellable,
			GError                   **error)
{
  GOutputStreamClass *class;
  gssize bytes_copied;

  g_return_val_if_fail (G_IS_OUTPUT_STREAM (stream), -1);
  g_return_val_if_fail (G_IS_INPUT_STREAM (source), -1);

  if (g_input_stream_is_closed (source))
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_CLOSED,
                           _("Source stream is already closed"));
      return -1;
    }

  if (!g_output_stream_set_pending (stream, error))
    return -1;

  class = G_OUTPUT_STREAM_GET_CLASS (stream);

  if (cancellable)
    g_cancellable_push_current (cancellable);

  bytes_copied = class->splice (stream, source, flags, cancellable, error);

  if (cancellable)
    g_cancellable_pop_current (cancellable);

  g_output_stream_clear_pending (stream);

  return bytes_copied;
}

static gssize
g_output_stream_real_splice (GOutputStream             *stream,
                             GInputStream              *source,
                             GOutputStreamSpliceFlags   flags,
                             GCancellable              *cancellable,
                             GError                   **error)
{
  GOutputStreamClass *class = G_OUTPUT_STREAM_GET_CLASS (stream);
  gssize n_read, n_written;
  gsize bytes_copied;
  char buffer[8192], *p;
  gboolean res;

  bytes_copied = 0;
  if (class->write_fn == NULL)
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                           _("Output stream doesn’t implement write"));
      res = FALSE;
      goto notsupported;
    }

  res = TRUE;
  do
    {
      n_read = g_input_stream_read (source, buffer, sizeof (buffer), cancellable, error);
      if (n_read == -1)
	{
	  res = FALSE;
	  break;
	}

      if (n_read == 0)
	break;

      p = buffer;
      while (n_read > 0)
	{
	  n_written = class->write_fn (stream, p, n_read, cancellable, error);
	  if (n_written == -1)
	    {
	      res = FALSE;
	      break;
	    }

	  p += n_written;
	  n_read -= n_written;
	  bytes_copied += n_written;
	}

      if (bytes_copied > G_MAXSSIZE)
	bytes_copied = G_MAXSSIZE;
    }
  while (res);

 notsupported:
  if (!res)
    error = NULL; /* Ignore further errors */

  if (flags & G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE)
    {
      /* Don't care about errors in source here */
      g_input_stream_close (source, cancellable, NULL);
    }

  if (flags & G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET)
    {
      /* But write errors on close are bad! */
      if (!g_output_stream_internal_close (stream, cancellable, error))
        res = FALSE;
    }

  if (res)
    return bytes_copied;

  return -1;
}

/* Must always be called inside
 * g_output_stream_set_pending()/g_output_stream_clear_pending(). */
static gboolean
g_output_stream_internal_close (GOutputStream  *stream,
                                GCancellable   *cancellable,
                                GError        **error)
{
  GOutputStreamClass *class;
  gboolean res;

  if (stream->priv->closed)
    return TRUE;

  class = G_OUTPUT_STREAM_GET_CLASS (stream);

  stream->priv->closing = TRUE;

  if (cancellable)
    g_cancellable_push_current (cancellable);

  if (class->flush)
    res = class->flush (stream, cancellable, error);
  else
    res = TRUE;

  if (!res)
    {
      /* flushing caused the error that we want to return,
       * but we still want to close the underlying stream if possible
       */
      if (class->close_fn)
        class->close_fn (stream, cancellable, NULL);
    }
  else
    {
      res = TRUE;
      if (class->close_fn)
        res = class->close_fn (stream, cancellable, error);
    }

  if (cancellable)
    g_cancellable_pop_current (cancellable);

  stream->priv->closing = FALSE;
  stream->priv->closed = TRUE;

  return res;
}

/**
 * g_output_stream_close:
 * @stream: A #GOutputStream.
 * @cancellable: (nullable): optional cancellable object
 * @error: location to store the error occurring, or %NULL to ignore
 *
 * Closes the stream, releasing resources related to it.
 *
 * Once the stream is closed, all other operations will return %G_IO_ERROR_CLOSED.
 * Closing a stream multiple times will not return an error.
 *
 * Closing a stream will automatically flush any outstanding buffers in the
 * stream.
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
 * is important to check and report the error to the user, otherwise
 * there might be a loss of data as all data might not be written.
 * 
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned.
 * Cancelling a close will still leave the stream closed, but there some streams
 * can use a faster close that doesn't block to e.g. check errors. On
 * cancellation (as with any error) there is no guarantee that all written
 * data will reach the target. 
 *
 * Returns: %TRUE on success, %FALSE on failure
 **/
gboolean
g_output_stream_close (GOutputStream  *stream,
		       GCancellable   *cancellable,
		       GError        **error)
{
  gboolean res;

  g_return_val_if_fail (G_IS_OUTPUT_STREAM (stream), FALSE);

  if (stream->priv->closed)
    return TRUE;

  if (!g_output_stream_set_pending (stream, error))
    return FALSE;

  res = g_output_stream_internal_close (stream, cancellable, error);

  g_output_stream_clear_pending (stream);
  
  return res;
}

static void
async_ready_write_callback_wrapper (GObject      *source_object,
                                    GAsyncResult *res,
                                    gpointer      user_data)
{
  GOutputStream *stream = G_OUTPUT_STREAM (source_object);
  GOutputStreamClass *class;
  GTask *task = user_data;
  gssize nwrote;
  GError *error = NULL;

  g_output_stream_clear_pending (stream);
  
  if (g_async_result_legacy_propagate_error (res, &error))
    nwrote = -1;
  else
    {
      class = G_OUTPUT_STREAM_GET_CLASS (stream);
      nwrote = class->write_finish (stream, res, &error);
    }

  if (nwrote >= 0)
    g_task_return_int (task, nwrote);
  else
    g_task_return_error (task, error);
  g_object_unref (task);
}

/**
 * g_output_stream_write_async:
 * @stream: A #GOutputStream.
 * @buffer: (array length=count) (element-type guint8): the buffer containing the data to write. 
 * @count: the number of bytes to write
 * @io_priority: the io priority of the request.
 * @cancellable: (nullable): optional #GCancellable object, %NULL to ignore.
 * @callback: (scope async): callback to call when the request is satisfied
 * @user_data: (closure): the data to pass to callback function
 *
 * Request an asynchronous write of @count bytes from @buffer into 
 * the stream. When the operation is finished @callback will be called.
 * You can then call g_output_stream_write_finish() to get the result of the 
 * operation.
 *
 * During an async request no other sync and async calls are allowed, 
 * and will result in %G_IO_ERROR_PENDING errors. 
 *
 * A value of @count larger than %G_MAXSSIZE will cause a 
 * %G_IO_ERROR_INVALID_ARGUMENT error.
 *
 * On success, the number of bytes written will be passed to the
 * @callback. It is not an error if this is not the same as the 
 * requested size, as it can happen e.g. on a partial I/O error, 
 * but generally we try to write as many bytes as requested. 
 *
 * You are guaranteed that this method will never fail with
 * %G_IO_ERROR_WOULD_BLOCK - if @stream can't accept more data, the
 * method will just wait until this changes.
 *
 * Any outstanding I/O request with higher priority (lower numerical 
 * value) will be executed before an outstanding request with lower 
 * priority. Default priority is %G_PRIORITY_DEFAULT.
 *
 * The asynchronous methods have a default fallback that uses threads
 * to implement asynchronicity, so they are optional for inheriting 
 * classes. However, if you override one you must override all.
 *
 * For the synchronous, blocking version of this function, see 
 * g_output_stream_write().
 *
 * Note that no copy of @buffer will be made, so it must stay valid
 * until @callback is called. See g_output_stream_write_bytes_async()
 * for a #GBytes version that will automatically hold a reference to
 * the contents (without copying) for the duration of the call.
 */
void
g_output_stream_write_async (GOutputStream       *stream,
			     const void          *buffer,
			     gsize                count,
			     int                  io_priority,
			     GCancellable        *cancellable,
			     GAsyncReadyCallback  callback,
			     gpointer             user_data)
{
  GOutputStreamClass *class;
  GError *error = NULL;
  GTask *task;

  g_return_if_fail (G_IS_OUTPUT_STREAM (stream));
  g_return_if_fail (buffer != NULL);

  task = g_task_new (stream, cancellable, callback, user_data);
  g_task_set_source_tag (task, g_output_stream_write_async);
  g_task_set_priority (task, io_priority);

  if (count == 0)
    {
      g_task_return_int (task, 0);
      g_object_unref (task);
      return;
    }

  if (((gssize) count) < 0)
    {
      g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                               _("Too large count value passed to %s"),
                               G_STRFUNC);
      g_object_unref (task);
      return;
    }

  if (!g_output_stream_set_pending (stream, &error))
    {
      g_task_return_error (task, error);
      g_object_unref (task);
      return;
    }
  
  class = G_OUTPUT_STREAM_GET_CLASS (stream);

  class->write_async (stream, buffer, count, io_priority, cancellable,
                      async_ready_write_callback_wrapper, task);
}

/**
 * g_output_stream_write_finish:
 * @stream: a #GOutputStream.
 * @result: a #GAsyncResult.
 * @error: a #GError location to store the error occurring, or %NULL to 
 * ignore.
 * 
 * Finishes a stream write operation.
 * 
 * Returns: a #gssize containing the number of bytes written to the stream.
 **/
gssize
g_output_stream_write_finish (GOutputStream  *stream,
                              GAsyncResult   *result,
                              GError        **error)
{
  g_return_val_if_fail (G_IS_OUTPUT_STREAM (stream), FALSE);
  g_return_val_if_fail (g_task_is_valid (result, stream), FALSE);
  g_return_val_if_fail (g_async_result_is_tagged (result, g_output_stream_write_async), FALSE);

  /* @result is always the GTask created by g_output_stream_write_async();
   * we called class->write_finish() from async_ready_write_callback_wrapper.
   */
  return g_task_propagate_int (G_TASK (result), error);
}

typedef struct
{
  const guint8 *buffer;
  gsize to_write;
  gsize bytes_written;
} AsyncWriteAll;

static void
free_async_write_all (gpointer data)
{
  g_slice_free (AsyncWriteAll, data);
}

static void
write_all_callback (GObject      *stream,
                    GAsyncResult *result,
                    gpointer      user_data)
{
  GTask *task = user_data;
  AsyncWriteAll *data = g_task_get_task_data (task);

  if (result)
    {
      GError *error = NULL;
      gssize nwritten;

      nwritten = g_output_stream_write_finish (G_OUTPUT_STREAM (stream), result, &error);

      if (nwritten == -1)
        {
          g_task_return_error (task, error);
          g_object_unref (task);
          return;
        }

      g_assert_cmpint (nwritten, <=, data->to_write);
      g_warn_if_fail (nwritten > 0);

      data->to_write -= nwritten;
      data->bytes_written += nwritten;
    }

  if (data->to_write == 0)
    {
      g_task_return_boolean (task, TRUE);
      g_object_unref (task);
    }

  else
    g_output_stream_write_async (G_OUTPUT_STREAM (stream),
                                 data->buffer + data->bytes_written,
                                 data->to_write,
                                 g_task_get_priority (task),
                                 g_task_get_cancellable (task),
                                 write_all_callback, task);
}

static void
write_all_async_thread (GTask        *task,
                        gpointer      source_object,
                        gpointer      task_data,
                        GCancellable *cancellable)
{
  GOutputStream *stream = source_object;
  AsyncWriteAll *data = task_data;
  GError *error = NULL;

  if (g_output_stream_write_all (stream, data->buffer, data->to_write, &data->bytes_written,
                                 g_task_get_cancellable (task), &error))
    g_task_return_boolean (task, TRUE);
  else
    g_task_return_error (task, error);
}

/**
 * g_output_stream_write_all_async:
 * @stream: A #GOutputStream
 * @buffer: (array length=count) (element-type guint8): the buffer containing the data to write
 * @count: the number of bytes to write
 * @io_priority: the io priority of the request
 * @cancellable: (nullable): optional #GCancellable object, %NULL to ignore
 * @callback: (scope async): callback to call when the request is satisfied
 * @user_data: (closure): the data to pass to callback function
 *
 * Request an asynchronous write of @count bytes from @buffer into
 * the stream. When the operation is finished @callback will be called.
 * You can then call g_output_stream_write_all_finish() to get the result of the
 * operation.
 *
 * This is the asynchronous version of g_output_stream_write_all().
 *
 * Call g_output_stream_write_all_finish() to collect the result.
 *
 * Any outstanding I/O request with higher priority (lower numerical
 * value) will be executed before an outstanding request with lower
 * priority. Default priority is %G_PRIORITY_DEFAULT.
 *
 * Note that no copy of @buffer will be made, so it must stay valid
 * until @callback is called.
 *
 * Since: 2.44
 */
void
g_output_stream_write_all_async (GOutputStream       *stream,
                                 const void          *buffer,
                                 gsize                count,
                                 int                  io_priority,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data)
{
  AsyncWriteAll *data;
  GTask *task;

  g_return_if_fail (G_IS_OUTPUT_STREAM (stream));
  g_return_if_fail (buffer != NULL || count == 0);

  task = g_task_new (stream, cancellable, callback, user_data);
  data = g_slice_new0 (AsyncWriteAll);
  data->buffer = buffer;
  data->to_write = count;

  g_task_set_source_tag (task, g_output_stream_write_all_async);
  g_task_set_task_data (task, data, free_async_write_all);
  g_task_set_priority (task, io_priority);

  /* If async writes are going to be handled via the threadpool anyway
   * then we may as well do it with a single dispatch instead of
   * bouncing in and out.
   */
  if (g_output_stream_async_write_is_via_threads (stream))
    {
      g_task_run_in_thread (task, write_all_async_thread);
      g_object_unref (task);
    }
  else
    write_all_callback (G_OBJECT (stream), NULL, task);
}

/**
 * g_output_stream_write_all_finish:
 * @stream: a #GOutputStream
 * @result: a #GAsyncResult
 * @bytes_written: (out) (optional): location to store the number of bytes that was written to the stream
 * @error: a #GError location to store the error occurring, or %NULL to ignore.
 *
 * Finishes an asynchronous stream write operation started with
 * g_output_stream_write_all_async().
 *
 * As a special exception to the normal conventions for functions that
 * use #GError, if this function returns %FALSE (and sets @error) then
 * @bytes_written will be set to the number of bytes that were
 * successfully written before the error was encountered.  This
 * functionality is only available from C.  If you need it from another
 * language then you must write your own loop around
 * g_output_stream_write_async().
 *
 * Returns: %TRUE on success, %FALSE if there was an error
 *
 * Since: 2.44
 **/
gboolean
g_output_stream_write_all_finish (GOutputStream  *stream,
                                  GAsyncResult   *result,
                                  gsize          *bytes_written,
                                  GError        **error)
{
  GTask *task;

  g_return_val_if_fail (G_IS_OUTPUT_STREAM (stream), FALSE);
  g_return_val_if_fail (g_task_is_valid (result, stream), FALSE);

  task = G_TASK (result);

  if (bytes_written)
    {
      AsyncWriteAll *data = (AsyncWriteAll *)g_task_get_task_data (task);

      *bytes_written = data->bytes_written;
    }

  return g_task_propagate_boolean (task, error);
}

static void
write_bytes_callback (GObject      *stream,
                      GAsyncResult *result,
                      gpointer      user_data)
{
  GTask *task = user_data;
  GError *error = NULL;
  gssize nwrote;

  nwrote = g_output_stream_write_finish (G_OUTPUT_STREAM (stream),
                                         result, &error);
  if (nwrote == -1)
    g_task_return_error (task, error);
  else
    g_task_return_int (task, nwrote);
  g_object_unref (task);
}

/**
 * g_output_stream_write_bytes_async:
 * @stream: A #GOutputStream.
 * @bytes: The bytes to write
 * @io_priority: the io priority of the request.
 * @cancellable: (nullable): optional #GCancellable object, %NULL to ignore.
 * @callback: (scope async): callback to call when the request is satisfied
 * @user_data: (closure): the data to pass to callback function
 *
 * This function is similar to g_output_stream_write_async(), but
 * takes a #GBytes as input.  Due to the refcounted nature of #GBytes,
 * this allows the stream to avoid taking a copy of the data.
 *
 * However, note that this function may still perform partial writes,
 * just like g_output_stream_write_async(). If that occurs, to continue
 * writing, you will need to create a new #GBytes containing just the
 * remaining bytes, using g_bytes_new_from_bytes(). Passing the same
 * #GBytes instance multiple times potentially can result in duplicated
 * data in the output stream.
 *
 * For the synchronous, blocking version of this function, see
 * g_output_stream_write_bytes().
 **/
void
g_output_stream_write_bytes_async (GOutputStream       *stream,
				   GBytes              *bytes,
				   int                  io_priority,
				   GCancellable        *cancellable,
				   GAsyncReadyCallback  callback,
				   gpointer             user_data)
{
  GTask *task;
  gsize size;
  gconstpointer data;

  data = g_bytes_get_data (bytes, &size);

  task = g_task_new (stream, cancellable, callback, user_data);
  g_task_set_source_tag (task, g_output_stream_write_bytes_async);
  g_task_set_task_data (task, g_bytes_ref (bytes),
                        (GDestroyNotify) g_bytes_unref);

  g_output_stream_write_async (stream,
                               data, size,
                               io_priority,
                               cancellable,
                               write_bytes_callback,
                               task);
}

/**
 * g_output_stream_write_bytes_finish:
 * @stream: a #GOutputStream.
 * @result: a #GAsyncResult.
 * @error: a #GError location to store the error occurring, or %NULL to
 * ignore.
 *
 * Finishes a stream write-from-#GBytes operation.
 *
 * Returns: a #gssize containing the number of bytes written to the stream.
 **/
gssize
g_output_stream_write_bytes_finish (GOutputStream  *stream,
				    GAsyncResult   *result,
				    GError        **error)
{
  g_return_val_if_fail (G_IS_OUTPUT_STREAM (stream), -1);
  g_return_val_if_fail (g_task_is_valid (result, stream), -1);

  return g_task_propagate_int (G_TASK (result), error);
}

static void
async_ready_splice_callback_wrapper (GObject      *source_object,
                                     GAsyncResult *res,
                                     gpointer     _data)
{
  GOutputStream *stream = G_OUTPUT_STREAM (source_object);
  GOutputStreamClass *class;
  GTask *task = _data;
  gssize nspliced;
  GError *error = NULL;

  g_output_stream_clear_pending (stream);
  
  if (g_async_result_legacy_propagate_error (res, &error))
    nspliced = -1;
  else
    {
      class = G_OUTPUT_STREAM_GET_CLASS (stream);
      nspliced = class->splice_finish (stream, res, &error);
    }

  if (nspliced >= 0)
    g_task_return_int (task, nspliced);
  else
    g_task_return_error (task, error);
  g_object_unref (task);
}

/**
 * g_output_stream_splice_async:
 * @stream: a #GOutputStream.
 * @source: a #GInputStream. 
 * @flags: a set of #GOutputStreamSpliceFlags.
 * @io_priority: the io priority of the request.
 * @cancellable: (nullable): optional #GCancellable object, %NULL to ignore. 
 * @callback: (scope async): a #GAsyncReadyCallback. 
 * @user_data: (closure): user data passed to @callback.
 * 
 * Splices a stream asynchronously.
 * When the operation is finished @callback will be called.
 * You can then call g_output_stream_splice_finish() to get the 
 * result of the operation.
 *
 * For the synchronous, blocking version of this function, see 
 * g_output_stream_splice().
 **/
void
g_output_stream_splice_async (GOutputStream            *stream,
			      GInputStream             *source,
			      GOutputStreamSpliceFlags  flags,
			      int                       io_priority,
			      GCancellable             *cancellable,
			      GAsyncReadyCallback       callback,
			      gpointer                  user_data)
{
  GOutputStreamClass *class;
  GTask *task;
  GError *error = NULL;

  g_return_if_fail (G_IS_OUTPUT_STREAM (stream));
  g_return_if_fail (G_IS_INPUT_STREAM (source));

  task = g_task_new (stream, cancellable, callback, user_data);
  g_task_set_source_tag (task, g_output_stream_splice_async);
  g_task_set_priority (task, io_priority);
  g_task_set_task_data (task, g_object_ref (source), g_object_unref);

  if (g_input_stream_is_closed (source))
    {
      g_task_return_new_error (task,
                               G_IO_ERROR, G_IO_ERROR_CLOSED,
                               _("Source stream is already closed"));
      g_object_unref (task);
      return;
    }
  
  if (!g_output_stream_set_pending (stream, &error))
    {
      g_task_return_error (task, error);
      g_object_unref (task);
      return;
    }

  class = G_OUTPUT_STREAM_GET_CLASS (stream);

  class->splice_async (stream, source, flags, io_priority, cancellable,
                       async_ready_splice_callback_wrapper, task);
}

/**
 * g_output_stream_splice_finish:
 * @stream: a #GOutputStream.
 * @result: a #GAsyncResult.
 * @error: a #GError location to store the error occurring, or %NULL to 
 * ignore.
 *
 * Finishes an asynchronous stream splice operation.
 * 
 * Returns: a #gssize of the number of bytes spliced. Note that if the
 *     number of bytes spliced is greater than %G_MAXSSIZE, then that
 *     will be returned, and there is no way to determine the actual
 *     number of bytes spliced.
 **/
gssize
g_output_stream_splice_finish (GOutputStream  *stream,
			       GAsyncResult   *result,
			       GError        **error)
{
  g_return_val_if_fail (G_IS_OUTPUT_STREAM (stream), FALSE);
  g_return_val_if_fail (g_task_is_valid (result, stream), FALSE);
  g_return_val_if_fail (g_async_result_is_tagged (result, g_output_stream_splice_async), FALSE);

  /* @result is always the GTask created by g_output_stream_splice_async();
   * we called class->splice_finish() from async_ready_splice_callback_wrapper.
   */
  return g_task_propagate_int (G_TASK (result), error);
}

static void
async_ready_flush_callback_wrapper (GObject      *source_object,
                                    GAsyncResult *res,
                                    gpointer      user_data)
{
  GOutputStream *stream = G_OUTPUT_STREAM (source_object);
  GOutputStreamClass *class;
  GTask *task = user_data;
  gboolean flushed;
  GError *error = NULL;

  g_output_stream_clear_pending (stream);
  
  if (g_async_result_legacy_propagate_error (res, &error))
    flushed = FALSE;
  else
    {
      class = G_OUTPUT_STREAM_GET_CLASS (stream);
      flushed = class->flush_finish (stream, res, &error);
    }

  if (flushed)
    g_task_return_boolean (task, TRUE);
  else
    g_task_return_error (task, error);
  g_object_unref (task);
}

/**
 * g_output_stream_flush_async:
 * @stream: a #GOutputStream.
 * @io_priority: the io priority of the request.
 * @cancellable: (nullable): optional #GCancellable object, %NULL to ignore.
 * @callback: (scope async): a #GAsyncReadyCallback to call when the request is satisfied
 * @user_data: (closure): the data to pass to callback function
 * 
 * Forces an asynchronous write of all user-space buffered data for
 * the given @stream.
 * For behaviour details see g_output_stream_flush().
 *
 * When the operation is finished @callback will be 
 * called. You can then call g_output_stream_flush_finish() to get the 
 * result of the operation.
 **/
void
g_output_stream_flush_async (GOutputStream       *stream,
                             int                  io_priority,
                             GCancellable        *cancellable,
                             GAsyncReadyCallback  callback,
                             gpointer             user_data)
{
  GOutputStreamClass *class;
  GTask *task;
  GError *error = NULL;

  g_return_if_fail (G_IS_OUTPUT_STREAM (stream));

  task = g_task_new (stream, cancellable, callback, user_data);
  g_task_set_source_tag (task, g_output_stream_flush_async);
  g_task_set_priority (task, io_priority);

  if (!g_output_stream_set_pending (stream, &error))
    {
      g_task_return_error (task, error);
      g_object_unref (task);
      return;
    }

  class = G_OUTPUT_STREAM_GET_CLASS (stream);
  
  if (class->flush_async == NULL)
    {
      g_output_stream_clear_pending (stream);
      g_task_return_boolean (task, TRUE);
      g_object_unref (task);
      return;
    }
      
  class->flush_async (stream, io_priority, cancellable,
                      async_ready_flush_callback_wrapper, task);
}

/**
 * g_output_stream_flush_finish:
 * @stream: a #GOutputStream.
 * @result: a GAsyncResult.
 * @error: a #GError location to store the error occurring, or %NULL to 
 * ignore.
 * 
 * Finishes flushing an output stream.
 * 
 * Returns: %TRUE if flush operation succeeded, %FALSE otherwise.
 **/
gboolean
g_output_stream_flush_finish (GOutputStream  *stream,
                              GAsyncResult   *result,
                              GError        **error)
{
  g_return_val_if_fail (G_IS_OUTPUT_STREAM (stream), FALSE);
  g_return_val_if_fail (g_task_is_valid (result, stream), FALSE);
  g_return_val_if_fail (g_async_result_is_tagged (result, g_output_stream_flush_async), FALSE);

  /* @result is always the GTask created by g_output_stream_flush_async();
   * we called class->flush_finish() from async_ready_flush_callback_wrapper.
   */
  return g_task_propagate_boolean (G_TASK (result), error);
}


static void
async_ready_close_callback_wrapper (GObject      *source_object,
                                    GAsyncResult *res,
                                    gpointer      user_data)
{
  GOutputStream *stream = G_OUTPUT_STREAM (source_object);
  GOutputStreamClass *class;
  GTask *task = user_data;
  GError *error = g_task_get_task_data (task);

  stream->priv->closing = FALSE;
  stream->priv->closed = TRUE;

  if (!error && !g_async_result_legacy_propagate_error (res, &error))
    {
      class = G_OUTPUT_STREAM_GET_CLASS (stream);

      class->close_finish (stream, res,
                           error ? NULL : &error);
    }

  if (error != NULL)
    g_task_return_error (task, error);
  else
    g_task_return_boolean (task, TRUE);
  g_object_unref (task);
}

static void
async_ready_close_flushed_callback_wrapper (GObject      *source_object,
                                            GAsyncResult *res,
                                            gpointer      user_data)
{
  GOutputStream *stream = G_OUTPUT_STREAM (source_object);
  GOutputStreamClass *class;
  GTask *task = user_data;
  GError *error = NULL;

  class = G_OUTPUT_STREAM_GET_CLASS (stream);

  if (!g_async_result_legacy_propagate_error (res, &error))
    {
      class->flush_finish (stream, res, &error);
    }

  /* propagate the possible error */
  if (error)
    g_task_set_task_data (task, error, NULL);

  /* we still close, even if there was a flush error */
  class->close_async (stream,
                      g_task_get_priority (task),
                      g_task_get_cancellable (task),
                      async_ready_close_callback_wrapper, task);
}

static void
real_close_async_cb (GObject      *source_object,
                     GAsyncResult *res,
                     gpointer      user_data)
{
  GOutputStream *stream = G_OUTPUT_STREAM (source_object);
  GTask *task = user_data;
  GError *error = NULL;
  gboolean ret;

  g_output_stream_clear_pending (stream);

  ret = g_output_stream_internal_close_finish (stream, res, &error);

  if (error != NULL)
    g_task_return_error (task, error);
  else
    g_task_return_boolean (task, ret);

  g_object_unref (task);
}

/**
 * g_output_stream_close_async:
 * @stream: A #GOutputStream.
 * @io_priority: the io priority of the request.
 * @cancellable: (nullable): optional cancellable object
 * @callback: (scope async): callback to call when the request is satisfied
 * @user_data: (closure): the data to pass to callback function
 *
 * Requests an asynchronous close of the stream, releasing resources 
 * related to it. When the operation is finished @callback will be 
 * called. You can then call g_output_stream_close_finish() to get 
 * the result of the operation.
 *
 * For behaviour details see g_output_stream_close().
 *
 * The asynchronous methods have a default fallback that uses threads
 * to implement asynchronicity, so they are optional for inheriting 
 * classes. However, if you override one you must override all.
 **/
void
g_output_stream_close_async (GOutputStream       *stream,
                             int                  io_priority,
                             GCancellable        *cancellable,
                             GAsyncReadyCallback  callback,
                             gpointer             user_data)
{
  GTask *task;
  GError *error = NULL;

  g_return_if_fail (G_IS_OUTPUT_STREAM (stream));
  
  task = g_task_new (stream, cancellable, callback, user_data);
  g_task_set_source_tag (task, g_output_stream_close_async);
  g_task_set_priority (task, io_priority);

  if (!g_output_stream_set_pending (stream, &error))
    {
      g_task_return_error (task, error);
      g_object_unref (task);
      return;
    }

  g_output_stream_internal_close_async (stream, io_priority, cancellable,
                                        real_close_async_cb, task);
}

/* Must always be called inside
 * g_output_stream_set_pending()/g_output_stream_clear_pending().
 */
void
g_output_stream_internal_close_async (GOutputStream       *stream,
                                      int                  io_priority,
                                      GCancellable        *cancellable,
                                      GAsyncReadyCallback  callback,
                                      gpointer             user_data)
{
  GOutputStreamClass *class;
  GTask *task;

  task = g_task_new (stream, cancellable, callback, user_data);
  g_task_set_source_tag (task, g_output_stream_internal_close_async);
  g_task_set_priority (task, io_priority);

  if (stream->priv->closed)
    {
      g_task_return_boolean (task, TRUE);
      g_object_unref (task);
      return;
    }

  class = G_OUTPUT_STREAM_GET_CLASS (stream);
  stream->priv->closing = TRUE;

  /* Call close_async directly if there is no need to flush, or if the flush
     can be done sync (in the output stream async close thread) */
  if (class->flush_async == NULL ||
      (class->flush_async == g_output_stream_real_flush_async &&
       (class->flush == NULL || class->close_async == g_output_stream_real_close_async)))
    {
      class->close_async (stream, io_priority, cancellable,
                          async_ready_close_callback_wrapper, task);
    }
  else
    {
      /* First do an async flush, then do the async close in the callback
         wrapper (see async_ready_close_flushed_callback_wrapper) */
      class->flush_async (stream, io_priority, cancellable,
                          async_ready_close_flushed_callback_wrapper, task);
    }
}

static gboolean
g_output_stream_internal_close_finish (GOutputStream  *stream,
                                       GAsyncResult   *result,
                                       GError        **error)
{
  g_return_val_if_fail (G_IS_OUTPUT_STREAM (stream), FALSE);
  g_return_val_if_fail (g_task_is_valid (result, stream), FALSE);
  g_return_val_if_fail (g_async_result_is_tagged (result, g_output_stream_internal_close_async), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

/**
 * g_output_stream_close_finish:
 * @stream: a #GOutputStream.
 * @result: a #GAsyncResult.
 * @error: a #GError location to store the error occurring, or %NULL to 
 * ignore.
 * 
 * Closes an output stream.
 * 
 * Returns: %TRUE if stream was successfully closed, %FALSE otherwise.
 **/
gboolean
g_output_stream_close_finish (GOutputStream  *stream,
                              GAsyncResult   *result,
                              GError        **error)
{
  g_return_val_if_fail (G_IS_OUTPUT_STREAM (stream), FALSE);
  g_return_val_if_fail (g_task_is_valid (result, stream), FALSE);
  g_return_val_if_fail (g_async_result_is_tagged (result, g_output_stream_close_async), FALSE);

  /* @result is always the GTask created by g_output_stream_close_async();
   * we called class->close_finish() from async_ready_close_callback_wrapper.
   */
  return g_task_propagate_boolean (G_TASK (result), error);
}

/**
 * g_output_stream_is_closed:
 * @stream: a #GOutputStream.
 * 
 * Checks if an output stream has already been closed.
 * 
 * Returns: %TRUE if @stream is closed. %FALSE otherwise. 
 **/
gboolean
g_output_stream_is_closed (GOutputStream *stream)
{
  g_return_val_if_fail (G_IS_OUTPUT_STREAM (stream), TRUE);
  
  return stream->priv->closed;
}

/**
 * g_output_stream_is_closing:
 * @stream: a #GOutputStream.
 *
 * Checks if an output stream is being closed. This can be
 * used inside e.g. a flush implementation to see if the
 * flush (or other i/o operation) is called from within
 * the closing operation.
 *
 * Returns: %TRUE if @stream is being closed. %FALSE otherwise.
 *
 * Since: 2.24
 **/
gboolean
g_output_stream_is_closing (GOutputStream *stream)
{
  g_return_val_if_fail (G_IS_OUTPUT_STREAM (stream), TRUE);

  return stream->priv->closing;
}

/**
 * g_output_stream_has_pending:
 * @stream: a #GOutputStream.
 * 
 * Checks if an output stream has pending actions.
 * 
 * Returns: %TRUE if @stream has pending actions. 
 **/
gboolean
g_output_stream_has_pending (GOutputStream *stream)
{
  g_return_val_if_fail (G_IS_OUTPUT_STREAM (stream), FALSE);
  
  return stream->priv->pending;
}

/**
 * g_output_stream_set_pending:
 * @stream: a #GOutputStream.
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
g_output_stream_set_pending (GOutputStream *stream,
			     GError **error)
{
  g_return_val_if_fail (G_IS_OUTPUT_STREAM (stream), FALSE);
  
  if (stream->priv->closed)
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_CLOSED,
                           _("Stream is already closed"));
      return FALSE;
    }
  
  if (stream->priv->pending)
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_PENDING,
                           /* Translators: This is an error you get if there is
                            * already an operation running against this stream when
                            * you try to start one */
                           _("Stream has outstanding operation"));
      return FALSE;
    }
  
  stream->priv->pending = TRUE;
  return TRUE;
}

/**
 * g_output_stream_clear_pending:
 * @stream: output stream
 * 
 * Clears the pending flag on @stream.
 **/
void
g_output_stream_clear_pending (GOutputStream *stream)
{
  g_return_if_fail (G_IS_OUTPUT_STREAM (stream));
  
  stream->priv->pending = FALSE;
}

/*< internal >
 * g_output_stream_async_write_is_via_threads:
 * @stream: a #GOutputStream.
 *
 * Checks if an output stream's write_async function uses threads.
 *
 * Returns: %TRUE if @stream's write_async function uses threads.
 **/
gboolean
g_output_stream_async_write_is_via_threads (GOutputStream *stream)
{
  GOutputStreamClass *class;

  g_return_val_if_fail (G_IS_OUTPUT_STREAM (stream), FALSE);

  class = G_OUTPUT_STREAM_GET_CLASS (stream);

  return (class->write_async == g_output_stream_real_write_async &&
      !(G_IS_POLLABLE_OUTPUT_STREAM (stream) &&
        g_pollable_output_stream_can_poll (G_POLLABLE_OUTPUT_STREAM (stream))));
}

/*< internal >
 * g_output_stream_async_close_is_via_threads:
 * @stream: output stream
 *
 * Checks if an output stream's close_async function uses threads.
 *
 * Returns: %TRUE if @stream's close_async function uses threads.
 **/
gboolean
g_output_stream_async_close_is_via_threads (GOutputStream *stream)
{
  GOutputStreamClass *class;

  g_return_val_if_fail (G_IS_OUTPUT_STREAM (stream), FALSE);

  class = G_OUTPUT_STREAM_GET_CLASS (stream);

  return class->close_async == g_output_stream_real_close_async;
}

/********************************************
 *   Default implementation of async ops    *
 ********************************************/

typedef struct {
  const void         *buffer;
  gsize               count_requested;
  gssize              count_written;
} WriteData;

static void
free_write_data (WriteData *op)
{
  g_slice_free (WriteData, op);
}

static void
write_async_thread (GTask        *task,
                    gpointer      source_object,
                    gpointer      task_data,
                    GCancellable *cancellable)
{
  GOutputStream *stream = source_object;
  WriteData *op = task_data;
  GOutputStreamClass *class;
  GError *error = NULL;
  gssize count_written;

  class = G_OUTPUT_STREAM_GET_CLASS (stream);
  count_written = class->write_fn (stream, op->buffer, op->count_requested,
                                   cancellable, &error);
  if (count_written == -1)
    g_task_return_error (task, error);
  else
    g_task_return_int (task, count_written);
}

static void write_async_pollable (GPollableOutputStream *stream,
                                  GTask                 *task);

static gboolean
write_async_pollable_ready (GPollableOutputStream *stream,
			    gpointer               user_data)
{
  GTask *task = user_data;

  write_async_pollable (stream, task);
  return FALSE;
}

static void
write_async_pollable (GPollableOutputStream *stream,
                      GTask                 *task)
{
  GError *error = NULL;
  WriteData *op = g_task_get_task_data (task);
  gssize count_written;

  if (g_task_return_error_if_cancelled (task))
    return;

  count_written = G_POLLABLE_OUTPUT_STREAM_GET_INTERFACE (stream)->
    write_nonblocking (stream, op->buffer, op->count_requested, &error);

  if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_WOULD_BLOCK))
    {
      GSource *source;

      g_error_free (error);

      source = g_pollable_output_stream_create_source (stream,
                                                       g_task_get_cancellable (task));
      g_task_attach_source (task, source,
                            (GSourceFunc) write_async_pollable_ready);
      g_source_unref (source);
      return;
    }

  if (count_written == -1)
    g_task_return_error (task, error);
  else
    g_task_return_int (task, count_written);
}

static void
g_output_stream_real_write_async (GOutputStream       *stream,
                                  const void          *buffer,
                                  gsize                count,
                                  int                  io_priority,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data)
{
  GTask *task;
  WriteData *op;

  op = g_slice_new0 (WriteData);
  task = g_task_new (stream, cancellable, callback, user_data);
  g_task_set_check_cancellable (task, FALSE);
  g_task_set_task_data (task, op, (GDestroyNotify) free_write_data);
  op->buffer = buffer;
  op->count_requested = count;

  if (!g_output_stream_async_write_is_via_threads (stream))
    write_async_pollable (G_POLLABLE_OUTPUT_STREAM (stream), task);
  else
    g_task_run_in_thread (task, write_async_thread);
  g_object_unref (task);
}

static gssize
g_output_stream_real_write_finish (GOutputStream  *stream,
                                   GAsyncResult   *result,
                                   GError        **error)
{
  g_return_val_if_fail (g_task_is_valid (result, stream), FALSE);

  return g_task_propagate_int (G_TASK (result), error);
}

typedef struct {
  GInputStream *source;
  GOutputStreamSpliceFlags flags;
  gssize n_read;
  gssize n_written;
  gsize bytes_copied;
  GError *error;
  guint8 *buffer;
} SpliceData;

static void
free_splice_data (SpliceData *op)
{
  g_clear_pointer (&op->buffer, g_free);
  g_object_unref (op->source);
  g_clear_error (&op->error);
  g_free (op);
}

static void
real_splice_async_complete_cb (GTask *task)
{
  SpliceData *op = g_task_get_task_data (task);

  if (op->flags & G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE &&
      !g_input_stream_is_closed (op->source))
    return;

  if (op->flags & G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET &&
      !g_output_stream_is_closed (g_task_get_source_object (task)))
    return;

  if (op->error != NULL)
    {
      g_task_return_error (task, op->error);
      op->error = NULL;
    }
  else
    {
      g_task_return_int (task, op->bytes_copied);
    }

  g_object_unref (task);
}

static void
real_splice_async_close_input_cb (GObject      *source,
                                  GAsyncResult *res,
                                  gpointer      user_data)
{
  GTask *task = user_data;

  g_input_stream_close_finish (G_INPUT_STREAM (source), res, NULL);

  real_splice_async_complete_cb (task);
}

static void
real_splice_async_close_output_cb (GObject      *source,
                                   GAsyncResult *res,
                                   gpointer      user_data)
{
  GTask *task = G_TASK (user_data);
  SpliceData *op = g_task_get_task_data (task);
  GError **error = (op->error == NULL) ? &op->error : NULL;

  g_output_stream_internal_close_finish (G_OUTPUT_STREAM (source), res, error);

  real_splice_async_complete_cb (task);
}

static void
real_splice_async_complete (GTask *task)
{
  SpliceData *op = g_task_get_task_data (task);
  gboolean done = TRUE;

  if (op->flags & G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE)
    {
      done = FALSE;
      g_input_stream_close_async (op->source, g_task_get_priority (task),
                                  g_task_get_cancellable (task),
                                  real_splice_async_close_input_cb, task);
    }

  if (op->flags & G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET)
    {
      done = FALSE;
      g_output_stream_internal_close_async (g_task_get_source_object (task),
                                            g_task_get_priority (task),
                                            g_task_get_cancellable (task),
                                            real_splice_async_close_output_cb,
                                            task);
    }

  if (done)
    real_splice_async_complete_cb (task);
}

static void real_splice_async_read_cb (GObject      *source,
                                       GAsyncResult *res,
                                       gpointer      user_data);

static void
real_splice_async_write_cb (GObject      *source,
                            GAsyncResult *res,
                            gpointer      user_data)
{
  GOutputStreamClass *class;
  GTask *task = G_TASK (user_data);
  SpliceData *op = g_task_get_task_data (task);
  gssize ret;

  class = G_OUTPUT_STREAM_GET_CLASS (g_task_get_source_object (task));

  ret = class->write_finish (G_OUTPUT_STREAM (source), res, &op->error);

  if (ret == -1)
    {
      real_splice_async_complete (task);
      return;
    }

  op->n_written += ret;
  op->bytes_copied += ret;
  if (op->bytes_copied > G_MAXSSIZE)
    op->bytes_copied = G_MAXSSIZE;

  if (op->n_written < op->n_read)
    {
      class->write_async (g_task_get_source_object (task),
                          op->buffer + op->n_written,
                          op->n_read - op->n_written,
                          g_task_get_priority (task),
                          g_task_get_cancellable (task),
                          real_splice_async_write_cb, task);
      return;
    }

  g_input_stream_read_async (op->source, op->buffer, 8192,
                             g_task_get_priority (task),
                             g_task_get_cancellable (task),
                             real_splice_async_read_cb, task);
}

static void
real_splice_async_read_cb (GObject      *source,
                           GAsyncResult *res,
                           gpointer      user_data)
{
  GOutputStreamClass *class;
  GTask *task = G_TASK (user_data);
  SpliceData *op = g_task_get_task_data (task);
  gssize ret;

  class = G_OUTPUT_STREAM_GET_CLASS (g_task_get_source_object (task));

  ret = g_input_stream_read_finish (op->source, res, &op->error);
  if (ret == -1 || ret == 0)
    {
      real_splice_async_complete (task);
      return;
    }

  op->n_read = ret;
  op->n_written = 0;

  class->write_async (g_task_get_source_object (task), op->buffer,
                      op->n_read, g_task_get_priority (task),
                      g_task_get_cancellable (task),
                      real_splice_async_write_cb, task);
}

static void
splice_async_thread (GTask        *task,
                     gpointer      source_object,
                     gpointer      task_data,
                     GCancellable *cancellable)
{
  GOutputStream *stream = source_object;
  SpliceData *op = task_data;
  GOutputStreamClass *class;
  GError *error = NULL;
  gssize bytes_copied;

  class = G_OUTPUT_STREAM_GET_CLASS (stream);
  
  bytes_copied = class->splice (stream,
                                op->source,
                                op->flags,
                                cancellable,
                                &error);
  if (bytes_copied == -1)
    g_task_return_error (task, error);
  else
    g_task_return_int (task, bytes_copied);
}

static void
g_output_stream_real_splice_async (GOutputStream             *stream,
                                   GInputStream              *source,
                                   GOutputStreamSpliceFlags   flags,
                                   int                        io_priority,
                                   GCancellable              *cancellable,
                                   GAsyncReadyCallback        callback,
                                   gpointer                   user_data)
{
  GTask *task;
  SpliceData *op;

  op = g_new0 (SpliceData, 1);
  task = g_task_new (stream, cancellable, callback, user_data);
  g_task_set_task_data (task, op, (GDestroyNotify)free_splice_data);
  op->flags = flags;
  op->source = g_object_ref (source);

  if (g_input_stream_async_read_is_via_threads (source) &&
      g_output_stream_async_write_is_via_threads (stream))
    {
      g_task_run_in_thread (task, splice_async_thread);
      g_object_unref (task);
    }
  else
    {
      op->buffer = g_malloc (8192);
      g_input_stream_read_async (op->source, op->buffer, 8192,
                                 g_task_get_priority (task),
                                 g_task_get_cancellable (task),
                                 real_splice_async_read_cb, task);
    }
}

static gssize
g_output_stream_real_splice_finish (GOutputStream  *stream,
                                    GAsyncResult   *result,
                                    GError        **error)
{
  g_return_val_if_fail (g_task_is_valid (result, stream), FALSE);

  return g_task_propagate_int (G_TASK (result), error);
}


static void
flush_async_thread (GTask        *task,
                    gpointer      source_object,
                    gpointer      task_data,
                    GCancellable *cancellable)
{
  GOutputStream *stream = source_object;
  GOutputStreamClass *class;
  gboolean result;
  GError *error = NULL;

  class = G_OUTPUT_STREAM_GET_CLASS (stream);
  result = TRUE;
  if (class->flush)
    result = class->flush (stream, cancellable, &error);

  if (result)
    g_task_return_boolean (task, TRUE);
  else
    g_task_return_error (task, error);
}

static void
g_output_stream_real_flush_async (GOutputStream       *stream,
                                  int                  io_priority,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data)
{
  GTask *task;

  task = g_task_new (stream, cancellable, callback, user_data);
  g_task_set_priority (task, io_priority);
  g_task_run_in_thread (task, flush_async_thread);
  g_object_unref (task);
}

static gboolean
g_output_stream_real_flush_finish (GOutputStream  *stream,
                                   GAsyncResult   *result,
                                   GError        **error)
{
  g_return_val_if_fail (g_task_is_valid (result, stream), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
close_async_thread (GTask        *task,
                    gpointer      source_object,
                    gpointer      task_data,
                    GCancellable *cancellable)
{
  GOutputStream *stream = source_object;
  GOutputStreamClass *class;
  GError *error = NULL;
  gboolean result = TRUE;

  class = G_OUTPUT_STREAM_GET_CLASS (stream);

  /* Do a flush here if there is a flush function, and we did not have to do
   * an async flush before (see g_output_stream_close_async)
   */
  if (class->flush != NULL &&
      (class->flush_async == NULL ||
       class->flush_async == g_output_stream_real_flush_async))
    {
      result = class->flush (stream, cancellable, &error);
    }

  /* Auto handling of cancelation disabled, and ignore
     cancellation, since we want to close things anyway, although
     possibly in a quick-n-dirty way. At least we never want to leak
     open handles */

  if (class->close_fn)
    {
      /* Make sure to close, even if the flush failed (see sync close) */
      if (!result)
        class->close_fn (stream, cancellable, NULL);
      else
        result = class->close_fn (stream, cancellable, &error);
    }

  if (result)
    g_task_return_boolean (task, TRUE);
  else
    g_task_return_error (task, error);
}

static void
g_output_stream_real_close_async (GOutputStream       *stream,
                                  int                  io_priority,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data)
{
  GTask *task;

  task = g_task_new (stream, cancellable, callback, user_data);
  g_task_set_priority (task, io_priority);
  g_task_run_in_thread (task, close_async_thread);
  g_object_unref (task);
}

static gboolean
g_output_stream_real_close_finish (GOutputStream  *stream,
                                   GAsyncResult   *result,
                                   GError        **error)
{
  g_return_val_if_fail (g_task_is_valid (result, stream), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}
