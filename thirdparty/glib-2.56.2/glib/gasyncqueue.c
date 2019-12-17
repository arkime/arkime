/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * GAsyncQueue: asynchronous queue implementation, based on GQueue.
 * Copyright (C) 2000 Sebastian Wilhelmi; University of Karlsruhe
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
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

/*
 * MT safe
 */

#include "config.h"

#include "gasyncqueue.h"
#include "gasyncqueueprivate.h"

#include "gmain.h"
#include "gmem.h"
#include "gqueue.h"
#include "gtestutils.h"
#include "gtimer.h"
#include "gthread.h"
#include "deprecated/gthread.h"


/**
 * SECTION:async_queues
 * @title: Asynchronous Queues
 * @short_description: asynchronous communication between threads
 * @see_also: #GThreadPool
 *
 * Often you need to communicate between different threads. In general
 * it's safer not to do this by shared memory, but by explicit message
 * passing. These messages only make sense asynchronously for
 * multi-threaded applications though, as a synchronous operation could
 * as well be done in the same thread.
 *
 * Asynchronous queues are an exception from most other GLib data
 * structures, as they can be used simultaneously from multiple threads
 * without explicit locking and they bring their own builtin reference
 * counting. This is because the nature of an asynchronous queue is that
 * it will always be used by at least 2 concurrent threads.
 *
 * For using an asynchronous queue you first have to create one with
 * g_async_queue_new(). #GAsyncQueue structs are reference counted,
 * use g_async_queue_ref() and g_async_queue_unref() to manage your
 * references.
 *
 * A thread which wants to send a message to that queue simply calls
 * g_async_queue_push() to push the message to the queue.
 *
 * A thread which is expecting messages from an asynchronous queue
 * simply calls g_async_queue_pop() for that queue. If no message is
 * available in the queue at that point, the thread is now put to sleep
 * until a message arrives. The message will be removed from the queue
 * and returned. The functions g_async_queue_try_pop() and
 * g_async_queue_timeout_pop() can be used to only check for the presence
 * of messages or to only wait a certain time for messages respectively.
 *
 * For almost every function there exist two variants, one that locks
 * the queue and one that doesn't. That way you can hold the queue lock
 * (acquire it with g_async_queue_lock() and release it with
 * g_async_queue_unlock()) over multiple queue accessing instructions.
 * This can be necessary to ensure the integrity of the queue, but should
 * only be used when really necessary, as it can make your life harder
 * if used unwisely. Normally you should only use the locking function
 * variants (those without the _unlocked suffix).
 *
 * In many cases, it may be more convenient to use #GThreadPool when
 * you need to distribute work to a set of worker threads instead of
 * using #GAsyncQueue manually. #GThreadPool uses a GAsyncQueue
 * internally.
 */

/**
 * GAsyncQueue:
 *
 * The GAsyncQueue struct is an opaque data structure which represents
 * an asynchronous queue. It should only be accessed through the
 * g_async_queue_* functions.
 */
struct _GAsyncQueue
{
  GMutex mutex;
  GCond cond;
  GQueue queue;
  GDestroyNotify item_free_func;
  guint waiting_threads;
  gint ref_count;
};

typedef struct
{
  GCompareDataFunc func;
  gpointer         user_data;
} SortData;

/**
 * g_async_queue_new:
 *
 * Creates a new asynchronous queue.
 *
 * Returns: a new #GAsyncQueue. Free with g_async_queue_unref()
 */
GAsyncQueue *
g_async_queue_new (void)
{
  return g_async_queue_new_full (NULL);
}

/**
 * g_async_queue_new_full:
 * @item_free_func: function to free queue elements
 *
 * Creates a new asynchronous queue and sets up a destroy notify
 * function that is used to free any remaining queue items when
 * the queue is destroyed after the final unref.
 *
 * Returns: a new #GAsyncQueue. Free with g_async_queue_unref()
 *
 * Since: 2.16
 */
GAsyncQueue *
g_async_queue_new_full (GDestroyNotify item_free_func)
{
  GAsyncQueue *queue;

  queue = g_new (GAsyncQueue, 1);
  g_mutex_init (&queue->mutex);
  g_cond_init (&queue->cond);
  g_queue_init (&queue->queue);
  queue->waiting_threads = 0;
  queue->ref_count = 1;
  queue->item_free_func = item_free_func;

  return queue;
}

