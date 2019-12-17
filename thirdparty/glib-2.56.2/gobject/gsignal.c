/* GObject - GLib Type, Object, Parameter and Signal Library
 * Copyright (C) 2000-2001 Red Hat, Inc.
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
 * this code is based on the original GtkSignal implementation
 * for the Gtk+ library by Peter Mattis <petm@xcf.berkeley.edu>
 */

/*
 * MT safe
 */

#include "config.h"

#include <string.h>
#include <signal.h>

#include "gsignal.h"
#include "gtype-private.h"
#include "gbsearcharray.h"
#include "gvaluecollector.h"
#include "gvaluetypes.h"
#include "gobject.h"
#include "genums.h"
#include "gobject_trace.h"


/**
 * SECTION:signals
 * @short_description: A means for customization of object behaviour
 *     and a general purpose notification mechanism
 * @title: Signals
 *
 * The basic concept of the signal system is that of the emission
 * of a signal. Signals are introduced per-type and are identified
 * through strings. Signals introduced for a parent type are available
 * in derived types as well, so basically they are a per-type facility
 * that is inherited.
 *
 * A signal emission mainly involves invocation of a certain set of
 * callbacks in precisely defined manner. There are two main categories
 * of such callbacks, per-object ones and user provided ones.
 * (Although signals can deal with any kind of instantiatable type, I'm
 * referring to those types as "object types" in the following, simply
 * because that is the context most users will encounter signals in.)
 * The per-object callbacks are most often referred to as "object method
 * handler" or "default (signal) handler", while user provided callbacks are
 * usually just called "signal handler".
 *
 * The object method handler is provided at signal creation time (this most
 * frequently happens at the end of an object class' creation), while user
 * provided handlers are frequently connected and disconnected to/from a
 * certain signal on certain object instances.
 *
 * A signal emission consists of five stages, unless prematurely stopped:
 *
 * 1. Invocation of the object method handler for %G_SIGNAL_RUN_FIRST signals
 *
 * 2. Invocation of normal user-provided signal handlers (where the @after
 *    flag is not set)
 *
 * 3. Invocation of the object method handler for %G_SIGNAL_RUN_LAST signals
 *
 * 4. Invocation of user provided signal handlers (where the @after flag is set)
 *
 * 5. Invocation of the object method handler for %G_SIGNAL_RUN_CLEANUP signals
 *
 * The user-provided signal handlers are called in the order they were
 * connected in.
 *
 * All handlers may prematurely stop a signal emission, and any number of
 * handlers may be connected, disconnected, blocked or unblocked during
 * a signal emission.
 *
 * There are certain criteria for skipping user handlers in stages 2 and 4
 * of a signal emission.
 *
 * First, user handlers may be blocked. Blocked handlers are omitted during
 * callback invocation, to return from the blocked state, a handler has to
 * get unblocked exactly the same amount of times it has been blocked before.
 *
 * Second, upon emission of a %G_SIGNAL_DETAILED signal, an additional
 * @detail argument passed in to g_signal_emit() has to match the detail
 * argument of the signal handler currently subject to invocation.
 * Specification of no detail argument for signal handlers (omission of the
 * detail part of the signal specification upon connection) serves as a
 * wildcard and matches any detail argument passed in to emission.
 *
 * ## Memory management of signal handlers # {#signal-memory-management}
 *
 * If you are connecting handlers to signals and using a #GObject instance as
 * your signal handler user data, you should remember to pair calls to
 * g_signal_connect() with calls to g_signal_handler_disconnect() or
 * g_signal_handlers_disconnect_by_func(). While signal handlers are
 * automatically disconnected when the object emitting the signal is finalised,
 * they are not automatically disconnected when the signal handler user data is
 * destroyed. If this user data is a #GObject instance, using it from a
 * signal handler after it has been finalised is an error.
 *
 * There are two strategies for managing such user data. The first is to
 * disconnect the signal handler (using g_signal_handler_disconnect() or
 * g_signal_handlers_disconnect_by_func()) when the user data (object) is
 * finalised; this has to be implemented manually. For non-threaded programs,
 * g_signal_connect_object() can be used to implement this automatically.
 * Currently, however, it is unsafe to use in threaded programs.
 *
 * The second is to hold a strong reference on the user data until after the
 * signal is disconnected for other reasons. This can be implemented
 * automatically using g_signal_connect_data().
 *
 * The first approach is recommended, as the second approach can result in
 * effective memory leaks of the user data if the signal handler is never
 * disconnected for some reason.
 */


#define REPORT_BUG      "please report occurrence circumstances to gtk-devel-list@gnome.org"

/* --- typedefs --- */
typedef struct _SignalNode   SignalNode;
typedef struct _SignalKey    SignalKey;
typedef struct _Emission     Emission;
typedef struct _Handler      Handler;
typedef struct _HandlerList  HandlerList;
typedef struct _HandlerMatch HandlerMatch;
typedef enum
{
  EMISSION_STOP,
  EMISSION_RUN,
  EMISSION_HOOK,
  EMISSION_RESTART
} EmissionState;


/* --- prototypes --- */
static inline guint		signal_id_lookup	(GQuark		  quark,
							 GType		  itype);
static	      void		signal_destroy_R	(SignalNode	 *signal_node);
static inline HandlerList*	handler_list_ensure	(guint		  signal_id,
							 gpointer	  instance);
static inline HandlerList*	handler_list_lookup	(guint		  signal_id,
							 gpointer	  instance);
static inline Handler*		handler_new		(guint            signal_id,
							 gpointer         instance,
                                                         gboolean	  after);
static	      void		handler_insert		(guint		  signal_id,
							 gpointer	  instance,
							 Handler	 *handler);
static	      Handler*		handler_lookup		(gpointer	  instance,
							 gulong		  handler_id,
							 GClosure        *closure,
							 guint		 *signal_id_p);
static inline HandlerMatch*	handler_match_prepend	(HandlerMatch	 *list,
							 Handler	 *handler,
							 guint		  signal_id);
static inline HandlerMatch*	handler_match_free1_R	(HandlerMatch	 *node,
							 gpointer	  instance);
static	      HandlerMatch*	handlers_find		(gpointer	  instance,
							 GSignalMatchType mask,
							 guint		  signal_id,
							 GQuark		  detail,
							 GClosure	 *closure,
							 gpointer	  func,
							 gpointer	  data,
							 gboolean	  one_and_only);
static inline void		handler_ref		(Handler	 *handler);
static inline void		handler_unref_R		(guint		  signal_id,
							 gpointer	  instance,
							 Handler	 *handler);
static gint			handler_lists_cmp	(gconstpointer	  node1,
							 gconstpointer	  node2);
static inline void		emission_push		(Emission	 *emission);
static inline void		emission_pop		(Emission	 *emission);
static inline Emission*		emission_find		(guint		  signal_id,
							 GQuark		  detail,
							 gpointer	  instance);
static gint			class_closures_cmp	(gconstpointer	  node1,
							 gconstpointer	  node2);
static gint			signal_key_cmp		(gconstpointer	  node1,
							 gconstpointer	  node2);
static	      gboolean		signal_emit_unlocked_R	(SignalNode	 *node,
							 GQuark		  detail,
							 gpointer	  instance,
							 GValue		 *return_value,
							 const GValue	 *instance_and_params);
static       void               add_invalid_closure_notify    (Handler         *handler,
							       gpointer         instance);
static       void               remove_invalid_closure_notify (Handler         *handler,
							       gpointer         instance);
static       void               invalid_closure_notify  (gpointer         data,
							 GClosure        *closure);
static const gchar *            type_debug_name         (GType            type);
static void                     node_check_deprecated   (const SignalNode *node);
static void                     node_update_single_va_closure (SignalNode *node);


/* --- structures --- */
typedef struct
{
  GSignalAccumulator func;
  gpointer           data;
} SignalAccumulator;
typedef struct
{
  GHook hook;
  GQuark detail;
} SignalHook;
#define	SIGNAL_HOOK(hook)	((SignalHook*) (hook))

struct _SignalNode
{
  /* permanent portion */
  guint              signal_id;
  GType              itype;
  const gchar       *name;
  guint              destroyed : 1;
  
  /* reinitializable portion */
  guint              flags : 9;
  guint              n_params : 8;
  guint              single_va_closure_is_valid : 1;
  guint              single_va_closure_is_after : 1;
  GType		    *param_types; /* mangled with G_SIGNAL_TYPE_STATIC_SCOPE flag */
  GType		     return_type; /* mangled with G_SIGNAL_TYPE_STATIC_SCOPE flag */
  GBSearchArray     *class_closure_bsa;
  SignalAccumulator *accumulator;
  GSignalCMarshaller c_marshaller;
  GSignalCVaMarshaller va_marshaller;
  GHookList         *emission_hooks;

  GClosure *single_va_closure;
};

#define	SINGLE_VA_CLOSURE_EMPTY_MAGIC GINT_TO_POINTER(1)	/* indicates single_va_closure is valid but empty */

struct _SignalKey
{
  GType  itype;
  GQuark quark;
  guint  signal_id;
};

struct _Emission
{
  Emission             *next;
  gpointer              instance;
  GSignalInvocationHint ihint;
  EmissionState         state;
  GType			chain_type;
};

struct _HandlerList
{
  guint    signal_id;
  Handler *handlers;
  Handler *tail_before;  /* normal signal handlers are appended here  */
  Handler *tail_after;   /* CONNECT_AFTER handlers are appended here  */
};

struct _Handler
{
  gulong        sequential_number;
  Handler      *next;
  Handler      *prev;
  GQuark	detail;
  guint         signal_id;
  guint         ref_count;
  guint         block_count : 16;
#define HANDLER_MAX_BLOCK_COUNT (1 << 16)
  guint         after : 1;
  guint         has_invalid_closure_notify : 1;
  GClosure     *closure;
  gpointer      instance;
};
struct _HandlerMatch
{
  Handler      *handler;
  HandlerMatch *next;
  guint         signal_id;
};

typedef struct
{
  GType     instance_type; /* 0 for default closure */
  GClosure *closure;
} ClassClosure;


/* --- variables --- */
static GBSearchArray *g_signal_key_bsa = NULL;
static const GBSearchConfig g_signal_key_bconfig = {
  sizeof (SignalKey),
  signal_key_cmp,
  G_BSEARCH_ARRAY_ALIGN_POWER2,
};
static GBSearchConfig g_signal_hlbsa_bconfig = {
  sizeof (HandlerList),
  handler_lists_cmp,
  0,
};
static GBSearchConfig g_class_closure_bconfig = {
  sizeof (ClassClosure),
  class_closures_cmp,
  0,
};
static GHashTable    *g_handler_list_bsa_ht = NULL;
static Emission      *g_emissions = NULL;
static gulong         g_handler_sequential_number = 1;
static GHashTable    *g_handlers = NULL;

G_LOCK_DEFINE_STATIC (g_signal_mutex);
#define	SIGNAL_LOCK()		G_LOCK (g_signal_mutex)
#define	SIGNAL_UNLOCK()		G_UNLOCK (g_signal_mutex)


/* --- signal nodes --- */
static guint          g_n_signal_nodes = 0;
static SignalNode   **g_signal_nodes = NULL;

static inline SignalNode*
LOOKUP_SIGNAL_NODE (guint signal_id)
{
  if (signal_id < g_n_signal_nodes)
    return g_signal_nodes[signal_id];
  else
    return NULL;
}


/* --- functions --- */
static inline guint
signal_id_lookup (GQuark quark,
		  GType  itype)
{
  GType *ifaces, type = itype;
  SignalKey key;
  guint n_ifaces;

  key.quark = quark;

  /* try looking up signals for this type and its ancestors */
  do
    {
      SignalKey *signal_key;
      
      key.itype = type;
      signal_key = g_bsearch_array_lookup (g_signal_key_bsa, &g_signal_key_bconfig, &key);
      
      if (signal_key)
	return signal_key->signal_id;
      
      type = g_type_parent (type);
    }
  while (type);

  /* no luck, try interfaces it exports */
  ifaces = g_type_interfaces (itype, &n_ifaces);
  while (n_ifaces--)
    {
      SignalKey *signal_key;

      key.itype = ifaces[n_ifaces];
      signal_key = g_bsearch_array_lookup (g_signal_key_bsa, &g_signal_key_bconfig, &key);

      if (signal_key)
	{
	  g_free (ifaces);
	  return signal_key->signal_id;
	}
    }
  g_free (ifaces);
  
  return 0;
}

static gint
class_closures_cmp (gconstpointer node1,
		    gconstpointer node2)
{
  const ClassClosure *c1 = node1, *c2 = node2;
  
  return G_BSEARCH_ARRAY_CMP (c1->instance_type, c2->instance_type);
}

static gint
handler_lists_cmp (gconstpointer node1,
                   gconstpointer node2)
{
  const HandlerList *hlist1 = node1, *hlist2 = node2;
  
  return G_BSEARCH_ARRAY_CMP (hlist1->signal_id, hlist2->signal_id);
}

static inline HandlerList*
handler_list_ensure (guint    signal_id,
		     gpointer instance)
{
  GBSearchArray *hlbsa = g_hash_table_lookup (g_handler_list_bsa_ht, instance);
  HandlerList key;
  
  key.signal_id = signal_id;
  key.handlers    = NULL;
  key.tail_before = NULL;
  key.tail_after  = NULL;
  if (!hlbsa)
    {
      hlbsa = g_bsearch_array_create (&g_signal_hlbsa_bconfig);
      hlbsa = g_bsearch_array_insert (hlbsa, &g_signal_hlbsa_bconfig, &key);
      g_hash_table_insert (g_handler_list_bsa_ht, instance, hlbsa);
    }
  else
    {
      GBSearchArray *o = hlbsa;

      hlbsa = g_bsearch_array_insert (o, &g_signal_hlbsa_bconfig, &key);
      if (hlbsa != o)
	g_hash_table_insert (g_handler_list_bsa_ht, instance, hlbsa);
    }
  return g_bsearch_array_lookup (hlbsa, &g_signal_hlbsa_bconfig, &key);
}

static inline HandlerList*
handler_list_lookup (guint    signal_id,
		     gpointer instance)
{
  GBSearchArray *hlbsa = g_hash_table_lookup (g_handler_list_bsa_ht, instance);
  HandlerList key;
  
  key.signal_id = signal_id;
  
  return hlbsa ? g_bsearch_array_lookup (hlbsa, &g_signal_hlbsa_bconfig, &key) : NULL;
}

static guint
handler_hash (gconstpointer key)
{
  return (guint)((Handler*)key)->sequential_number;
}

static gboolean
handler_equal (gconstpointer a, gconstpointer b)
{
  Handler *ha = (Handler *)a;
  Handler *hb = (Handler *)b;
  return (ha->sequential_number == hb->sequential_number) &&
      (ha->instance  == hb->instance);
}

static Handler*
handler_lookup (gpointer  instance,
		gulong    handler_id,
		GClosure *closure,
		guint    *signal_id_p)
{
  GBSearchArray *hlbsa;

  if (handler_id)
    {
      Handler key;
      key.sequential_number = handler_id;
      key.instance = instance;
      return g_hash_table_lookup (g_handlers, &key);

    }

  hlbsa = g_hash_table_lookup (g_handler_list_bsa_ht, instance);
  
  if (hlbsa)
    {
      guint i;
      
      for (i = 0; i < hlbsa->n_nodes; i++)
        {
          HandlerList *hlist = g_bsearch_array_get_nth (hlbsa, &g_signal_hlbsa_bconfig, i);
          Handler *handler;
          
          for (handler = hlist->handlers; handler; handler = handler->next)
            if (closure ? (handler->closure == closure) : (handler->sequential_number == handler_id))
              {
                if (signal_id_p)
                  *signal_id_p = hlist->signal_id;

                return handler;
              }
        }
    }
  
  return NULL;
}

static inline HandlerMatch*
handler_match_prepend (HandlerMatch *list,
		       Handler      *handler,
		       guint	     signal_id)
{
  HandlerMatch *node;
  
  node = g_slice_new (HandlerMatch);
  node->handler = handler;
  node->next = list;
  node->signal_id = signal_id;
  handler_ref (handler);
  
  return node;
}
static inline HandlerMatch*
handler_match_free1_R (HandlerMatch *node,
		       gpointer      instance)
{
  HandlerMatch *next = node->next;
  
  handler_unref_R (node->signal_id, instance, node->handler);
  g_slice_free (HandlerMatch, node);
  
  return next;
}

