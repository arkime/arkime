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
#include "glib.h"
#include <gioerror.h>
#include "glib-private.h"
#include "gcancellable.h"
#include "glibintl.h"


/**
 * SECTION:gcancellable
 * @short_description: Thread-safe Operation Cancellation Stack
 * @include: gio/gio.h
 *
 * GCancellable is a thread-safe operation cancellation stack used 
 * throughout GIO to allow for cancellation of synchronous and
 * asynchronous operations.
 */

enum {
  CANCELLED,
  LAST_SIGNAL
};

struct _GCancellablePrivate
{
  guint cancelled : 1;
  guint cancelled_running : 1;
  guint cancelled_running_waiting : 1;

  guint fd_refcount;
  GWakeup *wakeup;
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE_WITH_PRIVATE (GCancellable, g_cancellable, G_TYPE_OBJECT)

static GPrivate current_cancellable;
static GMutex cancellable_mutex;
static GCond cancellable_cond;

static void
g_cancellable_finalize (GObject *object)
{
  GCancellable *cancellable = G_CANCELLABLE (object);

  if (cancellable->priv->wakeup)
    GLIB_PRIVATE_CALL (g_wakeup_free) (cancellable->priv->wakeup);

  G_OBJECT_CLASS (g_cancellable_parent_class)->finalize (object);
}

static void
g_cancellable_class_init (GCancellableClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = g_cancellable_finalize;

  /**
   * GCancellable::cancelled:
   * @cancellable: a #GCancellable.
   * 
   * Emitted when the operation has been cancelled.
   * 
   * Can be used by implementations of cancellable operations. If the
   * operation is cancelled from another thread, the signal will be
   * emitted in the thread that cancelled the operation, not the
   * thread that is running the operation.
   *
   * Note that disconnecting from this signal (or any signal) in a
   * multi-threaded program is prone to race conditions. For instance
   * it is possible that a signal handler may be invoked even after
   * a call to g_signal_handler_disconnect() for that handler has
   * already returned.
   * 
   * There is also a problem when cancellation happens right before
   * connecting to the signal. If this happens the signal will
   * unexpectedly not be emitted, and checking before connecting to
   * the signal leaves a race condition where this is still happening.
   *
   * In order to make it safe and easy to connect handlers there
   * are two helper functions: g_cancellable_connect() and
   * g_cancellable_disconnect() which protect against problems
   * like this.
   *
   * An example of how to us this:
   * |[<!-- language="C" -->
   *     // Make sure we don't do unnecessary work if already cancelled
   *     if (g_cancellable_set_error_if_cancelled (cancellable, error))
   *       return;
   *
   *     // Set up all the data needed to be able to handle cancellation
   *     // of the operation
   *     my_data = my_data_new (...);
   *
   *     id = 0;
   *     if (cancellable)
   *       id = g_cancellable_connect (cancellable,
   *     			      G_CALLBACK (cancelled_handler)
   *     			      data, NULL);
   *
   *     // cancellable operation here...
   *
   *     g_cancellable_disconnect (cancellable, id);
   *
   *     // cancelled_handler is never called after this, it is now safe
   *     // to free the data
   *     my_data_free (my_data);  
   * ]|
   *
   * Note that the cancelled signal is emitted in the thread that
   * the user cancelled from, which may be the main thread. So, the
   * cancellable signal should not do something that can block.
   */
  signals[CANCELLED] =
    g_signal_new (I_("cancelled"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GCancellableClass, cancelled),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
  
}

static void
g_cancellable_init (GCancellable *cancellable)
{
  cancellable->priv = g_cancellable_get_instance_private (cancellable);
}

/**
 * g_cancellable_new:
 * 
 * Creates a new #GCancellable object.
 *
 * Applications that want to start one or more operations
 * that should be cancellable should create a #GCancellable
 * and pass it to the operations.
 *
 * One #GCancellable can be used in multiple consecutive
 * operations or in multiple concurrent operations.
 *  
 * Returns: a #GCancellable.
 **/
GCancellable *
g_cancellable_new (void)
{
  return g_object_new (G_TYPE_CANCELLABLE, NULL);
}

/**
 * g_cancellable_push_current:
 * @cancellable: a #GCancellable object
 *
 * Pushes @cancellable onto the cancellable stack. The current
 * cancellable can then be received using g_cancellable_get_current().
 *
 * This is useful when implementing cancellable operations in
 * code that does not allow you to pass down the cancellable object.
 *
 * This is typically called automatically by e.g. #GFile operations,
 * so you rarely have to call this yourself.
 **/
void
g_cancellable_push_current (GCancellable *cancellable)
{
  GSList *l;

  g_return_if_fail (cancellable != NULL);

  l = g_private_get (&current_cancellable);
  l = g_slist_prepend (l, cancellable);
  g_private_set (&current_cancellable, l);
}

/**
 * g_cancellable_pop_current:
 * @cancellable: a #GCancellable object
 *
 * Pops @cancellable off the cancellable stack (verifying that @cancellable
 * is on the top of the stack).
 **/
void
g_cancellable_pop_current (GCancellable *cancellable)
{
  GSList *l;

  l = g_private_get (&current_cancellable);

  g_return_if_fail (l != NULL);
  g_return_if_fail (l->data == cancellable);

  l = g_slist_delete_link (l, l);
  g_private_set (&current_cancellable, l);
}

/**
 * g_cancellable_get_current:
 *
 * Gets the top cancellable from the stack.
 *
 * Returns: (nullable) (transfer none): a #GCancellable from the top
 * of the stack, or %NULL if the stack is empty.
 **/
GCancellable *
g_cancellable_get_current  (void)
{
  GSList *l;

  l = g_private_get (&current_cancellable);
  if (l == NULL)
    return NULL;

  return G_CANCELLABLE (l->data);
}

/**
 * g_cancellable_reset:
 * @cancellable: a #GCancellable object.
 * 
 * Resets @cancellable to its uncancelled state.
 *
 * If cancellable is currently in use by any cancellable operation
 * then the behavior of this function is undefined.
 *
 * Note that it is generally not a good idea to reuse an existing
 * cancellable for more operations after it has been cancelled once,
 * as this function might tempt you to do. The recommended practice
 * is to drop the reference to a cancellable after cancelling it,
 * and let it die with the outstanding async operations. You should
 * create a fresh cancellable for further async operations.
 **/
void 
g_cancellable_reset (GCancellable *cancellable)
{
  GCancellablePrivate *priv;

  g_return_if_fail (G_IS_CANCELLABLE (cancellable));

  g_mutex_lock (&cancellable_mutex);

  priv = cancellable->priv;

  while (priv->cancelled_running)
    {
      priv->cancelled_running_waiting = TRUE;
      g_cond_wait (&cancellable_cond, &cancellable_mutex);
    }

  if (priv->cancelled)
    {
      if (priv->wakeup)
        GLIB_PRIVATE_CALL (g_wakeup_acknowledge) (priv->wakeup);

      priv->cancelled = FALSE;
    }

  g_mutex_unlock (&cancellable_mutex);
}

/**
 * g_cancellable_is_cancelled:
 * @cancellable: (nullable): a #GCancellable or %NULL
 *
 * Checks if a cancellable job has been cancelled.
 *
 * Returns: %TRUE if @cancellable is cancelled,
 * FALSE if called with %NULL or if item is not cancelled.
 **/
gboolean
g_cancellable_is_cancelled (GCancellable *cancellable)
{
  return cancellable != NULL && cancellable->priv->cancelled;
}

/**
 * g_cancellable_set_error_if_cancelled:
 * @cancellable: (nullable): a #GCancellable or %NULL
 * @error: #GError to append error state to
 *
 * If the @cancellable is cancelled, sets the error to notify
 * that the operation was cancelled.
 *
 * Returns: %TRUE if @cancellable was cancelled, %FALSE if it was not
 */
gboolean
g_cancellable_set_error_if_cancelled (GCancellable  *cancellable,
                                      GError       **error)
{
  if (g_cancellable_is_cancelled (cancellable))
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_CANCELLED,
                           _("Operation was cancelled"));
      return TRUE;
    }