/**
 * g_async_queue_ref:
 * @queue: a #GAsyncQueue
 *
 * Increases the reference count of the asynchronous @queue by 1.
 * You do not need to hold the lock to call this function.
 *
 * Returns: the @queue that was passed in (since 2.6)
 */
GAsyncQueue *
g_async_queue_ref (GAsyncQueue *queue)
{
  g_return_val_if_fail (queue, NULL);

  g_atomic_int_inc (&queue->ref_count);

  return queue;
}

/**
 * g_async_queue_ref_unlocked:
 * @queue: a #GAsyncQueue
 *
 * Increases the reference count of the asynchronous @queue by 1.
 *
 * Deprecated: 2.8: Reference counting is done atomically.
 * so g_async_queue_ref() can be used regardless of the @queue's
 * lock.
 */
void
g_async_queue_ref_unlocked (GAsyncQueue *queue)
{
  g_return_if_fail (queue);

  g_atomic_int_inc (&queue->ref_count);
}

/**
 * g_async_queue_unref_and_unlock:
 * @queue: a #GAsyncQueue
 *
 * Decreases the reference count of the asynchronous @queue by 1
 * and releases the lock. This function must be called while holding
 * the @queue's lock. If the reference count went to 0, the @queue
 * will be destroyed and the memory allocated will be freed.
 *
 * Deprecated: 2.8: Reference counting is done atomically.
 * so g_async_queue_unref() can be used regardless of the @queue's
 * lock.
 */
void
g_async_queue_unref_and_unlock (GAsyncQueue *queue)
{
  g_return_if_fail (queue);

  g_mutex_unlock (&queue->mutex);
  g_async_queue_unref (queue);
}

/**
 * g_async_queue_unref:
 * @queue: a #GAsyncQueue.
 *
 * Decreases the reference count of the asynchronous @queue by 1.
 *
 * If the reference count went to 0, the @queue will be destroyed
 * and the memory allocated will be freed. So you are not allowed
 * to use the @queue afterwards, as it might have disappeared.
 * You do not need to hold the lock to call this function.
 */
void
g_async_queue_unref (GAsyncQueue *queue)
{
  g_return_if_fail (queue);

  if (g_atomic_int_dec_and_test (&queue->ref_count))
    {
      g_return_if_fail (queue->waiting_threads == 0);
      g_mutex_clear (&queue->mutex);
      g_cond_clear (&queue->cond);
      if (queue->item_free_func)
        g_queue_foreach (&queue->queue, (GFunc) queue->item_free_func, NULL);
      g_queue_clear (&queue->queue);
      g_free (queue);
    }
}

/**
 * g_async_queue_lock:
 * @queue: a #GAsyncQueue
 *
 * Acquires the @queue's lock. If another thread is already
 * holding the lock, this call will block until the lock
 * becomes available.
 *
 * Call g_async_queue_unlock() to drop the lock again.
 *
 * While holding the lock, you can only call the
 * g_async_queue_*_unlocked() functions on @queue. Otherwise,
 * deadlock may occur.
 */
void
g_async_queue_lock (GAsyncQueue *queue)
{
  g_return_if_fail (queue);

  g_mutex_lock (&queue->mutex);
}

/**
 * g_async_queue_unlock:
 * @queue: a #GAsyncQueue
 *
 * Releases the queue's lock.
 *
 * Calling this function when you have not acquired
 * the with g_async_queue_lock() leads to undefined
 * behaviour.
 */
void
g_async_queue_unlock (GAsyncQueue *queue)
{
  g_return_if_fail (queue);

  g_mutex_unlock (&queue->mutex);
}

/**
 * g_async_queue_push:
 * @queue: a #GAsyncQueue
 * @data: @data to push into the @queue
 *
 * Pushes the @data into the @queue. @data must not be %NULL.
 */
void
g_async_queue_push (GAsyncQueue *queue,
                    gpointer     data)
{
  g_return_if_fail (queue);
  g_return_if_fail (data);

  g_mutex_lock (&queue->mutex);
  g_async_queue_push_unlocked (queue, data);
  g_mutex_unlock (&queue->mutex);
}

