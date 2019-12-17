/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * gthread.c: MT safety related functions
 * Copyright 1998 Sebastian Wilhelmi; University of Karlsruhe
 *                Owen Taylor
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

/* we know we are deprecated here, no need for warnings */
#define GLIB_DISABLE_DEPRECATION_WARNINGS

#include "gmessages.h"
#include "gslice.h"
#include "gmain.h"
#include "gthread.h"
#include "gthreadprivate.h"
#include "deprecated/gthread.h"
#include "garray.h"

#include "gutils.h"

/* {{{1 Documentation */

/**
 * SECTION:threads-deprecated
 * @title: Deprecated thread API
 * @short_description: old thread APIs (for reference only)
 * @see_also: #GThread
 *
 * These APIs are deprecated.  You should not use them in new code.
 * This section remains only to assist with understanding code that was
 * written to use these APIs at some point in the past.
 **/

/**
 * GThreadPriority:
 * @G_THREAD_PRIORITY_LOW: a priority lower than normal
 * @G_THREAD_PRIORITY_NORMAL: the default priority
 * @G_THREAD_PRIORITY_HIGH: a priority higher than normal
 * @G_THREAD_PRIORITY_URGENT: the highest priority
 *
 * Thread priorities.
 *
 * Deprecated:2.32: Thread priorities no longer have any effect.
 */

/**
 * GThreadFunctions:
 * @mutex_new: virtual function pointer for g_mutex_new()
 * @mutex_lock: virtual function pointer for g_mutex_lock()
 * @mutex_trylock: virtual function pointer for g_mutex_trylock()
 * @mutex_unlock: virtual function pointer for g_mutex_unlock()
 * @mutex_free: virtual function pointer for g_mutex_free()
 * @cond_new: virtual function pointer for g_cond_new()
 * @cond_signal: virtual function pointer for g_cond_signal()
 * @cond_broadcast: virtual function pointer for g_cond_broadcast()
 * @cond_wait: virtual function pointer for g_cond_wait()
 * @cond_timed_wait: virtual function pointer for g_cond_timed_wait()
 * @cond_free: virtual function pointer for g_cond_free()
 * @private_new: virtual function pointer for g_private_new()
 * @private_get: virtual function pointer for g_private_get()
 * @private_set: virtual function pointer for g_private_set()
 * @thread_create: virtual function pointer for g_thread_create()
 * @thread_yield: virtual function pointer for g_thread_yield()
 * @thread_join: virtual function pointer for g_thread_join()
 * @thread_exit: virtual function pointer for g_thread_exit()
 * @thread_set_priority: virtual function pointer for
 *                       g_thread_set_priority()
 * @thread_self: virtual function pointer for g_thread_self()
 * @thread_equal: used internally by recursive mutex locks and by some
 *                assertion checks
 *
 * This function table is no longer used by g_thread_init()
 * to initialize the thread system.
 */

/**
 * G_THREADS_IMPL_POSIX:
 *
 * This macro is defined if POSIX style threads are used.
 *
 * Deprecated:2.32:POSIX threads are in use on all non-Windows systems.
 *                 Use G_OS_WIN32 to detect Windows.
 */

/**
 * G_THREADS_IMPL_WIN32:
 *
 * This macro is defined if Windows style threads are used.
 *
 * Deprecated:2.32:Use G_OS_WIN32 to detect Windows.
 */


/* {{{1 Exported Variables */

/* Set this FALSE to have previously-compiled GStaticMutex code use the
 * slow path (ie: call into us) to avoid compatibility problems.
 */
gboolean g_thread_use_default_impl = FALSE;

GThreadFunctions g_thread_functions_for_glib_use =
{
  g_mutex_new,
  g_mutex_lock,
  g_mutex_trylock,
  g_mutex_unlock,
  g_mutex_free,
  g_cond_new,
  g_cond_signal,
  g_cond_broadcast,
  g_cond_wait,
  g_cond_timed_wait,
  g_cond_free,
  g_private_new,
  g_private_get,
  g_private_set,
  NULL,
  g_thread_yield,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
};

static guint64
gettime (void)
{
  return g_get_monotonic_time () * 1000;
}

guint64 (*g_thread_gettime) (void) = gettime;

/* Initialisation {{{1 ---------------------------------------------------- */
gboolean         g_threads_got_initialized = TRUE;

/**
 * g_thread_init:
 * @vtable: a function table of type #GThreadFunctions, that provides
 *     the entry points to the thread system to be used. Since 2.32,
 *     this parameter is ignored and should always be %NULL
 *
 * If you use GLib from more than one thread, you must initialize the
 * thread system by calling g_thread_init().
 *
 * Since version 2.24, calling g_thread_init() multiple times is allowed,
 * but nothing happens except for the first call.
 *
 * Since version 2.32, GLib does not support custom thread implementations
 * anymore and the @vtable parameter is ignored and you should pass %NULL.
 *
 * <note><para>g_thread_init() must not be called directly or indirectly
 * in a callback from GLib. Also no mutexes may be currently locked while
 * calling g_thread_init().</para></note>
 *
 * <note><para>To use g_thread_init() in your program, you have to link
 * with the libraries that the command <command>pkg-config --libs
 * gthread-2.0</command> outputs. This is not the case for all the
 * other thread-related functions of GLib. Those can be used without
 * having to link with the thread libraries.</para></note>
 *
 * Deprecated:2.32: This function is no longer necessary. The GLib
 *     threading system is automatically initialized at the start
 *     of your program.
 */

/**
 * g_thread_get_initialized:
 *
 * Indicates if g_thread_init() has been called.
 *
 * Returns: %TRUE if threads have been initialized.
 *
 * Since: 2.20
 */
gboolean
g_thread_get_initialized (void)
{
  return g_thread_supported ();
}

/* We need this for ABI compatibility */
GLIB_AVAILABLE_IN_ALL
void g_thread_init_glib (void);
void g_thread_init_glib (void) { }

/* Internal variables {{{1 */

static GSList      *g_thread_all_threads = NULL;
static GSList      *g_thread_free_indices = NULL;

