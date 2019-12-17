/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright 2011 Red Hat, Inc.
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
 */

#include "config.h"
#include "gio_trace.h"

#include "gtask.h"

#include "gasyncresult.h"
#include "gcancellable.h"
#include "glib-private.h"

#include "glibintl.h"

/**
 * SECTION:gtask
 * @short_description: Cancellable synchronous or asynchronous task
 *     and result
 * @include: gio/gio.h
 * @see_also: #GAsyncResult
 *
 * A #GTask represents and manages a cancellable "task".
 *
 * ## Asynchronous operations
 *
 * The most common usage of #GTask is as a #GAsyncResult, to
 * manage data during an asynchronous operation. You call
 * g_task_new() in the "start" method, followed by
 * g_task_set_task_data() and the like if you need to keep some
 * additional data associated with the task, and then pass the
 * task object around through your asynchronous operation.
 * Eventually, you will call a method such as
 * g_task_return_pointer() or g_task_return_error(), which will
 * save the value you give it and then invoke the task's callback
 * function in the
 * [thread-default main context][g-main-context-push-thread-default]
 * where it was created (waiting until the next iteration of the main
 * loop first, if necessary). The caller will pass the #GTask back to
 * the operation's finish function (as a #GAsyncResult), and you can
 * can use g_task_propagate_pointer() or the like to extract the
 * return value.
 *
 * Here is an example for using GTask as a GAsyncResult:
 * |[<!-- language="C" -->
 *     typedef struct {
 *       CakeFrostingType frosting;
 *       char *message;
 *     } DecorationData;
 *
 *     static void
 *     decoration_data_free (DecorationData *decoration)
 *     {
 *       g_free (decoration->message);
 *       g_slice_free (DecorationData, decoration);
 *     }
 *
 *     static void
 *     baked_cb (Cake     *cake,
 *               gpointer  user_data)
 *     {
 *       GTask *task = user_data;
 *       DecorationData *decoration = g_task_get_task_data (task);
 *       GError *error = NULL;
 *
 *       if (cake == NULL)
 *         {
 *           g_task_return_new_error (task, BAKER_ERROR, BAKER_ERROR_NO_FLOUR,
 *                                    "Go to the supermarket");
 *           g_object_unref (task);
 *           return;
 *         }
 *
 *       if (!cake_decorate (cake, decoration->frosting, decoration->message, &error))
 *         {
 *           g_object_unref (cake);
 *           // g_task_return_error() takes ownership of error
 *           g_task_return_error (task, error);
 *           g_object_unref (task);
 *           return;
 *         }
 *
 *       g_task_return_pointer (task, cake, g_object_unref);
 *       g_object_unref (task);
 *     }
 *
 *     void
 *     baker_bake_cake_async (Baker               *self,
 *                            guint                radius,
 *                            CakeFlavor           flavor,
 *                            CakeFrostingType     frosting,
 *                            const char          *message,
 *                            GCancellable        *cancellable,
 *                            GAsyncReadyCallback  callback,
 *                            gpointer             user_data)
 *     {
 *       GTask *task;
 *       DecorationData *decoration;
 *       Cake  *cake;
 *
 *       task = g_task_new (self, cancellable, callback, user_data);
 *       if (radius < 3)
 *         {
 *           g_task_return_new_error (task, BAKER_ERROR, BAKER_ERROR_TOO_SMALL,
 *                                    "%ucm radius cakes are silly",
 *                                    radius);
 *           g_object_unref (task);
 *           return;
 *         }
 *
 *       cake = _baker_get_cached_cake (self, radius, flavor, frosting, message);
 *       if (cake != NULL)
 *         {
 *           // _baker_get_cached_cake() returns a reffed cake
 *           g_task_return_pointer (task, cake, g_object_unref);
 *           g_object_unref (task);
 *           return;
 *         }
 *
 *       decoration = g_slice_new (DecorationData);
 *       decoration->frosting = frosting;
 *       decoration->message = g_strdup (message);
 *       g_task_set_task_data (task, decoration, (GDestroyNotify) decoration_data_free);
 *
 *       _baker_begin_cake (self, radius, flavor, cancellable, baked_cb, task);
 *     }
 *
 *     Cake *
 *     baker_bake_cake_finish (Baker         *self,
 *                             GAsyncResult  *result,
 *                             GError       **error)
 *     {
 *       g_return_val_if_fail (g_task_is_valid (result, self), NULL);
 *
 *       return g_task_propagate_pointer (G_TASK (result), error);
 *     }
 * ]|
 *
 * ## Chained asynchronous operations
 *
 * #GTask also tries to simplify asynchronous operations that
 * internally chain together several smaller asynchronous
 * operations. g_task_get_cancellable(), g_task_get_context(),
 * and g_task_get_priority() allow you to get back the task's
 * #GCancellable, #GMainContext, and [I/O priority][io-priority]
 * when starting a new subtask, so you don't have to keep track
 * of them yourself. g_task_attach_source() simplifies the case
 * of waiting for a source to fire (automatically using the correct
 * #GMainContext and priority).
 *
 * Here is an example for chained asynchronous operations:
 *   |[<!-- language="C" -->
 *     typedef struct {
 *       Cake *cake;
 *       CakeFrostingType frosting;
 *       char *message;
 *     } BakingData;
 *
 *     static void
 *     decoration_data_free (BakingData *bd)
 *     {
 *       if (bd->cake)
 *         g_object_unref (bd->cake);
 *       g_free (bd->message);
 *       g_slice_free (BakingData, bd);
 *     }
 *
 *     static void
 *     decorated_cb (Cake         *cake,
 *                   GAsyncResult *result,
 *                   gpointer      user_data)
 *     {
 *       GTask *task = user_data;
 *       GError *error = NULL;
 *
 *       if (!cake_decorate_finish (cake, result, &error))
 *         {
 *           g_object_unref (cake);
 *           g_task_return_error (task, error);
 *           g_object_unref (task);
 *           return;
 *         }
 *
 *       // baking_data_free() will drop its ref on the cake, so we have to
 *       // take another here to give to the caller.
 *       g_task_return_pointer (task, g_object_ref (cake), g_object_unref);
 *       g_object_unref (task);
 *     }
 *
 *     static gboolean
 *     decorator_ready (gpointer user_data)
 *     {
 *       GTask *task = user_data;
 *       BakingData *bd = g_task_get_task_data (task);
 *
 *       cake_decorate_async (bd->cake, bd->frosting, bd->message,
 *                            g_task_get_cancellable (task),
 *                            decorated_cb, task);
 *
 *       return G_SOURCE_REMOVE;
 *     }
 *
 *     static void
 *     baked_cb (Cake     *cake,
 *               gpointer  user_data)
 *     {
 *       GTask *task = user_data;
 *       BakingData *bd = g_task_get_task_data (task);
 *       GError *error = NULL;
 *
 *       if (cake == NULL)
 *         {
 *           g_task_return_new_error (task, BAKER_ERROR, BAKER_ERROR_NO_FLOUR,
 *                                    "Go to the supermarket");
 *           g_object_unref (task);
 *           return;
 *         }
 *
 *       bd->cake = cake;
 *
 *       // Bail out now if the user has already cancelled
 *       if (g_task_return_error_if_cancelled (task))
 *         {
 *           g_object_unref (task);
 *           return;
 *         }
 *
 *       if (cake_decorator_available (cake))
 *         decorator_ready (task);
 *       else
 *         {
 *           GSource *source;
 *
 *           source = cake_decorator_wait_source_new (cake);
 *           // Attach @source to @task's GMainContext and have it call
 *           // decorator_ready() when it is ready.
 *           g_task_attach_source (task, source, decorator_ready);
 *           g_source_unref (source);
 *         }
 *     }
 *
 *     void
 *     baker_bake_cake_async (Baker               *self,
 *                            guint                radius,
 *                            CakeFlavor           flavor,
 *                            CakeFrostingType     frosting,
 *                            const char          *message,
 *                            gint                 priority,
 *                            GCancellable        *cancellable,
 *                            GAsyncReadyCallback  callback,
 *                            gpointer             user_data)
 *     {
 *       GTask *task;
 *       BakingData *bd;
 *
 *       task = g_task_new (self, cancellable, callback, user_data);
 *       g_task_set_priority (task, priority);
 *
 *       bd = g_slice_new0 (BakingData);
 *       bd->frosting = frosting;
 *       bd->message = g_strdup (message);
 *       g_task_set_task_data (task, bd, (GDestroyNotify) baking_data_free);
 *
 *       _baker_begin_cake (self, radius, flavor, cancellable, baked_cb, task);
 *     }
 *
 *     Cake *
 *     baker_bake_cake_finish (Baker         *self,
 *                             GAsyncResult  *result,
 *                             GError       **error)
 *     {
 *       g_return_val_if_fail (g_task_is_valid (result, self), NULL);
 *
 *       return g_task_propagate_pointer (G_TASK (result), error);
 *     }
 * ]|
 *
 * ## Asynchronous operations from synchronous ones
 *
 * You can use g_task_run_in_thread() to turn a synchronous
 * operation into an asynchronous one, by running it in a thread.
 * When it completes, the result will be dispatched to the
 * [thread-default main context][g-main-context-push-thread-default]
 * where the #GTask was created.
 *
 * Running a task in a thread:
 *   |[<!-- language="C" -->
 *     typedef struct {
 *       guint radius;
 *       CakeFlavor flavor;
 *       CakeFrostingType frosting;
 *       char *message;
 *     } CakeData;
 *
 *     static void
 *     cake_data_free (CakeData *cake_data)
 *     {
 *       g_free (cake_data->message);
 *       g_slice_free (CakeData, cake_data);
 *     }
 *
 *     static void
 *     bake_cake_thread (GTask         *task,
 *                       gpointer       source_object,
 *                       gpointer       task_data,
 *                       GCancellable  *cancellable)
 *     {
 *       Baker *self = source_object;
 *       CakeData *cake_data = task_data;
 *       Cake *cake;
 *       GError *error = NULL;
 *
 *       cake = bake_cake (baker, cake_data->radius, cake_data->flavor,
 *                         cake_data->frosting, cake_data->message,
 *                         cancellable, &error);
 *       if (cake)
 *         g_task_return_pointer (task, cake, g_object_unref);
 *       else
 *         g_task_return_error (task, error);
 *     }
 *
 *     void
 *     baker_bake_cake_async (Baker               *self,
 *                            guint                radius,
 *                            CakeFlavor           flavor,
 *                            CakeFrostingType     frosting,
 *                            const char          *message,
 *                            GCancellable        *cancellable,
 *                            GAsyncReadyCallback  callback,
 *                            gpointer             user_data)
 *     {
 *       CakeData *cake_data;
 *       GTask *task;
 *
 *       cake_data = g_slice_new (CakeData);
 *       cake_data->radius = radius;
 *       cake_data->flavor = flavor;
 *       cake_data->frosting = frosting;
 *       cake_data->message = g_strdup (message);
 *       task = g_task_new (self, cancellable, callback, user_data);
 *       g_task_set_task_data (task, cake_data, (GDestroyNotify) cake_data_free);
 *       g_task_run_in_thread (task, bake_cake_thread);
 *       g_object_unref (task);
 *     }
 *
 *     Cake *
 *     baker_bake_cake_finish (Baker         *self,
 *                             GAsyncResult  *result,
 *                             GError       **error)
 *     {
 *       g_return_val_if_fail (g_task_is_valid (result, self), NULL);
 *
 *       return g_task_propagate_pointer (G_TASK (result), error);
 *     }
 * ]|
 *
 * ## Adding cancellability to uncancellable tasks
 * 
 * Finally, g_task_run_in_thread() and g_task_run_in_thread_sync()
 * can be used to turn an uncancellable operation into a
 * cancellable one. If you call g_task_set_return_on_cancel(),
 * passing %TRUE, then if the task's #GCancellable is cancelled,
 * it will return control back to the caller immediately, while
 * allowing the task thread to continue running in the background
 * (and simply discarding its result when it finally does finish).
 * Provided that the task thread is careful about how it uses
 * locks and other externally-visible resources, this allows you
 * to make "GLib-friendly" asynchronous and cancellable
 * synchronous variants of blocking APIs.
 *
 * Cancelling a task:
 *   |[<!-- language="C" -->
 *     static void
 *     bake_cake_thread (GTask         *task,
 *                       gpointer       source_object,
 *                       gpointer       task_data,
 *                       GCancellable  *cancellable)
 *     {
 *       Baker *self = source_object;
 *       CakeData *cake_data = task_data;
 *       Cake *cake;
 *       GError *error = NULL;
 *
 *       cake = bake_cake (baker, cake_data->radius, cake_data->flavor,
 *                         cake_data->frosting, cake_data->message,
 *                         &error);
 *       if (error)
 *         {
 *           g_task_return_error (task, error);
 *           return;
 *         }
 *
 *       // If the task has already been cancelled, then we don't want to add
 *       // the cake to the cake cache. Likewise, we don't  want to have the
 *       // task get cancelled in the middle of updating the cache.
 *       // g_task_set_return_on_cancel() will return %TRUE here if it managed
 *       // to disable return-on-cancel, or %FALSE if the task was cancelled
 *       // before it could.
 *       if (g_task_set_return_on_cancel (task, FALSE))
 *         {
 *           // If the caller cancels at this point, their
 *           // GAsyncReadyCallback won't be invoked until we return,
 *           // so we don't have to worry that this code will run at
 *           // the same time as that code does. But if there were
 *           // other functions that might look at the cake cache,
 *           // then we'd probably need a GMutex here as well.
 *           baker_add_cake_to_cache (baker, cake);
 *           g_task_return_pointer (task, cake, g_object_unref);
 *         }
 *     }
 *
 *     void
 *     baker_bake_cake_async (Baker               *self,
 *                            guint                radius,
 *                            CakeFlavor           flavor,
 *                            CakeFrostingType     frosting,
 *                            const char          *message,
 *                            GCancellable        *cancellable,
 *                            GAsyncReadyCallback  callback,
 *                            gpointer             user_data)
 *     {
 *       CakeData *cake_data;
 *       GTask *task;
 *
 *       cake_data = g_slice_new (CakeData);
 *
 *       ...
 *
 *       task = g_task_new (self, cancellable, callback, user_data);
 *       g_task_set_task_data (task, cake_data, (GDestroyNotify) cake_data_free);
 *       g_task_set_return_on_cancel (task, TRUE);
 *       g_task_run_in_thread (task, bake_cake_thread);
 *     }
 *
 *     Cake *
 *     baker_bake_cake_sync (Baker               *self,
 *                           guint                radius,
 *                           CakeFlavor           flavor,
 *                           CakeFrostingType     frosting,
 *                           const char          *message,
 *                           GCancellable        *cancellable,
 *                           GError             **error)
 *     {
 *       CakeData *cake_data;
 *       GTask *task;
 *       Cake *cake;
 *
 *       cake_data = g_slice_new (CakeData);
 *
 *       ...
 *
 *       task = g_task_new (self, cancellable, NULL, NULL);
 *       g_task_set_task_data (task, cake_data, (GDestroyNotify) cake_data_free);
 *       g_task_set_return_on_cancel (task, TRUE);
 *       g_task_run_in_thread_sync (task, bake_cake_thread);
 *
 *       cake = g_task_propagate_pointer (task, error);
 *       g_object_unref (task);
 *       return cake;
 *     }
 * ]|
 *
 * ## Porting from GSimpleAsyncResult
 * 
 * #GTask's API attempts to be simpler than #GSimpleAsyncResult's
 * in several ways:
 * - You can save task-specific data with g_task_set_task_data(), and
 *   retrieve it later with g_task_get_task_data(). This replaces the
 *   abuse of g_simple_async_result_set_op_res_gpointer() for the same
 *   purpose with #GSimpleAsyncResult.
 * - In addition to the task data, #GTask also keeps track of the
 *   [priority][io-priority], #GCancellable, and
 *   #GMainContext associated with the task, so tasks that consist of
 *   a chain of simpler asynchronous operations will have easy access
 *   to those values when starting each sub-task.
 * - g_task_return_error_if_cancelled() provides simplified
 *   handling for cancellation. In addition, cancellation
 *   overrides any other #GTask return value by default, like
 *   #GSimpleAsyncResult does when
 *   g_simple_async_result_set_check_cancellable() is called.
 *   (You can use g_task_set_check_cancellable() to turn off that
 *   behavior.) On the other hand, g_task_run_in_thread()
 *   guarantees that it will always run your
 *   `task_func`, even if the task's #GCancellable
 *   is already cancelled before the task gets a chance to run;
 *   you can start your `task_func` with a
 *   g_task_return_error_if_cancelled() check if you need the
 *   old behavior.
 * - The "return" methods (eg, g_task_return_pointer())
 *   automatically cause the task to be "completed" as well, and
 *   there is no need to worry about the "complete" vs "complete
 *   in idle" distinction. (#GTask automatically figures out
 *   whether the task's callback can be invoked directly, or
 *   if it needs to be sent to another #GMainContext, or delayed
 *   until the next iteration of the current #GMainContext.)
 * - The "finish" functions for #GTask based operations are generally
 *   much simpler than #GSimpleAsyncResult ones, normally consisting
 *   of only a single call to g_task_propagate_pointer() or the like.
 *   Since g_task_propagate_pointer() "steals" the return value from
 *   the #GTask, it is not necessary to juggle pointers around to
 *   prevent it from being freed twice.
 * - With #GSimpleAsyncResult, it was common to call
 *   g_simple_async_result_propagate_error() from the
 *   `_finish()` wrapper function, and have
 *   virtual method implementations only deal with successful
 *   returns. This behavior is deprecated, because it makes it
 *   difficult for a subclass to chain to a parent class's async
 *   methods. Instead, the wrapper function should just be a
 *   simple wrapper, and the virtual method should call an
 *   appropriate `g_task_propagate_` function.
 *   Note that wrapper methods can now use
 *   g_async_result_legacy_propagate_error() to do old-style
 *   #GSimpleAsyncResult error-returning behavior, and
 *   g_async_result_is_tagged() to check if a result is tagged as
 *   having come from the `_async()` wrapper
 *   function (for "short-circuit" results, such as when passing
 *   0 to g_input_stream_read_async()).
 */

