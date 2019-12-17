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

/* Prelude {{{1 ----------------------------------------------------------- */

/*
 * Modified by the GLib Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GLib Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GLib at ftp://ftp.gtk.org/pub/gtk/.
 */

/*
 * MT safe
 */

/* implement gthread.h's inline functions */
#define G_IMPLEMENT_INLINES 1
#define __G_THREAD_C__

#include "config.h"

#include "gthread.h"
#include "gthreadprivate.h"

#include <string.h>

#ifdef G_OS_UNIX
#include <unistd.h>
#endif

#ifndef G_OS_WIN32
#include <sys/time.h>
#include <time.h>
#else
#include <windows.h>
#endif /* G_OS_WIN32 */

#include "gslice.h"
#include "gstrfuncs.h"
#include "gtestutils.h"
#include "glib_trace.h"

/**
 * SECTION:threads
 * @title: Threads
 * @short_description: portable support for threads, mutexes, locks,
 *     conditions and thread private data
 * @see_also: #GThreadPool, #GAsyncQueue
 *
 * Threads act almost like processes, but unlike processes all threads
 * of one process share the same memory. This is good, as it provides
 * easy communication between the involved threads via this shared
 * memory, and it is bad, because strange things (so called
 * "Heisenbugs") might happen if the program is not carefully designed.
 * In particular, due to the concurrent nature of threads, no
 * assumptions on the order of execution of code running in different
 * threads can be made, unless order is explicitly forced by the
 * programmer through synchronization primitives.
 *
 * The aim of the thread-related functions in GLib is to provide a
 * portable means for writing multi-threaded software. There are
 * primitives for mutexes to protect the access to portions of memory
 * (#GMutex, #GRecMutex and #GRWLock). There is a facility to use
 * individual bits for locks (g_bit_lock()). There are primitives
 * for condition variables to allow synchronization of threads (#GCond).
 * There are primitives for thread-private data - data that every
 * thread has a private instance of (#GPrivate). There are facilities
 * for one-time initialization (#GOnce, g_once_init_enter()). Finally,
 * there are primitives to create and manage threads (#GThread).
 *
 * The GLib threading system used to be initialized with g_thread_init().
 * This is no longer necessary. Since version 2.32, the GLib threading
 * system is automatically initialized at the start of your program,
 * and all thread-creation functions and synchronization primitives
 * are available right away.
 *
 * Note that it is not safe to assume that your program has no threads
 * even if you don't call g_thread_new() yourself. GLib and GIO can
 * and will create threads for their own purposes in some cases, such
 * as when using g_unix_signal_source_new() or when using GDBus.
 *
 * Originally, UNIX did not have threads, and therefore some traditional
 * UNIX APIs are problematic in threaded programs. Some notable examples
 * are
 * 
 * - C library functions that return data in statically allocated
 *   buffers, such as strtok() or strerror(). For many of these,
 *   there are thread-safe variants with a _r suffix, or you can
 *   look at corresponding GLib APIs (like g_strsplit() or g_strerror()).
 *
 * - The functions setenv() and unsetenv() manipulate the process
 *   environment in a not thread-safe way, and may interfere with getenv()
 *   calls in other threads. Note that getenv() calls may be hidden behind
 *   other APIs. For example, GNU gettext() calls getenv() under the
 *   covers. In general, it is best to treat the environment as readonly.
 *   If you absolutely have to modify the environment, do it early in
 *   main(), when no other threads are around yet.
 *
 * - The setlocale() function changes the locale for the entire process,
 *   affecting all threads. Temporary changes to the locale are often made
 *   to change the behavior of string scanning or formatting functions
 *   like scanf() or printf(). GLib offers a number of string APIs
 *   (like g_ascii_formatd() or g_ascii_strtod()) that can often be
 *   used as an alternative. Or you can use the uselocale() function
 *   to change the locale only for the current thread.
 *
 * - The fork() function only takes the calling thread into the child's
 *   copy of the process image. If other threads were executing in critical
 *   sections they could have left mutexes locked which could easily
 *   cause deadlocks in the new child. For this reason, you should
 *   call exit() or exec() as soon as possible in the child and only
 *   make signal-safe library calls before that.
 *
 * - The daemon() function uses fork() in a way contrary to what is
 *   described above. It should not be used with GLib programs.
 *
 * GLib itself is internally completely thread-safe (all global data is
 * automatically locked), but individual data structure instances are
 * not automatically locked for performance reasons. For example,
 * you must coordinate accesses to the same #GHashTable from multiple
 * threads. The two notable exceptions from this rule are #GMainLoop
 * and #GAsyncQueue, which are thread-safe and need no further
 * application-level locking to be accessed from multiple threads.
 * Most refcounting functions such as g_object_ref() are also thread-safe.
 *
 * A common use for #GThreads is to move a long-running blocking operation out
 * of the main thread and into a worker thread. For GLib functions, such as
 * single GIO operations, this is not necessary, and complicates the code.
 * Instead, the `…_async()` version of the function should be used from the main
 * thread, eliminating the need for locking and synchronisation between multiple
 * threads. If an operation does need to be moved to a worker thread, consider
 * using g_task_run_in_thread(), or a #GThreadPool. #GThreadPool is often a
 * better choice than #GThread, as it handles thread reuse and task queueing;
 * #GTask uses this internally.
 *
 * However, if multiple blocking operations need to be performed in sequence,
 * and it is not possible to use #GTask for them, moving them to a worker thread
 * can clarify the code.
 */