static HandlerMatch*
handlers_find (gpointer         instance,
	       GSignalMatchType mask,
	       guint            signal_id,
	       GQuark           detail,
	       GClosure        *closure,
	       gpointer         func,
	       gpointer         data,
	       gboolean         one_and_only)
{
  HandlerMatch *mlist = NULL;
  
  if (mask & G_SIGNAL_MATCH_ID)
    {
      HandlerList *hlist = handler_list_lookup (signal_id, instance);
      Handler *handler;
      SignalNode *node = NULL;
      
      if (mask & G_SIGNAL_MATCH_FUNC)
	{
	  node = LOOKUP_SIGNAL_NODE (signal_id);
	  if (!node || !node->c_marshaller)
	    return NULL;
	}
      
      mask = ~mask;
      for (handler = hlist ? hlist->handlers : NULL; handler; handler = handler->next)
        if (handler->sequential_number &&
	    ((mask & G_SIGNAL_MATCH_DETAIL) || handler->detail == detail) &&
	    ((mask & G_SIGNAL_MATCH_CLOSURE) || handler->closure == closure) &&
            ((mask & G_SIGNAL_MATCH_DATA) || handler->closure->data == data) &&
	    ((mask & G_SIGNAL_MATCH_UNBLOCKED) || handler->block_count == 0) &&
	    ((mask & G_SIGNAL_MATCH_FUNC) || (handler->closure->marshal == node->c_marshaller &&
					      G_REAL_CLOSURE (handler->closure)->meta_marshal == NULL &&
					      ((GCClosure*) handler->closure)->callback == func)))
	  {
	    mlist = handler_match_prepend (mlist, handler, signal_id);
	    if (one_and_only)
	      return mlist;
	  }
    }
  else
    {
      GBSearchArray *hlbsa = g_hash_table_lookup (g_handler_list_bsa_ht, instance);
      
      mask = ~mask;
      if (hlbsa)
        {
          guint i;
          
          for (i = 0; i < hlbsa->n_nodes; i++)
            {
              HandlerList *hlist = g_bsearch_array_get_nth (hlbsa, &g_signal_hlbsa_bconfig, i);
	      SignalNode *node = NULL;
              Handler *handler;
              
	      if (!(mask & G_SIGNAL_MATCH_FUNC))
		{
		  node = LOOKUP_SIGNAL_NODE (hlist->signal_id);
		  if (!node->c_marshaller)
		    continue;
		}
	      
              for (handler = hlist->handlers; handler; handler = handler->next)
		if (handler->sequential_number &&
		    ((mask & G_SIGNAL_MATCH_DETAIL) || handler->detail == detail) &&
                    ((mask & G_SIGNAL_MATCH_CLOSURE) || handler->closure == closure) &&
                    ((mask & G_SIGNAL_MATCH_DATA) || handler->closure->data == data) &&
		    ((mask & G_SIGNAL_MATCH_UNBLOCKED) || handler->block_count == 0) &&
		    ((mask & G_SIGNAL_MATCH_FUNC) || (handler->closure->marshal == node->c_marshaller &&
						      G_REAL_CLOSURE (handler->closure)->meta_marshal == NULL &&
						      ((GCClosure*) handler->closure)->callback == func)))
		  {
		    mlist = handler_match_prepend (mlist, handler, hlist->signal_id);
		    if (one_and_only)
		      return mlist;
		  }
            }
        }
    }
  
  return mlist;
}

static inline Handler*
handler_new (guint signal_id, gpointer instance, gboolean after)
{
  Handler *handler = g_slice_new (Handler);
#ifndef G_DISABLE_CHECKS
  if (g_handler_sequential_number < 1)
    g_error (G_STRLOC ": handler id overflow, %s", REPORT_BUG);
#endif
  
  handler->sequential_number = g_handler_sequential_number++;
  handler->prev = NULL;
  handler->next = NULL;
  handler->detail = 0;
  handler->signal_id = signal_id;
  handler->instance = instance;
  handler->ref_count = 1;
  handler->block_count = 0;
  handler->after = after != FALSE;
  handler->closure = NULL;
  handler->has_invalid_closure_notify = 0;

  g_hash_table_add (g_handlers, handler);
  
  return handler;
}

static inline void
handler_ref (Handler *handler)
{
  g_return_if_fail (handler->ref_count > 0);
  
  handler->ref_count++;
}

static inline void
handler_unref_R (guint    signal_id,
		 gpointer instance,
		 Handler *handler)
{
  g_return_if_fail (handler->ref_count > 0);

  handler->ref_count--;

  if (G_UNLIKELY (handler->ref_count == 0))
    {
      HandlerList *hlist = NULL;

      if (handler->next)
        handler->next->prev = handler->prev;
      if (handler->prev)    /* watch out for g_signal_handlers_destroy()! */
        handler->prev->next = handler->next;
      else
        {
          hlist = handler_list_lookup (signal_id, instance);
          g_assert (hlist != NULL);
          hlist->handlers = handler->next;
        }

      if (instance)
        {
          /*  check if we are removing the handler pointed to by tail_before  */
          if (!handler->after && (!handler->next || handler->next->after))
            {
              if (!hlist)
                hlist = handler_list_lookup (signal_id, instance);
              if (hlist)
                {
                  g_assert (hlist->tail_before == handler); /* paranoid */
                  hlist->tail_before = handler->prev;
                }
            }

          /*  check if we are removing the handler pointed to by tail_after  */
          if (!handler->next)
            {
              if (!hlist)
                hlist = handler_list_lookup (signal_id, instance);
              if (hlist)
                {
                  g_assert (hlist->tail_after == handler); /* paranoid */
                  hlist->tail_after = handler->prev;
                }
            }
        }

      SIGNAL_UNLOCK ();
      g_closure_unref (handler->closure);
      SIGNAL_LOCK ();
      g_slice_free (Handler, handler);
    }
}

static void
handler_insert (guint    signal_id,
		gpointer instance,
		Handler  *handler)
{
  HandlerList *hlist;
  
  g_assert (handler->prev == NULL && handler->next == NULL); /* paranoid */
  
  hlist = handler_list_ensure (signal_id, instance);
  if (!hlist->handlers)
    {
      hlist->handlers = handler;
      if (!handler->after)
        hlist->tail_before = handler;
    }
  else if (handler->after)
    {
      handler->prev = hlist->tail_after;
      hlist->tail_after->next = handler;
    }
  else
    {
      if (hlist->tail_before)
        {
          handler->next = hlist->tail_before->next;
          if (handler->next)
            handler->next->prev = handler;
          handler->prev = hlist->tail_before;
          hlist->tail_before->next = handler;
        }
      else /* insert !after handler into a list of only after handlers */
        {
          handler->next = hlist->handlers;
          if (handler->next)
            handler->next->prev = handler;
          hlist->handlers = handler;
        }
      hlist->tail_before = handler;
    }

  if (!handler->next)
    hlist->tail_after = handler;
}

static void
node_update_single_va_closure (SignalNode *node)
{
  GClosure *closure = NULL;
  gboolean is_after = FALSE;

  /* Fast path single-handler without boxing the arguments in GValues */
  if (G_TYPE_IS_OBJECT (node->itype) &&
      (node->flags & (G_SIGNAL_MUST_COLLECT)) == 0 &&
      (node->emission_hooks == NULL || node->emission_hooks->hooks == NULL))
    {
      GSignalFlags run_type;
      ClassClosure * cc; 
      GBSearchArray *bsa = node->class_closure_bsa;

      if (bsa == NULL || bsa->n_nodes == 0)
	closure = SINGLE_VA_CLOSURE_EMPTY_MAGIC;
      else if (bsa->n_nodes == 1)
	{
	  /* Look for default class closure (can't support non-default as it
	     chains up using GValues */
	  cc = g_bsearch_array_get_nth (bsa, &g_class_closure_bconfig, 0);
	  if (cc->instance_type == 0)
	    {
	      run_type = node->flags & (G_SIGNAL_RUN_FIRST|G_SIGNAL_RUN_LAST|G_SIGNAL_RUN_CLEANUP);
	      /* Only support *one* of run-first or run-last, not multiple or cleanup */
	      if (run_type == G_SIGNAL_RUN_FIRST ||
		  run_type == G_SIGNAL_RUN_LAST)
		{
		  closure = cc->closure;
		  is_after = (run_type == G_SIGNAL_RUN_LAST);
		}
	    }
	}
    }

  node->single_va_closure_is_valid = TRUE;
  node->single_va_closure = closure;
  node->single_va_closure_is_after = is_after;
}

static inline void
emission_push (Emission  *emission)
{
  emission->next = g_emissions;
  g_emissions = emission;
}

static inline void
emission_pop (Emission  *emission)
{
  Emission *node, *last = NULL;

  for (node = g_emissions; node; last = node, node = last->next)
    if (node == emission)
      {
	if (last)
	  last->next = node->next;
	else
	  g_emissions = node->next;
	return;
      }
  g_assert_not_reached ();
}

static inline Emission*
emission_find (guint     signal_id,
	       GQuark    detail,
	       gpointer  instance)
{
  Emission *emission;
  
  for (emission = g_emissions; emission; emission = emission->next)
    if (emission->instance == instance &&
	emission->ihint.signal_id == signal_id &&
	emission->ihint.detail == detail)
      return emission;
  return NULL;
}

static inline Emission*
emission_find_innermost (gpointer instance)
{
  Emission *emission;
  
  for (emission = g_emissions; emission; emission = emission->next)
    if (emission->instance == instance)
      return emission;

  return NULL;
}

static gint
signal_key_cmp (gconstpointer node1,
                gconstpointer node2)
{
  const SignalKey *key1 = node1, *key2 = node2;
  
  if (key1->itype == key2->itype)
    return G_BSEARCH_ARRAY_CMP (key1->quark, key2->quark);
  else
    return G_BSEARCH_ARRAY_CMP (key1->itype, key2->itype);
}

void
_g_signal_init (void)
{
  SIGNAL_LOCK ();
  if (!g_n_signal_nodes)
    {
      /* setup handler list binary searchable array hash table (in german, that'd be one word ;) */
      g_handler_list_bsa_ht = g_hash_table_new (g_direct_hash, NULL);
      g_signal_key_bsa = g_bsearch_array_create (&g_signal_key_bconfig);
      
      /* invalid (0) signal_id */
      g_n_signal_nodes = 1;
      g_signal_nodes = g_renew (SignalNode*, g_signal_nodes, g_n_signal_nodes);
      g_signal_nodes[0] = NULL;
      g_handlers = g_hash_table_new (handler_hash, handler_equal);
    }
  SIGNAL_UNLOCK ();
}

void
_g_signals_destroy (GType itype)
{
  guint i;
  
  SIGNAL_LOCK ();
  for (i = 1; i < g_n_signal_nodes; i++)
    {
      SignalNode *node = g_signal_nodes[i];
      
      if (node->itype == itype)
        {
          if (node->destroyed)
            g_warning (G_STRLOC ": signal \"%s\" of type '%s' already destroyed",
                       node->name,
                       type_debug_name (node->itype));
          else
	    signal_destroy_R (node);
        }
    }
  SIGNAL_UNLOCK ();
}

/**
 * g_signal_stop_emission:
 * @instance: (type GObject.Object): the object whose signal handlers you wish to stop.
 * @signal_id: the signal identifier, as returned by g_signal_lookup().
 * @detail: the detail which the signal was emitted with.
 *
 * Stops a signal's current emission.
 *
 * This will prevent the default method from running, if the signal was
 * %G_SIGNAL_RUN_LAST and you connected normally (i.e. without the "after"
 * flag).
 *
 * Prints a warning if used on a signal which isn't being emitted.
 */
void
g_signal_stop_emission (gpointer instance,
                        guint    signal_id,
			GQuark   detail)
{
  SignalNode *node;
  
  g_return_if_fail (G_TYPE_CHECK_INSTANCE (instance));
  g_return_if_fail (signal_id > 0);
  
  SIGNAL_LOCK ();
  node = LOOKUP_SIGNAL_NODE (signal_id);
  if (node && detail && !(node->flags & G_SIGNAL_DETAILED))
    {
      g_warning ("%s: signal id '%u' does not support detail (%u)", G_STRLOC, signal_id, detail);
      SIGNAL_UNLOCK ();
      return;
    }
  if (node && g_type_is_a (G_TYPE_FROM_INSTANCE (instance), node->itype))
    {
      Emission *emission = emission_find (signal_id, detail, instance);
      
      if (emission)
        {
          if (emission->state == EMISSION_HOOK)
            g_warning (G_STRLOC ": emission of signal \"%s\" for instance '%p' cannot be stopped from emission hook",
                       node->name, instance);
          else if (emission->state == EMISSION_RUN)
            emission->state = EMISSION_STOP;
        }
      else
        g_warning (G_STRLOC ": no emission of signal \"%s\" to stop for instance '%p'",
                   node->name, instance);
    }
  else
    g_warning ("%s: signal id '%u' is invalid for instance '%p'", G_STRLOC, signal_id, instance);
  SIGNAL_UNLOCK ();
}

static void
signal_finalize_hook (GHookList *hook_list,
		      GHook     *hook)
{
  GDestroyNotify destroy = hook->destroy;

  if (destroy)
    {
      hook->destroy = NULL;
      SIGNAL_UNLOCK ();
      destroy (hook->data);
      SIGNAL_LOCK ();
    }
}

/**
 * g_signal_add_emission_hook:
 * @signal_id: the signal identifier, as returned by g_signal_lookup().
 * @detail: the detail on which to call the hook.
 * @hook_func: a #GSignalEmissionHook function.
 * @hook_data: user data for @hook_func.
 * @data_destroy: a #GDestroyNotify for @hook_data.
 *
 * Adds an emission hook for a signal, which will get called for any emission
 * of that signal, independent of the instance. This is possible only
 * for signals which don't have #G_SIGNAL_NO_HOOKS flag set.
 *
 * Returns: the hook id, for later use with g_signal_remove_emission_hook().
 */
gulong
g_signal_add_emission_hook (guint               signal_id,
			    GQuark              detail,
			    GSignalEmissionHook hook_func,
			    gpointer            hook_data,
			    GDestroyNotify      data_destroy)
{
  static gulong seq_hook_id = 1;
  SignalNode *node;
  GHook *hook;
  SignalHook *signal_hook;

  g_return_val_if_fail (signal_id > 0, 0);
  g_return_val_if_fail (hook_func != NULL, 0);

  SIGNAL_LOCK ();
  node = LOOKUP_SIGNAL_NODE (signal_id);
  if (!node || node->destroyed)
    {
      g_warning ("%s: invalid signal id '%u'", G_STRLOC, signal_id);
      SIGNAL_UNLOCK ();
      return 0;
    }
  if (node->flags & G_SIGNAL_NO_HOOKS) 
    {
      g_warning ("%s: signal id '%u' does not support emission hooks (G_SIGNAL_NO_HOOKS flag set)", G_STRLOC, signal_id);
      SIGNAL_UNLOCK ();
      return 0;
    }
  if (detail && !(node->flags & G_SIGNAL_DETAILED))
    {
      g_warning ("%s: signal id '%u' does not support detail (%u)", G_STRLOC, signal_id, detail);
      SIGNAL_UNLOCK ();
      return 0;
    }
    node->single_va_closure_is_valid = FALSE;
  if (!node->emission_hooks)
    {
      node->emission_hooks = g_new (GHookList, 1);
      g_hook_list_init (node->emission_hooks, sizeof (SignalHook));
      node->emission_hooks->finalize_hook = signal_finalize_hook;
    }

  node_check_deprecated (node);

  hook = g_hook_alloc (node->emission_hooks);
  hook->data = hook_data;
  hook->func = (gpointer) hook_func;
  hook->destroy = data_destroy;
  signal_hook = SIGNAL_HOOK (hook);
  signal_hook->detail = detail;
  node->emission_hooks->seq_id = seq_hook_id;
  g_hook_append (node->emission_hooks, hook);
  seq_hook_id = node->emission_hooks->seq_id;

  SIGNAL_UNLOCK ();

  return hook->hook_id;
}