/* Protects g_thread_all_threads and g_thread_free_indices */
G_LOCK_DEFINE_STATIC (g_static_mutex);
G_LOCK_DEFINE_STATIC (g_thread);

/* Misc. GThread functions {{{1 */

/**
 * g_thread_set_priority:
 * @thread: a #GThread.
 * @priority: ignored
 *
 * This function does nothing.
 *
 * Deprecated:2.32: Thread priorities no longer have any effect.
 */
void
g_thread_set_priority (GThread         *thread,
                       GThreadPriority  priority)
{
}

/**
 * g_thread_foreach:
 * @thread_func: function to call for all #GThread structures
 * @user_data: second argument to @thread_func
 *
 * Call @thread_func on all #GThreads that have been
 * created with g_thread_create().
 *
 * Note that threads may decide to exit while @thread_func is
 * running, so without intimate knowledge about the lifetime of
 * foreign threads, @thread_func shouldn't access the GThread*
 * pointer passed in as first argument. However, @thread_func will
 * not be called for threads which are known to have exited already.
 *
 * Due to thread lifetime checks, this function has an execution complexity
 * which is quadratic in the number of existing threads.
 *
 * Since: 2.10
 *
 * Deprecated:2.32: There aren't many things you can do with a #GThread,
 *     except comparing it with one that was returned from g_thread_create().
 *     There are better ways to find out if your thread is still alive.
 */
void
g_thread_foreach (GFunc    thread_func,
                  gpointer user_data)
{
  GSList *slist = NULL;
  GRealThread *thread;
  g_return_if_fail (thread_func != NULL);
  /* snapshot the list of threads for iteration */
  G_LOCK (g_thread);
  slist = g_slist_copy (g_thread_all_threads);
  G_UNLOCK (g_thread);
  /* walk the list, skipping non-existent threads */
  while (slist)
    {
      GSList *node = slist;
      slist = node->next;
      /* check whether the current thread still exists */
      G_LOCK (g_thread);
      if (g_slist_find (g_thread_all_threads, node->data))
        thread = node->data;
      else
        thread = NULL;
      G_UNLOCK (g_thread);
      if (thread)
        thread_func (thread, user_data);
      g_slist_free_1 (node);
    }
}

static void
g_enumerable_thread_remove (gpointer data)
{
  GRealThread *thread = data;

  G_LOCK (g_thread);
  g_thread_all_threads = g_slist_remove (g_thread_all_threads, thread);
  G_UNLOCK (g_thread);
}

GPrivate enumerable_thread_private = G_PRIVATE_INIT (g_enumerable_thread_remove);

static void
g_enumerable_thread_add (GRealThread *thread)
{
  G_LOCK (g_thread);
  g_thread_all_threads = g_slist_prepend (g_thread_all_threads, thread);
  G_UNLOCK (g_thread);

  g_private_set (&enumerable_thread_private, thread);
}

static gpointer
g_deprecated_thread_proxy (gpointer data)
{
  GRealThread *real = data;

  g_enumerable_thread_add (real);

  return g_thread_proxy (data);
}

/**
 * g_thread_create:
 * @func: a function to execute in the new thread
 * @data: an argument to supply to the new thread
 * @joinable: should this thread be joinable?
 * @error: return location for error, or %NULL
 *
 * This function creates a new thread.
 *
 * The new thread executes the function @func with the argument @data.
 * If the thread was created successfully, it is returned.
 *
 * @error can be %NULL to ignore errors, or non-%NULL to report errors.
 * The error is set, if and only if the function returns %NULL.
 *
 * This function returns a reference to the created thread only if
 * @joinable is %TRUE.  In that case, you must free this reference by
 * calling g_thread_unref() or g_thread_join().  If @joinable is %FALSE
 * then you should probably not touch the return value.
 *
 * Returns: the new #GThread on success
 *
 * Deprecated:2.32: Use g_thread_new() instead
 */
GThread *
g_thread_create (GThreadFunc   func,
                 gpointer      data,
                 gboolean      joinable,
                 GError      **error)
{
  return g_thread_create_full (func, data, 0, joinable, 0, 0, error);
}

/**
 * g_thread_create_full:
 * @func: a function to execute in the new thread.
 * @data: an argument to supply to the new thread.
 * @stack_size: a stack size for the new thread.
 * @joinable: should this thread be joinable?
 * @bound: ignored
 * @priority: ignored
 * @error: return location for error.
 *
 * This function creates a new thread.
 *
 * Returns: the new #GThread on success.
 *
 * Deprecated:2.32: The @bound and @priority arguments are now ignored.
 * Use g_thread_new().
 */
GThread *
g_thread_create_full (GThreadFunc       func,
                      gpointer          data,
                      gulong            stack_size,
                      gboolean          joinable,
                      gboolean          bound,
                      GThreadPriority   priority,
                      GError          **error)
{
  GThread *thread;

  thread = g_thread_new_internal (NULL, g_deprecated_thread_proxy,
                                  func, data, stack_size, error);

  if (thread && !joinable)
    {
      thread->joinable = FALSE;
      g_thread_unref (thread);
    }

  return thread;
}

/* GOnce {{{1 ------------------------------------------------------------- */
gboolean
g_once_init_enter_impl (volatile gsize *location)
{
  return (g_once_init_enter) (location);
}

/* GStaticMutex {{{1 ------------------------------------------------------ */