/**
 * g_async_queue_push_unlocked:
 * @queue: a #GAsyncQueue
 * @data: @data to push into the @queue
 *
 * Pushes the @data into the @queue. @data must not be %NULL.
 *
 * This function must be called while holding the @queue's lock.
 */
void
g_async_queue_push_unlocked (GAsyncQueue *queue,
                             gpointer     data)
{
  g_return_if_fail (queue);
  g_return_if_fail (data);

  g_queue_push_head (&queue->queue, data);
  if (queue->waiting_threads > 0)
    g_cond_signal (&queue->cond);
}

/**
 * g_async_queue_push_sorted:
 * @queue: a #GAsyncQueue
 * @data: the @data to push into the @queue
 * @func: the #GCompareDataFunc is used to sort @queue
 * @user_data: user data passed to @func.
 *
 * Inserts @data into @queue using @func to determine the new
 * position.
 *
 * This function requires that the @queue is sorted before pushing on
 * new elements, see g_async_queue_sort().
 *
 * This function will lock @queue before it sorts the queue and unlock
 * it when it is finished.
 *
 * For an example of @func see g_async_queue_sort().
 *
 * Since: 2.10
 */
void
g_async_queue_push_sorted (GAsyncQueue      *queue,
                           gpointer          data,
                           GCompareDataFunc  func,
                           gpointer          user_data)
{
  g_return_if_fail (queue != NULL);

  g_mutex_lock (&queue->mutex);
  g_async_queue_push_sorted_unlocked (queue, data, func, user_data);
  g_mutex_unlock (&queue->mutex);
}

static gint
g_async_queue_invert_compare (gpointer  v1,
                              gpointer  v2,
                              SortData *sd)
{
  return -sd->func (v1, v2, sd->user_data);
}

/**
 * g_async_queue_push_sorted_unlocked:
 * @queue: a #GAsyncQueue
 * @data: the @data to push into the @queue
 * @func: the #GCompareDataFunc is used to sort @queue
 * @user_data: user data passed to @func.
 *
 * Inserts @data into @queue using @func to determine the new
 * position.
 *
 * The sort function @func is passed two elements of the @queue.
 * It should return 0 if they are equal, a negative value if the
 * first element should be higher in the @queue or a positive value
 * if the first element should be lower in the @queue than the second
 * element.
 *
 * This function requires that the @queue is sorted before pushing on
 * new elements, see g_async_queue_sort().
 *
 * This function must be called while holding the @queue's lock.
 *
 * For an example of @func see g_async_queue_sort().
 *
 * Since: 2.10
 */
void
g_async_queue_push_sorted_unlocked (GAsyncQueue      *queue,
                                    gpointer          data,
                                    GCompareDataFunc  func,
                                    gpointer          user_data)
{
  SortData sd;

  g_return_if_fail (queue != NULL);

  sd.func = func;
  sd.user_data = user_data;

  g_queue_insert_sorted (&queue->queue,
                         data,
                         (GCompareDataFunc)g_async_queue_invert_compare,
                         &sd);
  if (queue->waiting_threads > 0)
    g_cond_signal (&queue->cond);
}

static gpointer
g_async_queue_pop_intern_unlocked (GAsyncQueue *queue,
                                   gboolean     wait,
                                   gint64       end_time)
{
  gpointer retval;

  if (!g_queue_peek_tail_link (&queue->queue) && wait)
    {
      queue->waiting_threads++;
      while (!g_queue_peek_tail_link (&queue->queue))
        {
	  if (end_time == -1)
	    g_cond_wait (&queue->cond, &queue->mutex);
	  else
	    {
	      if (!g_cond_wait_until (&queue->cond, &queue->mutex, end_time))
		break;
	    }
        }
      queue->waiting_threads--;
    }

  retval = g_queue_pop_tail (&queue->queue);

  g_assert (retval || !wait || end_time > 0);

  return retval;
}

/**
 * g_async_queue_pop:
 * @queue: a #GAsyncQueue
 *
 * Pops data from the @queue. If @queue is empty, this function
 * blocks until data becomes available.
 *
 * Returns: data from the queue
 */
gpointer
g_async_queue_pop (GAsyncQueue *queue)
{
  gpointer retval;

  g_return_val_if_fail (queue, NULL);

  g_mutex_lock (&queue->mutex);
  retval = g_async_queue_pop_intern_unlocked (queue, TRUE, -1);
  g_mutex_unlock (&queue->mutex);

  return retval;
}

