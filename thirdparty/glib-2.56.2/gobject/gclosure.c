/* GObject - GLib Type, Object, Parameter and Signal Library
 * Copyright (C) 2000-2001 Red Hat, Inc.
 * Copyright (C) 2005 Imendio AB
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

/*
 * MT safe with regards to reference counting.
 */

#include "config.h"

#include "../glib/valgrind.h"
#include <string.h>

#include <ffi.h>

#include "gclosure.h"
#include "gboxed.h"
#include "gobject.h"
#include "genums.h"
#include "gvalue.h"
#include "gvaluetypes.h"
#include "gtype-private.h"


/**
 * SECTION:gclosure
 * @short_description: Functions as first-class objects
 * @title: Closures
 *
 * A #GClosure represents a callback supplied by the programmer. It
 * will generally comprise a function of some kind and a marshaller
 * used to call it. It is the responsibility of the marshaller to
 * convert the arguments for the invocation from #GValues into
 * a suitable form, perform the callback on the converted arguments,
 * and transform the return value back into a #GValue.
 *
 * In the case of C programs, a closure usually just holds a pointer
 * to a function and maybe a data argument, and the marshaller
 * converts between #GValue and native C types. The GObject
 * library provides the #GCClosure type for this purpose. Bindings for
 * other languages need marshallers which convert between #GValues
 * and suitable representations in the runtime of the language in
 * order to use functions written in that languages as callbacks.
 *
 * Within GObject, closures play an important role in the
 * implementation of signals. When a signal is registered, the
 * @c_marshaller argument to g_signal_new() specifies the default C
 * marshaller for any closure which is connected to this
 * signal. GObject provides a number of C marshallers for this
 * purpose, see the g_cclosure_marshal_*() functions. Additional C
 * marshallers can be generated with the [glib-genmarshal][glib-genmarshal]
 * utility.  Closures can be explicitly connected to signals with
 * g_signal_connect_closure(), but it usually more convenient to let
 * GObject create a closure automatically by using one of the
 * g_signal_connect_*() functions which take a callback function/user
 * data pair.
 *
 * Using closures has a number of important advantages over a simple
 * callback function/data pointer combination:
 * 
 * - Closures allow the callee to get the types of the callback parameters,
 *   which means that language bindings don't have to write individual glue
 *   for each callback type.
 *
 * - The reference counting of #GClosure makes it easy to handle reentrancy
 *   right; if a callback is removed while it is being invoked, the closure
 *   and its parameters won't be freed until the invocation finishes.
 *
 * - g_closure_invalidate() and invalidation notifiers allow callbacks to be
 *   automatically removed when the objects they point to go away.
 */

#define	CLOSURE_MAX_REF_COUNT		((1 << 15) - 1)
#define	CLOSURE_MAX_N_GUARDS		((1 << 1) - 1)
#define	CLOSURE_MAX_N_FNOTIFIERS	((1 << 2) - 1)
#define	CLOSURE_MAX_N_INOTIFIERS	((1 << 8) - 1)
#define	CLOSURE_N_MFUNCS(cl)		(((cl)->n_guards << 1L))
/* same as G_CLOSURE_N_NOTIFIERS() (keep in sync) */
#define	CLOSURE_N_NOTIFIERS(cl)		(CLOSURE_N_MFUNCS (cl) + \
                                         (cl)->n_fnotifiers + \
                                         (cl)->n_inotifiers)

typedef union {
  GClosure closure;
  volatile gint vint;
} ClosureInt;

#define CHANGE_FIELD(_closure, _field, _OP, _value, _must_set, _SET_OLD, _SET_NEW)      \
G_STMT_START {                                                                          \
  ClosureInt *cunion = (ClosureInt*) _closure;                 		                \
  gint new_int, old_int, success;                              		                \
  do                                                    		                \
    {                                                   		                \
      ClosureInt tmp;                                   		                \
      tmp.vint = old_int = cunion->vint;                		                \
      _SET_OLD tmp.closure._field;                                                      \
      tmp.closure._field _OP _value;                      		                \
      _SET_NEW tmp.closure._field;                                                      \
      new_int = tmp.vint;                               		                \
      success = g_atomic_int_compare_and_exchange (&cunion->vint, old_int, new_int);    \
    }                                                   		                \
  while (!success && _must_set);                                                        \
} G_STMT_END

#define SWAP(_closure, _field, _value, _oldv)   CHANGE_FIELD (_closure, _field, =, _value, TRUE, *(_oldv) =,     (void) )
#define SET(_closure, _field, _value)           CHANGE_FIELD (_closure, _field, =, _value, TRUE,     (void),     (void) )
#define INC(_closure, _field)                   CHANGE_FIELD (_closure, _field, +=,     1, TRUE,     (void),     (void) )
#define INC_ASSIGN(_closure, _field, _newv)     CHANGE_FIELD (_closure, _field, +=,     1, TRUE,     (void), *(_newv) = )
#define DEC(_closure, _field)                   CHANGE_FIELD (_closure, _field, -=,     1, TRUE,     (void),     (void) )
#define DEC_ASSIGN(_closure, _field, _newv)     CHANGE_FIELD (_closure, _field, -=,     1, TRUE,     (void), *(_newv) = )

#if 0   /* for non-thread-safe closures */
#define SWAP(cl,f,v,o)     (void) (*(o) = cl->f, cl->f = v)
#define SET(cl,f,v)        (void) (cl->f = v)
#define INC(cl,f)          (void) (cl->f += 1)
#define INC_ASSIGN(cl,f,n) (void) (cl->f += 1, *(n) = cl->f)
#define DEC(cl,f)          (void) (cl->f -= 1)
#define DEC_ASSIGN(cl,f,n) (void) (cl->f -= 1, *(n) = cl->f)
#endif

enum {
  FNOTIFY,
  INOTIFY,
  PRE_NOTIFY,
  POST_NOTIFY
};


/* --- functions --- */
/**
 * g_closure_new_simple:
 * @sizeof_closure: the size of the structure to allocate, must be at least
 *                  `sizeof (GClosure)`
 * @data: data to store in the @data field of the newly allocated #GClosure
 *
 * Allocates a struct of the given size and initializes the initial
 * part as a #GClosure. This function is mainly useful when
 * implementing new types of closures.
 *
 * |[<!-- language="C" --> 
 * typedef struct _MyClosure MyClosure;
 * struct _MyClosure
 * {
 *   GClosure closure;
 *   // extra data goes here
 * };
 *
 * static void
 * my_closure_finalize (gpointer  notify_data,
 *                      GClosure *closure)
 * {
 *   MyClosure *my_closure = (MyClosure *)closure;
 *
 *   // free extra data here
 * }
 *
 * MyClosure *my_closure_new (gpointer data)
 * {
 *   GClosure *closure;
 *   MyClosure *my_closure;
 *
 *   closure = g_closure_new_simple (sizeof (MyClosure), data);
 *   my_closure = (MyClosure *) closure;
 *
 *   // initialize extra data here
 *
 *   g_closure_add_finalize_notifier (closure, notify_data,
 *                                    my_closure_finalize);
 *   return my_closure;
 * }
 * ]|
 *
 * Returns: (transfer none): a floating reference to a new #GClosure
 */
GClosure*
g_closure_new_simple (guint           sizeof_closure,
		      gpointer        data)
{
  GClosure *closure;
  gint private_size;
  gchar *allocated;

  g_return_val_if_fail (sizeof_closure >= sizeof (GClosure), NULL);

  private_size = sizeof (GRealClosure) - sizeof (GClosure);

  /* See comments in gtype.c about what's going on here... */
  if (RUNNING_ON_VALGRIND)
    {
      private_size += sizeof (gpointer);

      allocated = g_malloc0 (private_size + sizeof_closure + sizeof (gpointer));

      *(gpointer *) (allocated + private_size + sizeof_closure) = allocated + sizeof (gpointer);

      VALGRIND_MALLOCLIKE_BLOCK (allocated + private_size, sizeof_closure + sizeof (gpointer), 0, TRUE);
      VALGRIND_MALLOCLIKE_BLOCK (allocated + sizeof (gpointer), private_size - sizeof (gpointer), 0, TRUE);
    }
  else
    allocated = g_malloc0 (private_size + sizeof_closure);

  closure = (GClosure *) (allocated + private_size);

  SET (closure, ref_count, 1);
  SET (closure, floating, TRUE);
  closure->data = data;

  return closure;
}