/* G_LOCK Documentation {{{1 ---------------------------------------------- */

/**
 * G_LOCK_DEFINE:
 * @name: the name of the lock
 *
 * The #G_LOCK_ macros provide a convenient interface to #GMutex.
 * #G_LOCK_DEFINE defines a lock. It can appear in any place where
 * variable definitions may appear in programs, i.e. in the first block
 * of a function or outside of functions. The @name parameter will be
 * mangled to get the name of the #GMutex. This means that you
 * can use names of existing variables as the parameter - e.g. the name
 * of the variable you intend to protect with the lock. Look at our
 * give_me_next_number() example using the #G_LOCK macros:
 *
 * Here is an example for using the #G_LOCK convenience macros:
 * |[<!-- language="C" --> 
 *   G_LOCK_DEFINE (current_number);
 *
 *   int
 *   give_me_next_number (void)
 *   {
 *     static int current_number = 0;
 *     int ret_val;
 *
 *     G_LOCK (current_number);
 *     ret_val = current_number = calc_next_number (current_number);
 *     G_UNLOCK (current_number);
 *
 *     return ret_val;
 *   }
 * ]|
 */

/**
 * G_LOCK_DEFINE_STATIC:
 * @name: the name of the lock
 *
 * This works like #G_LOCK_DEFINE, but it creates a static object.
 */

/**
 * G_LOCK_EXTERN:
 * @name: the name of the lock
 *
 * This declares a lock, that is defined with #G_LOCK_DEFINE in another
 * module.
 */

/**
 * G_LOCK:
 * @name: the name of the lock
 *
 * Works like g_mutex_lock(), but for a lock defined with
 * #G_LOCK_DEFINE.
 */

/**
 * G_TRYLOCK:
 * @name: the name of the lock
 *
 * Works like g_mutex_trylock(), but for a lock defined with
 * #G_LOCK_DEFINE.
 *
 * Returns: %TRUE, if the lock could be locked.
 */

/**
 * G_UNLOCK:
 * @name: the name of the lock
 *
 * Works like g_mutex_unlock(), but for a lock defined with
 * #G_LOCK_DEFINE.
 */

/* GMutex Documentation {{{1 ------------------------------------------ */