/**
 * GTask:
 *
 * The opaque object representing a synchronous or asynchronous task
 * and its result.
 */

struct _GTask {
  GObject parent_instance;

  gpointer source_object;
  gpointer source_tag;

  gpointer task_data;
  GDestroyNotify task_data_destroy;

  GMainContext *context;
  gint64 creation_time;
  gint priority;
  GCancellable *cancellable;
  gboolean check_cancellable;

  GAsyncReadyCallback callback;
  gpointer callback_data;
  gboolean completed;

  GTaskThreadFunc task_func;
  GMutex lock;
  GCond cond;
  gboolean return_on_cancel;
  gboolean thread_cancelled;
  gboolean synchronous;
  gboolean thread_complete;
  gboolean blocking_other_task;

  GError *error;
  union {
    gpointer pointer;
    gssize   size;
    gboolean boolean;
  } result;
  GDestroyNotify result_destroy;
  gboolean had_error;
  gboolean result_set;
};

#define G_TASK_IS_THREADED(task) ((task)->task_func != NULL)

struct _GTaskClass
{
  GObjectClass parent_class;
};

typedef enum
{
  PROP_COMPLETED = 1,
} GTaskProperty;

static void g_task_async_result_iface_init (GAsyncResultIface *iface);
static void g_task_thread_pool_init (void);