static inline void
closure_invoke_notifiers (GClosure *closure,
			  guint     notify_type)
{
  /* notifier layout:
   *     n_guards    n_guards     n_fnotif.  n_inotifiers
   * ->[[pre_guards][post_guards][fnotifiers][inotifiers]]
   *
   * CLOSURE_N_MFUNCS(cl)    = n_guards + n_guards;
   * CLOSURE_N_NOTIFIERS(cl) = CLOSURE_N_MFUNCS(cl) + n_fnotifiers + n_inotifiers
   *
   * constrains/catches:
   * - closure->notifiers may be reloacted during callback
   * - closure->n_fnotifiers and closure->n_inotifiers may change during callback
   * - i.e. callbacks can be removed/added during invocation
   * - must prepare for callback removal during FNOTIFY and INOTIFY (done via ->marshal= & ->data=)
   * - must distinguish (->marshal= & ->data=) for INOTIFY vs. FNOTIFY (via ->in_inotify)
   * + closure->n_guards is const during PRE_NOTIFY & POST_NOTIFY
   * + none of the callbacks can cause recursion
   * + closure->n_inotifiers is const 0 during FNOTIFY
   */
  switch (notify_type)
    {
      GClosureNotifyData *ndata;
      guint i, offs;
    case FNOTIFY:
      while (closure->n_fnotifiers)
	{
          guint n;
	  DEC_ASSIGN (closure, n_fnotifiers, &n);

	  ndata = closure->notifiers + CLOSURE_N_MFUNCS (closure) + n;
	  closure->marshal = (GClosureMarshal) ndata->notify;
	  closure->data = ndata->data;
	  ndata->notify (ndata->data, closure);
	}
      closure->marshal = NULL;
      closure->data = NULL;
      break;
    case INOTIFY:
      SET (closure, in_inotify, TRUE);
      while (closure->n_inotifiers)
	{
          guint n;
          DEC_ASSIGN (closure, n_inotifiers, &n);

	  ndata = closure->notifiers + CLOSURE_N_MFUNCS (closure) + closure->n_fnotifiers + n;
	  closure->marshal = (GClosureMarshal) ndata->notify;
	  closure->data = ndata->data;
	  ndata->notify (ndata->data, closure);
	}
      closure->marshal = NULL;
      closure->data = NULL;
      SET (closure, in_inotify, FALSE);
      break;
    case PRE_NOTIFY:
      i = closure->n_guards;
      offs = 0;
      while (i--)
	{
	  ndata = closure->notifiers + offs + i;
	  ndata->notify (ndata->data, closure);
	}
      break;
    case POST_NOTIFY:
      i = closure->n_guards;
      offs = i;
      while (i--)
	{
	  ndata = closure->notifiers + offs + i;
	  ndata->notify (ndata->data, closure);
	}
      break;
    }
}

static void
g_closure_set_meta_va_marshal (GClosure       *closure,
			       GVaClosureMarshal va_meta_marshal)
{
  GRealClosure *real_closure;

  g_return_if_fail (closure != NULL);
  g_return_if_fail (va_meta_marshal != NULL);
  g_return_if_fail (closure->is_invalid == FALSE);
  g_return_if_fail (closure->in_marshal == FALSE);

  real_closure = G_REAL_CLOSURE (closure);

  g_return_if_fail (real_closure->meta_marshal != NULL);

  real_closure->va_meta_marshal = va_meta_marshal;
}

/**
 * g_closure_set_meta_marshal: (skip)
 * @closure: a #GClosure
 * @marshal_data: (closure meta_marshal): context-dependent data to pass
 *  to @meta_marshal
 * @meta_marshal: a #GClosureMarshal function
 *
 * Sets the meta marshaller of @closure.  A meta marshaller wraps
 * @closure->marshal and modifies the way it is called in some
 * fashion. The most common use of this facility is for C callbacks.
 * The same marshallers (generated by [glib-genmarshal][glib-genmarshal]),
 * are used everywhere, but the way that we get the callback function
 * differs. In most cases we want to use @closure->callback, but in
 * other cases we want to use some different technique to retrieve the
 * callback function.
 *
 * For example, class closures for signals (see
 * g_signal_type_cclosure_new()) retrieve the callback function from a
 * fixed offset in the class structure.  The meta marshaller retrieves
 * the right callback and passes it to the marshaller as the
 * @marshal_data argument.
 */
void
g_closure_set_meta_marshal (GClosure       *closure,
			    gpointer        marshal_data,
			    GClosureMarshal meta_marshal)
{
  GRealClosure *real_closure;

  g_return_if_fail (closure != NULL);
  g_return_if_fail (meta_marshal != NULL);
  g_return_if_fail (closure->is_invalid == FALSE);
  g_return_if_fail (closure->in_marshal == FALSE);

  real_closure = G_REAL_CLOSURE (closure);

  g_return_if_fail (real_closure->meta_marshal == NULL);

  real_closure->meta_marshal = meta_marshal;
  real_closure->meta_marshal_data = marshal_data;
}

/**
 * g_closure_add_marshal_guards: (skip)
 * @closure: a #GClosure
 * @pre_marshal_data: (closure pre_marshal_notify): data to pass
 *  to @pre_marshal_notify
 * @pre_marshal_notify: a function to call before the closure callback
 * @post_marshal_data: (closure post_marshal_notify): data to pass
 *  to @post_marshal_notify
 * @post_marshal_notify: a function to call after the closure callback
 *
 * Adds a pair of notifiers which get invoked before and after the
 * closure callback, respectively. This is typically used to protect
 * the extra arguments for the duration of the callback. See
 * g_object_watch_closure() for an example of marshal guards.
 */
void
g_closure_add_marshal_guards (GClosure      *closure,
			      gpointer       pre_marshal_data,
			      GClosureNotify pre_marshal_notify,
			      gpointer       post_marshal_data,
			      GClosureNotify post_marshal_notify)
{
  guint i;

  g_return_if_fail (closure != NULL);
  g_return_if_fail (pre_marshal_notify != NULL);
  g_return_if_fail (post_marshal_notify != NULL);
  g_return_if_fail (closure->is_invalid == FALSE);
  g_return_if_fail (closure->in_marshal == FALSE);
  g_return_if_fail (closure->n_guards < CLOSURE_MAX_N_GUARDS);

  closure->notifiers = g_renew (GClosureNotifyData, closure->notifiers, CLOSURE_N_NOTIFIERS (closure) + 2);
  if (closure->n_inotifiers)
    closure->notifiers[(CLOSURE_N_MFUNCS (closure) +
			closure->n_fnotifiers +
			closure->n_inotifiers + 1)] = closure->notifiers[(CLOSURE_N_MFUNCS (closure) +
									  closure->n_fnotifiers + 0)];
  if (closure->n_inotifiers > 1)
    closure->notifiers[(CLOSURE_N_MFUNCS (closure) +
			closure->n_fnotifiers +
			closure->n_inotifiers)] = closure->notifiers[(CLOSURE_N_MFUNCS (closure) +
								      closure->n_fnotifiers + 1)];
  if (closure->n_fnotifiers)
    closure->notifiers[(CLOSURE_N_MFUNCS (closure) +
			closure->n_fnotifiers + 1)] = closure->notifiers[CLOSURE_N_MFUNCS (closure) + 0];
  if (closure->n_fnotifiers > 1)
    closure->notifiers[(CLOSURE_N_MFUNCS (closure) +
			closure->n_fnotifiers)] = closure->notifiers[CLOSURE_N_MFUNCS (closure) + 1];
  if (closure->n_guards)
    closure->notifiers[(closure->n_guards +
			closure->n_guards + 1)] = closure->notifiers[closure->n_guards];
  i = closure->n_guards;
  closure->notifiers[i].data = pre_marshal_data;
  closure->notifiers[i].notify = pre_marshal_notify;
  closure->notifiers[i + 1].data = post_marshal_data;
  closure->notifiers[i + 1].notify = post_marshal_notify;
  INC (closure, n_guards);
}

/**
 * g_closure_add_finalize_notifier: (skip)
 * @closure: a #GClosure
 * @notify_data: (closure notify_func): data to pass to @notify_func
 * @notify_func: the callback function to register
 *
 * Registers a finalization notifier which will be called when the
 * reference count of @closure goes down to 0. Multiple finalization
 * notifiers on a single closure are invoked in unspecified order. If
 * a single call to g_closure_unref() results in the closure being
 * both invalidated and finalized, then the invalidate notifiers will
 * be run before the finalize notifiers.
 */
void
g_closure_add_finalize_notifier (GClosure      *closure,
				 gpointer       notify_data,
				 GClosureNotify notify_func)
{
  guint i;

  g_return_if_fail (closure != NULL);
  g_return_if_fail (notify_func != NULL);
  g_return_if_fail (closure->n_fnotifiers < CLOSURE_MAX_N_FNOTIFIERS);

  closure->notifiers = g_renew (GClosureNotifyData, closure->notifiers, CLOSURE_N_NOTIFIERS (closure) + 1);
  if (closure->n_inotifiers)
    closure->notifiers[(CLOSURE_N_MFUNCS (closure) +
			closure->n_fnotifiers +
			closure->n_inotifiers)] = closure->notifiers[(CLOSURE_N_MFUNCS (closure) +
								      closure->n_fnotifiers + 0)];
  i = CLOSURE_N_MFUNCS (closure) + closure->n_fnotifiers;
  closure->notifiers[i].data = notify_data;
  closure->notifiers[i].notify = notify_func;
  INC (closure, n_fnotifiers);
}