/**
 * GMutex:
 *
 * The #GMutex struct is an opaque data structure to represent a mutex
 * (mutual exclusion). It can be used to protect data against shared
 * access.
 *
 * Take for example the following function:
 * |[<!-- language="C" --> 
 *   int
 *   give_me_next_number (void)
 *   {
 *     static int current_number = 0;
 *
 *     // now do a very complicated calculation to calculate the new
 *     // number, this might for example be a random number generator
 *     current_number = calc_next_number (current_number);
 *
 *     return current_number;
 *   }
 * ]|
 * It is easy to see that this won't work in a multi-threaded
 * application. There current_number must be protected against shared
 * access. A #GMutex can be used as a solution to this problem:
 * |[<!-- language="C" --> 
 *   int
 *   give_me_next_number (void)
 *   {
 *     static GMutex mutex;
 *     static int current_number = 0;
 *     int ret_val;
 *
 *     g_mutex_lock (&mutex);
 *     ret_val = current_number = calc_next_number (current_number);
 *     g_mutex_unlock (&mutex);
 *
 *     return ret_val;
 *   }
 * ]|
 * Notice that the #GMutex is not initialised to any particular value.
 * Its placement in static storage ensures that it will be initialised
 * to all-zeros, which is appropriate.
 *
 * If a #GMutex is placed in other contexts (eg: embedded in a struct)
 * then it must be explicitly initialised using g_mutex_init().
 *
 * A #GMutex should only be accessed via g_mutex_ functions.
 */

/* GRecMutex Documentation {{{1 -------------------------------------- */

/**
 * GRecMutex:
 *
 * The GRecMutex struct is an opaque data structure to represent a
 * recursive mutex. It is similar to a #GMutex with the difference
 * that it is possible to lock a GRecMutex multiple times in the same
 * thread without deadlock. When doing so, care has to be taken to
 * unlock the recursive mutex as often as it has been locked.
 *
 * If a #GRecMutex is allocated in static storage then it can be used
 * without initialisation.  Otherwise, you should call
 * g_rec_mutex_init() on it and g_rec_mutex_clear() when done.
 *
 * A GRecMutex should only be accessed with the
 * g_rec_mutex_ functions.
 *
 * Since: 2.32
 */

/* GRWLock Documentation {{{1 ---------------------------------------- */

/**
 * GRWLock:
 *
 * The GRWLock struct is an opaque data structure to represent a
 * reader-writer lock. It is similar to a #GMutex in that it allows
 * multiple threads to coordinate access to a shared resource.
 *
 * The difference to a mutex is that a reader-writer lock discriminates
 * between read-only ('reader') and full ('writer') access. While only
 * one thread at a time is allowed write access (by holding the 'writer'
 * lock via g_rw_lock_writer_lock()), multiple threads can gain
 * simultaneous read-only access (by holding the 'reader' lock via
 * g_rw_lock_reader_lock()).
 *
 * Here is an example for an array with access functions:
 * |[<!-- language="C" --> 
 *   GRWLock lock;
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
 *     g_rw_lock_reader_lock (&lock);
 *     if (index < array->len)
 *       retval = g_ptr_array_index (array, index);
 *     g_rw_lock_reader_unlock (&lock);
 *
 *     return retval;
 *   }
 *
 *   void
 *   my_array_set (guint index, gpointer data)
 *   {
 *     g_rw_lock_writer_lock (&lock);
 *
 *     if (!array)
 *       array = g_ptr_array_new ();
 *
 *     if (index >= array->len)
 *       g_ptr_array_set_size (array, index+1);
 *     g_ptr_array_index (array, index) = data;
 *
 *     g_rw_lock_writer_unlock (&lock);
 *   }
 *  ]|
 * This example shows an array which can be accessed by many readers
 * (the my_array_get() function) simultaneously, whereas the writers
 * (the my_array_set() function) will only be allowed one at a time
 * and only if no readers currently access the array. This is because
 * of the potentially dangerous resizing of the array. Using these
 * functions is fully multi-thread safe now.
 *
 * If a #GRWLock is allocated in static storage then it can be used
 * without initialisation.  Otherwise, you should call
 * g_rw_lock_init() on it and g_rw_lock_clear() when done.
 *
 * A GRWLock should only be accessed with the g_rw_lock_ functions.
 *
 * Since: 2.32
 */

/* GCond Documentation {{{1 ------------------------------------------ */