G_DEFINE_TYPE_WITH_CODE (GTask, g_task, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_RESULT,
                                                g_task_async_result_iface_init);
                         g_task_thread_pool_init ();)

static GThreadPool *task_pool;
static GMutex task_pool_mutex;
static GPrivate task_private = G_PRIVATE_INIT (NULL);
static GSource *task_pool_manager;
static guint64 task_wait_time;
static gint tasks_running;

/* When the task pool fills up and blocks, and the program keeps
 * queueing more tasks, we will slowly add more threads to the pool
 * (in case the existing tasks are trying to queue subtasks of their
 * own) until tasks start completing again. These "overflow" threads
 * will only run one task apiece, and then exit, so the pool will
 * eventually get back down to its base size.
 *
 * The base and multiplier below gives us 10 extra threads after about
 * a second of blocking, 30 after 5 seconds, 100 after a minute, and
 * 200 after 20 minutes.
 */
#define G_TASK_POOL_SIZE 10
#define G_TASK_WAIT_TIME_BASE 100000
#define G_TASK_WAIT_TIME_MULTIPLIER 1.03
#define G_TASK_WAIT_TIME_MAX (30 * 60 * 1000000)

static void
g_task_init (GTask *task)
{
  task->check_cancellable = TRUE;
}

static void
g_task_finalize (GObject *object)
{
  GTask *task = G_TASK (object);

  g_clear_object (&task->source_object);
  g_clear_object (&task->cancellable);

  if (task->context)
    g_main_context_unref (task->context);

  if (task->task_data_destroy)
    task->task_data_destroy (task->task_data);

  if (task->result_destroy && task->result.pointer)
    task->result_destroy (task->result.pointer);

  if (task->error)
      g_error_free (task->error);

  if (G_TASK_IS_THREADED (task))
    {
      g_mutex_clear (&task->lock);
      g_cond_clear (&task->cond);
    }

  G_OBJECT_CLASS (g_task_parent_class)->finalize (object);
}

/**
 * g_task_new:
 * @source_object: (nullable) (type GObject): the #GObject that owns
 *   this task, or %NULL.
 * @cancellable: (nullable): optional #GCancellable object, %NULL to ignore.
 * @callback: (scope async): a #GAsyncReadyCallback.
 * @callback_data: (closure): user data passed to @callback.
 *
 * Creates a #GTask acting on @source_object, which will eventually be
 * used to invoke @callback in the current
 * [thread-default main context][g-main-context-push-thread-default].
 *
 * Call this in the "start" method of your asynchronous method, and
 * pass the #GTask around throughout the asynchronous operation. You
 * can use g_task_set_task_data() to attach task-specific data to the
 * object, which you can retrieve later via g_task_get_task_data().
 *
 * By default, if @cancellable is cancelled, then the return value of
 * the task will always be %G_IO_ERROR_CANCELLED, even if the task had
 * already completed before the cancellation. This allows for
 * simplified handling in cases where cancellation may imply that
 * other objects that the task depends on have been destroyed. If you
 * do not want this behavior, you can use
 * g_task_set_check_cancellable() to change it.
 *
 * Returns: a #GTask.
 *
 * Since: 2.36
 */
