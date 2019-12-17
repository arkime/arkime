/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * gmain.c: Main loop abstraction, timeouts, and idle functions
 * Copyright 1998 Owen Taylor
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

/*
 * Modified by the GLib Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GLib Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GLib at ftp://ftp.gtk.org/pub/gtk/.
 */

/*
 * MT safe
 */

#include "config.h"
#include "glibconfig.h"
#include "glib_trace.h"

/* Uncomment the next line (and the corresponding line in gpoll.c) to
 * enable debugging printouts if the environment variable
 * G_MAIN_POLL_DEBUG is set to some value.
 */
/* #define G_MAIN_POLL_DEBUG */

#ifdef _WIN32
/* Always enable debugging printout on Windows, as it is more often
 * needed there...
 */
#define G_MAIN_POLL_DEBUG
#endif

#ifdef G_OS_UNIX
#include "glib-unix.h"
#include <pthread.h>
#ifdef HAVE_EVENTFD
#include <sys/eventfd.h>
#endif
#endif

#include <signal.h>
#include <sys/types.h>
#include <time.h>
#include <stdlib.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif /* HAVE_SYS_TIME_H */
#ifdef G_OS_UNIX
#include <unistd.h>
#endif /* G_OS_UNIX */
#include <errno.h>
#include <string.h>

#ifdef G_OS_WIN32
#define STRICT
#include <windows.h>
#endif /* G_OS_WIN32 */

#ifdef HAVE_MACH_MACH_TIME_H
#include <mach/mach_time.h>
#endif

#include "glib_trace.h"

#include "gmain.h"

#include "garray.h"
#include "giochannel.h"
#include "ghash.h"
#include "ghook.h"
#include "gqueue.h"
#include "gstrfuncs.h"
#include "gtestutils.h"

#ifdef G_OS_WIN32
#include "gwin32.h"
#endif

#ifdef  G_MAIN_POLL_DEBUG
#include "gtimer.h"
#endif

#include "gwakeup.h"
#include "gmain-internal.h"
#include "glib-init.h"
#include "glib-private.h"

/**
 * SECTION:main
 * @title: The Main Event Loop
 * @short_description: manages all available sources of events
 *
 * The main event loop manages all the available sources of events for
 * GLib and GTK+ applications. These events can come from any number of
 * different types of sources such as file descriptors (plain files,
 * pipes or sockets) and timeouts. New types of event sources can also
 * be added using g_source_attach().
 *
 * To allow multiple independent sets of sources to be handled in
 * different threads, each source is associated with a #GMainContext.
 * A GMainContext can only be running in a single thread, but
 * sources can be added to it and removed from it from other threads.
 *
 * Each event source is assigned a priority. The default priority,
 * #G_PRIORITY_DEFAULT, is 0. Values less than 0 denote higher priorities.
 * Values greater than 0 denote lower priorities. Events from high priority
 * sources are always processed before events from lower priority sources.
 *
 * Idle functions can also be added, and assigned a priority. These will
 * be run whenever no events with a higher priority are ready to be processed.
 *
 * The #GMainLoop data type represents a main event loop. A GMainLoop is
 * created with g_main_loop_new(). After adding the initial event sources,
 * g_main_loop_run() is called. This continuously checks for new events from
 * each of the event sources and dispatches them. Finally, the processing of
 * an event from one of the sources leads to a call to g_main_loop_quit() to
 * exit the main loop, and g_main_loop_run() returns.
 *
 * It is possible to create new instances of #GMainLoop recursively.
 * This is often used in GTK+ applications when showing modal dialog
 * boxes. Note that event sources are associated with a particular
 * #GMainContext, and will be checked and dispatched for all main
 * loops associated with that GMainContext.
 *
 * GTK+ contains wrappers of some of these functions, e.g. gtk_main(),
 * gtk_main_quit() and gtk_events_pending().
 *
 * ## Creating new source types
 *
 * One of the unusual features of the #GMainLoop functionality
 * is that new types of event source can be created and used in
 * addition to the builtin type of event source. A new event source
 * type is used for handling GDK events. A new source type is created
 * by "deriving" from the #GSource structure. The derived type of
 * source is represented by a structure that has the #GSource structure
 * as a first element, and other elements specific to the new source
 * type. To create an instance of the new source type, call
 * g_source_new() passing in the size of the derived structure and
 * a table of functions. These #GSourceFuncs determine the behavior of
 * the new source type.
 *
 * New source types basically interact with the main context
 * in two ways. Their prepare function in #GSourceFuncs can set a timeout
 * to determine the maximum amount of time that the main loop will sleep
 * before checking the source again. In addition, or as well, the source
 * can add file descriptors to the set that the main context checks using
 * g_source_add_poll().
 *
 * ## Customizing the main loop iteration
 *
 * Single iterations of a #GMainContext can be run with
 * g_main_context_iteration(). In some cases, more detailed control
 * of exactly how the details of the main loop work is desired, for
 * instance, when integrating the #GMainLoop with an external main loop.
 * In such cases, you can call the component functions of
 * g_main_context_iteration() directly. These functions are
 * g_main_context_prepare(), g_main_context_query(),
 * g_main_context_check() and g_main_context_dispatch().
 *
 * ## State of a Main Context # {#mainloop-states}
 *
 * The operation of these functions can best be seen in terms
 * of a state diagram, as shown in this image.
 *
 * ![](mainloop-states.gif)
 *
 * On UNIX, the GLib mainloop is incompatible with fork(). Any program
 * using the mainloop must either exec() or exit() from the child
 * without returning to the mainloop.
 *
 * ## Memory management of sources # {#mainloop-memory-management}
 *
 * There are two options for memory management of the user data passed to a
 * #GSource to be passed to its callback on invocation. This data is provided
 * in calls to g_timeout_add(), g_timeout_add_full(), g_idle_add(), etc. and
 * more generally, using g_source_set_callback(). This data is typically an
 * object which ‘owns’ the timeout or idle callback, such as a widget or a
 * network protocol implementation. In many cases, it is an error for the
 * callback to be invoked after this owning object has been destroyed, as that
 * results in use of freed memory.
 *
 * The first, and preferred, option is to store the source ID returned by
 * functions such as g_timeout_add() or g_source_attach(), and explicitly
 * remove that source from the main context using g_source_remove() when the
 * owning object is finalized. This ensures that the callback can only be
 * invoked while the object is still alive.
 *
 * The second option is to hold a strong reference to the object in the
 * callback, and to release it in the callback’s #GDestroyNotify. This ensures
 * that the object is kept alive until after the source is finalized, which is
 * guaranteed to be after it is invoked for the final time. The #GDestroyNotify
 * is another callback passed to the ‘full’ variants of #GSource functions (for
 * example, g_timeout_add_full()). It is called when the source is finalized,
 * and is designed for releasing references like this.
 *
 * One important caveat of this second approach is that it will keep the object
 * alive indefinitely if the main loop is stopped before the #GSource is
 * invoked, which may be undesirable.
 */

/* Types */

typedef struct _GTimeoutSource GTimeoutSource;
typedef struct _GChildWatchSource GChildWatchSource;
typedef struct _GUnixSignalWatchSource GUnixSignalWatchSource;
typedef struct _GPollRec GPollRec;
typedef struct _GSourceCallback GSourceCallback;

typedef enum
{
  G_SOURCE_READY = 1 << G_HOOK_FLAG_USER_SHIFT,
  G_SOURCE_CAN_RECURSE = 1 << (G_HOOK_FLAG_USER_SHIFT + 1),
  G_SOURCE_BLOCKED = 1 << (G_HOOK_FLAG_USER_SHIFT + 2)
} GSourceFlags;

typedef struct _GSourceList GSourceList;

struct _GSourceList
{
  GSource *head, *tail;
  gint priority;
};

typedef struct _GMainWaiter GMainWaiter;

struct _GMainWaiter
{
  GCond *cond;
  GMutex *mutex;
};

typedef struct _GMainDispatch GMainDispatch;

struct _GMainDispatch
{
  gint depth;
  GSource *source;
};

#ifdef G_MAIN_POLL_DEBUG
gboolean _g_main_poll_debug = FALSE;
#endif

struct _GMainContext
{
  /* The following lock is used for both the list of sources
   * and the list of poll records
   */
  GMutex mutex;
  GCond cond;
  GThread *owner;
  guint owner_count;
  GSList *waiters;

  volatile gint ref_count;

  GHashTable *sources;              /* guint -> GSource */

  GPtrArray *pending_dispatches;
  gint timeout;			/* Timeout for current iteration */

  guint next_id;
  GList *source_lists;
  gint in_check_or_prepare;

  GPollRec *poll_records;
  guint n_poll_records;
  GPollFD *cached_poll_array;
  guint cached_poll_array_size;

  GWakeup *wakeup;

  GPollFD wake_up_rec;

/* Flag indicating whether the set of fd's changed during a poll */
  gboolean poll_changed;

  GPollFunc poll_func;

  gint64   time;
  gboolean time_is_fresh;
};

struct _GSourceCallback
{
  volatile gint ref_count;
  GSourceFunc func;
  gpointer    data;
  GDestroyNotify notify;
};

struct _GMainLoop
{
  GMainContext *context;
  gboolean is_running;
  volatile gint ref_count;
};

struct _GTimeoutSource
{
  GSource     source;
  guint       interval;
  gboolean    seconds;
};

struct _GChildWatchSource
{
  GSource     source;
  GPid        pid;
  gint        child_status;
#ifdef G_OS_WIN32
  GPollFD     poll;
#else /* G_OS_WIN32 */
  gboolean    child_exited;
#endif /* G_OS_WIN32 */
};

struct _GUnixSignalWatchSource
{
  GSource     source;
  int         signum;
  gboolean    pending;
};

struct _GPollRec
{
  GPollFD *fd;
  GPollRec *prev;
  GPollRec *next;
  gint priority;
};

struct _GSourcePrivate
{
  GSList *child_sources;
  GSource *parent_source;

  gint64 ready_time;

  /* This is currently only used on UNIX, but we always declare it (and
   * let it remain empty on Windows) to avoid #ifdef all over the place.
   */
  GSList *fds;
};

typedef struct _GSourceIter
{
  GMainContext *context;
  gboolean may_modify;
  GList *current_list;
  GSource *source;
} GSourceIter;

#define LOCK_CONTEXT(context) g_mutex_lock (&context->mutex)
#define UNLOCK_CONTEXT(context) g_mutex_unlock (&context->mutex)
#define G_THREAD_SELF g_thread_self ()

#define SOURCE_DESTROYED(source) (((source)->flags & G_HOOK_FLAG_ACTIVE) == 0)
#define SOURCE_BLOCKED(source) (((source)->flags & G_SOURCE_BLOCKED) != 0)

#define SOURCE_UNREF(source, context)                       \
   G_STMT_START {                                           \
    if ((source)->ref_count > 1)                            \
      (source)->ref_count--;                                \
    else                                                    \
      g_source_unref_internal ((source), (context), TRUE);  \
   } G_STMT_END


/* Forward declarations */

static void g_source_unref_internal             (GSource      *source,
						 GMainContext *context,
						 gboolean      have_lock);
static void g_source_destroy_internal           (GSource      *source,
						 GMainContext *context,
						 gboolean      have_lock);
static void g_source_set_priority_unlocked      (GSource      *source,
						 GMainContext *context,
						 gint          priority);
static void g_child_source_remove_internal      (GSource      *child_source,
                                                 GMainContext *context);

static void g_main_context_poll                 (GMainContext *context,
						 gint          timeout,
						 gint          priority,
						 GPollFD      *fds,
						 gint          n_fds);
static void g_main_context_add_poll_unlocked    (GMainContext *context,
						 gint          priority,
						 GPollFD      *fd);
static void g_main_context_remove_poll_unlocked (GMainContext *context,
						 GPollFD      *fd);

static void     g_source_iter_init  (GSourceIter   *iter,
				     GMainContext  *context,
				     gboolean       may_modify);
static gboolean g_source_iter_next  (GSourceIter   *iter,
				     GSource      **source);
static void     g_source_iter_clear (GSourceIter   *iter);

static gboolean g_timeout_dispatch (GSource     *source,
				    GSourceFunc  callback,
				    gpointer     user_data);
static gboolean g_child_watch_prepare  (GSource     *source,
				        gint        *timeout);
static gboolean g_child_watch_check    (GSource     *source);
static gboolean g_child_watch_dispatch (GSource     *source,
					GSourceFunc  callback,
					gpointer     user_data);
static void     g_child_watch_finalize (GSource     *source);
#ifdef G_OS_UNIX
static void g_unix_signal_handler (int signum);
static gboolean g_unix_signal_watch_prepare  (GSource     *source,
					      gint        *timeout);
static gboolean g_unix_signal_watch_check    (GSource     *source);
static gboolean g_unix_signal_watch_dispatch (GSource     *source,
					      GSourceFunc  callback,
					      gpointer     user_data);
static void     g_unix_signal_watch_finalize  (GSource     *source);
#endif
static gboolean g_idle_prepare     (GSource     *source,
				    gint        *timeout);
static gboolean g_idle_check       (GSource     *source);
static gboolean g_idle_dispatch    (GSource     *source,
				    GSourceFunc  callback,
				    gpointer     user_data);

static void block_source (GSource *source);

static GMainContext *glib_worker_context;

G_LOCK_DEFINE_STATIC (main_loop);
static GMainContext *default_main_context;

#ifndef G_OS_WIN32


/* UNIX signals work by marking one of these variables then waking the
 * worker context to check on them and dispatch accordingly.
 */
#ifdef HAVE_SIG_ATOMIC_T
static volatile sig_atomic_t unix_signal_pending[NSIG];
static volatile sig_atomic_t any_unix_signal_pending;
#else
static volatile int unix_signal_pending[NSIG];
static volatile int any_unix_signal_pending;
#endif
static volatile guint unix_signal_refcount[NSIG];

/* Guards all the data below */
G_LOCK_DEFINE_STATIC (unix_signal_lock);
static GSList *unix_signal_watches;
static GSList *unix_child_watches;

GSourceFuncs g_unix_signal_funcs =
{
  g_unix_signal_watch_prepare,
  g_unix_signal_watch_check,
  g_unix_signal_watch_dispatch,
  g_unix_signal_watch_finalize
};
#endif /* !G_OS_WIN32 */
G_LOCK_DEFINE_STATIC (main_context_list);
static GSList *main_context_list = NULL;

GSourceFuncs g_timeout_funcs =
{
  NULL, /* prepare */
  NULL, /* check */
  g_timeout_dispatch,
  NULL
};

GSourceFuncs g_child_watch_funcs =
{
  g_child_watch_prepare,
  g_child_watch_check,
  g_child_watch_dispatch,
  g_child_watch_finalize
};

GSourceFuncs g_idle_funcs =
{
  g_idle_prepare,
  g_idle_check,
  g_idle_dispatch,
  NULL
};

/**
 * g_main_context_ref:
 * @context: a #GMainContext
 * 
 * Increases the reference count on a #GMainContext object by one.
 *
 * Returns: the @context that was passed in (since 2.6)
 **/
GMainContext *
g_main_context_ref (GMainContext *context)
{
  g_return_val_if_fail (context != NULL, NULL);
  g_return_val_if_fail (g_atomic_int_get (&context->ref_count) > 0, NULL); 

  g_atomic_int_inc (&context->ref_count);

  return context;
}

static inline void
poll_rec_list_free (GMainContext *context,
		    GPollRec     *list)
{
  g_slice_free_chain (GPollRec, list, next);
}

/**
 * g_main_context_unref:
 * @context: a #GMainContext
 * 
 * Decreases the reference count on a #GMainContext object by one. If
 * the result is zero, free the context and free all associated memory.
 **/
void
g_main_context_unref (GMainContext *context)
{
  GSourceIter iter;
  GSource *source;
  GList *sl_iter;
  GSourceList *list;
  guint i;

  g_return_if_fail (context != NULL);
  g_return_if_fail (g_atomic_int_get (&context->ref_count) > 0); 

  if (!g_atomic_int_dec_and_test (&context->ref_count))
    return;

  G_LOCK (main_context_list);
  main_context_list = g_slist_remove (main_context_list, context);
  G_UNLOCK (main_context_list);

  /* Free pending dispatches */
  for (i = 0; i < context->pending_dispatches->len; i++)
    g_source_unref_internal (context->pending_dispatches->pdata[i], context, FALSE);

  /* g_source_iter_next() assumes the context is locked. */
  LOCK_CONTEXT (context);
  g_source_iter_init (&iter, context, TRUE);
  while (g_source_iter_next (&iter, &source))
    {
      source->context = NULL;
      g_source_destroy_internal (source, context, TRUE);
    }
  UNLOCK_CONTEXT (context);

  for (sl_iter = context->source_lists; sl_iter; sl_iter = sl_iter->next)
    {
      list = sl_iter->data;
      g_slice_free (GSourceList, list);
    }
  g_list_free (context->source_lists);

  g_hash_table_destroy (context->sources);

  g_mutex_clear (&context->mutex);

  g_ptr_array_free (context->pending_dispatches, TRUE);
  g_free (context->cached_poll_array);

  poll_rec_list_free (context, context->poll_records);

  g_wakeup_free (context->wakeup);
  g_cond_clear (&context->cond);

  g_free (context);
}

/* Helper function used by mainloop/overflow test.
 */
GMainContext *
g_main_context_new_with_next_id (guint next_id)
{
  GMainContext *ret = g_main_context_new ();
  
  ret->next_id = next_id;
  
  return ret;
}

/**
 * g_main_context_new:
 * 
 * Creates a new #GMainContext structure.
 * 
 * Returns: the new #GMainContext
 **/
GMainContext *
g_main_context_new (void)
{
  static gsize initialised;
  GMainContext *context;

  if (g_once_init_enter (&initialised))
    {
#ifdef G_MAIN_POLL_DEBUG
      if (getenv ("G_MAIN_POLL_DEBUG") != NULL)
        _g_main_poll_debug = TRUE;
#endif

      g_once_init_leave (&initialised, TRUE);
    }

  context = g_new0 (GMainContext, 1);

  TRACE (GLIB_MAIN_CONTEXT_NEW (context));

  g_mutex_init (&context->mutex);
  g_cond_init (&context->cond);

  context->sources = g_hash_table_new (NULL, NULL);
  context->owner = NULL;
  context->waiters = NULL;

  context->ref_count = 1;

  context->next_id = 1;
  
  context->source_lists = NULL;
  
  context->poll_func = g_poll;
  
  context->cached_poll_array = NULL;
  context->cached_poll_array_size = 0;
  
  context->pending_dispatches = g_ptr_array_new ();
  
  context->time_is_fresh = FALSE;
  
  context->wakeup = g_wakeup_new ();
  g_wakeup_get_pollfd (context->wakeup, &context->wake_up_rec);
  g_main_context_add_poll_unlocked (context, 0, &context->wake_up_rec);

  G_LOCK (main_context_list);
  main_context_list = g_slist_append (main_context_list, context);

#ifdef G_MAIN_POLL_DEBUG
  if (_g_main_poll_debug)
    g_print ("created context=%p\n", context);
#endif

  G_UNLOCK (main_context_list);

  return context;
}

/**
 * g_main_context_default:
 * 
 * Returns the global default main context. This is the main context
 * used for main loop functions when a main loop is not explicitly
 * specified, and corresponds to the "main" main loop. See also
 * g_main_context_get_thread_default().
 * 
 * Returns: (transfer none): the global default main context.
 **/
GMainContext *
g_main_context_default (void)
{
  /* Slow, but safe */
  
  G_LOCK (main_loop);

  if (!default_main_context)
    {
      default_main_context = g_main_context_new ();

      TRACE (GLIB_MAIN_CONTEXT_DEFAULT (default_main_context));

#ifdef G_MAIN_POLL_DEBUG
      if (_g_main_poll_debug)
	g_print ("default context=%p\n", default_main_context);
#endif
    }

  G_UNLOCK (main_loop);

  return default_main_context;
}

static void
free_context (gpointer data)
{
  GMainContext *context = data;

  TRACE (GLIB_MAIN_CONTEXT_FREE (context));

  g_main_context_release (context);
  if (context)
    g_main_context_unref (context);
}