/**
 * g_signal_remove_emission_hook:
 * @signal_id: the id of the signal
 * @hook_id: the id of the emission hook, as returned by
 *  g_signal_add_emission_hook()
 *
 * Deletes an emission hook.
 */
void
g_signal_remove_emission_hook (guint  signal_id,
			       gulong hook_id)
{
  SignalNode *node;

  g_return_if_fail (signal_id > 0);
  g_return_if_fail (hook_id > 0);

  SIGNAL_LOCK ();
  node = LOOKUP_SIGNAL_NODE (signal_id);
  if (!node || node->destroyed)
    {
      g_warning ("%s: invalid signal id '%u'", G_STRLOC, signal_id);
      goto out;
    }
  else if (!node->emission_hooks || !g_hook_destroy (node->emission_hooks, hook_id))
    g_warning ("%s: signal \"%s\" had no hook (%lu) to remove", G_STRLOC, node->name, hook_id);

  node->single_va_closure_is_valid = FALSE;

 out:
  SIGNAL_UNLOCK ();
}

static inline guint
signal_parse_name (const gchar *name,
		   GType        itype,
		   GQuark      *detail_p,
		   gboolean     force_quark)
{
  const gchar *colon = strchr (name, ':');
  guint signal_id;
  
  if (!colon)
    {
      signal_id = signal_id_lookup (g_quark_try_string (name), itype);
      if (signal_id && detail_p)
	*detail_p = 0;
    }
  else if (colon[1] == ':')
    {
      gchar buffer[32];
      guint l = colon - name;
      
      if (l < 32)
	{
	  memcpy (buffer, name, l);
	  buffer[l] = 0;
	  signal_id = signal_id_lookup (g_quark_try_string (buffer), itype);
	}
      else
	{
	  gchar *signal = g_new (gchar, l + 1);
	  
	  memcpy (signal, name, l);
	  signal[l] = 0;
	  signal_id = signal_id_lookup (g_quark_try_string (signal), itype);
	  g_free (signal);
	}
      
      if (signal_id && detail_p)
	*detail_p = colon[2] ? (force_quark ? g_quark_from_string : g_quark_try_string) (colon + 2) : 0;
    }
  else
    signal_id = 0;
  return signal_id;
}

/**
 * g_signal_parse_name:
 * @detailed_signal: a string of the form "signal-name::detail".
 * @itype: The interface/instance type that introduced "signal-name".
 * @signal_id_p: (out): Location to store the signal id.
 * @detail_p: (out): Location to store the detail quark.
 * @force_detail_quark: %TRUE forces creation of a #GQuark for the detail.
 *
 * Internal function to parse a signal name into its @signal_id
 * and @detail quark.
 *
 * Returns: Whether the signal name could successfully be parsed and @signal_id_p and @detail_p contain valid return values.
 */
gboolean
g_signal_parse_name (const gchar *detailed_signal,
		     GType        itype,
		     guint       *signal_id_p,
		     GQuark      *detail_p,
		     gboolean	  force_detail_quark)
{
  SignalNode *node;
  GQuark detail = 0;
  guint signal_id;
  
  g_return_val_if_fail (detailed_signal != NULL, FALSE);
  g_return_val_if_fail (G_TYPE_IS_INSTANTIATABLE (itype) || G_TYPE_IS_INTERFACE (itype), FALSE);
  
  SIGNAL_LOCK ();
  signal_id = signal_parse_name (detailed_signal, itype, &detail, force_detail_quark);
  SIGNAL_UNLOCK ();

  node = signal_id ? LOOKUP_SIGNAL_NODE (signal_id) : NULL;
  if (!node || node->destroyed ||
      (detail && !(node->flags & G_SIGNAL_DETAILED)))
    return FALSE;

  if (signal_id_p)
    *signal_id_p = signal_id;
  if (detail_p)
    *detail_p = detail;
  
  return TRUE;
}

/**
 * g_signal_stop_emission_by_name:
 * @instance: (type GObject.Object): the object whose signal handlers you wish to stop.
 * @detailed_signal: a string of the form "signal-name::detail".
 *
 * Stops a signal's current emission.
 *
 * This is just like g_signal_stop_emission() except it will look up the
 * signal id for you.
 */
void
g_signal_stop_emission_by_name (gpointer     instance,
				const gchar *detailed_signal)
{
  guint signal_id;
  GQuark detail = 0;
  GType itype;
  
  g_return_if_fail (G_TYPE_CHECK_INSTANCE (instance));
  g_return_if_fail (detailed_signal != NULL);
  
  SIGNAL_LOCK ();
  itype = G_TYPE_FROM_INSTANCE (instance);
  signal_id = signal_parse_name (detailed_signal, itype, &detail, TRUE);
  if (signal_id)
    {
      SignalNode *node = LOOKUP_SIGNAL_NODE (signal_id);
      
      if (detail && !(node->flags & G_SIGNAL_DETAILED))
	g_warning ("%s: signal '%s' does not support details", G_STRLOC, detailed_signal);
      else if (!g_type_is_a (itype, node->itype))
        g_warning ("%s: signal '%s' is invalid for instance '%p' of type '%s'",
                   G_STRLOC, detailed_signal, instance, g_type_name (itype));
      else
	{
	  Emission *emission = emission_find (signal_id, detail, instance);
	  
	  if (emission)
	    {
	      if (emission->state == EMISSION_HOOK)
		g_warning (G_STRLOC ": emission of signal \"%s\" for instance '%p' cannot be stopped from emission hook",
			   node->name, instance);
	      else if (emission->state == EMISSION_RUN)
		emission->state = EMISSION_STOP;
	    }
	  else
	    g_warning (G_STRLOC ": no emission of signal \"%s\" to stop for instance '%p'",
		       node->name, instance);
	}
    }
  else
    g_warning ("%s: signal '%s' is invalid for instance '%p' of type '%s'",
               G_STRLOC, detailed_signal, instance, g_type_name (itype));
  SIGNAL_UNLOCK ();
}

/**
 * g_signal_lookup:
 * @name: the signal's name.
 * @itype: the type that the signal operates on.
 *
 * Given the name of the signal and the type of object it connects to, gets
 * the signal's identifying integer. Emitting the signal by number is
 * somewhat faster than using the name each time.
 *
 * Also tries the ancestors of the given type.
 *
 * See g_signal_new() for details on allowed signal names.
 *
 * Returns: the signal's identifying number, or 0 if no signal was found.
 */
guint
g_signal_lookup (const gchar *name,
                 GType        itype)
{
  guint signal_id;
  g_return_val_if_fail (name != NULL, 0);
  g_return_val_if_fail (G_TYPE_IS_INSTANTIATABLE (itype) || G_TYPE_IS_INTERFACE (itype), 0);
  
  SIGNAL_LOCK ();
  signal_id = signal_id_lookup (g_quark_try_string (name), itype);
  SIGNAL_UNLOCK ();
  if (!signal_id)
    {
      /* give elaborate warnings */
      if (!g_type_name (itype))
	g_warning (G_STRLOC ": unable to lookup signal \"%s\" for invalid type id '%"G_GSIZE_FORMAT"'",
		   name, itype);
      else if (!G_TYPE_IS_INSTANTIATABLE (itype))
	g_warning (G_STRLOC ": unable to lookup signal \"%s\" for non instantiatable type '%s'",
		   name, g_type_name (itype));
      else if (!g_type_class_peek (itype))
	g_warning (G_STRLOC ": unable to lookup signal \"%s\" of unloaded type '%s'",
		   name, g_type_name (itype));
    }
  
  return signal_id;
}

/**
 * g_signal_list_ids:
 * @itype: Instance or interface type.
 * @n_ids: Location to store the number of signal ids for @itype.
 *
 * Lists the signals by id that a certain instance or interface type
 * created. Further information about the signals can be acquired through
 * g_signal_query().
 *
 * Returns: (array length=n_ids) (transfer full): Newly allocated array of signal IDs.
 */
guint*
g_signal_list_ids (GType  itype,
		   guint *n_ids)
{
  SignalKey *keys;
  GArray *result;
  guint n_nodes;
  guint i;
  
  g_return_val_if_fail (G_TYPE_IS_INSTANTIATABLE (itype) || G_TYPE_IS_INTERFACE (itype), NULL);
  g_return_val_if_fail (n_ids != NULL, NULL);
  
  SIGNAL_LOCK ();
  keys = g_bsearch_array_get_nth (g_signal_key_bsa, &g_signal_key_bconfig, 0);
  n_nodes = g_bsearch_array_get_n_nodes (g_signal_key_bsa);
  result = g_array_new (FALSE, FALSE, sizeof (guint));
  
  for (i = 0; i < n_nodes; i++)
    if (keys[i].itype == itype)
      {
	const gchar *name = g_quark_to_string (keys[i].quark);
	
	/* Signal names with "_" in them are aliases to the same
	 * name with "-" instead of "_".
	 */
	if (!strchr (name, '_'))
	  g_array_append_val (result, keys[i].signal_id);
      }
  *n_ids = result->len;
  SIGNAL_UNLOCK ();
  if (!n_nodes)
    {
      /* give elaborate warnings */
      if (!g_type_name (itype))
	g_warning (G_STRLOC ": unable to list signals for invalid type id '%"G_GSIZE_FORMAT"'",
		   itype);
      else if (!G_TYPE_IS_INSTANTIATABLE (itype) && !G_TYPE_IS_INTERFACE (itype))
	g_warning (G_STRLOC ": unable to list signals of non instantiatable type '%s'",
		   g_type_name (itype));
      else if (!g_type_class_peek (itype) && !G_TYPE_IS_INTERFACE (itype))
	g_warning (G_STRLOC ": unable to list signals of unloaded type '%s'",
		   g_type_name (itype));
    }
  
  return (guint*) g_array_free (result, FALSE);
}

/**
 * g_signal_name:
 * @signal_id: the signal's identifying number.
 *
 * Given the signal's identifier, finds its name.
 *
 * Two different signals may have the same name, if they have differing types.
 *
 * Returns: the signal name, or %NULL if the signal number was invalid.
 */
const gchar *
g_signal_name (guint signal_id)
{
  SignalNode *node;
  const gchar *name;
  
  SIGNAL_LOCK ();
  node = LOOKUP_SIGNAL_NODE (signal_id);
  name = node ? node->name : NULL;
  SIGNAL_UNLOCK ();
  
  return (char*) name;
}

/**
 * g_signal_query:
 * @signal_id: The signal id of the signal to query information for.
 * @query: (out caller-allocates): A user provided structure that is
 *  filled in with constant values upon success.
 *
 * Queries the signal system for in-depth information about a
 * specific signal. This function will fill in a user-provided
 * structure to hold signal-specific information. If an invalid
 * signal id is passed in, the @signal_id member of the #GSignalQuery
 * is 0. All members filled into the #GSignalQuery structure should
 * be considered constant and have to be left untouched.
 */
void
g_signal_query (guint         signal_id,
		GSignalQuery *query)
{
  SignalNode *node;
  
  g_return_if_fail (query != NULL);
  
  SIGNAL_LOCK ();
  node = LOOKUP_SIGNAL_NODE (signal_id);
  if (!node || node->destroyed)
    query->signal_id = 0;
  else
    {
      query->signal_id = node->signal_id;
      query->signal_name = node->name;
      query->itype = node->itype;
      query->signal_flags = node->flags;
      query->return_type = node->return_type;
      query->n_params = node->n_params;
      query->param_types = node->param_types;
    }
  SIGNAL_UNLOCK ();
}

/**
 * g_signal_new:
 * @signal_name: the name for the signal
 * @itype: the type this signal pertains to. It will also pertain to
 *  types which are derived from this type.
 * @signal_flags: a combination of #GSignalFlags specifying detail of when
 *  the default handler is to be invoked. You should at least specify
 *  %G_SIGNAL_RUN_FIRST or %G_SIGNAL_RUN_LAST.
 * @class_offset: The offset of the function pointer in the class structure
 *  for this type. Used to invoke a class method generically. Pass 0 to
 *  not associate a class method slot with this signal.
 * @accumulator: the accumulator for this signal; may be %NULL.
 * @accu_data: user data for the @accumulator.
 * @c_marshaller: (nullable): the function to translate arrays of parameter
 *  values to signal emissions into C language callback invocations or %NULL.
 * @return_type: the type of return value, or #G_TYPE_NONE for a signal
 *  without a return value.
 * @n_params: the number of parameter types to follow.
 * @...: a list of types, one for each parameter.
 *
 * Creates a new signal. (This is usually done in the class initializer.)
 *
 * A signal name consists of segments consisting of ASCII letters and
 * digits, separated by either the '-' or '_' character. The first
 * character of a signal name must be a letter. Names which violate these
 * rules lead to undefined behaviour of the GSignal system.
 *
 * When registering a signal and looking up a signal, either separator can
 * be used, but they cannot be mixed.
 *
 * If 0 is used for @class_offset subclasses cannot override the class handler
 * in their class_init method by doing super_class->signal_handler = my_signal_handler.
 * Instead they will have to use g_signal_override_class_handler().
 *
 * If c_marshaller is %NULL, g_cclosure_marshal_generic() will be used as
 * the marshaller for this signal.
 *
 * Returns: the signal id
 */
guint
g_signal_new (const gchar	 *signal_name,
	      GType		  itype,
	      GSignalFlags	  signal_flags,
	      guint               class_offset,
	      GSignalAccumulator  accumulator,
	      gpointer		  accu_data,
	      GSignalCMarshaller  c_marshaller,
	      GType		  return_type,
	      guint		  n_params,
	      ...)
{
  va_list args;
  guint signal_id;

  g_return_val_if_fail (signal_name != NULL, 0);
  
  va_start (args, n_params);

  signal_id = g_signal_new_valist (signal_name, itype, signal_flags,
                                   class_offset ? g_signal_type_cclosure_new (itype, class_offset) : NULL,
				   accumulator, accu_data, c_marshaller,
                                   return_type, n_params, args);

  va_end (args);

  return signal_id;
}

/**
 * g_signal_new_class_handler:
 * @signal_name: the name for the signal
 * @itype: the type this signal pertains to. It will also pertain to
 *  types which are derived from this type.
 * @signal_flags: a combination of #GSignalFlags specifying detail of when
 *  the default handler is to be invoked. You should at least specify
 *  %G_SIGNAL_RUN_FIRST or %G_SIGNAL_RUN_LAST.
 * @class_handler: a #GCallback which acts as class implementation of
 *  this signal. Used to invoke a class method generically. Pass %NULL to
 *  not associate a class method with this signal.
 * @accumulator: the accumulator for this signal; may be %NULL.
 * @accu_data: user data for the @accumulator.
 * @c_marshaller: (nullable): the function to translate arrays of parameter
 *  values to signal emissions into C language callback invocations or %NULL.
 * @return_type: the type of return value, or #G_TYPE_NONE for a signal
 *  without a return value.
 * @n_params: the number of parameter types to follow.
 * @...: a list of types, one for each parameter.
 *
 * Creates a new signal. (This is usually done in the class initializer.)
 *
 * This is a variant of g_signal_new() that takes a C callback instead
 * off a class offset for the signal's class handler. This function
 * doesn't need a function pointer exposed in the class structure of
 * an object definition, instead the function pointer is passed
 * directly and can be overriden by derived classes with
 * g_signal_override_class_closure() or
 * g_signal_override_class_handler()and chained to with
 * g_signal_chain_from_overridden() or
 * g_signal_chain_from_overridden_handler().
 *
 * See g_signal_new() for information about signal names.
 *
 * If c_marshaller is %NULL, g_cclosure_marshal_generic() will be used as
 * the marshaller for this signal.
 *
 * Returns: the signal id
 *
 * Since: 2.18
 */
guint
g_signal_new_class_handler (const gchar        *signal_name,
                            GType               itype,
                            GSignalFlags        signal_flags,
                            GCallback           class_handler,
                            GSignalAccumulator  accumulator,
                            gpointer            accu_data,
                            GSignalCMarshaller  c_marshaller,
                            GType               return_type,
                            guint               n_params,
                            ...)
{
  va_list args;
  guint signal_id;

  g_return_val_if_fail (signal_name != NULL, 0);

  va_start (args, n_params);

  signal_id = g_signal_new_valist (signal_name, itype, signal_flags,
                                   class_handler ? g_cclosure_new (class_handler, NULL, NULL) : NULL,
                                   accumulator, accu_data, c_marshaller,
                                   return_type, n_params, args);

  va_end (args);

  return signal_id;
}