/**
 * GStaticMutex:
 *
 * A #GStaticMutex works like a #GMutex.
 *
 * Prior to GLib 2.32, GStaticMutex had the significant advantage
 * that it doesn't need to be created at run-time, but can be defined
 * at compile-time. Since 2.32, #GMutex can be statically allocated
 * as well, and GStaticMutex has been deprecated.
 *
 * Here is a version of our give_me_next_number() example using
 * a GStaticMutex:
 * |[
 *   int
 *   give_me_next_number (void)
 *   {
 *     static int current_number = 0;
 *     int ret_val;
 *     static GStaticMutex mutex = G_STATIC_MUTEX_INIT;
 *
 *     g_static_mutex_lock (&mutex);
 *     ret_val = current_number = calc_next_number (current_number);
 *     g_static_mutex_unlock (&mutex);
 *
 *     return ret_val;
 *   }
 * ]|
 *
 * Sometimes you would like to dynamically create a mutex. If you don't
 * want to require prior calling to g_thread_init(), because your code
 * should also be usable in non-threaded programs, you are not able to
 * use g_mutex_new() and thus #GMutex, as that requires a prior call to
 * g_thread_init(). In theses cases you can also use a #GStaticMutex.
 * It must be initialized with g_static_mutex_init() before using it
 * and freed with with g_static_mutex_free() when not needed anymore to
 * free up any allocated resources.
 *
 * Even though #GStaticMutex is not opaque, it should only be used with
 * the following functions, as it is defined differently on different
 * platforms.
 *
 * All of the g_static_mutex_* functions apart from
 * g_static_mutex_get_mutex() can also be used even if g_thread_init()
 * has not yet been called. Then they do nothing, apart from
 * g_static_mutex_trylock() which does nothing but returning %TRUE.
 *
 * All of the g_static_mutex_* functions are actually macros. Apart from
 * taking their addresses, you can however use them as if they were
 * functions.
 */

/**
 * G_STATIC_MUTEX_INIT:
 *
 * A #GStaticMutex must be initialized with this macro, before it can
 * be used. This macro can used be to initialize a variable, but it
 * cannot be assigned to a variable. In that case you have to use
 * g_static_mutex_init().
 *
 * |[
 * GStaticMutex my_mutex = G_STATIC_MUTEX_INIT;
 * ]|
 **/

/**
 * g_static_mutex_init:
 * @mutex: a #GStaticMutex to be initialized.
 *
 * Initializes @mutex.
 * Alternatively you can initialize it with #G_STATIC_MUTEX_INIT.
 *
 * Deprecated: 2.32: Use g_mutex_init()
 */
void
g_static_mutex_init (GStaticMutex *mutex)
{
  static const GStaticMutex init_mutex = G_STATIC_MUTEX_INIT;

  g_return_if_fail (mutex);

  *mutex = init_mutex;
}

/* IMPLEMENTATION NOTE:
 *
 * On some platforms a GStaticMutex is actually a normal GMutex stored
 * inside of a structure instead of being allocated dynamically.  We can
 * only do this for platforms on which we know, in advance, how to
 * allocate (size) and initialise (value) that memory.
 *
 * On other platforms, a GStaticMutex is nothing more than a pointer to
 * a GMutex.  In that case, the first access we make to the static mutex
 * must first allocate the normal GMutex and store it into the pointer.
 *
 * configure.ac writes macros into glibconfig.h to determine if
 * g_static_mutex_get_mutex() accesses the structure in memory directly
 * (on platforms where we are able to do that) or if it ends up here,
 * where we may have to allocate the GMutex before returning it.
 */

/**
 * g_static_mutex_get_mutex:
 * @mutex: a #GStaticMutex.
 *
 * For some operations (like g_cond_wait()) you must have a #GMutex
 * instead of a #GStaticMutex. This function will return the
 * corresponding #GMutex for @mutex.
 *
 * Returns: the #GMutex corresponding to @mutex.
 *
 * Deprecated: 2.32: Just use a #GMutex
 */
GMutex *
g_static_mutex_get_mutex_impl (GStaticMutex* mutex)
{
  GMutex *result;

  if (!g_thread_supported ())
    return NULL;

  result = g_atomic_pointer_get (&mutex->mutex);

  if (!result)
    {
      G_LOCK (g_static_mutex);

      result = mutex->mutex;
      if (!result)
        {
          result = g_mutex_new ();
          g_atomic_pointer_set (&mutex->mutex, result);
        }

      G_UNLOCK (g_static_mutex);
    }

  return result;
}

/* IMPLEMENTATION NOTE:
 *
 * g_static_mutex_lock(), g_static_mutex_trylock() and
 * g_static_mutex_unlock() are all preprocessor macros that wrap the
 * corresponding g_mutex_*() function around a call to
 * g_static_mutex_get_mutex().
 */

/**
 * g_static_mutex_lock:
 * @mutex: a #GStaticMutex.
 *
 * Works like g_mutex_lock(), but for a #GStaticMutex.
 *
 * Deprecated: 2.32: Use g_mutex_lock()
 */

/**
 * g_static_mutex_trylock:
 * @mutex: a #GStaticMutex.
 *
 * Works like g_mutex_trylock(), but for a #GStaticMutex.
 *
 * Returns: %TRUE, if the #GStaticMutex could be locked.
 *
 * Deprecated: 2.32: Use g_mutex_trylock()
 */

/**
 * g_static_mutex_unlock:
 * @mutex: a #GStaticMutex.
 *
 * Works like g_mutex_unlock(), but for a #GStaticMutex.
 *
 * Deprecated: 2.32: Use g_mutex_unlock()
 */

/**
 * g_static_mutex_free:
 * @mutex: a #GStaticMutex to be freed.
 *
 * Releases all resources allocated to @mutex.
 *
 * You don't have to call this functions for a #GStaticMutex with an
 * unbounded lifetime, i.e. objects declared 'static', but if you have
 * a #GStaticMutex as a member of a structure and the structure is
 * freed, you should also free the #GStaticMutex.
 *
 * Calling g_static_mutex_free() on a locked mutex may result in
 * undefined behaviour.
 *
 * Deprecated: 2.32: Use g_mutex_clear()
 */
void
g_static_mutex_free (GStaticMutex* mutex)
{
  GMutex **runtime_mutex;

  g_return_if_fail (mutex);

  /* The runtime_mutex is the first (or only) member of GStaticMutex,
   * see both versions (of glibconfig.h) in configure.ac. Note, that
   * this variable is NULL, if g_thread_init() hasn't been called or
   * if we're using the default thread implementation and it provides
   * static mutexes. */
  runtime_mutex = ((GMutex**)mutex);

  if (*runtime_mutex)
    g_mutex_free (*runtime_mutex);

  *runtime_mutex = NULL;
}