static void
free_context_stack (gpointer data)
{
  g_queue_free_full((GQueue *) data, (GDestroyNotify) free_context);
}

static GPrivate thread_context_stack = G_PRIVATE_INIT (free_context_stack);

/**
 * g_main_context_push_thread_default:
 * @context: (nullable): a #GMainContext, or %NULL for the global default context
 *
 * Acquires @context and sets it as the thread-default context for the
 * current thread. This will cause certain asynchronous operations
 * (such as most [gio][gio]-based I/O) which are
 * started in this thread to run under @context and deliver their
 * results to its main loop, rather than running under the global
 * default context in the main thread. Note that calling this function
 * changes the context returned by g_main_context_get_thread_default(),
 * not the one returned by g_main_context_default(), so it does not affect
 * the context used by functions like g_idle_add().
 *
 * Normally you would call this function shortly after creating a new
 * thread, passing it a #GMainContext which will be run by a
 * #GMainLoop in that thread, to set a new default context for all
 * async operations in that thread. In this case you may not need to
 * ever call g_main_context_pop_thread_default(), assuming you want the
 * new #GMainContext to be the default for the whole lifecycle of the
 * thread.
 *
 * If you don't have control over how the new thread was created (e.g.
 * in the new thread isn't newly created, or if the thread life
 * cycle is managed by a #GThreadPool), it is always suggested to wrap
 * the logic that needs to use the new #GMainContext inside a
 * g_main_context_push_thread_default() / g_main_context_pop_thread_default()
 * pair, otherwise threads that are re-used will end up never explicitly
 * releasing the #GMainContext reference they hold.
 *
 * In some cases you may want to schedule a single operation in a
 * non-default context, or temporarily use a non-default context in
 * the main thread. In that case, you can wrap the call to the
 * asynchronous operation inside a
 * g_main_context_push_thread_default() /
 * g_main_context_pop_thread_default() pair, but it is up to you to
 * ensure that no other asynchronous operations accidentally get
 * started while the non-default context is active.
 *
 * Beware that libraries that predate this function may not correctly
 * handle being used from a thread with a thread-default context. Eg,
 * see g_file_supports_thread_contexts().
 *
 * Since: 2.22
 **/
void
g_main_context_push_thread_default (GMainContext *context)
{
  GQueue *stack;
  gboolean acquired_context;

  acquired_context = g_main_context_acquire (context);
  g_return_if_fail (acquired_context);

  if (context == g_main_context_default ())
    context = NULL;
  else if (context)
    g_main_context_ref (context);

  stack = g_private_get (&thread_context_stack);
  if (!stack)
    {
      stack = g_queue_new ();
      g_private_set (&thread_context_stack, stack);
    }

  g_queue_push_head (stack, context);

  TRACE (GLIB_MAIN_CONTEXT_PUSH_THREAD_DEFAULT (context));
}

/**
 * g_main_context_pop_thread_default:
 * @context: (nullable): a #GMainContext object, or %NULL
 *
 * Pops @context off the thread-default context stack (verifying that
 * it was on the top of the stack).
 *
 * Since: 2.22
 **/
void
g_main_context_pop_thread_default (GMainContext *context)
{
  GQueue *stack;

  if (context == g_main_context_default ())
    context = NULL;

  stack = g_private_get (&thread_context_stack);

  g_return_if_fail (stack != NULL);
  g_return_if_fail (g_queue_peek_head (stack) == context);

  TRACE (GLIB_MAIN_CONTEXT_POP_THREAD_DEFAULT (context));

  g_queue_pop_head (stack);

  g_main_context_release (context);
  if (context)
    g_main_context_unref (context);
}

/**
 * g_main_context_get_thread_default:
 *
 * Gets the thread-default #GMainContext for this thread. Asynchronous
 * operations that want to be able to be run in contexts other than
 * the default one should call this method or
 * g_main_context_ref_thread_default() to get a #GMainContext to add
 * their #GSources to. (Note that even in single-threaded
 * programs applications may sometimes want to temporarily push a
 * non-default context, so it is not safe to assume that this will
 * always return %NULL if you are running in the default thread.)
 *
 * If you need to hold a reference on the context, use
 * g_main_context_ref_thread_default() instead.
 *
 * Returns: (transfer none): the thread-default #GMainContext, or
 * %NULL if the thread-default context is the global default context.
 *
 * Since: 2.22
 **/
GMainContext *
g_main_context_get_thread_default (void)
{
  GQueue *stack;

  stack = g_private_get (&thread_context_stack);
  if (stack)
    return g_queue_peek_head (stack);
  else
    return NULL;
}

/**
 * g_main_context_ref_thread_default:
 *
 * Gets the thread-default #GMainContext for this thread, as with
 * g_main_context_get_thread_default(), but also adds a reference to
 * it with g_main_context_ref(). In addition, unlike
 * g_main_context_get_thread_default(), if the thread-default context
 * is the global default context, this will return that #GMainContext
 * (with a ref added to it) rather than returning %NULL.
 *
 * Returns: (transfer full): the thread-default #GMainContext. Unref
 *     with g_main_context_unref() when you are done with it.
 *
 * Since: 2.32
 */
GMainContext *
g_main_context_ref_thread_default (void)
{
  GMainContext *context;

  context = g_main_context_get_thread_default ();
  if (!context)
    context = g_main_context_default ();
  return g_main_context_ref (context);
}

/* Hooks for adding to the main loop */

/**
 * g_source_new:
 * @source_funcs: structure containing functions that implement
 *                the sources behavior.
 * @struct_size: size of the #GSource structure to create.
 * 
 * Creates a new #GSource structure. The size is specified to
 * allow creating structures derived from #GSource that contain
 * additional data. The size passed in must be at least
 * `sizeof (GSource)`.
 * 
 * The source will not initially be associated with any #GMainContext
 * and must be added to one with g_source_attach() before it will be
 * executed.
 * 
 * Returns: the newly-created #GSource.
 **/
GSource *
g_source_new (GSourceFuncs *source_funcs,
	      guint         struct_size)
{
  GSource *source;

  g_return_val_if_fail (source_funcs != NULL, NULL);
  g_return_val_if_fail (struct_size >= sizeof (GSource), NULL);
  
  source = (GSource*) g_malloc0 (struct_size);
  source->priv = g_slice_new0 (GSourcePrivate);
  source->source_funcs = source_funcs;
  source->ref_count = 1;
  
  source->priority = G_PRIORITY_DEFAULT;

  source->flags = G_HOOK_FLAG_ACTIVE;

  source->priv->ready_time = -1;

  /* NULL/0 initialization for all other fields */

  TRACE (GLIB_SOURCE_NEW (source, source_funcs->prepare, source_funcs->check,
                          source_funcs->dispatch, source_funcs->finalize,
                          struct_size));

  return source;
}

/* Holds context's lock */
static void
g_source_iter_init (GSourceIter  *iter,
		    GMainContext *context,
		    gboolean      may_modify)
{
  iter->context = context;
  iter->current_list = NULL;
  iter->source = NULL;
  iter->may_modify = may_modify;
}

/* Holds context's lock */
static gboolean
g_source_iter_next (GSourceIter *iter, GSource **source)
{
  GSource *next_source;

  if (iter->source)
    next_source = iter->source->next;
  else
    next_source = NULL;

  if (!next_source)
    {
      if (iter->current_list)
	iter->current_list = iter->current_list->next;
      else
	iter->current_list = iter->context->source_lists;

      if (iter->current_list)
	{
	  GSourceList *source_list = iter->current_list->data;

	  next_source = source_list->head;
	}
    }

  /* Note: unreffing iter->source could potentially cause its
   * GSourceList to be removed from source_lists (if iter->source is
   * the only source in its list, and it is destroyed), so we have to
   * keep it reffed until after we advance iter->current_list, above.
   */

  if (iter->source && iter->may_modify)
    SOURCE_UNREF (iter->source, iter->context);
  iter->source = next_source;
  if (iter->source && iter->may_modify)
    iter->source->ref_count++;

  *source = iter->source;
  return *source != NULL;
}

/* Holds context's lock. Only necessary to call if you broke out of
 * the g_source_iter_next() loop early.
 */
static void
g_source_iter_clear (GSourceIter *iter)
{
  if (iter->source && iter->may_modify)
    {
      SOURCE_UNREF (iter->source, iter->context);
      iter->source = NULL;
    }
}

/* Holds context's lock
 */
static GSourceList *
find_source_list_for_priority (GMainContext *context,
			       gint          priority,
			       gboolean      create)
{
  GList *iter, *last;
  GSourceList *source_list;

  last = NULL;
  for (iter = context->source_lists; iter != NULL; last = iter, iter = iter->next)
    {
      source_list = iter->data;

      if (source_list->priority == priority)
	return source_list;

      if (source_list->priority > priority)
	{
	  if (!create)
	    return NULL;

	  source_list = g_slice_new0 (GSourceList);
	  source_list->priority = priority;
	  context->source_lists = g_list_insert_before (context->source_lists,
							iter,
							source_list);
	  return source_list;
	}
    }

  if (!create)
    return NULL;

  source_list = g_slice_new0 (GSourceList);
  source_list->priority = priority;

  if (!last)
    context->source_lists = g_list_append (NULL, source_list);
  else
    {
      /* This just appends source_list to the end of
       * context->source_lists without having to walk the list again.
       */
      last = g_list_append (last, source_list);
    }
  return source_list;
}

/* Holds context's lock
 */
static void
source_add_to_context (GSource      *source,
		       GMainContext *context)
{
  GSourceList *source_list;
  GSource *prev, *next;

  source_list = find_source_list_for_priority (context, source->priority, TRUE);

  if (source->priv->parent_source)
    {
      g_assert (source_list->head != NULL);

      /* Put the source immediately before its parent */
      prev = source->priv->parent_source->prev;
      next = source->priv->parent_source;
    }
  else
    {
      prev = source_list->tail;
      next = NULL;
    }

  source->next = next;
  if (next)
    next->prev = source;
  else
    source_list->tail = source;
  
  source->prev = prev;
  if (prev)
    prev->next = source;
  else
    source_list->head = source;
}

/* Holds context's lock
 */
static void
source_remove_from_context (GSource      *source,
			    GMainContext *context)
{
  GSourceList *source_list;

  source_list = find_source_list_for_priority (context, source->priority, FALSE);
  g_return_if_fail (source_list != NULL);

  if (source->prev)
    source->prev->next = source->next;
  else
    source_list->head = source->next;

  if (source->next)
    source->next->prev = source->prev;
  else
    source_list->tail = source->prev;

  source->prev = NULL;
  source->next = NULL;

  if (source_list->head == NULL)
    {
      context->source_lists = g_list_remove (context->source_lists, source_list);
      g_slice_free (GSourceList, source_list);
    }
}

static guint
g_source_attach_unlocked (GSource      *source,
                          GMainContext *context,
                          gboolean      do_wakeup)
{
  GSList *tmp_list;
  guint id;

  /* The counter may have wrapped, so we must ensure that we do not
   * reuse the source id of an existing source.
   */
  do
    id = context->next_id++;
  while (id == 0 || g_hash_table_contains (context->sources, GUINT_TO_POINTER (id)));

  source->context = context;
  source->source_id = id;
  source->ref_count++;

  g_hash_table_insert (context->sources, GUINT_TO_POINTER (id), source);

  source_add_to_context (source, context);

  if (!SOURCE_BLOCKED (source))
    {
      tmp_list = source->poll_fds;
      while (tmp_list)
        {
          g_main_context_add_poll_unlocked (context, source->priority, tmp_list->data);
          tmp_list = tmp_list->next;
        }

      for (tmp_list = source->priv->fds; tmp_list; tmp_list = tmp_list->next)
        g_main_context_add_poll_unlocked (context, source->priority, tmp_list->data);
    }

  tmp_list = source->priv->child_sources;
  while (tmp_list)
    {
      g_source_attach_unlocked (tmp_list->data, context, FALSE);
      tmp_list = tmp_list->next;
    }

  /* If another thread has acquired the context, wake it up since it
   * might be in poll() right now.
   */
  if (do_wakeup && context->owner && context->owner != G_THREAD_SELF)
    g_wakeup_signal (context->wakeup);

  return source->source_id;
}

/**
 * g_source_attach:
 * @source: a #GSource
 * @context: (nullable): a #GMainContext (if %NULL, the default context will be used)
 * 
 * Adds a #GSource to a @context so that it will be executed within
 * that context. Remove it by calling g_source_destroy().
 *
 * Returns: the ID (greater than 0) for the source within the 
 *   #GMainContext. 
 **/
guint
g_source_attach (GSource      *source,
		 GMainContext *context)
{
  guint result = 0;

  g_return_val_if_fail (source->context == NULL, 0);
  g_return_val_if_fail (!SOURCE_DESTROYED (source), 0);
  
  if (!context)
    context = g_main_context_default ();

  LOCK_CONTEXT (context);

  result = g_source_attach_unlocked (source, context, TRUE);

  TRACE (GLIB_MAIN_SOURCE_ATTACH (g_source_get_name (source), source, context,
                                  result));

  UNLOCK_CONTEXT (context);

  return result;
}

static void
g_source_destroy_internal (GSource      *source,
			   GMainContext *context,
			   gboolean      have_lock)
{
  TRACE (GLIB_MAIN_SOURCE_DESTROY (g_source_get_name (source), source,
                                   context));

  if (!have_lock)
    LOCK_CONTEXT (context);
  
  if (!SOURCE_DESTROYED (source))
    {
      GSList *tmp_list;
      gpointer old_cb_data;
      GSourceCallbackFuncs *old_cb_funcs;
      
      source->flags &= ~G_HOOK_FLAG_ACTIVE;

      old_cb_data = source->callback_data;
      old_cb_funcs = source->callback_funcs;

      source->callback_data = NULL;
      source->callback_funcs = NULL;

      if (old_cb_funcs)
	{
	  UNLOCK_CONTEXT (context);
	  old_cb_funcs->unref (old_cb_data);
	  LOCK_CONTEXT (context);
	}

      if (!SOURCE_BLOCKED (source))
	{
	  tmp_list = source->poll_fds;
	  while (tmp_list)
	    {
	      g_main_context_remove_poll_unlocked (context, tmp_list->data);
	      tmp_list = tmp_list->next;
	    }

          for (tmp_list = source->priv->fds; tmp_list; tmp_list = tmp_list->next)
            g_main_context_remove_poll_unlocked (context, tmp_list->data);
	}

      while (source->priv->child_sources)
        g_child_source_remove_internal (source->priv->child_sources->data, context);

      if (source->priv->parent_source)
        g_child_source_remove_internal (source, context);
	  
      g_source_unref_internal (source, context, TRUE);
    }

  if (!have_lock)
    UNLOCK_CONTEXT (context);
}

/**
 * g_source_destroy:
 * @source: a #GSource
 * 
 * Removes a source from its #GMainContext, if any, and mark it as
 * destroyed.  The source cannot be subsequently added to another
 * context. It is safe to call this on sources which have already been
 * removed from their context.
 **/
void
g_source_destroy (GSource *source)
{
  GMainContext *context;
  
  g_return_if_fail (source != NULL);
  
  context = source->context;
  
  if (context)
    g_source_destroy_internal (source, context, FALSE);
  else
    source->flags &= ~G_HOOK_FLAG_ACTIVE;
}

/**
 * g_source_get_id:
 * @source: a #GSource
 * 
 * Returns the numeric ID for a particular source. The ID of a source
 * is a positive integer which is unique within a particular main loop 
 * context. The reverse
 * mapping from ID to source is done by g_main_context_find_source_by_id().
 *
 * Returns: the ID (greater than 0) for the source
 **/
guint
g_source_get_id (GSource *source)
{
  guint result;
  
  g_return_val_if_fail (source != NULL, 0);
  g_return_val_if_fail (source->context != NULL, 0);

  LOCK_CONTEXT (source->context);
  result = source->source_id;
  UNLOCK_CONTEXT (source->context);
  
  return result;
}

/**
 * g_source_get_context:
 * @source: a #GSource
 * 
 * Gets the #GMainContext with which the source is associated.
 *
 * You can call this on a source that has been destroyed, provided
 * that the #GMainContext it was attached to still exists (in which
 * case it will return that #GMainContext). In particular, you can
 * always call this function on the source returned from
 * g_main_current_source(). But calling this function on a source
 * whose #GMainContext has been destroyed is an error.
 * 
 * Returns: (transfer none) (nullable): the #GMainContext with which the
 *               source is associated, or %NULL if the context has not
 *               yet been added to a source.
 **/
GMainContext *
g_source_get_context (GSource *source)
{
  g_return_val_if_fail (source->context != NULL || !SOURCE_DESTROYED (source), NULL);

  return source->context;
}

/**
 * g_source_add_poll:
 * @source:a #GSource 
 * @fd: a #GPollFD structure holding information about a file
 *      descriptor to watch.
 *
 * Adds a file descriptor to the set of file descriptors polled for
 * this source. This is usually combined with g_source_new() to add an
 * event source. The event source's check function will typically test
 * the @revents field in the #GPollFD struct and return %TRUE if events need
 * to be processed.
 *
 * This API is only intended to be used by implementations of #GSource.
 * Do not call this API on a #GSource that you did not create.
 *
 * Using this API forces the linear scanning of event sources on each
 * main loop iteration.  Newly-written event sources should try to use
 * g_source_add_unix_fd() instead of this API.
 **/
void
g_source_add_poll (GSource *source,
		   GPollFD *fd)
{
  GMainContext *context;
  
  g_return_if_fail (source != NULL);
  g_return_if_fail (fd != NULL);
  g_return_if_fail (!SOURCE_DESTROYED (source));
  
  context = source->context;

  if (context)
    LOCK_CONTEXT (context);
  
  source->poll_fds = g_slist_prepend (source->poll_fds, fd);

  if (context)
    {
      if (!SOURCE_BLOCKED (source))
	g_main_context_add_poll_unlocked (context, source->priority, fd);
      UNLOCK_CONTEXT (context);
    }
}

/**
 * g_source_remove_poll:
 * @source:a #GSource 
 * @fd: a #GPollFD structure previously passed to g_source_add_poll().
 * 
 * Removes a file descriptor from the set of file descriptors polled for
 * this source. 
 *
 * This API is only intended to be used by implementations of #GSource.
 * Do not call this API on a #GSource that you did not create.
 **/
void
g_source_remove_poll (GSource *source,
		      GPollFD *fd)
{
  GMainContext *context;
  
  g_return_if_fail (source != NULL);
  g_return_if_fail (fd != NULL);
  g_return_if_fail (!SOURCE_DESTROYED (source));
  
  context = source->context;

  if (context)
    LOCK_CONTEXT (context);
  
  source->poll_fds = g_slist_remove (source->poll_fds, fd);

  if (context)
    {
      if (!SOURCE_BLOCKED (source))
	g_main_context_remove_poll_unlocked (context, fd);
      UNLOCK_CONTEXT (context);
    }
}

/**
 * g_source_add_child_source:
 * @source:a #GSource
 * @child_source: a second #GSource that @source should "poll"
 *
 * Adds @child_source to @source as a "polled" source; when @source is
 * added to a #GMainContext, @child_source will be automatically added
 * with the same priority, when @child_source is triggered, it will
 * cause @source to dispatch (in addition to calling its own
 * callback), and when @source is destroyed, it will destroy
 * @child_source as well. (@source will also still be dispatched if
 * its own prepare/check functions indicate that it is ready.)
 *
 * If you don't need @child_source to do anything on its own when it
 * triggers, you can call g_source_set_dummy_callback() on it to set a
 * callback that does nothing (except return %TRUE if appropriate).
 *
 * @source will hold a reference on @child_source while @child_source
 * is attached to it.
 *
 * This API is only intended to be used by implementations of #GSource.
 * Do not call this API on a #GSource that you did not create.
 *
 * Since: 2.28
 **/