  return FALSE;
}

/**
 * g_cancellable_get_fd:
 * @cancellable: a #GCancellable.
 * 
 * Gets the file descriptor for a cancellable job. This can be used to
 * implement cancellable operations on Unix systems. The returned fd will
 * turn readable when @cancellable is cancelled.
 *
 * You are not supposed to read from the fd yourself, just check for
 * readable status. Reading to unset the readable status is done
 * with g_cancellable_reset().
 * 
 * After a successful return from this function, you should use 
 * g_cancellable_release_fd() to free up resources allocated for 
 * the returned file descriptor.
 *
 * See also g_cancellable_make_pollfd().
 *
 * Returns: A valid file descriptor. %-1 if the file descriptor 
 * is not supported, or on errors. 
 **/
int
g_cancellable_get_fd (GCancellable *cancellable)
{
  GPollFD pollfd;

  if (cancellable == NULL)
	  return -1;

#ifdef G_OS_WIN32
  pollfd.fd = -1;
#else
  g_cancellable_make_pollfd (cancellable, &pollfd);
#endif

  return pollfd.fd;
}

/**
 * g_cancellable_make_pollfd:
 * @cancellable: (nullable): a #GCancellable or %NULL
 * @pollfd: a pointer to a #GPollFD
 * 
 * Creates a #GPollFD corresponding to @cancellable; this can be passed
 * to g_poll() and used to poll for cancellation. This is useful both
 * for unix systems without a native poll and for portability to
 * windows.
 *
 * When this function returns %TRUE, you should use 
 * g_cancellable_release_fd() to free up resources allocated for the 
 * @pollfd. After a %FALSE return, do not call g_cancellable_release_fd().
 *
 * If this function returns %FALSE, either no @cancellable was given or
 * resource limits prevent this function from allocating the necessary 
 * structures for polling. (On Linux, you will likely have reached 
 * the maximum number of file descriptors.) The suggested way to handle
 * these cases is to ignore the @cancellable.
 *
 * You are not supposed to read from the fd yourself, just check for
 * readable status. Reading to unset the readable status is done
 * with g_cancellable_reset().
 *
 * Returns: %TRUE if @pollfd was successfully initialized, %FALSE on 
 *          failure to prepare the cancellable.
 * 
 * Since: 2.22
 **/