/**
 * g_closure_add_invalidate_notifier: (skip)
 * @closure: a #GClosure
 * @notify_data: (closure notify_func): data to pass to @notify_func
 * @notify_func: the callback function to register
 *
 * Registers an invalidation notifier which will be called when the
 * @closure is invalidated with g_closure_invalidate(). Invalidation
 * notifiers are invoked before finalization notifiers, in an
 * unspecified order.
 */
void
g_closure_add_invalidate_notifier (GClosure      *closure,
				   gpointer       notify_data,
				   GClosureNotify notify_func)
{
  guint i;

  g_return_if_fail (closure != NULL);
  g_return_if_fail (notify_func != NULL);
  g_return_if_fail (closure->is_invalid == FALSE);
  g_return_if_fail (closure->n_inotifiers < CLOSURE_MAX_N_INOTIFIERS);

  closure->notifiers = g_renew (GClosureNotifyData, closure->notifiers, CLOSURE_N_NOTIFIERS (closure) + 1);
  i = CLOSURE_N_MFUNCS (closure) + closure->n_fnotifiers + closure->n_inotifiers;
  closure->notifiers[i].data = notify_data;
  closure->notifiers[i].notify = notify_func;
  INC (closure, n_inotifiers);
}

static inline gboolean
closure_try_remove_inotify (GClosure       *closure,
			    gpointer       notify_data,
			    GClosureNotify notify_func)
{
  GClosureNotifyData *ndata, *nlast;

  nlast = closure->notifiers + CLOSURE_N_NOTIFIERS (closure) - 1;
  for (ndata = nlast + 1 - closure->n_inotifiers; ndata <= nlast; ndata++)
    if (ndata->notify == notify_func && ndata->data == notify_data)
      {
	DEC (closure, n_inotifiers);
	if (ndata < nlast)
	  *ndata = *nlast;

	return TRUE;
      }
  return FALSE;
}

static inline gboolean
closure_try_remove_fnotify (GClosure       *closure,
			    gpointer       notify_data,
			    GClosureNotify notify_func)
{
  GClosureNotifyData *ndata, *nlast;

  nlast = closure->notifiers + CLOSURE_N_NOTIFIERS (closure) - closure->n_inotifiers - 1;
  for (ndata = nlast + 1 - closure->n_fnotifiers; ndata <= nlast; ndata++)
    if (ndata->notify == notify_func && ndata->data == notify_data)
      {
	DEC (closure, n_fnotifiers);
	if (ndata < nlast)
	  *ndata = *nlast;
	if (closure->n_inotifiers)
	  closure->notifiers[(CLOSURE_N_MFUNCS (closure) +
			      closure->n_fnotifiers)] = closure->notifiers[(CLOSURE_N_MFUNCS (closure) +
									    closure->n_fnotifiers +
									    closure->n_inotifiers)];
	return TRUE;
      }
  return FALSE;
}

/**
 * g_closure_ref:
 * @closure: #GClosure to increment the reference count on
 *
 * Increments the reference count on a closure to force it staying
 * alive while the caller holds a pointer to it.
 *
 * Returns: (transfer none): The @closure passed in, for convenience
 */
GClosure*
g_closure_ref (GClosure *closure)
{
  guint new_ref_count;
  g_return_val_if_fail (closure != NULL, NULL);
  g_return_val_if_fail (closure->ref_count > 0, NULL);
  g_return_val_if_fail (closure->ref_count < CLOSURE_MAX_REF_COUNT, NULL);

  INC_ASSIGN (closure, ref_count, &new_ref_count);
  g_return_val_if_fail (new_ref_count > 1, NULL);

  return closure;
}

/**
 * g_closure_invalidate:
 * @closure: GClosure to invalidate
 *
 * Sets a flag on the closure to indicate that its calling
 * environment has become invalid, and thus causes any future
 * invocations of g_closure_invoke() on this @closure to be
 * ignored. Also, invalidation notifiers installed on the closure will
 * be called at this point. Note that unless you are holding a
 * reference to the closure yourself, the invalidation notifiers may
 * unref the closure and cause it to be destroyed, so if you need to
 * access the closure after calling g_closure_invalidate(), make sure
 * that you've previously called g_closure_ref().
 *
 * Note that g_closure_invalidate() will also be called when the
 * reference count of a closure drops to zero (unless it has already
 * been invalidated before).
 */
void
g_closure_invalidate (GClosure *closure)
{
  g_return_if_fail (closure != NULL);

  if (!closure->is_invalid)
    {
      gboolean was_invalid;
      g_closure_ref (closure);           /* preserve floating flag */
      SWAP (closure, is_invalid, TRUE, &was_invalid);
      /* invalidate only once */
      if (!was_invalid)
        closure_invoke_notifiers (closure, INOTIFY);
      g_closure_unref (closure);
    }
}

/**
 * g_closure_unref:
 * @closure: #GClosure to decrement the reference count on
 *
 * Decrements the reference count of a closure after it was previously
 * incremented by the same caller. If no other callers are using the
 * closure, then the closure will be destroyed and freed.
 */
void
g_closure_unref (GClosure *closure)
{
  guint new_ref_count;

  g_return_if_fail (closure != NULL);
  g_return_if_fail (closure->ref_count > 0);

  if (closure->ref_count == 1)	/* last unref, invalidate first */
    g_closure_invalidate (closure);

  DEC_ASSIGN (closure, ref_count, &new_ref_count);

  if (new_ref_count == 0)
    {
      closure_invoke_notifiers (closure, FNOTIFY);
      g_free (closure->notifiers);

      /* See comments in gtype.c about what's going on here... */
      if (RUNNING_ON_VALGRIND)
        {
          gchar *allocated;

          allocated = (gchar *) G_REAL_CLOSURE (closure);
          allocated -= sizeof (gpointer);

          g_free (allocated);

          VALGRIND_FREELIKE_BLOCK (allocated + sizeof (gpointer), 0);
          VALGRIND_FREELIKE_BLOCK (closure, 0);
        }
      else
        g_free (G_REAL_CLOSURE (closure));
    }
}

/**
 * g_closure_sink:
 * @closure: #GClosure to decrement the initial reference count on, if it's
 *           still being held
 *
 * Takes over the initial ownership of a closure.  Each closure is
 * initially created in a "floating" state, which means that the initial
 * reference count is not owned by any caller. g_closure_sink() checks
 * to see if the object is still floating, and if so, unsets the
 * floating state and decreases the reference count. If the closure
 * is not floating, g_closure_sink() does nothing. The reason for the
 * existence of the floating state is to prevent cumbersome code
 * sequences like:
 * |[<!-- language="C" --> 
 * closure = g_cclosure_new (cb_func, cb_data);
 * g_source_set_closure (source, closure);
 * g_closure_unref (closure); // GObject doesn't really need this
 * ]|
 * Because g_source_set_closure() (and similar functions) take ownership of the
 * initial reference count, if it is unowned, we instead can write:
 * |[<!-- language="C" --> 
 * g_source_set_closure (source, g_cclosure_new (cb_func, cb_data));
 * ]|
 *
 * Generally, this function is used together with g_closure_ref(). Ane example
 * of storing a closure for later notification looks like:
 * |[<!-- language="C" --> 
 * static GClosure *notify_closure = NULL;
 * void
 * foo_notify_set_closure (GClosure *closure)
 * {
 *   if (notify_closure)
 *     g_closure_unref (notify_closure);
 *   notify_closure = closure;
 *   if (notify_closure)
 *     {
 *       g_closure_ref (notify_closure);
 *       g_closure_sink (notify_closure);
 *     }
 * }
 * ]|
 *
 * Because g_closure_sink() may decrement the reference count of a closure
 * (if it hasn't been called on @closure yet) just like g_closure_unref(),
 * g_closure_ref() should be called prior to this function.
 */
void
g_closure_sink (GClosure *closure)
{
  g_return_if_fail (closure != NULL);
  g_return_if_fail (closure->ref_count > 0);

  /* floating is basically a kludge to avoid creating closures
   * with a ref_count of 0. so the initial ref_count a closure has
   * is unowned. with invoking g_closure_sink() code may
   * indicate that it takes over that intiial ref_count.
   */
  if (closure->floating)
    {
      gboolean was_floating;
      SWAP (closure, floating, FALSE, &was_floating);
      /* unref floating flag only once */
      if (was_floating)
        g_closure_unref (closure);
    }
}

/**
 * g_closure_remove_invalidate_notifier: (skip)
 * @closure: a #GClosure
 * @notify_data: data which was passed to g_closure_add_invalidate_notifier()
 *               when registering @notify_func
 * @notify_func: the callback function to remove
 *
 * Removes an invalidation notifier.
 *
 * Notice that notifiers are automatically removed after they are run.
 */