void
g_source_add_child_source (GSource *source,
			   GSource *child_source)
{
  GMainContext *context;

  g_return_if_fail (source != NULL);
  g_return_if_fail (child_source != NULL);
  g_return_if_fail (!SOURCE_DESTROYED (source));
  g_return_if_fail (!SOURCE_DESTROYED (child_source));
  g_return_if_fail (child_source->context == NULL);
  g_return_if_fail (child_source->priv->parent_source == NULL);

  context = source->context;

  if (context)
    LOCK_CONTEXT (context);

  TRACE (GLIB_SOURCE_ADD_CHILD_SOURCE (source, child_source));

  source->priv->child_sources = g_slist_prepend (source->priv->child_sources,
						 g_source_ref (child_source));
  child_source->priv->parent_source = source;
  g_source_set_priority_unlocked (child_source, NULL, source->priority);
  if (SOURCE_BLOCKED (source))
    block_source (child_source);

  if (context)
    {
      g_source_attach_unlocked (child_source, context, TRUE);
      UNLOCK_CONTEXT (context);
    }
}

static void
g_child_source_remove_internal (GSource *child_source,
                                GMainContext *context)
{
  GSource *parent_source = child_source->priv->parent_source;

  parent_source->priv->child_sources =
    g_slist_remove (parent_source->priv->child_sources, child_source);
  child_source->priv->parent_source = NULL;

  g_source_destroy_internal (child_source, context, TRUE);
  g_source_unref_internal (child_source, context, TRUE);
}

/**
 * g_source_remove_child_source:
 * @source:a #GSource
 * @child_source: a #GSource previously passed to
 *     g_source_add_child_source().
 *
 * Detaches @child_source from @source and destroys it.
 *
 * This API is only intended to be used by implementations of #GSource.
 * Do not call this API on a #GSource that you did not create.
 *
 * Since: 2.28
 **/
void
g_source_remove_child_source (GSource *source,
			      GSource *child_source)
{
  GMainContext *context;

  g_return_if_fail (source != NULL);
  g_return_if_fail (child_source != NULL);
  g_return_if_fail (child_source->priv->parent_source == source);
  g_return_if_fail (!SOURCE_DESTROYED (source));
  g_return_if_fail (!SOURCE_DESTROYED (child_source));

  context = source->context;

  if (context)
    LOCK_CONTEXT (context);

  g_child_source_remove_internal (child_source, context);

  if (context)
    UNLOCK_CONTEXT (context);
}

static void
g_source_callback_ref (gpointer cb_data)
{
  GSourceCallback *callback = cb_data;

  g_atomic_int_inc (&callback->ref_count);
}

static void
g_source_callback_unref (gpointer cb_data)
{
  GSourceCallback *callback = cb_data;

  if (g_atomic_int_dec_and_test (&callback->ref_count))
    {
      if (callback->notify)
        callback->notify (callback->data);
      g_free (callback);
    }
}

static void
g_source_callback_get (gpointer     cb_data,
		       GSource     *source, 
		       GSourceFunc *func,
		       gpointer    *data)
{
  GSourceCallback *callback = cb_data;

  *func = callback->func;
  *data = callback->data;
}

static GSourceCallbackFuncs g_source_callback_funcs = {
  g_source_callback_ref,
  g_source_callback_unref,
  g_source_callback_get,
};

/**
 * g_source_set_callback_indirect:
 * @source: the source
 * @callback_data: pointer to callback data "object"
 * @callback_funcs: functions for reference counting @callback_data
 *                  and getting the callback and data
 * 
 * Sets the callback function storing the data as a refcounted callback
 * "object". This is used internally. Note that calling 
 * g_source_set_callback_indirect() assumes
 * an initial reference count on @callback_data, and thus
 * @callback_funcs->unref will eventually be called once more
 * than @callback_funcs->ref.
 **/
void
g_source_set_callback_indirect (GSource              *source,
				gpointer              callback_data,
				GSourceCallbackFuncs *callback_funcs)
{
  GMainContext *context;
  gpointer old_cb_data;
  GSourceCallbackFuncs *old_cb_funcs;
  
  g_return_if_fail (source != NULL);
  g_return_if_fail (callback_funcs != NULL || callback_data == NULL);

  context = source->context;

  if (context)
    LOCK_CONTEXT (context);

  if (callback_funcs != &g_source_callback_funcs)
    TRACE (GLIB_SOURCE_SET_CALLBACK_INDIRECT (source, callback_data,
                                              callback_funcs->ref,
                                              callback_funcs->unref,
                                              callback_funcs->get));

  old_cb_data = source->callback_data;
  old_cb_funcs = source->callback_funcs;

  source->callback_data = callback_data;
  source->callback_funcs = callback_funcs;
  
  if (context)
    UNLOCK_CONTEXT (context);
  
  if (old_cb_funcs)
    old_cb_funcs->unref (old_cb_data);
}

/**
 * g_source_set_callback:
 * @source: the source
 * @func: a callback function
 * @data: the data to pass to callback function
 * @notify: (nullable): a function to call when @data is no longer in use, or %NULL.
 * 
 * Sets the callback function for a source. The callback for a source is
 * called from the source's dispatch function.
 *
 * The exact type of @func depends on the type of source; ie. you
 * should not count on @func being called with @data as its first
 * parameter.
 *
 * See [memory management of sources][mainloop-memory-management] for details
 * on how to handle memory management of @data.
 * 
 * Typically, you won't use this function. Instead use functions specific
 * to the type of source you are using.
 **/
void
g_source_set_callback (GSource        *source,
		       GSourceFunc     func,
		       gpointer        data,
		       GDestroyNotify  notify)
{
  GSourceCallback *new_callback;

  g_return_if_fail (source != NULL);

  TRACE (GLIB_SOURCE_SET_CALLBACK (source, func, data, notify));

  new_callback = g_new (GSourceCallback, 1);

  new_callback->ref_count = 1;
  new_callback->func = func;
  new_callback->data = data;
  new_callback->notify = notify;

  g_source_set_callback_indirect (source, new_callback, &g_source_callback_funcs);
}


/**
 * g_source_set_funcs:
 * @source: a #GSource
 * @funcs: the new #GSourceFuncs
 * 
 * Sets the source functions (can be used to override 
 * default implementations) of an unattached source.
 * 
 * Since: 2.12
 */
void
g_source_set_funcs (GSource     *source,
	           GSourceFuncs *funcs)
{
  g_return_if_fail (source != NULL);
  g_return_if_fail (source->context == NULL);
  g_return_if_fail (source->ref_count > 0);
  g_return_if_fail (funcs != NULL);

  source->source_funcs = funcs;
}

static void
g_source_set_priority_unlocked (GSource      *source,
				GMainContext *context,
				gint          priority)
{
  GSList *tmp_list;
  
  g_return_if_fail (source->priv->parent_source == NULL ||
		    source->priv->parent_source->priority == priority);

  TRACE (GLIB_SOURCE_SET_PRIORITY (source, context, priority));

  if (context)
    {
      /* Remove the source from the context's source and then
       * add it back after so it is sorted in the correct place
       */
      source_remove_from_context (source, source->context);
    }

  source->priority = priority;

  if (context)
    {
      source_add_to_context (source, source->context);

      if (!SOURCE_BLOCKED (source))
	{
	  tmp_list = source->poll_fds;
	  while (tmp_list)
	    {
	      g_main_context_remove_poll_unlocked (context, tmp_list->data);
	      g_main_context_add_poll_unlocked (context, priority, tmp_list->data);
	      
	      tmp_list = tmp_list->next;
	    }

          for (tmp_list = source->priv->fds; tmp_list; tmp_list = tmp_list->next)
            {
              g_main_context_remove_poll_unlocked (context, tmp_list->data);
              g_main_context_add_poll_unlocked (context, priority, tmp_list->data);
            }
	}
    }

  if (source->priv->child_sources)
    {
      tmp_list = source->priv->child_sources;
      while (tmp_list)
	{
	  g_source_set_priority_unlocked (tmp_list->data, context, priority);
	  tmp_list = tmp_list->next;
	}
    }
}

/**
 * g_source_set_priority:
 * @source: a #GSource
 * @priority: the new priority.
 *
 * Sets the priority of a source. While the main loop is being run, a
 * source will be dispatched if it is ready to be dispatched and no
 * sources at a higher (numerically smaller) priority are ready to be
 * dispatched.
 *
 * A child source always has the same priority as its parent.  It is not
 * permitted to change the priority of a source once it has been added
 * as a child of another source.
 **/
void
g_source_set_priority (GSource  *source,
		       gint      priority)
{
  GMainContext *context;

  g_return_if_fail (source != NULL);
  g_return_if_fail (source->priv->parent_source == NULL);

  context = source->context;

  if (context)
    LOCK_CONTEXT (context);
  g_source_set_priority_unlocked (source, context, priority);
  if (context)
    UNLOCK_CONTEXT (context);
}

/**
 * g_source_get_priority:
 * @source: a #GSource
 * 
 * Gets the priority of a source.
 * 
 * Returns: the priority of the source
 **/
gint
g_source_get_priority (GSource *source)
{
  g_return_val_if_fail (source != NULL, 0);

  return source->priority;
}

/**
 * g_source_set_ready_time:
 * @source: a #GSource
 * @ready_time: the monotonic time at which the source will be ready,
 *              0 for "immediately", -1 for "never"
 *
 * Sets a #GSource to be dispatched when the given monotonic time is
 * reached (or passed).  If the monotonic time is in the past (as it
 * always will be if @ready_time is 0) then the source will be
 * dispatched immediately.
 *
 * If @ready_time is -1 then the source is never woken up on the basis
 * of the passage of time.
 *
 * Dispatching the source does not reset the ready time.  You should do
 * so yourself, from the source dispatch function.
 *
 * Note that if you have a pair of sources where the ready time of one
 * suggests that it will be delivered first but the priority for the
 * other suggests that it would be delivered first, and the ready time
 * for both sources is reached during the same main context iteration,
 * then the order of dispatch is undefined.
 *
 * It is a no-op to call this function on a #GSource which has already been
 * destroyed with g_source_destroy().
 *
 * This API is only intended to be used by implementations of #GSource.
 * Do not call this API on a #GSource that you did not create.
 *
 * Since: 2.36
 **/
void
g_source_set_ready_time (GSource *source,
                         gint64   ready_time)
{
  GMainContext *context;

  g_return_if_fail (source != NULL);
  /* We deliberately don't check for ref_count > 0 here, because that
   * breaks cancellable_source_cancelled() in GCancellable: it has no
   * way to find out that the last-unref has happened until the
   * finalize() function is called, but that's too late, because the
   * ref_count already has already reached 0 before that time.
   * However, priv is only poisoned (set to NULL) after finalize(),
   * so we can use this as a simple guard against use-after-free.
   * See https://bugzilla.gnome.org/show_bug.cgi?id=791754 */
  g_return_if_fail (source->priv != NULL);

  context = source->context;

  if (context)
    LOCK_CONTEXT (context);

  if (source->priv->ready_time == ready_time)
    {
      if (context)
        UNLOCK_CONTEXT (context);

      return;
    }

  source->priv->ready_time = ready_time;

  TRACE (GLIB_SOURCE_SET_READY_TIME (source, ready_time));

  if (context)
    {
      /* Quite likely that we need to change the timeout on the poll */
      if (!SOURCE_BLOCKED (source))
        g_wakeup_signal (context->wakeup);
      UNLOCK_CONTEXT (context);
    }
}

/**
 * g_source_get_ready_time:
 * @source: a #GSource
 *
 * Gets the "ready time" of @source, as set by
 * g_source_set_ready_time().
 *
 * Any time before the current monotonic time (including 0) is an
 * indication that the source will fire immediately.
 *
 * Returns: the monotonic ready time, -1 for "never"
 **/
gint64
g_source_get_ready_time (GSource *source)
{
  g_return_val_if_fail (source != NULL, -1);

  return source->priv->ready_time;
}

/**
 * g_source_set_can_recurse:
 * @source: a #GSource
 * @can_recurse: whether recursion is allowed for this source
 * 
 * Sets whether a source can be called recursively. If @can_recurse is
 * %TRUE, then while the source is being dispatched then this source
 * will be processed normally. Otherwise, all processing of this
 * source is blocked until the dispatch function returns.
 **/
void
g_source_set_can_recurse (GSource  *source,
			  gboolean  can_recurse)
{
  GMainContext *context;
  
  g_return_if_fail (source != NULL);

  context = source->context;

  if (context)
    LOCK_CONTEXT (context);
  
  if (can_recurse)
    source->flags |= G_SOURCE_CAN_RECURSE;
  else
    source->flags &= ~G_SOURCE_CAN_RECURSE;

  if (context)
    UNLOCK_CONTEXT (context);
}

/**
 * g_source_get_can_recurse:
 * @source: a #GSource
 * 
 * Checks whether a source is allowed to be called recursively.
 * see g_source_set_can_recurse().
 * 
 * Returns: whether recursion is allowed.
 **/
gboolean
g_source_get_can_recurse (GSource  *source)
{
  g_return_val_if_fail (source != NULL, FALSE);
  
  return (source->flags & G_SOURCE_CAN_RECURSE) != 0;
}


/**
 * g_source_set_name:
 * @source: a #GSource
 * @name: debug name for the source
 *
 * Sets a name for the source, used in debugging and profiling.
 * The name defaults to #NULL.
 *
 * The source name should describe in a human-readable way
 * what the source does. For example, "X11 event queue"
 * or "GTK+ repaint idle handler" or whatever it is.
 *
 * It is permitted to call this function multiple times, but is not
 * recommended due to the potential performance impact.  For example,
 * one could change the name in the "check" function of a #GSourceFuncs
 * to include details like the event type in the source name.
 *
 * Use caution if changing the name while another thread may be
 * accessing it with g_source_get_name(); that function does not copy
 * the value, and changing the value will free it while the other thread
 * may be attempting to use it.
 *
 * Since: 2.26
 **/
void
g_source_set_name (GSource    *source,
                   const char *name)
{
  GMainContext *context;

  g_return_if_fail (source != NULL);

  context = source->context;

  if (context)
    LOCK_CONTEXT (context);

  TRACE (GLIB_SOURCE_SET_NAME (source, name));

  /* setting back to NULL is allowed, just because it's
   * weird if get_name can return NULL but you can't
   * set that.
   */

  g_free (source->name);
  source->name = g_strdup (name);

  if (context)
    UNLOCK_CONTEXT (context);
}

/**
 * g_source_get_name:
 * @source: a #GSource
 *
 * Gets a name for the source, used in debugging and profiling.  The
 * name may be #NULL if it has never been set with g_source_set_name().
 *
 * Returns: the name of the source
 *
 * Since: 2.26
 **/
const char *
g_source_get_name (GSource *source)
{
  g_return_val_if_fail (source != NULL, NULL);

  return source->name;
}

/**
 * g_source_set_name_by_id:
 * @tag: a #GSource ID
 * @name: debug name for the source
 *
 * Sets the name of a source using its ID.
 *
 * This is a convenience utility to set source names from the return
 * value of g_idle_add(), g_timeout_add(), etc.
 *
 * It is a programmer error to attempt to set the name of a non-existent
 * source.
 *
 * More specifically: source IDs can be reissued after a source has been
 * destroyed and therefore it is never valid to use this function with a
 * source ID which may have already been removed.  An example is when
 * scheduling an idle to run in another thread with g_idle_add(): the
 * idle may already have run and been removed by the time this function
 * is called on its (now invalid) source ID.  This source ID may have
 * been reissued, leading to the operation being performed against the
 * wrong source.
 *
 * Since: 2.26
 **/
void
g_source_set_name_by_id (guint           tag,
                         const char     *name)
{
  GSource *source;

  g_return_if_fail (tag > 0);

  source = g_main_context_find_source_by_id (NULL, tag);
  if (source == NULL)
    return;

  g_source_set_name (source, name);
}


/**
 * g_source_ref:
 * @source: a #GSource
 * 
 * Increases the reference count on a source by one.
 * 
 * Returns: @source
 **/
GSource *
g_source_ref (GSource *source)
{
  GMainContext *context;
  
  g_return_val_if_fail (source != NULL, NULL);

  context = source->context;

  if (context)
    LOCK_CONTEXT (context);

  source->ref_count++;

  if (context)
    UNLOCK_CONTEXT (context);

  return source;
}

/* g_source_unref() but possible to call within context lock
 */
static void
g_source_unref_internal (GSource      *source,
			 GMainContext *context,
			 gboolean      have_lock)
{
  gpointer old_cb_data = NULL;
  GSourceCallbackFuncs *old_cb_funcs = NULL;

  g_return_if_fail (source != NULL);
  
  if (!have_lock && context)
    LOCK_CONTEXT (context);

  source->ref_count--;
  if (source->ref_count == 0)
    {
      TRACE (GLIB_SOURCE_BEFORE_FREE (source, context,
                                      source->source_funcs->finalize));

      old_cb_data = source->callback_data;
      old_cb_funcs = source->callback_funcs;

      source->callback_data = NULL;
      source->callback_funcs = NULL;

      if (context)
	{
	  if (!SOURCE_DESTROYED (source))
	    g_warning (G_STRLOC ": ref_count == 0, but source was still attached to a context!");
	  source_remove_from_context (source, context);

          g_hash_table_remove (context->sources, GUINT_TO_POINTER (source->source_id));
	}

      if (source->source_funcs->finalize)
	{
          /* Temporarily increase the ref count again so that GSource methods
           * can be called from finalize(). */
          source->ref_count++;
	  if (context)
	    UNLOCK_CONTEXT (context);
	  source->source_funcs->finalize (source);
	  if (context)
	    LOCK_CONTEXT (context);
          source->ref_count--;
	}

      if (old_cb_funcs)
        {
          /* Temporarily increase the ref count again so that GSource methods
           * can be called from callback_funcs.unref(). */
          source->ref_count++;
          if (context)
            UNLOCK_CONTEXT (context);

          old_cb_funcs->unref (old_cb_data);

          if (context)
            LOCK_CONTEXT (context);
          source->ref_count--;
        }

      g_free (source->name);
      source->name = NULL;

      g_slist_free (source->poll_fds);
      source->poll_fds = NULL;

      g_slist_free_full (source->priv->fds, g_free);

      while (source->priv->child_sources)
        {
          GSource *child_source = source->priv->child_sources->data;

          source->priv->child_sources =
            g_slist_remove (source->priv->child_sources, child_source);
          child_source->priv->parent_source = NULL;

          g_source_unref_internal (child_source, context, have_lock);
        }

      g_slice_free (GSourcePrivate, source->priv);
      source->priv = NULL;

      g_free (source);
    }

  if (!have_lock && context)
    UNLOCK_CONTEXT (context);
}

/**
 * g_source_unref:
 * @source: a #GSource
 * 
 * Decreases the reference count of a source by one. If the
 * resulting reference count is zero the source and associated
 * memory will be destroyed. 
 **/
void
g_source_unref (GSource *source)
{
  g_return_if_fail (source != NULL);

  g_source_unref_internal (source, source->context, FALSE);
}

/**
 * g_main_context_find_source_by_id:
 * @context: (nullable): a #GMainContext (if %NULL, the default context will be used)
 * @source_id: the source ID, as returned by g_source_get_id().
 *
 * Finds a #GSource given a pair of context and ID.
 *
 * It is a programmer error to attempt to lookup a non-existent source.
 *
 * More specifically: source IDs can be reissued after a source has been
 * destroyed and therefore it is never valid to use this function with a
 * source ID which may have already been removed.  An example is when
 * scheduling an idle to run in another thread with g_idle_add(): the
 * idle may already have run and been removed by the time this function
 * is called on its (now invalid) source ID.  This source ID may have
 * been reissued, leading to the operation being performed against the
 * wrong source.
 *
 * Returns: (transfer none): the #GSource
 **/
GSource *
g_main_context_find_source_by_id (GMainContext *context,
                                  guint         source_id)
{
  GSource *source;

  g_return_val_if_fail (source_id > 0, NULL);

  if (context == NULL)
    context = g_main_context_default ();

  LOCK_CONTEXT (context);
  source = g_hash_table_lookup (context->sources, GUINT_TO_POINTER (source_id));
  UNLOCK_CONTEXT (context);

  if (source && SOURCE_DESTROYED (source))
    source = NULL;

  return source;
}