gboolean
g_cancellable_make_pollfd (GCancellable *cancellable, GPollFD *pollfd)
{
  g_return_val_if_fail (pollfd != NULL, FALSE);
  if (cancellable == NULL)
    return FALSE;
  g_return_val_if_fail (G_IS_CANCELLABLE (cancellable), FALSE);

  g_mutex_lock (&cancellable_mutex);

  cancellable->priv->fd_refcount++;

  if (cancellable->priv->wakeup == NULL)
    {
      cancellable->priv->wakeup = GLIB_PRIVATE_CALL (g_wakeup_new) ();

      if (cancellable->priv->cancelled)
        GLIB_PRIVATE_CALL (g_wakeup_signal) (cancellable->priv->wakeup);
    }

  GLIB_PRIVATE_CALL (g_wakeup_get_pollfd) (cancellable->priv->wakeup, pollfd);

  g_mutex_unlock (&cancellable_mutex);

  return TRUE;
}

/**
 * g_cancellable_release_fd:
 * @cancellable: a #GCancellable
 *
 * Releases a resources previously allocated by g_cancellable_get_fd()
 * or g_cancellable_make_pollfd().
 *
 * For compatibility reasons with older releases, calling this function 
 * is not strictly required, the resources will be automatically freed
 * when the @cancellable is finalized. However, the @cancellable will
 * block scarce file descriptors until it is finalized if this function
 * is not called. This can cause the application to run out of file 
 * descriptors when many #GCancellables are used at the same time.
 * 
 * Since: 2.22
 **/
void
g_cancellable_release_fd (GCancellable *cancellable)
{
  GCancellablePrivate *priv;

  if (cancellable == NULL)
    return;

  g_return_if_fail (G_IS_CANCELLABLE (cancellable));
  g_return_if_fail (cancellable->priv->fd_refcount > 0);

  priv = cancellable->priv;

  g_mutex_lock (&cancellable_mutex);

  priv->fd_refcount--;
  if (priv->fd_refcount == 0)
    {
      GLIB_PRIVATE_CALL (g_wakeup_free) (priv->wakeup);
      priv->wakeup = NULL;
    }

  g_mutex_unlock (&cancellable_mutex);
}

/**
 * g_cancellable_cancel:
 * @cancellable: (nullable): a #GCancellable object.
 * 
 * Will set @cancellable to cancelled, and will emit the
 * #GCancellable::cancelled signal. (However, see the warning about
 * race conditions in the documentation for that signal if you are
 * planning to connect to it.)
 *
 * This function is thread-safe. In other words, you can safely call
 * it from a thread other than the one running the operation that was
 * passed the @cancellable.
 *
 * If @cancellable is %NULL, this function returns immediately for convenience.
 *
 * The convention within GIO is that cancelling an asynchronous
 * operation causes it to complete asynchronously. That is, if you
 * cancel the operation from the same thread in which it is running,
 * then the operation's #GAsyncReadyCallback will not be invoked until
 * the application returns to the main loop.
 **/