/* {{{1 GStaticRecMutex */

/**
 * GStaticRecMutex:
 *
 * A #GStaticRecMutex works like a #GStaticMutex, but it can be locked
 * multiple times by one thread. If you enter it n times, you have to
 * unlock it n times again to let other threads lock it. An exception
 * is the function g_static_rec_mutex_unlock_full(): that allows you to
 * unlock a #GStaticRecMutex completely returning the depth, (i.e. the
 * number of times this mutex was locked). The depth can later be used
 * to restore the state of the #GStaticRecMutex by calling
 * g_static_rec_mutex_lock_full(). In GLib 2.32, #GStaticRecMutex has
 * been deprecated in favor of #GRecMutex.
 *
 * Even though #GStaticRecMutex is not opaque, it should only be used
 * with the following functions.
 *
 * All of the g_static_rec_mutex_* functions can be used even if
 * g_thread_init() has not been called. Then they do nothing, apart
 * from g_static_rec_mutex_trylock(), which does nothing but returning
 * %TRUE.
 */

/**
 * G_STATIC_REC_MUTEX_INIT:
 *
 * A #GStaticRecMutex must be initialized with this macro before it can
 * be used. This macro can used be to initialize a variable, but it
 * cannot be assigned to a variable. In that case you have to use
 * g_static_rec_mutex_init().
 *
 * |[
 *   GStaticRecMutex my_mutex = G_STATIC_REC_MUTEX_INIT;
 * ]|
 */

/**
 * g_static_rec_mutex_init:
 * @mutex: a #GStaticRecMutex to be initialized.
 *
 * A #GStaticRecMutex must be initialized with this function before it
 * can be used. Alternatively you can initialize it with
 * #G_STATIC_REC_MUTEX_INIT.
 *
 * Deprecated: 2.32: Use g_rec_mutex_init()
 */
void
g_static_rec_mutex_init (GStaticRecMutex *mutex)
{
  static const GStaticRecMutex init_mutex = G_STATIC_REC_MUTEX_INIT;

  g_return_if_fail (mutex);

  *mutex = init_mutex;
}

static GRecMutex *
g_static_rec_mutex_get_rec_mutex_impl (GStaticRecMutex* mutex)
{
  GRecMutex *result;

  if (!g_thread_supported ())
    return NULL;

  result = g_atomic_pointer_get (&mutex->mutex.mutex);

  if (!result)
    {
      G_LOCK (g_static_mutex);

      result = (GRecMutex *) mutex->mutex.mutex;
      if (!result)
        {
          result = g_slice_new (GRecMutex);
          g_rec_mutex_init (result);
          g_atomic_pointer_set (&mutex->mutex.mutex, result);
        }

      G_UNLOCK (g_static_mutex);
    }

  return result;
}

/**
 * g_static_rec_mutex_lock:
 * @mutex: a #GStaticRecMutex to lock.
 *
 * Locks @mutex. If @mutex is already locked by another thread, the
 * current thread will block until @mutex is unlocked by the other
 * thread. If @mutex is already locked by the calling thread, this
 * functions increases the depth of @mutex and returns immediately.
 *
 * Deprecated: 2.32: Use g_rec_mutex_lock()
 */
void
g_static_rec_mutex_lock (GStaticRecMutex* mutex)
{
  GRecMutex *rm;
  rm = g_static_rec_mutex_get_rec_mutex_impl (mutex);
  g_rec_mutex_lock (rm);
  mutex->depth++;
}

/**
 * g_static_rec_mutex_trylock:
 * @mutex: a #GStaticRecMutex to lock.
 *
 * Tries to lock @mutex. If @mutex is already locked by another thread,
 * it immediately returns %FALSE. Otherwise it locks @mutex and returns
 * %TRUE. If @mutex is already locked by the calling thread, this
 * functions increases the depth of @mutex and immediately returns
 * %TRUE.
 *
 * Returns: %TRUE, if @mutex could be locked.
 *
 * Deprecated: 2.32: Use g_rec_mutex_trylock()
 */
gboolean
g_static_rec_mutex_trylock (GStaticRecMutex* mutex)
{
  GRecMutex *rm;
  rm = g_static_rec_mutex_get_rec_mutex_impl (mutex);

  if (g_rec_mutex_trylock (rm))
    {
      mutex->depth++;
      return TRUE;
    }
  else
    return FALSE;
}

/**
 * g_static_rec_mutex_unlock:
 * @mutex: a #GStaticRecMutex to unlock.
 *
 * Unlocks @mutex. Another thread will be allowed to lock @mutex only
 * when it has been unlocked as many times as it had been locked
 * before. If @mutex is completely unlocked and another thread is
 * blocked in a g_static_rec_mutex_lock() call for @mutex, it will be
 * woken and can lock @mutex itself.
 *
 * Deprecated: 2.32: Use g_rec_mutex_unlock()
 */
void
g_static_rec_mutex_unlock (GStaticRecMutex* mutex)
{
  GRecMutex *rm;
  rm = g_static_rec_mutex_get_rec_mutex_impl (mutex);
  mutex->depth--;
  g_rec_mutex_unlock (rm);
}

/**
 * g_static_rec_mutex_lock_full:
 * @mutex: a #GStaticRecMutex to lock.
 * @depth: number of times this mutex has to be unlocked to be
 *         completely unlocked.
 *
 * Works like calling g_static_rec_mutex_lock() for @mutex @depth times.
 *
 * Deprecated: 2.32: Use g_rec_mutex_lock()
 */
void
g_static_rec_mutex_lock_full (GStaticRecMutex *mutex,
                              guint            depth)
{
  GRecMutex *rm;

  rm = g_static_rec_mutex_get_rec_mutex_impl (mutex);
  while (depth--)
    {
      g_rec_mutex_lock (rm);
      mutex->depth++;
    }
}