/**
 * GCond:
 *
 * The #GCond struct is an opaque data structure that represents a
 * condition. Threads can block on a #GCond if they find a certain
 * condition to be false. If other threads change the state of this
 * condition they signal the #GCond, and that causes the waiting
 * threads to be woken up.
 *
 * Consider the following example of a shared variable.  One or more
 * threads can wait for data to be published to the variable and when
 * another thread publishes the data, it can signal one of the waiting
 * threads to wake up to collect the data.
 *
 * Here is an example for using GCond to block a thread until a condition
 * is satisfied:
 * |[<!-- language="C" --> 
 *   gpointer current_data = NULL;
 *   GMutex data_mutex;
 *   GCond data_cond;
 *
 *   void
 *   push_data (gpointer data)
 *   {
 *     g_mutex_lock (&data_mutex);
 *     current_data = data;
 *     g_cond_signal (&data_cond);
 *     g_mutex_unlock (&data_mutex);
 *   }
 *
 *   gpointer
 *   pop_data (void)
 *   {
 *     gpointer data;
 *
 *     g_mutex_lock (&data_mutex);
 *     while (!current_data)
 *       g_cond_wait (&data_cond, &data_mutex);
 *     data = current_data;
 *     current_data = NULL;
 *     g_mutex_unlock (&data_mutex);
 *
 *     return data;
 *   }
 * ]|
 * Whenever a thread calls pop_data() now, it will wait until
 * current_data is non-%NULL, i.e. until some other thread
 * has called push_data().
 *
 * The example shows that use of a condition variable must always be
 * paired with a mutex.  Without the use of a mutex, there would be a
 * race between the check of @current_data by the while loop in
 * pop_data() and waiting. Specifically, another thread could set
 * @current_data after the check, and signal the cond (with nobody
 * waiting on it) before the first thread goes to sleep. #GCond is
 * specifically useful for its ability to release the mutex and go
 * to sleep atomically.
 *
 * It is also important to use the g_cond_wait() and g_cond_wait_until()
 * functions only inside a loop which checks for the condition to be
 * true.  See g_cond_wait() for an explanation of why the condition may
 * not be true even after it returns.
 *
 * If a #GCond is allocated in static storage then it can be used
 * without initialisation.  Otherwise, you should call g_cond_init()
 * on it and g_cond_clear() when done.
 *
 * A #GCond should only be accessed via the g_cond_ functions.
 */

/* GThread Documentation {{{1 ---------------------------------------- */

/**
 * GThread:
 *
 * The #GThread struct represents a running thread. This struct
 * is returned by g_thread_new() or g_thread_try_new(). You can
 * obtain the #GThread struct representing the current thread by
 * calling g_thread_self().
 *
 * GThread is refcounted, see g_thread_ref() and g_thread_unref().
 * The thread represented by it holds a reference while it is running,
 * and g_thread_join() consumes the reference that it is given, so
 * it is normally not necessary to manage GThread references
 * explicitly.
 *
 * The structure is opaque -- none of its fields may be directly
 * accessed.
 */

/**
 * GThreadFunc:
 * @data: data passed to the thread
 *
 * Specifies the type of the @func functions passed to g_thread_new()
 * or g_thread_try_new().
 *
 * Returns: the return value of the thread
 */

/**
 * g_thread_supported:
 *
 * This macro returns %TRUE if the thread system is initialized,
 * and %FALSE if it is not.
 *
 * For language bindings, g_thread_get_initialized() provides
 * the same functionality as a function.
 *
 * Returns: %TRUE, if the thread system is initialized
 */

/* GThreadError {{{1 ------------------------------------------------------- */
/**
 * GThreadError:
 * @G_THREAD_ERROR_AGAIN: a thread couldn't be created due to resource
 *                        shortage. Try again later.
 *
 * Possible errors of thread related functions.
 **/

/**
 * G_THREAD_ERROR:
 *
 * The error domain of the GLib thread subsystem.
 **/
G_DEFINE_QUARK (g_thread_error, g_thread_error)