GTask *
g_task_new (gpointer              source_object,
            GCancellable         *cancellable,
            GAsyncReadyCallback   callback,
            gpointer              callback_data)
{
  GTask *task;
  GSource *source;

  task = g_object_new (G_TYPE_TASK, NULL);
  task->source_object = source_object ? g_object_ref (source_object) : NULL;
  task->cancellable = cancellable ? g_object_ref (cancellable) : NULL;
  task->callback = callback;
  task->callback_data = callback_data;
  task->context = g_main_context_ref_thread_default ();

  source = g_main_current_source ();
  if (source)
    task->creation_time = g_source_get_time (source);

  TRACE (GIO_TASK_NEW (task, source_object, cancellable,
                       callback, callback_data));

  return task;
}

/**
 * g_task_report_error:
 * @source_object: (nullable) (type GObject): the #GObject that owns
 *   this task, or %NULL.
 * @callback: (scope async): a #GAsyncReadyCallback.
 * @callback_data: (closure): user data passed to @callback.
 * @source_tag: an opaque pointer indicating the source of this task
 * @error: (transfer full): error to report
 *
 * Creates a #GTask and then immediately calls g_task_return_error()
 * on it. Use this in the wrapper function of an asynchronous method
 * when you want to avoid even calling the virtual method. You can
 * then use g_async_result_is_tagged() in the finish method wrapper to
 * check if the result there is tagged as having been created by the
 * wrapper method, and deal with it appropriately if so.
 *
 * See also g_task_report_new_error().
 *
 * Since: 2.36
 */
void
g_task_report_error (gpointer             source_object,
                     GAsyncReadyCallback  callback,
                     gpointer             callback_data,
                     gpointer             source_tag,
                     GError              *error)
{
  GTask *task;

  task = g_task_new (source_object, NULL, callback, callback_data);
  g_task_set_source_tag (task, source_tag);
  g_task_return_error (task, error);
  g_object_unref (task);
}

/**
 * g_task_report_new_error:
 * @source_object: (nullable) (type GObject): the #GObject that owns
 *   this task, or %NULL.
 * @callback: (scope async): a #GAsyncReadyCallback.
 * @callback_data: (closure): user data passed to @callback.
 * @source_tag: an opaque pointer indicating the source of this task
 * @domain: a #GQuark.
 * @code: an error code.
 * @format: a string with format characters.
 * @...: a list of values to insert into @format.
 *
 * Creates a #GTask and then immediately calls
 * g_task_return_new_error() on it. Use this in the wrapper function
 * of an asynchronous method when you want to avoid even calling the
 * virtual method. You can then use g_async_result_is_tagged() in the
 * finish method wrapper to check if the result there is tagged as
 * having been created by the wrapper method, and deal with it
 * appropriately if so.
 *
 * See also g_task_report_error().
 *
 * Since: 2.36
 */
void
g_task_report_new_error (gpointer             source_object,
                         GAsyncReadyCallback  callback,
                         gpointer             callback_data,
                         gpointer             source_tag,
                         GQuark               domain,
                         gint                 code,
                         const char          *format,
                         ...)
{
  GError *error;
  va_list ap;

  va_start (ap, format);
  error = g_error_new_valist (domain, code, format, ap);
  va_end (ap);

  g_task_report_error (source_object, callback, callback_data,
                       source_tag, error);
}

/**
 * g_task_set_task_data:
 * @task: the #GTask
 * @task_data: (nullable): task-specific data
 * @task_data_destroy: (nullable): #GDestroyNotify for @task_data
 *
 * Sets @task's task data (freeing the existing task data, if any).
 *
 * Since: 2.36
 */
void
g_task_set_task_data (GTask          *task,
                      gpointer        task_data,
                      GDestroyNotify  task_data_destroy)
{
  g_return_if_fail (G_IS_TASK (task));

  if (task->task_data_destroy)
    task->task_data_destroy (task->task_data);

  task->task_data = task_data;
  task->task_data_destroy = task_data_destroy;

  TRACE (GIO_TASK_SET_TASK_DATA (task, task_data, task_data_destroy));
}

/**
 * g_task_set_priority:
 * @task: the #GTask
 * @priority: the [priority][io-priority] of the request
 *
 * Sets @task's priority. If you do not call this, it will default to
 * %G_PRIORITY_DEFAULT.
 *
 * This will affect the priority of #GSources created with
 * g_task_attach_source() and the scheduling of tasks run in threads,
 * and can also be explicitly retrieved later via
 * g_task_get_priority().
 *
 * Since: 2.36
 */
void
g_task_set_priority (GTask *task,
                     gint   priority)
{
  g_return_if_fail (G_IS_TASK (task));

  task->priority = priority;

  TRACE (GIO_TASK_SET_PRIORITY (task, priority));
}

/**
 * g_task_set_check_cancellable:
 * @task: the #GTask
 * @check_cancellable: whether #GTask will check the state of
 *   its #GCancellable for you.
 *
 * Sets or clears @task's check-cancellable flag. If this is %TRUE
 * (the default), then g_task_propagate_pointer(), etc, and
 * g_task_had_error() will check the task's #GCancellable first, and
 * if it has been cancelled, then they will consider the task to have
 * returned an "Operation was cancelled" error
 * (%G_IO_ERROR_CANCELLED), regardless of any other error or return
 * value the task may have had.
 *
 * If @check_cancellable is %FALSE, then the #GTask will not check the
 * cancellable itself, and it is up to @task's owner to do this (eg,
 * via g_task_return_error_if_cancelled()).
 *
 * If you are using g_task_set_return_on_cancel() as well, then
 * you must leave check-cancellable set %TRUE.
 *
 * Since: 2.36
 */
void
g_task_set_check_cancellable (GTask    *task,
                              gboolean  check_cancellable)
{
  g_return_if_fail (G_IS_TASK (task));
  g_return_if_fail (check_cancellable || !task->return_on_cancel);

  task->check_cancellable = check_cancellable;
}

static void g_task_thread_complete (GTask *task);

/**
 * g_task_set_return_on_cancel:
 * @task: the #GTask
 * @return_on_cancel: whether the task returns automatically when
 *   it is cancelled.
 *
 * Sets or clears @task's return-on-cancel flag. This is only
 * meaningful for tasks run via g_task_run_in_thread() or
 * g_task_run_in_thread_sync().
 *
 * If @return_on_cancel is %TRUE, then cancelling @task's
 * #GCancellable will immediately cause it to return, as though the
 * task's #GTaskThreadFunc had called
 * g_task_return_error_if_cancelled() and then returned.
 *
 * This allows you to create a cancellable wrapper around an
 * uninterruptable function. The #GTaskThreadFunc just needs to be
 * careful that it does not modify any externally-visible state after
 * it has been cancelled. To do that, the thread should call
 * g_task_set_return_on_cancel() again to (atomically) set
 * return-on-cancel %FALSE before making externally-visible changes;
 * if the task gets cancelled before the return-on-cancel flag could
 * be changed, g_task_set_return_on_cancel() will indicate this by
 * returning %FALSE.
 *
 * You can disable and re-enable this flag multiple times if you wish.
 * If the task's #GCancellable is cancelled while return-on-cancel is
 * %FALSE, then calling g_task_set_return_on_cancel() to set it %TRUE
 * again will cause the task to be cancelled at that point.
 *
 * If the task's #GCancellable is already cancelled before you call
 * g_task_run_in_thread()/g_task_run_in_thread_sync(), then the
 * #GTaskThreadFunc will still be run (for consistency), but the task
 * will also be completed right away.
 *
 * Returns: %TRUE if @task's return-on-cancel flag was changed to
 *   match @return_on_cancel. %FALSE if @task has already been
 *   cancelled.
 *
 * Since: 2.36
 */