/**
 * g_static_rec_mutex_unlock_full:
 * @mutex: a #GStaticRecMutex to completely unlock.
 *
 * Completely unlocks @mutex. If another thread is blocked in a
 * g_static_rec_mutex_lock() call for @mutex, it will be woken and can
 * lock @mutex itself. This function returns the number of times that
 * @mutex has been locked by the current thread. To restore the state
 * before the call to g_static_rec_mutex_unlock_full() you can call
 * g_static_rec_mutex_lock_full() with the depth returned by this
 * function.
 *
 * Returns: number of times @mutex has been locked by the current
 *          thread.
 *
 * Deprecated: 2.32: Use g_rec_mutex_unlock()
 */
guint
g_static_rec_mutex_unlock_full (GStaticRecMutex *mutex)
{
  GRecMutex *rm;
  gint depth;
  gint i;

  rm = g_static_rec_mutex_get_rec_mutex_impl (mutex);

  /* all access to mutex->depth done while still holding the lock */
  depth = mutex->depth;
  i = mutex->depth;
  mutex->depth = 0;

  while (i--)
    g_rec_mutex_unlock (rm);

  return depth;
}

/**
 * g_static_rec_mutex_free:
 * @mutex: a #GStaticRecMutex to be freed.
 *
 * Releases all resources allocated to a #GStaticRecMutex.
 *
 * You don't have to call this functions for a #GStaticRecMutex with an
 * unbounded lifetime, i.e. objects declared 'static', but if you have
 * a #GStaticRecMutex as a member of a structure and the structure is
 * freed, you should also free the #GStaticRecMutex.
 *
 * Deprecated: 2.32: Use g_rec_mutex_clear()
 */
void
g_static_rec_mutex_free (GStaticRecMutex *mutex)
{
  g_return_if_fail (mutex);

  if (mutex->mutex.mutex)
    {
      GRecMutex *rm = (GRecMutex *) mutex->mutex.mutex;

      g_rec_mutex_clear (rm);
      g_slice_free (GRecMutex, rm);
    }
}

/* GStaticRWLock {{{1 ----------------------------------------------------- */

/**
 * GStaticRWLock:
 *
 * The #GStaticRWLock struct represents a read-write lock. A read-write
 * lock can be used for protecting data that some portions of code only
 * read from, while others also write. In such situations it is
 * desirable that several readers can read at once, whereas of course
 * only one writer may write at a time.
 *
 * Take a look at the following example:
 * |[
 *   GStaticRWLock rwlock = G_STATIC_RW_LOCK_INIT;
 *   GPtrArray *array;
 *
 *   gpointer
 *   my_array_get (guint index)
 *   {
 *     gpointer retval = NULL;
 *
 *     if (!array)
 *       return NULL;
 *
 *     g_static_rw_lock_reader_lock (&rwlock);
 *     if (index < array->len)
 *       retval = g_ptr_array_index (array, index);
 *     g_static_rw_lock_reader_unlock (&rwlock);
 *
 *     return retval;
 *   }
 *
 *   void
 *   my_array_set (guint index, gpointer data)
 *   {
 *     g_static_rw_lock_writer_lock (&rwlock);
 *
 *     if (!array)
 *       array = g_ptr_array_new ();
 *
 *     if (index >= array->len)
 *       g_ptr_array_set_size (array, index + 1);
 *     g_ptr_array_index (array, index) = data;
 *
 *     g_static_rw_lock_writer_unlock (&rwlock);
 *   }
 * ]|
 *
 * This example shows an array which can be accessed by many readers
 * (the my_array_get() function) simultaneously, whereas the writers
 * (the my_array_set() function) will only be allowed once at a time
 * and only if no readers currently access the array. This is because
 * of the potentially dangerous resizing of the array. Using these
 * functions is fully multi-thread safe now.
 *
 * Most of the time, writers should have precedence over readers. That
 * means, for this implementation, that as soon as a writer wants to
 * lock the data, no other reader is allowed to lock the data, whereas,
 * of course, the readers that already have locked the data are allowed
 * to finish their operation. As soon as the last reader unlocks the
 * data, the writer will lock it.
 *
 * Even though #GStaticRWLock is not opaque, it should only be used
 * with the following functions.
 *
 * All of the g_static_rw_lock_* functions can be used even if
 * g_thread_init() has not been called. Then they do nothing, apart
 * from g_static_rw_lock_*_trylock, which does nothing but returning %TRUE.
 *
 * A read-write lock has a higher overhead than a mutex. For example, both
 * g_static_rw_lock_reader_lock() and g_static_rw_lock_reader_unlock() have
 * to lock and unlock a #GStaticMutex, so it takes at least twice the time
 * to lock and unlock a #GStaticRWLock that it does to lock and unlock a
 * #GStaticMutex. So only data structures that are accessed by multiple
 * readers, and which keep the lock for a considerable time justify a
 * #GStaticRWLock. The above example most probably would fare better with a
 * #GStaticMutex.
 *
 * Deprecated: 2.32: Use a #GRWLock instead
 **/

/**
 * G_STATIC_RW_LOCK_INIT:
 *
 * A #GStaticRWLock must be initialized with this macro before it can
 * be used. This macro can used be to initialize a variable, but it
 * cannot be assigned to a variable. In that case you have to use
 * g_static_rw_lock_init().
 *
 * |[
 *   GStaticRWLock my_lock = G_STATIC_RW_LOCK_INIT;
 * ]|
 */

/**
 * g_static_rw_lock_init:
 * @lock: a #GStaticRWLock to be initialized.
 *
 * A #GStaticRWLock must be initialized with this function before it
 * can be used. Alternatively you can initialize it with
 * #G_STATIC_RW_LOCK_INIT.
 *
 * Deprecated: 2.32: Use g_rw_lock_init() instead
 */
void
g_static_rw_lock_init (GStaticRWLock* lock)
{
  static const GStaticRWLock init_lock = G_STATIC_RW_LOCK_INIT;

  g_return_if_fail (lock);

  *lock = init_lock;
}