void
g_closure_remove_invalidate_notifier (GClosure      *closure,
				      gpointer       notify_data,
				      GClosureNotify notify_func)
{
  g_return_if_fail (closure != NULL);
  g_return_if_fail (notify_func != NULL);

  if (closure->is_invalid && closure->in_inotify && /* account removal of notify_func() while it's called */
      ((gpointer) closure->marshal) == ((gpointer) notify_func) &&
      closure->data == notify_data)
    closure->marshal = NULL;
  else if (!closure_try_remove_inotify (closure, notify_data, notify_func))
    g_warning (G_STRLOC ": unable to remove uninstalled invalidation notifier: %p (%p)",
	       notify_func, notify_data);
}

/**
 * g_closure_remove_finalize_notifier: (skip)
 * @closure: a #GClosure
 * @notify_data: data which was passed to g_closure_add_finalize_notifier()
 *  when registering @notify_func
 * @notify_func: the callback function to remove
 *
 * Removes a finalization notifier.
 *
 * Notice that notifiers are automatically removed after they are run.
 */
void
g_closure_remove_finalize_notifier (GClosure      *closure,
				    gpointer       notify_data,
				    GClosureNotify notify_func)
{
  g_return_if_fail (closure != NULL);
  g_return_if_fail (notify_func != NULL);

  if (closure->is_invalid && !closure->in_inotify && /* account removal of notify_func() while it's called */
      ((gpointer) closure->marshal) == ((gpointer) notify_func) &&
      closure->data == notify_data)
    closure->marshal = NULL;
  else if (!closure_try_remove_fnotify (closure, notify_data, notify_func))
    g_warning (G_STRLOC ": unable to remove uninstalled finalization notifier: %p (%p)",
               notify_func, notify_data);
}

/**
 * g_closure_invoke:
 * @closure: a #GClosure
 * @return_value: (optional) (out): a #GValue to store the return
 *                value. May be %NULL if the callback of @closure
 *                doesn't return a value.
 * @n_param_values: the length of the @param_values array
 * @param_values: (array length=n_param_values): an array of
 *                #GValues holding the arguments on which to
 *                invoke the callback of @closure
 * @invocation_hint: (nullable): a context-dependent invocation hint
 *
 * Invokes the closure, i.e. executes the callback represented by the @closure.
 */
void
g_closure_invoke (GClosure       *closure,
		  GValue /*out*/ *return_value,
		  guint           n_param_values,
		  const GValue   *param_values,
		  gpointer        invocation_hint)
{
  GRealClosure *real_closure;

  g_return_if_fail (closure != NULL);

  real_closure = G_REAL_CLOSURE (closure);

  g_closure_ref (closure);      /* preserve floating flag */
  if (!closure->is_invalid)
    {
      GClosureMarshal marshal;
      gpointer marshal_data;
      gboolean in_marshal = closure->in_marshal;

      g_return_if_fail (closure->marshal || real_closure->meta_marshal);

      SET (closure, in_marshal, TRUE);
      if (real_closure->meta_marshal)
	{
	  marshal_data = real_closure->meta_marshal_data;
	  marshal = real_closure->meta_marshal;
	}
      else
	{
	  marshal_data = NULL;
	  marshal = closure->marshal;
	}
      if (!in_marshal)
	closure_invoke_notifiers (closure, PRE_NOTIFY);
      marshal (closure,
	       return_value,
	       n_param_values, param_values,
	       invocation_hint,
	       marshal_data);
      if (!in_marshal)
	closure_invoke_notifiers (closure, POST_NOTIFY);
      SET (closure, in_marshal, in_marshal);
    }
  g_closure_unref (closure);
}

gboolean
_g_closure_supports_invoke_va (GClosure       *closure)
{
  GRealClosure *real_closure;

  g_return_val_if_fail (closure != NULL, FALSE);

  real_closure = G_REAL_CLOSURE (closure);

  return
    real_closure->va_marshal != NULL &&
    (real_closure->meta_marshal == NULL ||
     real_closure->va_meta_marshal != NULL);
}

void
_g_closure_invoke_va (GClosure       *closure,
		      GValue /*out*/ *return_value,
		      gpointer        instance,
		      va_list         args,
		      int             n_params,
		      GType          *param_types)
{
  GRealClosure *real_closure;

  g_return_if_fail (closure != NULL);

  real_closure = G_REAL_CLOSURE (closure);

  g_closure_ref (closure);      /* preserve floating flag */
  if (!closure->is_invalid)
    {
      GVaClosureMarshal marshal;
      gpointer marshal_data;
      gboolean in_marshal = closure->in_marshal;

      g_return_if_fail (closure->marshal || real_closure->meta_marshal);

      SET (closure, in_marshal, TRUE);
      if (real_closure->va_meta_marshal)
	{
	  marshal_data = real_closure->meta_marshal_data;
	  marshal = real_closure->va_meta_marshal;
	}
      else
	{
	  marshal_data = NULL;
	  marshal = real_closure->va_marshal;
	}
      if (!in_marshal)
	closure_invoke_notifiers (closure, PRE_NOTIFY);
      marshal (closure,
	       return_value,
	       instance, args,
	       marshal_data,
	       n_params, param_types);
      if (!in_marshal)
	closure_invoke_notifiers (closure, POST_NOTIFY);
      SET (closure, in_marshal, in_marshal);
    }
  g_closure_unref (closure);
}


/**
 * g_closure_set_marshal: (skip)
 * @closure: a #GClosure
 * @marshal: a #GClosureMarshal function
 *
 * Sets the marshaller of @closure. The `marshal_data`
 * of @marshal provides a way for a meta marshaller to provide additional
 * information to the marshaller. (See g_closure_set_meta_marshal().) For
 * GObject's C predefined marshallers (the g_cclosure_marshal_*()
 * functions), what it provides is a callback function to use instead of
 * @closure->callback.
 */
void
g_closure_set_marshal (GClosure       *closure,
		       GClosureMarshal marshal)
{
  g_return_if_fail (closure != NULL);
  g_return_if_fail (marshal != NULL);

  if (closure->marshal && closure->marshal != marshal)
    g_warning ("attempt to override closure->marshal (%p) with new marshal (%p)",
	       closure->marshal, marshal);
  else
    closure->marshal = marshal;
}

void
_g_closure_set_va_marshal (GClosure       *closure,
			   GVaClosureMarshal marshal)
{
  GRealClosure *real_closure;

  g_return_if_fail (closure != NULL);
  g_return_if_fail (marshal != NULL);

  real_closure = G_REAL_CLOSURE (closure);

  if (real_closure->va_marshal && real_closure->va_marshal != marshal)
    g_warning ("attempt to override closure->va_marshal (%p) with new marshal (%p)",
	       real_closure->va_marshal, marshal);
  else
    real_closure->va_marshal = marshal;
}

/**
 * g_cclosure_new: (skip)
 * @callback_func: the function to invoke
 * @user_data: (closure callback_func): user data to pass to @callback_func
 * @destroy_data: destroy notify to be called when @user_data is no longer used
 *
 * Creates a new closure which invokes @callback_func with @user_data as
 * the last parameter.
 *
 * Returns: (transfer none): a floating reference to a new #GCClosure
 */
GClosure*
g_cclosure_new (GCallback      callback_func,
		gpointer       user_data,
		GClosureNotify destroy_data)
{
  GClosure *closure;
  
  g_return_val_if_fail (callback_func != NULL, NULL);
  
  closure = g_closure_new_simple (sizeof (GCClosure), user_data);
  if (destroy_data)
    g_closure_add_finalize_notifier (closure, user_data, destroy_data);
  ((GCClosure*) closure)->callback = (gpointer) callback_func;
  
  return closure;
}

/**
 * g_cclosure_new_swap: (skip)
 * @callback_func: the function to invoke
 * @user_data: (closure callback_func): user data to pass to @callback_func
 * @destroy_data: destroy notify to be called when @user_data is no longer used
 *
 * Creates a new closure which invokes @callback_func with @user_data as
 * the first parameter.
 *
 * Returns: (transfer none): a floating reference to a new #GCClosure
 */
GClosure*
g_cclosure_new_swap (GCallback      callback_func,
		     gpointer       user_data,
		     GClosureNotify destroy_data)
{
  GClosure *closure;
  
  g_return_val_if_fail (callback_func != NULL, NULL);
  
  closure = g_closure_new_simple (sizeof (GCClosure), user_data);
  if (destroy_data)
    g_closure_add_finalize_notifier (closure, user_data, destroy_data);
  ((GCClosure*) closure)->callback = (gpointer) callback_func;
  SET (closure, derivative_flag, TRUE);
  
  return closure;
}