/**
 * g_main_context_find_source_by_funcs_user_data:
 * @context: (nullable): a #GMainContext (if %NULL, the default context will be used).
 * @funcs: the @source_funcs passed to g_source_new().
 * @user_data: the user data from the callback.
 * 
 * Finds a source with the given source functions and user data.  If
 * multiple sources exist with the same source function and user data,
 * the first one found will be returned.
 * 
 * Returns: (transfer none): the source, if one was found, otherwise %NULL
 **/
GSource *
g_main_context_find_source_by_funcs_user_data (GMainContext *context,
					       GSourceFuncs *funcs,
					       gpointer      user_data)
{
  GSourceIter iter;
  GSource *source;
  
  g_return_val_if_fail (funcs != NULL, NULL);

  if (context == NULL)
    context = g_main_context_default ();
  
  LOCK_CONTEXT (context);

  g_source_iter_init (&iter, context, FALSE);
  while (g_source_iter_next (&iter, &source))
    {
      if (!SOURCE_DESTROYED (source) &&
	  source->source_funcs == funcs &&
	  source->callback_funcs)
	{
	  GSourceFunc callback;
	  gpointer callback_data;

	  source->callback_funcs->get (source->callback_data, source, &callback, &callback_data);
	  
	  if (callback_data == user_data)
	    break;
	}
    }
  g_source_iter_clear (&iter);

  UNLOCK_CONTEXT (context);

  return source;
}

/**
 * g_main_context_find_source_by_user_data:
 * @context: a #GMainContext
 * @user_data: the user_data for the callback.
 * 
 * Finds a source with the given user data for the callback.  If
 * multiple sources exist with the same user data, the first
 * one found will be returned.
 * 
 * Returns: (transfer none): the source, if one was found, otherwise %NULL
 **/
GSource *
g_main_context_find_source_by_user_data (GMainContext *context,
					 gpointer      user_data)
{
  GSourceIter iter;
  GSource *source;
  
  if (context == NULL)
    context = g_main_context_default ();
  
  LOCK_CONTEXT (context);

  g_source_iter_init (&iter, context, FALSE);
  while (g_source_iter_next (&iter, &source))
    {
      if (!SOURCE_DESTROYED (source) &&
	  source->callback_funcs)
	{
	  GSourceFunc callback;
	  gpointer callback_data = NULL;

	  source->callback_funcs->get (source->callback_data, source, &callback, &callback_data);

	  if (callback_data == user_data)
	    break;
	}
    }
  g_source_iter_clear (&iter);

  UNLOCK_CONTEXT (context);

  return source;
}

/**
 * g_source_remove:
 * @tag: the ID of the source to remove.
 *
 * Removes the source with the given ID from the default main context. You must
 * use g_source_destroy() for sources added to a non-default main context.
 *
 * The ID of a #GSource is given by g_source_get_id(), or will be
 * returned by the functions g_source_attach(), g_idle_add(),
 * g_idle_add_full(), g_timeout_add(), g_timeout_add_full(),
 * g_child_watch_add(), g_child_watch_add_full(), g_io_add_watch(), and
 * g_io_add_watch_full().
 *
 * It is a programmer error to attempt to remove a non-existent source.
 *
 * More specifically: source IDs can be reissued after a source has been
 * destroyed and therefore it is never valid to use this function with a
 * source ID which may have already been removed.  An example is when
 * scheduling an idle to run in another thread with g_idle_add(): the
 * idle may already have run and been removed by the time this function
 * is called on its (now invalid) source ID.  This source ID may have
 * been reissued, leading to the operation being performed against the
 * wrong source.
 *
 * Returns: For historical reasons, this function always returns %TRUE
 **/
gboolean
g_source_remove (guint tag)
{
  GSource *source;

  g_return_val_if_fail (tag > 0, FALSE);

  source = g_main_context_find_source_by_id (NULL, tag);
  if (source)
    g_source_destroy (source);
  else
    g_critical ("Source ID %u was not found when attempting to remove it", tag);

  return source != NULL;
}

/**
 * g_source_remove_by_user_data:
 * @user_data: the user_data for the callback.
 * 
 * Removes a source from the default main loop context given the user
 * data for the callback. If multiple sources exist with the same user
 * data, only one will be destroyed.
 * 
 * Returns: %TRUE if a source was found and removed. 
 **/
gboolean
g_source_remove_by_user_data (gpointer user_data)
{
  GSource *source;
  
  source = g_main_context_find_source_by_user_data (NULL, user_data);
  if (source)
    {
      g_source_destroy (source);
      return TRUE;
    }
  else
    return FALSE;
}

/**
 * g_source_remove_by_funcs_user_data:
 * @funcs: The @source_funcs passed to g_source_new()
 * @user_data: the user data for the callback
 * 
 * Removes a source from the default main loop context given the
 * source functions and user data. If multiple sources exist with the
 * same source functions and user data, only one will be destroyed.
 * 
 * Returns: %TRUE if a source was found and removed. 
 **/
gboolean
g_source_remove_by_funcs_user_data (GSourceFuncs *funcs,
				    gpointer      user_data)
{
  GSource *source;

  g_return_val_if_fail (funcs != NULL, FALSE);

  source = g_main_context_find_source_by_funcs_user_data (NULL, funcs, user_data);
  if (source)
    {
      g_source_destroy (source);
      return TRUE;
    }
  else
    return FALSE;
}

/**
 * g_clear_handle_id: (skip)
 * @tag_ptr: (not nullable): a pointer to the handler ID
 * @clear_func: (not nullable): the function to call to clear the handler
 *
 * Clears a numeric handler, such as a #GSource ID.
 *
 * @tag_ptr must be a valid pointer to the variable holding the handler.
 *
 * If the ID is zero then this function does nothing.
 * Otherwise, clear_func() is called with the ID as a parameter, and the tag is
 * set to zero.
 *
 * A macro is also included that allows this function to be used without
 * pointer casts.
 *
 * Since: 2.56
 */
#undef g_clear_handle_id
void
g_clear_handle_id (guint            *tag_ptr,
                   GClearHandleFunc  clear_func)
{
  guint _handle_id;

  _handle_id = *tag_ptr;
  if (_handle_id > 0)
    {
      *tag_ptr = 0;
      clear_func (_handle_id);
    }
}

#ifdef G_OS_UNIX
/**
 * g_source_add_unix_fd:
 * @source: a #GSource
 * @fd: the fd to monitor
 * @events: an event mask
 *
 * Monitors @fd for the IO events in @events.
 *
 * The tag returned by this function can be used to remove or modify the
 * monitoring of the fd using g_source_remove_unix_fd() or
 * g_source_modify_unix_fd().
 *
 * It is not necessary to remove the fd before destroying the source; it
 * will be cleaned up automatically.
 *
 * This API is only intended to be used by implementations of #GSource.
 * Do not call this API on a #GSource that you did not create.
 *
 * As the name suggests, this function is not available on Windows.
 *
 * Returns: (not nullable): an opaque tag
 *
 * Since: 2.36
 **/
gpointer
g_source_add_unix_fd (GSource      *source,
                      gint          fd,
                      GIOCondition  events)
{
  GMainContext *context;
  GPollFD *poll_fd;

  g_return_val_if_fail (source != NULL, NULL);
  g_return_val_if_fail (!SOURCE_DESTROYED (source), NULL);

  poll_fd = g_new (GPollFD, 1);
  poll_fd->fd = fd;
  poll_fd->events = events;
  poll_fd->revents = 0;

  context = source->context;

  if (context)
    LOCK_CONTEXT (context);

  source->priv->fds = g_slist_prepend (source->priv->fds, poll_fd);

  if (context)
    {
      if (!SOURCE_BLOCKED (source))
        g_main_context_add_poll_unlocked (context, source->priority, poll_fd);
      UNLOCK_CONTEXT (context);
    }

  return poll_fd;
}

/**
 * g_source_modify_unix_fd:
 * @source: a #GSource
 * @tag: (not nullable): the tag from g_source_add_unix_fd()
 * @new_events: the new event mask to watch
 *
 * Updates the event mask to watch for the fd identified by @tag.
 *
 * @tag is the tag returned from g_source_add_unix_fd().
 *
 * If you want to remove a fd, don't set its event mask to zero.
 * Instead, call g_source_remove_unix_fd().
 *
 * This API is only intended to be used by implementations of #GSource.
 * Do not call this API on a #GSource that you did not create.
 *
 * As the name suggests, this function is not available on Windows.
 *
 * Since: 2.36
 **/
void
g_source_modify_unix_fd (GSource      *source,
                         gpointer      tag,
                         GIOCondition  new_events)
{
  GMainContext *context;
  GPollFD *poll_fd;

  g_return_if_fail (source != NULL);
  g_return_if_fail (g_slist_find (source->priv->fds, tag));

  context = source->context;
  poll_fd = tag;

  poll_fd->events = new_events;

  if (context)
    g_main_context_wakeup (context);
}

/**
 * g_source_remove_unix_fd:
 * @source: a #GSource
 * @tag: (not nullable): the tag from g_source_add_unix_fd()
 *
 * Reverses the effect of a previous call to g_source_add_unix_fd().
 *
 * You only need to call this if you want to remove an fd from being
 * watched while keeping the same source around.  In the normal case you
 * will just want to destroy the source.
 *
 * This API is only intended to be used by implementations of #GSource.
 * Do not call this API on a #GSource that you did not create.
 *
 * As the name suggests, this function is not available on Windows.
 *
 * Since: 2.36
 **/
void
g_source_remove_unix_fd (GSource  *source,
                         gpointer  tag)
{
  GMainContext *context;
  GPollFD *poll_fd;

  g_return_if_fail (source != NULL);
  g_return_if_fail (g_slist_find (source->priv->fds, tag));

  context = source->context;
  poll_fd = tag;

  if (context)
    LOCK_CONTEXT (context);

  source->priv->fds = g_slist_remove (source->priv->fds, poll_fd);

  if (context)
    {
      if (!SOURCE_BLOCKED (source))
        g_main_context_remove_poll_unlocked (context, poll_fd);

      UNLOCK_CONTEXT (context);
    }

  g_free (poll_fd);
}

/**
 * g_source_query_unix_fd:
 * @source: a #GSource
 * @tag: (not nullable): the tag from g_source_add_unix_fd()
 *
 * Queries the events reported for the fd corresponding to @tag on
 * @source during the last poll.
 *
 * The return value of this function is only defined when the function
 * is called from the check or dispatch functions for @source.
 *
 * This API is only intended to be used by implementations of #GSource.
 * Do not call this API on a #GSource that you did not create.
 *
 * As the name suggests, this function is not available on Windows.
 *
 * Returns: the conditions reported on the fd
 *
 * Since: 2.36
 **/
GIOCondition
g_source_query_unix_fd (GSource  *source,
                        gpointer  tag)
{
  GPollFD *poll_fd;

  g_return_val_if_fail (source != NULL, 0);
  g_return_val_if_fail (g_slist_find (source->priv->fds, tag), 0);

  poll_fd = tag;

  return poll_fd->revents;
}
#endif /* G_OS_UNIX */

/**
 * g_get_current_time:
 * @result: #GTimeVal structure in which to store current time.
 *
 * Equivalent to the UNIX gettimeofday() function, but portable.
 *
 * You may find g_get_real_time() to be more convenient.
 **/
void
g_get_current_time (GTimeVal *result)
{
#ifndef G_OS_WIN32
  struct timeval r;

  g_return_if_fail (result != NULL);

  /*this is required on alpha, there the timeval structs are int's
    not longs and a cast only would fail horribly*/
  gettimeofday (&r, NULL);
  result->tv_sec = r.tv_sec;
  result->tv_usec = r.tv_usec;
#else
  FILETIME ft;
  guint64 time64;

  g_return_if_fail (result != NULL);

  GetSystemTimeAsFileTime (&ft);
  memmove (&time64, &ft, sizeof (FILETIME));

  /* Convert from 100s of nanoseconds since 1601-01-01
   * to Unix epoch. Yes, this is Y2038 unsafe.
   */
  time64 -= G_GINT64_CONSTANT (116444736000000000);
  time64 /= 10;

  result->tv_sec = time64 / 1000000;
  result->tv_usec = time64 % 1000000;
#endif
}

/**
 * g_get_real_time:
 *
 * Queries the system wall-clock time.
 *
 * This call is functionally equivalent to g_get_current_time() except
 * that the return value is often more convenient than dealing with a
 * #GTimeVal.
 *
 * You should only use this call if you are actually interested in the real
 * wall-clock time.  g_get_monotonic_time() is probably more useful for
 * measuring intervals.
 *
 * Returns: the number of microseconds since January 1, 1970 UTC.
 *
 * Since: 2.28
 **/
gint64
g_get_real_time (void)
{
  GTimeVal tv;

  g_get_current_time (&tv);

  return (((gint64) tv.tv_sec) * 1000000) + tv.tv_usec;
}

/**
 * g_get_monotonic_time:
 *
 * Queries the system monotonic time.
 *
 * The monotonic clock will always increase and doesn't suffer
 * discontinuities when the user (or NTP) changes the system time.  It
 * may or may not continue to tick during times where the machine is
 * suspended.
 *
 * We try to use the clock that corresponds as closely as possible to
 * the passage of time as measured by system calls such as poll() but it
 * may not always be possible to do this.
 *
 * Returns: the monotonic time, in microseconds
 *
 * Since: 2.28
 **/
#if defined (G_OS_WIN32)
/* NOTE:
 * time_usec = ticks_since_boot * usec_per_sec / ticks_per_sec
 *
 * Doing (ticks_since_boot * usec_per_sec) before the division can overflow 64 bits
 * (ticks_since_boot  / ticks_per_sec) and then multiply would not be accurate enough.
 * So for now we calculate (usec_per_sec / ticks_per_sec) and use floating point
 */
static gdouble g_monotonic_usec_per_tick = 0;

void
g_clock_win32_init (void)
{
  LARGE_INTEGER freq;

  if (!QueryPerformanceFrequency (&freq) || freq.QuadPart == 0)
    {
      /* The documentation says that this should never happen */
      g_assert_not_reached ();
      return;
    }

  g_monotonic_usec_per_tick = (gdouble)G_USEC_PER_SEC / freq.QuadPart;
}

gint64
g_get_monotonic_time (void)
{
  if (G_LIKELY (g_monotonic_usec_per_tick != 0))
    {
      LARGE_INTEGER ticks;

      if (QueryPerformanceCounter (&ticks))
        return (gint64)(ticks.QuadPart * g_monotonic_usec_per_tick);

      g_warning ("QueryPerformanceCounter Failed (%lu)", GetLastError ());
      g_monotonic_usec_per_tick = 0;
    }

  return 0;
}
#elif defined(HAVE_MACH_MACH_TIME_H) /* Mac OS */
gint64
g_get_monotonic_time (void)
{
  static mach_timebase_info_data_t timebase_info;

  if (timebase_info.denom == 0)
    {
      /* This is a fraction that we must use to scale
       * mach_absolute_time() by in order to reach nanoseconds.
       *
       * We've only ever observed this to be 1/1, but maybe it could be
       * 1000/1 if mach time is microseconds already, or 1/1000 if
       * picoseconds.  Try to deal nicely with that.
       */
      mach_timebase_info (&timebase_info);

      /* We actually want microseconds... */
      if (timebase_info.numer % 1000 == 0)
        timebase_info.numer /= 1000;
      else
        timebase_info.denom *= 1000;

      /* We want to make the numer 1 to avoid having to multiply... */
      if (timebase_info.denom % timebase_info.numer == 0)
        {
          timebase_info.denom /= timebase_info.numer;
          timebase_info.numer = 1;
        }
      else
        {
          /* We could just multiply by timebase_info.numer below, but why
           * bother for a case that may never actually exist...
           *
           * Plus -- performing the multiplication would risk integer
           * overflow.  If we ever actually end up in this situation, we
           * should more carefully evaluate the correct course of action.
           */
          mach_timebase_info (&timebase_info); /* Get a fresh copy for a better message */
          g_error ("Got weird mach timebase info of %d/%d.  Please file a bug against GLib.",
                   timebase_info.numer, timebase_info.denom);
        }
    }

  return mach_absolute_time () / timebase_info.denom;
}
#else
gint64
g_get_monotonic_time (void)
{
  struct timespec ts;
  gint result;

  result = clock_gettime (CLOCK_MONOTONIC, &ts);

  if G_UNLIKELY (result != 0)
    g_error ("GLib requires working CLOCK_MONOTONIC");

  return (((gint64) ts.tv_sec) * 1000000) + (ts.tv_nsec / 1000);
}
#endif

static void
g_main_dispatch_free (gpointer dispatch)
{
  g_slice_free (GMainDispatch, dispatch);
}

/* Running the main loop */

static GMainDispatch *
get_dispatch (void)
{
  static GPrivate depth_private = G_PRIVATE_INIT (g_main_dispatch_free);
  GMainDispatch *dispatch;

  dispatch = g_private_get (&depth_private);

  if (!dispatch)
    {
      dispatch = g_slice_new0 (GMainDispatch);
      g_private_set (&depth_private, dispatch);
    }

  return dispatch;
}

/**
 * g_main_depth:
 *
 * Returns the depth of the stack of calls to
 * g_main_context_dispatch() on any #GMainContext in the current thread.
 *  That is, when called from the toplevel, it gives 0. When
 * called from within a callback from g_main_context_iteration()
 * (or g_main_loop_run(), etc.) it returns 1. When called from within 
 * a callback to a recursive call to g_main_context_iteration(),
 * it returns 2. And so forth.
 *
 * This function is useful in a situation like the following:
 * Imagine an extremely simple "garbage collected" system.
 *
 * |[<!-- language="C" --> 
 * static GList *free_list;
 * 
 * gpointer
 * allocate_memory (gsize size)
 * { 
 *   gpointer result = g_malloc (size);
 *   free_list = g_list_prepend (free_list, result);
 *   return result;
 * }
 * 
 * void
 * free_allocated_memory (void)
 * {
 *   GList *l;
 *   for (l = free_list; l; l = l->next);
 *     g_free (l->data);
 *   g_list_free (free_list);
 *   free_list = NULL;
 *  }
 * 
 * [...]
 * 
 * while (TRUE); 
 *  {
 *    g_main_context_iteration (NULL, TRUE);
 *    free_allocated_memory();
 *   }
 * ]|
 *
 * This works from an application, however, if you want to do the same
 * thing from a library, it gets more difficult, since you no longer
 * control the main loop. You might think you can simply use an idle
 * function to make the call to free_allocated_memory(), but that
 * doesn't work, since the idle function could be called from a
 * recursive callback. This can be fixed by using g_main_depth()
 *
 * |[<!-- language="C" --> 
 * gpointer
 * allocate_memory (gsize size)
 * { 
 *   FreeListBlock *block = g_new (FreeListBlock, 1);
 *   block->mem = g_malloc (size);
 *   block->depth = g_main_depth ();   
 *   free_list = g_list_prepend (free_list, block);
 *   return block->mem;
 * }
 * 
 * void
 * free_allocated_memory (void)
 * {
 *   GList *l;
 *   
 *   int depth = g_main_depth ();
 *   for (l = free_list; l; );
 *     {
 *       GList *next = l->next;
 *       FreeListBlock *block = l->data;
 *       if (block->depth > depth)
 *         {
 *           g_free (block->mem);
 *           g_free (block);
 *           free_list = g_list_delete_link (free_list, l);
 *         }
 *               
 *       l = next;
 *     }
 *   }
 * ]|
 *
 * There is a temptation to use g_main_depth() to solve
 * problems with reentrancy. For instance, while waiting for data
 * to be received from the network in response to a menu item,
 * the menu item might be selected again. It might seem that
 * one could make the menu item's callback return immediately
 * and do nothing if g_main_depth() returns a value greater than 1.
 * However, this should be avoided since the user then sees selecting
 * the menu item do nothing. Furthermore, you'll find yourself adding
 * these checks all over your code, since there are doubtless many,
 * many things that the user could do. Instead, you can use the
 * following techniques:
 *
 * 1. Use gtk_widget_set_sensitive() or modal dialogs to prevent
 *    the user from interacting with elements while the main
 *    loop is recursing.
 * 
 * 2. Avoid main loop recursion in situations where you can't handle
 *    arbitrary  callbacks. Instead, structure your code so that you
 *    simply return to the main loop and then get called again when
 *    there is more work to do.
 * 
 * Returns: The main loop recursion level in the current thread
 */