/* Local Data {{{1 -------------------------------------------------------- */

static GMutex    g_once_mutex;
static GCond     g_once_cond;
static GSList   *g_once_init_list = NULL;

static void g_thread_cleanup (gpointer data);
static GPrivate     g_thread_specific_private = G_PRIVATE_INIT (g_thread_cleanup);

G_LOCK_DEFINE_STATIC (g_thread_new);

/* GOnce {{{1 ------------------------------------------------------------- */

/**
 * GOnce:
 * @status: the status of the #GOnce
 * @retval: the value returned by the call to the function, if @status
 *          is %G_ONCE_STATUS_READY
 *
 * A #GOnce struct controls a one-time initialization function. Any
 * one-time initialization function must have its own unique #GOnce
 * struct.
 *
 * Since: 2.4
 */

/**
 * G_ONCE_INIT:
 *
 * A #GOnce must be initialized with this macro before it can be used.
 *
 * |[<!-- language="C" --> 
 *   GOnce my_once = G_ONCE_INIT;
 * ]|
 *
 * Since: 2.4
 */

/**
 * GOnceStatus:
 * @G_ONCE_STATUS_NOTCALLED: the function has not been called yet.
 * @G_ONCE_STATUS_PROGRESS: the function call is currently in progress.
 * @G_ONCE_STATUS_READY: the function has been called.
 *
 * The possible statuses of a one-time initialization function
 * controlled by a #GOnce struct.
 *
 * Since: 2.4
 */

/**
 * g_once:
 * @once: a #GOnce structure
 * @func: the #GThreadFunc function associated to @once. This function
 *        is called only once, regardless of the number of times it and
 *        its associated #GOnce struct are passed to g_once().
 * @arg: data to be passed to @func
 *
 * The first call to this routine by a process with a given #GOnce
 * struct calls @func with the given argument. Thereafter, subsequent
 * calls to g_once()  with the same #GOnce struct do not call @func
 * again, but return the stored result of the first call. On return
 * from g_once(), the status of @once will be %G_ONCE_STATUS_READY.
 *
 * For example, a mutex or a thread-specific data key must be created
 * exactly once. In a threaded environment, calling g_once() ensures
 * that the initialization is serialized across multiple threads.
 *
 * Calling g_once() recursively on the same #GOnce struct in
 * @func will lead to a deadlock.
 *
 * |[<!-- language="C" --> 
 *   gpointer
 *   get_debug_flags (void)
 *   {
 *     static GOnce my_once = G_ONCE_INIT;
 *
 *     g_once (&my_once, parse_debug_flags, NULL);
 *
 *     return my_once.retval;
 *   }
 * ]|
 *
 * Since: 2.4
 */
gpointer
g_once_impl (GOnce       *once,
	     GThreadFunc  func,
	     gpointer     arg)
{
  g_mutex_lock (&g_once_mutex);

  while (once->status == G_ONCE_STATUS_PROGRESS)
    g_cond_wait (&g_once_cond, &g_once_mutex);

  if (once->status != G_ONCE_STATUS_READY)
    {
      once->status = G_ONCE_STATUS_PROGRESS;
      g_mutex_unlock (&g_once_mutex);

      once->retval = func (arg);

      g_mutex_lock (&g_once_mutex);
      once->status = G_ONCE_STATUS_READY;
      g_cond_broadcast (&g_once_cond);
    }

  g_mutex_unlock (&g_once_mutex);

  return once->retval;
}

/**
 * g_once_init_enter:
 * @location: (not nullable): location of a static initializable variable
 *    containing 0
 *
 * Function to be called when starting a critical initialization
 * section. The argument @location must point to a static
 * 0-initialized variable that will be set to a value other than 0 at
 * the end of the initialization section. In combination with
 * g_once_init_leave() and the unique address @value_location, it can
 * be ensured that an initialization section will be executed only once
 * during a program's life time, and that concurrent threads are
 * blocked until initialization completed. To be used in constructs
 * like this:
 *
 * |[<!-- language="C" --> 
 *   static gsize initialization_value = 0;
 *
 *   if (g_once_init_enter (&initialization_value))
 *     {
 *       gsize setup_value = 42; // initialization code here
 *
 *       g_once_init_leave (&initialization_value, setup_value);
 *     }
 *
 *   // use initialization_value here
 * ]|
 *
 * Returns: %TRUE if the initialization section should be entered,
 *     %FALSE and blocks otherwise
 *
 * Since: 2.14
 */