static void
g_type_class_meta_marshal (GClosure       *closure,
			   GValue /*out*/ *return_value,
			   guint           n_param_values,
			   const GValue   *param_values,
			   gpointer        invocation_hint,
			   gpointer        marshal_data)
{
  GTypeClass *class;
  gpointer callback;
  /* GType itype = (GType) closure->data; */
  guint offset = GPOINTER_TO_UINT (marshal_data);
  
  class = G_TYPE_INSTANCE_GET_CLASS (g_value_peek_pointer (param_values + 0), itype, GTypeClass);
  callback = G_STRUCT_MEMBER (gpointer, class, offset);
  if (callback)
    closure->marshal (closure,
		      return_value,
		      n_param_values, param_values,
		      invocation_hint,
		      callback);
}

static void
g_type_class_meta_marshalv (GClosure *closure,
			    GValue   *return_value,
			    gpointer  instance,
			    va_list   args,
			    gpointer  marshal_data,
			    int       n_params,
			    GType    *param_types)
{
  GRealClosure *real_closure;
  GTypeClass *class;
  gpointer callback;
  /* GType itype = (GType) closure->data; */
  guint offset = GPOINTER_TO_UINT (marshal_data);

  real_closure = G_REAL_CLOSURE (closure);

  class = G_TYPE_INSTANCE_GET_CLASS (instance, itype, GTypeClass);
  callback = G_STRUCT_MEMBER (gpointer, class, offset);
  if (callback)
    real_closure->va_marshal (closure,
			      return_value,
			      instance, args,
			      callback,
			      n_params,
			      param_types);
}

static void
g_type_iface_meta_marshal (GClosure       *closure,
			   GValue /*out*/ *return_value,
			   guint           n_param_values,
			   const GValue   *param_values,
			   gpointer        invocation_hint,
			   gpointer        marshal_data)
{
  GTypeClass *class;
  gpointer callback;
  GType itype = (GType) closure->data;
  guint offset = GPOINTER_TO_UINT (marshal_data);
  
  class = G_TYPE_INSTANCE_GET_INTERFACE (g_value_peek_pointer (param_values + 0), itype, GTypeClass);
  callback = G_STRUCT_MEMBER (gpointer, class, offset);
  if (callback)
    closure->marshal (closure,
		      return_value,
		      n_param_values, param_values,
		      invocation_hint,
		      callback);
}

gboolean
_g_closure_is_void (GClosure *closure,
		    gpointer instance)
{
  GRealClosure *real_closure;
  GTypeClass *class;
  gpointer callback;
  GType itype;
  guint offset;

  if (closure->is_invalid)
    return TRUE;

  real_closure = G_REAL_CLOSURE (closure);

  if (real_closure->meta_marshal == g_type_iface_meta_marshal)
    {
      itype = (GType) closure->data;
      offset = GPOINTER_TO_UINT (real_closure->meta_marshal_data);

      class = G_TYPE_INSTANCE_GET_INTERFACE (instance, itype, GTypeClass);
      callback = G_STRUCT_MEMBER (gpointer, class, offset);
      return callback == NULL;
    }
  else if (real_closure->meta_marshal == g_type_class_meta_marshal)
    {
      offset = GPOINTER_TO_UINT (real_closure->meta_marshal_data);

      class = G_TYPE_INSTANCE_GET_CLASS (instance, itype, GTypeClass);
      callback = G_STRUCT_MEMBER (gpointer, class, offset);
      return callback == NULL;
    }

  return FALSE;
}

static void
g_type_iface_meta_marshalv (GClosure *closure,
			    GValue   *return_value,
			    gpointer  instance,
			    va_list   args,
			    gpointer  marshal_data,
			    int       n_params,
			    GType    *param_types)
{
  GRealClosure *real_closure;
  GTypeClass *class;
  gpointer callback;
  GType itype = (GType) closure->data;
  guint offset = GPOINTER_TO_UINT (marshal_data);

  real_closure = G_REAL_CLOSURE (closure);

  class = G_TYPE_INSTANCE_GET_INTERFACE (instance, itype, GTypeClass);
  callback = G_STRUCT_MEMBER (gpointer, class, offset);
  if (callback)
    real_closure->va_marshal (closure,
			      return_value,
			      instance, args,
			      callback,
			      n_params,
			      param_types);
}

/**
 * g_signal_type_cclosure_new:
 * @itype: the #GType identifier of an interface or classed type
 * @struct_offset: the offset of the member function of @itype's class
 *  structure which is to be invoked by the new closure
 *
 * Creates a new closure which invokes the function found at the offset
 * @struct_offset in the class structure of the interface or classed type
 * identified by @itype.
 *
 * Returns: (transfer none): a floating reference to a new #GCClosure
 */
GClosure*
g_signal_type_cclosure_new (GType    itype,
			    guint    struct_offset)
{
  GClosure *closure;
  
  g_return_val_if_fail (G_TYPE_IS_CLASSED (itype) || G_TYPE_IS_INTERFACE (itype), NULL);
  g_return_val_if_fail (struct_offset >= sizeof (GTypeClass), NULL);
  
  closure = g_closure_new_simple (sizeof (GClosure), (gpointer) itype);
  if (G_TYPE_IS_INTERFACE (itype))
    {
      g_closure_set_meta_marshal (closure, GUINT_TO_POINTER (struct_offset), g_type_iface_meta_marshal);
      g_closure_set_meta_va_marshal (closure, g_type_iface_meta_marshalv);
    }
  else
    {
      g_closure_set_meta_marshal (closure, GUINT_TO_POINTER (struct_offset), g_type_class_meta_marshal);
      g_closure_set_meta_va_marshal (closure, g_type_class_meta_marshalv);
    }
  return closure;
}

#include <ffi.h>
static ffi_type *
value_to_ffi_type (const GValue *gvalue,
                   gpointer *value,
                   gint *enum_tmpval,
                   gboolean *tmpval_used)
{
  ffi_type *rettype = NULL;
  GType type = g_type_fundamental (G_VALUE_TYPE (gvalue));
  g_assert (type != G_TYPE_INVALID);

  if (enum_tmpval)
    {
      g_assert (tmpval_used != NULL);
      *tmpval_used = FALSE;
    }

  switch (type)
    {
    case G_TYPE_BOOLEAN:
    case G_TYPE_CHAR:
    case G_TYPE_INT:
      rettype = &ffi_type_sint;
      *value = (gpointer)&(gvalue->data[0].v_int);
      break;
    case G_TYPE_ENUM:
      /* enums are stored in v_long even though they are integers, which makes
       * marshalling through libffi somewhat complicated.  They need to be
       * marshalled as signed ints, but we need to use a temporary int sized
       * value to pass to libffi otherwise it'll pull the wrong value on
       * BE machines with 32-bit integers when treating v_long as 32-bit int.
       */
      g_assert (enum_tmpval != NULL);
      rettype = &ffi_type_sint;
      *enum_tmpval = g_value_get_enum (gvalue);
      *value = enum_tmpval;
      *tmpval_used = TRUE;
      break;
    case G_TYPE_FLAGS:
      g_assert (enum_tmpval != NULL);
      rettype = &ffi_type_uint;
      *enum_tmpval = g_value_get_flags (gvalue);
      *value = enum_tmpval;
      *tmpval_used = TRUE;
      break;
    case G_TYPE_UCHAR:
    case G_TYPE_UINT:
      rettype = &ffi_type_uint;
      *value = (gpointer)&(gvalue->data[0].v_uint);
      break;
    case G_TYPE_STRING:
    case G_TYPE_OBJECT:
    case G_TYPE_BOXED:
    case G_TYPE_PARAM:
    case G_TYPE_POINTER:
    case G_TYPE_INTERFACE:
    case G_TYPE_VARIANT:
      rettype = &ffi_type_pointer;
      *value = (gpointer)&(gvalue->data[0].v_pointer);
      break;
    case G_TYPE_FLOAT:
      rettype = &ffi_type_float;
      *value = (gpointer)&(gvalue->data[0].v_float);
      break;
    case G_TYPE_DOUBLE:
      rettype = &ffi_type_double;
      *value = (gpointer)&(gvalue->data[0].v_double);
      break;
    case G_TYPE_LONG:
      rettype = &ffi_type_slong;
      *value = (gpointer)&(gvalue->data[0].v_long);
      break;
    case G_TYPE_ULONG:
      rettype = &ffi_type_ulong;
      *value = (gpointer)&(gvalue->data[0].v_ulong);
      break;
    case G_TYPE_INT64:
      rettype = &ffi_type_sint64;
      *value = (gpointer)&(gvalue->data[0].v_int64);
      break;
    case G_TYPE_UINT64:
      rettype = &ffi_type_uint64;
      *value = (gpointer)&(gvalue->data[0].v_uint64);
      break;
    default:
      rettype = &ffi_type_pointer;
      *value = NULL;
      g_warning ("value_to_ffi_type: Unsupported fundamental type: %s", g_type_name (type));
      break;
    }
  return rettype;
}