gboolean
g_task_set_return_on_cancel (GTask    *task,
                             gboolean  return_on_cancel)
{
  g_return_val_if_fail (G_IS_TASK (task), FALSE);
  g_return_val_if_fail (task->check_cancellable || !return_on_cancel, FALSE);

  if (!G_TASK_IS_THREADED (task))
    {
      task->return_on_cancel = return_on_cancel;
      return TRUE;
    }

  g_mutex_lock (&task->lock);
  if (task->thread_cancelled)
    {
      if (return_on_cancel && !task->return_on_cancel)
        {
          g_mutex_unlock (&task->lock);
          g_task_thread_complete (task);
        }
      else
        g_mutex_unlock (&task->lock);
      return FALSE;
    }
  task->return_on_cancel = return_on_cancel;
  g_mutex_unlock (&task->lock);

  return TRUE;
}

/**
 * g_task_set_source_tag:
 * @task: the #GTask
 * @source_tag: an opaque pointer indicating the source of this task
 *
 * Sets @task's source tag. You can use this to tag a task return
 * value with a particular pointer (usually a pointer to the function
 * doing the tagging) and then later check it using
 * g_task_get_source_tag() (or g_async_result_is_tagged()) in the
 * task's "finish" function, to figure out if the response came from a
 * particular place.
 *
 * Since: 2.36
 */
void
g_task_set_source_tag (GTask    *task,
                       gpointer  source_tag)
{
  g_return_if_fail (G_IS_TASK (task));

  task->source_tag = source_tag;

  TRACE (GIO_TASK_SET_SOURCE_TAG (task, source_tag));
}

/**
 * g_task_get_source_object:
 * @task: a #GTask
 *
 * Gets the source object from @task. Like
 * g_async_result_get_source_object(), but does not ref the object.
 *
 * Returns: (transfer none) (nullable) (type GObject): @task's source object, or %NULL
 *
 * Since: 2.36
 */
gpointer
g_task_get_source_object (GTask *task)
{
  g_return_val_if_fail (G_IS_TASK (task), NULL);

  return task->source_object;
}

static GObject *
g_task_ref_source_object (GAsyncResult *res)
{
  GTask *task = G_TASK (res);

  if (task->source_object)
    return g_object_ref (task->source_object);
  else
    return NULL;
}

/**
 * g_task_get_task_data:
 * @task: a #GTask
 *
 * Gets @task's `task_data`.
 *
 * Returns: (transfer none): @task's `task_data`.
 *
 * Since: 2.36
 */
gpointer
g_task_get_task_data (GTask *task)
{
  g_return_val_if_fail (G_IS_TASK (task), NULL);

  return task->task_data;
}

/**
 * g_task_get_priority:
 * @task: a #GTask
 *
 * Gets @task's priority
 *
 * Returns: @task's priority
 *
 * Since: 2.36
 */
gint
g_task_get_priority (GTask *task)
{
  g_return_val_if_fail (G_IS_TASK (task), G_PRIORITY_DEFAULT);

  return task->priority;
}

/**
 * g_task_get_context:
 * @task: a #GTask
 *
 * Gets the #GMainContext that @task will return its result in (that
 * is, the context that was the
 * [thread-default main context][g-main-context-push-thread-default]
 * at the point when @task was created).
 *
 * This will always return a non-%NULL value, even if the task's
 * context is the default #GMainContext.
 *
 * Returns: (transfer none): @task's #GMainContext
 *
 * Since: 2.36
 */
GMainContext *
g_task_get_context (GTask *task)
{
  g_return_val_if_fail (G_IS_TASK (task), NULL);

  return task->context;
}

/**
 * g_task_get_cancellable:
 * @task: a #GTask
 *
 * Gets @task's #GCancellable
 *
 * Returns: (transfer none): @task's #GCancellable
 *
 * Since: 2.36
 */
GCancellable *
g_task_get_cancellable (GTask *task)
{
  g_return_val_if_fail (G_IS_TASK (task), NULL);

  return task->cancellable;
}

/**
 * g_task_get_check_cancellable:
 * @task: the #GTask
 *
 * Gets @task's check-cancellable flag. See
 * g_task_set_check_cancellable() for more details.
 *
 * Since: 2.36
 */
gboolean
g_task_get_check_cancellable (GTask *task)
{
  g_return_val_if_fail (G_IS_TASK (task), FALSE);

  return task->check_cancellable;
}

/**
 * g_task_get_return_on_cancel:
 * @task: the #GTask
 *
 * Gets @task's return-on-cancel flag. See
 * g_task_set_return_on_cancel() for more details.
 *
 * Since: 2.36
 */
gboolean
g_task_get_return_on_cancel (GTask *task)
{
  g_return_val_if_fail (G_IS_TASK (task), FALSE);

  return task->return_on_cancel;
}

/**
 * g_task_get_source_tag:
 * @task: a #GTask
 *
 * Gets @task's source tag. See g_task_set_source_tag().
 *
 * Returns: (transfer none): @task's source tag
 *
 * Since: 2.36
 */
gpointer
g_task_get_source_tag (GTask *task)
{
  g_return_val_if_fail (G_IS_TASK (task), NULL);

  return task->source_tag;
}


static void
g_task_return_now (GTask *task)
{
  TRACE (GIO_TASK_BEFORE_RETURN (task, task->source_object, task->callback,
                                 task->callback_data));

  g_main_context_push_thread_default (task->context);

  if (task->callback != NULL)
    {
      task->callback (task->source_object,
                      G_ASYNC_RESULT (task),
                      task->callback_data);
    }

  task->completed = TRUE;
  g_object_notify (G_OBJECT (task), "completed");

  g_main_context_pop_thread_default (task->context);
}

static gboolean
complete_in_idle_cb (gpointer task)
{
  g_task_return_now (task);
  g_object_unref (task);
  return FALSE;
}

typedef enum {
  G_TASK_RETURN_SUCCESS,
  G_TASK_RETURN_ERROR,
  G_TASK_RETURN_FROM_THREAD
} GTaskReturnType;

static void
g_task_return (GTask           *task,
               GTaskReturnType  type)
{
  GSource *source;

  if (type == G_TASK_RETURN_SUCCESS)
    task->result_set = TRUE;

  if (task->synchronous)
    return;

  /* Normally we want to invoke the task's callback when its return
   * value is set. But if the task is running in a thread, then we
   * want to wait until after the task_func returns, to simplify
   * locking/refcounting/etc.
   */
  if (G_TASK_IS_THREADED (task) && type != G_TASK_RETURN_FROM_THREAD)
    return;

  g_object_ref (task);

  /* See if we can complete the task immediately. First, we have to be
   * running inside the task's thread/GMainContext.
   */
  source = g_main_current_source ();
  if (source && g_source_get_context (source) == task->context)
    {
      /* Second, we can only complete immediately if this is not the
       * same iteration of the main loop that the task was created in.
       */
      if (g_source_get_time (source) > task->creation_time)
        {
          g_task_return_now (task);
          g_object_unref (task);
          return;
        }
    }

  /* Otherwise, complete in the next iteration */
  source = g_idle_source_new ();
  g_source_set_name (source, "[gio] complete_in_idle_cb");
  g_task_attach_source (task, source, complete_in_idle_cb);
  g_source_unref (source);
}


/**
 * GTaskThreadFunc:
 * @task: the #GTask
 * @source_object: (type GObject): @task's source object
 * @task_data: @task's task data
 * @cancellable: @task's #GCancellable, or %NULL
 *
 * The prototype for a task function to be run in a thread via
 * g_task_run_in_thread() or g_task_run_in_thread_sync().
 *
 * If the return-on-cancel flag is set on @task, and @cancellable gets
 * cancelled, then the #GTask will be completed immediately (as though
 * g_task_return_error_if_cancelled() had been called), without
 * waiting for the task function to complete. However, the task
 * function will continue running in its thread in the background. The
 * function therefore needs to be careful about how it uses
 * externally-visible state in this case. See
 * g_task_set_return_on_cancel() for more details.
 *
 * Other than in that case, @task will be completed when the
 * #GTaskThreadFunc returns, not when it calls a
 * `g_task_return_` function.
 *
 * Since: 2.36
 */