int
g_main_depth (void)
{
  GMainDispatch *dispatch = get_dispatch ();
  return dispatch->depth;
}

/**
 * g_main_current_source:
 *
 * Returns the currently firing source for this thread.
 * 
 * Returns: (transfer none): The currently firing source or %NULL.
 *
 * Since: 2.12
 */
GSource *
g_main_current_source (void)
{
  GMainDispatch *dispatch = get_dispatch ();
  return dispatch->source;
}

/**
 * g_source_is_destroyed:
 * @source: a #GSource
 *
 * Returns whether @source has been destroyed.
 *
 * This is important when you operate upon your objects 
 * from within idle handlers, but may have freed the object 
 * before the dispatch of your idle handler.
 *
 * |[<!-- language="C" --> 
 * static gboolean 
 * idle_callback (gpointer data)
 * {
 *   SomeWidget *self = data;
 *    
 *   GDK_THREADS_ENTER ();
 *   // do stuff with self
 *   self->idle_id = 0;
 *   GDK_THREADS_LEAVE ();
 *    
 *   return G_SOURCE_REMOVE;
 * }
 *  
 * static void 
 * some_widget_do_stuff_later (SomeWidget *self)
 * {
 *   self->idle_id = g_idle_add (idle_callback, self);
 * }
 *  
 * static void 
 * some_widget_finalize (GObject *object)
 * {
 *   SomeWidget *self = SOME_WIDGET (object);
 *    
 *   if (self->idle_id)
 *     g_source_remove (self->idle_id);
 *    
 *   G_OBJECT_CLASS (parent_class)->finalize (object);
 * }
 * ]|
 *
 * This will fail in a multi-threaded application if the 
 * widget is destroyed before the idle handler fires due 
 * to the use after free in the callback. A solution, to 
 * this particular problem, is to check to if the source
 * has already been destroy within the callback.
 *
 * |[<!-- language="C" --> 
 * static gboolean 
 * idle_callback (gpointer data)
 * {
 *   SomeWidget *self = data;
 *   
 *   GDK_THREADS_ENTER ();
 *   if (!g_source_is_destroyed (g_main_current_source ()))
 *     {
 *       // do stuff with self
 *     }
 *   GDK_THREADS_LEAVE ();
 *   
 *   return FALSE;
 * }
 * ]|
 *
 * Calls to this function from a thread other than the one acquired by the
 * #GMainContext the #GSource is attached to are typically redundant, as the
 * source could be destroyed immediately after this function returns. However,
 * once a source is destroyed it cannot be un-destroyed, so this function can be
 * used for opportunistic checks from any thread.
 *
 * Returns: %TRUE if the source has been destroyed
 *
 * Since: 2.12
 */
gboolean
g_source_is_destroyed (GSource *source)
{
  return SOURCE_DESTROYED (source);
}

/* Temporarily remove all this source's file descriptors from the
 * poll(), so that if data comes available for one of the file descriptors
 * we don't continually spin in the poll()
 */
/* HOLDS: source->context's lock */
static void
block_source (GSource *source)
{
  GSList *tmp_list;

  g_return_if_fail (!SOURCE_BLOCKED (source));

  source->flags |= G_SOURCE_BLOCKED;

  if (source->context)
    {
      tmp_list = source->poll_fds;
      while (tmp_list)
        {
          g_main_context_remove_poll_unlocked (source->context, tmp_list->data);
          tmp_list = tmp_list->next;
        }

      for (tmp_list = source->priv->fds; tmp_list; tmp_list = tmp_list->next)
        g_main_context_remove_poll_unlocked (source->context, tmp_list->data);
    }

  if (source->priv && source->priv->child_sources)
    {
      tmp_list = source->priv->child_sources;
      while (tmp_list)
	{
	  block_source (tmp_list->data);
	  tmp_list = tmp_list->next;
	}
    }
}

/* HOLDS: source->context's lock */
static void
unblock_source (GSource *source)
{
  GSList *tmp_list;

  g_return_if_fail (SOURCE_BLOCKED (source)); /* Source already unblocked */
  g_return_if_fail (!SOURCE_DESTROYED (source));
  
  source->flags &= ~G_SOURCE_BLOCKED;

  tmp_list = source->poll_fds;
  while (tmp_list)
    {
      g_main_context_add_poll_unlocked (source->context, source->priority, tmp_list->data);
      tmp_list = tmp_list->next;
    }

  for (tmp_list = source->priv->fds; tmp_list; tmp_list = tmp_list->next)
    g_main_context_add_poll_unlocked (source->context, source->priority, tmp_list->data);

  if (source->priv && source->priv->child_sources)
    {
      tmp_list = source->priv->child_sources;
      while (tmp_list)
	{
	  unblock_source (tmp_list->data);
	  tmp_list = tmp_list->next;
	}
    }
}

/* HOLDS: context's lock */
static void
g_main_dispatch (GMainContext *context)
{
  GMainDispatch *current = get_dispatch ();
  guint i;

  for (i = 0; i < context->pending_dispatches->len; i++)
    {
      GSource *source = context->pending_dispatches->pdata[i];

      context->pending_dispatches->pdata[i] = NULL;
      g_assert (source);

      source->flags &= ~G_SOURCE_READY;

      if (!SOURCE_DESTROYED (source))
	{
	  gboolean was_in_call;
	  gpointer user_data = NULL;
	  GSourceFunc callback = NULL;
	  GSourceCallbackFuncs *cb_funcs;
	  gpointer cb_data;
	  gboolean need_destroy;

	  gboolean (*dispatch) (GSource *,
				GSourceFunc,
				gpointer);
          GSource *prev_source;

	  dispatch = source->source_funcs->dispatch;
	  cb_funcs = source->callback_funcs;
	  cb_data = source->callback_data;

	  if (cb_funcs)
	    cb_funcs->ref (cb_data);
	  
	  if ((source->flags & G_SOURCE_CAN_RECURSE) == 0)
	    block_source (source);
	  
	  was_in_call = source->flags & G_HOOK_FLAG_IN_CALL;
	  source->flags |= G_HOOK_FLAG_IN_CALL;

	  if (cb_funcs)
	    cb_funcs->get (cb_data, source, &callback, &user_data);

	  UNLOCK_CONTEXT (context);

          /* These operations are safe because 'current' is thread-local
           * and not modified from anywhere but this function.
           */
          prev_source = current->source;
          current->source = source;
          current->depth++;

          TRACE (GLIB_MAIN_BEFORE_DISPATCH (g_source_get_name (source), source,
                                            dispatch, callback, user_data));
          need_destroy = !(* dispatch) (source, callback, user_data);
          TRACE (GLIB_MAIN_AFTER_DISPATCH (g_source_get_name (source), source,
                                           dispatch, need_destroy));

          current->source = prev_source;
          current->depth--;

	  if (cb_funcs)
	    cb_funcs->unref (cb_data);

 	  LOCK_CONTEXT (context);
	  
	  if (!was_in_call)
	    source->flags &= ~G_HOOK_FLAG_IN_CALL;

	  if (SOURCE_BLOCKED (source) && !SOURCE_DESTROYED (source))
	    unblock_source (source);
	  
	  /* Note: this depends on the fact that we can't switch
	   * sources from one main context to another
	   */
	  if (need_destroy && !SOURCE_DESTROYED (source))
	    {
	      g_assert (source->context == context);
	      g_source_destroy_internal (source, context, TRUE);
	    }
	}
      
      SOURCE_UNREF (source, context);
    }

  g_ptr_array_set_size (context->pending_dispatches, 0);
}

/**
 * g_main_context_acquire:
 * @context: a #GMainContext
 * 
 * Tries to become the owner of the specified context.
 * If some other thread is the owner of the context,
 * returns %FALSE immediately. Ownership is properly
 * recursive: the owner can require ownership again
 * and will release ownership when g_main_context_release()
 * is called as many times as g_main_context_acquire().
 *
 * You must be the owner of a context before you
 * can call g_main_context_prepare(), g_main_context_query(),
 * g_main_context_check(), g_main_context_dispatch().
 * 
 * Returns: %TRUE if the operation succeeded, and
 *   this thread is now the owner of @context.
 **/
gboolean 
g_main_context_acquire (GMainContext *context)
{
  gboolean result = FALSE;
  GThread *self = G_THREAD_SELF;

  if (context == NULL)
    context = g_main_context_default ();
  
  LOCK_CONTEXT (context);

  if (!context->owner)
    {
      context->owner = self;
      g_assert (context->owner_count == 0);
      TRACE (GLIB_MAIN_CONTEXT_ACQUIRE (context, TRUE  /* success */));
    }

  if (context->owner == self)
    {
      context->owner_count++;
      result = TRUE;
    }
  else
    {
      TRACE (GLIB_MAIN_CONTEXT_ACQUIRE (context, FALSE  /* failure */));
    }

  UNLOCK_CONTEXT (context); 

  return result;
}

/**
 * g_main_context_release:
 * @context: a #GMainContext
 * 
 * Releases ownership of a context previously acquired by this thread
 * with g_main_context_acquire(). If the context was acquired multiple
 * times, the ownership will be released only when g_main_context_release()
 * is called as many times as it was acquired.
 **/
void
g_main_context_release (GMainContext *context)
{
  if (context == NULL)
    context = g_main_context_default ();
  
  LOCK_CONTEXT (context);

  context->owner_count--;
  if (context->owner_count == 0)
    {
      TRACE (GLIB_MAIN_CONTEXT_RELEASE (context));

      context->owner = NULL;

      if (context->waiters)
	{
	  GMainWaiter *waiter = context->waiters->data;
	  gboolean loop_internal_waiter = (waiter->mutex == &context->mutex);
	  context->waiters = g_slist_delete_link (context->waiters,
						  context->waiters);
	  if (!loop_internal_waiter)
	    g_mutex_lock (waiter->mutex);
	  
	  g_cond_signal (waiter->cond);
	  
	  if (!loop_internal_waiter)
	    g_mutex_unlock (waiter->mutex);
	}
    }

  UNLOCK_CONTEXT (context); 
}

/**
 * g_main_context_wait:
 * @context: a #GMainContext
 * @cond: a condition variable
 * @mutex: a mutex, currently held
 * 
 * Tries to become the owner of the specified context,
 * as with g_main_context_acquire(). But if another thread
 * is the owner, atomically drop @mutex and wait on @cond until 
 * that owner releases ownership or until @cond is signaled, then
 * try again (once) to become the owner.
 * 
 * Returns: %TRUE if the operation succeeded, and
 *   this thread is now the owner of @context.
 **/
gboolean
g_main_context_wait (GMainContext *context,
		     GCond        *cond,
		     GMutex       *mutex)
{
  gboolean result = FALSE;
  GThread *self = G_THREAD_SELF;
  gboolean loop_internal_waiter;
  
  if (context == NULL)
    context = g_main_context_default ();

  if G_UNLIKELY (cond != &context->cond || mutex != &context->mutex)
    {
      static gboolean warned;

      if (!warned)
        {
          g_critical ("WARNING!! g_main_context_wait() will be removed in a future release.  "
                      "If you see this message, please file a bug immediately.");
          warned = TRUE;
        }
    }

  loop_internal_waiter = (mutex == &context->mutex);
  
  if (!loop_internal_waiter)
    LOCK_CONTEXT (context);

  if (context->owner && context->owner != self)
    {
      GMainWaiter waiter;

      waiter.cond = cond;
      waiter.mutex = mutex;

      context->waiters = g_slist_append (context->waiters, &waiter);
      
      if (!loop_internal_waiter)
	UNLOCK_CONTEXT (context);
      g_cond_wait (cond, mutex);
      if (!loop_internal_waiter)      
	LOCK_CONTEXT (context);

      context->waiters = g_slist_remove (context->waiters, &waiter);
    }

  if (!context->owner)
    {
      context->owner = self;
      g_assert (context->owner_count == 0);
    }

  if (context->owner == self)
    {
      context->owner_count++;
      result = TRUE;
    }

  if (!loop_internal_waiter)
    UNLOCK_CONTEXT (context); 
  
  return result;
}

/**
 * g_main_context_prepare:
 * @context: a #GMainContext
 * @priority: location to store priority of highest priority
 *            source already ready.
 *
 * Prepares to poll sources within a main loop. The resulting information
 * for polling is determined by calling g_main_context_query ().
 *
 * You must have successfully acquired the context with
 * g_main_context_acquire() before you may call this function.
 *
 * Returns: %TRUE if some source is ready to be dispatched
 *               prior to polling.
 **/
gboolean
g_main_context_prepare (GMainContext *context,
			gint         *priority)
{
  guint i;
  gint n_ready = 0;
  gint current_priority = G_MAXINT;
  GSource *source;
  GSourceIter iter;

  if (context == NULL)
    context = g_main_context_default ();
  
  LOCK_CONTEXT (context);

  context->time_is_fresh = FALSE;

  if (context->in_check_or_prepare)
    {
      g_warning ("g_main_context_prepare() called recursively from within a source's check() or "
		 "prepare() member.");
      UNLOCK_CONTEXT (context);
      return FALSE;
    }

  TRACE (GLIB_MAIN_CONTEXT_BEFORE_PREPARE (context));

#if 0
  /* If recursing, finish up current dispatch, before starting over */
  if (context->pending_dispatches)
    {
      if (dispatch)
	g_main_dispatch (context, &current_time);
      
      UNLOCK_CONTEXT (context);
      return TRUE;
    }
#endif

  /* If recursing, clear list of pending dispatches */

  for (i = 0; i < context->pending_dispatches->len; i++)
    {
      if (context->pending_dispatches->pdata[i])
	SOURCE_UNREF ((GSource *)context->pending_dispatches->pdata[i], context);
    }
  g_ptr_array_set_size (context->pending_dispatches, 0);
  
  /* Prepare all sources */

  context->timeout = -1;
  
  g_source_iter_init (&iter, context, TRUE);
  while (g_source_iter_next (&iter, &source))
    {
      gint source_timeout = -1;

      if (SOURCE_DESTROYED (source) || SOURCE_BLOCKED (source))
	continue;
      if ((n_ready > 0) && (source->priority > current_priority))
	break;

      if (!(source->flags & G_SOURCE_READY))
	{
	  gboolean result;
	  gboolean (* prepare) (GSource  *source,
                                gint     *timeout);

          prepare = source->source_funcs->prepare;

          if (prepare)
            {
              context->in_check_or_prepare++;
              UNLOCK_CONTEXT (context);

              result = (* prepare) (source, &source_timeout);
              TRACE (GLIB_MAIN_AFTER_PREPARE (source, prepare, source_timeout));

              LOCK_CONTEXT (context);
              context->in_check_or_prepare--;
            }
          else
            {
              source_timeout = -1;
              result = FALSE;
            }

          if (result == FALSE && source->priv->ready_time != -1)
            {
              if (!context->time_is_fresh)
                {
                  context->time = g_get_monotonic_time ();
                  context->time_is_fresh = TRUE;
                }

              if (source->priv->ready_time <= context->time)
                {
                  source_timeout = 0;
                  result = TRUE;
                }
              else
                {
                  gint timeout;

                  /* rounding down will lead to spinning, so always round up */
                  timeout = (source->priv->ready_time - context->time + 999) / 1000;

                  if (source_timeout < 0 || timeout < source_timeout)
                    source_timeout = timeout;
                }
            }

	  if (result)
	    {
	      GSource *ready_source = source;

	      while (ready_source)
		{
		  ready_source->flags |= G_SOURCE_READY;
		  ready_source = ready_source->priv->parent_source;
		}
	    }
	}

      if (source->flags & G_SOURCE_READY)
	{
	  n_ready++;
	  current_priority = source->priority;
	  context->timeout = 0;
	}
      
      if (source_timeout >= 0)
	{
	  if (context->timeout < 0)
	    context->timeout = source_timeout;
	  else
	    context->timeout = MIN (context->timeout, source_timeout);
	}
    }
  g_source_iter_clear (&iter);

  TRACE (GLIB_MAIN_CONTEXT_AFTER_PREPARE (context, current_priority, n_ready));

  UNLOCK_CONTEXT (context);
  
  if (priority)
    *priority = current_priority;
  
  return (n_ready > 0);
}

/**
 * g_main_context_query:
 * @context: a #GMainContext
 * @max_priority: maximum priority source to check
 * @timeout_: (out): location to store timeout to be used in polling
 * @fds: (out caller-allocates) (array length=n_fds): location to
 *       store #GPollFD records that need to be polled.
 * @n_fds: (in): length of @fds.
 *
 * Determines information necessary to poll this main loop.
 *
 * You must have successfully acquired the context with
 * g_main_context_acquire() before you may call this function.
 *
 * Returns: the number of records actually stored in @fds,
 *   or, if more than @n_fds records need to be stored, the number
 *   of records that need to be stored.
 **/
gint
g_main_context_query (GMainContext *context,
		      gint          max_priority,
		      gint         *timeout,
		      GPollFD      *fds,
		      gint          n_fds)
{
  gint n_poll;
  GPollRec *pollrec, *lastpollrec;
  gushort events;
  
  LOCK_CONTEXT (context);

  TRACE (GLIB_MAIN_CONTEXT_BEFORE_QUERY (context, max_priority));

  n_poll = 0;
  lastpollrec = NULL;
  for (pollrec = context->poll_records; pollrec; pollrec = pollrec->next)
    {
      if (pollrec->priority > max_priority)
        continue;

      /* In direct contradiction to the Unix98 spec, IRIX runs into
       * difficulty if you pass in POLLERR, POLLHUP or POLLNVAL
       * flags in the events field of the pollfd while it should
       * just ignoring them. So we mask them out here.
       */
      events = pollrec->fd->events & ~(G_IO_ERR|G_IO_HUP|G_IO_NVAL);

      if (lastpollrec && pollrec->fd->fd == lastpollrec->fd->fd)
        {
          if (n_poll - 1 < n_fds)
            fds[n_poll - 1].events |= events;
        }
      else
        {
          if (n_poll < n_fds)
            {
              fds[n_poll].fd = pollrec->fd->fd;
              fds[n_poll].events = events;
              fds[n_poll].revents = 0;
            }

          n_poll++;
        }

      lastpollrec = pollrec;
    }

  context->poll_changed = FALSE;
  
  if (timeout)
    {
      *timeout = context->timeout;
      if (*timeout != 0)
        context->time_is_fresh = FALSE;
    }

  TRACE (GLIB_MAIN_CONTEXT_AFTER_QUERY (context, context->timeout,
                                        fds, n_poll));

  UNLOCK_CONTEXT (context);

  return n_poll;
}

/**
 * g_main_context_check:
 * @context: a #GMainContext
 * @max_priority: the maximum numerical priority of sources to check
 * @fds: (array length=n_fds): array of #GPollFD's that was passed to
 *       the last call to g_main_context_query()
 * @n_fds: return value of g_main_context_query()
 *
 * Passes the results of polling back to the main loop.
 *
 * You must have successfully acquired the context with
 * g_main_context_acquire() before you may call this function.
 *
 * Returns: %TRUE if some sources are ready to be dispatched.
 **/