static void
value_from_ffi_type (GValue *gvalue, gpointer *value)
{
  ffi_arg *int_val = (ffi_arg*) value;

  switch (g_type_fundamental (G_VALUE_TYPE (gvalue)))
    {
    case G_TYPE_INT:
      g_value_set_int (gvalue, (gint) *int_val);
      break;
    case G_TYPE_FLOAT:
      g_value_set_float (gvalue, *(gfloat*)value);
      break;
    case G_TYPE_DOUBLE:
      g_value_set_double (gvalue, *(gdouble*)value);
      break;
    case G_TYPE_BOOLEAN:
      g_value_set_boolean (gvalue, (gboolean) *int_val);
      break;
    case G_TYPE_STRING:
      g_value_take_string (gvalue, *(gchar**)value);
      break;
    case G_TYPE_CHAR:
      g_value_set_schar (gvalue, (gint8) *int_val);
      break;
    case G_TYPE_UCHAR:
      g_value_set_uchar (gvalue, (guchar) *int_val);
      break;
    case G_TYPE_UINT:
      g_value_set_uint (gvalue, (guint) *int_val);
      break;
    case G_TYPE_POINTER:
      g_value_set_pointer (gvalue, *(gpointer*)value);
      break;
    case G_TYPE_LONG:
      g_value_set_long (gvalue, (glong) *int_val);
      break;
    case G_TYPE_ULONG:
      g_value_set_ulong (gvalue, (gulong) *int_val);
      break;
    case G_TYPE_INT64:
      g_value_set_int64 (gvalue, (gint64) *int_val);
      break;
    case G_TYPE_UINT64:
      g_value_set_uint64 (gvalue, (guint64) *int_val);
      break;
    case G_TYPE_BOXED:
      g_value_take_boxed (gvalue, *(gpointer*)value);
      break;
    case G_TYPE_ENUM:
      g_value_set_enum (gvalue, (gint) *int_val);
      break;
    case G_TYPE_FLAGS:
      g_value_set_flags (gvalue, (guint) *int_val);
      break;
    case G_TYPE_PARAM:
      g_value_take_param (gvalue, *(gpointer*)value);
      break;
    case G_TYPE_OBJECT:
      g_value_take_object (gvalue, *(gpointer*)value);
      break;
    case G_TYPE_VARIANT:
      g_value_take_variant (gvalue, *(gpointer*)value);
      break;
    default:
      g_warning ("value_from_ffi_type: Unsupported fundamental type: %s",
                g_type_name (g_type_fundamental (G_VALUE_TYPE (gvalue))));
    }
}

typedef union {
  gpointer _gpointer;
  float _float;
  double _double;
  gint _gint;
  guint _guint;
  glong _glong;
  gulong _gulong;
  gint64 _gint64;
  guint64 _guint64;
} va_arg_storage;

static ffi_type *
va_to_ffi_type (GType gtype,
		va_list *va,
		va_arg_storage *storage)
{
  ffi_type *rettype = NULL;
  GType type = g_type_fundamental (gtype);
  g_assert (type != G_TYPE_INVALID);

  switch (type)
    {
    case G_TYPE_BOOLEAN:
    case G_TYPE_CHAR:
    case G_TYPE_INT:
    case G_TYPE_ENUM:
      rettype = &ffi_type_sint;
      storage->_gint = va_arg (*va, gint);
      break;
    case G_TYPE_UCHAR:
    case G_TYPE_UINT:
    case G_TYPE_FLAGS:
      rettype = &ffi_type_uint;
      storage->_guint = va_arg (*va, guint);
      break;
    case G_TYPE_STRING:
    case G_TYPE_OBJECT:
    case G_TYPE_BOXED:
    case G_TYPE_PARAM:
    case G_TYPE_POINTER:
    case G_TYPE_INTERFACE:
    case G_TYPE_VARIANT:
      rettype = &ffi_type_pointer;
      storage->_gpointer = va_arg (*va, gpointer);
      break;
    case G_TYPE_FLOAT:
      /* Float args are passed as doubles in varargs */
      rettype = &ffi_type_float;
      storage->_float = (float)va_arg (*va, double);
      break;
    case G_TYPE_DOUBLE:
      rettype = &ffi_type_double;
      storage->_double = va_arg (*va, double);
      break;
    case G_TYPE_LONG:
      rettype = &ffi_type_slong;
      storage->_glong = va_arg (*va, glong);
      break;
    case G_TYPE_ULONG:
      rettype = &ffi_type_ulong;
      storage->_gulong = va_arg (*va, gulong);
      break;
    case G_TYPE_INT64:
      rettype = &ffi_type_sint64;
      storage->_gint64 = va_arg (*va, gint64);
      break;
    case G_TYPE_UINT64:
      rettype = &ffi_type_uint64;
      storage->_guint64 = va_arg (*va, guint64);
      break;
    default:
      rettype = &ffi_type_pointer;
      storage->_guint64  = 0;
      g_warning ("va_to_ffi_type: Unsupported fundamental type: %s", g_type_name (type));
      break;
    }
  return rettype;
}

/**
 * g_cclosure_marshal_generic:
 * @closure: A #GClosure.
 * @return_gvalue: A #GValue to store the return value. May be %NULL
 *   if the callback of closure doesn't return a value.
 * @n_param_values: The length of the @param_values array.
 * @param_values: An array of #GValues holding the arguments
 *   on which to invoke the callback of closure.
 * @invocation_hint: The invocation hint given as the last argument to
 *   g_closure_invoke().
 * @marshal_data: Additional data specified when registering the
 *   marshaller, see g_closure_set_marshal() and
 *   g_closure_set_meta_marshal()
 *
 * A generic marshaller function implemented via
 * [libffi](http://sourceware.org/libffi/).
 *
 * Normally this function is not passed explicitly to g_signal_new(),
 * but used automatically by GLib when specifying a %NULL marshaller.
 *
 * Since: 2.30
 */
void
g_cclosure_marshal_generic (GClosure     *closure,
                            GValue       *return_gvalue,
                            guint         n_param_values,
                            const GValue *param_values,
                            gpointer      invocation_hint,
                            gpointer      marshal_data)
{
  ffi_type *rtype;
  void *rvalue;
  int n_args;
  ffi_type **atypes;
  void **args;
  int i;
  ffi_cif cif;
  GCClosure *cc = (GCClosure*) closure;
  gint *enum_tmpval;
  gboolean tmpval_used = FALSE;

  enum_tmpval = g_alloca (sizeof (gint));
  if (return_gvalue && G_VALUE_TYPE (return_gvalue))
    {
      rtype = value_to_ffi_type (return_gvalue, &rvalue, enum_tmpval, &tmpval_used);
    }
  else
    {
      rtype = &ffi_type_void;
    }

  rvalue = g_alloca (MAX (rtype->size, sizeof (ffi_arg)));

  n_args = n_param_values + 1;
  atypes = g_alloca (sizeof (ffi_type *) * n_args);
  args =  g_alloca (sizeof (gpointer) * n_args);

  if (tmpval_used)
    enum_tmpval = g_alloca (sizeof (gint));

  if (G_CCLOSURE_SWAP_DATA (closure))
    {
      atypes[n_args-1] = value_to_ffi_type (param_values + 0,
                                            &args[n_args-1],
                                            enum_tmpval,
                                            &tmpval_used);
      atypes[0] = &ffi_type_pointer;
      args[0] = &closure->data;
    }
  else
    {
      atypes[0] = value_to_ffi_type (param_values + 0,
                                     &args[0],
                                     enum_tmpval,
                                     &tmpval_used);
      atypes[n_args-1] = &ffi_type_pointer;
      args[n_args-1] = &closure->data;
    }

  for (i = 1; i < n_args - 1; i++)
    {
      if (tmpval_used)
        enum_tmpval = g_alloca (sizeof (gint));

      atypes[i] = value_to_ffi_type (param_values + i,
                                     &args[i],
                                     enum_tmpval,
                                     &tmpval_used);
    }

  if (ffi_prep_cif (&cif, FFI_DEFAULT_ABI, n_args, rtype, atypes) != FFI_OK)
    return;

  ffi_call (&cif, marshal_data ? marshal_data : cc->callback, rvalue, args);

  if (return_gvalue && G_VALUE_TYPE (return_gvalue))
    value_from_ffi_type (return_gvalue, rvalue);
}

/**
 * g_cclosure_marshal_generic_va:
 * @closure: the #GClosure to which the marshaller belongs
 * @return_value: (nullable): a #GValue to store the return
 *  value. May be %NULL if the callback of @closure doesn't return a
 *  value.
 * @instance: (type GObject.TypeInstance): the instance on which the closure is
 *  invoked.
 * @args_list: va_list of arguments to be passed to the closure.
 * @marshal_data: (nullable): additional data specified when
 *  registering the marshaller, see g_closure_set_marshal() and
 *  g_closure_set_meta_marshal()
 * @n_params: the length of the @param_types array
 * @param_types: (array length=n_params): the #GType of each argument from
 *  @args_list.
 *
 * A generic #GVaClosureMarshal function implemented via
 * [libffi](http://sourceware.org/libffi/).
 *
 * Since: 2.30
 */