static void task_thread_cancelled (GCancellable *cancellable,
                                   gpointer      user_data);

static void
g_task_thread_complete (GTask *task)
{
  g_mutex_lock (&task->lock);
  if (task->thread_complete)
    {
      /* The task belatedly completed after having been cancelled
       * (or was cancelled in the midst of being completed).
       */
      g_mutex_unlock (&task->lock);
      return;
    }

  TRACE (GIO_TASK_AFTER_RUN_IN_THREAD (task, task->thread_cancelled));

  task->thread_complete = TRUE;
  g_mutex_unlock (&task->lock);

  if (task->cancellable)
    g_signal_handlers_disconnect_by_func (task->cancellable, task_thread_cancelled, task);

  if (task->synchronous)
    g_cond_signal (&task->cond);
  else
    g_task_return (task, G_TASK_RETURN_FROM_THREAD);
}

static gboolean
task_pool_manager_timeout (gpointer user_data)
{
  g_mutex_lock (&task_pool_mutex);
  g_thread_pool_set_max_threads (task_pool, tasks_running + 1, NULL);
  g_source_set_ready_time (task_pool_manager, -1);
  g_mutex_unlock (&task_pool_mutex);

  return TRUE;
}

static void
g_task_thread_setup (void)
{
  g_private_set (&task_private, GUINT_TO_POINTER (TRUE));
  g_mutex_lock (&task_pool_mutex);
  tasks_running++;

  if (tasks_running == G_TASK_POOL_SIZE)
    task_wait_time = G_TASK_WAIT_TIME_BASE;
  else if (tasks_running > G_TASK_POOL_SIZE && task_wait_time < G_TASK_WAIT_TIME_MAX)
    task_wait_time *= G_TASK_WAIT_TIME_MULTIPLIER;

  if (tasks_running >= G_TASK_POOL_SIZE)
    g_source_set_ready_time (task_pool_manager, g_get_monotonic_time () + task_wait_time);

  g_mutex_unlock (&task_pool_mutex);
}

static void
g_task_thread_cleanup (void)
{
  gint tasks_pending;

  g_mutex_lock (&task_pool_mutex);
  tasks_pending = g_thread_pool_unprocessed (task_pool);

  if (tasks_running > G_TASK_POOL_SIZE)
    g_thread_pool_set_max_threads (task_pool, tasks_running - 1, NULL);
  else if (tasks_running + tasks_pending < G_TASK_POOL_SIZE)
    g_source_set_ready_time (task_pool_manager, -1);

  tasks_running--;
  g_mutex_unlock (&task_pool_mutex);
  g_private_set (&task_private, GUINT_TO_POINTER (FALSE));
}

static void
g_task_thread_pool_thread (gpointer thread_data,
                           gpointer pool_data)
{
  GTask *task = thread_data;

  g_task_thread_setup ();

  task->task_func (task, task->source_object, task->task_data,
                   task->cancellable);
  g_task_thread_complete (task);
  g_object_unref (task);

  g_task_thread_cleanup ();
}

static void
task_thread_cancelled (GCancellable *cancellable,
                       gpointer      user_data)
{
  GTask *task = user_data;

  /* Move this task to the front of the queue - no need for
   * a complete resorting of the queue.
   */
  g_thread_pool_move_to_front (task_pool, task);

  g_mutex_lock (&task->lock);
  task->thread_cancelled = TRUE;

  if (!task->return_on_cancel)
    {
      g_mutex_unlock (&task->lock);
      return;
    }

  /* We don't actually set task->error; g_task_return_error() doesn't
   * use a lock, and g_task_propagate_error() will call
   * g_cancellable_set_error_if_cancelled() anyway.
   */
  g_mutex_unlock (&task->lock);
  g_task_thread_complete (task);
}

static void
task_thread_cancelled_disconnect_notify (gpointer  task,
                                         GClosure *closure)
{
  g_object_unref (task);
}

static void
g_task_start_task_thread (GTask           *task,
                          GTaskThreadFunc  task_func)
{
  g_mutex_init (&task->lock);
  g_cond_init (&task->cond);

  g_mutex_lock (&task->lock);

  TRACE (GIO_TASK_BEFORE_RUN_IN_THREAD (task, task_func));

  task->task_func = task_func;

  if (task->cancellable)
    {
      if (task->return_on_cancel &&
          g_cancellable_set_error_if_cancelled (task->cancellable,
                                                &task->error))
        {
          task->thread_cancelled = task->thread_complete = TRUE;
          TRACE (GIO_TASK_AFTER_RUN_IN_THREAD (task, task->thread_cancelled));
          g_thread_pool_push (task_pool, g_object_ref (task), NULL);
          return;
        }

      /* This introduces a reference count loop between the GTask and
       * GCancellable, but is necessary to avoid a race on finalising the GTask
       * between task_thread_cancelled() (in one thread) and
       * g_task_thread_complete() (in another).
       *
       * Accordingly, the signal handler *must* be removed once the task has
       * completed.
       */
      g_signal_connect_data (task->cancellable, "cancelled",
                             G_CALLBACK (task_thread_cancelled),
                             g_object_ref (task),
                             task_thread_cancelled_disconnect_notify, 0);
    }

  if (g_private_get (&task_private))
    task->blocking_other_task = TRUE;
  g_thread_pool_push (task_pool, g_object_ref (task), NULL);
}

/**
 * g_task_run_in_thread:
 * @task: a #GTask
 * @task_func: a #GTaskThreadFunc
 *
 * Runs @task_func in another thread. When @task_func returns, @task's
 * #GAsyncReadyCallback will be invoked in @task's #GMainContext.
 *
 * This takes a ref on @task until the task completes.
 *
 * See #GTaskThreadFunc for more details about how @task_func is handled.
 *
 * Although GLib currently rate-limits the tasks queued via
 * g_task_run_in_thread(), you should not assume that it will always
 * do this. If you have a very large number of tasks to run, but don't
 * want them to all run at once, you should only queue a limited
 * number of them at a time.
 *
 * Since: 2.36
 */
void
g_task_run_in_thread (GTask           *task,
                      GTaskThreadFunc  task_func)
{
  g_return_if_fail (G_IS_TASK (task));

  g_object_ref (task);
  g_task_start_task_thread (task, task_func);

  /* The task may already be cancelled, or g_thread_pool_push() may
   * have failed.
   */
  if (task->thread_complete)
    {
      g_mutex_unlock (&task->lock);
      g_task_return (task, G_TASK_RETURN_FROM_THREAD);
    }
  else
    g_mutex_unlock (&task->lock);

  g_object_unref (task);
}

/**
 * g_task_run_in_thread_sync:
 * @task: a #GTask
 * @task_func: a #GTaskThreadFunc
 *
 * Runs @task_func in another thread, and waits for it to return or be
 * cancelled. You can use g_task_propagate_pointer(), etc, afterward
 * to get the result of @task_func.
 *
 * See #GTaskThreadFunc for more details about how @task_func is handled.
 *
 * Normally this is used with tasks created with a %NULL
 * `callback`, but note that even if the task does
 * have a callback, it will not be invoked when @task_func returns.
 * #GTask:completed will be set to %TRUE just before this function returns.
 *
 * Although GLib currently rate-limits the tasks queued via
 * g_task_run_in_thread_sync(), you should not assume that it will
 * always do this. If you have a very large number of tasks to run,
 * but don't want them to all run at once, you should only queue a
 * limited number of them at a time.
 *
 * Since: 2.36
 */