static inline ClassClosure*
signal_find_class_closure (SignalNode *node,
			   GType       itype)
{
  GBSearchArray *bsa = node->class_closure_bsa;
  ClassClosure *cc;

  if (bsa)
    {
      ClassClosure key;

      /* cc->instance_type is 0 for default closure */

      if (g_bsearch_array_get_n_nodes (bsa) == 1)
        {
          cc = g_bsearch_array_get_nth (bsa, &g_class_closure_bconfig, 0);
          if (cc && cc->instance_type == 0) /* check for default closure */
            return cc;
        }

      key.instance_type = itype;
      cc = g_bsearch_array_lookup (bsa, &g_class_closure_bconfig, &key);
      while (!cc && key.instance_type)
	{
	  key.instance_type = g_type_parent (key.instance_type);
	  cc = g_bsearch_array_lookup (bsa, &g_class_closure_bconfig, &key);
	}
    }
  else
    cc = NULL;
  return cc;
}

static inline GClosure*
signal_lookup_closure (SignalNode    *node,
		       GTypeInstance *instance)
{
  ClassClosure *cc;

  cc = signal_find_class_closure (node, G_TYPE_FROM_INSTANCE (instance));
  return cc ? cc->closure : NULL;
}

static void
signal_add_class_closure (SignalNode *node,
			  GType       itype,
			  GClosure   *closure)
{
  ClassClosure key;

  node->single_va_closure_is_valid = FALSE;

  if (!node->class_closure_bsa)
    node->class_closure_bsa = g_bsearch_array_create (&g_class_closure_bconfig);
  key.instance_type = itype;
  key.closure = g_closure_ref (closure);
  node->class_closure_bsa = g_bsearch_array_insert (node->class_closure_bsa,
						    &g_class_closure_bconfig,
						    &key);
  g_closure_sink (closure);
  if (node->c_marshaller && closure && G_CLOSURE_NEEDS_MARSHAL (closure))
    {
      g_closure_set_marshal (closure, node->c_marshaller);
      if (node->va_marshaller)
	_g_closure_set_va_marshal (closure, node->va_marshaller);
    }
}

/**
 * g_signal_newv:
 * @signal_name: the name for the signal
 * @itype: the type this signal pertains to. It will also pertain to
 *     types which are derived from this type
 * @signal_flags: a combination of #GSignalFlags specifying detail of when
 *     the default handler is to be invoked. You should at least specify
 *     %G_SIGNAL_RUN_FIRST or %G_SIGNAL_RUN_LAST
 * @class_closure: (nullable): The closure to invoke on signal emission;
 *     may be %NULL
 * @accumulator: (nullable): the accumulator for this signal; may be %NULL
 * @accu_data: user data for the @accumulator
 * @c_marshaller: (nullable): the function to translate arrays of
 *     parameter values to signal emissions into C language callback
 *     invocations or %NULL
 * @return_type: the type of return value, or #G_TYPE_NONE for a signal
 *     without a return value
 * @n_params: the length of @param_types
 * @param_types: (array length=n_params): an array of types, one for
 *     each parameter
 *
 * Creates a new signal. (This is usually done in the class initializer.)
 *
 * See g_signal_new() for details on allowed signal names.
 *
 * If c_marshaller is %NULL, g_cclosure_marshal_generic() will be used as
 * the marshaller for this signal.
 *
 * Returns: the signal id
 */