inline static void
g_static_rw_lock_wait (GCond** cond, GStaticMutex* mutex)
{
  if (!*cond)
      *cond = g_cond_new ();
  g_cond_wait (*cond, g_static_mutex_get_mutex (mutex));
}

inline static void
g_static_rw_lock_signal (GStaticRWLock* lock)
{
  if (lock->want_to_write && lock->write_cond)
    g_cond_signal (lock->write_cond);
  else if (lock->want_to_read && lock->read_cond)
    g_cond_broadcast (lock->read_cond);
}

/**
 * g_static_rw_lock_reader_lock:
 * @lock: a #GStaticRWLock to lock for reading.
 *
 * Locks @lock for reading. There may be unlimited concurrent locks for
 * reading of a #GStaticRWLock at the same time.  If @lock is already
 * locked for writing by another thread or if another thread is already
 * waiting to lock @lock for writing, this function will block until
 * @lock is unlocked by the other writing thread and no other writing
 * threads want to lock @lock. This lock has to be unlocked by
 * g_static_rw_lock_reader_unlock().
 *
 * #GStaticRWLock is not recursive. It might seem to be possible to
 * recursively lock for reading, but that can result in a deadlock, due
 * to writer preference.
 *
 * Deprecated: 2.32: Use g_rw_lock_reader_lock() instead
 */
void
g_static_rw_lock_reader_lock (GStaticRWLock* lock)
{
  g_return_if_fail (lock);

  if (!g_threads_got_initialized)
    return;

  g_static_mutex_lock (&lock->mutex);
  lock->want_to_read++;
  while (lock->have_writer || lock->want_to_write)
    g_static_rw_lock_wait (&lock->read_cond, &lock->mutex);
  lock->want_to_read--;
  lock->read_counter++;
  g_static_mutex_unlock (&lock->mutex);
}

/**
 * g_static_rw_lock_reader_trylock:
 * @lock: a #GStaticRWLock to lock for reading
 *
 * Tries to lock @lock for reading. If @lock is already locked for
 * writing by another thread or if another thread is already waiting to
 * lock @lock for writing, immediately returns %FALSE. Otherwise locks
 * @lock for reading and returns %TRUE. This lock has to be unlocked by
 * g_static_rw_lock_reader_unlock().
 *
 * Returns: %TRUE, if @lock could be locked for reading
 *
 * Deprectated: 2.32: Use g_rw_lock_reader_trylock() instead
 */
gboolean
g_static_rw_lock_reader_trylock (GStaticRWLock* lock)
{
  gboolean ret_val = FALSE;

  g_return_val_if_fail (lock, FALSE);

  if (!g_threads_got_initialized)
    return TRUE;

  g_static_mutex_lock (&lock->mutex);
  if (!lock->have_writer && !lock->want_to_write)
    {
      lock->read_counter++;
      ret_val = TRUE;
    }
  g_static_mutex_unlock (&lock->mutex);
  return ret_val;
}

/**
 * g_static_rw_lock_reader_unlock:
 * @lock: a #GStaticRWLock to unlock after reading
 *
 * Unlocks @lock. If a thread waits to lock @lock for writing and all
 * locks for reading have been unlocked, the waiting thread is woken up
 * and can lock @lock for writing.
 *
 * Deprectated: 2.32: Use g_rw_lock_reader_unlock() instead
 */
void
g_static_rw_lock_reader_unlock  (GStaticRWLock* lock)
{
  g_return_if_fail (lock);

  if (!g_threads_got_initialized)
    return;

  g_static_mutex_lock (&lock->mutex);
  lock->read_counter--;
  if (lock->read_counter == 0)
    g_static_rw_lock_signal (lock);
  g_static_mutex_unlock (&lock->mutex);
}

/**
 * g_static_rw_lock_writer_lock:
 * @lock: a #GStaticRWLock to lock for writing
 *
 * Locks @lock for writing. If @lock is already locked for writing or
 * reading by other threads, this function will block until @lock is
 * completely unlocked and then lock @lock for writing. While this
 * functions waits to lock @lock, no other thread can lock @lock for
 * reading. When @lock is locked for writing, no other thread can lock
 * @lock (neither for reading nor writing). This lock has to be
 * unlocked by g_static_rw_lock_writer_unlock().
 *
 * Deprectated: 2.32: Use g_rw_lock_writer_lock() instead
 */
void
g_static_rw_lock_writer_lock (GStaticRWLock* lock)
{
  g_return_if_fail (lock);

  if (!g_threads_got_initialized)
    return;

  g_static_mutex_lock (&lock->mutex);
  lock->want_to_write++;
  while (lock->have_writer || lock->read_counter)
    g_static_rw_lock_wait (&lock->write_cond, &lock->mutex);
  lock->want_to_write--;
  lock->have_writer = TRUE;
  g_static_mutex_unlock (&lock->mutex);
}

/**
 * g_static_rw_lock_writer_trylock:
 * @lock: a #GStaticRWLock to lock for writing
 *
 * Tries to lock @lock for writing. If @lock is already locked (for
 * either reading or writing) by another thread, it immediately returns
 * %FALSE. Otherwise it locks @lock for writing and returns %TRUE. This
 * lock has to be unlocked by g_static_rw_lock_writer_unlock().
 *
 * Returns: %TRUE, if @lock could be locked for writing
 *
 * Deprectated: 2.32: Use g_rw_lock_writer_trylock() instead
 */
gboolean
g_static_rw_lock_writer_trylock (GStaticRWLock* lock)
{
  gboolean ret_val = FALSE;

  g_return_val_if_fail (lock, FALSE);

  if (!g_threads_got_initialized)
    return TRUE;

  g_static_mutex_lock (&lock->mutex);
  if (!lock->have_writer && !lock->read_counter)
    {
      lock->have_writer = TRUE;
      ret_val = TRUE;
    }
  g_static_mutex_unlock (&lock->mutex);
  return ret_val;
}