void
g_task_run_in_thread_sync (GTask           *task,
                           GTaskThreadFunc  task_func)
{
  g_return_if_fail (G_IS_TASK (task));

  g_object_ref (task);

  task->synchronous = TRUE;
  g_task_start_task_thread (task, task_func);

  while (!task->thread_complete)
    g_cond_wait (&task->cond, &task->lock);

  g_mutex_unlock (&task->lock);

  TRACE (GIO_TASK_BEFORE_RETURN (task, task->source_object,
                                 NULL  /* callback */,
                                 NULL  /* callback data */));

  /* Notify of completion in this thread. */
  task->completed = TRUE;
  g_object_notify (G_OBJECT (task), "completed");

  g_object_unref (task);
}

/**
 * g_task_attach_source:
 * @task: a #GTask
 * @source: the source to attach
 * @callback: the callback to invoke when @source triggers
 *
 * A utility function for dealing with async operations where you need
 * to wait for a #GSource to trigger. Attaches @source to @task's
 * #GMainContext with @task's [priority][io-priority], and sets @source's
 * callback to @callback, with @task as the callback's `user_data`.
 *
 * This takes a reference on @task until @source is destroyed.
 *
 * Since: 2.36
 */
void
g_task_attach_source (GTask       *task,
                      GSource     *source,
                      GSourceFunc  callback)
{
  g_return_if_fail (G_IS_TASK (task));

  g_source_set_callback (source, callback,
                         g_object_ref (task), g_object_unref);
  g_source_set_priority (source, task->priority);
  g_source_attach (source, task->context);
}


static gboolean
g_task_propagate_error (GTask   *task,
                        GError **error)
{
  gboolean error_set;

  if (task->check_cancellable &&
      g_cancellable_set_error_if_cancelled (task->cancellable, error))
    error_set = TRUE;
  else if (task->error)
    {
      g_propagate_error (error, task->error);
      task->error = NULL;
      task->had_error = TRUE;
      error_set = TRUE;
    }
  else
    error_set = FALSE;

  TRACE (GIO_TASK_PROPAGATE (task, error_set));

  return error_set;
}

/**
 * g_task_return_pointer:
 * @task: a #GTask
 * @result: (nullable) (transfer full): the pointer result of a task
 *     function
 * @result_destroy: (nullable): a #GDestroyNotify function.
 *
 * Sets @task's result to @result and completes the task. If @result
 * is not %NULL, then @result_destroy will be used to free @result if
 * the caller does not take ownership of it with
 * g_task_propagate_pointer().
 *
 * "Completes the task" means that for an ordinary asynchronous task
 * it will either invoke the task's callback, or else queue that
 * callback to be invoked in the proper #GMainContext, or in the next
 * iteration of the current #GMainContext. For a task run via
 * g_task_run_in_thread() or g_task_run_in_thread_sync(), calling this
 * method will save @result to be returned to the caller later, but
 * the task will not actually be completed until the #GTaskThreadFunc
 * exits.
 *
 * Note that since the task may be completed before returning from
 * g_task_return_pointer(), you cannot assume that @result is still
 * valid after calling this, unless you are still holding another
 * reference on it.
 *
 * Since: 2.36
 */
void
g_task_return_pointer (GTask          *task,
                       gpointer        result,
                       GDestroyNotify  result_destroy)
{
  g_return_if_fail (G_IS_TASK (task));
  g_return_if_fail (task->result_set == FALSE);

  task->result.pointer = result;
  task->result_destroy = result_destroy;

  g_task_return (task, G_TASK_RETURN_SUCCESS);
}

/**
 * g_task_propagate_pointer:
 * @task: a #GTask
 * @error: return location for a #GError
 *
 * Gets the result of @task as a pointer, and transfers ownership
 * of that value to the caller.
 *
 * If the task resulted in an error, or was cancelled, then this will
 * instead return %NULL and set @error.
 *
 * Since this method transfers ownership of the return value (or
 * error) to the caller, you may only call it once.
 *
 * Returns: (transfer full): the task result, or %NULL on error
 *
 * Since: 2.36
 */
gpointer
g_task_propagate_pointer (GTask   *task,
                          GError **error)
{
  g_return_val_if_fail (G_IS_TASK (task), NULL);

  if (g_task_propagate_error (task, error))
    return NULL;

  g_return_val_if_fail (task->result_set == TRUE, NULL);

  task->result_destroy = NULL;
  task->result_set = FALSE;
  return task->result.pointer;
}

/**
 * g_task_return_int:
 * @task: a #GTask.
 * @result: the integer (#gssize) result of a task function.
 *
 * Sets @task's result to @result and completes the task (see
 * g_task_return_pointer() for more discussion of exactly what this
 * means).
 *
 * Since: 2.36
 */
void
g_task_return_int (GTask  *task,
                   gssize  result)
{
  g_return_if_fail (G_IS_TASK (task));
  g_return_if_fail (task->result_set == FALSE);

  task->result.size = result;

  g_task_return (task, G_TASK_RETURN_SUCCESS);
}

/**
 * g_task_propagate_int:
 * @task: a #GTask.
 * @error: return location for a #GError
 *
 * Gets the result of @task as an integer (#gssize).
 *
 * If the task resulted in an error, or was cancelled, then this will
 * instead return -1 and set @error.
 *
 * Since this method transfers ownership of the return value (or
 * error) to the caller, you may only call it once.
 *
 * Returns: the task result, or -1 on error
 *
 * Since: 2.36
 */
gssize
g_task_propagate_int (GTask   *task,
                      GError **error)
{
  g_return_val_if_fail (G_IS_TASK (task), -1);

  if (g_task_propagate_error (task, error))
    return -1;

  g_return_val_if_fail (task->result_set == TRUE, -1);

  task->result_set = FALSE;
  return task->result.size;
}

/**
 * g_task_return_boolean:
 * @task: a #GTask.
 * @result: the #gboolean result of a task function.
 *
 * Sets @task's result to @result and completes the task (see
 * g_task_return_pointer() for more discussion of exactly what this
 * means).
 *
 * Since: 2.36
 */
void
g_task_return_boolean (GTask    *task,
                       gboolean  result)
{
  g_return_if_fail (G_IS_TASK (task));
  g_return_if_fail (task->result_set == FALSE);

  task->result.boolean = result;

  g_task_return (task, G_TASK_RETURN_SUCCESS);
}

/**
 * g_task_propagate_boolean:
 * @task: a #GTask.
 * @error: return location for a #GError
 *
 * Gets the result of @task as a #gboolean.
 *
 * If the task resulted in an error, or was cancelled, then this will
 * instead return %FALSE and set @error.
 *
 * Since this method transfers ownership of the return value (or
 * error) to the caller, you may only call it once.
 *
 * Returns: the task result, or %FALSE on error
 *
 * Since: 2.36
 */
gboolean
g_task_propagate_boolean (GTask   *task,
                          GError **error)
{
  g_return_val_if_fail (G_IS_TASK (task), FALSE);

  if (g_task_propagate_error (task, error))
    return FALSE;

  g_return_val_if_fail (task->result_set == TRUE, FALSE);

  task->result_set = FALSE;
  return task->result.boolean;
}

/**
 * g_task_return_error:
 * @task: a #GTask.
 * @error: (transfer full): the #GError result of a task function.
 *
 * Sets @task's result to @error (which @task assumes ownership of)
 * and completes the task (see g_task_return_pointer() for more
 * discussion of exactly what this means).
 *
 * Note that since the task takes ownership of @error, and since the
 * task may be completed before returning from g_task_return_error(),
 * you cannot assume that @error is still valid after calling this.
 * Call g_error_copy() on the error if you need to keep a local copy
 * as well.
 *
 * See also g_task_return_new_error().
 *
 * Since: 2.36
 */