guint
g_signal_newv (const gchar       *signal_name,
               GType              itype,
               GSignalFlags       signal_flags,
               GClosure          *class_closure,
               GSignalAccumulator accumulator,
	       gpointer		  accu_data,
               GSignalCMarshaller c_marshaller,
               GType		  return_type,
               guint              n_params,
               GType		 *param_types)
{
  gchar *name;
  guint signal_id, i;
  SignalNode *node;
  GSignalCMarshaller builtin_c_marshaller;
  GSignalCVaMarshaller builtin_va_marshaller;
  GSignalCVaMarshaller va_marshaller;
  
  g_return_val_if_fail (signal_name != NULL, 0);
  g_return_val_if_fail (G_TYPE_IS_INSTANTIATABLE (itype) || G_TYPE_IS_INTERFACE (itype), 0);
  if (n_params)
    g_return_val_if_fail (param_types != NULL, 0);
  g_return_val_if_fail ((return_type & G_SIGNAL_TYPE_STATIC_SCOPE) == 0, 0);
  if (return_type == (G_TYPE_NONE & ~G_SIGNAL_TYPE_STATIC_SCOPE))
    g_return_val_if_fail (accumulator == NULL, 0);
  if (!accumulator)
    g_return_val_if_fail (accu_data == NULL, 0);

  name = g_strdup (signal_name);
  g_strdelimit (name, G_STR_DELIMITERS ":^", '_');  /* FIXME do character checks like for types */
  
  SIGNAL_LOCK ();
  
  signal_id = signal_id_lookup (g_quark_try_string (name), itype);
  node = LOOKUP_SIGNAL_NODE (signal_id);
  if (node && !node->destroyed)
    {
      g_warning (G_STRLOC ": signal \"%s\" already exists in the '%s' %s",
                 name,
                 type_debug_name (node->itype),
                 G_TYPE_IS_INTERFACE (node->itype) ? "interface" : "class ancestry");
      g_free (name);
      SIGNAL_UNLOCK ();
      return 0;
    }
  if (node && node->itype != itype)
    {
      g_warning (G_STRLOC ": signal \"%s\" for type '%s' was previously created for type '%s'",
                 name,
                 type_debug_name (itype),
                 type_debug_name (node->itype));
      g_free (name);
      SIGNAL_UNLOCK ();
      return 0;
    }
  for (i = 0; i < n_params; i++)
    if (!G_TYPE_IS_VALUE (param_types[i] & ~G_SIGNAL_TYPE_STATIC_SCOPE))
      {
	g_warning (G_STRLOC ": parameter %d of type '%s' for signal \"%s::%s\" is not a value type",
		   i + 1, type_debug_name (param_types[i]), type_debug_name (itype), name);
	g_free (name);
	SIGNAL_UNLOCK ();
	return 0;
      }
  if (return_type != G_TYPE_NONE && !G_TYPE_IS_VALUE (return_type & ~G_SIGNAL_TYPE_STATIC_SCOPE))
    {
      g_warning (G_STRLOC ": return value of type '%s' for signal \"%s::%s\" is not a value type",
		 type_debug_name (return_type), type_debug_name (itype), name);
      g_free (name);
      SIGNAL_UNLOCK ();
      return 0;
    }
  if (return_type != G_TYPE_NONE &&
      (signal_flags & (G_SIGNAL_RUN_FIRST | G_SIGNAL_RUN_LAST | G_SIGNAL_RUN_CLEANUP)) == G_SIGNAL_RUN_FIRST)
    {
      g_warning (G_STRLOC ": signal \"%s::%s\" has return type '%s' and is only G_SIGNAL_RUN_FIRST",
		 type_debug_name (itype), name, type_debug_name (return_type));
      g_free (name);
      SIGNAL_UNLOCK ();
      return 0;
    }
  
  /* setup permanent portion of signal node */
  if (!node)
    {
      SignalKey key;
      
      signal_id = g_n_signal_nodes++;
      node = g_new (SignalNode, 1);
      node->signal_id = signal_id;
      g_signal_nodes = g_renew (SignalNode*, g_signal_nodes, g_n_signal_nodes);
      g_signal_nodes[signal_id] = node;
      node->itype = itype;
      node->name = name;
      key.itype = itype;
      key.quark = g_quark_from_string (node->name);
      key.signal_id = signal_id;
      g_signal_key_bsa = g_bsearch_array_insert (g_signal_key_bsa, &g_signal_key_bconfig, &key);
      g_strdelimit (name, "_", '-');
      node->name = g_intern_string (name);
      key.quark = g_quark_from_string (name);
      g_signal_key_bsa = g_bsearch_array_insert (g_signal_key_bsa, &g_signal_key_bconfig, &key);

      TRACE(GOBJECT_SIGNAL_NEW(signal_id, name, itype));
    }
  node->destroyed = FALSE;

  /* setup reinitializable portion */
  node->single_va_closure_is_valid = FALSE;
  node->flags = signal_flags & G_SIGNAL_FLAGS_MASK;
  node->n_params = n_params;
  node->param_types = g_memdup (param_types, sizeof (GType) * n_params);
  node->return_type = return_type;
  node->class_closure_bsa = NULL;
  if (accumulator)
    {
      node->accumulator = g_new (SignalAccumulator, 1);
      node->accumulator->func = accumulator;
      node->accumulator->data = accu_data;
    }
  else
    node->accumulator = NULL;

  builtin_c_marshaller = NULL;
  builtin_va_marshaller = NULL;

  /* Pick up built-in va marshallers for standard types, and
     instead of generic marshaller if no marshaller specified */
  if (n_params == 0 && return_type == G_TYPE_NONE)
    {
      builtin_c_marshaller = g_cclosure_marshal_VOID__VOID;
      builtin_va_marshaller = g_cclosure_marshal_VOID__VOIDv;
    }
  else if (n_params == 1 && return_type == G_TYPE_NONE)
    {
#define ADD_CHECK(__type__) \
      else if (g_type_is_a (param_types[0] & ~G_SIGNAL_TYPE_STATIC_SCOPE, G_TYPE_ ##__type__))         \
	{                                                                \
	  builtin_c_marshaller = g_cclosure_marshal_VOID__ ## __type__;  \
	  builtin_va_marshaller = g_cclosure_marshal_VOID__ ## __type__ ##v;     \
	}

      if (0) {}
      ADD_CHECK (BOOLEAN)
      ADD_CHECK (CHAR)
      ADD_CHECK (UCHAR)
      ADD_CHECK (INT)
      ADD_CHECK (UINT)
      ADD_CHECK (LONG)
      ADD_CHECK (ULONG)
      ADD_CHECK (ENUM)
      ADD_CHECK (FLAGS)
      ADD_CHECK (FLOAT)
      ADD_CHECK (DOUBLE)
      ADD_CHECK (STRING)
      ADD_CHECK (PARAM)
      ADD_CHECK (BOXED)
      ADD_CHECK (POINTER)
      ADD_CHECK (OBJECT)
      ADD_CHECK (VARIANT)
    }

  if (c_marshaller == NULL)
    {
      if (builtin_c_marshaller)
        {
	  c_marshaller = builtin_c_marshaller;
          va_marshaller = builtin_va_marshaller;
        }
      else
	{
	  c_marshaller = g_cclosure_marshal_generic;
	  va_marshaller = g_cclosure_marshal_generic_va;
	}
    }
  else
    va_marshaller = NULL;

  node->c_marshaller = c_marshaller;
  node->va_marshaller = va_marshaller;
  node->emission_hooks = NULL;
  if (class_closure)
    signal_add_class_closure (node, 0, class_closure);

  SIGNAL_UNLOCK ();

  g_free (name);

  return signal_id;
}

/**
 * g_signal_set_va_marshaller:
 * @signal_id: the signal id
 * @instance_type: the instance type on which to set the marshaller.
 * @va_marshaller: the marshaller to set.
 *
 * Change the #GSignalCVaMarshaller used for a given signal.  This is a
 * specialised form of the marshaller that can often be used for the
 * common case of a single connected signal handler and avoids the
 * overhead of #GValue.  Its use is optional.
 *
 * Since: 2.32
 */
void
g_signal_set_va_marshaller (guint              signal_id,
			    GType              instance_type,
			    GSignalCVaMarshaller va_marshaller)
{
  SignalNode *node;
  
  g_return_if_fail (signal_id > 0);
  g_return_if_fail (va_marshaller != NULL);
  
  SIGNAL_LOCK ();
  node = LOOKUP_SIGNAL_NODE (signal_id);
  if (node)
    {
      node->va_marshaller = va_marshaller;
      if (node->class_closure_bsa)
	{
	  ClassClosure *cc = g_bsearch_array_get_nth (node->class_closure_bsa, &g_class_closure_bconfig, 0);
	  if (cc->closure->marshal == node->c_marshaller)
	    _g_closure_set_va_marshal (cc->closure, va_marshaller);
	}

      node->single_va_closure_is_valid = FALSE;
    }

  SIGNAL_UNLOCK ();
}


/**
 * g_signal_new_valist:
 * @signal_name: the name for the signal
 * @itype: the type this signal pertains to. It will also pertain to
 *  types which are derived from this type.
 * @signal_flags: a combination of #GSignalFlags specifying detail of when
 *  the default handler is to be invoked. You should at least specify
 *  %G_SIGNAL_RUN_FIRST or %G_SIGNAL_RUN_LAST.
 * @class_closure: The closure to invoke on signal emission; may be %NULL.
 * @accumulator: the accumulator for this signal; may be %NULL.
 * @accu_data: user data for the @accumulator.
 * @c_marshaller: (nullable): the function to translate arrays of parameter
 *  values to signal emissions into C language callback invocations or %NULL.
 * @return_type: the type of return value, or #G_TYPE_NONE for a signal
 *  without a return value.
 * @n_params: the number of parameter types in @args.
 * @args: va_list of #GType, one for each parameter.
 *
 * Creates a new signal. (This is usually done in the class initializer.)
 *
 * See g_signal_new() for details on allowed signal names.
 *
 * If c_marshaller is %NULL, g_cclosure_marshal_generic() will be used as
 * the marshaller for this signal.
 *
 * Returns: the signal id
 */
guint
g_signal_new_valist (const gchar       *signal_name,
                     GType              itype,
                     GSignalFlags       signal_flags,
                     GClosure          *class_closure,
                     GSignalAccumulator accumulator,
		     gpointer		accu_data,
                     GSignalCMarshaller c_marshaller,
                     GType              return_type,
                     guint              n_params,
                     va_list            args)
{
  GType *param_types;
  guint i;
  guint signal_id;

  if (n_params > 0)
    {
      param_types = g_new (GType, n_params);

      for (i = 0; i < n_params; i++)
	param_types[i] = va_arg (args, GType);
    }
  else
    param_types = NULL;

  signal_id = g_signal_newv (signal_name, itype, signal_flags,
			     class_closure, accumulator, accu_data, c_marshaller,
			     return_type, n_params, param_types);
  g_free (param_types);

  return signal_id;
}

static void
signal_destroy_R (SignalNode *signal_node)
{
  SignalNode node = *signal_node;

  signal_node->destroyed = TRUE;
  
  /* reentrancy caution, zero out real contents first */
  signal_node->single_va_closure_is_valid = FALSE;
  signal_node->n_params = 0;
  signal_node->param_types = NULL;
  signal_node->return_type = 0;
  signal_node->class_closure_bsa = NULL;
  signal_node->accumulator = NULL;
  signal_node->c_marshaller = NULL;
  signal_node->va_marshaller = NULL;
  signal_node->emission_hooks = NULL;
  
#ifdef	G_ENABLE_DEBUG
  /* check current emissions */
  {
    Emission *emission;
    
    for (emission = g_emissions; emission; emission = emission->next)
      if (emission->ihint.signal_id == node.signal_id)
        g_critical (G_STRLOC ": signal \"%s\" being destroyed is currently in emission (instance '%p')",
                    node.name, emission->instance);
  }
#endif
  
  /* free contents that need to
   */
  SIGNAL_UNLOCK ();
  g_free (node.param_types);
  if (node.class_closure_bsa)
    {
      guint i;

      for (i = 0; i < node.class_closure_bsa->n_nodes; i++)
	{
	  ClassClosure *cc = g_bsearch_array_get_nth (node.class_closure_bsa, &g_class_closure_bconfig, i);

	  g_closure_unref (cc->closure);
	}
      g_bsearch_array_free (node.class_closure_bsa, &g_class_closure_bconfig);
    }
  g_free (node.accumulator);
  if (node.emission_hooks)
    {
      g_hook_list_clear (node.emission_hooks);
      g_free (node.emission_hooks);
    }
  SIGNAL_LOCK ();
}

/**
 * g_signal_override_class_closure:
 * @signal_id: the signal id
 * @instance_type: the instance type on which to override the class closure
 *  for the signal.
 * @class_closure: the closure.
 *
 * Overrides the class closure (i.e. the default handler) for the given signal
 * for emissions on instances of @instance_type. @instance_type must be derived
 * from the type to which the signal belongs.
 *
 * See g_signal_chain_from_overridden() and
 * g_signal_chain_from_overridden_handler() for how to chain up to the
 * parent class closure from inside the overridden one.
 */
void
g_signal_override_class_closure (guint     signal_id,
				 GType     instance_type,
				 GClosure *class_closure)
{
  SignalNode *node;
  
  g_return_if_fail (signal_id > 0);
  g_return_if_fail (class_closure != NULL);
  
  SIGNAL_LOCK ();
  node = LOOKUP_SIGNAL_NODE (signal_id);
  node_check_deprecated (node);
  if (!g_type_is_a (instance_type, node->itype))
    g_warning ("%s: type '%s' cannot be overridden for signal id '%u'", G_STRLOC, type_debug_name (instance_type), signal_id);
  else
    {
      ClassClosure *cc = signal_find_class_closure (node, instance_type);
      
      if (cc && cc->instance_type == instance_type)
	g_warning ("%s: type '%s' is already overridden for signal id '%u'", G_STRLOC, type_debug_name (instance_type), signal_id);
      else
	signal_add_class_closure (node, instance_type, class_closure);
    }
  SIGNAL_UNLOCK ();
}

/**
 * g_signal_override_class_handler:
 * @signal_name: the name for the signal
 * @instance_type: the instance type on which to override the class handler
 *  for the signal.
 * @class_handler: the handler.
 *
 * Overrides the class closure (i.e. the default handler) for the
 * given signal for emissions on instances of @instance_type with
 * callback @class_handler. @instance_type must be derived from the
 * type to which the signal belongs.
 *
 * See g_signal_chain_from_overridden() and
 * g_signal_chain_from_overridden_handler() for how to chain up to the
 * parent class closure from inside the overridden one.
 *
 * Since: 2.18
 */
void
g_signal_override_class_handler (const gchar *signal_name,
				 GType        instance_type,
				 GCallback    class_handler)
{
  guint signal_id;

  g_return_if_fail (signal_name != NULL);
  g_return_if_fail (instance_type != G_TYPE_NONE);
  g_return_if_fail (class_handler != NULL);

  signal_id = g_signal_lookup (signal_name, instance_type);

  if (signal_id)
    g_signal_override_class_closure (signal_id, instance_type,
                                     g_cclosure_new (class_handler, NULL, NULL));
  else
    g_warning ("%s: signal name '%s' is invalid for type id '%"G_GSIZE_FORMAT"'",
               G_STRLOC, signal_name, instance_type);

}

/**
 * g_signal_chain_from_overridden:
 * @instance_and_params: (array) the argument list of the signal emission.
 *  The first element in the array is a #GValue for the instance the signal
 *  is being emitted on. The rest are any arguments to be passed to the signal.
 * @return_value: Location for the return value.
 *
 * Calls the original class closure of a signal. This function should only
 * be called from an overridden class closure; see
 * g_signal_override_class_closure() and
 * g_signal_override_class_handler().
 */
void
g_signal_chain_from_overridden (const GValue *instance_and_params,
				GValue       *return_value)
{
  GType chain_type = 0, restore_type = 0;
  Emission *emission = NULL;
  GClosure *closure = NULL;
  guint n_params = 0;
  gpointer instance;
  
  g_return_if_fail (instance_and_params != NULL);
  instance = g_value_peek_pointer (instance_and_params);
  g_return_if_fail (G_TYPE_CHECK_INSTANCE (instance));
  
  SIGNAL_LOCK ();
  emission = emission_find_innermost (instance);
  if (emission)
    {
      SignalNode *node = LOOKUP_SIGNAL_NODE (emission->ihint.signal_id);
      
      g_assert (node != NULL);	/* paranoid */
      
      /* we should probably do the same parameter checks as g_signal_emit() here.
       */
      if (emission->chain_type != G_TYPE_NONE)
	{
	  ClassClosure *cc = signal_find_class_closure (node, emission->chain_type);
	  
	  g_assert (cc != NULL);	/* closure currently in call stack */

	  n_params = node->n_params;
	  restore_type = cc->instance_type;
	  cc = signal_find_class_closure (node, g_type_parent (cc->instance_type));
	  if (cc && cc->instance_type != restore_type)
	    {
	      closure = cc->closure;
	      chain_type = cc->instance_type;
	    }
	}
      else
	g_warning ("%s: signal id '%u' cannot be chained from current emission stage for instance '%p'", G_STRLOC, node->signal_id, instance);
    }
  else
    g_warning ("%s: no signal is currently being emitted for instance '%p'", G_STRLOC, instance);

  if (closure)
    {
      emission->chain_type = chain_type;
      SIGNAL_UNLOCK ();
      g_closure_invoke (closure,
			return_value,
			n_params + 1,
			instance_and_params,
			&emission->ihint);
      SIGNAL_LOCK ();
      emission->chain_type = restore_type;
    }
  SIGNAL_UNLOCK ();
}

/**
 * g_signal_chain_from_overridden_handler: (skip)
 * @instance: (type GObject.TypeInstance): the instance the signal is being
 *    emitted on.
 * @...: parameters to be passed to the parent class closure, followed by a
 *  location for the return value. If the return type of the signal
 *  is #G_TYPE_NONE, the return value location can be omitted.
 *
 * Calls the original class closure of a signal. This function should
 * only be called from an overridden class closure; see
 * g_signal_override_class_closure() and
 * g_signal_override_class_handler().
 *
 * Since: 2.18
 */
void
g_signal_chain_from_overridden_handler (gpointer instance,
                                        ...)
{
  GType chain_type = 0, restore_type = 0;
  Emission *emission = NULL;
  GClosure *closure = NULL;
  SignalNode *node;
  guint n_params = 0;

  g_return_if_fail (G_TYPE_CHECK_INSTANCE (instance));

  SIGNAL_LOCK ();
  emission = emission_find_innermost (instance);
  if (emission)
    {
      node = LOOKUP_SIGNAL_NODE (emission->ihint.signal_id);

      g_assert (node != NULL);	/* paranoid */

      /* we should probably do the same parameter checks as g_signal_emit() here.
       */
      if (emission->chain_type != G_TYPE_NONE)
	{
	  ClassClosure *cc = signal_find_class_closure (node, emission->chain_type);

	  g_assert (cc != NULL);	/* closure currently in call stack */

	  n_params = node->n_params;
	  restore_type = cc->instance_type;
	  cc = signal_find_class_closure (node, g_type_parent (cc->instance_type));
	  if (cc && cc->instance_type != restore_type)
	    {
	      closure = cc->closure;
	      chain_type = cc->instance_type;
	    }
	}
      else
	g_warning ("%s: signal id '%u' cannot be chained from current emission stage for instance '%p'", G_STRLOC, node->signal_id, instance);
    }
  else
    g_warning ("%s: no signal is currently being emitted for instance '%p'", G_STRLOC, instance);

  if (closure)
    {
      GValue *instance_and_params;
      GType signal_return_type;
      GValue *param_values;
      va_list var_args;
      guint i;

      va_start (var_args, instance);

      signal_return_type = node->return_type;
      instance_and_params = g_alloca (sizeof (GValue) * (n_params + 1));
      memset (instance_and_params, 0, sizeof (GValue) * (n_params + 1));
      param_values = instance_and_params + 1;

      for (i = 0; i < node->n_params; i++)
        {
          gchar *error;
          GType ptype = node->param_types[i] & ~G_SIGNAL_TYPE_STATIC_SCOPE;
          gboolean static_scope = node->param_types[i] & G_SIGNAL_TYPE_STATIC_SCOPE;

          SIGNAL_UNLOCK ();
          G_VALUE_COLLECT_INIT (param_values + i, ptype,
				var_args,
				static_scope ? G_VALUE_NOCOPY_CONTENTS : 0,
				&error);
          if (error)
            {
              g_warning ("%s: %s", G_STRLOC, error);
              g_free (error);

              /* we purposely leak the value here, it might not be
               * in a sane state if an error condition occoured
               */
              while (i--)
                g_value_unset (param_values + i);

              va_end (var_args);
              return;
            }
          SIGNAL_LOCK ();
        }

      SIGNAL_UNLOCK ();
      instance_and_params->g_type = 0;
      g_value_init_from_instance (instance_and_params, instance);
      SIGNAL_LOCK ();

      emission->chain_type = chain_type;
      SIGNAL_UNLOCK ();

      if (signal_return_type == G_TYPE_NONE)
        {
          g_closure_invoke (closure,
                            NULL,
                            n_params + 1,
                            instance_and_params,
                            &emission->ihint);
        }
      else
        {
          GValue return_value = G_VALUE_INIT;
          gchar *error = NULL;
          GType rtype = signal_return_type & ~G_SIGNAL_TYPE_STATIC_SCOPE;
          gboolean static_scope = signal_return_type & G_SIGNAL_TYPE_STATIC_SCOPE;

          g_value_init (&return_value, rtype);

          g_closure_invoke (closure,
                            &return_value,
                            n_params + 1,
                            instance_and_params,
                            &emission->ihint);

          G_VALUE_LCOPY (&return_value,
                         var_args,
                         static_scope ? G_VALUE_NOCOPY_CONTENTS : 0,
                         &error);
          if (!error)
            {
              g_value_unset (&return_value);
            }
          else
            {
              g_warning ("%s: %s", G_STRLOC, error);
              g_free (error);

              /* we purposely leak the value here, it might not be
               * in a sane state if an error condition occurred
               */
            }
        }

      for (i = 0; i < n_params; i++)
        g_value_unset (param_values + i);
      g_value_unset (instance_and_params);

      va_end (var_args);

      SIGNAL_LOCK ();
      emission->chain_type = restore_type;
    }
  SIGNAL_UNLOCK ();
}

/**
 * g_signal_get_invocation_hint:
 * @instance: (type GObject.Object): the instance to query
 *
 * Returns the invocation hint of the innermost signal emission of instance.
 *
 * Returns: (transfer none): the invocation hint of the innermost signal  emission.
 */
GSignalInvocationHint*
g_signal_get_invocation_hint (gpointer instance)
{
  Emission *emission = NULL;
  
  g_return_val_if_fail (G_TYPE_CHECK_INSTANCE (instance), NULL);

  SIGNAL_LOCK ();
  emission = emission_find_innermost (instance);
  SIGNAL_UNLOCK ();
  
  return emission ? &emission->ihint : NULL;
}

/**
 * g_signal_connect_closure_by_id:
 * @instance: (type GObject.Object): the instance to connect to.
 * @signal_id: the id of the signal.
 * @detail: the detail.
 * @closure: the closure to connect.
 * @after: whether the handler should be called before or after the
 *  default handler of the signal.
 *
 * Connects a closure to a signal for a particular object.
 *
 * Returns: the handler ID (always greater than 0 for successful connections)
 */
gulong
g_signal_connect_closure_by_id (gpointer  instance,
				guint     signal_id,
				GQuark    detail,
				GClosure *closure,
				gboolean  after)
{
  SignalNode *node;
  gulong handler_seq_no = 0;
  
  g_return_val_if_fail (G_TYPE_CHECK_INSTANCE (instance), 0);
  g_return_val_if_fail (signal_id > 0, 0);
  g_return_val_if_fail (closure != NULL, 0);
  
  SIGNAL_LOCK ();
  node = LOOKUP_SIGNAL_NODE (signal_id);
  if (node)
    {
      if (detail && !(node->flags & G_SIGNAL_DETAILED))
	g_warning ("%s: signal id '%u' does not support detail (%u)", G_STRLOC, signal_id, detail);
      else if (!g_type_is_a (G_TYPE_FROM_INSTANCE (instance), node->itype))
	g_warning ("%s: signal id '%u' is invalid for instance '%p'", G_STRLOC, signal_id, instance);
      else
	{
	  Handler *handler = handler_new (signal_id, instance, after);
	  
	  handler_seq_no = handler->sequential_number;
	  handler->detail = detail;
	  handler->closure = g_closure_ref (closure);
	  g_closure_sink (closure);
	  add_invalid_closure_notify (handler, instance);
	  handler_insert (signal_id, instance, handler);
	  if (node->c_marshaller && G_CLOSURE_NEEDS_MARSHAL (closure))
	    {
	      g_closure_set_marshal (closure, node->c_marshaller);
	      if (node->va_marshaller)
		_g_closure_set_va_marshal (closure, node->va_marshaller);
	    }
	}
    }
  else
    g_warning ("%s: signal id '%u' is invalid for instance '%p'", G_STRLOC, signal_id, instance);
  SIGNAL_UNLOCK ();
  
  return handler_seq_no;
}

/**
 * g_signal_connect_closure:
 * @instance: (type GObject.Object): the instance to connect to.
 * @detailed_signal: a string of the form "signal-name::detail".
 * @closure: the closure to connect.
 * @after: whether the handler should be called before or after the
 *  default handler of the signal.
 *
 * Connects a closure to a signal for a particular object.
 *
 * Returns: the handler ID (always greater than 0 for successful connections)
 */
gulong
g_signal_connect_closure (gpointer     instance,
			  const gchar *detailed_signal,
			  GClosure    *closure,
			  gboolean     after)
{
  guint signal_id;
  gulong handler_seq_no = 0;
  GQuark detail = 0;
  GType itype;

  g_return_val_if_fail (G_TYPE_CHECK_INSTANCE (instance), 0);
  g_return_val_if_fail (detailed_signal != NULL, 0);
  g_return_val_if_fail (closure != NULL, 0);

  SIGNAL_LOCK ();
  itype = G_TYPE_FROM_INSTANCE (instance);
  signal_id = signal_parse_name (detailed_signal, itype, &detail, TRUE);
  if (signal_id)
    {
      SignalNode *node = LOOKUP_SIGNAL_NODE (signal_id);

      if (detail && !(node->flags & G_SIGNAL_DETAILED))
	g_warning ("%s: signal '%s' does not support details", G_STRLOC, detailed_signal);
      else if (!g_type_is_a (itype, node->itype))
        g_warning ("%s: signal '%s' is invalid for instance '%p' of type '%s'",
                   G_STRLOC, detailed_signal, instance, g_type_name (itype));
      else
	{
	  Handler *handler = handler_new (signal_id, instance, after);

	  handler_seq_no = handler->sequential_number;
	  handler->detail = detail;
	  handler->closure = g_closure_ref (closure);
	  g_closure_sink (closure);
	  add_invalid_closure_notify (handler, instance);
	  handler_insert (signal_id, instance, handler);
	  if (node->c_marshaller && G_CLOSURE_NEEDS_MARSHAL (handler->closure))
	    {
	      g_closure_set_marshal (handler->closure, node->c_marshaller);
	      if (node->va_marshaller)
		_g_closure_set_va_marshal (handler->closure, node->va_marshaller);
	    }
	}
    }
  else
    g_warning ("%s: signal '%s' is invalid for instance '%p' of type '%s'",
               G_STRLOC, detailed_signal, instance, g_type_name (itype));
  SIGNAL_UNLOCK ();

  return handler_seq_no;
}

static void
node_check_deprecated (const SignalNode *node)
{
  static const gchar * g_enable_diagnostic = NULL;

  if (G_UNLIKELY (!g_enable_diagnostic))
    {
      g_enable_diagnostic = g_getenv ("G_ENABLE_DIAGNOSTIC");
      if (!g_enable_diagnostic)
        g_enable_diagnostic = "0";
    }

  if (g_enable_diagnostic[0] == '1')
    {
      if (node->flags & G_SIGNAL_DEPRECATED)
        {
          g_warning ("The signal %s::%s is deprecated and shouldn't be used "
                     "anymore. It will be removed in a future version.",
                     type_debug_name (node->itype), node->name);
        }
    }
}

/**
 * g_signal_connect_data:
 * @instance: (type GObject.Object): the instance to connect to.
 * @detailed_signal: a string of the form "signal-name::detail".
 * @c_handler: the #GCallback to connect.
 * @data: data to pass to @c_handler calls.
 * @destroy_data: a #GClosureNotify for @data.
 * @connect_flags: a combination of #GConnectFlags.
 *
 * Connects a #GCallback function to a signal for a particular object. Similar
 * to g_signal_connect(), but allows to provide a #GClosureNotify for the data
 * which will be called when the signal handler is disconnected and no longer
 * used. Specify @connect_flags if you need `..._after()` or
 * `..._swapped()` variants of this function.
 *
 * Returns: the handler ID (always greater than 0 for successful connections)
 */
gulong
g_signal_connect_data (gpointer       instance,
		       const gchar   *detailed_signal,
		       GCallback      c_handler,
		       gpointer       data,
		       GClosureNotify destroy_data,
		       GConnectFlags  connect_flags)
{
  guint signal_id;
  gulong handler_seq_no = 0;
  GQuark detail = 0;
  GType itype;
  gboolean swapped, after;
  
  g_return_val_if_fail (G_TYPE_CHECK_INSTANCE (instance), 0);
  g_return_val_if_fail (detailed_signal != NULL, 0);
  g_return_val_if_fail (c_handler != NULL, 0);

  swapped = (connect_flags & G_CONNECT_SWAPPED) != FALSE;
  after = (connect_flags & G_CONNECT_AFTER) != FALSE;

  SIGNAL_LOCK ();
  itype = G_TYPE_FROM_INSTANCE (instance);
  signal_id = signal_parse_name (detailed_signal, itype, &detail, TRUE);
  if (signal_id)
    {
      SignalNode *node = LOOKUP_SIGNAL_NODE (signal_id);

      node_check_deprecated (node);

      if (detail && !(node->flags & G_SIGNAL_DETAILED))
	g_warning ("%s: signal '%s' does not support details", G_STRLOC, detailed_signal);
      else if (!g_type_is_a (itype, node->itype))
        g_warning ("%s: signal '%s' is invalid for instance '%p' of type '%s'",
                   G_STRLOC, detailed_signal, instance, g_type_name (itype));
      else
	{
	  Handler *handler = handler_new (signal_id, instance, after);

	  handler_seq_no = handler->sequential_number;
	  handler->detail = detail;
	  handler->closure = g_closure_ref ((swapped ? g_cclosure_new_swap : g_cclosure_new) (c_handler, data, destroy_data));
	  g_closure_sink (handler->closure);
	  handler_insert (signal_id, instance, handler);
	  if (node->c_marshaller && G_CLOSURE_NEEDS_MARSHAL (handler->closure))
	    {
	      g_closure_set_marshal (handler->closure, node->c_marshaller);
	      if (node->va_marshaller)
		_g_closure_set_va_marshal (handler->closure, node->va_marshaller);
	    }
        }
    }
  else
    g_warning ("%s: signal '%s' is invalid for instance '%p' of type '%s'",
               G_STRLOC, detailed_signal, instance, g_type_name (itype));
  SIGNAL_UNLOCK ();

  return handler_seq_no;
}

/**
 * g_signal_handler_block:
 * @instance: (type GObject.Object): The instance to block the signal handler of.
 * @handler_id: Handler id of the handler to be blocked.
 *
 * Blocks a handler of an instance so it will not be called during any
 * signal emissions unless it is unblocked again. Thus "blocking" a
 * signal handler means to temporarily deactive it, a signal handler
 * has to be unblocked exactly the same amount of times it has been
 * blocked before to become active again.
 *
 * The @handler_id has to be a valid signal handler id, connected to a
 * signal of @instance.
 */
void
g_signal_handler_block (gpointer instance,
                        gulong   handler_id)
{
  Handler *handler;
  
  g_return_if_fail (G_TYPE_CHECK_INSTANCE (instance));
  g_return_if_fail (handler_id > 0);
  
  SIGNAL_LOCK ();
  handler = handler_lookup (instance, handler_id, NULL, NULL);
  if (handler)
    {
#ifndef G_DISABLE_CHECKS
      if (handler->block_count >= HANDLER_MAX_BLOCK_COUNT - 1)
        g_error (G_STRLOC ": handler block_count overflow, %s", REPORT_BUG);
#endif
      handler->block_count += 1;
    }
  else
    g_warning ("%s: instance '%p' has no handler with id '%lu'", G_STRLOC, instance, handler_id);
  SIGNAL_UNLOCK ();
}

/**
 * g_signal_handler_unblock:
 * @instance: (type GObject.Object): The instance to unblock the signal handler of.
 * @handler_id: Handler id of the handler to be unblocked.
 *
 * Undoes the effect of a previous g_signal_handler_block() call.  A
 * blocked handler is skipped during signal emissions and will not be
 * invoked, unblocking it (for exactly the amount of times it has been
 * blocked before) reverts its "blocked" state, so the handler will be
 * recognized by the signal system and is called upon future or
 * currently ongoing signal emissions (since the order in which
 * handlers are called during signal emissions is deterministic,
 * whether the unblocked handler in question is called as part of a
 * currently ongoing emission depends on how far that emission has
 * proceeded yet).
 *
 * The @handler_id has to be a valid id of a signal handler that is
 * connected to a signal of @instance and is currently blocked.
 */
void
g_signal_handler_unblock (gpointer instance,
                          gulong   handler_id)
{
  Handler *handler;
  
  g_return_if_fail (G_TYPE_CHECK_INSTANCE (instance));
  g_return_if_fail (handler_id > 0);
  
  SIGNAL_LOCK ();
  handler = handler_lookup (instance, handler_id, NULL, NULL);
  if (handler)
    {
      if (handler->block_count)
        handler->block_count -= 1;
      else
        g_warning (G_STRLOC ": handler '%lu' of instance '%p' is not blocked", handler_id, instance);
    }
  else
    g_warning ("%s: instance '%p' has no handler with id '%lu'", G_STRLOC, instance, handler_id);
  SIGNAL_UNLOCK ();
}

/**
 * g_signal_handler_disconnect:
 * @instance: (type GObject.Object): The instance to remove the signal handler from.
 * @handler_id: Handler id of the handler to be disconnected.
 *
 * Disconnects a handler from an instance so it will not be called during
 * any future or currently ongoing emissions of the signal it has been
 * connected to. The @handler_id becomes invalid and may be reused.
 *
 * The @handler_id has to be a valid signal handler id, connected to a
 * signal of @instance.
 */
void
g_signal_handler_disconnect (gpointer instance,
                             gulong   handler_id)
{
  Handler *handler;
  
  g_return_if_fail (G_TYPE_CHECK_INSTANCE (instance));
  g_return_if_fail (handler_id > 0);
  
  SIGNAL_LOCK ();
  handler = handler_lookup (instance, handler_id, 0, 0);
  if (handler)
    {
      g_hash_table_remove (g_handlers, handler);
      handler->sequential_number = 0;
      handler->block_count = 1;
      remove_invalid_closure_notify (handler, instance);
      handler_unref_R (handler->signal_id, instance, handler);
    }
  else
    g_warning ("%s: instance '%p' has no handler with id '%lu'", G_STRLOC, instance, handler_id);
  SIGNAL_UNLOCK ();
}

/**
 * g_signal_handler_is_connected:
 * @instance: (type GObject.Object): The instance where a signal handler is sought.
 * @handler_id: the handler ID.
 *
 * Returns whether @handler_id is the ID of a handler connected to @instance.
 *
 * Returns: whether @handler_id identifies a handler connected to @instance.
 */
gboolean
g_signal_handler_is_connected (gpointer instance,
			       gulong   handler_id)
{
  Handler *handler;
  gboolean connected;

  g_return_val_if_fail (G_TYPE_CHECK_INSTANCE (instance), FALSE);

  SIGNAL_LOCK ();
  handler = handler_lookup (instance, handler_id, NULL, NULL);
  connected = handler != NULL;
  SIGNAL_UNLOCK ();

  return connected;
}

/**
 * g_signal_handlers_destroy:
 * @instance: (type GObject.Object): The instance whose signal handlers are destroyed
 *
 * Destroy all signal handlers of a type instance. This function is
 * an implementation detail of the #GObject dispose implementation,
 * and should not be used outside of the type system.
 */
void
g_signal_handlers_destroy (gpointer instance)
{
  GBSearchArray *hlbsa;
  
  g_return_if_fail (G_TYPE_CHECK_INSTANCE (instance));
  
  SIGNAL_LOCK ();
  hlbsa = g_hash_table_lookup (g_handler_list_bsa_ht, instance);
  if (hlbsa)
    {
      guint i;
      
      /* reentrancy caution, delete instance trace first */
      g_hash_table_remove (g_handler_list_bsa_ht, instance);
      
      for (i = 0; i < hlbsa->n_nodes; i++)
        {
          HandlerList *hlist = g_bsearch_array_get_nth (hlbsa, &g_signal_hlbsa_bconfig, i);
          Handler *handler = hlist->handlers;
	  
          while (handler)
            {
              Handler *tmp = handler;
	      
              handler = tmp->next;
              tmp->block_count = 1;
              /* cruel unlink, this works because _all_ handlers vanish */
              tmp->next = NULL;
              tmp->prev = tmp;
              if (tmp->sequential_number)
		{
                  g_hash_table_remove (g_handlers, tmp);
		  remove_invalid_closure_notify (tmp, instance);
		  tmp->sequential_number = 0;
		  handler_unref_R (0, NULL, tmp);
		}
            }
        }
      g_bsearch_array_free (hlbsa, &g_signal_hlbsa_bconfig);
    }
  SIGNAL_UNLOCK ();
}

/**
 * g_signal_handler_find:
 * @instance: (type GObject.Object): The instance owning the signal handler to be found.
 * @mask: Mask indicating which of @signal_id, @detail, @closure, @func
 *  and/or @data the handler has to match.
 * @signal_id: Signal the handler has to be connected to.
 * @detail: Signal detail the handler has to be connected to.
 * @closure: (nullable): The closure the handler will invoke.
 * @func: The C closure callback of the handler (useless for non-C closures).
 * @data: The closure data of the handler's closure.
 *
 * Finds the first signal handler that matches certain selection criteria.
 * The criteria mask is passed as an OR-ed combination of #GSignalMatchType
 * flags, and the criteria values are passed as arguments.
 * The match @mask has to be non-0 for successful matches.
 * If no handler was found, 0 is returned.
 *
 * Returns: A valid non-0 signal handler id for a successful match.
 */
gulong
g_signal_handler_find (gpointer         instance,
                       GSignalMatchType mask,
                       guint            signal_id,
		       GQuark		detail,
                       GClosure        *closure,
                       gpointer         func,
                       gpointer         data)
{
  gulong handler_seq_no = 0;
  
  g_return_val_if_fail (G_TYPE_CHECK_INSTANCE (instance), 0);
  g_return_val_if_fail ((mask & ~G_SIGNAL_MATCH_MASK) == 0, 0);
  
  if (mask & G_SIGNAL_MATCH_MASK)
    {
      HandlerMatch *mlist;
      
      SIGNAL_LOCK ();
      mlist = handlers_find (instance, mask, signal_id, detail, closure, func, data, TRUE);
      if (mlist)
	{
	  handler_seq_no = mlist->handler->sequential_number;
	  handler_match_free1_R (mlist, instance);
	}
      SIGNAL_UNLOCK ();
    }
  
  return handler_seq_no;
}

static guint
signal_handlers_foreach_matched_R (gpointer         instance,
				   GSignalMatchType mask,
				   guint            signal_id,
				   GQuark           detail,
				   GClosure        *closure,
				   gpointer         func,
				   gpointer         data,
				   void		  (*callback) (gpointer instance,
							       gulong   handler_seq_no))
{
  HandlerMatch *mlist;
  guint n_handlers = 0;
  
  mlist = handlers_find (instance, mask, signal_id, detail, closure, func, data, FALSE);
  while (mlist)
    {
      n_handlers++;
      if (mlist->handler->sequential_number)
	{
	  SIGNAL_UNLOCK ();
	  callback (instance, mlist->handler->sequential_number);
	  SIGNAL_LOCK ();
	}
      mlist = handler_match_free1_R (mlist, instance);
    }
  
  return n_handlers;
}

/**
 * g_signal_handlers_block_matched:
 * @instance: (type GObject.Object): The instance to block handlers from.
 * @mask: Mask indicating which of @signal_id, @detail, @closure, @func
 *  and/or @data the handlers have to match.
 * @signal_id: Signal the handlers have to be connected to.
 * @detail: Signal detail the handlers have to be connected to.
 * @closure: (nullable): The closure the handlers will invoke.
 * @func: The C closure callback of the handlers (useless for non-C closures).
 * @data: The closure data of the handlers' closures.
 *
 * Blocks all handlers on an instance that match a certain selection criteria.
 * The criteria mask is passed as an OR-ed combination of #GSignalMatchType
 * flags, and the criteria values are passed as arguments.
 * Passing at least one of the %G_SIGNAL_MATCH_CLOSURE, %G_SIGNAL_MATCH_FUNC
 * or %G_SIGNAL_MATCH_DATA match flags is required for successful matches.
 * If no handlers were found, 0 is returned, the number of blocked handlers
 * otherwise.
 *
 * Returns: The number of handlers that matched.
 */
guint
g_signal_handlers_block_matched (gpointer         instance,
				 GSignalMatchType mask,
				 guint            signal_id,
				 GQuark           detail,
				 GClosure        *closure,
				 gpointer         func,
				 gpointer         data)
{
  guint n_handlers = 0;
  
  g_return_val_if_fail (G_TYPE_CHECK_INSTANCE (instance), 0);
  g_return_val_if_fail ((mask & ~G_SIGNAL_MATCH_MASK) == 0, 0);
  
  if (mask & (G_SIGNAL_MATCH_CLOSURE | G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA))
    {
      SIGNAL_LOCK ();
      n_handlers = signal_handlers_foreach_matched_R (instance, mask, signal_id, detail,
						      closure, func, data,
						      g_signal_handler_block);
      SIGNAL_UNLOCK ();
    }
  
  return n_handlers;
}

/**
 * g_signal_handlers_unblock_matched:
 * @instance: (type GObject.Object): The instance to unblock handlers from.
 * @mask: Mask indicating which of @signal_id, @detail, @closure, @func
 *  and/or @data the handlers have to match.
 * @signal_id: Signal the handlers have to be connected to.
 * @detail: Signal detail the handlers have to be connected to.
 * @closure: (nullable): The closure the handlers will invoke.
 * @func: The C closure callback of the handlers (useless for non-C closures).
 * @data: The closure data of the handlers' closures.
 *
 * Unblocks all handlers on an instance that match a certain selection
 * criteria. The criteria mask is passed as an OR-ed combination of
 * #GSignalMatchType flags, and the criteria values are passed as arguments.
 * Passing at least one of the %G_SIGNAL_MATCH_CLOSURE, %G_SIGNAL_MATCH_FUNC
 * or %G_SIGNAL_MATCH_DATA match flags is required for successful matches.
 * If no handlers were found, 0 is returned, the number of unblocked handlers
 * otherwise. The match criteria should not apply to any handlers that are
 * not currently blocked.
 *
 * Returns: The number of handlers that matched.
 */
guint
g_signal_handlers_unblock_matched (gpointer         instance,
				   GSignalMatchType mask,
				   guint            signal_id,
				   GQuark           detail,
				   GClosure        *closure,
				   gpointer         func,
				   gpointer         data)
{
  guint n_handlers = 0;
  
  g_return_val_if_fail (G_TYPE_CHECK_INSTANCE (instance), 0);
  g_return_val_if_fail ((mask & ~G_SIGNAL_MATCH_MASK) == 0, 0);
  
  if (mask & (G_SIGNAL_MATCH_CLOSURE | G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA))
    {
      SIGNAL_LOCK ();
      n_handlers = signal_handlers_foreach_matched_R (instance, mask, signal_id, detail,
						      closure, func, data,
						      g_signal_handler_unblock);
      SIGNAL_UNLOCK ();
    }
  
  return n_handlers;
}

/**
 * g_signal_handlers_disconnect_matched:
 * @instance: (type GObject.Object): The instance to remove handlers from.
 * @mask: Mask indicating which of @signal_id, @detail, @closure, @func
 *  and/or @data the handlers have to match.
 * @signal_id: Signal the handlers have to be connected to.
 * @detail: Signal detail the handlers have to be connected to.
 * @closure: (nullable): The closure the handlers will invoke.
 * @func: The C closure callback of the handlers (useless for non-C closures).
 * @data: The closure data of the handlers' closures.
 *
 * Disconnects all handlers on an instance that match a certain
 * selection criteria. The criteria mask is passed as an OR-ed
 * combination of #GSignalMatchType flags, and the criteria values are
 * passed as arguments.  Passing at least one of the
 * %G_SIGNAL_MATCH_CLOSURE, %G_SIGNAL_MATCH_FUNC or
 * %G_SIGNAL_MATCH_DATA match flags is required for successful
 * matches.  If no handlers were found, 0 is returned, the number of
 * disconnected handlers otherwise.
 *
 * Returns: The number of handlers that matched.
 */
guint
g_signal_handlers_disconnect_matched (gpointer         instance,
				      GSignalMatchType mask,
				      guint            signal_id,
				      GQuark           detail,
				      GClosure        *closure,
				      gpointer         func,
				      gpointer         data)
{
  guint n_handlers = 0;
  
  g_return_val_if_fail (G_TYPE_CHECK_INSTANCE (instance), 0);
  g_return_val_if_fail ((mask & ~G_SIGNAL_MATCH_MASK) == 0, 0);
  
  if (mask & (G_SIGNAL_MATCH_CLOSURE | G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA))
    {
      SIGNAL_LOCK ();
      n_handlers = signal_handlers_foreach_matched_R (instance, mask, signal_id, detail,
						      closure, func, data,
						      g_signal_handler_disconnect);
      SIGNAL_UNLOCK ();
    }
  
  return n_handlers;
}

/**
 * g_signal_has_handler_pending:
 * @instance: (type GObject.Object): the object whose signal handlers are sought.
 * @signal_id: the signal id.
 * @detail: the detail.
 * @may_be_blocked: whether blocked handlers should count as match.
 *
 * Returns whether there are any handlers connected to @instance for the
 * given signal id and detail.
 *
 * If @detail is 0 then it will only match handlers that were connected
 * without detail.  If @detail is non-zero then it will match handlers
 * connected both without detail and with the given detail.  This is
 * consistent with how a signal emitted with @detail would be delivered
 * to those handlers.
 *
 * Since 2.46 this also checks for a non-default class closure being
 * installed, as this is basically always what you want.
 *
 * One example of when you might use this is when the arguments to the
 * signal are difficult to compute. A class implementor may opt to not
 * emit the signal if no one is attached anyway, thus saving the cost
 * of building the arguments.
 *
 * Returns: %TRUE if a handler is connected to the signal, %FALSE
 *          otherwise.
 */
gboolean
g_signal_has_handler_pending (gpointer instance,
			      guint    signal_id,
			      GQuark   detail,
			      gboolean may_be_blocked)
{
  HandlerMatch *mlist;
  gboolean has_pending;
  SignalNode *node;
  
  g_return_val_if_fail (G_TYPE_CHECK_INSTANCE (instance), FALSE);
  g_return_val_if_fail (signal_id > 0, FALSE);
  
  SIGNAL_LOCK ();

  node = LOOKUP_SIGNAL_NODE (signal_id);
  if (detail)
    {
      if (!(node->flags & G_SIGNAL_DETAILED))
	{
	  g_warning ("%s: signal id '%u' does not support detail (%u)", G_STRLOC, signal_id, detail);
	  SIGNAL_UNLOCK ();
	  return FALSE;
	}
    }
  mlist = handlers_find (instance,
			 (G_SIGNAL_MATCH_ID | G_SIGNAL_MATCH_DETAIL | (may_be_blocked ? 0 : G_SIGNAL_MATCH_UNBLOCKED)),
			 signal_id, detail, NULL, NULL, NULL, TRUE);
  if (mlist)
    {
      has_pending = TRUE;
      handler_match_free1_R (mlist, instance);
    }
  else
    {
      ClassClosure *class_closure = signal_find_class_closure (node, G_TYPE_FROM_INSTANCE (instance));
      if (class_closure != NULL && class_closure->instance_type != 0)
        has_pending = TRUE;
      else
        has_pending = FALSE;
    }
  SIGNAL_UNLOCK ();

  return has_pending;
}

/**
 * g_signal_emitv:
 * @instance_and_params: (array): argument list for the signal emission.
 *  The first element in the array is a #GValue for the instance the signal
 *  is being emitted on. The rest are any arguments to be passed to the signal.
 * @signal_id: the signal id
 * @detail: the detail
 * @return_value: (inout) (optional): Location to
 * store the return value of the signal emission. This must be provided if the
 * specified signal returns a value, but may be ignored otherwise.
 *
 * Emits a signal.
 *
 * Note that g_signal_emitv() doesn't change @return_value if no handlers are
 * connected, in contrast to g_signal_emit() and g_signal_emit_valist().
 */
void
g_signal_emitv (const GValue *instance_and_params,
		guint         signal_id,
		GQuark	      detail,
		GValue       *return_value)
{
  gpointer instance;
  SignalNode *node;
#ifdef G_ENABLE_DEBUG
  const GValue *param_values;
  guint i;
#endif
  
  g_return_if_fail (instance_and_params != NULL);
  instance = g_value_peek_pointer (instance_and_params);
  g_return_if_fail (G_TYPE_CHECK_INSTANCE (instance));
  g_return_if_fail (signal_id > 0);

#ifdef G_ENABLE_DEBUG
  param_values = instance_and_params + 1;
#endif

  SIGNAL_LOCK ();
  node = LOOKUP_SIGNAL_NODE (signal_id);
  if (!node || !g_type_is_a (G_TYPE_FROM_INSTANCE (instance), node->itype))
    {
      g_warning ("%s: signal id '%u' is invalid for instance '%p'", G_STRLOC, signal_id, instance);
      SIGNAL_UNLOCK ();
      return;
    }
#ifdef G_ENABLE_DEBUG
  if (detail && !(node->flags & G_SIGNAL_DETAILED))
    {
      g_warning ("%s: signal id '%u' does not support detail (%u)", G_STRLOC, signal_id, detail);
      SIGNAL_UNLOCK ();
      return;
    }
  for (i = 0; i < node->n_params; i++)
    if (!G_TYPE_CHECK_VALUE_TYPE (param_values + i, node->param_types[i] & ~G_SIGNAL_TYPE_STATIC_SCOPE))
      {
	g_critical ("%s: value for '%s' parameter %u for signal \"%s\" is of type '%s'",
		    G_STRLOC,
		    type_debug_name (node->param_types[i]),
		    i,
		    node->name,
		    G_VALUE_TYPE_NAME (param_values + i));
	SIGNAL_UNLOCK ();
	return;
      }
  if (node->return_type != G_TYPE_NONE)
    {
      if (!return_value)
	{
	  g_critical ("%s: return value '%s' for signal \"%s\" is (NULL)",
		      G_STRLOC,
		      type_debug_name (node->return_type),
		      node->name);
	  SIGNAL_UNLOCK ();
	  return;
	}
      else if (!node->accumulator && !G_TYPE_CHECK_VALUE_TYPE (return_value, node->return_type & ~G_SIGNAL_TYPE_STATIC_SCOPE))
	{
	  g_critical ("%s: return value '%s' for signal \"%s\" is of type '%s'",
		      G_STRLOC,
		      type_debug_name (node->return_type),
		      node->name,
		      G_VALUE_TYPE_NAME (return_value));
	  SIGNAL_UNLOCK ();
	  return;
	}
    }
  else
    return_value = NULL;
#endif	/* G_ENABLE_DEBUG */

  /* optimize NOP emissions */
  if (!node->single_va_closure_is_valid)
    node_update_single_va_closure (node);

  if (node->single_va_closure != NULL &&
      (node->single_va_closure == SINGLE_VA_CLOSURE_EMPTY_MAGIC ||
       _g_closure_is_void (node->single_va_closure, instance)))
    {
      HandlerList* hlist = handler_list_lookup (node->signal_id, instance);
      if (hlist == NULL || hlist->handlers == NULL)
	{
	  /* nothing to do to emit this signal */
	  SIGNAL_UNLOCK ();
	  /* g_printerr ("omitting emission of \"%s\"\n", node->name); */
	  return;
	}
    }

  SIGNAL_UNLOCK ();
  signal_emit_unlocked_R (node, detail, instance, return_value, instance_and_params);
}

static inline gboolean
accumulate (GSignalInvocationHint *ihint,
	    GValue                *return_accu,
	    GValue	          *handler_return,
	    SignalAccumulator     *accumulator)
{
  gboolean continue_emission;

  if (!accumulator)
    return TRUE;

  continue_emission = accumulator->func (ihint, return_accu, handler_return, accumulator->data);
  g_value_reset (handler_return);

  return continue_emission;
}

/**
 * g_signal_emit_valist: (skip)
 * @instance: (type GObject.TypeInstance): the instance the signal is being
 *    emitted on.
 * @signal_id: the signal id
 * @detail: the detail
 * @var_args: a list of parameters to be passed to the signal, followed by a
 *  location for the return value. If the return type of the signal
 *  is #G_TYPE_NONE, the return value location can be omitted.
 *
 * Emits a signal.
 *
 * Note that g_signal_emit_valist() resets the return value to the default
 * if no handlers are connected, in contrast to g_signal_emitv().
 */
void
g_signal_emit_valist (gpointer instance,
		      guint    signal_id,
		      GQuark   detail,
		      va_list  var_args)
{
  GValue *instance_and_params;
  GType signal_return_type;
  GValue *param_values;
  SignalNode *node;
  guint i, n_params;

  g_return_if_fail (G_TYPE_CHECK_INSTANCE (instance));
  g_return_if_fail (signal_id > 0);

  SIGNAL_LOCK ();
  node = LOOKUP_SIGNAL_NODE (signal_id);
  if (!node || !g_type_is_a (G_TYPE_FROM_INSTANCE (instance), node->itype))
    {
      g_warning ("%s: signal id '%u' is invalid for instance '%p'", G_STRLOC, signal_id, instance);
      SIGNAL_UNLOCK ();
      return;
    }
#ifndef G_DISABLE_CHECKS
  if (detail && !(node->flags & G_SIGNAL_DETAILED))
    {
      g_warning ("%s: signal id '%u' does not support detail (%u)", G_STRLOC, signal_id, detail);
      SIGNAL_UNLOCK ();
      return;
    }
#endif  /* !G_DISABLE_CHECKS */

  if (!node->single_va_closure_is_valid)
    node_update_single_va_closure (node);

  if (node->single_va_closure != NULL)
    {
      HandlerList* hlist = handler_list_lookup (node->signal_id, instance);
      Handler *fastpath_handler = NULL;
      Handler *l;
      GClosure *closure = NULL;
      gboolean fastpath = TRUE;
      GSignalFlags run_type = G_SIGNAL_RUN_FIRST;

      if (node->single_va_closure != SINGLE_VA_CLOSURE_EMPTY_MAGIC &&
	  !_g_closure_is_void (node->single_va_closure, instance))
	{
	  if (_g_closure_supports_invoke_va (node->single_va_closure))
	    {
	      closure = node->single_va_closure;
	      if (node->single_va_closure_is_after)
		run_type = G_SIGNAL_RUN_LAST;
	      else
		run_type = G_SIGNAL_RUN_FIRST;
	    }
	  else
	    fastpath = FALSE;
	}

      for (l = hlist ? hlist->handlers : NULL; fastpath && l != NULL; l = l->next)
	{
	  if (!l->block_count &&
	      (!l->detail || l->detail == detail))
	    {
	      if (closure != NULL || !_g_closure_supports_invoke_va (l->closure))
		{
		  fastpath = FALSE;
		  break;
		}
	      else
		{
                  fastpath_handler = l;
		  closure = l->closure;
		  if (l->after)
		    run_type = G_SIGNAL_RUN_LAST;
		  else
		    run_type = G_SIGNAL_RUN_FIRST;
		}
	    }
	}

      if (fastpath && closure == NULL && node->return_type == G_TYPE_NONE)
	{
	  SIGNAL_UNLOCK ();
	  return;
	}

      /* Don't allow no-recurse emission as we might have to restart, which means
	 we will run multiple handlers and thus must ref all arguments */
      if (closure != NULL && (node->flags & (G_SIGNAL_NO_RECURSE)) != 0)
	fastpath = FALSE;
      
      if (fastpath)
	{
	  SignalAccumulator *accumulator;
	  Emission emission;
	  GValue *return_accu, accu = G_VALUE_INIT;
	  guint signal_id;
	  GType instance_type = G_TYPE_FROM_INSTANCE (instance);
	  GValue emission_return = G_VALUE_INIT;
          GType rtype = node->return_type & ~G_SIGNAL_TYPE_STATIC_SCOPE;
	  gboolean static_scope = node->return_type & G_SIGNAL_TYPE_STATIC_SCOPE;

	  signal_id = node->signal_id;
	  accumulator = node->accumulator;
	  if (rtype == G_TYPE_NONE)
	    return_accu = NULL;
	  else if (accumulator)
	    return_accu = &accu;
	  else
	    return_accu = &emission_return;

	  emission.instance = instance;
	  emission.ihint.signal_id = signal_id;
	  emission.ihint.detail = detail;
	  emission.ihint.run_type = run_type;
	  emission.state = EMISSION_RUN;
	  emission.chain_type = instance_type;
	  emission_push (&emission);

          if (fastpath_handler)
            handler_ref (fastpath_handler);

	  SIGNAL_UNLOCK ();

	  TRACE(GOBJECT_SIGNAL_EMIT(signal_id, detail, instance, instance_type));

	  if (rtype != G_TYPE_NONE)
	    g_value_init (&emission_return, rtype);

	  if (accumulator)
	    g_value_init (&accu, rtype);

	  if (closure != NULL)
	    {
	      g_object_ref (instance);
	      _g_closure_invoke_va (closure,
				    return_accu,
				    instance,
				    var_args,
				    node->n_params,
				    node->param_types);
	      accumulate (&emission.ihint, &emission_return, &accu, accumulator);
	    }

	  SIGNAL_LOCK ();

	  emission.chain_type = G_TYPE_NONE;
	  emission_pop (&emission);

          if (fastpath_handler)
            handler_unref_R (signal_id, instance, fastpath_handler);

	  SIGNAL_UNLOCK ();

	  if (accumulator)
	    g_value_unset (&accu);

	  if (rtype != G_TYPE_NONE)
	    {
	      gchar *error = NULL;
	      for (i = 0; i < node->n_params; i++)
		{
		  GType ptype = node->param_types[i] & ~G_SIGNAL_TYPE_STATIC_SCOPE;
		  G_VALUE_COLLECT_SKIP (ptype, var_args);
		}

	      G_VALUE_LCOPY (&emission_return,
			     var_args,
			     static_scope ? G_VALUE_NOCOPY_CONTENTS : 0,
			     &error);
	      if (!error)
		g_value_unset (&emission_return);
	      else
		{
		  g_warning ("%s: %s", G_STRLOC, error);
		  g_free (error);
		  /* we purposely leak the value here, it might not be
		   * in a sane state if an error condition occurred
		   */
		}
	    }
	  
	  TRACE(GOBJECT_SIGNAL_EMIT_END(signal_id, detail, instance, instance_type));

          if (closure != NULL)
            g_object_unref (instance);

	  return;
	}
    }
  SIGNAL_UNLOCK ();

  n_params = node->n_params;
  signal_return_type = node->return_type;
  instance_and_params = g_alloca (sizeof (GValue) * (n_params + 1));
  memset (instance_and_params, 0, sizeof (GValue) * (n_params + 1));
  param_values = instance_and_params + 1;

  for (i = 0; i < node->n_params; i++)
    {
      gchar *error;
      GType ptype = node->param_types[i] & ~G_SIGNAL_TYPE_STATIC_SCOPE;
      gboolean static_scope = node->param_types[i] & G_SIGNAL_TYPE_STATIC_SCOPE;

      G_VALUE_COLLECT_INIT (param_values + i, ptype,
			    var_args,
			    static_scope ? G_VALUE_NOCOPY_CONTENTS : 0,
			    &error);
      if (error)
	{
	  g_warning ("%s: %s", G_STRLOC, error);
	  g_free (error);

	  /* we purposely leak the value here, it might not be
	   * in a sane state if an error condition occoured
	   */
	  while (i--)
	    g_value_unset (param_values + i);

	  return;
	}
    }

  instance_and_params->g_type = 0;
  g_value_init_from_instance (instance_and_params, instance);
  if (signal_return_type == G_TYPE_NONE)
    signal_emit_unlocked_R (node, detail, instance, NULL, instance_and_params);
  else
    {
      GValue return_value = G_VALUE_INIT;
      gchar *error = NULL;
      GType rtype = signal_return_type & ~G_SIGNAL_TYPE_STATIC_SCOPE;
      gboolean static_scope = signal_return_type & G_SIGNAL_TYPE_STATIC_SCOPE;
      
      g_value_init (&return_value, rtype);

      signal_emit_unlocked_R (node, detail, instance, &return_value, instance_and_params);

      G_VALUE_LCOPY (&return_value,
		     var_args,
		     static_scope ? G_VALUE_NOCOPY_CONTENTS : 0,
		     &error);
      if (!error)
	g_value_unset (&return_value);
      else
	{
	  g_warning ("%s: %s", G_STRLOC, error);
	  g_free (error);
	  
	  /* we purposely leak the value here, it might not be
	   * in a sane state if an error condition occurred
	   */
	}
    }
  for (i = 0; i < n_params; i++)
    g_value_unset (param_values + i);
  g_value_unset (instance_and_params);
}

/**
 * g_signal_emit:
 * @instance: (type GObject.Object): the instance the signal is being emitted on.
 * @signal_id: the signal id
 * @detail: the detail
 * @...: parameters to be passed to the signal, followed by a
 *  location for the return value. If the return type of the signal
 *  is #G_TYPE_NONE, the return value location can be omitted.
 *
 * Emits a signal.
 *
 * Note that g_signal_emit() resets the return value to the default
 * if no handlers are connected, in contrast to g_signal_emitv().
 */
void
g_signal_emit (gpointer instance,
	       guint    signal_id,
	       GQuark   detail,
	       ...)
{
  va_list var_args;

  va_start (var_args, detail);
  g_signal_emit_valist (instance, signal_id, detail, var_args);
  va_end (var_args);
}

/**
 * g_signal_emit_by_name:
 * @instance: (type GObject.Object): the instance the signal is being emitted on.
 * @detailed_signal: a string of the form "signal-name::detail".
 * @...: parameters to be passed to the signal, followed by a
 *  location for the return value. If the return type of the signal
 *  is #G_TYPE_NONE, the return value location can be omitted.
 *
 * Emits a signal.
 *
 * Note that g_signal_emit_by_name() resets the return value to the default
 * if no handlers are connected, in contrast to g_signal_emitv().
 */
void
g_signal_emit_by_name (gpointer     instance,
		       const gchar *detailed_signal,
		       ...)
{
  GQuark detail = 0;
  guint signal_id;
  GType itype;

  g_return_if_fail (G_TYPE_CHECK_INSTANCE (instance));
  g_return_if_fail (detailed_signal != NULL);

  itype = G_TYPE_FROM_INSTANCE (instance);

  SIGNAL_LOCK ();
  signal_id = signal_parse_name (detailed_signal, itype, &detail, TRUE);
  SIGNAL_UNLOCK ();

  if (signal_id)
    {
      va_list var_args;

      va_start (var_args, detailed_signal);
      g_signal_emit_valist (instance, signal_id, detail, var_args);
      va_end (var_args);
    }
  else
    g_warning ("%s: signal name '%s' is invalid for instance '%p' of type '%s'",
               G_STRLOC, detailed_signal, instance, g_type_name (itype));
}

static gboolean
signal_emit_unlocked_R (SignalNode   *node,
			GQuark	      detail,
			gpointer      instance,
			GValue	     *emission_return,
			const GValue *instance_and_params)
{
  SignalAccumulator *accumulator;
  Emission emission;
  GClosure *class_closure;
  HandlerList *hlist;
  Handler *handler_list = NULL;
  GValue *return_accu, accu = G_VALUE_INIT;
  guint signal_id;
  gulong max_sequential_handler_number;
  gboolean return_value_altered = FALSE;
  
  TRACE(GOBJECT_SIGNAL_EMIT(node->signal_id, detail, instance, G_TYPE_FROM_INSTANCE (instance)));

  SIGNAL_LOCK ();
  signal_id = node->signal_id;

  if (node->flags & G_SIGNAL_NO_RECURSE)
    {
      Emission *node = emission_find (signal_id, detail, instance);
      
      if (node)
	{
	  node->state = EMISSION_RESTART;
	  SIGNAL_UNLOCK ();
	  return return_value_altered;
	}
    }
  accumulator = node->accumulator;
  if (accumulator)
    {
      SIGNAL_UNLOCK ();
      g_value_init (&accu, node->return_type & ~G_SIGNAL_TYPE_STATIC_SCOPE);
      return_accu = &accu;
      SIGNAL_LOCK ();
    }
  else
    return_accu = emission_return;
  emission.instance = instance;
  emission.ihint.signal_id = node->signal_id;
  emission.ihint.detail = detail;
  emission.ihint.run_type = 0;
  emission.state = 0;
  emission.chain_type = G_TYPE_NONE;
  emission_push (&emission);
  class_closure = signal_lookup_closure (node, instance);
  
 EMIT_RESTART:
  
  if (handler_list)
    handler_unref_R (signal_id, instance, handler_list);
  max_sequential_handler_number = g_handler_sequential_number;
  hlist = handler_list_lookup (signal_id, instance);
  handler_list = hlist ? hlist->handlers : NULL;
  if (handler_list)
    handler_ref (handler_list);
  
  emission.ihint.run_type = G_SIGNAL_RUN_FIRST;
  
  if ((node->flags & G_SIGNAL_RUN_FIRST) && class_closure)
    {
      emission.state = EMISSION_RUN;

      emission.chain_type = G_TYPE_FROM_INSTANCE (instance);
      SIGNAL_UNLOCK ();
      g_closure_invoke (class_closure,
			return_accu,
			node->n_params + 1,
			instance_and_params,
			&emission.ihint);
      if (!accumulate (&emission.ihint, emission_return, &accu, accumulator) &&
	  emission.state == EMISSION_RUN)
	emission.state = EMISSION_STOP;
      SIGNAL_LOCK ();
      emission.chain_type = G_TYPE_NONE;
      return_value_altered = TRUE;
      
      if (emission.state == EMISSION_STOP)
	goto EMIT_CLEANUP;
      else if (emission.state == EMISSION_RESTART)
	goto EMIT_RESTART;
    }
  
  if (node->emission_hooks)
    {
      gboolean need_destroy, was_in_call, may_recurse = TRUE;
      GHook *hook;

      emission.state = EMISSION_HOOK;
      hook = g_hook_first_valid (node->emission_hooks, may_recurse);
      while (hook)
	{
	  SignalHook *signal_hook = SIGNAL_HOOK (hook);
	  
	  if (!signal_hook->detail || signal_hook->detail == detail)
	    {
	      GSignalEmissionHook hook_func = (GSignalEmissionHook) hook->func;
	      
	      was_in_call = G_HOOK_IN_CALL (hook);
	      hook->flags |= G_HOOK_FLAG_IN_CALL;
              SIGNAL_UNLOCK ();
	      need_destroy = !hook_func (&emission.ihint, node->n_params + 1, instance_and_params, hook->data);
	      SIGNAL_LOCK ();
	      if (!was_in_call)
		hook->flags &= ~G_HOOK_FLAG_IN_CALL;
	      if (need_destroy)
		g_hook_destroy_link (node->emission_hooks, hook);
	    }
	  hook = g_hook_next_valid (node->emission_hooks, hook, may_recurse);
	}
      
      if (emission.state == EMISSION_RESTART)
	goto EMIT_RESTART;
    }
  
  if (handler_list)
    {
      Handler *handler = handler_list;
      
      emission.state = EMISSION_RUN;
      handler_ref (handler);
      do
	{
	  Handler *tmp;
	  
	  if (handler->after)
	    {
	      handler_unref_R (signal_id, instance, handler_list);
	      handler_list = handler;
	      break;
	    }
	  else if (!handler->block_count && (!handler->detail || handler->detail == detail) &&
		   handler->sequential_number < max_sequential_handler_number)
	    {
	      SIGNAL_UNLOCK ();
	      g_closure_invoke (handler->closure,
				return_accu,
				node->n_params + 1,
				instance_and_params,
				&emission.ihint);
	      if (!accumulate (&emission.ihint, emission_return, &accu, accumulator) &&
		  emission.state == EMISSION_RUN)
		emission.state = EMISSION_STOP;
	      SIGNAL_LOCK ();
	      return_value_altered = TRUE;
	      
	      tmp = emission.state == EMISSION_RUN ? handler->next : NULL;
	    }
	  else
	    tmp = handler->next;
	  
	  if (tmp)
	    handler_ref (tmp);
	  handler_unref_R (signal_id, instance, handler_list);
	  handler_list = handler;
	  handler = tmp;
	}
      while (handler);
      
      if (emission.state == EMISSION_STOP)
	goto EMIT_CLEANUP;
      else if (emission.state == EMISSION_RESTART)
	goto EMIT_RESTART;
    }
  
  emission.ihint.run_type = G_SIGNAL_RUN_LAST;
  
  if ((node->flags & G_SIGNAL_RUN_LAST) && class_closure)
    {
      emission.state = EMISSION_RUN;
      
      emission.chain_type = G_TYPE_FROM_INSTANCE (instance);
      SIGNAL_UNLOCK ();
      g_closure_invoke (class_closure,
			return_accu,
			node->n_params + 1,
			instance_and_params,
			&emission.ihint);
      if (!accumulate (&emission.ihint, emission_return, &accu, accumulator) &&
	  emission.state == EMISSION_RUN)
	emission.state = EMISSION_STOP;
      SIGNAL_LOCK ();
      emission.chain_type = G_TYPE_NONE;
      return_value_altered = TRUE;
      
      if (emission.state == EMISSION_STOP)
	goto EMIT_CLEANUP;
      else if (emission.state == EMISSION_RESTART)
	goto EMIT_RESTART;
    }
  
  if (handler_list)
    {
      Handler *handler = handler_list;
      
      emission.state = EMISSION_RUN;
      handler_ref (handler);
      do
	{
	  Handler *tmp;
	  
	  if (handler->after && !handler->block_count && (!handler->detail || handler->detail == detail) &&
	      handler->sequential_number < max_sequential_handler_number)
	    {
	      SIGNAL_UNLOCK ();
	      g_closure_invoke (handler->closure,
				return_accu,
				node->n_params + 1,
				instance_and_params,
				&emission.ihint);
	      if (!accumulate (&emission.ihint, emission_return, &accu, accumulator) &&
		  emission.state == EMISSION_RUN)
		emission.state = EMISSION_STOP;
	      SIGNAL_LOCK ();
	      return_value_altered = TRUE;
	      
	      tmp = emission.state == EMISSION_RUN ? handler->next : NULL;
	    }
	  else
	    tmp = handler->next;
	  
	  if (tmp)
	    handler_ref (tmp);
	  handler_unref_R (signal_id, instance, handler);
	  handler = tmp;
	}
      while (handler);
      
      if (emission.state == EMISSION_STOP)
	goto EMIT_CLEANUP;
      else if (emission.state == EMISSION_RESTART)
	goto EMIT_RESTART;
    }
  
 EMIT_CLEANUP:
  
  emission.ihint.run_type = G_SIGNAL_RUN_CLEANUP;
  
  if ((node->flags & G_SIGNAL_RUN_CLEANUP) && class_closure)
    {
      gboolean need_unset = FALSE;
      
      emission.state = EMISSION_STOP;
      
      emission.chain_type = G_TYPE_FROM_INSTANCE (instance);
      SIGNAL_UNLOCK ();
      if (node->return_type != G_TYPE_NONE && !accumulator)
	{
	  g_value_init (&accu, node->return_type & ~G_SIGNAL_TYPE_STATIC_SCOPE);
	  need_unset = TRUE;
	}
      g_closure_invoke (class_closure,
			node->return_type != G_TYPE_NONE ? &accu : NULL,
			node->n_params + 1,
			instance_and_params,
			&emission.ihint);
      if (need_unset)
	g_value_unset (&accu);
      SIGNAL_LOCK ();
      emission.chain_type = G_TYPE_NONE;
      
      if (emission.state == EMISSION_RESTART)
	goto EMIT_RESTART;
    }
  
  if (handler_list)
    handler_unref_R (signal_id, instance, handler_list);
  
  emission_pop (&emission);
  SIGNAL_UNLOCK ();
  if (accumulator)
    g_value_unset (&accu);

  TRACE(GOBJECT_SIGNAL_EMIT_END(node->signal_id, detail, instance, G_TYPE_FROM_INSTANCE (instance)));

  return return_value_altered;
}

static void
add_invalid_closure_notify (Handler  *handler,
			    gpointer  instance)
{
  g_closure_add_invalidate_notifier (handler->closure, instance, invalid_closure_notify);
  handler->has_invalid_closure_notify = 1;
}

static void
remove_invalid_closure_notify (Handler  *handler,
			       gpointer  instance)
{
  if (handler->has_invalid_closure_notify)
    {
      g_closure_remove_invalidate_notifier (handler->closure, instance, invalid_closure_notify);
      handler->has_invalid_closure_notify = 0;
    }
}

static void
invalid_closure_notify (gpointer  instance,
		        GClosure *closure)
{
  Handler *handler;
  guint signal_id;

  SIGNAL_LOCK ();

  handler = handler_lookup (instance, 0, closure, &signal_id);
  /* See https://bugzilla.gnome.org/show_bug.cgi?id=730296 for discussion about this... */
  g_assert (handler != NULL);
  g_assert (handler->closure == closure);

  handler->sequential_number = 0;
  handler->block_count = 1;
  handler_unref_R (signal_id, instance, handler);

  SIGNAL_UNLOCK ();
}

static const gchar*
type_debug_name (GType type)
{
  if (type)
    {
      const char *name = g_type_name (type & ~G_SIGNAL_TYPE_STATIC_SCOPE);
      return name ? name : "<unknown>";
    }
  else
    return "<invalid>";
}

/**
 * g_signal_accumulator_true_handled:
 * @ihint: standard #GSignalAccumulator parameter
 * @return_accu: standard #GSignalAccumulator parameter
 * @handler_return: standard #GSignalAccumulator parameter
 * @dummy: standard #GSignalAccumulator parameter
 *
 * A predefined #GSignalAccumulator for signals that return a
 * boolean values. The behavior that this accumulator gives is
 * that a return of %TRUE stops the signal emission: no further
 * callbacks will be invoked, while a return of %FALSE allows
 * the emission to continue. The idea here is that a %TRUE return
 * indicates that the callback handled the signal, and no further
 * handling is needed.
 *
 * Since: 2.4
 *
 * Returns: standard #GSignalAccumulator result
 */
gboolean
g_signal_accumulator_true_handled (GSignalInvocationHint *ihint,
				   GValue                *return_accu,
				   const GValue          *handler_return,
				   gpointer               dummy)
{
  gboolean continue_emission;
  gboolean signal_handled;
  
  signal_handled = g_value_get_boolean (handler_return);
  g_value_set_boolean (return_accu, signal_handled);
  continue_emission = !signal_handled;
  
  return continue_emission;
}

/**
 * g_signal_accumulator_first_wins:
 * @ihint: standard #GSignalAccumulator parameter
 * @return_accu: standard #GSignalAccumulator parameter
 * @handler_return: standard #GSignalAccumulator parameter
 * @dummy: standard #GSignalAccumulator parameter
 *
 * A predefined #GSignalAccumulator for signals intended to be used as a
 * hook for application code to provide a particular value.  Usually
 * only one such value is desired and multiple handlers for the same
 * signal don't make much sense (except for the case of the default
 * handler defined in the class structure, in which case you will
 * usually want the signal connection to override the class handler).
 *
 * This accumulator will use the return value from the first signal
 * handler that is run as the return value for the signal and not run
 * any further handlers (ie: the first handler "wins").
 *
 * Returns: standard #GSignalAccumulator result
 *
 * Since: 2.28
 **/
gboolean
g_signal_accumulator_first_wins (GSignalInvocationHint *ihint,
                                 GValue                *return_accu,
                                 const GValue          *handler_return,
                                 gpointer               dummy)
{
  g_value_copy (handler_return, return_accu);
  return FALSE;
}