/**
 * g_static_rw_lock_writer_unlock:
 * @lock: a #GStaticRWLock to unlock after writing.
 *
 * Unlocks @lock. If a thread is waiting to lock @lock for writing and
 * all locks for reading have been unlocked, the waiting thread is
 * woken up and can lock @lock for writing. If no thread is waiting to
 * lock @lock for writing, and some thread or threads are waiting to
 * lock @lock for reading, the waiting threads are woken up and can
 * lock @lock for reading.
 *
 * Deprectated: 2.32: Use g_rw_lock_writer_unlock() instead
 */
void
g_static_rw_lock_writer_unlock (GStaticRWLock* lock)
{
  g_return_if_fail (lock);

  if (!g_threads_got_initialized)
    return;

  g_static_mutex_lock (&lock->mutex);
  lock->have_writer = FALSE;
  g_static_rw_lock_signal (lock);
  g_static_mutex_unlock (&lock->mutex);
}

/**
 * g_static_rw_lock_free:
 * @lock: a #GStaticRWLock to be freed.
 *
 * Releases all resources allocated to @lock.
 *
 * You don't have to call this functions for a #GStaticRWLock with an
 * unbounded lifetime, i.e. objects declared 'static', but if you have
 * a #GStaticRWLock as a member of a structure, and the structure is
 * freed, you should also free the #GStaticRWLock.
 *
 * Deprecated: 2.32: Use a #GRWLock instead
 */
void
g_static_rw_lock_free (GStaticRWLock* lock)
{
  g_return_if_fail (lock);

  if (lock->read_cond)
    {
      g_cond_free (lock->read_cond);
      lock->read_cond = NULL;
    }
  if (lock->write_cond)
    {
      g_cond_free (lock->write_cond);
      lock->write_cond = NULL;
    }
  g_static_mutex_free (&lock->mutex);
}

/* GPrivate {{{1 ------------------------------------------------------ */

/**
 * g_private_new:
 * @notify: a #GDestroyNotify
 *
 * Creates a new #GPrivate.
 *
 * Deprecated:2.32: dynamic allocation of #GPrivate is a bad idea.  Use
 *                  static storage and G_PRIVATE_INIT() instead.
 *
 * Returns: a newly allocated #GPrivate (which can never be destroyed)
 */
GPrivate *
g_private_new (GDestroyNotify notify)
{
  GPrivate tmp = G_PRIVATE_INIT (notify);
  GPrivate *key;

  key = g_slice_new (GPrivate);
  *key = tmp;

  return key;
}

/* {{{1 GStaticPrivate */

typedef struct _GStaticPrivateNode GStaticPrivateNode;
struct _GStaticPrivateNode
{
  gpointer        data;
  GDestroyNotify  destroy;
  GStaticPrivate *owner;
};

static void
g_static_private_cleanup (gpointer data)
{
  GArray *array = data;
  guint i;

  for (i = 0; i < array->len; i++ )
    {
      GStaticPrivateNode *node = &g_array_index (array, GStaticPrivateNode, i);
      if (node->destroy)
        node->destroy (node->data);
    }

  g_array_free (array, TRUE);
}

GPrivate static_private_private = G_PRIVATE_INIT (g_static_private_cleanup);

/**
 * GStaticPrivate:
 *
 * A #GStaticPrivate works almost like a #GPrivate, but it has one
 * significant advantage. It doesn't need to be created at run-time
 * like a #GPrivate, but can be defined at compile-time. This is
 * similar to the difference between #GMutex and #GStaticMutex.
 *
 * Now look at our give_me_next_number() example with #GStaticPrivate:
 * |[
 *   int
 *   give_me_next_number ()
 *   {
 *     static GStaticPrivate current_number_key = G_STATIC_PRIVATE_INIT;
 *     int *current_number = g_static_private_get (&current_number_key);
 *
 *     if (!current_number)
 *       {
 *         current_number = g_new (int, 1);
 *         *current_number = 0;
 *         g_static_private_set (&current_number_key, current_number, g_free);
 *       }
 *
 *     *current_number = calc_next_number (*current_number);
 *
 *     return *current_number;
 *   }
 * ]|
 */

/**
 * G_STATIC_PRIVATE_INIT:
 *
 * Every #GStaticPrivate must be initialized with this macro, before it
 * can be used.
 *
 * |[
 *   GStaticPrivate my_private = G_STATIC_PRIVATE_INIT;
 * ]|
 */

/**
 * g_static_private_init:
 * @private_key: a #GStaticPrivate to be initialized
 *
 * Initializes @private_key. Alternatively you can initialize it with
 * #G_STATIC_PRIVATE_INIT.
 */
void
g_static_private_init (GStaticPrivate *private_key)
{
  private_key->index = 0;
}

/**
 * g_static_private_get:
 * @private_key: a #GStaticPrivate
 *
 * Works like g_private_get() only for a #GStaticPrivate.
 *
 * This function works even if g_thread_init() has not yet been called.
 *
 * Returns: the corresponding pointer
 */
gpointer
g_static_private_get (GStaticPrivate *private_key)
{
  GArray *array;
  gpointer ret = NULL;

  array = g_private_get (&static_private_private);

  if (array && private_key->index != 0 && private_key->index <= array->len)
    {
      GStaticPrivateNode *node;

      node = &g_array_index (array, GStaticPrivateNode, private_key->index - 1);

      /* Deal with the possibility that the GStaticPrivate which used
       * to have this index got freed and the index got allocated to
       * a new one. In this case, the data in the node is stale, so
       * free it and return NULL.
       */
      if (G_UNLIKELY (node->owner != private_key))
        {
          if (node->destroy)
            node->destroy (node->data);
          node->destroy = NULL;
          node->data = NULL;
          node->owner = NULL;
        }
      ret = node->data;
    }

  return ret;
}

/**
 * g_static_private_set:
 * @private_key: a #GStaticPrivate
 * @data: the new pointer
 * @notify: a function to be called with the pointer whenever the
 *     current thread ends or sets this pointer again
 *
 * Sets the pointer keyed to @private_key for the current thread and
 * the function @notify to be called with that pointer (%NULL or
 * non-%NULL), whenever the pointer is set again or whenever the
 * current thread ends.
 *
 * This function works even if g_thread_init() has not yet been called.
 * If g_thread_init() is called later, the @data keyed to @private_key
 * will be inherited only by the main thread, i.e. the one that called
 * g_thread_init().
 *
 * @notify is used quite differently from @destructor in g_private_new().
 */