gboolean
g_main_context_check (GMainContext *context,
		      gint          max_priority,
		      GPollFD      *fds,
		      gint          n_fds)
{
  GSource *source;
  GSourceIter iter;
  GPollRec *pollrec;
  gint n_ready = 0;
  gint i;
   
  LOCK_CONTEXT (context);

  if (context->in_check_or_prepare)
    {
      g_warning ("g_main_context_check() called recursively from within a source's check() or "
		 "prepare() member.");
      UNLOCK_CONTEXT (context);
      return FALSE;
    }

  TRACE (GLIB_MAIN_CONTEXT_BEFORE_CHECK (context, max_priority, fds, n_fds));

  for (i = 0; i < n_fds; i++)
    {
      if (fds[i].fd == context->wake_up_rec.fd)
        {
          if (fds[i].revents)
            {
              TRACE (GLIB_MAIN_CONTEXT_WAKEUP_ACKNOWLEDGE (context));
              g_wakeup_acknowledge (context->wakeup);
            }
          break;
        }
    }

  /* If the set of poll file descriptors changed, bail out
   * and let the main loop rerun
   */
  if (context->poll_changed)
    {
      TRACE (GLIB_MAIN_CONTEXT_AFTER_CHECK (context, 0));

      UNLOCK_CONTEXT (context);
      return FALSE;
    }

  pollrec = context->poll_records;
  i = 0;
  while (pollrec && i < n_fds)
    {
      while (pollrec && pollrec->fd->fd == fds[i].fd)
        {
          if (pollrec->priority <= max_priority)
            {
              pollrec->fd->revents =
                fds[i].revents & (pollrec->fd->events | G_IO_ERR | G_IO_HUP | G_IO_NVAL);
            }
          pollrec = pollrec->next;
        }

      i++;
    }

  g_source_iter_init (&iter, context, TRUE);
  while (g_source_iter_next (&iter, &source))
    {
      if (SOURCE_DESTROYED (source) || SOURCE_BLOCKED (source))
	continue;
      if ((n_ready > 0) && (source->priority > max_priority))
	break;

      if (!(source->flags & G_SOURCE_READY))
	{
          gboolean result;
          gboolean (* check) (GSource *source);

          check = source->source_funcs->check;

          if (check)
            {
              /* If the check function is set, call it. */
              context->in_check_or_prepare++;
              UNLOCK_CONTEXT (context);

              result = (* check) (source);

              TRACE (GLIB_MAIN_AFTER_CHECK (source, check, result));

              LOCK_CONTEXT (context);
              context->in_check_or_prepare--;
            }
          else
            result = FALSE;

          if (result == FALSE)
            {
              GSList *tmp_list;

              /* If not already explicitly flagged ready by ->check()
               * (or if we have no check) then we can still be ready if
               * any of our fds poll as ready.
               */
              for (tmp_list = source->priv->fds; tmp_list; tmp_list = tmp_list->next)
                {
                  GPollFD *pollfd = tmp_list->data;

                  if (pollfd->revents)
                    {
                      result = TRUE;
                      break;
                    }
                }
            }

          if (result == FALSE && source->priv->ready_time != -1)
            {
              if (!context->time_is_fresh)
                {
                  context->time = g_get_monotonic_time ();
                  context->time_is_fresh = TRUE;
                }

              if (source->priv->ready_time <= context->time)
                result = TRUE;
            }

	  if (result)
	    {
	      GSource *ready_source = source;

	      while (ready_source)
		{
		  ready_source->flags |= G_SOURCE_READY;
		  ready_source = ready_source->priv->parent_source;
		}
	    }
	}

      if (source->flags & G_SOURCE_READY)
	{
	  source->ref_count++;
	  g_ptr_array_add (context->pending_dispatches, source);

	  n_ready++;

          /* never dispatch sources with less priority than the first
           * one we choose to dispatch
           */
          max_priority = source->priority;
	}
    }
  g_source_iter_clear (&iter);

  TRACE (GLIB_MAIN_CONTEXT_AFTER_CHECK (context, n_ready));

  UNLOCK_CONTEXT (context);

  return n_ready > 0;
}

/**
 * g_main_context_dispatch:
 * @context: a #GMainContext
 *
 * Dispatches all pending sources.
 *
 * You must have successfully acquired the context with
 * g_main_context_acquire() before you may call this function.
 **/
void
g_main_context_dispatch (GMainContext *context)
{
  LOCK_CONTEXT (context);

  TRACE (GLIB_MAIN_CONTEXT_BEFORE_DISPATCH (context));

  if (context->pending_dispatches->len > 0)
    {
      g_main_dispatch (context);
    }

  TRACE (GLIB_MAIN_CONTEXT_AFTER_DISPATCH (context));

  UNLOCK_CONTEXT (context);
}

/* HOLDS context lock */
static gboolean
g_main_context_iterate (GMainContext *context,
			gboolean      block,
			gboolean      dispatch,
			GThread      *self)
{
  gint max_priority;
  gint timeout;
  gboolean some_ready;
  gint nfds, allocated_nfds;
  GPollFD *fds = NULL;

  UNLOCK_CONTEXT (context);

  if (!g_main_context_acquire (context))
    {
      gboolean got_ownership;

      LOCK_CONTEXT (context);

      if (!block)
	return FALSE;

      got_ownership = g_main_context_wait (context,
                                           &context->cond,
                                           &context->mutex);

      if (!got_ownership)
	return FALSE;
    }
  else
    LOCK_CONTEXT (context);
  
  if (!context->cached_poll_array)
    {
      context->cached_poll_array_size = context->n_poll_records;
      context->cached_poll_array = g_new (GPollFD, context->n_poll_records);
    }

  allocated_nfds = context->cached_poll_array_size;
  fds = context->cached_poll_array;
  
  UNLOCK_CONTEXT (context);

  g_main_context_prepare (context, &max_priority); 
  
  while ((nfds = g_main_context_query (context, max_priority, &timeout, fds, 
				       allocated_nfds)) > allocated_nfds)
    {
      LOCK_CONTEXT (context);
      g_free (fds);
      context->cached_poll_array_size = allocated_nfds = nfds;
      context->cached_poll_array = fds = g_new (GPollFD, nfds);
      UNLOCK_CONTEXT (context);
    }

  if (!block)
    timeout = 0;
  
  g_main_context_poll (context, timeout, max_priority, fds, nfds);
  
  some_ready = g_main_context_check (context, max_priority, fds, nfds);
  
  if (dispatch)
    g_main_context_dispatch (context);
  
  g_main_context_release (context);

  LOCK_CONTEXT (context);

  return some_ready;
}

/**
 * g_main_context_pending:
 * @context: (nullable): a #GMainContext (if %NULL, the default context will be used)
 *
 * Checks if any sources have pending events for the given context.
 * 
 * Returns: %TRUE if events are pending.
 **/
gboolean 
g_main_context_pending (GMainContext *context)
{
  gboolean retval;

  if (!context)
    context = g_main_context_default();

  LOCK_CONTEXT (context);
  retval = g_main_context_iterate (context, FALSE, FALSE, G_THREAD_SELF);
  UNLOCK_CONTEXT (context);
  
  return retval;
}

/**
 * g_main_context_iteration:
 * @context: (nullable): a #GMainContext (if %NULL, the default context will be used) 
 * @may_block: whether the call may block.
 *
 * Runs a single iteration for the given main loop. This involves
 * checking to see if any event sources are ready to be processed,
 * then if no events sources are ready and @may_block is %TRUE, waiting
 * for a source to become ready, then dispatching the highest priority
 * events sources that are ready. Otherwise, if @may_block is %FALSE
 * sources are not waited to become ready, only those highest priority
 * events sources will be dispatched (if any), that are ready at this
 * given moment without further waiting.
 *
 * Note that even when @may_block is %TRUE, it is still possible for
 * g_main_context_iteration() to return %FALSE, since the wait may
 * be interrupted for other reasons than an event source becoming ready.
 *
 * Returns: %TRUE if events were dispatched.
 **/
gboolean
g_main_context_iteration (GMainContext *context, gboolean may_block)
{
  gboolean retval;

  if (!context)
    context = g_main_context_default();
  
  LOCK_CONTEXT (context);
  retval = g_main_context_iterate (context, may_block, TRUE, G_THREAD_SELF);
  UNLOCK_CONTEXT (context);
  
  return retval;
}

/**
 * g_main_loop_new:
 * @context: (nullable): a #GMainContext  (if %NULL, the default context will be used).
 * @is_running: set to %TRUE to indicate that the loop is running. This
 * is not very important since calling g_main_loop_run() will set this to
 * %TRUE anyway.
 * 
 * Creates a new #GMainLoop structure.
 * 
 * Returns: a new #GMainLoop.
 **/
GMainLoop *
g_main_loop_new (GMainContext *context,
		 gboolean      is_running)
{
  GMainLoop *loop;

  if (!context)
    context = g_main_context_default();
  
  g_main_context_ref (context);

  loop = g_new0 (GMainLoop, 1);
  loop->context = context;
  loop->is_running = is_running != FALSE;
  loop->ref_count = 1;

  TRACE (GLIB_MAIN_LOOP_NEW (loop, context));

  return loop;
}

/**
 * g_main_loop_ref:
 * @loop: a #GMainLoop
 * 
 * Increases the reference count on a #GMainLoop object by one.
 * 
 * Returns: @loop
 **/
GMainLoop *
g_main_loop_ref (GMainLoop *loop)
{
  g_return_val_if_fail (loop != NULL, NULL);
  g_return_val_if_fail (g_atomic_int_get (&loop->ref_count) > 0, NULL);

  g_atomic_int_inc (&loop->ref_count);

  return loop;
}

/**
 * g_main_loop_unref:
 * @loop: a #GMainLoop
 * 
 * Decreases the reference count on a #GMainLoop object by one. If
 * the result is zero, free the loop and free all associated memory.
 **/
void
g_main_loop_unref (GMainLoop *loop)
{
  g_return_if_fail (loop != NULL);
  g_return_if_fail (g_atomic_int_get (&loop->ref_count) > 0);

  if (!g_atomic_int_dec_and_test (&loop->ref_count))
    return;

  g_main_context_unref (loop->context);
  g_free (loop);
}

/**
 * g_main_loop_run:
 * @loop: a #GMainLoop
 * 
 * Runs a main loop until g_main_loop_quit() is called on the loop.
 * If this is called for the thread of the loop's #GMainContext,
 * it will process events from the loop, otherwise it will
 * simply wait.
 **/
void 
g_main_loop_run (GMainLoop *loop)
{
  GThread *self = G_THREAD_SELF;

  g_return_if_fail (loop != NULL);
  g_return_if_fail (g_atomic_int_get (&loop->ref_count) > 0);

  if (!g_main_context_acquire (loop->context))
    {
      gboolean got_ownership = FALSE;
      
      /* Another thread owns this context */
      LOCK_CONTEXT (loop->context);

      g_atomic_int_inc (&loop->ref_count);

      if (!loop->is_running)
	loop->is_running = TRUE;

      while (loop->is_running && !got_ownership)
	got_ownership = g_main_context_wait (loop->context,
                                             &loop->context->cond,
                                             &loop->context->mutex);
      
      if (!loop->is_running)
	{
	  UNLOCK_CONTEXT (loop->context);
	  if (got_ownership)
	    g_main_context_release (loop->context);
	  g_main_loop_unref (loop);
	  return;
	}

      g_assert (got_ownership);
    }
  else
    LOCK_CONTEXT (loop->context);

  if (loop->context->in_check_or_prepare)
    {
      g_warning ("g_main_loop_run(): called recursively from within a source's "
		 "check() or prepare() member, iteration not possible.");
      return;
    }

  g_atomic_int_inc (&loop->ref_count);
  loop->is_running = TRUE;
  while (loop->is_running)
    g_main_context_iterate (loop->context, TRUE, TRUE, self);

  UNLOCK_CONTEXT (loop->context);
  
  g_main_context_release (loop->context);
  
  g_main_loop_unref (loop);
}

/**
 * g_main_loop_quit:
 * @loop: a #GMainLoop
 * 
 * Stops a #GMainLoop from running. Any calls to g_main_loop_run()
 * for the loop will return. 
 *
 * Note that sources that have already been dispatched when 
 * g_main_loop_quit() is called will still be executed.
 **/
void 
g_main_loop_quit (GMainLoop *loop)
{
  g_return_if_fail (loop != NULL);
  g_return_if_fail (g_atomic_int_get (&loop->ref_count) > 0);

  LOCK_CONTEXT (loop->context);
  loop->is_running = FALSE;
  g_wakeup_signal (loop->context->wakeup);

  g_cond_broadcast (&loop->context->cond);

  UNLOCK_CONTEXT (loop->context);

  TRACE (GLIB_MAIN_LOOP_QUIT (loop));
}

/**
 * g_main_loop_is_running:
 * @loop: a #GMainLoop.
 * 
 * Checks to see if the main loop is currently being run via g_main_loop_run().
 * 
 * Returns: %TRUE if the mainloop is currently being run.
 **/
gboolean
g_main_loop_is_running (GMainLoop *loop)
{
  g_return_val_if_fail (loop != NULL, FALSE);
  g_return_val_if_fail (g_atomic_int_get (&loop->ref_count) > 0, FALSE);

  return loop->is_running;
}

/**
 * g_main_loop_get_context:
 * @loop: a #GMainLoop.
 * 
 * Returns the #GMainContext of @loop.
 * 
 * Returns: (transfer none): the #GMainContext of @loop
 **/
GMainContext *
g_main_loop_get_context (GMainLoop *loop)
{
  g_return_val_if_fail (loop != NULL, NULL);
  g_return_val_if_fail (g_atomic_int_get (&loop->ref_count) > 0, NULL);
 
  return loop->context;
}

/* HOLDS: context's lock */
static void
g_main_context_poll (GMainContext *context,
		     gint          timeout,
		     gint          priority,
		     GPollFD      *fds,
		     gint          n_fds)
{
#ifdef  G_MAIN_POLL_DEBUG
  GTimer *poll_timer;
  GPollRec *pollrec;
  gint i;
#endif

  GPollFunc poll_func;

  if (n_fds || timeout != 0)
    {
      int ret, errsv;

#ifdef	G_MAIN_POLL_DEBUG
      poll_timer = NULL;
      if (_g_main_poll_debug)
	{
	  g_print ("polling context=%p n=%d timeout=%d\n",
		   context, n_fds, timeout);
	  poll_timer = g_timer_new ();
	}
#endif

      LOCK_CONTEXT (context);

      poll_func = context->poll_func;

      UNLOCK_CONTEXT (context);
      ret = (*poll_func) (fds, n_fds, timeout);
      errsv = errno;
      if (ret < 0 && errsv != EINTR)
	{
#ifndef G_OS_WIN32
	  g_warning ("poll(2) failed due to: %s.",
		     g_strerror (errsv));
#else
	  /* If g_poll () returns -1, it has already called g_warning() */
#endif
	}
      
#ifdef	G_MAIN_POLL_DEBUG
      if (_g_main_poll_debug)
	{
	  LOCK_CONTEXT (context);

	  g_print ("g_main_poll(%d) timeout: %d - elapsed %12.10f seconds",
		   n_fds,
		   timeout,
		   g_timer_elapsed (poll_timer, NULL));
	  g_timer_destroy (poll_timer);
	  pollrec = context->poll_records;

	  while (pollrec != NULL)
	    {
	      i = 0;
	      while (i < n_fds)
		{
		  if (fds[i].fd == pollrec->fd->fd &&
		      pollrec->fd->events &&
		      fds[i].revents)
		    {
		      g_print (" [" G_POLLFD_FORMAT " :", fds[i].fd);
		      if (fds[i].revents & G_IO_IN)
			g_print ("i");
		      if (fds[i].revents & G_IO_OUT)
			g_print ("o");
		      if (fds[i].revents & G_IO_PRI)
			g_print ("p");
		      if (fds[i].revents & G_IO_ERR)
			g_print ("e");
		      if (fds[i].revents & G_IO_HUP)
			g_print ("h");
		      if (fds[i].revents & G_IO_NVAL)
			g_print ("n");
		      g_print ("]");
		    }
		  i++;
		}
	      pollrec = pollrec->next;
	    }
	  g_print ("\n");

	  UNLOCK_CONTEXT (context);
	}
#endif
    } /* if (n_fds || timeout != 0) */
}

/**
 * g_main_context_add_poll:
 * @context: (nullable): a #GMainContext (or %NULL for the default context)
 * @fd: a #GPollFD structure holding information about a file
 *      descriptor to watch.
 * @priority: the priority for this file descriptor which should be
 *      the same as the priority used for g_source_attach() to ensure that the
 *      file descriptor is polled whenever the results may be needed.
 *
 * Adds a file descriptor to the set of file descriptors polled for
 * this context. This will very seldom be used directly. Instead
 * a typical event source will use g_source_add_unix_fd() instead.
 **/
void
g_main_context_add_poll (GMainContext *context,
			 GPollFD      *fd,
			 gint          priority)
{
  if (!context)
    context = g_main_context_default ();
  
  g_return_if_fail (g_atomic_int_get (&context->ref_count) > 0);
  g_return_if_fail (fd);

  LOCK_CONTEXT (context);
  g_main_context_add_poll_unlocked (context, priority, fd);
  UNLOCK_CONTEXT (context);
}

/* HOLDS: main_loop_lock */
static void 
g_main_context_add_poll_unlocked (GMainContext *context,
				  gint          priority,
				  GPollFD      *fd)
{
  GPollRec *prevrec, *nextrec;
  GPollRec *newrec = g_slice_new (GPollRec);

  /* This file descriptor may be checked before we ever poll */
  fd->revents = 0;
  newrec->fd = fd;
  newrec->priority = priority;

  prevrec = NULL;
  nextrec = context->poll_records;
  while (nextrec)
    {
      if (nextrec->fd->fd > fd->fd)
        break;
      prevrec = nextrec;
      nextrec = nextrec->next;
    }

  if (prevrec)
    prevrec->next = newrec;
  else
    context->poll_records = newrec;

  newrec->prev = prevrec;
  newrec->next = nextrec;

  if (nextrec)
    nextrec->prev = newrec;

  context->n_poll_records++;

  context->poll_changed = TRUE;

  /* Now wake up the main loop if it is waiting in the poll() */
  g_wakeup_signal (context->wakeup);
}

/**
 * g_main_context_remove_poll:
 * @context:a #GMainContext 
 * @fd: a #GPollFD descriptor previously added with g_main_context_add_poll()
 * 
 * Removes file descriptor from the set of file descriptors to be
 * polled for a particular context.
 **/
void
g_main_context_remove_poll (GMainContext *context,
			    GPollFD      *fd)
{
  if (!context)
    context = g_main_context_default ();
  
  g_return_if_fail (g_atomic_int_get (&context->ref_count) > 0);
  g_return_if_fail (fd);

  LOCK_CONTEXT (context);
  g_main_context_remove_poll_unlocked (context, fd);
  UNLOCK_CONTEXT (context);
}

static void
g_main_context_remove_poll_unlocked (GMainContext *context,
				     GPollFD      *fd)
{
  GPollRec *pollrec, *prevrec, *nextrec;

  prevrec = NULL;
  pollrec = context->poll_records;

  while (pollrec)
    {
      nextrec = pollrec->next;
      if (pollrec->fd == fd)
	{
	  if (prevrec != NULL)
	    prevrec->next = nextrec;
	  else
	    context->poll_records = nextrec;

	  if (nextrec != NULL)
	    nextrec->prev = prevrec;

	  g_slice_free (GPollRec, pollrec);

	  context->n_poll_records--;
	  break;
	}
      prevrec = pollrec;
      pollrec = nextrec;
    }

  context->poll_changed = TRUE;

  /* Now wake up the main loop if it is waiting in the poll() */
  g_wakeup_signal (context->wakeup);
}

/**
 * g_source_get_current_time:
 * @source:  a #GSource
 * @timeval: #GTimeVal structure in which to store current time.
 *
 * This function ignores @source and is otherwise the same as
 * g_get_current_time().
 *
 * Deprecated: 2.28: use g_source_get_time() instead
 **/
void
g_source_get_current_time (GSource  *source,
			   GTimeVal *timeval)
{
  g_get_current_time (timeval);
}

/**
 * g_source_get_time:
 * @source: a #GSource
 *
 * Gets the time to be used when checking this source. The advantage of
 * calling this function over calling g_get_monotonic_time() directly is
 * that when checking multiple sources, GLib can cache a single value
 * instead of having to repeatedly get the system monotonic time.
 *
 * The time here is the system monotonic time, if available, or some
 * other reasonable alternative otherwise.  See g_get_monotonic_time().
 *
 * Returns: the monotonic time in microseconds
 *
 * Since: 2.28
 **/