/**
 * g_async_queue_pop_unlocked:
 * @queue: a #GAsyncQueue
 *
 * Pops data from the @queue. If @queue is empty, this function
 * blocks until data becomes available.
 *
 * This function must be called while holding the @queue's lock.
 *
 * Returns: data from the queue.
 */
gpointer
g_async_queue_pop_unlocked (GAsyncQueue *queue)
{
  g_return_val_if_fail (queue, NULL);

  return g_async_queue_pop_intern_unlocked (queue, TRUE, -1);
}

/**
 * g_async_queue_try_pop:
 * @queue: a #GAsyncQueue
 *
 * Tries to pop data from the @queue. If no data is available,
 * %NULL is returned.
 *
 * Returns: data from the queue or %NULL, when no data is
 *     available immediately.
 */
gpointer
g_async_queue_try_pop (GAsyncQueue *queue)
{
  gpointer retval;

  g_return_val_if_fail (queue, NULL);

  g_mutex_lock (&queue->mutex);
  retval = g_async_queue_pop_intern_unlocked (queue, FALSE, -1);
  g_mutex_unlock (&queue->mutex);

  return retval;
}

/**
 * g_async_queue_try_pop_unlocked:
 * @queue: a #GAsyncQueue
 *
 * Tries to pop data from the @queue. If no data is available,
 * %NULL is returned.
 *
 * This function must be called while holding the @queue's lock.
 *
 * Returns: data from the queue or %NULL, when no data is
 *     available immediately.
 */
gpointer
g_async_queue_try_pop_unlocked (GAsyncQueue *queue)
{
  g_return_val_if_fail (queue, NULL);

  return g_async_queue_pop_intern_unlocked (queue, FALSE, -1);
}

/**
 * g_async_queue_timeout_pop:
 * @queue: a #GAsyncQueue
 * @timeout: the number of microseconds to wait
 *
 * Pops data from the @queue. If the queue is empty, blocks for
 * @timeout microseconds, or until data becomes available.
 *
 * If no data is received before the timeout, %NULL is returned.
 *
 * Returns: data from the queue or %NULL, when no data is
 *     received before the timeout.
 */
gpointer
g_async_queue_timeout_pop (GAsyncQueue *queue,
			   guint64      timeout)
{
  gint64 end_time = g_get_monotonic_time () + timeout;
  gpointer retval;

  g_mutex_lock (&queue->mutex);
  retval = g_async_queue_pop_intern_unlocked (queue, TRUE, end_time);
  g_mutex_unlock (&queue->mutex);

  return retval;
}

/**
 * g_async_queue_timeout_pop_unlocked:
 * @queue: a #GAsyncQueue
 * @timeout: the number of microseconds to wait
 *
 * Pops data from the @queue. If the queue is empty, blocks for
 * @timeout microseconds, or until data becomes available.
 *
 * If no data is received before the timeout, %NULL is returned.
 *
 * This function must be called while holding the @queue's lock.
 *
 * Returns: data from the queue or %NULL, when no data is
 *     received before the timeout.
 */
gpointer
g_async_queue_timeout_pop_unlocked (GAsyncQueue *queue,
				    guint64      timeout)
{
  gint64 end_time = g_get_monotonic_time () + timeout;

  return g_async_queue_pop_intern_unlocked (queue, TRUE, end_time);
}

/**
 * g_async_queue_timed_pop:
 * @queue: a #GAsyncQueue
 * @end_time: a #GTimeVal, determining the final time
 *
 * Pops data from the @queue. If the queue is empty, blocks until
 * @end_time or until data becomes available.
 *
 * If no data is received before @end_time, %NULL is returned.
 *
 * To easily calculate @end_time, a combination of g_get_current_time()
 * and g_time_val_add() can be used.
 *
 * Returns: data from the queue or %NULL, when no data is
 *     received before @end_time.
 *
 * Deprecated: use g_async_queue_timeout_pop().
 */