void
g_cclosure_marshal_generic_va (GClosure *closure,
			       GValue   *return_value,
			       gpointer  instance,
			       va_list   args_list,
			       gpointer  marshal_data,
			       int       n_params,
			       GType    *param_types)
{
  ffi_type *rtype;
  void *rvalue;
  int n_args;
  ffi_type **atypes;
  void **args;
  va_arg_storage *storage;
  int i;
  ffi_cif cif;
  GCClosure *cc = (GCClosure*) closure;
  gint *enum_tmpval;
  gboolean tmpval_used = FALSE;
  va_list args_copy;

  enum_tmpval = g_alloca (sizeof (gint));
  if (return_value && G_VALUE_TYPE (return_value))
    {
      rtype = value_to_ffi_type (return_value, &rvalue, enum_tmpval, &tmpval_used);
    }
  else
    {
      rtype = &ffi_type_void;
    }

  rvalue = g_alloca (MAX (rtype->size, sizeof (ffi_arg)));

  n_args = n_params + 2;
  atypes = g_alloca (sizeof (ffi_type *) * n_args);
  args =  g_alloca (sizeof (gpointer) * n_args);
  storage = g_alloca (sizeof (va_arg_storage) * n_params);

  if (G_CCLOSURE_SWAP_DATA (closure))
    {
      atypes[n_args-1] = &ffi_type_pointer;
      args[n_args-1] = &instance;
      atypes[0] = &ffi_type_pointer;
      args[0] = &closure->data;
    }
  else
    {
      atypes[0] = &ffi_type_pointer;
      args[0] = &instance;
      atypes[n_args-1] = &ffi_type_pointer;
      args[n_args-1] = &closure->data;
    }

  G_VA_COPY (args_copy, args_list);

  /* Box non-primitive arguments */
  for (i = 0; i < n_params; i++)
    {
      GType type = param_types[i]  & ~G_SIGNAL_TYPE_STATIC_SCOPE;
      GType fundamental = G_TYPE_FUNDAMENTAL (type);

      atypes[i+1] = va_to_ffi_type (type,
				    &args_copy,
				    &storage[i]);
      args[i+1] = &storage[i];

      if ((param_types[i]  & G_SIGNAL_TYPE_STATIC_SCOPE) == 0)
	{
	  if (fundamental == G_TYPE_STRING && storage[i]._gpointer != NULL)
	    storage[i]._gpointer = g_strdup (storage[i]._gpointer);
	  else if (fundamental == G_TYPE_PARAM && storage[i]._gpointer != NULL)
	    storage[i]._gpointer = g_param_spec_ref (storage[i]._gpointer);
	  else if (fundamental == G_TYPE_BOXED && storage[i]._gpointer != NULL)
	    storage[i]._gpointer = g_boxed_copy (type, storage[i]._gpointer);
	  else if (fundamental == G_TYPE_VARIANT && storage[i]._gpointer != NULL)
	    storage[i]._gpointer = g_variant_ref_sink (storage[i]._gpointer);
	}
      if (fundamental == G_TYPE_OBJECT && storage[i]._gpointer != NULL)
	storage[i]._gpointer = g_object_ref (storage[i]._gpointer);
    }

  va_end (args_copy);
  
  if (ffi_prep_cif (&cif, FFI_DEFAULT_ABI, n_args, rtype, atypes) != FFI_OK)
    return;

  ffi_call (&cif, marshal_data ? marshal_data : cc->callback, rvalue, args);

  /* Unbox non-primitive arguments */
  for (i = 0; i < n_params; i++)
    {
      GType type = param_types[i]  & ~G_SIGNAL_TYPE_STATIC_SCOPE;
      GType fundamental = G_TYPE_FUNDAMENTAL (type);

      if ((param_types[i]  & G_SIGNAL_TYPE_STATIC_SCOPE) == 0)
	{
	  if (fundamental == G_TYPE_STRING && storage[i]._gpointer != NULL)
	    g_free (storage[i]._gpointer);
	  else if (fundamental == G_TYPE_PARAM && storage[i]._gpointer != NULL)
	    g_param_spec_unref (storage[i]._gpointer);
	  else if (fundamental == G_TYPE_BOXED && storage[i]._gpointer != NULL)
	    g_boxed_free (type, storage[i]._gpointer);
	  else if (fundamental == G_TYPE_VARIANT && storage[i]._gpointer != NULL)
	    g_variant_unref (storage[i]._gpointer);
	}
      if (fundamental == G_TYPE_OBJECT && storage[i]._gpointer != NULL)
	g_object_unref (storage[i]._gpointer);
    }
  
  if (return_value && G_VALUE_TYPE (return_value))
    value_from_ffi_type (return_value, rvalue);
}

/**
 * g_cclosure_marshal_VOID__VOID:
 * @closure: the #GClosure to which the marshaller belongs
 * @return_value: ignored
 * @n_param_values: 1
 * @param_values: a #GValue array holding only the instance
 * @invocation_hint: the invocation hint given as the last argument
 *  to g_closure_invoke()
 * @marshal_data: additional data specified when registering the marshaller
 *
 * A marshaller for a #GCClosure with a callback of type
 * `void (*callback) (gpointer instance, gpointer user_data)`.
 */

/**
 * g_cclosure_marshal_VOID__BOOLEAN:
 * @closure: the #GClosure to which the marshaller belongs
 * @return_value: ignored
 * @n_param_values: 2
 * @param_values: a #GValue array holding the instance and the #gboolean parameter
 * @invocation_hint: the invocation hint given as the last argument
 *  to g_closure_invoke()
 * @marshal_data: additional data specified when registering the marshaller
 *
 * A marshaller for a #GCClosure with a callback of type
 * `void (*callback) (gpointer instance, gboolean arg1, gpointer user_data)`.
 */

/**
 * g_cclosure_marshal_VOID__CHAR:
 * @closure: the #GClosure to which the marshaller belongs
 * @return_value: ignored
 * @n_param_values: 2
 * @param_values: a #GValue array holding the instance and the #gchar parameter
 * @invocation_hint: the invocation hint given as the last argument
 *  to g_closure_invoke()
 * @marshal_data: additional data specified when registering the marshaller
 *
 * A marshaller for a #GCClosure with a callback of type
 * `void (*callback) (gpointer instance, gchar arg1, gpointer user_data)`.
 */

/**
 * g_cclosure_marshal_VOID__UCHAR:
 * @closure: the #GClosure to which the marshaller belongs
 * @return_value: ignored
 * @n_param_values: 2
 * @param_values: a #GValue array holding the instance and the #guchar parameter
 * @invocation_hint: the invocation hint given as the last argument
 *  to g_closure_invoke()
 * @marshal_data: additional data specified when registering the marshaller
 *
 * A marshaller for a #GCClosure with a callback of type
 * `void (*callback) (gpointer instance, guchar arg1, gpointer user_data)`.
 */

/**
 * g_cclosure_marshal_VOID__INT:
 * @closure: the #GClosure to which the marshaller belongs
 * @return_value: ignored
 * @n_param_values: 2
 * @param_values: a #GValue array holding the instance and the #gint parameter
 * @invocation_hint: the invocation hint given as the last argument
 *  to g_closure_invoke()
 * @marshal_data: additional data specified when registering the marshaller
 *
 * A marshaller for a #GCClosure with a callback of type
 * `void (*callback) (gpointer instance, gint arg1, gpointer user_data)`.
 */

/**
 * g_cclosure_marshal_VOID__UINT:
 * @closure: the #GClosure to which the marshaller belongs
 * @return_value: ignored
 * @n_param_values: 2
 * @param_values: a #GValue array holding the instance and the #guint parameter
 * @invocation_hint: the invocation hint given as the last argument
 *  to g_closure_invoke()
 * @marshal_data: additional data specified when registering the marshaller
 *
 * A marshaller for a #GCClosure with a callback of type
 * `void (*callback) (gpointer instance, guint arg1, gpointer user_data)`.
 */

/**
 * g_cclosure_marshal_VOID__LONG:
 * @closure: the #GClosure to which the marshaller belongs
 * @return_value: ignored
 * @n_param_values: 2
 * @param_values: a #GValue array holding the instance and the #glong parameter
 * @invocation_hint: the invocation hint given as the last argument
 *  to g_closure_invoke()
 * @marshal_data: additional data specified when registering the marshaller
 *
 * A marshaller for a #GCClosure with a callback of type
 * `void (*callback) (gpointer instance, glong arg1, gpointer user_data)`.
 */