void
g_static_private_set (GStaticPrivate *private_key,
                      gpointer        data,
                      GDestroyNotify  notify)
{
  GArray *array;
  static guint next_index = 0;
  GStaticPrivateNode *node;

  if (!private_key->index)
    {
      G_LOCK (g_thread);

      if (!private_key->index)
        {
          if (g_thread_free_indices)
            {
              private_key->index = GPOINTER_TO_UINT (g_thread_free_indices->data);
              g_thread_free_indices = g_slist_delete_link (g_thread_free_indices,
                                                           g_thread_free_indices);
            }
          else
            private_key->index = ++next_index;
        }

      G_UNLOCK (g_thread);
    }

  array = g_private_get (&static_private_private);
  if (!array)
    {
      array = g_array_new (FALSE, TRUE, sizeof (GStaticPrivateNode));
      g_private_set (&static_private_private, array);
    }
  if (private_key->index > array->len)
    g_array_set_size (array, private_key->index);

  node = &g_array_index (array, GStaticPrivateNode, private_key->index - 1);

  if (node->destroy)
    node->destroy (node->data);

  node->data = data;
  node->destroy = notify;
  node->owner = private_key;
}

/**
 * g_static_private_free:
 * @private_key: a #GStaticPrivate to be freed
 *
 * Releases all resources allocated to @private_key.
 *
 * You don't have to call this functions for a #GStaticPrivate with an
 * unbounded lifetime, i.e. objects declared 'static', but if you have
 * a #GStaticPrivate as a member of a structure and the structure is
 * freed, you should also free the #GStaticPrivate.
 */
void
g_static_private_free (GStaticPrivate *private_key)
{
  guint idx = private_key->index;

  if (!idx)
    return;

  private_key->index = 0;

  /* Freeing the per-thread data is deferred to either the
   * thread end or the next g_static_private_get() call for
   * the same index.
   */
  G_LOCK (g_thread);
  g_thread_free_indices = g_slist_prepend (g_thread_free_indices,
                                           GUINT_TO_POINTER (idx));
  G_UNLOCK (g_thread);
}

/* GMutex {{{1 ------------------------------------------------------ */

/**
 * g_mutex_new:
 *
 * Allocates and initializes a new #GMutex.
 *
 * Returns: a newly allocated #GMutex. Use g_mutex_free() to free
 *
 * Deprecated: 2.32: GMutex can now be statically allocated, or embedded
 * in structures and initialised with g_mutex_init().
 */
GMutex *
g_mutex_new (void)
{
  GMutex *mutex;

  mutex = g_slice_new (GMutex);
  g_mutex_init (mutex);

  return mutex;
}

/**
 * g_mutex_free:
 * @mutex: a #GMutex
 *
 * Destroys a @mutex that has been created with g_mutex_new().
 *
 * Calling g_mutex_free() on a locked mutex may result
 * in undefined behaviour.
 *
 * Deprecated: 2.32: GMutex can now be statically allocated, or embedded
 * in structures and initialised with g_mutex_init().
 */
void
g_mutex_free (GMutex *mutex)
{
  g_mutex_clear (mutex);
  g_slice_free (GMutex, mutex);
}

/* GCond {{{1 ------------------------------------------------------ */

/**
 * g_cond_new:
 *
 * Allocates and initializes a new #GCond.
 *
 * Returns: a newly allocated #GCond. Free with g_cond_free()
 *
 * Deprecated: 2.32: GCond can now be statically allocated, or embedded
 * in structures and initialised with g_cond_init().
 */
GCond *
g_cond_new (void)
{
  GCond *cond;

  cond = g_slice_new (GCond);
  g_cond_init (cond);

  return cond;
}

/**
 * g_cond_free:
 * @cond: a #GCond
 *
 * Destroys a #GCond that has been created with g_cond_new().
 *
 * Calling g_cond_free() for a #GCond on which threads are
 * blocking leads to undefined behaviour.
 *
 * Deprecated: 2.32: GCond can now be statically allocated, or embedded
 * in structures and initialised with g_cond_init().
 */
void
g_cond_free (GCond *cond)
{
  g_cond_clear (cond);
  g_slice_free (GCond, cond);
}

/**
 * g_cond_timed_wait:
 * @cond: a #GCond
 * @mutex: a #GMutex that is currently locked
 * @abs_time: a #GTimeVal, determining the final time
 *
 * Waits until this thread is woken up on @cond, but not longer than
 * until the time specified by @abs_time. The @mutex is unlocked before
 * falling asleep and locked again before resuming.
 *
 * If @abs_time is %NULL, g_cond_timed_wait() acts like g_cond_wait().
 *
 * This function can be used even if g_thread_init() has not yet been
 * called, and, in that case, will immediately return %TRUE.
 *
 * To easily calculate @abs_time a combination of g_get_current_time()
 * and g_time_val_add() can be used.
 *
 * Returns: %TRUE if @cond was signalled, or %FALSE on timeout
 *
 * Deprecated:2.32: Use g_cond_wait_until() instead.
 */
gboolean
g_cond_timed_wait (GCond    *cond,
                   GMutex   *mutex,
                   GTimeVal *abs_time)
{
  gint64 end_time;

  if (abs_time == NULL)
    {
      g_cond_wait (cond, mutex);
      return TRUE;
    }

  end_time = abs_time->tv_sec;
  end_time *= 1000000;
  end_time += abs_time->tv_usec;

  /* would be nice if we had clock_rtoffset, but that didn't seem to
   * make it into the kernel yet...
   */
  end_time += g_get_monotonic_time () - g_get_real_time ();

  return g_cond_wait_until (cond, mutex, end_time);
}

/* {{{1 Epilogue */
/* vim: set foldmethod=marker: */