gpointer
g_async_queue_timed_pop (GAsyncQueue *queue,
                         GTimeVal    *end_time)
{
  gint64 m_end_time;
  gpointer retval;

  g_return_val_if_fail (queue, NULL);

  if (end_time != NULL)
    {
      m_end_time = g_get_monotonic_time () +
        ((gint64) end_time->tv_sec * G_USEC_PER_SEC + end_time->tv_usec - g_get_real_time ());
    }
  else
    m_end_time = -1;

  g_mutex_lock (&queue->mutex);
  retval = g_async_queue_pop_intern_unlocked (queue, TRUE, m_end_time);
  g_mutex_unlock (&queue->mutex);

  return retval;
}

/**
 * g_async_queue_timed_pop_unlocked:
 * @queue: a #GAsyncQueue
 * @end_time: a #GTimeVal, determining the final time
 *
 * Pops data from the @queue. If the queue is empty, blocks until
 * @end_time or until data becomes available.
 *
 * If no data is received before @end_time, %NULL is returned.
 *
 * To easily calculate @end_time, a combination of g_get_current_time()
 * and g_time_val_add() can be used.
 *
 * This function must be called while holding the @queue's lock.
 *
 * Returns: data from the queue or %NULL, when no data is
 *     received before @end_time.
 *
 * Deprecated: use g_async_queue_timeout_pop_unlocked().
 */
gpointer
g_async_queue_timed_pop_unlocked (GAsyncQueue *queue,
                                  GTimeVal    *end_time)
{
  gint64 m_end_time;

  g_return_val_if_fail (queue, NULL);

  if (end_time != NULL)
    {
      m_end_time = g_get_monotonic_time () +
        ((gint64) end_time->tv_sec * G_USEC_PER_SEC + end_time->tv_usec - g_get_real_time ());
    }
  else
    m_end_time = -1;

  return g_async_queue_pop_intern_unlocked (queue, TRUE, m_end_time);
}

/**
 * g_async_queue_length:
 * @queue: a #GAsyncQueue.
 *
 * Returns the length of the queue.
 *
 * Actually this function returns the number of data items in
 * the queue minus the number of waiting threads, so a negative
 * value means waiting threads, and a positive value means available
 * entries in the @queue. A return value of 0 could mean n entries
 * in the queue and n threads waiting. This can happen due to locking
 * of the queue or due to scheduling.
 *
 * Returns: the length of the @queue
 */
gint
g_async_queue_length (GAsyncQueue *queue)
{
  gint retval;

  g_return_val_if_fail (queue, 0);

  g_mutex_lock (&queue->mutex);
  retval = queue->queue.length - queue->waiting_threads;
  g_mutex_unlock (&queue->mutex);

  return retval;
}

/**
 * g_async_queue_length_unlocked:
 * @queue: a #GAsyncQueue
 *
 * Returns the length of the queue.
 *
 * Actually this function returns the number of data items in
 * the queue minus the number of waiting threads, so a negative
 * value means waiting threads, and a positive value means available
 * entries in the @queue. A return value of 0 could mean n entries
 * in the queue and n threads waiting. This can happen due to locking
 * of the queue or due to scheduling.
 *
 * This function must be called while holding the @queue's lock.
 *
 * Returns: the length of the @queue.
 */
gint
g_async_queue_length_unlocked (GAsyncQueue *queue)
{
  g_return_val_if_fail (queue, 0);

  return queue->queue.length - queue->waiting_threads;
}

/**
 * g_async_queue_sort:
 * @queue: a #GAsyncQueue
 * @func: the #GCompareDataFunc is used to sort @queue
 * @user_data: user data passed to @func
 *
 * Sorts @queue using @func.
 *
 * The sort function @func is passed two elements of the @queue.
 * It should return 0 if they are equal, a negative value if the
 * first element should be higher in the @queue or a positive value
 * if the first element should be lower in the @queue than the second
 * element.
 *
 * This function will lock @queue before it sorts the queue and unlock
 * it when it is finished.
 *
 * If you were sorting a list of priority numbers to make sure the
 * lowest priority would be at the top of the queue, you could use:
 * |[<!-- language="C" -->
 *  gint32 id1;
 *  gint32 id2;
 *
 *  id1 = GPOINTER_TO_INT (element1);
 *  id2 = GPOINTER_TO_INT (element2);
 *
 *  return (id1 > id2 ? +1 : id1 == id2 ? 0 : -1);
 * ]|
 *
 * Since: 2.10
 */