gboolean
(g_once_init_enter) (volatile void *location)
{
  volatile gsize *value_location = location;
  gboolean need_init = FALSE;
  g_mutex_lock (&g_once_mutex);
  if (g_atomic_pointer_get (value_location) == NULL)
    {
      if (!g_slist_find (g_once_init_list, (void*) value_location))
        {
          need_init = TRUE;
          g_once_init_list = g_slist_prepend (g_once_init_list, (void*) value_location);
        }
      else
        do
          g_cond_wait (&g_once_cond, &g_once_mutex);
        while (g_slist_find (g_once_init_list, (void*) value_location));
    }
  g_mutex_unlock (&g_once_mutex);
  return need_init;
}

/**
 * g_once_init_leave:
 * @location: (not nullable): location of a static initializable variable
 *    containing 0
 * @result: new non-0 value for *@value_location
 *
 * Counterpart to g_once_init_enter(). Expects a location of a static
 * 0-initialized initialization variable, and an initialization value
 * other than 0. Sets the variable to the initialization value, and
 * releases concurrent threads blocking in g_once_init_enter() on this
 * initialization variable.
 *
 * Since: 2.14
 */
void
(g_once_init_leave) (volatile void *location,
                     gsize          result)
{
  volatile gsize *value_location = location;

  g_return_if_fail (g_atomic_pointer_get (value_location) == NULL);
  g_return_if_fail (result != 0);
  g_return_if_fail (g_once_init_list != NULL);

  g_atomic_pointer_set (value_location, result);
  g_mutex_lock (&g_once_mutex);
  g_once_init_list = g_slist_remove (g_once_init_list, (void*) value_location);
  g_cond_broadcast (&g_once_cond);
  g_mutex_unlock (&g_once_mutex);
}

/* GThread {{{1 -------------------------------------------------------- */

/**
 * g_thread_ref:
 * @thread: a #GThread
 *
 * Increase the reference count on @thread.
 *
 * Returns: a new reference to @thread
 *
 * Since: 2.32
 */
GThread *
g_thread_ref (GThread *thread)
{
  GRealThread *real = (GRealThread *) thread;

  g_atomic_int_inc (&real->ref_count);

  return thread;
}

/**
 * g_thread_unref:
 * @thread: a #GThread
 *
 * Decrease the reference count on @thread, possibly freeing all
 * resources associated with it.
 *
 * Note that each thread holds a reference to its #GThread while
 * it is running, so it is safe to drop your own reference to it
 * if you don't need it anymore.
 *
 * Since: 2.32
 */
void
g_thread_unref (GThread *thread)
{
  GRealThread *real = (GRealThread *) thread;

  if (g_atomic_int_dec_and_test (&real->ref_count))
    {
      if (real->ours)
        g_system_thread_free (real);
      else
        g_slice_free (GRealThread, real);
    }
}

static void
g_thread_cleanup (gpointer data)
{
  g_thread_unref (data);
}

gpointer
g_thread_proxy (gpointer data)
{
  GRealThread* thread = data;

  g_assert (data);

  /* This has to happen before G_LOCK, as that might call g_thread_self */
  g_private_set (&g_thread_specific_private, data);

  /* The lock makes sure that g_thread_new_internal() has a chance to
   * setup 'func' and 'data' before we make the call.
   */
  G_LOCK (g_thread_new);
  G_UNLOCK (g_thread_new);

  TRACE (GLIB_THREAD_SPAWNED (thread->thread.func, thread->thread.data,
                              thread->name));

  if (thread->name)
    {
      g_system_thread_set_name (thread->name);
      g_free (thread->name);
      thread->name = NULL;
    }

  thread->retval = thread->thread.func (thread->thread.data);

  return NULL;
}