/**
 * g_cclosure_marshal_VOID__ULONG:
 * @closure: the #GClosure to which the marshaller belongs
 * @return_value: ignored
 * @n_param_values: 2
 * @param_values: a #GValue array holding the instance and the #gulong parameter
 * @invocation_hint: the invocation hint given as the last argument
 *  to g_closure_invoke()
 * @marshal_data: additional data specified when registering the marshaller
 *
 * A marshaller for a #GCClosure with a callback of type
 * `void (*callback) (gpointer instance, gulong arg1, gpointer user_data)`.
 */

/**
 * g_cclosure_marshal_VOID__ENUM:
 * @closure: the #GClosure to which the marshaller belongs
 * @return_value: ignored
 * @n_param_values: 2
 * @param_values: a #GValue array holding the instance and the enumeration parameter
 * @invocation_hint: the invocation hint given as the last argument
 *  to g_closure_invoke()
 * @marshal_data: additional data specified when registering the marshaller
 *
 * A marshaller for a #GCClosure with a callback of type
 * `void (*callback) (gpointer instance, gint arg1, gpointer user_data)` where the #gint parameter denotes an enumeration type..
 */

/**
 * g_cclosure_marshal_VOID__FLAGS:
 * @closure: the #GClosure to which the marshaller belongs
 * @return_value: ignored
 * @n_param_values: 2
 * @param_values: a #GValue array holding the instance and the flags parameter
 * @invocation_hint: the invocation hint given as the last argument
 *  to g_closure_invoke()
 * @marshal_data: additional data specified when registering the marshaller
 *
 * A marshaller for a #GCClosure with a callback of type
 * `void (*callback) (gpointer instance, gint arg1, gpointer user_data)` where the #gint parameter denotes a flags type.
 */

/**
 * g_cclosure_marshal_VOID__FLOAT:
 * @closure: the #GClosure to which the marshaller belongs
 * @return_value: ignored
 * @n_param_values: 2
 * @param_values: a #GValue array holding the instance and the #gfloat parameter
 * @invocation_hint: the invocation hint given as the last argument
 *  to g_closure_invoke()
 * @marshal_data: additional data specified when registering the marshaller
 *
 * A marshaller for a #GCClosure with a callback of type
 * `void (*callback) (gpointer instance, gfloat arg1, gpointer user_data)`.
 */

/**
 * g_cclosure_marshal_VOID__DOUBLE:
 * @closure: the #GClosure to which the marshaller belongs
 * @return_value: ignored
 * @n_param_values: 2
 * @param_values: a #GValue array holding the instance and the #gdouble parameter
 * @invocation_hint: the invocation hint given as the last argument
 *  to g_closure_invoke()
 * @marshal_data: additional data specified when registering the marshaller
 *
 * A marshaller for a #GCClosure with a callback of type
 * `void (*callback) (gpointer instance, gdouble arg1, gpointer user_data)`.
 */

/**
 * g_cclosure_marshal_VOID__STRING:
 * @closure: the #GClosure to which the marshaller belongs
 * @return_value: ignored
 * @n_param_values: 2
 * @param_values: a #GValue array holding the instance and the #gchar* parameter
 * @invocation_hint: the invocation hint given as the last argument
 *  to g_closure_invoke()
 * @marshal_data: additional data specified when registering the marshaller
 *
 * A marshaller for a #GCClosure with a callback of type
 * `void (*callback) (gpointer instance, const gchar *arg1, gpointer user_data)`.
 */

/**
 * g_cclosure_marshal_VOID__PARAM:
 * @closure: the #GClosure to which the marshaller belongs
 * @return_value: ignored
 * @n_param_values: 2
 * @param_values: a #GValue array holding the instance and the #GParamSpec* parameter
 * @invocation_hint: the invocation hint given as the last argument
 *  to g_closure_invoke()
 * @marshal_data: additional data specified when registering the marshaller
 *
 * A marshaller for a #GCClosure with a callback of type
 * `void (*callback) (gpointer instance, GParamSpec *arg1, gpointer user_data)`.
 */

/**
 * g_cclosure_marshal_VOID__BOXED:
 * @closure: the #GClosure to which the marshaller belongs
 * @return_value: ignored
 * @n_param_values: 2
 * @param_values: a #GValue array holding the instance and the #GBoxed* parameter
 * @invocation_hint: the invocation hint given as the last argument
 *  to g_closure_invoke()
 * @marshal_data: additional data specified when registering the marshaller
 *
 * A marshaller for a #GCClosure with a callback of type
 * `void (*callback) (gpointer instance, GBoxed *arg1, gpointer user_data)`.
 */

/**
 * g_cclosure_marshal_VOID__POINTER:
 * @closure: the #GClosure to which the marshaller belongs
 * @return_value: ignored
 * @n_param_values: 2
 * @param_values: a #GValue array holding the instance and the #gpointer parameter
 * @invocation_hint: the invocation hint given as the last argument
 *  to g_closure_invoke()
 * @marshal_data: additional data specified when registering the marshaller
 *
 * A marshaller for a #GCClosure with a callback of type
 * `void (*callback) (gpointer instance, gpointer arg1, gpointer user_data)`.
 */

/**
 * g_cclosure_marshal_VOID__OBJECT:
 * @closure: the #GClosure to which the marshaller belongs
 * @return_value: ignored
 * @n_param_values: 2
 * @param_values: a #GValue array holding the instance and the #GObject* parameter
 * @invocation_hint: the invocation hint given as the last argument
 *  to g_closure_invoke()
 * @marshal_data: additional data specified when registering the marshaller
 *
 * A marshaller for a #GCClosure with a callback of type
 * `void (*callback) (gpointer instance, GObject *arg1, gpointer user_data)`.
 */

/**
 * g_cclosure_marshal_VOID__VARIANT:
 * @closure: the #GClosure to which the marshaller belongs
 * @return_value: ignored
 * @n_param_values: 2
 * @param_values: a #GValue array holding the instance and the #GVariant* parameter
 * @invocation_hint: the invocation hint given as the last argument
 *  to g_closure_invoke()
 * @marshal_data: additional data specified when registering the marshaller
 *
 * A marshaller for a #GCClosure with a callback of type
 * `void (*callback) (gpointer instance, GVariant *arg1, gpointer user_data)`.
 *
 * Since: 2.26
 */

/**
 * g_cclosure_marshal_VOID__UINT_POINTER:
 * @closure: the #GClosure to which the marshaller belongs
 * @return_value: ignored
 * @n_param_values: 3
 * @param_values: a #GValue array holding instance, arg1 and arg2
 * @invocation_hint: the invocation hint given as the last argument
 *  to g_closure_invoke()
 * @marshal_data: additional data specified when registering the marshaller
 *
 * A marshaller for a #GCClosure with a callback of type
 * `void (*callback) (gpointer instance, guint arg1, gpointer arg2, gpointer user_data)`.
 */

/**
 * g_cclosure_marshal_BOOLEAN__FLAGS:
 * @closure: the #GClosure to which the marshaller belongs
 * @return_value: a #GValue which can store the returned #gboolean
 * @n_param_values: 2
 * @param_values: a #GValue array holding instance and arg1
 * @invocation_hint: the invocation hint given as the last argument
 *  to g_closure_invoke()
 * @marshal_data: additional data specified when registering the marshaller
 *
 * A marshaller for a #GCClosure with a callback of type
 * `gboolean (*callback) (gpointer instance, gint arg1, gpointer user_data)` where the #gint parameter
 * denotes a flags type.
 */

/**
 * g_cclosure_marshal_BOOL__FLAGS:
 *
 * Another name for g_cclosure_marshal_BOOLEAN__FLAGS().
 */
/**
 * g_cclosure_marshal_STRING__OBJECT_POINTER:
 * @closure: the #GClosure to which the marshaller belongs
 * @return_value: a #GValue, which can store the returned string
 * @n_param_values: 3
 * @param_values: a #GValue array holding instance, arg1 and arg2
 * @invocation_hint: the invocation hint given as the last argument
 *  to g_closure_invoke()
 * @marshal_data: additional data specified when registering the marshaller
 *
 * A marshaller for a #GCClosure with a callback of type
 * `gchar* (*callback) (gpointer instance, GObject *arg1, gpointer arg2, gpointer user_data)`.
 */
/**
 * g_cclosure_marshal_BOOLEAN__OBJECT_BOXED_BOXED:
 * @closure: the #GClosure to which the marshaller belongs
 * @return_value: a #GValue, which can store the returned string
 * @n_param_values: 3
 * @param_values: a #GValue array holding instance, arg1 and arg2
 * @invocation_hint: the invocation hint given as the last argument
 *  to g_closure_invoke()
 * @marshal_data: additional data specified when registering the marshaller
 *
 * A marshaller for a #GCClosure with a callback of type
 * `gboolean (*callback) (gpointer instance, GBoxed *arg1, GBoxed *arg2, gpointer user_data)`.
 *
 * Since: 2.26
 */
