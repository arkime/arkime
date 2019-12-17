/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright © 2008 codethink
 * Copyright © 2009 Red Hat, Inc.
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
 * Authors: Ryan Lortie <desrt@desrt.ca>
 *          Alexander Larsson <alexl@redhat.com>
 */

#include "config.h"
#include <glib.h>
#include "glibintl.h"

#include "giostream.h"
#include "gasyncresult.h"
#include "gioprivate.h"
#include "gtask.h"

/**
 * SECTION:giostream
 * @short_description: Base class for implementing read/write streams
 * @include: gio/gio.h
 * @see_also: #GInputStream, #GOutputStream
 *
 * GIOStream represents an object that has both read and write streams.
 * Generally the two streams act as separate input and output streams,
 * but they share some common resources and state. For instance, for
 * seekable streams, both streams may use the same position.
 *
 * Examples of #GIOStream objects are #GSocketConnection, which represents
 * a two-way network connection; and #GFileIOStream, which represents a
 * file handle opened in read-write mode.
 *
 * To do the actual reading and writing you need to get the substreams
 * with g_io_stream_get_input_stream() and g_io_stream_get_output_stream().
 *
 * The #GIOStream object owns the input and the output streams, not the other
 * way around, so keeping the substreams alive will not keep the #GIOStream
 * object alive. If the #GIOStream object is freed it will be closed, thus
 * closing the substreams, so even if the substreams stay alive they will
 * always return %G_IO_ERROR_CLOSED for all operations.
 *
 * To close a stream use g_io_stream_close() which will close the common
 * stream object and also the individual substreams. You can also close
 * the substreams themselves. In most cases this only marks the
 * substream as closed, so further I/O on it fails but common state in the
 * #GIOStream may still be open. However, some streams may support
 * "half-closed" states where one direction of the stream is actually shut down.
 *
 * Operations on #GIOStreams cannot be started while another operation on the
 * #GIOStream or its substreams is in progress. Specifically, an application can
 * read from the #GInputStream and write to the #GOutputStream simultaneously
 * (either in separate threads, or as asynchronous operations in the same
 * thread), but an application cannot start any #GIOStream operation while there
 * is a #GIOStream, #GInputStream or #GOutputStream operation in progress, and
 * an application can’t start any #GInputStream or #GOutputStream operation
 * while there is a #GIOStream operation in progress.
 *
 * This is a product of individual stream operations being associated with a
 * given #GMainContext (the thread-default context at the time the operation was
 * started), rather than entire streams being associated with a single
 * #GMainContext.
 *
 * GIO may run operations on #GIOStreams from other (worker) threads, and this
 * may be exposed to application code in the behaviour of wrapper streams, such
 * as #GBufferedInputStream or #GTlsConnection. With such wrapper APIs,
 * application code may only run operations on the base (wrapped) stream when
 * the wrapper stream is idle. Note that the semantics of such operations may
 * not be well-defined due to the state the wrapper stream leaves the base
 * stream in (though they are guaranteed not to crash).
 *
 * Since: 2.22
 */

enum
{
  PROP_0,
  PROP_INPUT_STREAM,
  PROP_OUTPUT_STREAM,
  PROP_CLOSED
};

struct _GIOStreamPrivate {
  guint closed : 1;
  guint pending : 1;
};

static gboolean g_io_stream_real_close        (GIOStream            *stream,
					       GCancellable         *cancellable,
					       GError              **error);
static void     g_io_stream_real_close_async  (GIOStream            *stream,
					       int                   io_priority,
					       GCancellable         *cancellable,
					       GAsyncReadyCallback   callback,
					       gpointer              user_data);
static gboolean g_io_stream_real_close_finish (GIOStream            *stream,
					       GAsyncResult         *result,
					       GError              **error);

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (GIOStream, g_io_stream, G_TYPE_OBJECT)

static void
g_io_stream_dispose (GObject *object)
{
  GIOStream *stream;

  stream = G_IO_STREAM (object);

  if (!stream->priv->closed)
    g_io_stream_close (stream, NULL, NULL);

  G_OBJECT_CLASS (g_io_stream_parent_class)->dispose (object);
}

static void
g_io_stream_init (GIOStream *stream)
{
  stream->priv = g_io_stream_get_instance_private (stream);
}