void
g_task_return_error (GTask  *task,
                     GError *error)
{
  g_return_if_fail (G_IS_TASK (task));
  g_return_if_fail (task->result_set == FALSE);
  g_return_if_fail (error != NULL);

  task->error = error;

  g_task_return (task, G_TASK_RETURN_ERROR);
}

/**
 * g_task_return_new_error:
 * @task: a #GTask.
 * @domain: a #GQuark.
 * @code: an error code.
 * @format: a string with format characters.
 * @...: a list of values to insert into @format.
 *
 * Sets @task's result to a new #GError created from @domain, @code,
 * @format, and the remaining arguments, and completes the task (see
 * g_task_return_pointer() for more discussion of exactly what this
 * means).
 *
 * See also g_task_return_error().
 *
 * Since: 2.36
 */
void
g_task_return_new_error (GTask           *task,
                         GQuark           domain,
                         gint             code,
                         const char      *format,
                         ...)
{
  GError *error;
  va_list args;

  va_start (args, format);
  error = g_error_new_valist (domain, code, format, args);
  va_end (args);

  g_task_return_error (task, error);
}

/**
 * g_task_return_error_if_cancelled:
 * @task: a #GTask
 *
 * Checks if @task's #GCancellable has been cancelled, and if so, sets
 * @task's error accordingly and completes the task (see
 * g_task_return_pointer() for more discussion of exactly what this
 * means).
 *
 * Returns: %TRUE if @task has been cancelled, %FALSE if not
 *
 * Since: 2.36
 */
gboolean
g_task_return_error_if_cancelled (GTask *task)
{
  GError *error = NULL;

  g_return_val_if_fail (G_IS_TASK (task), FALSE);
  g_return_val_if_fail (task->result_set == FALSE, FALSE);

  if (g_cancellable_set_error_if_cancelled (task->cancellable, &error))
    {
      /* We explicitly set task->error so this works even when
       * check-cancellable is not set.
       */
      g_clear_error (&task->error);
      task->error = error;

      g_task_return (task, G_TASK_RETURN_ERROR);
      return TRUE;
    }
  else
    return FALSE;
}

/**
 * g_task_had_error:
 * @task: a #GTask.
 *
 * Tests if @task resulted in an error.
 *
 * Returns: %TRUE if the task resulted in an error, %FALSE otherwise.
 *
 * Since: 2.36
 */
gboolean
g_task_had_error (GTask *task)
{
  g_return_val_if_fail (G_IS_TASK (task), FALSE);

  if (task->error != NULL || task->had_error)
    return TRUE;

  if (task->check_cancellable && g_cancellable_is_cancelled (task->cancellable))
    return TRUE;

  return FALSE;
}

/**
 * g_task_get_completed:
 * @task: a #GTask.
 *
 * Gets the value of #GTask:completed. This changes from %FALSE to %TRUE after
 * the task’s callback is invoked, and will return %FALSE if called from inside
 * the callback.
 *
 * Returns: %TRUE if the task has completed, %FALSE otherwise.
 *
 * Since: 2.44
 */
gboolean
g_task_get_completed (GTask *task)
{
  g_return_val_if_fail (G_IS_TASK (task), FALSE);

  return task->completed;
}

/**
 * g_task_is_valid:
 * @result: (type Gio.AsyncResult): A #GAsyncResult
 * @source_object: (nullable) (type GObject): the source object
 *   expected to be associated with the task
 *
 * Checks that @result is a #GTask, and that @source_object is its
 * source object (or that @source_object is %NULL and @result has no
 * source object). This can be used in g_return_if_fail() checks.
 *
 * Returns: %TRUE if @result and @source_object are valid, %FALSE
 * if not
 *
 * Since: 2.36
 */
gboolean
g_task_is_valid (gpointer result,
                 gpointer source_object)
{
  if (!G_IS_TASK (result))
    return FALSE;

  return G_TASK (result)->source_object == source_object;
}

static gint
g_task_compare_priority (gconstpointer a,
                         gconstpointer b,
                         gpointer      user_data)
{
  const GTask *ta = a;
  const GTask *tb = b;
  gboolean a_cancelled, b_cancelled;

  /* Tasks that are causing other tasks to block have higher
   * priority.
   */
  if (ta->blocking_other_task && !tb->blocking_other_task)
    return -1;
  else if (tb->blocking_other_task && !ta->blocking_other_task)
    return 1;

  /* Let already-cancelled tasks finish right away */
  a_cancelled = (ta->check_cancellable &&
                 g_cancellable_is_cancelled (ta->cancellable));
  b_cancelled = (tb->check_cancellable &&
                 g_cancellable_is_cancelled (tb->cancellable));
  if (a_cancelled && !b_cancelled)
    return -1;
  else if (b_cancelled && !a_cancelled)
    return 1;

  /* Lower priority == run sooner == negative return value */
  return ta->priority - tb->priority;
}

static gboolean
trivial_source_dispatch (GSource     *source,
                         GSourceFunc  callback,
                         gpointer     user_data)
{
  return callback (user_data);
}

GSourceFuncs trivial_source_funcs = {
  NULL, /* prepare */
  NULL, /* check */
  trivial_source_dispatch,
  NULL
};

static void
g_task_thread_pool_init (void)
{
  task_pool = g_thread_pool_new (g_task_thread_pool_thread, NULL,
                                 G_TASK_POOL_SIZE, FALSE, NULL);
  g_assert (task_pool != NULL);

  g_thread_pool_set_sort_function (task_pool, g_task_compare_priority, NULL);

  task_pool_manager = g_source_new (&trivial_source_funcs, sizeof (GSource));
  g_source_set_callback (task_pool_manager, task_pool_manager_timeout, NULL, NULL);
  g_source_set_ready_time (task_pool_manager, -1);
  g_source_attach (task_pool_manager,
                   GLIB_PRIVATE_CALL (g_get_worker_context ()));
  g_source_unref (task_pool_manager);
}

static void
g_task_get_property (GObject    *object,
                     guint       prop_id,
                     GValue     *value,
                     GParamSpec *pspec)
{
  GTask *task = G_TASK (object);

  switch ((GTaskProperty) prop_id)
    {
    case PROP_COMPLETED:
      g_value_set_boolean (value, task->completed);
      break;
    }
}

static void
g_task_class_init (GTaskClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->get_property = g_task_get_property;
  gobject_class->finalize = g_task_finalize;

  /**
   * GTask:completed:
   *
   * Whether the task has completed, meaning its callback (if set) has been
   * invoked. This can only happen after g_task_return_pointer(),
   * g_task_return_error() or one of the other return functions have been called
   * on the task.
   *
   * This property is guaranteed to change from %FALSE to %TRUE exactly once.
   *
   * The #GObject::notify signal for this change is emitted in the same main
   * context as the task’s callback, immediately after that callback is invoked.
   *
   * Since: 2.44
   */
  g_object_class_install_property (gobject_class, PROP_COMPLETED,
    g_param_spec_boolean ("completed",
                          P_("Task completed"),
                          P_("Whether the task has completed yet"),
                          FALSE, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
}

static gpointer
g_task_get_user_data (GAsyncResult *res)
{
  return G_TASK (res)->callback_data;
}

static gboolean
g_task_is_tagged (GAsyncResult *res,
                  gpointer      source_tag)
{
  return G_TASK (res)->source_tag == source_tag;
}

static void
g_task_async_result_iface_init (GAsyncResultIface *iface)
{
  iface->get_user_data = g_task_get_user_data;
  iface->get_source_object = g_task_ref_source_object;
  iface->is_tagged = g_task_is_tagged;
}