gint64
g_source_get_time (GSource *source)
{
  GMainContext *context;
  gint64 result;

  g_return_val_if_fail (source->context != NULL, 0);

  context = source->context;

  LOCK_CONTEXT (context);

  if (!context->time_is_fresh)
    {
      context->time = g_get_monotonic_time ();
      context->time_is_fresh = TRUE;
    }

  result = context->time;

  UNLOCK_CONTEXT (context);

  return result;
}

/**
 * g_main_context_set_poll_func:
 * @context: a #GMainContext
 * @func: the function to call to poll all file descriptors
 * 
 * Sets the function to use to handle polling of file descriptors. It
 * will be used instead of the poll() system call 
 * (or GLib's replacement function, which is used where 
 * poll() isn't available).
 *
 * This function could possibly be used to integrate the GLib event
 * loop with an external event loop.
 **/
void
g_main_context_set_poll_func (GMainContext *context,
			      GPollFunc     func)
{
  if (!context)
    context = g_main_context_default ();
  
  g_return_if_fail (g_atomic_int_get (&context->ref_count) > 0);

  LOCK_CONTEXT (context);
  
  if (func)
    context->poll_func = func;
  else
    context->poll_func = g_poll;

  UNLOCK_CONTEXT (context);
}

/**
 * g_main_context_get_poll_func:
 * @context: a #GMainContext
 * 
 * Gets the poll function set by g_main_context_set_poll_func().
 * 
 * Returns: the poll function
 **/
GPollFunc
g_main_context_get_poll_func (GMainContext *context)
{
  GPollFunc result;
  
  if (!context)
    context = g_main_context_default ();
  
  g_return_val_if_fail (g_atomic_int_get (&context->ref_count) > 0, NULL);

  LOCK_CONTEXT (context);
  result = context->poll_func;
  UNLOCK_CONTEXT (context);

  return result;
}

/**
 * g_main_context_wakeup:
 * @context: a #GMainContext
 * 
 * If @context is currently blocking in g_main_context_iteration()
 * waiting for a source to become ready, cause it to stop blocking
 * and return.  Otherwise, cause the next invocation of
 * g_main_context_iteration() to return without blocking.
 *
 * This API is useful for low-level control over #GMainContext; for
 * example, integrating it with main loop implementations such as
 * #GMainLoop.
 *
 * Another related use for this function is when implementing a main
 * loop with a termination condition, computed from multiple threads:
 *
 * |[<!-- language="C" --> 
 *   #define NUM_TASKS 10
 *   static volatile gint tasks_remaining = NUM_TASKS;
 *   ...
 *  
 *   while (g_atomic_int_get (&tasks_remaining) != 0)
 *     g_main_context_iteration (NULL, TRUE);
 * ]|
 *  
 * Then in a thread:
 * |[<!-- language="C" --> 
 *   perform_work();
 *
 *   if (g_atomic_int_dec_and_test (&tasks_remaining))
 *     g_main_context_wakeup (NULL);
 * ]|
 **/
void
g_main_context_wakeup (GMainContext *context)
{
  if (!context)
    context = g_main_context_default ();

  g_return_if_fail (g_atomic_int_get (&context->ref_count) > 0);

  TRACE (GLIB_MAIN_CONTEXT_WAKEUP (context));

  g_wakeup_signal (context->wakeup);
}

/**
 * g_main_context_is_owner:
 * @context: a #GMainContext
 * 
 * Determines whether this thread holds the (recursive)
 * ownership of this #GMainContext. This is useful to
 * know before waiting on another thread that may be
 * blocking to get ownership of @context.
 *
 * Returns: %TRUE if current thread is owner of @context.
 *
 * Since: 2.10
 **/
gboolean
g_main_context_is_owner (GMainContext *context)
{
  gboolean is_owner;

  if (!context)
    context = g_main_context_default ();

  LOCK_CONTEXT (context);
  is_owner = context->owner == G_THREAD_SELF;
  UNLOCK_CONTEXT (context);

  return is_owner;
}

/* Timeouts */

static void
g_timeout_set_expiration (GTimeoutSource *timeout_source,
                          gint64          current_time)
{
  gint64 expiration;

  expiration = current_time + (guint64) timeout_source->interval * 1000;

  if (timeout_source->seconds)
    {
      gint64 remainder;
      static gint timer_perturb = -1;

      if (timer_perturb == -1)
        {
          /*
           * we want a per machine/session unique 'random' value; try the dbus
           * address first, that has a UUID in it. If there is no dbus, use the
           * hostname for hashing.
           */
          const char *session_bus_address = g_getenv ("DBUS_SESSION_BUS_ADDRESS");
          if (!session_bus_address)
            session_bus_address = g_getenv ("HOSTNAME");
          if (session_bus_address)
            timer_perturb = ABS ((gint) g_str_hash (session_bus_address)) % 1000000;
          else
            timer_perturb = 0;
        }

      /* We want the microseconds part of the timeout to land on the
       * 'timer_perturb' mark, but we need to make sure we don't try to
       * set the timeout in the past.  We do this by ensuring that we
       * always only *increase* the expiration time by adding a full
       * second in the case that the microsecond portion decreases.
       */
      expiration -= timer_perturb;

      remainder = expiration % 1000000;
      if (remainder >= 1000000/4)
        expiration += 1000000;

      expiration -= remainder;
      expiration += timer_perturb;
    }

  g_source_set_ready_time ((GSource *) timeout_source, expiration);
}

static gboolean
g_timeout_dispatch (GSource     *source,
                    GSourceFunc  callback,
                    gpointer     user_data)
{
  GTimeoutSource *timeout_source = (GTimeoutSource *)source;
  gboolean again;

  if (!callback)
    {
      g_warning ("Timeout source dispatched without callback\n"
                 "You must call g_source_set_callback().");
      return FALSE;
    }

  again = callback (user_data);

  TRACE (GLIB_TIMEOUT_DISPATCH (source, source->context, callback, user_data, again));

  if (again)
    g_timeout_set_expiration (timeout_source, g_source_get_time (source));

  return again;
}

/**
 * g_timeout_source_new:
 * @interval: the timeout interval in milliseconds.
 * 
 * Creates a new timeout source.
 *
 * The source will not initially be associated with any #GMainContext
 * and must be added to one with g_source_attach() before it will be
 * executed.
 *
 * The interval given is in terms of monotonic time, not wall clock
 * time.  See g_get_monotonic_time().
 * 
 * Returns: the newly-created timeout source
 **/
GSource *
g_timeout_source_new (guint interval)
{
  GSource *source = g_source_new (&g_timeout_funcs, sizeof (GTimeoutSource));
  GTimeoutSource *timeout_source = (GTimeoutSource *)source;

  timeout_source->interval = interval;
  g_timeout_set_expiration (timeout_source, g_get_monotonic_time ());

  return source;
}

/**
 * g_timeout_source_new_seconds:
 * @interval: the timeout interval in seconds
 *
 * Creates a new timeout source.
 *
 * The source will not initially be associated with any #GMainContext
 * and must be added to one with g_source_attach() before it will be
 * executed.
 *
 * The scheduling granularity/accuracy of this timeout source will be
 * in seconds.
 *
 * The interval given in terms of monotonic time, not wall clock time.
 * See g_get_monotonic_time().
 *
 * Returns: the newly-created timeout source
 *
 * Since: 2.14	
 **/
GSource *
g_timeout_source_new_seconds (guint interval)
{
  GSource *source = g_source_new (&g_timeout_funcs, sizeof (GTimeoutSource));
  GTimeoutSource *timeout_source = (GTimeoutSource *)source;

  timeout_source->interval = 1000 * interval;
  timeout_source->seconds = TRUE;

  g_timeout_set_expiration (timeout_source, g_get_monotonic_time ());

  return source;
}


/**
 * g_timeout_add_full: (rename-to g_timeout_add)
 * @priority: the priority of the timeout source. Typically this will be in
 *            the range between #G_PRIORITY_DEFAULT and #G_PRIORITY_HIGH.
 * @interval: the time between calls to the function, in milliseconds
 *             (1/1000ths of a second)
 * @function: function to call
 * @data:     data to pass to @function
 * @notify: (nullable): function to call when the timeout is removed, or %NULL
 * 
 * Sets a function to be called at regular intervals, with the given
 * priority.  The function is called repeatedly until it returns
 * %FALSE, at which point the timeout is automatically destroyed and
 * the function will not be called again.  The @notify function is
 * called when the timeout is destroyed.  The first call to the
 * function will be at the end of the first @interval.
 *
 * Note that timeout functions may be delayed, due to the processing of other
 * event sources. Thus they should not be relied on for precise timing.
 * After each call to the timeout function, the time of the next
 * timeout is recalculated based on the current time and the given interval
 * (it does not try to 'catch up' time lost in delays).
 *
 * See [memory management of sources][mainloop-memory-management] for details
 * on how to handle the return value and memory management of @data.
 *
 * This internally creates a main loop source using g_timeout_source_new()
 * and attaches it to the global #GMainContext using g_source_attach(), so
 * the callback will be invoked in whichever thread is running that main
 * context. You can do these steps manually if you need greater control or to
 * use a custom main context.
 *
 * The interval given in terms of monotonic time, not wall clock time.
 * See g_get_monotonic_time().
 * 
 * Returns: the ID (greater than 0) of the event source.
 **/
guint
g_timeout_add_full (gint           priority,
		    guint          interval,
		    GSourceFunc    function,
		    gpointer       data,
		    GDestroyNotify notify)
{
  GSource *source;
  guint id;
  
  g_return_val_if_fail (function != NULL, 0);

  source = g_timeout_source_new (interval);

  if (priority != G_PRIORITY_DEFAULT)
    g_source_set_priority (source, priority);

  g_source_set_callback (source, function, data, notify);
  id = g_source_attach (source, NULL);

  TRACE (GLIB_TIMEOUT_ADD (source, g_main_context_default (), id, priority, interval, function, data));

  g_source_unref (source);

  return id;
}

/**
 * g_timeout_add:
 * @interval: the time between calls to the function, in milliseconds
 *             (1/1000ths of a second)
 * @function: function to call
 * @data:     data to pass to @function
 * 
 * Sets a function to be called at regular intervals, with the default
 * priority, #G_PRIORITY_DEFAULT.  The function is called repeatedly
 * until it returns %FALSE, at which point the timeout is automatically
 * destroyed and the function will not be called again.  The first call
 * to the function will be at the end of the first @interval.
 *
 * Note that timeout functions may be delayed, due to the processing of other
 * event sources. Thus they should not be relied on for precise timing.
 * After each call to the timeout function, the time of the next
 * timeout is recalculated based on the current time and the given interval
 * (it does not try to 'catch up' time lost in delays).
 *
 * See [memory management of sources][mainloop-memory-management] for details
 * on how to handle the return value and memory management of @data.
 *
 * If you want to have a timer in the "seconds" range and do not care
 * about the exact time of the first call of the timer, use the
 * g_timeout_add_seconds() function; this function allows for more
 * optimizations and more efficient system power usage.
 *
 * This internally creates a main loop source using g_timeout_source_new()
 * and attaches it to the global #GMainContext using g_source_attach(), so
 * the callback will be invoked in whichever thread is running that main
 * context. You can do these steps manually if you need greater control or to
 * use a custom main context.
 * 
 * The interval given is in terms of monotonic time, not wall clock
 * time.  See g_get_monotonic_time().
 * 
 * Returns: the ID (greater than 0) of the event source.
 **/
guint
g_timeout_add (guint32        interval,
	       GSourceFunc    function,
	       gpointer       data)
{
  return g_timeout_add_full (G_PRIORITY_DEFAULT, 
			     interval, function, data, NULL);
}

/**
 * g_timeout_add_seconds_full: (rename-to g_timeout_add_seconds)
 * @priority: the priority of the timeout source. Typically this will be in
 *            the range between #G_PRIORITY_DEFAULT and #G_PRIORITY_HIGH.
 * @interval: the time between calls to the function, in seconds
 * @function: function to call
 * @data:     data to pass to @function
 * @notify: (nullable): function to call when the timeout is removed, or %NULL
 *
 * Sets a function to be called at regular intervals, with @priority.
 * The function is called repeatedly until it returns %FALSE, at which
 * point the timeout is automatically destroyed and the function will
 * not be called again.
 *
 * Unlike g_timeout_add(), this function operates at whole second granularity.
 * The initial starting point of the timer is determined by the implementation
 * and the implementation is expected to group multiple timers together so that
 * they fire all at the same time.
 * To allow this grouping, the @interval to the first timer is rounded
 * and can deviate up to one second from the specified interval.
 * Subsequent timer iterations will generally run at the specified interval.
 *
 * Note that timeout functions may be delayed, due to the processing of other
 * event sources. Thus they should not be relied on for precise timing.
 * After each call to the timeout function, the time of the next
 * timeout is recalculated based on the current time and the given @interval
 *
 * See [memory management of sources][mainloop-memory-management] for details
 * on how to handle the return value and memory management of @data.
 *
 * If you want timing more precise than whole seconds, use g_timeout_add()
 * instead.
 *
 * The grouping of timers to fire at the same time results in a more power
 * and CPU efficient behavior so if your timer is in multiples of seconds
 * and you don't require the first timer exactly one second from now, the
 * use of g_timeout_add_seconds() is preferred over g_timeout_add().
 *
 * This internally creates a main loop source using 
 * g_timeout_source_new_seconds() and attaches it to the main loop context 
 * using g_source_attach(). You can do these steps manually if you need 
 * greater control.
 * 
 * The interval given is in terms of monotonic time, not wall clock
 * time.  See g_get_monotonic_time().
 * 
 * Returns: the ID (greater than 0) of the event source.
 *
 * Since: 2.14
 **/
guint
g_timeout_add_seconds_full (gint           priority,
                            guint32        interval,
                            GSourceFunc    function,
                            gpointer       data,
                            GDestroyNotify notify)
{
  GSource *source;
  guint id;

  g_return_val_if_fail (function != NULL, 0);

  source = g_timeout_source_new_seconds (interval);

  if (priority != G_PRIORITY_DEFAULT)
    g_source_set_priority (source, priority);

  g_source_set_callback (source, function, data, notify);
  id = g_source_attach (source, NULL);
  g_source_unref (source);

  return id;
}

/**
 * g_timeout_add_seconds:
 * @interval: the time between calls to the function, in seconds
 * @function: function to call
 * @data: data to pass to @function
 *
 * Sets a function to be called at regular intervals with the default
 * priority, #G_PRIORITY_DEFAULT. The function is called repeatedly until
 * it returns %FALSE, at which point the timeout is automatically destroyed
 * and the function will not be called again.
 *
 * This internally creates a main loop source using
 * g_timeout_source_new_seconds() and attaches it to the main loop context
 * using g_source_attach(). You can do these steps manually if you need
 * greater control. Also see g_timeout_add_seconds_full().
 *
 * Note that the first call of the timer may not be precise for timeouts
 * of one second. If you need finer precision and have such a timeout,
 * you may want to use g_timeout_add() instead.
 *
 * See [memory management of sources][mainloop-memory-management] for details
 * on how to handle the return value and memory management of @data.
 *
 * The interval given is in terms of monotonic time, not wall clock
 * time.  See g_get_monotonic_time().
 * 
 * Returns: the ID (greater than 0) of the event source.
 *
 * Since: 2.14
 **/
guint
g_timeout_add_seconds (guint       interval,
                       GSourceFunc function,
                       gpointer    data)
{
  g_return_val_if_fail (function != NULL, 0);

  return g_timeout_add_seconds_full (G_PRIORITY_DEFAULT, interval, function, data, NULL);
}

/* Child watch functions */

#ifdef G_OS_WIN32

static gboolean
g_child_watch_prepare (GSource *source,
		       gint    *timeout)
{
  *timeout = -1;
  return FALSE;
}

static gboolean 
g_child_watch_check (GSource  *source)
{
  GChildWatchSource *child_watch_source;
  gboolean child_exited;

  child_watch_source = (GChildWatchSource *) source;

  child_exited = child_watch_source->poll.revents & G_IO_IN;

  if (child_exited)
    {
      DWORD child_status;

      /*
       * Note: We do _not_ check for the special value of STILL_ACTIVE
       * since we know that the process has exited and doing so runs into
       * problems if the child process "happens to return STILL_ACTIVE(259)"
       * as Microsoft's Platform SDK puts it.
       */
      if (!GetExitCodeProcess (child_watch_source->pid, &child_status))
        {
	  gchar *emsg = g_win32_error_message (GetLastError ());
	  g_warning (G_STRLOC ": GetExitCodeProcess() failed: %s", emsg);
	  g_free (emsg);

	  child_watch_source->child_status = -1;
	}
      else
	child_watch_source->child_status = child_status;
    }

  return child_exited;
}

static void
g_child_watch_finalize (GSource *source)
{
}

#else /* G_OS_WIN32 */

static void
wake_source (GSource *source)
{
  GMainContext *context;

  /* This should be thread-safe:
   *
   *  - if the source is currently being added to a context, that
   *    context will be woken up anyway
   *
   *  - if the source is currently being destroyed, we simply need not
   *    to crash:
   *
   *    - the memory for the source will remain valid until after the
   *      source finalize function was called (which would remove the
   *      source from the global list which we are currently holding the
   *      lock for)
   *
   *    - the GMainContext will either be NULL or point to a live
   *      GMainContext
   *
   *    - the GMainContext will remain valid since we hold the
   *      main_context_list lock
   *
   *  Since we are holding a lot of locks here, don't try to enter any
   *  more GMainContext functions for fear of dealock -- just hit the
   *  GWakeup and run.  Even if that's safe now, it could easily become
   *  unsafe with some very minor changes in the future, and signal
   *  handling is not the most well-tested codepath.
   */
  G_LOCK(main_context_list);
  context = source->context;
  if (context)
    g_wakeup_signal (context->wakeup);
  G_UNLOCK(main_context_list);
}

static void
dispatch_unix_signals_unlocked (void)
{
  gboolean pending[NSIG];
  GSList *node;
  gint i;

  /* clear this first in case another one arrives while we're processing */
  any_unix_signal_pending = FALSE;

  /* We atomically test/clear the bit from the global array in case
   * other signals arrive while we are dispatching.
   *
   * We then can safely use our own array below without worrying about
   * races.
   */
  for (i = 0; i < NSIG; i++)
    {
      /* Be very careful with (the volatile) unix_signal_pending.
       *
       * We must ensure that it's not possible that we clear it without
       * handling the signal.  We therefore must ensure that our pending
       * array has a field set (ie: we will do something about the
       * signal) before we clear the item in unix_signal_pending.
       *
       * Note specifically: we must check _our_ array.
       */
      pending[i] = unix_signal_pending[i];
      if (pending[i])
        unix_signal_pending[i] = FALSE;
    }

  /* handle GChildWatchSource instances */
  if (pending[SIGCHLD])
    {
      /* The only way we can do this is to scan all of the children.
       *
       * The docs promise that we will not reap children that we are not
       * explicitly watching, so that ties our hands from calling
       * waitpid(-1).  We also can't use siginfo's si_pid field since if
       * multiple SIGCHLD arrive at the same time, one of them can be
       * dropped (since a given UNIX signal can only be pending once).
       */
      for (node = unix_child_watches; node; node = node->next)
        {
          GChildWatchSource *source = node->data;

          if (!source->child_exited)
            {
              pid_t pid;
              do
                {
                  g_assert (source->pid > 0);

                  pid = waitpid (source->pid, &source->child_status, WNOHANG);
                  if (pid > 0)
                    {
                      source->child_exited = TRUE;
                      wake_source ((GSource *) source);
                    }
                  else if (pid == -1 && errno == ECHILD)
                    {
                      g_warning ("GChildWatchSource: Exit status of a child process was requested but ECHILD was received by waitpid(). See the documentation of g_child_watch_source_new() for possible causes.");
                      source->child_exited = TRUE;
                      source->child_status = 0;
                      wake_source ((GSource *) source);
                    }
                }
              while (pid == -1 && errno == EINTR);
            }
        }
    }

  /* handle GUnixSignalWatchSource instances */
  for (node = unix_signal_watches; node; node = node->next)
    {
      GUnixSignalWatchSource *source = node->data;

      if (!source->pending)
        {
          if (pending[source->signum])
            {
              source->pending = TRUE;

              wake_source ((GSource *) source);
            }
        }
    }

}