void
g_async_queue_sort (GAsyncQueue      *queue,
                    GCompareDataFunc  func,
                    gpointer          user_data)
{
  g_return_if_fail (queue != NULL);
  g_return_if_fail (func != NULL);

  g_mutex_lock (&queue->mutex);
  g_async_queue_sort_unlocked (queue, func, user_data);
  g_mutex_unlock (&queue->mutex);
}

/**
 * g_async_queue_sort_unlocked:
 * @queue: a #GAsyncQueue
 * @func: the #GCompareDataFunc is used to sort @queue
 * @user_data: user data passed to @func
 *
 * Sorts @queue using @func.
 *
 * The sort function @func is passed two elements of the @queue.
 * It should return 0 if they are equal, a negative value if the
 * first element should be higher in the @queue or a positive value
 * if the first element should be lower in the @queue than the second
 * element.
 *
 * This function must be called while holding the @queue's lock.
 *
 * Since: 2.10
 */
void
g_async_queue_sort_unlocked (GAsyncQueue      *queue,
                             GCompareDataFunc  func,
                             gpointer          user_data)
{
  SortData sd;

  g_return_if_fail (queue != NULL);
  g_return_if_fail (func != NULL);

  sd.func = func;
  sd.user_data = user_data;

  g_queue_sort (&queue->queue,
                (GCompareDataFunc)g_async_queue_invert_compare,
                &sd);
}

/**
 * g_async_queue_remove:
 * @queue: a #GAsyncQueue
 * @item: the data to remove from the @queue
 *
 * Remove an item from the queue.
 *
 * Returns: %TRUE if the item was removed
 *
 * Since: 2.46
 */
gboolean
g_async_queue_remove (GAsyncQueue *queue,
                      gpointer     item)
{
  gboolean ret;

  g_return_val_if_fail (queue != NULL, FALSE);
  g_return_val_if_fail (item != NULL, FALSE);

  g_mutex_lock (&queue->mutex);
  ret = g_async_queue_remove_unlocked (queue, item);
  g_mutex_unlock (&queue->mutex);

  return ret;
}

/**
 * g_async_queue_remove_unlocked:
 * @queue: a #GAsyncQueue
 * @item: the data to remove from the @queue
 *
 * Remove an item from the queue.
 *
 * This function must be called while holding the @queue's lock.
 *
 * Returns: %TRUE if the item was removed
 *
 * Since: 2.46
 */
gboolean
g_async_queue_remove_unlocked (GAsyncQueue *queue,
                               gpointer     item)
{
  g_return_val_if_fail (queue != NULL, FALSE);
  g_return_val_if_fail (item != NULL, FALSE);

  return g_queue_remove (&queue->queue, item);
}

/**
 * g_async_queue_push_front:
 * @queue: a #GAsyncQueue
 * @item: data to push into the @queue
 *
 * Pushes the @item into the @queue. @item must not be %NULL.
 * In contrast to g_async_queue_push(), this function
 * pushes the new item ahead of the items already in the queue,
 * so that it will be the next one to be popped off the queue.
 *
 * Since: 2.46
 */
void
g_async_queue_push_front (GAsyncQueue *queue,
                          gpointer     item)
{
  g_return_if_fail (queue != NULL);
  g_return_if_fail (item != NULL);

  g_mutex_lock (&queue->mutex);
  g_async_queue_push_front_unlocked (queue, item);
  g_mutex_unlock (&queue->mutex);
}

/**
 * g_async_queue_push_front_unlocked:
 * @queue: a #GAsyncQueue
 * @item: data to push into the @queue
 *
 * Pushes the @item into the @queue. @item must not be %NULL.
 * In contrast to g_async_queue_push_unlocked(), this function
 * pushes the new item ahead of the items already in the queue,
 * so that it will be the next one to be popped off the queue.
 *
 * This function must be called while holding the @queue's lock.
 *
 * Since: 2.46
 */
void
g_async_queue_push_front_unlocked (GAsyncQueue *queue,
                                   gpointer     item)
{
  g_return_if_fail (queue != NULL);
  g_return_if_fail (item != NULL);

  g_queue_push_tail (&queue->queue, item);
  if (queue->waiting_threads > 0)
    g_cond_signal (&queue->cond);
}

/*
 * Private API
 */

GMutex *
_g_async_queue_get_mutex (GAsyncQueue *queue)
{
  g_return_val_if_fail (queue, NULL);

  return &queue->mutex;
}