void
g_cancellable_cancel (GCancellable *cancellable)
{
  GCancellablePrivate *priv;

  if (cancellable == NULL ||
      cancellable->priv->cancelled)
    return;

  priv = cancellable->priv;

  g_mutex_lock (&cancellable_mutex);

  if (priv->cancelled)
    {
      g_mutex_unlock (&cancellable_mutex);
      return;
    }

  priv->cancelled = TRUE;
  priv->cancelled_running = TRUE;

  if (priv->wakeup)
    GLIB_PRIVATE_CALL (g_wakeup_signal) (priv->wakeup);

  g_mutex_unlock (&cancellable_mutex);

  g_object_ref (cancellable);
  g_signal_emit (cancellable, signals[CANCELLED], 0);

  g_mutex_lock (&cancellable_mutex);

  priv->cancelled_running = FALSE;
  if (priv->cancelled_running_waiting)
    g_cond_broadcast (&cancellable_cond);
  priv->cancelled_running_waiting = FALSE;

  g_mutex_unlock (&cancellable_mutex);

  g_object_unref (cancellable);
}

/**
 * g_cancellable_connect:
 * @cancellable: A #GCancellable.
 * @callback: The #GCallback to connect.
 * @data: Data to pass to @callback.
 * @data_destroy_func: (nullable): Free function for @data or %NULL.
 *
 * Convenience function to connect to the #GCancellable::cancelled
 * signal. Also handles the race condition that may happen
 * if the cancellable is cancelled right before connecting.
 *
 * @callback is called at most once, either directly at the
 * time of the connect if @cancellable is already cancelled,
 * or when @cancellable is cancelled in some thread.
 *
 * @data_destroy_func will be called when the handler is
 * disconnected, or immediately if the cancellable is already
 * cancelled.
 *
 * See #GCancellable::cancelled for details on how to use this.
 *
 * Since GLib 2.40, the lock protecting @cancellable is not held when
 * @callback is invoked.  This lifts a restriction in place for
 * earlier GLib versions which now makes it easier to write cleanup
 * code that unconditionally invokes e.g. g_cancellable_cancel().
 *
 * Returns: The id of the signal handler or 0 if @cancellable has already
 *          been cancelled.
 *
 * Since: 2.22
 */
gulong
g_cancellable_connect (GCancellable   *cancellable,
		       GCallback       callback,
		       gpointer        data,
		       GDestroyNotify  data_destroy_func)
{
  gulong id;

  g_return_val_if_fail (G_IS_CANCELLABLE (cancellable), 0);

  g_mutex_lock (&cancellable_mutex);

  if (cancellable->priv->cancelled)
    {
      void (*_callback) (GCancellable *cancellable,
                         gpointer      user_data);

      g_mutex_unlock (&cancellable_mutex);

      _callback = (void *)callback;
      id = 0;

      _callback (cancellable, data);

      if (data_destroy_func)
        data_destroy_func (data);
    }
  else
    {
      id = g_signal_connect_data (cancellable, "cancelled",
                                  callback, data,
                                  (GClosureNotify) data_destroy_func,
                                  0);

      g_mutex_unlock (&cancellable_mutex);
    }


  return id;
}

/**
 * g_cancellable_disconnect:
 * @cancellable: (nullable): A #GCancellable or %NULL.
 * @handler_id: Handler id of the handler to be disconnected, or `0`.
 *
 * Disconnects a handler from a cancellable instance similar to
 * g_signal_handler_disconnect().  Additionally, in the event that a
 * signal handler is currently running, this call will block until the
 * handler has finished.  Calling this function from a
 * #GCancellable::cancelled signal handler will therefore result in a
 * deadlock.
 *
 * This avoids a race condition where a thread cancels at the
 * same time as the cancellable operation is finished and the
 * signal handler is removed. See #GCancellable::cancelled for
 * details on how to use this.
 *
 * If @cancellable is %NULL or @handler_id is `0` this function does
 * nothing.
 *
 * Since: 2.22
 */