static void
dispatch_unix_signals (void)
{
  G_LOCK(unix_signal_lock);
  dispatch_unix_signals_unlocked ();
  G_UNLOCK(unix_signal_lock);
}

static gboolean
g_child_watch_prepare (GSource *source,
		       gint    *timeout)
{
  GChildWatchSource *child_watch_source;

  child_watch_source = (GChildWatchSource *) source;

  return child_watch_source->child_exited;
}

static gboolean
g_child_watch_check (GSource *source)
{
  GChildWatchSource *child_watch_source;

  child_watch_source = (GChildWatchSource *) source;

  return child_watch_source->child_exited;
}

static gboolean
g_unix_signal_watch_prepare (GSource *source,
			     gint    *timeout)
{
  GUnixSignalWatchSource *unix_signal_source;

  unix_signal_source = (GUnixSignalWatchSource *) source;

  return unix_signal_source->pending;
}

static gboolean
g_unix_signal_watch_check (GSource  *source)
{
  GUnixSignalWatchSource *unix_signal_source;

  unix_signal_source = (GUnixSignalWatchSource *) source;

  return unix_signal_source->pending;
}

static gboolean
g_unix_signal_watch_dispatch (GSource    *source, 
			      GSourceFunc callback,
			      gpointer    user_data)
{
  GUnixSignalWatchSource *unix_signal_source;
  gboolean again;

  unix_signal_source = (GUnixSignalWatchSource *) source;

  if (!callback)
    {
      g_warning ("Unix signal source dispatched without callback\n"
		 "You must call g_source_set_callback().");
      return FALSE;
    }

  again = (callback) (user_data);

  unix_signal_source->pending = FALSE;

  return again;
}

static void
ref_unix_signal_handler_unlocked (int signum)
{
  /* Ensure we have the worker context */
  g_get_worker_context ();
  unix_signal_refcount[signum]++;
  if (unix_signal_refcount[signum] == 1)
    {
      struct sigaction action;
      action.sa_handler = g_unix_signal_handler;
      sigemptyset (&action.sa_mask);
#ifdef SA_RESTART
      action.sa_flags = SA_RESTART | SA_NOCLDSTOP;
#else
      action.sa_flags = SA_NOCLDSTOP;
#endif
      sigaction (signum, &action, NULL);
    }
}

static void
unref_unix_signal_handler_unlocked (int signum)
{
  unix_signal_refcount[signum]--;
  if (unix_signal_refcount[signum] == 0)
    {
      struct sigaction action;
      memset (&action, 0, sizeof (action));
      action.sa_handler = SIG_DFL;
      sigemptyset (&action.sa_mask);
      sigaction (signum, &action, NULL);
    }
}

GSource *
_g_main_create_unix_signal_watch (int signum)
{
  GSource *source;
  GUnixSignalWatchSource *unix_signal_source;

  source = g_source_new (&g_unix_signal_funcs, sizeof (GUnixSignalWatchSource));
  unix_signal_source = (GUnixSignalWatchSource *) source;

  unix_signal_source->signum = signum;
  unix_signal_source->pending = FALSE;

  G_LOCK (unix_signal_lock);
  ref_unix_signal_handler_unlocked (signum);
  unix_signal_watches = g_slist_prepend (unix_signal_watches, unix_signal_source);
  dispatch_unix_signals_unlocked ();
  G_UNLOCK (unix_signal_lock);

  return source;
}

static void
g_unix_signal_watch_finalize (GSource    *source)
{
  GUnixSignalWatchSource *unix_signal_source;

  unix_signal_source = (GUnixSignalWatchSource *) source;

  G_LOCK (unix_signal_lock);
  unref_unix_signal_handler_unlocked (unix_signal_source->signum);
  unix_signal_watches = g_slist_remove (unix_signal_watches, source);
  G_UNLOCK (unix_signal_lock);
}

static void
g_child_watch_finalize (GSource *source)
{
  G_LOCK (unix_signal_lock);
  unix_child_watches = g_slist_remove (unix_child_watches, source);
  unref_unix_signal_handler_unlocked (SIGCHLD);
  G_UNLOCK (unix_signal_lock);
}

#endif /* G_OS_WIN32 */

static gboolean
g_child_watch_dispatch (GSource    *source, 
			GSourceFunc callback,
			gpointer    user_data)
{
  GChildWatchSource *child_watch_source;
  GChildWatchFunc child_watch_callback = (GChildWatchFunc) callback;

  child_watch_source = (GChildWatchSource *) source;

  if (!callback)
    {
      g_warning ("Child watch source dispatched without callback\n"
		 "You must call g_source_set_callback().");
      return FALSE;
    }

  (child_watch_callback) (child_watch_source->pid, child_watch_source->child_status, user_data);

  /* We never keep a child watch source around as the child is gone */
  return FALSE;
}

#ifndef G_OS_WIN32

static void
g_unix_signal_handler (int signum)
{
  gint saved_errno = errno;

  unix_signal_pending[signum] = TRUE;
  any_unix_signal_pending = TRUE;

  g_wakeup_signal (glib_worker_context->wakeup);

  errno = saved_errno;
}

#endif /* !G_OS_WIN32 */

/**
 * g_child_watch_source_new:
 * @pid: process to watch. On POSIX the positive pid of a child process. On
 * Windows a handle for a process (which doesn't have to be a child).
 * 
 * Creates a new child_watch source.
 *
 * The source will not initially be associated with any #GMainContext
 * and must be added to one with g_source_attach() before it will be
 * executed.
 * 
 * Note that child watch sources can only be used in conjunction with
 * `g_spawn...` when the %G_SPAWN_DO_NOT_REAP_CHILD flag is used.
 *
 * Note that on platforms where #GPid must be explicitly closed
 * (see g_spawn_close_pid()) @pid must not be closed while the
 * source is still active. Typically, you will want to call
 * g_spawn_close_pid() in the callback function for the source.
 *
 * On POSIX platforms, the following restrictions apply to this API
 * due to limitations in POSIX process interfaces:
 *
 * * @pid must be a child of this process
 * * @pid must be positive
 * * the application must not call `waitpid` with a non-positive
 *   first argument, for instance in another thread
 * * the application must not wait for @pid to exit by any other
 *   mechanism, including `waitpid(pid, ...)` or a second child-watch
 *   source for the same @pid
 * * the application must not ignore SIGCHILD
 *
 * If any of those conditions are not met, this and related APIs will
 * not work correctly. This can often be diagnosed via a GLib warning
 * stating that `ECHILD` was received by `waitpid`.
 *
 * Calling `waitpid` for specific processes other than @pid remains a
 * valid thing to do.
 *
 * Returns: the newly-created child watch source
 *
 * Since: 2.4
 **/
GSource *
g_child_watch_source_new (GPid pid)
{
  GSource *source;
  GChildWatchSource *child_watch_source;

#ifndef G_OS_WIN32
  g_return_val_if_fail (pid > 0, NULL);
#endif

  source = g_source_new (&g_child_watch_funcs, sizeof (GChildWatchSource));
  child_watch_source = (GChildWatchSource *)source;

  child_watch_source->pid = pid;

#ifdef G_OS_WIN32
  child_watch_source->poll.fd = (gintptr) pid;
  child_watch_source->poll.events = G_IO_IN;

  g_source_add_poll (source, &child_watch_source->poll);
#else /* G_OS_WIN32 */
  G_LOCK (unix_signal_lock);
  ref_unix_signal_handler_unlocked (SIGCHLD);
  unix_child_watches = g_slist_prepend (unix_child_watches, child_watch_source);
  if (waitpid (pid, &child_watch_source->child_status, WNOHANG) > 0)
    child_watch_source->child_exited = TRUE;
  G_UNLOCK (unix_signal_lock);
#endif /* G_OS_WIN32 */

  return source;
}

/**
 * g_child_watch_add_full: (rename-to g_child_watch_add)
 * @priority: the priority of the idle source. Typically this will be in the
 *            range between #G_PRIORITY_DEFAULT_IDLE and #G_PRIORITY_HIGH_IDLE.
 * @pid:      process to watch. On POSIX the positive pid of a child process. On
 * Windows a handle for a process (which doesn't have to be a child).
 * @function: function to call
 * @data:     data to pass to @function
 * @notify: (nullable): function to call when the idle is removed, or %NULL
 * 
 * Sets a function to be called when the child indicated by @pid 
 * exits, at the priority @priority.
 *
 * If you obtain @pid from g_spawn_async() or g_spawn_async_with_pipes() 
 * you will need to pass #G_SPAWN_DO_NOT_REAP_CHILD as flag to 
 * the spawn function for the child watching to work.
 *
 * In many programs, you will want to call g_spawn_check_exit_status()
 * in the callback to determine whether or not the child exited
 * successfully.
 * 
 * Also, note that on platforms where #GPid must be explicitly closed
 * (see g_spawn_close_pid()) @pid must not be closed while the source
 * is still active.  Typically, you should invoke g_spawn_close_pid()
 * in the callback function for the source.
 * 
 * GLib supports only a single callback per process id.
 * On POSIX platforms, the same restrictions mentioned for
 * g_child_watch_source_new() apply to this function.
 *
 * This internally creates a main loop source using 
 * g_child_watch_source_new() and attaches it to the main loop context 
 * using g_source_attach(). You can do these steps manually if you 
 * need greater control.
 *
 * Returns: the ID (greater than 0) of the event source.
 *
 * Since: 2.4
 **/
guint
g_child_watch_add_full (gint            priority,
			GPid            pid,
			GChildWatchFunc function,
			gpointer        data,
			GDestroyNotify  notify)
{
  GSource *source;
  guint id;
  
  g_return_val_if_fail (function != NULL, 0);
#ifndef G_OS_WIN32
  g_return_val_if_fail (pid > 0, 0);
#endif

  source = g_child_watch_source_new (pid);

  if (priority != G_PRIORITY_DEFAULT)
    g_source_set_priority (source, priority);

  g_source_set_callback (source, (GSourceFunc) function, data, notify);
  id = g_source_attach (source, NULL);
  g_source_unref (source);

  return id;
}

/**
 * g_child_watch_add:
 * @pid:      process id to watch. On POSIX the positive pid of a child
 * process. On Windows a handle for a process (which doesn't have to be
 * a child).
 * @function: function to call
 * @data:     data to pass to @function
 * 
 * Sets a function to be called when the child indicated by @pid 
 * exits, at a default priority, #G_PRIORITY_DEFAULT.
 * 
 * If you obtain @pid from g_spawn_async() or g_spawn_async_with_pipes() 
 * you will need to pass #G_SPAWN_DO_NOT_REAP_CHILD as flag to 
 * the spawn function for the child watching to work.
 * 
 * Note that on platforms where #GPid must be explicitly closed
 * (see g_spawn_close_pid()) @pid must not be closed while the
 * source is still active. Typically, you will want to call
 * g_spawn_close_pid() in the callback function for the source.
 *
 * GLib supports only a single callback per process id.
 * On POSIX platforms, the same restrictions mentioned for
 * g_child_watch_source_new() apply to this function.
 *
 * This internally creates a main loop source using 
 * g_child_watch_source_new() and attaches it to the main loop context 
 * using g_source_attach(). You can do these steps manually if you 
 * need greater control.
 *
 * Returns: the ID (greater than 0) of the event source.
 *
 * Since: 2.4
 **/
guint 
g_child_watch_add (GPid            pid,
		   GChildWatchFunc function,
		   gpointer        data)
{
  return g_child_watch_add_full (G_PRIORITY_DEFAULT, pid, function, data, NULL);
}


/* Idle functions */

static gboolean 
g_idle_prepare  (GSource  *source,
		 gint     *timeout)
{
  *timeout = 0;

  return TRUE;
}

static gboolean 
g_idle_check    (GSource  *source)
{
  return TRUE;
}

static gboolean
g_idle_dispatch (GSource    *source, 
		 GSourceFunc callback,
		 gpointer    user_data)
{
  gboolean again;

  if (!callback)
    {
      g_warning ("Idle source dispatched without callback\n"
		 "You must call g_source_set_callback().");
      return FALSE;
    }

  again = callback (user_data);

  TRACE (GLIB_IDLE_DISPATCH (source, source->context, callback, user_data, again));

  return again;
}

/**
 * g_idle_source_new:
 * 
 * Creates a new idle source.
 *
 * The source will not initially be associated with any #GMainContext
 * and must be added to one with g_source_attach() before it will be
 * executed. Note that the default priority for idle sources is
 * %G_PRIORITY_DEFAULT_IDLE, as compared to other sources which
 * have a default priority of %G_PRIORITY_DEFAULT.
 * 
 * Returns: the newly-created idle source
 **/
GSource *
g_idle_source_new (void)
{
  GSource *source;

  source = g_source_new (&g_idle_funcs, sizeof (GSource));
  g_source_set_priority (source, G_PRIORITY_DEFAULT_IDLE);

  return source;
}

/**
 * g_idle_add_full: (rename-to g_idle_add)
 * @priority: the priority of the idle source. Typically this will be in the
 *            range between #G_PRIORITY_DEFAULT_IDLE and #G_PRIORITY_HIGH_IDLE.
 * @function: function to call
 * @data:     data to pass to @function
 * @notify: (nullable): function to call when the idle is removed, or %NULL
 * 
 * Adds a function to be called whenever there are no higher priority
 * events pending.  If the function returns %FALSE it is automatically
 * removed from the list of event sources and will not be called again.
 *
 * See [memory management of sources][mainloop-memory-management] for details
 * on how to handle the return value and memory management of @data.
 * 
 * This internally creates a main loop source using g_idle_source_new()
 * and attaches it to the global #GMainContext using g_source_attach(), so
 * the callback will be invoked in whichever thread is running that main
 * context. You can do these steps manually if you need greater control or to
 * use a custom main context.
 * 
 * Returns: the ID (greater than 0) of the event source.
 **/
guint 
g_idle_add_full (gint           priority,
		 GSourceFunc    function,
		 gpointer       data,
		 GDestroyNotify notify)
{
  GSource *source;
  guint id;
  
  g_return_val_if_fail (function != NULL, 0);

  source = g_idle_source_new ();

  if (priority != G_PRIORITY_DEFAULT_IDLE)
    g_source_set_priority (source, priority);

  g_source_set_callback (source, function, data, notify);
  id = g_source_attach (source, NULL);

  TRACE (GLIB_IDLE_ADD (source, g_main_context_default (), id, priority, function, data));

  g_source_unref (source);

  return id;
}

/**
 * g_idle_add:
 * @function: function to call 
 * @data: data to pass to @function.
 * 
 * Adds a function to be called whenever there are no higher priority
 * events pending to the default main loop. The function is given the
 * default idle priority, #G_PRIORITY_DEFAULT_IDLE.  If the function
 * returns %FALSE it is automatically removed from the list of event
 * sources and will not be called again.
 *
 * See [memory management of sources][mainloop-memory-management] for details
 * on how to handle the return value and memory management of @data.
 * 
 * This internally creates a main loop source using g_idle_source_new()
 * and attaches it to the global #GMainContext using g_source_attach(), so
 * the callback will be invoked in whichever thread is running that main
 * context. You can do these steps manually if you need greater control or to
 * use a custom main context.
 * 
 * Returns: the ID (greater than 0) of the event source.
 **/
guint 
g_idle_add (GSourceFunc    function,
	    gpointer       data)
{
  return g_idle_add_full (G_PRIORITY_DEFAULT_IDLE, function, data, NULL);
}

/**
 * g_idle_remove_by_data:
 * @data: the data for the idle source's callback.
 * 
 * Removes the idle function with the given data.
 * 
 * Returns: %TRUE if an idle source was found and removed.
 **/
gboolean
g_idle_remove_by_data (gpointer data)
{
  return g_source_remove_by_funcs_user_data (&g_idle_funcs, data);
}

/**
 * g_main_context_invoke:
 * @context: (nullable): a #GMainContext, or %NULL
 * @function: function to call
 * @data: data to pass to @function
 *
 * Invokes a function in such a way that @context is owned during the
 * invocation of @function.
 *
 * If @context is %NULL then the global default main context — as
 * returned by g_main_context_default() — is used.
 *
 * If @context is owned by the current thread, @function is called
 * directly.  Otherwise, if @context is the thread-default main context
 * of the current thread and g_main_context_acquire() succeeds, then
 * @function is called and g_main_context_release() is called
 * afterwards.
 *
 * In any other case, an idle source is created to call @function and
 * that source is attached to @context (presumably to be run in another
 * thread).  The idle source is attached with #G_PRIORITY_DEFAULT
 * priority.  If you want a different priority, use
 * g_main_context_invoke_full().
 *
 * Note that, as with normal idle functions, @function should probably
 * return %FALSE.  If it returns %TRUE, it will be continuously run in a
 * loop (and may prevent this call from returning).
 *
 * Since: 2.28
 **/
void
g_main_context_invoke (GMainContext *context,
                       GSourceFunc   function,
                       gpointer      data)
{
  g_main_context_invoke_full (context,
                              G_PRIORITY_DEFAULT,
                              function, data, NULL);
}

/**
 * g_main_context_invoke_full:
 * @context: (nullable): a #GMainContext, or %NULL
 * @priority: the priority at which to run @function
 * @function: function to call
 * @data: data to pass to @function
 * @notify: (nullable): a function to call when @data is no longer in use, or %NULL.
 *
 * Invokes a function in such a way that @context is owned during the
 * invocation of @function.
 *
 * This function is the same as g_main_context_invoke() except that it
 * lets you specify the priority in case @function ends up being
 * scheduled as an idle and also lets you give a #GDestroyNotify for @data.
 *
 * @notify should not assume that it is called from any particular
 * thread or with any particular context acquired.
 *
 * Since: 2.28
 **/
void
g_main_context_invoke_full (GMainContext   *context,
                            gint            priority,
                            GSourceFunc     function,
                            gpointer        data,
                            GDestroyNotify  notify)
{
  g_return_if_fail (function != NULL);

  if (!context)
    context = g_main_context_default ();

  if (g_main_context_is_owner (context))
    {
      while (function (data));
      if (notify != NULL)
        notify (data);
    }

  else
    {
      GMainContext *thread_default;

      thread_default = g_main_context_get_thread_default ();

      if (!thread_default)
        thread_default = g_main_context_default ();

      if (thread_default == context && g_main_context_acquire (context))
        {
          while (function (data));

          g_main_context_release (context);

          if (notify != NULL)
            notify (data);
        }
      else
        {
          GSource *source;

          source = g_idle_source_new ();
          g_source_set_priority (source, priority);
          g_source_set_callback (source, function, data, notify);
          g_source_attach (source, context);
          g_source_unref (source);
        }
    }
}

static gpointer
glib_worker_main (gpointer data)
{
  while (TRUE)
    {
      g_main_context_iteration (glib_worker_context, TRUE);

#ifdef G_OS_UNIX
      if (any_unix_signal_pending)
        dispatch_unix_signals ();
#endif
    }

  return NULL; /* worst GCC warning message ever... */
}

GMainContext *
g_get_worker_context (void)
{
  static gsize initialised;

  if (g_once_init_enter (&initialised))
    {
      /* mask all signals in the worker thread */
#ifdef G_OS_UNIX
      sigset_t prev_mask;
      sigset_t all;

      sigfillset (&all);
      pthread_sigmask (SIG_SETMASK, &all, &prev_mask);
#endif
      glib_worker_context = g_main_context_new ();
      g_thread_new ("gmain", glib_worker_main, NULL);
#ifdef G_OS_UNIX
      pthread_sigmask (SIG_SETMASK, &prev_mask, NULL);
#endif
      g_once_init_leave (&initialised, TRUE);
    }

  return glib_worker_context;
}