static void
g_io_stream_get_property (GObject    *object,
			  guint       prop_id,
			  GValue     *value,
			  GParamSpec *pspec)
{
  GIOStream *stream = G_IO_STREAM (object);

  switch (prop_id)
    {
      case PROP_CLOSED:
        g_value_set_boolean (value, stream->priv->closed);
        break;

      case PROP_INPUT_STREAM:
        g_value_set_object (value, g_io_stream_get_input_stream (stream));
        break;

      case PROP_OUTPUT_STREAM:
        g_value_set_object (value, g_io_stream_get_output_stream (stream));
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
g_io_stream_class_init (GIOStreamClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = g_io_stream_dispose;
  gobject_class->get_property = g_io_stream_get_property;

  klass->close_fn = g_io_stream_real_close;
  klass->close_async = g_io_stream_real_close_async;
  klass->close_finish = g_io_stream_real_close_finish;

  g_object_class_install_property (gobject_class, PROP_CLOSED,
                                   g_param_spec_boolean ("closed",
                                                         P_("Closed"),
                                                         P_("Is the stream closed"),
                                                         FALSE,
                                                         G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_INPUT_STREAM,
				   g_param_spec_object ("input-stream",
							P_("Input stream"),
							P_("The GInputStream to read from"),
							G_TYPE_INPUT_STREAM,
							G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_OUTPUT_STREAM,
				   g_param_spec_object ("output-stream",
							P_("Output stream"),
							P_("The GOutputStream to write to"),
							G_TYPE_OUTPUT_STREAM,
							G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
}

/**
 * g_io_stream_is_closed:
 * @stream: a #GIOStream
 *
 * Checks if a stream is closed.
 *
 * Returns: %TRUE if the stream is closed.
 *
 * Since: 2.22
 */
gboolean
g_io_stream_is_closed (GIOStream *stream)
{
  g_return_val_if_fail (G_IS_IO_STREAM (stream), TRUE);

  return stream->priv->closed;
}

/**
 * g_io_stream_get_input_stream:
 * @stream: a #GIOStream
 *
 * Gets the input stream for this object. This is used
 * for reading.
 *
 * Returns: (transfer none): a #GInputStream, owned by the #GIOStream.
 * Do not free.
 *
 * Since: 2.22
 */
GInputStream *
g_io_stream_get_input_stream (GIOStream *stream)
{
  GIOStreamClass *klass;

  klass = G_IO_STREAM_GET_CLASS (stream);

  g_assert (klass->get_input_stream != NULL);

  return klass->get_input_stream (stream);
}

/**
 * g_io_stream_get_output_stream:
 * @stream: a #GIOStream
 *
 * Gets the output stream for this object. This is used for
 * writing.
 *
 * Returns: (transfer none): a #GOutputStream, owned by the #GIOStream.
 * Do not free.
 *
 * Since: 2.22
 */
GOutputStream *
g_io_stream_get_output_stream (GIOStream *stream)
{
  GIOStreamClass *klass;

  klass = G_IO_STREAM_GET_CLASS (stream);

  g_assert (klass->get_output_stream != NULL);
  return klass->get_output_stream (stream);
}

/**
 * g_io_stream_has_pending:
 * @stream: a #GIOStream
 *
 * Checks if a stream has pending actions.
 *
 * Returns: %TRUE if @stream has pending actions.
 *
 * Since: 2.22
 **/
gboolean
g_io_stream_has_pending (GIOStream *stream)
{
  g_return_val_if_fail (G_IS_IO_STREAM (stream), FALSE);

  return stream->priv->pending;
}

/**
 * g_io_stream_set_pending:
 * @stream: a #GIOStream
 * @error: a #GError location to store the error occurring, or %NULL to
 *     ignore
 *
 * Sets @stream to have actions pending. If the pending flag is
 * already set or @stream is closed, it will return %FALSE and set
 * @error.
 *
 * Returns: %TRUE if pending was previously unset and is now set.
 *
 * Since: 2.22
 */
gboolean
g_io_stream_set_pending (GIOStream  *stream,
			 GError    **error)
{
  g_return_val_if_fail (G_IS_IO_STREAM (stream), FALSE);

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
 * g_io_stream_clear_pending:
 * @stream: a #GIOStream
 *
 * Clears the pending flag on @stream.
 *
 * Since: 2.22
 */
void
g_io_stream_clear_pending (GIOStream *stream)
{
  g_return_if_fail (G_IS_IO_STREAM (stream));

  stream->priv->pending = FALSE;
}

static gboolean
g_io_stream_real_close (GIOStream     *stream,
			GCancellable  *cancellable,
			GError       **error)
{
  gboolean res;

  res = g_output_stream_close (g_io_stream_get_output_stream (stream),
			       cancellable, error);

  /* If this errored out, unset error so that we don't report
     further errors, but still do the following ops */
  if (error != NULL && *error != NULL)
    error = NULL;

  res &= g_input_stream_close (g_io_stream_get_input_stream (stream),
			       cancellable, error);

  return res;
}

/**
 * g_io_stream_close:
 * @stream: a #GIOStream
 * @cancellable: (nullable): optional #GCancellable object, %NULL to ignore
 * @error: location to store the error occurring, or %NULL to ignore
 *
 * Closes the stream, releasing resources related to it. This will also
 * close the individual input and output streams, if they are not already
 * closed.
 *
 * Once the stream is closed, all other operations will return
 * %G_IO_ERROR_CLOSED. Closing a stream multiple times will not
 * return an error.
 *
 * Closing a stream will automatically flush any outstanding buffers
 * in the stream.
 *
 * Streams will be automatically closed when the last reference
 * is dropped, but you might want to call this function to make sure
 * resources are released as early as possible.
 *
 * Some streams might keep the backing store of the stream (e.g. a file
 * descriptor) open after the stream is closed. See the documentation for
 * the individual stream for details.
 *
 * On failure the first error that happened will be reported, but the
 * close operation will finish as much as possible. A stream that failed
 * to close will still return %G_IO_ERROR_CLOSED for all operations.
 * Still, it is important to check and report the error to the user,
 * otherwise there might be a loss of data as all data might not be written.
 *
 * If @cancellable is not NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned.
 * Cancelling a close will still leave the stream closed, but some streams
 * can use a faster close that doesn't block to e.g. check errors.
 *
 * The default implementation of this method just calls close on the
 * individual input/output streams.
 *
 * Returns: %TRUE on success, %FALSE on failure
 *
 * Since: 2.22
 */
gboolean
g_io_stream_close (GIOStream     *stream,
		   GCancellable  *cancellable,
		   GError       **error)
{
  GIOStreamClass *class;
  gboolean res;

  g_return_val_if_fail (G_IS_IO_STREAM (stream), FALSE);

  class = G_IO_STREAM_GET_CLASS (stream);

  if (stream->priv->closed)
    return TRUE;

  if (!g_io_stream_set_pending (stream, error))
    return FALSE;

  if (cancellable)
    g_cancellable_push_current (cancellable);

  res = TRUE;
  if (class->close_fn)
    res = class->close_fn (stream, cancellable, error);

  if (cancellable)
    g_cancellable_pop_current (cancellable);

  stream->priv->closed = TRUE;
  g_io_stream_clear_pending (stream);

  return res;
}

static void
async_ready_close_callback_wrapper (GObject      *source_object,
                                    GAsyncResult *res,
                                    gpointer      user_data)
{
  GIOStream *stream = G_IO_STREAM (source_object);
  GIOStreamClass *klass = G_IO_STREAM_GET_CLASS (stream);
  GTask *task = user_data;
  GError *error = NULL;
  gboolean success;

  stream->priv->closed = TRUE;
  g_io_stream_clear_pending (stream);

  if (g_async_result_legacy_propagate_error (res, &error))
    success = FALSE;
  else
    success = klass->close_finish (stream, res, &error);

  if (error)
    g_task_return_error (task, error);
  else
    g_task_return_boolean (task, success);

  g_object_unref (task);
}

/**
 * g_io_stream_close_async:
 * @stream: a #GIOStream
 * @io_priority: the io priority of the request
 * @cancellable: (nullable): optional cancellable object
 * @callback: (scope async): callback to call when the request is satisfied
 * @user_data: (closure): the data to pass to callback function
 *
 * Requests an asynchronous close of the stream, releasing resources
 * related to it. When the operation is finished @callback will be
 * called. You can then call g_io_stream_close_finish() to get
 * the result of the operation.
 *
 * For behaviour details see g_io_stream_close().
 *
 * The asynchronous methods have a default fallback that uses threads
 * to implement asynchronicity, so they are optional for inheriting
 * classes. However, if you override one you must override all.
 *
 * Since: 2.22
 */
void
g_io_stream_close_async (GIOStream           *stream,
			 int                  io_priority,
			 GCancellable        *cancellable,
			 GAsyncReadyCallback  callback,
			 gpointer             user_data)
{
  GIOStreamClass *class;
  GError *error = NULL;
  GTask *task;

  g_return_if_fail (G_IS_IO_STREAM (stream));

  task = g_task_new (stream, cancellable, callback, user_data);
  g_task_set_source_tag (task, g_io_stream_close_async);

  if (stream->priv->closed)
    {
      g_task_return_boolean (task, TRUE);
      g_object_unref (task);
      return;
    }

  if (!g_io_stream_set_pending (stream, &error))
    {
      g_task_return_error (task, error);
      g_object_unref (task);
      return;
    }

  class = G_IO_STREAM_GET_CLASS (stream);

  class->close_async (stream, io_priority, cancellable,
		      async_ready_close_callback_wrapper, task);
}

/**
 * g_io_stream_close_finish:
 * @stream: a #GIOStream
 * @result: a #GAsyncResult
 * @error: a #GError location to store the error occurring, or %NULL to
 *    ignore
 *
 * Closes a stream.
 *
 * Returns: %TRUE if stream was successfully closed, %FALSE otherwise.
 *
 * Since: 2.22
 */
gboolean
g_io_stream_close_finish (GIOStream     *stream,
			  GAsyncResult  *result,
			  GError       **error)
{
  g_return_val_if_fail (G_IS_IO_STREAM (stream), FALSE);
  g_return_val_if_fail (g_task_is_valid (result, stream), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}


static void
close_async_thread (GTask        *task,
                    gpointer      source_object,
                    gpointer      task_data,
                    GCancellable *cancellable)
{
  GIOStream *stream = source_object;
  GIOStreamClass *class;
  GError *error = NULL;
  gboolean result;

  class = G_IO_STREAM_GET_CLASS (stream);
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

typedef struct
{
  GError *error;
  gint pending;
} CloseAsyncData;

static void
stream_close_complete (GObject      *source,
                       GAsyncResult *result,
                       gpointer      user_data)
{
  GTask *task = user_data;
  CloseAsyncData *data;

  data = g_task_get_task_data (task);
  data->pending--;

  if (G_IS_OUTPUT_STREAM (source))
    {
      GError *error = NULL;

      /* Match behaviour with the sync route and give precedent to the
       * error returned from closing the output stream.
       */
      g_output_stream_close_finish (G_OUTPUT_STREAM (source), result, &error);
      if (error)
        {
          if (data->error)
            g_error_free (data->error);
          data->error = error;
        }
    }
  else
    g_input_stream_close_finish (G_INPUT_STREAM (source), result, data->error ? NULL : &data->error);

  if (data->pending == 0)
    {
      if (data->error)
        g_task_return_error (task, data->error);
      else
        g_task_return_boolean (task, TRUE);

      g_slice_free (CloseAsyncData, data);
      g_object_unref (task);
    }
}

static void
g_io_stream_real_close_async (GIOStream           *stream,
			      int                  io_priority,
			      GCancellable        *cancellable,
			      GAsyncReadyCallback  callback,
			      gpointer             user_data)
{
  GInputStream *input;
  GOutputStream *output;
  GTask *task;

  task = g_task_new (stream, cancellable, callback, user_data);
  g_task_set_source_tag (task, g_io_stream_real_close_async);
  g_task_set_check_cancellable (task, FALSE);
  g_task_set_priority (task, io_priority);

  input = g_io_stream_get_input_stream (stream);
  output = g_io_stream_get_output_stream (stream);

  if (g_input_stream_async_close_is_via_threads (input) && g_output_stream_async_close_is_via_threads (output))
    {
      /* No sense in dispatching to the thread twice -- just do it all
       * in one go.
       */
      g_task_run_in_thread (task, close_async_thread);
      g_object_unref (task);
    }
  else
    {
      CloseAsyncData *data;

      /* We should avoid dispatching to another thread in case either
       * object that would not do it for itself because it may not be
       * threadsafe.
       */
      data = g_slice_new (CloseAsyncData);
      data->error = NULL;
      data->pending = 2;

      g_task_set_task_data (task, data, NULL);
      g_input_stream_close_async (input, io_priority, cancellable, stream_close_complete, task);
      g_output_stream_close_async (output, io_priority, cancellable, stream_close_complete, task);
    }
}

static gboolean
g_io_stream_real_close_finish (GIOStream     *stream,
			       GAsyncResult  *result,
			       GError       **error)
{
  g_return_val_if_fail (g_task_is_valid (result, stream), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

typedef struct
{
  GIOStream *stream1;
  GIOStream *stream2;
  GIOStreamSpliceFlags flags;
  gint io_priority;
  GCancellable *cancellable;
  gulong cancelled_id;
  GCancellable *op1_cancellable;
  GCancellable *op2_cancellable;
  guint completed;
  GError *error;
} SpliceContext;

static void
splice_context_free (SpliceContext *ctx)
{
  g_object_unref (ctx->stream1);
  g_object_unref (ctx->stream2);
  if (ctx->cancellable != NULL)
    g_object_unref (ctx->cancellable);
  g_object_unref (ctx->op1_cancellable);
  g_object_unref (ctx->op2_cancellable);
  g_clear_error (&ctx->error);
  g_slice_free (SpliceContext, ctx);
}

static void
splice_complete (GTask         *task,
                 SpliceContext *ctx)
{
  if (ctx->cancelled_id != 0)
    g_cancellable_disconnect (ctx->cancellable, ctx->cancelled_id);
  ctx->cancelled_id = 0;

  if (ctx->error != NULL)
    {
      g_task_return_error (task, ctx->error);
      ctx->error = NULL;
    }
  else
    g_task_return_boolean (task, TRUE);
}

static void
splice_close_cb (GObject      *iostream,
                 GAsyncResult *res,
                 gpointer      user_data)
{
  GTask *task = user_data;
  SpliceContext *ctx = g_task_get_task_data (task);
  GError *error = NULL;

  g_io_stream_close_finish (G_IO_STREAM (iostream), res, &error);

  ctx->completed++;

  /* Keep the first error that occurred */
  if (error != NULL && ctx->error == NULL)
    ctx->error = error;
  else
    g_clear_error (&error);

  /* If all operations are done, complete now */
  if (ctx->completed == 4)
    splice_complete (task, ctx);

  g_object_unref (task);
}

static void
splice_cb (GObject      *ostream,
           GAsyncResult *res,
           gpointer      user_data)
{
  GTask *task = user_data;
  SpliceContext *ctx = g_task_get_task_data (task);
  GError *error = NULL;

  g_output_stream_splice_finish (G_OUTPUT_STREAM (ostream), res, &error);

  ctx->completed++;

  /* ignore cancellation error if it was not requested by the user */
  if (error != NULL &&
      g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED) &&
      (ctx->cancellable == NULL ||
       !g_cancellable_is_cancelled (ctx->cancellable)))
    g_clear_error (&error);

  /* Keep the first error that occurred */
  if (error != NULL && ctx->error == NULL)
    ctx->error = error;
  else
    g_clear_error (&error);

   if (ctx->completed == 1 &&
       (ctx->flags & G_IO_STREAM_SPLICE_WAIT_FOR_BOTH) == 0)
    {
      /* We don't want to wait for the 2nd operation to finish, cancel it */
      g_cancellable_cancel (ctx->op1_cancellable);
      g_cancellable_cancel (ctx->op2_cancellable);
    }
  else if (ctx->completed == 2)
    {
      if (ctx->cancellable == NULL ||
          !g_cancellable_is_cancelled (ctx->cancellable))
        {
          g_cancellable_reset (ctx->op1_cancellable);
          g_cancellable_reset (ctx->op2_cancellable);
        }

      /* Close the IO streams if needed */
      if ((ctx->flags & G_IO_STREAM_SPLICE_CLOSE_STREAM1) != 0)
	{
	  g_io_stream_close_async (ctx->stream1,
                                   g_task_get_priority (task),
                                   ctx->op1_cancellable,
                                   splice_close_cb, g_object_ref (task));
	}
      else
        ctx->completed++;

      if ((ctx->flags & G_IO_STREAM_SPLICE_CLOSE_STREAM2) != 0)
	{
	  g_io_stream_close_async (ctx->stream2,
                                   g_task_get_priority (task),
                                   ctx->op2_cancellable,
                                   splice_close_cb, g_object_ref (task));
	}
      else
        ctx->completed++;

      /* If all operations are done, complete now */
      if (ctx->completed == 4)
        splice_complete (task, ctx);
    }

  g_object_unref (task);
}

static void
splice_cancelled_cb (GCancellable *cancellable,
                     GTask        *task)
{
  SpliceContext *ctx;

  ctx = g_task_get_task_data (task);
  g_cancellable_cancel (ctx->op1_cancellable);
  g_cancellable_cancel (ctx->op2_cancellable);
}

/**
 * g_io_stream_splice_async:
 * @stream1: a #GIOStream.
 * @stream2: a #GIOStream.
 * @flags: a set of #GIOStreamSpliceFlags.
 * @io_priority: the io priority of the request.
 * @cancellable: (nullable): optional #GCancellable object, %NULL to ignore.
 * @callback: (scope async): a #GAsyncReadyCallback.
 * @user_data: (closure): user data passed to @callback.
 *
 * Asyncronously splice the output stream of @stream1 to the input stream of
 * @stream2, and splice the output stream of @stream2 to the input stream of
 * @stream1.
 *
 * When the operation is finished @callback will be called.
 * You can then call g_io_stream_splice_finish() to get the
 * result of the operation.
 *
 * Since: 2.28
 **/
void
g_io_stream_splice_async (GIOStream            *stream1,
                          GIOStream            *stream2,
                          GIOStreamSpliceFlags  flags,
                          gint                  io_priority,
                          GCancellable         *cancellable,
                          GAsyncReadyCallback   callback,
                          gpointer              user_data)
{
  GTask *task;
  SpliceContext *ctx;
  GInputStream *istream;
  GOutputStream *ostream;

  if (cancellable != NULL && g_cancellable_is_cancelled (cancellable))
    {
      g_task_report_new_error (NULL, callback, user_data,
                               g_io_stream_splice_async,
                               G_IO_ERROR, G_IO_ERROR_CANCELLED,
                               "Operation has been cancelled");
      return;
    }

  ctx = g_slice_new0 (SpliceContext);
  ctx->stream1 = g_object_ref (stream1);
  ctx->stream2 = g_object_ref (stream2);
  ctx->flags = flags;
  ctx->op1_cancellable = g_cancellable_new ();
  ctx->op2_cancellable = g_cancellable_new ();
  ctx->completed = 0;

  task = g_task_new (NULL, cancellable, callback, user_data);
  g_task_set_source_tag (task, g_io_stream_splice_async);
  g_task_set_task_data (task, ctx, (GDestroyNotify) splice_context_free);

  if (cancellable != NULL)
    {
      ctx->cancellable = g_object_ref (cancellable);
      ctx->cancelled_id = g_cancellable_connect (cancellable,
          G_CALLBACK (splice_cancelled_cb), g_object_ref (task),
          g_object_unref);
    }

  istream = g_io_stream_get_input_stream (stream1);
  ostream = g_io_stream_get_output_stream (stream2);
  g_output_stream_splice_async (ostream, istream, G_OUTPUT_STREAM_SPLICE_NONE,
      io_priority, ctx->op1_cancellable, splice_cb,
      g_object_ref (task));

  istream = g_io_stream_get_input_stream (stream2);
  ostream = g_io_stream_get_output_stream (stream1);
  g_output_stream_splice_async (ostream, istream, G_OUTPUT_STREAM_SPLICE_NONE,
      io_priority, ctx->op2_cancellable, splice_cb,
      g_object_ref (task));

  g_object_unref (task);
}

/**
 * g_io_stream_splice_finish:
 * @result: a #GAsyncResult.
 * @error: a #GError location to store the error occurring, or %NULL to
 * ignore.
 *
 * Finishes an asynchronous io stream splice operation.
 *
 * Returns: %TRUE on success, %FALSE otherwise.
 *
 * Since: 2.28
 **/
gboolean
g_io_stream_splice_finish (GAsyncResult  *result,
                           GError       **error)
{
  g_return_val_if_fail (g_task_is_valid (result, NULL), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}