/**
 * g_thread_new:
 * @name: (nullable): an (optional) name for the new thread
 * @func: a function to execute in the new thread
 * @data: an argument to supply to the new thread
 *
 * This function creates a new thread. The new thread starts by invoking
 * @func with the argument data. The thread will run until @func returns
 * or until g_thread_exit() is called from the new thread. The return value
 * of @func becomes the return value of the thread, which can be obtained
 * with g_thread_join().
 *
 * The @name can be useful for discriminating threads in a debugger.
 * It is not used for other purposes and does not have to be unique.
 * Some systems restrict the length of @name to 16 bytes.
 *
 * If the thread can not be created the program aborts. See
 * g_thread_try_new() if you want to attempt to deal with failures.
 *
 * If you are using threads to offload (potentially many) short-lived tasks,
 * #GThreadPool may be more appropriate than manually spawning and tracking
 * multiple #GThreads.
 *
 * To free the struct returned by this function, use g_thread_unref().
 * Note that g_thread_join() implicitly unrefs the #GThread as well.
 *
 * Returns: the new #GThread
 *
 * Since: 2.32
 */
GThread *
g_thread_new (const gchar *name,
              GThreadFunc  func,
              gpointer     data)
{
  GError *error = NULL;
  GThread *thread;

  thread = g_thread_new_internal (name, g_thread_proxy, func, data, 0, &error);

  if G_UNLIKELY (thread == NULL)
    g_error ("creating thread '%s': %s", name ? name : "", error->message);

  return thread;
}

/**
 * g_thread_try_new:
 * @name: (nullable): an (optional) name for the new thread
 * @func: a function to execute in the new thread
 * @data: an argument to supply to the new thread
 * @error: return location for error, or %NULL
 *
 * This function is the same as g_thread_new() except that
 * it allows for the possibility of failure.
 *
 * If a thread can not be created (due to resource limits),
 * @error is set and %NULL is returned.
 *
 * Returns: the new #GThread, or %NULL if an error occurred
 *
 * Since: 2.32
 */
GThread *
g_thread_try_new (const gchar  *name,
                  GThreadFunc   func,
                  gpointer      data,
                  GError      **error)
{
  return g_thread_new_internal (name, g_thread_proxy, func, data, 0, error);
}

GThread *
g_thread_new_internal (const gchar   *name,
                       GThreadFunc    proxy,
                       GThreadFunc    func,
                       gpointer       data,
                       gsize          stack_size,
                       GError       **error)
{
  GRealThread *thread;

  g_return_val_if_fail (func != NULL, NULL);

  G_LOCK (g_thread_new);
  thread = g_system_thread_new (proxy, stack_size, error);
  if (thread)
    {
      thread->ref_count = 2;
      thread->ours = TRUE;
      thread->thread.joinable = TRUE;
      thread->thread.func = func;
      thread->thread.data = data;
      thread->name = g_strdup (name);
    }
  G_UNLOCK (g_thread_new);

  return (GThread*) thread;
}

/**
 * g_thread_exit:
 * @retval: the return value of this thread
 *
 * Terminates the current thread.
 *
 * If another thread is waiting for us using g_thread_join() then the
 * waiting thread will be woken up and get @retval as the return value
 * of g_thread_join().
 *
 * Calling g_thread_exit() with a parameter @retval is equivalent to
 * returning @retval from the function @func, as given to g_thread_new().
 *
 * You must only call g_thread_exit() from a thread that you created
 * yourself with g_thread_new() or related APIs. You must not call
 * this function from a thread created with another threading library
 * or or from within a #GThreadPool.
 */
void
g_thread_exit (gpointer retval)
{
  GRealThread* real = (GRealThread*) g_thread_self ();

  if G_UNLIKELY (!real->ours)
    g_error ("attempt to g_thread_exit() a thread not created by GLib");

  real->retval = retval;

  g_system_thread_exit ();
}