void
g_cancellable_disconnect (GCancellable  *cancellable,
			  gulong         handler_id)
{
  GCancellablePrivate *priv;

  if (handler_id == 0 ||  cancellable == NULL)
    return;

  g_mutex_lock (&cancellable_mutex);

  priv = cancellable->priv;

  while (priv->cancelled_running)
    {
      priv->cancelled_running_waiting = TRUE;
      g_cond_wait (&cancellable_cond, &cancellable_mutex);
    }

  g_signal_handler_disconnect (cancellable, handler_id);

  g_mutex_unlock (&cancellable_mutex);
}

typedef struct {
  GSource       source;

  GCancellable *cancellable;
  guint         cancelled_handler;
} GCancellableSource;

/*
 * We can't guarantee that the source still has references, so we are
 * relying on the fact that g_source_set_ready_time() no longer makes
 * assertions about the reference count - the source might be in the
 * window between last-unref and finalize, during which its refcount
 * is officially 0. However, we *can* guarantee that it's OK to
 * dereference it in a limited way, because we know we haven't yet reached
 * cancellable_source_finalize() - if we had, then we would have waited
 * for signal emission to finish, then disconnected the signal handler
 * under the lock.
 * See https://bugzilla.gnome.org/show_bug.cgi?id=791754
 */
static void
cancellable_source_cancelled (GCancellable *cancellable,
			      gpointer      user_data)
{
  GSource *source = user_data;

  g_source_set_ready_time (source, 0);
}

static gboolean
cancellable_source_dispatch (GSource     *source,
			     GSourceFunc  callback,
			     gpointer     user_data)
{
  GCancellableSourceFunc func = (GCancellableSourceFunc)callback;
  GCancellableSource *cancellable_source = (GCancellableSource *)source;

  g_source_set_ready_time (source, -1);
  return (*func) (cancellable_source->cancellable, user_data);
}

static void
cancellable_source_finalize (GSource *source)
{
  GCancellableSource *cancellable_source = (GCancellableSource *)source;

  if (cancellable_source->cancellable)
    {
      g_cancellable_disconnect (cancellable_source->cancellable,
                                cancellable_source->cancelled_handler);
      g_object_unref (cancellable_source->cancellable);
    }
}

static gboolean
cancellable_source_closure_callback (GCancellable *cancellable,
				     gpointer      data)
{
  GClosure *closure = data;

  GValue params = G_VALUE_INIT;
  GValue result_value = G_VALUE_INIT;
  gboolean result;

  g_value_init (&result_value, G_TYPE_BOOLEAN);

  g_value_init (&params, G_TYPE_CANCELLABLE);
  g_value_set_object (&params, cancellable);

  g_closure_invoke (closure, &result_value, 1, &params, NULL);

  result = g_value_get_boolean (&result_value);
  g_value_unset (&result_value);
  g_value_unset (&params);

  return result;
}

static GSourceFuncs cancellable_source_funcs =
{
  NULL,
  NULL,
  cancellable_source_dispatch,
  cancellable_source_finalize,
  (GSourceFunc)cancellable_source_closure_callback,
};

/**
 * g_cancellable_source_new: (skip)
 * @cancellable: (nullable): a #GCancellable, or %NULL
 *
 * Creates a source that triggers if @cancellable is cancelled and
 * calls its callback of type #GCancellableSourceFunc. This is
 * primarily useful for attaching to another (non-cancellable) source
 * with g_source_add_child_source() to add cancellability to it.
 *
 * For convenience, you can call this with a %NULL #GCancellable,
 * in which case the source will never trigger.
 *
 * The new #GSource will hold a reference to the #GCancellable.
 *
 * Returns: (transfer full): the new #GSource.
 *
 * Since: 2.28
 */
GSource *
g_cancellable_source_new (GCancellable *cancellable)
{
  GSource *source;
  GCancellableSource *cancellable_source;

  source = g_source_new (&cancellable_source_funcs, sizeof (GCancellableSource));
  g_source_set_name (source, "GCancellable");
  cancellable_source = (GCancellableSource *)source;

  if (cancellable)
    {
      cancellable_source->cancellable = g_object_ref (cancellable);

      /* We intentionally don't use g_cancellable_connect() here,
       * because we don't want the "at most once" behavior.
       */
      cancellable_source->cancelled_handler =
        g_signal_connect (cancellable, "cancelled",
                          G_CALLBACK (cancellable_source_cancelled),
                          source);
      if (g_cancellable_is_cancelled (cancellable))
        g_source_set_ready_time (source, 0);
    }

  return source;
}