/**
 * g_thread_join:
 * @thread: a #GThread
 *
 * Waits until @thread finishes, i.e. the function @func, as
 * given to g_thread_new(), returns or g_thread_exit() is called.
 * If @thread has already terminated, then g_thread_join()
 * returns immediately.
 *
 * Any thread can wait for any other thread by calling g_thread_join(),
 * not just its 'creator'. Calling g_thread_join() from multiple threads
 * for the same @thread leads to undefined behaviour.
 *
 * The value returned by @func or given to g_thread_exit() is
 * returned by this function.
 *
 * g_thread_join() consumes the reference to the passed-in @thread.
 * This will usually cause the #GThread struct and associated resources
 * to be freed. Use g_thread_ref() to obtain an extra reference if you
 * want to keep the GThread alive beyond the g_thread_join() call.
 *
 * Returns: the return value of the thread
 */
gpointer
g_thread_join (GThread *thread)
{
  GRealThread *real = (GRealThread*) thread;
  gpointer retval;

  g_return_val_if_fail (thread, NULL);
  g_return_val_if_fail (real->ours, NULL);

  g_system_thread_wait (real);

  retval = real->retval;

  /* Just to make sure, this isn't used any more */
  thread->joinable = 0;

  g_thread_unref (thread);

  return retval;
}

/**
 * g_thread_self:
 *
 * This function returns the #GThread corresponding to the
 * current thread. Note that this function does not increase
 * the reference count of the returned struct.
 *
 * This function will return a #GThread even for threads that
 * were not created by GLib (i.e. those created by other threading
 * APIs). This may be useful for thread identification purposes
 * (i.e. comparisons) but you must not use GLib functions (such
 * as g_thread_join()) on these threads.
 *
 * Returns: the #GThread representing the current thread
 */
GThread*
g_thread_self (void)
{
  GRealThread* thread = g_private_get (&g_thread_specific_private);

  if (!thread)
    {
      /* If no thread data is available, provide and set one.
       * This can happen for the main thread and for threads
       * that are not created by GLib.
       */
      thread = g_slice_new0 (GRealThread);
      thread->ref_count = 1;

      g_private_set (&g_thread_specific_private, thread);
    }

  return (GThread*) thread;
}

/**
 * g_get_num_processors:
 *
 * Determine the approximate number of threads that the system will
 * schedule simultaneously for this process.  This is intended to be
 * used as a parameter to g_thread_pool_new() for CPU bound tasks and
 * similar cases.
 *
 * Returns: Number of schedulable threads, always greater than 0
 *
 * Since: 2.36
 */
guint
g_get_num_processors (void)
{
#ifdef G_OS_WIN32
  unsigned int count;
  SYSTEM_INFO sysinfo;
  DWORD_PTR process_cpus;
  DWORD_PTR system_cpus;

  /* This *never* fails, use it as fallback */
  GetNativeSystemInfo (&sysinfo);
  count = (int) sysinfo.dwNumberOfProcessors;

  if (GetProcessAffinityMask (GetCurrentProcess (),
                              &process_cpus, &system_cpus))
    {
      unsigned int af_count;

      for (af_count = 0; process_cpus != 0; process_cpus >>= 1)
        if (process_cpus & 1)
          af_count++;

      /* Prefer affinity-based result, if available */
      if (af_count > 0)
        count = af_count;
    }

  if (count > 0)
    return count;
#elif defined(_SC_NPROCESSORS_ONLN)
  {
    int count;

    count = sysconf (_SC_NPROCESSORS_ONLN);
    if (count > 0)
      return count;
  }
#elif defined HW_NCPU
  {
    int mib[2], count = 0;
    size_t len;

    mib[0] = CTL_HW;
    mib[1] = HW_NCPU;
    len = sizeof(count);

    if (sysctl (mib, 2, &count, &len, NULL, 0) == 0 && count > 0)
      return count;
  }
#endif

  return 1; /* Fallback */
}

/* Epilogue {{{1 */
/* vim: set foldmethod=marker: */
