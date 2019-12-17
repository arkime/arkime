/* GObject - GLib Type, Object, Parameter and Signal Library
 * Copyright (C) 1998-1999, 2000-2001 Tim Janik and Red Hat, Inc.
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
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

/*
 * MT safe with regards to reference counting.
 */

#include "config.h"

#include <string.h>
#include <signal.h>

#include "gobject.h"
#include "gtype-private.h"
#include "gvaluecollector.h"
#include "gsignal.h"
#include "gparamspecs.h"
#include "gvaluetypes.h"
#include "gobject_trace.h"
#include "gconstructor.h"

/**
 * SECTION:objects
 * @title: GObject
 * @short_description: The base object type
 * @see_also: #GParamSpecObject, g_param_spec_object()
 *
 * GObject is the fundamental type providing the common attributes and
 * methods for all object types in GTK+, Pango and other libraries
 * based on GObject.  The GObject class provides methods for object
 * construction and destruction, property access methods, and signal
 * support.  Signals are described in detail [here][gobject-Signals].
 *
 * For a tutorial on implementing a new GObject class, see [How to define and
 * implement a new GObject][howto-gobject]. For a list of naming conventions for
 * GObjects and their methods, see the [GType conventions][gtype-conventions].
 * For the high-level concepts behind GObject, read [Instantiable classed types:
 * Objects][gtype-instantiable-classed].
 *
 * ## Floating references # {#floating-ref}
 *
 * GInitiallyUnowned is derived from GObject. The only difference between
 * the two is that the initial reference of a GInitiallyUnowned is flagged
 * as a "floating" reference. This means that it is not specifically
 * claimed to be "owned" by any code portion. The main motivation for
 * providing floating references is C convenience. In particular, it
 * allows code to be written as:
 * |[<!-- language="C" --> 
 * container = create_container ();
 * container_add_child (container, create_child());
 * ]|
 * If container_add_child() calls g_object_ref_sink() on the passed-in child,
 * no reference of the newly created child is leaked. Without floating
 * references, container_add_child() can only g_object_ref() the new child,
 * so to implement this code without reference leaks, it would have to be
 * written as:
 * |[<!-- language="C" --> 
 * Child *child;
 * container = create_container ();
 * child = create_child ();
 * container_add_child (container, child);
 * g_object_unref (child);
 * ]|
 * The floating reference can be converted into an ordinary reference by
 * calling g_object_ref_sink(). For already sunken objects (objects that
 * don't have a floating reference anymore), g_object_ref_sink() is equivalent
 * to g_object_ref() and returns a new reference.
 *
 * Since floating references are useful almost exclusively for C convenience,
 * language bindings that provide automated reference and memory ownership
 * maintenance (such as smart pointers or garbage collection) should not
 * expose floating references in their API.
 *
 * Some object implementations may need to save an objects floating state
 * across certain code portions (an example is #GtkMenu), to achieve this,
 * the following sequence can be used:
 *
 * |[<!-- language="C" --> 
 * // save floating state
 * gboolean was_floating = g_object_is_floating (object);
 * g_object_ref_sink (object);
 * // protected code portion
 *
 * ...
 *
 * // restore floating state
 * if (was_floating)
 *   g_object_force_floating (object);
 * else
 *   g_object_unref (object); // release previously acquired reference
 * ]|
 */


/* --- macros --- */
#define PARAM_SPEC_PARAM_ID(pspec)		((pspec)->param_id)
#define	PARAM_SPEC_SET_PARAM_ID(pspec, id)	((pspec)->param_id = (id))

#define OBJECT_HAS_TOGGLE_REF_FLAG 0x1
#define OBJECT_HAS_TOGGLE_REF(object) \
    ((g_datalist_get_flags (&(object)->qdata) & OBJECT_HAS_TOGGLE_REF_FLAG) != 0)
#define OBJECT_FLOATING_FLAG 0x2

#define CLASS_HAS_PROPS_FLAG 0x1
#define CLASS_HAS_PROPS(class) \
    ((class)->flags & CLASS_HAS_PROPS_FLAG)
#define CLASS_HAS_CUSTOM_CONSTRUCTOR(class) \
    ((class)->constructor != g_object_constructor)
#define CLASS_HAS_CUSTOM_CONSTRUCTED(class) \
    ((class)->constructed != g_object_constructed)

#define CLASS_HAS_DERIVED_CLASS_FLAG 0x2
#define CLASS_HAS_DERIVED_CLASS(class) \
    ((class)->flags & CLASS_HAS_DERIVED_CLASS_FLAG)

/* --- signals --- */
enum {
  NOTIFY,
  LAST_SIGNAL
};


/* --- properties --- */
enum {
  PROP_NONE
};


/* --- prototypes --- */
static void	g_object_base_class_init		(GObjectClass	*class);
static void	g_object_base_class_finalize		(GObjectClass	*class);
static void	g_object_do_class_init			(GObjectClass	*class);
static void	g_object_init				(GObject	*object,
							 GObjectClass	*class);
static GObject*	g_object_constructor			(GType                  type,
							 guint                  n_construct_properties,
							 GObjectConstructParam *construct_params);
static void     g_object_constructed                    (GObject        *object);
static void	g_object_real_dispose			(GObject	*object);
static void	g_object_finalize			(GObject	*object);
static void	g_object_do_set_property		(GObject        *object,
							 guint           property_id,
							 const GValue   *value,
							 GParamSpec     *pspec);
static void	g_object_do_get_property		(GObject        *object,
							 guint           property_id,
							 GValue         *value,
							 GParamSpec     *pspec);
static void	g_value_object_init			(GValue		*value);
static void	g_value_object_free_value		(GValue		*value);
static void	g_value_object_copy_value		(const GValue	*src_value,
							 GValue		*dest_value);
static void	g_value_object_transform_value		(const GValue	*src_value,
							 GValue		*dest_value);
static gpointer g_value_object_peek_pointer             (const GValue   *value);
static gchar*	g_value_object_collect_value		(GValue		*value,
							 guint           n_collect_values,
							 GTypeCValue    *collect_values,
							 guint           collect_flags);
static gchar*	g_value_object_lcopy_value		(const GValue	*value,
							 guint           n_collect_values,
							 GTypeCValue    *collect_values,
							 guint           collect_flags);
static void	g_object_dispatch_properties_changed	(GObject	*object,
							 guint		 n_pspecs,
							 GParamSpec    **pspecs);
static guint               object_floating_flag_handler (GObject        *object,
                                                         gint            job);

static void object_interface_check_properties           (gpointer        check_data,
							 gpointer        g_iface);

/* --- typedefs --- */
typedef struct _GObjectNotifyQueue            GObjectNotifyQueue;

struct _GObjectNotifyQueue
{
  GSList  *pspecs;
  guint16  n_pspecs;
  guint16  freeze_count;
};

/* --- variables --- */
G_LOCK_DEFINE_STATIC (closure_array_mutex);
G_LOCK_DEFINE_STATIC (weak_refs_mutex);
G_LOCK_DEFINE_STATIC (toggle_refs_mutex);
static GQuark	            quark_closure_array = 0;
static GQuark	            quark_weak_refs = 0;
static GQuark	            quark_toggle_refs = 0;
static GQuark               quark_notify_queue;
static GQuark               quark_in_construction;
static GParamSpecPool      *pspec_pool = NULL;
static gulong	            gobject_signals[LAST_SIGNAL] = { 0, };
static guint (*floating_flag_handler) (GObject*, gint) = object_floating_flag_handler;
/* qdata pointing to GSList<GWeakRef *>, protected by weak_locations_lock */
static GQuark	            quark_weak_locations = 0;
static GRWLock              weak_locations_lock;

G_LOCK_DEFINE_STATIC(notify_lock);

/* --- functions --- */
static void
g_object_notify_queue_free (gpointer data)
{
  GObjectNotifyQueue *nqueue = data;

  g_slist_free (nqueue->pspecs);
  g_slice_free (GObjectNotifyQueue, nqueue);
}

static GObjectNotifyQueue*
g_object_notify_queue_freeze (GObject  *object,
                              gboolean  conditional)
{
  GObjectNotifyQueue *nqueue;

  G_LOCK(notify_lock);
  nqueue = g_datalist_id_get_data (&object->qdata, quark_notify_queue);
  if (!nqueue)
    {
      if (conditional)
        {
          G_UNLOCK(notify_lock);
          return NULL;
        }

      nqueue = g_slice_new0 (GObjectNotifyQueue);
      g_datalist_id_set_data_full (&object->qdata, quark_notify_queue,
                                   nqueue, g_object_notify_queue_free);
    }

  if (nqueue->freeze_count >= 65535)
    g_critical("Free queue for %s (%p) is larger than 65535,"
               " called g_object_freeze_notify() too often."
               " Forgot to call g_object_thaw_notify() or infinite loop",
               G_OBJECT_TYPE_NAME (object), object);
  else
    nqueue->freeze_count++;
  G_UNLOCK(notify_lock);

  return nqueue;
}

static void
g_object_notify_queue_thaw (GObject            *object,
                            GObjectNotifyQueue *nqueue)
{
  GParamSpec *pspecs_mem[16], **pspecs, **free_me = NULL;
  GSList *slist;
  guint n_pspecs = 0;

  g_return_if_fail (nqueue->freeze_count > 0);
  g_return_if_fail (g_atomic_int_get(&object->ref_count) > 0);

  G_LOCK(notify_lock);

  /* Just make sure we never get into some nasty race condition */
  if (G_UNLIKELY(nqueue->freeze_count == 0)) {
    G_UNLOCK(notify_lock);
    g_warning ("%s: property-changed notification for %s(%p) is not frozen",
               G_STRFUNC, G_OBJECT_TYPE_NAME (object), object);
    return;
  }

  nqueue->freeze_count--;
  if (nqueue->freeze_count) {
    G_UNLOCK(notify_lock);
    return;
  }

  pspecs = nqueue->n_pspecs > 16 ? free_me = g_new (GParamSpec*, nqueue->n_pspecs) : pspecs_mem;

  for (slist = nqueue->pspecs; slist; slist = slist->next)
    {
      pspecs[n_pspecs++] = slist->data;
    }
  g_datalist_id_set_data (&object->qdata, quark_notify_queue, NULL);

  G_UNLOCK(notify_lock);

  if (n_pspecs)
    G_OBJECT_GET_CLASS (object)->dispatch_properties_changed (object, n_pspecs, pspecs);
  g_free (free_me);
}

static void
g_object_notify_queue_add (GObject            *object,
                           GObjectNotifyQueue *nqueue,
                           GParamSpec         *pspec)
{
  G_LOCK(notify_lock);

  g_assert (nqueue->n_pspecs < 65535);

  if (g_slist_find (nqueue->pspecs, pspec) == NULL)
    {
      nqueue->pspecs = g_slist_prepend (nqueue->pspecs, pspec);
      nqueue->n_pspecs++;
    }

  G_UNLOCK(notify_lock);
}

#ifdef	G_ENABLE_DEBUG
G_LOCK_DEFINE_STATIC     (debug_objects);
static guint		 debug_objects_count = 0;
static GHashTable	*debug_objects_ht = NULL;

static void
debug_objects_foreach (gpointer key,
		       gpointer value,
		       gpointer user_data)
{
  GObject *object = value;

  g_message ("[%p] stale %s\tref_count=%u",
	     object,
	     G_OBJECT_TYPE_NAME (object),
	     object->ref_count);
}

#ifdef G_HAS_CONSTRUCTORS
#ifdef G_DEFINE_DESTRUCTOR_NEEDS_PRAGMA
#pragma G_DEFINE_DESTRUCTOR_PRAGMA_ARGS(debug_objects_atexit)
#endif
G_DEFINE_DESTRUCTOR(debug_objects_atexit)
#endif /* G_HAS_CONSTRUCTORS */

static void
debug_objects_atexit (void)
{
  GOBJECT_IF_DEBUG (OBJECTS,
    {
      G_LOCK (debug_objects);
      g_message ("stale GObjects: %u", debug_objects_count);
      g_hash_table_foreach (debug_objects_ht, debug_objects_foreach, NULL);
      G_UNLOCK (debug_objects);
    });
}
#endif	/* G_ENABLE_DEBUG */

void
_g_object_type_init (void)
{
  static gboolean initialized = FALSE;
  static const GTypeFundamentalInfo finfo = {
    G_TYPE_FLAG_CLASSED | G_TYPE_FLAG_INSTANTIATABLE | G_TYPE_FLAG_DERIVABLE | G_TYPE_FLAG_DEEP_DERIVABLE,
  };
  GTypeInfo info = {
    sizeof (GObjectClass),
    (GBaseInitFunc) g_object_base_class_init,
    (GBaseFinalizeFunc) g_object_base_class_finalize,
    (GClassInitFunc) g_object_do_class_init,
    NULL	/* class_destroy */,
    NULL	/* class_data */,
    sizeof (GObject),
    0		/* n_preallocs */,
    (GInstanceInitFunc) g_object_init,
    NULL,	/* value_table */
  };
  static const GTypeValueTable value_table = {
    g_value_object_init,	  /* value_init */
    g_value_object_free_value,	  /* value_free */
    g_value_object_copy_value,	  /* value_copy */
    g_value_object_peek_pointer,  /* value_peek_pointer */
    "p",			  /* collect_format */
    g_value_object_collect_value, /* collect_value */
    "p",			  /* lcopy_format */
    g_value_object_lcopy_value,	  /* lcopy_value */
  };
  GType type;
  
  g_return_if_fail (initialized == FALSE);
  initialized = TRUE;
  
  /* G_TYPE_OBJECT
   */
  info.value_table = &value_table;
  type = g_type_register_fundamental (G_TYPE_OBJECT, g_intern_static_string ("GObject"), &info, &finfo, 0);
  g_assert (type == G_TYPE_OBJECT);
  g_value_register_transform_func (G_TYPE_OBJECT, G_TYPE_OBJECT, g_value_object_transform_value);

#if G_ENABLE_DEBUG
  /* We cannot use GOBJECT_IF_DEBUG here because of the G_HAS_CONSTRUCTORS
   * conditional in between, as the C spec leaves conditionals inside macro
   * expansions as undefined behavior. Only GCC and Clang are known to work
   * but compilation breaks on MSVC.
   *
   * See: https://bugzilla.gnome.org/show_bug.cgi?id=769504
   */
  if (_g_type_debug_flags & G_TYPE_DEBUG_OBJECTS) \
    {
      debug_objects_ht = g_hash_table_new (g_direct_hash, NULL);
# ifndef G_HAS_CONSTRUCTORS
      g_atexit (debug_objects_atexit);
# endif /* G_HAS_CONSTRUCTORS */
    }
#endif /* G_ENABLE_DEBUG */
}

static void
g_object_base_class_init (GObjectClass *class)
{
  GObjectClass *pclass = g_type_class_peek_parent (class);

  /* Don't inherit HAS_DERIVED_CLASS flag from parent class */
  class->flags &= ~CLASS_HAS_DERIVED_CLASS_FLAG;

  if (pclass)
    pclass->flags |= CLASS_HAS_DERIVED_CLASS_FLAG;

  /* reset instance specific fields and methods that don't get inherited */
  class->construct_properties = pclass ? g_slist_copy (pclass->construct_properties) : NULL;
  class->get_property = NULL;
  class->set_property = NULL;
}

static void
g_object_base_class_finalize (GObjectClass *class)
{
  GList *list, *node;
  
  _g_signals_destroy (G_OBJECT_CLASS_TYPE (class));

  g_slist_free (class->construct_properties);
  class->construct_properties = NULL;
  list = g_param_spec_pool_list_owned (pspec_pool, G_OBJECT_CLASS_TYPE (class));
  for (node = list; node; node = node->next)
    {
      GParamSpec *pspec = node->data;
      
      g_param_spec_pool_remove (pspec_pool, pspec);
      PARAM_SPEC_SET_PARAM_ID (pspec, 0);
      g_param_spec_unref (pspec);
    }
  g_list_free (list);
}

static void
g_object_do_class_init (GObjectClass *class)
{
  /* read the comment about typedef struct CArray; on why not to change this quark */
  quark_closure_array = g_quark_from_static_string ("GObject-closure-array");

  quark_weak_refs = g_quark_from_static_string ("GObject-weak-references");
  quark_weak_locations = g_quark_from_static_string ("GObject-weak-locations");
  quark_toggle_refs = g_quark_from_static_string ("GObject-toggle-references");
  quark_notify_queue = g_quark_from_static_string ("GObject-notify-queue");
  quark_in_construction = g_quark_from_static_string ("GObject-in-construction");
  pspec_pool = g_param_spec_pool_new (TRUE);

  class->constructor = g_object_constructor;
  class->constructed = g_object_constructed;
  class->set_property = g_object_do_set_property;
  class->get_property = g_object_do_get_property;
  class->dispose = g_object_real_dispose;
  class->finalize = g_object_finalize;
  class->dispatch_properties_changed = g_object_dispatch_properties_changed;
  class->notify = NULL;

  /**
   * GObject::notify:
   * @gobject: the object which received the signal.
   * @pspec: the #GParamSpec of the property which changed.
   *
   * The notify signal is emitted on an object when one of its
   * properties has been changed. Note that getting this signal
   * doesn't guarantee that the value of the property has actually
   * changed, it may also be emitted when the setter for the property
   * is called to reinstate the previous value.
   *
   * This signal is typically used to obtain change notification for a
   * single property, by specifying the property name as a detail in the
   * g_signal_connect() call, like this:
   * |[<!-- language="C" --> 
   * g_signal_connect (text_view->buffer, "notify::paste-target-list",
   *                   G_CALLBACK (gtk_text_view_target_list_notify),
   *                   text_view)
   * ]|
   * It is important to note that you must use
   * [canonical][canonical-parameter-name] parameter names as
   * detail strings for the notify signal.
   */
  gobject_signals[NOTIFY] =
    g_signal_new (g_intern_static_string ("notify"),
		  G_TYPE_FROM_CLASS (class),
		  G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE | G_SIGNAL_DETAILED | G_SIGNAL_NO_HOOKS | G_SIGNAL_ACTION,
		  G_STRUCT_OFFSET (GObjectClass, notify),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__PARAM,
		  G_TYPE_NONE,
		  1, G_TYPE_PARAM);

  /* Install a check function that we'll use to verify that classes that
   * implement an interface implement all properties for that interface
   */
  g_type_add_interface_check (NULL, object_interface_check_properties);
}

static inline gboolean
install_property_internal (GType       g_type,
			   guint       property_id,
			   GParamSpec *pspec)
{
  if (g_param_spec_pool_lookup (pspec_pool, pspec->name, g_type, FALSE))
    {
      g_warning ("When installing property: type '%s' already has a property named '%s'",
		 g_type_name (g_type),
		 pspec->name);
      return FALSE;
    }

  g_param_spec_ref_sink (pspec);
  PARAM_SPEC_SET_PARAM_ID (pspec, property_id);
  g_param_spec_pool_insert (pspec_pool, pspec, g_type);
  return TRUE;
}

static gboolean
validate_pspec_to_install (GParamSpec *pspec)
{
  g_return_val_if_fail (G_IS_PARAM_SPEC (pspec), FALSE);
  g_return_val_if_fail (PARAM_SPEC_PARAM_ID (pspec) == 0, FALSE);	/* paranoid */

  g_return_val_if_fail (pspec->flags & (G_PARAM_READABLE | G_PARAM_WRITABLE), FALSE);

  if (pspec->flags & G_PARAM_CONSTRUCT)
    g_return_val_if_fail ((pspec->flags & G_PARAM_CONSTRUCT_ONLY) == 0, FALSE);

  if (pspec->flags & (G_PARAM_CONSTRUCT | G_PARAM_CONSTRUCT_ONLY))
    g_return_val_if_fail (pspec->flags & G_PARAM_WRITABLE, FALSE);

  return TRUE;
}

static gboolean
validate_and_install_class_property (GObjectClass *class,
                                     GType         oclass_type,
                                     GType         parent_type,
                                     guint         property_id,
                                     GParamSpec   *pspec)
{
  if (!validate_pspec_to_install (pspec))
    return FALSE;

  if (pspec->flags & G_PARAM_WRITABLE)
    g_return_val_if_fail (class->set_property != NULL, FALSE);
  if (pspec->flags & G_PARAM_READABLE)
    g_return_val_if_fail (class->get_property != NULL, FALSE);

  class->flags |= CLASS_HAS_PROPS_FLAG;
  if (install_property_internal (oclass_type, property_id, pspec))
    {
      if (pspec->flags & (G_PARAM_CONSTRUCT | G_PARAM_CONSTRUCT_ONLY))
        class->construct_properties = g_slist_append (class->construct_properties, pspec);

      /* for property overrides of construct properties, we have to get rid
       * of the overidden inherited construct property
       */
      pspec = g_param_spec_pool_lookup (pspec_pool, pspec->name, parent_type, TRUE);
      if (pspec && pspec->flags & (G_PARAM_CONSTRUCT | G_PARAM_CONSTRUCT_ONLY))
        class->construct_properties = g_slist_remove (class->construct_properties, pspec);

      return TRUE;
    }
  else
    return FALSE;
}

/**
 * g_object_class_install_property:
 * @oclass: a #GObjectClass
 * @property_id: the id for the new property
 * @pspec: the #GParamSpec for the new property
 *
 * Installs a new property.
 *
 * All properties should be installed during the class initializer.  It
 * is possible to install properties after that, but doing so is not
 * recommend, and specifically, is not guaranteed to be thread-safe vs.
 * use of properties on the same type on other threads.
 *
 * Note that it is possible to redefine a property in a derived class,
 * by installing a property with the same name. This can be useful at times,
 * e.g. to change the range of allowed values or the default value.
 */
void
g_object_class_install_property (GObjectClass *class,
				 guint	       property_id,
				 GParamSpec   *pspec)
{
  GType oclass_type, parent_type;

  g_return_if_fail (G_IS_OBJECT_CLASS (class));
  g_return_if_fail (property_id > 0);

  oclass_type = G_OBJECT_CLASS_TYPE (class);
  parent_type = g_type_parent (oclass_type);

  if (CLASS_HAS_DERIVED_CLASS (class))
    g_error ("Attempt to add property %s::%s to class after it was derived", G_OBJECT_CLASS_NAME (class), pspec->name);

  (void) validate_and_install_class_property (class,
                                              oclass_type,
                                              parent_type,
                                              property_id,
                                              pspec);
}

/**
 * g_object_class_install_properties:
 * @oclass: a #GObjectClass
 * @n_pspecs: the length of the #GParamSpecs array
 * @pspecs: (array length=n_pspecs): the #GParamSpecs array
 *   defining the new properties
 *
 * Installs new properties from an array of #GParamSpecs.
 *
 * All properties should be installed during the class initializer.  It
 * is possible to install properties after that, but doing so is not
 * recommend, and specifically, is not guaranteed to be thread-safe vs.
 * use of properties on the same type on other threads.
 *
 * The property id of each property is the index of each #GParamSpec in
 * the @pspecs array.
 *
 * The property id of 0 is treated specially by #GObject and it should not
 * be used to store a #GParamSpec.
 *
 * This function should be used if you plan to use a static array of
 * #GParamSpecs and g_object_notify_by_pspec(). For instance, this
 * class initialization:
 *
 * |[<!-- language="C" --> 
 * enum {
 *   PROP_0, PROP_FOO, PROP_BAR, N_PROPERTIES
 * };
 *
 * static GParamSpec *obj_properties[N_PROPERTIES] = { NULL, };
 *
 * static void
 * my_object_class_init (MyObjectClass *klass)
 * {
 *   GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
 *
 *   obj_properties[PROP_FOO] =
 *     g_param_spec_int ("foo", "Foo", "Foo",
 *                       -1, G_MAXINT,
 *                       0,
 *                       G_PARAM_READWRITE);
 *
 *   obj_properties[PROP_BAR] =
 *     g_param_spec_string ("bar", "Bar", "Bar",
 *                          NULL,
 *                          G_PARAM_READWRITE);
 *
 *   gobject_class->set_property = my_object_set_property;
 *   gobject_class->get_property = my_object_get_property;
 *   g_object_class_install_properties (gobject_class,
 *                                      N_PROPERTIES,
 *                                      obj_properties);
 * }
 * ]|
 *
 * allows calling g_object_notify_by_pspec() to notify of property changes:
 *
 * |[<!-- language="C" --> 
 * void
 * my_object_set_foo (MyObject *self, gint foo)
 * {
 *   if (self->foo != foo)
 *     {
 *       self->foo = foo;
 *       g_object_notify_by_pspec (G_OBJECT (self), obj_properties[PROP_FOO]);
 *     }
 *  }
 * ]|
 *
 * Since: 2.26
 */
void
g_object_class_install_properties (GObjectClass  *oclass,
                                   guint          n_pspecs,
                                   GParamSpec   **pspecs)
{
  GType oclass_type, parent_type;
  gint i;

  g_return_if_fail (G_IS_OBJECT_CLASS (oclass));
  g_return_if_fail (n_pspecs > 1);
  g_return_if_fail (pspecs[0] == NULL);

  if (CLASS_HAS_DERIVED_CLASS (oclass))
    g_error ("Attempt to add properties to %s after it was derived",
             G_OBJECT_CLASS_NAME (oclass));

  oclass_type = G_OBJECT_CLASS_TYPE (oclass);
  parent_type = g_type_parent (oclass_type);

  /* we skip the first element of the array as it would have a 0 prop_id */
  for (i = 1; i < n_pspecs; i++)
    {
      GParamSpec *pspec = pspecs[i];

      if (!validate_and_install_class_property (oclass,
                                                oclass_type,
                                                parent_type,
                                                i,
                                                pspec))
        {
          break;
        }
    }
}

/**
 * g_object_interface_install_property:
 * @g_iface: (type GObject.TypeInterface): any interface vtable for the
 *    interface, or the default
 *  vtable for the interface.
 * @pspec: the #GParamSpec for the new property
 *
 * Add a property to an interface; this is only useful for interfaces
 * that are added to GObject-derived types. Adding a property to an
 * interface forces all objects classes with that interface to have a
 * compatible property. The compatible property could be a newly
 * created #GParamSpec, but normally
 * g_object_class_override_property() will be used so that the object
 * class only needs to provide an implementation and inherits the
 * property description, default value, bounds, and so forth from the
 * interface property.
 *
 * This function is meant to be called from the interface's default
 * vtable initialization function (the @class_init member of
 * #GTypeInfo.) It must not be called after after @class_init has
 * been called for any object types implementing this interface.
 *
 * If @pspec is a floating reference, it will be consumed.
 *
 * Since: 2.4
 */
void
g_object_interface_install_property (gpointer      g_iface,
				     GParamSpec   *pspec)
{
  GTypeInterface *iface_class = g_iface;
	
  g_return_if_fail (G_TYPE_IS_INTERFACE (iface_class->g_type));
  g_return_if_fail (!G_IS_PARAM_SPEC_OVERRIDE (pspec)); /* paranoid */

  if (!validate_pspec_to_install (pspec))
    return;

  (void) install_property_internal (iface_class->g_type, 0, pspec);
}

/**
 * g_object_class_find_property:
 * @oclass: a #GObjectClass
 * @property_name: the name of the property to look up
 *
 * Looks up the #GParamSpec for a property of a class.
 *
 * Returns: (transfer none): the #GParamSpec for the property, or
 *          %NULL if the class doesn't have a property of that name
 */
GParamSpec*
g_object_class_find_property (GObjectClass *class,
			      const gchar  *property_name)
{
  GParamSpec *pspec;
  GParamSpec *redirect;
	
  g_return_val_if_fail (G_IS_OBJECT_CLASS (class), NULL);
  g_return_val_if_fail (property_name != NULL, NULL);
  
  pspec = g_param_spec_pool_lookup (pspec_pool,
				    property_name,
				    G_OBJECT_CLASS_TYPE (class),
				    TRUE);
  if (pspec)
    {
      redirect = g_param_spec_get_redirect_target (pspec);
      if (redirect)
	return redirect;
      else
	return pspec;
    }
  else
    return NULL;
}

/**
 * g_object_interface_find_property:
 * @g_iface: (type GObject.TypeInterface): any interface vtable for the
 *  interface, or the default vtable for the interface
 * @property_name: name of a property to lookup.
 *
 * Find the #GParamSpec with the given name for an
 * interface. Generally, the interface vtable passed in as @g_iface
 * will be the default vtable from g_type_default_interface_ref(), or,
 * if you know the interface has already been loaded,
 * g_type_default_interface_peek().
 *
 * Since: 2.4
 *
 * Returns: (transfer none): the #GParamSpec for the property of the
 *          interface with the name @property_name, or %NULL if no
 *          such property exists.
 */
GParamSpec*
g_object_interface_find_property (gpointer      g_iface,
				  const gchar  *property_name)
{
  GTypeInterface *iface_class = g_iface;
	
  g_return_val_if_fail (G_TYPE_IS_INTERFACE (iface_class->g_type), NULL);
  g_return_val_if_fail (property_name != NULL, NULL);
  
  return g_param_spec_pool_lookup (pspec_pool,
				   property_name,
				   iface_class->g_type,
				   FALSE);
}

/**
 * g_object_class_override_property:
 * @oclass: a #GObjectClass
 * @property_id: the new property ID
 * @name: the name of a property registered in a parent class or
 *  in an interface of this class.
 *
 * Registers @property_id as referring to a property with the name
 * @name in a parent class or in an interface implemented by @oclass.
 * This allows this class to "override" a property implementation in
 * a parent class or to provide the implementation of a property from
 * an interface.
 *
 * Internally, overriding is implemented by creating a property of type
 * #GParamSpecOverride; generally operations that query the properties of
 * the object class, such as g_object_class_find_property() or
 * g_object_class_list_properties() will return the overridden
 * property. However, in one case, the @construct_properties argument of
 * the @constructor virtual function, the #GParamSpecOverride is passed
 * instead, so that the @param_id field of the #GParamSpec will be
 * correct.  For virtually all uses, this makes no difference. If you
 * need to get the overridden property, you can call
 * g_param_spec_get_redirect_target().
 *
 * Since: 2.4
 */
void
g_object_class_override_property (GObjectClass *oclass,
				  guint         property_id,
				  const gchar  *name)
{
  GParamSpec *overridden = NULL;
  GParamSpec *new;
  GType parent_type;
  
  g_return_if_fail (G_IS_OBJECT_CLASS (oclass));
  g_return_if_fail (property_id > 0);
  g_return_if_fail (name != NULL);

  /* Find the overridden property; first check parent types
   */
  parent_type = g_type_parent (G_OBJECT_CLASS_TYPE (oclass));
  if (parent_type != G_TYPE_NONE)
    overridden = g_param_spec_pool_lookup (pspec_pool,
					   name,
					   parent_type,
					   TRUE);
  if (!overridden)
    {
      GType *ifaces;
      guint n_ifaces;
      
      /* Now check interfaces
       */
      ifaces = g_type_interfaces (G_OBJECT_CLASS_TYPE (oclass), &n_ifaces);
      while (n_ifaces-- && !overridden)
	{
	  overridden = g_param_spec_pool_lookup (pspec_pool,
						 name,
						 ifaces[n_ifaces],
						 FALSE);
	}
      
      g_free (ifaces);
    }

  if (!overridden)
    {
      g_warning ("%s: Can't find property to override for '%s::%s'",
		 G_STRFUNC, G_OBJECT_CLASS_NAME (oclass), name);
      return;
    }

  new = g_param_spec_override (name, overridden);
  g_object_class_install_property (oclass, property_id, new);
}

/**
 * g_object_class_list_properties:
 * @oclass: a #GObjectClass
 * @n_properties: (out): return location for the length of the returned array
 *
 * Get an array of #GParamSpec* for all properties of a class.
 *
 * Returns: (array length=n_properties) (transfer container): an array of
 *          #GParamSpec* which should be freed after use
 */
GParamSpec** /* free result */
g_object_class_list_properties (GObjectClass *class,
				guint        *n_properties_p)
{
  GParamSpec **pspecs;
  guint n;

  g_return_val_if_fail (G_IS_OBJECT_CLASS (class), NULL);

  pspecs = g_param_spec_pool_list (pspec_pool,
				   G_OBJECT_CLASS_TYPE (class),
				   &n);
  if (n_properties_p)
    *n_properties_p = n;

  return pspecs;
}

/**
 * g_object_interface_list_properties:
 * @g_iface: (type GObject.TypeInterface): any interface vtable for the
 *  interface, or the default vtable for the interface
 * @n_properties_p: (out): location to store number of properties returned.
 *
 * Lists the properties of an interface.Generally, the interface
 * vtable passed in as @g_iface will be the default vtable from
 * g_type_default_interface_ref(), or, if you know the interface has
 * already been loaded, g_type_default_interface_peek().
 *
 * Since: 2.4
 *
 * Returns: (array length=n_properties_p) (transfer container): a
 *          pointer to an array of pointers to #GParamSpec
 *          structures. The paramspecs are owned by GLib, but the
 *          array should be freed with g_free() when you are done with
 *          it.
 */
GParamSpec**
g_object_interface_list_properties (gpointer      g_iface,
				    guint        *n_properties_p)
{
  GTypeInterface *iface_class = g_iface;
  GParamSpec **pspecs;
  guint n;

  g_return_val_if_fail (G_TYPE_IS_INTERFACE (iface_class->g_type), NULL);

  pspecs = g_param_spec_pool_list (pspec_pool,
				   iface_class->g_type,
				   &n);
  if (n_properties_p)
    *n_properties_p = n;

  return pspecs;
}

static inline gboolean
object_in_construction (GObject *object)
{
  return g_datalist_id_get_data (&object->qdata, quark_in_construction) != NULL;
}

static void
g_object_init (GObject		*object,
	       GObjectClass	*class)
{
  object->ref_count = 1;
  object->qdata = NULL;

  if (CLASS_HAS_PROPS (class))
    {
      /* freeze object's notification queue, g_object_newv() preserves pairedness */
      g_object_notify_queue_freeze (object, FALSE);
    }

  if (CLASS_HAS_CUSTOM_CONSTRUCTOR (class))
    {
      /* mark object in-construction for notify_queue_thaw() and to allow construct-only properties */
      g_datalist_id_set_data (&object->qdata, quark_in_construction, object);
    }

  GOBJECT_IF_DEBUG (OBJECTS,
    {
      G_LOCK (debug_objects);
      debug_objects_count++;
      g_hash_table_add (debug_objects_ht, object);
      G_UNLOCK (debug_objects);
    });
}

static void
g_object_do_set_property (GObject      *object,
			  guint         property_id,
			  const GValue *value,
			  GParamSpec   *pspec)
{
  switch (property_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
g_object_do_get_property (GObject     *object,
			  guint        property_id,
			  GValue      *value,
			  GParamSpec  *pspec)
{
  switch (property_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
g_object_real_dispose (GObject *object)
{
  g_signal_handlers_destroy (object);
  g_datalist_id_set_data (&object->qdata, quark_closure_array, NULL);
  g_datalist_id_set_data (&object->qdata, quark_weak_refs, NULL);
}

static void
g_object_finalize (GObject *object)
{
  if (object_in_construction (object))
    {
      g_critical ("object %s %p finalized while still in-construction",
                  G_OBJECT_TYPE_NAME (object), object);
    }

  g_datalist_clear (&object->qdata);
  
  GOBJECT_IF_DEBUG (OBJECTS,
    {
      G_LOCK (debug_objects);
      g_assert (g_hash_table_contains (debug_objects_ht, object));
      g_hash_table_remove (debug_objects_ht, object);
      debug_objects_count--;
      G_UNLOCK (debug_objects);
    });
}

static void
g_object_dispatch_properties_changed (GObject     *object,
				      guint        n_pspecs,
				      GParamSpec **pspecs)
{
  guint i;

  for (i = 0; i < n_pspecs; i++)
    g_signal_emit (object, gobject_signals[NOTIFY], g_param_spec_get_name_quark (pspecs[i]), pspecs[i]);
}

/**
 * g_object_run_dispose:
 * @object: a #GObject
 *
 * Releases all references to other objects. This can be used to break
 * reference cycles.
 *
 * This function should only be called from object system implementations.
 */
void
g_object_run_dispose (GObject *object)
{
  g_return_if_fail (G_IS_OBJECT (object));
  g_return_if_fail (object->ref_count > 0);

  g_object_ref (object);
  TRACE (GOBJECT_OBJECT_DISPOSE(object,G_TYPE_FROM_INSTANCE(object), 0));
  G_OBJECT_GET_CLASS (object)->dispose (object);
  TRACE (GOBJECT_OBJECT_DISPOSE_END(object,G_TYPE_FROM_INSTANCE(object), 0));
  g_object_unref (object);
}

/**
 * g_object_freeze_notify:
 * @object: a #GObject
 *
 * Increases the freeze count on @object. If the freeze count is
 * non-zero, the emission of "notify" signals on @object is
 * stopped. The signals are queued until the freeze count is decreased
 * to zero. Duplicate notifications are squashed so that at most one
 * #GObject::notify signal is emitted for each property modified while the
 * object is frozen.
 *
 * This is necessary for accessors that modify multiple properties to prevent
 * premature notification while the object is still being modified.
 */
void
g_object_freeze_notify (GObject *object)
{
  g_return_if_fail (G_IS_OBJECT (object));

  if (g_atomic_int_get (&object->ref_count) == 0)
    return;

  g_object_ref (object);
  g_object_notify_queue_freeze (object, FALSE);
  g_object_unref (object);
}

static GParamSpec *
get_notify_pspec (GParamSpec *pspec)
{
  GParamSpec *redirected;

  /* we don't notify on non-READABLE parameters */
  if (~pspec->flags & G_PARAM_READABLE)
    return NULL;

  /* if the paramspec is redirected, notify on the target */
  redirected = g_param_spec_get_redirect_target (pspec);
  if (redirected != NULL)
    return redirected;

  /* else, notify normally */
  return pspec;
}

static inline void
g_object_notify_by_spec_internal (GObject    *object,
				  GParamSpec *pspec)
{
  GParamSpec *notify_pspec;

  notify_pspec = get_notify_pspec (pspec);

  if (notify_pspec != NULL)
    {
      GObjectNotifyQueue *nqueue;

      /* conditional freeze: only increase freeze count if already frozen */
      nqueue = g_object_notify_queue_freeze (object, TRUE);

      if (nqueue != NULL)
        {
          /* we're frozen, so add to the queue and release our freeze */
          g_object_notify_queue_add (object, nqueue, notify_pspec);
          g_object_notify_queue_thaw (object, nqueue);
        }
      else
        /* not frozen, so just dispatch the notification directly */
        G_OBJECT_GET_CLASS (object)
          ->dispatch_properties_changed (object, 1, &notify_pspec);
    }
}

/**
 * g_object_notify:
 * @object: a #GObject
 * @property_name: the name of a property installed on the class of @object.
 *
 * Emits a "notify" signal for the property @property_name on @object.
 *
 * When possible, eg. when signaling a property change from within the class
 * that registered the property, you should use g_object_notify_by_pspec()
 * instead.
 *
 * Note that emission of the notify signal may be blocked with
 * g_object_freeze_notify(). In this case, the signal emissions are queued
 * and will be emitted (in reverse order) when g_object_thaw_notify() is
 * called.
 */
void
g_object_notify (GObject     *object,
		 const gchar *property_name)
{
  GParamSpec *pspec;
  
  g_return_if_fail (G_IS_OBJECT (object));
  g_return_if_fail (property_name != NULL);
  if (g_atomic_int_get (&object->ref_count) == 0)
    return;
  
  g_object_ref (object);
  /* We don't need to get the redirect target
   * (by, e.g. calling g_object_class_find_property())
   * because g_object_notify_queue_add() does that
   */
  pspec = g_param_spec_pool_lookup (pspec_pool,
				    property_name,
				    G_OBJECT_TYPE (object),
				    TRUE);

  if (!pspec)
    g_warning ("%s: object class '%s' has no property named '%s'",
	       G_STRFUNC,
	       G_OBJECT_TYPE_NAME (object),
	       property_name);
  else
    g_object_notify_by_spec_internal (object, pspec);
  g_object_unref (object);
}

/**
 * g_object_notify_by_pspec:
 * @object: a #GObject
 * @pspec: the #GParamSpec of a property installed on the class of @object.
 *
 * Emits a "notify" signal for the property specified by @pspec on @object.
 *
 * This function omits the property name lookup, hence it is faster than
 * g_object_notify().
 *
 * One way to avoid using g_object_notify() from within the
 * class that registered the properties, and using g_object_notify_by_pspec()
 * instead, is to store the GParamSpec used with
 * g_object_class_install_property() inside a static array, e.g.:
 *
 *|[<!-- language="C" --> 
 *   enum
 *   {
 *     PROP_0,
 *     PROP_FOO,
 *     PROP_LAST
 *   };
 *
 *   static GParamSpec *properties[PROP_LAST];
 *
 *   static void
 *   my_object_class_init (MyObjectClass *klass)
 *   {
 *     properties[PROP_FOO] = g_param_spec_int ("foo", "Foo", "The foo",
 *                                              0, 100,
 *                                              50,
 *                                              G_PARAM_READWRITE);
 *     g_object_class_install_property (gobject_class,
 *                                      PROP_FOO,
 *                                      properties[PROP_FOO]);
 *   }
 * ]|
 *
 * and then notify a change on the "foo" property with:
 *
 * |[<!-- language="C" --> 
 *   g_object_notify_by_pspec (self, properties[PROP_FOO]);
 * ]|
 *
 * Since: 2.26
 */
void
g_object_notify_by_pspec (GObject    *object,
			  GParamSpec *pspec)
{

  g_return_if_fail (G_IS_OBJECT (object));
  g_return_if_fail (G_IS_PARAM_SPEC (pspec));

  if (g_atomic_int_get (&object->ref_count) == 0)
    return;

  g_object_ref (object);
  g_object_notify_by_spec_internal (object, pspec);
  g_object_unref (object);
}

/**
 * g_object_thaw_notify:
 * @object: a #GObject
 *
 * Reverts the effect of a previous call to
 * g_object_freeze_notify(). The freeze count is decreased on @object
 * and when it reaches zero, queued "notify" signals are emitted.
 *
 * Duplicate notifications for each property are squashed so that at most one
 * #GObject::notify signal is emitted for each property, in the reverse order
 * in which they have been queued.
 *
 * It is an error to call this function when the freeze count is zero.
 */
void
g_object_thaw_notify (GObject *object)
{
  GObjectNotifyQueue *nqueue;
  
  g_return_if_fail (G_IS_OBJECT (object));
  if (g_atomic_int_get (&object->ref_count) == 0)
    return;
  
  g_object_ref (object);

  /* FIXME: Freezing is the only way to get at the notify queue.
   * So we freeze once and then thaw twice.
   */
  nqueue = g_object_notify_queue_freeze (object, FALSE);
  g_object_notify_queue_thaw (object, nqueue);
  g_object_notify_queue_thaw (object, nqueue);

  g_object_unref (object);
}

static void
consider_issuing_property_deprecation_warning (const GParamSpec *pspec)
{
  static GHashTable *already_warned_table;
  static const gchar *enable_diagnostic;
  static GMutex already_warned_lock;
  gboolean already;

  if (!(pspec->flags & G_PARAM_DEPRECATED))
    return;

  if (g_once_init_enter (&enable_diagnostic))
    {
      const gchar *value = g_getenv ("G_ENABLE_DIAGNOSTIC");

      if (!value)
        value = "0";

      g_once_init_leave (&enable_diagnostic, value);
    }

  if (enable_diagnostic[0] == '0')
    return;

  /* We hash only on property names: this means that we could end up in
   * a situation where we fail to emit a warning about a pair of
   * same-named deprecated properties used on two separate types.
   * That's pretty unlikely to occur, and even if it does, you'll still
   * have seen the warning for the first one...
   *
   * Doing it this way lets us hash directly on the (interned) property
   * name pointers.
   */
  g_mutex_lock (&already_warned_lock);

  if (already_warned_table == NULL)
    already_warned_table = g_hash_table_new (NULL, NULL);

  already = g_hash_table_contains (already_warned_table, (gpointer) pspec->name);
  if (!already)
    g_hash_table_add (already_warned_table, (gpointer) pspec->name);

  g_mutex_unlock (&already_warned_lock);

  if (!already)
    g_warning ("The property %s:%s is deprecated and shouldn't be used "
               "anymore. It will be removed in a future version.",
               g_type_name (pspec->owner_type), pspec->name);
}

static inline void
object_get_property (GObject     *object,
		     GParamSpec  *pspec,
		     GValue      *value)
{
  GObjectClass *class = g_type_class_peek (pspec->owner_type);
  guint param_id = PARAM_SPEC_PARAM_ID (pspec);
  GParamSpec *redirect;

  if (class == NULL)
    {
      g_warning ("'%s::%s' is not a valid property name; '%s' is not a GObject subtype",
                 g_type_name (pspec->owner_type), pspec->name, g_type_name (pspec->owner_type));
      return;
    }

  redirect = g_param_spec_get_redirect_target (pspec);
  if (redirect)
    pspec = redirect;

  consider_issuing_property_deprecation_warning (pspec);

  class->get_property (object, param_id, value, pspec);
}

static inline void
object_set_property (GObject             *object,
		     GParamSpec          *pspec,
		     const GValue        *value,
		     GObjectNotifyQueue  *nqueue)
{
  GValue tmp_value = G_VALUE_INIT;
  GObjectClass *class = g_type_class_peek (pspec->owner_type);
  guint param_id = PARAM_SPEC_PARAM_ID (pspec);
  GParamSpec *redirect;

  if (class == NULL)
    {
      g_warning ("'%s::%s' is not a valid property name; '%s' is not a GObject subtype",
                 g_type_name (pspec->owner_type), pspec->name, g_type_name (pspec->owner_type));
      return;
    }

  redirect = g_param_spec_get_redirect_target (pspec);
  if (redirect)
    pspec = redirect;

  /* provide a copy to work from, convert (if necessary) and validate */
  g_value_init (&tmp_value, pspec->value_type);
  if (!g_value_transform (value, &tmp_value))
    g_warning ("unable to set property '%s' of type '%s' from value of type '%s'",
	       pspec->name,
	       g_type_name (pspec->value_type),
	       G_VALUE_TYPE_NAME (value));
  else if (g_param_value_validate (pspec, &tmp_value) && !(pspec->flags & G_PARAM_LAX_VALIDATION))
    {
      gchar *contents = g_strdup_value_contents (value);

      g_warning ("value \"%s\" of type '%s' is invalid or out of range for property '%s' of type '%s'",
		 contents,
		 G_VALUE_TYPE_NAME (value),
		 pspec->name,
		 g_type_name (pspec->value_type));
      g_free (contents);
    }
  else
    {
      class->set_property (object, param_id, &tmp_value, pspec);

      if (~pspec->flags & G_PARAM_EXPLICIT_NOTIFY)
        {
          GParamSpec *notify_pspec;

          notify_pspec = get_notify_pspec (pspec);

          if (notify_pspec != NULL)
            g_object_notify_queue_add (object, nqueue, notify_pspec);
        }
    }
  g_value_unset (&tmp_value);
}

static void
object_interface_check_properties (gpointer check_data,
				   gpointer g_iface)
{
  GTypeInterface *iface_class = g_iface;
  GObjectClass *class;
  GType iface_type = iface_class->g_type;
  GParamSpec **pspecs;
  guint n;

  class = g_type_class_ref (iface_class->g_instance_type);

  if (class == NULL)
    return;

  if (!G_IS_OBJECT_CLASS (class))
    goto out;

  pspecs = g_param_spec_pool_list (pspec_pool, iface_type, &n);

  while (n--)
    {
      GParamSpec *class_pspec = g_param_spec_pool_lookup (pspec_pool,
							  pspecs[n]->name,
							  G_OBJECT_CLASS_TYPE (class),
							  TRUE);

      if (!class_pspec)
	{
	  g_critical ("Object class %s doesn't implement property "
		      "'%s' from interface '%s'",
		      g_type_name (G_OBJECT_CLASS_TYPE (class)),
		      pspecs[n]->name,
		      g_type_name (iface_type));

	  continue;
	}

      /* We do a number of checks on the properties of an interface to
       * make sure that all classes implementing the interface are
       * overriding the properties in a sane way.
       *
       * We do the checks in order of importance so that we can give
       * more useful error messages first.
       *
       * First, we check that the implementation doesn't remove the
       * basic functionality (readability, writability) advertised by
       * the interface.  Next, we check that it doesn't introduce
       * additional restrictions (such as construct-only).  Finally, we
       * make sure the types are compatible.
       */

#define SUBSET(a,b,mask) (((a) & ~(b) & (mask)) == 0)
      /* If the property on the interface is readable then the
       * implementation must be readable.  If the interface is writable
       * then the implementation must be writable.
       */
      if (!SUBSET (pspecs[n]->flags, class_pspec->flags, G_PARAM_READABLE | G_PARAM_WRITABLE))
        {
          g_critical ("Flags for property '%s' on class '%s' remove functionality compared with the "
                      "property on interface '%s'\n", pspecs[n]->name,
                      g_type_name (G_OBJECT_CLASS_TYPE (class)), g_type_name (iface_type));
          continue;
        }

      /* If the property on the interface is writable then we need to
       * make sure the implementation doesn't introduce new restrictions
       * on that writability (ie: construct-only).
       *
       * If the interface was not writable to begin with then we don't
       * really have any problems here because "writable at construct
       * time only" is still more permissive than "read only".
       */
      if (pspecs[n]->flags & G_PARAM_WRITABLE)
        {
          if (!SUBSET (class_pspec->flags, pspecs[n]->flags, G_PARAM_CONSTRUCT_ONLY))
            {
              g_critical ("Flags for property '%s' on class '%s' introduce additional restrictions on "
                          "writability compared with the property on interface '%s'\n", pspecs[n]->name,
                          g_type_name (G_OBJECT_CLASS_TYPE (class)), g_type_name (iface_type));
              continue;
            }
        }
#undef SUBSET

      /* If the property on the interface is readable then we are
       * effectively advertising that reading the property will return a
       * value of a specific type.  All implementations of the interface
       * need to return items of this type -- but may be more
       * restrictive.  For example, it is legal to have:
       *
       *   GtkWidget *get_item();
       *
       * that is implemented by a function that always returns a
       * GtkEntry.  In short: readability implies that the
       * implementation  value type must be equal or more restrictive.
       *
       * Similarly, if the property on the interface is writable then
       * must be able to accept the property being set to any value of
       * that type, including subclasses.  In this case, we may also be
       * less restrictive.  For example, it is legal to have:
       *
       *   set_item (GtkEntry *);
       *
       * that is implemented by a function that will actually work with
       * any GtkWidget.  In short: writability implies that the
       * implementation value type must be equal or less restrictive.
       *
       * In the case that the property is both readable and writable
       * then the only way that both of the above can be satisfied is
       * with a type that is exactly equal.
       */
      switch (pspecs[n]->flags & (G_PARAM_READABLE | G_PARAM_WRITABLE))
        {
        case G_PARAM_READABLE | G_PARAM_WRITABLE:
          /* class pspec value type must have exact equality with interface */
          if (pspecs[n]->value_type != class_pspec->value_type)
            g_critical ("Read/writable property '%s' on class '%s' has type '%s' which is not exactly equal to the "
                        "type '%s' of the property on the interface '%s'\n", pspecs[n]->name,
                        g_type_name (G_OBJECT_CLASS_TYPE (class)), g_type_name (G_PARAM_SPEC_VALUE_TYPE (class_pspec)),
                        g_type_name (G_PARAM_SPEC_VALUE_TYPE (pspecs[n])), g_type_name (iface_type));
          break;

        case G_PARAM_READABLE:
          /* class pspec value type equal or more restrictive than interface */
          if (!g_type_is_a (class_pspec->value_type, pspecs[n]->value_type))
            g_critical ("Read-only property '%s' on class '%s' has type '%s' which is not equal to or more "
                        "restrictive than the type '%s' of the property on the interface '%s'\n", pspecs[n]->name,
                        g_type_name (G_OBJECT_CLASS_TYPE (class)), g_type_name (G_PARAM_SPEC_VALUE_TYPE (class_pspec)),
                        g_type_name (G_PARAM_SPEC_VALUE_TYPE (pspecs[n])), g_type_name (iface_type));
          break;

        case G_PARAM_WRITABLE:
          /* class pspec value type equal or less restrictive than interface */
          if (!g_type_is_a (pspecs[n]->value_type, class_pspec->value_type))
            g_critical ("Write-only property '%s' on class '%s' has type '%s' which is not equal to or less "
                        "restrictive than the type '%s' of the property on the interface '%s' \n", pspecs[n]->name,
                        g_type_name (G_OBJECT_CLASS_TYPE (class)), g_type_name (G_PARAM_SPEC_VALUE_TYPE (class_pspec)),
                        g_type_name (G_PARAM_SPEC_VALUE_TYPE (pspecs[n])), g_type_name (iface_type));
          break;

        default:
          g_assert_not_reached ();
        }
    }

  g_free (pspecs);

 out:
  g_type_class_unref (class);
}

GType
g_object_get_type (void)
{
    return G_TYPE_OBJECT;
}

/**
 * g_object_new: (skip)
 * @object_type: the type id of the #GObject subtype to instantiate
 * @first_property_name: the name of the first property
 * @...: the value of the first property, followed optionally by more
 *  name/value pairs, followed by %NULL
 *
 * Creates a new instance of a #GObject subtype and sets its properties.
 *
 * Construction parameters (see #G_PARAM_CONSTRUCT, #G_PARAM_CONSTRUCT_ONLY)
 * which are not explicitly specified are set to their default values.
 *
 * Returns: (transfer full) (type GObject.Object): a new instance of
 *   @object_type
 */
gpointer
g_object_new (GType	   object_type,
	      const gchar *first_property_name,
	      ...)
{
  GObject *object;
  va_list var_args;
  
  /* short circuit for calls supplying no properties */
  if (!first_property_name)
    return g_object_new_with_properties (object_type, 0, NULL, NULL);

  va_start (var_args, first_property_name);
  object = g_object_new_valist (object_type, first_property_name, var_args);
  va_end (var_args);
  
  return object;
}

static gpointer
g_object_new_with_custom_constructor (GObjectClass          *class,
                                      GObjectConstructParam *params,
                                      guint                  n_params)
{
  GObjectNotifyQueue *nqueue = NULL;
  gboolean newly_constructed;
  GObjectConstructParam *cparams;
  GObject *object;
  GValue *cvalues;
  gint n_cparams;
  gint cvals_used;
  GSList *node;
  gint i;

  /* If we have ->constructed() then we have to do a lot more work.
   * It's possible that this is a singleton and it's also possible
   * that the user's constructor() will attempt to modify the values
   * that we pass in, so we'll need to allocate copies of them.
   * It's also possible that the user may attempt to call
   * g_object_set() from inside of their constructor, so we need to
   * add ourselves to a list of objects for which that is allowed
   * while their constructor() is running.
   */

  /* Create the array of GObjectConstructParams for constructor() */
  n_cparams = g_slist_length (class->construct_properties);
  cparams = g_new (GObjectConstructParam, n_cparams);
  cvalues = g_new0 (GValue, n_cparams);
  cvals_used = 0;
  i = 0;

  /* As above, we may find the value in the passed-in params list.
   *
   * If we have the value passed in then we can use the GValue from
   * it directly because it is safe to modify.  If we use the
   * default value from the class, we had better not pass that in
   * and risk it being modified, so we create a new one.
   * */
  for (node = class->construct_properties; node; node = node->next)
    {
      GParamSpec *pspec;
      GValue *value;
      gint j;

      pspec = node->data;
      value = NULL; /* to silence gcc... */

      for (j = 0; j < n_params; j++)
        if (params[j].pspec == pspec)
          {
            consider_issuing_property_deprecation_warning (pspec);
            value = params[j].value;
            break;
          }

      if (j == n_params)
        {
          value = &cvalues[cvals_used++];
          g_value_init (value, pspec->value_type);
          g_param_value_set_default (pspec, value);
        }

      cparams[i].pspec = pspec;
      cparams[i].value = value;
      i++;
    }

  /* construct object from construction parameters */
  object = class->constructor (class->g_type_class.g_type, n_cparams, cparams);
  /* free construction values */
  g_free (cparams);
  while (cvals_used--)
    g_value_unset (&cvalues[cvals_used]);
  g_free (cvalues);

  /* There is code in the wild that relies on being able to return NULL
   * from its custom constructor.  This was never a supported operation,
   * but since the code is already out there...
   */
  if (object == NULL)
    {
      g_critical ("Custom constructor for class %s returned NULL (which is invalid). "
                  "Please use GInitable instead.", G_OBJECT_CLASS_NAME (class));
      return NULL;
    }

  /* g_object_init() will have marked the object as being in-construction.
   * Check if the returned object still is so marked, or if this is an
   * already-existing singleton (in which case we should not do 'constructed').
   */
  newly_constructed = object_in_construction (object);
  if (newly_constructed)
    g_datalist_id_set_data (&object->qdata, quark_in_construction, NULL);

  if (CLASS_HAS_PROPS (class))
    {
      /* If this object was newly_constructed then g_object_init()
       * froze the queue.  We need to freeze it here in order to get
       * the handle so that we can thaw it below (otherwise it will
       * be frozen forever).
       *
       * We also want to do a freeze if we have any params to set,
       * even on a non-newly_constructed object.
       *
       * It's possible that we have the case of non-newly created
       * singleton and all of the passed-in params were construct
       * properties so n_params > 0 but we will actually set no
       * properties.  This is a pretty lame case to optimise, so
       * just ignore it and freeze anyway.
       */
      if (newly_constructed || n_params)
        nqueue = g_object_notify_queue_freeze (object, FALSE);

      /* Remember: if it was newly_constructed then g_object_init()
       * already did a freeze, so we now have two.  Release one.
       */
      if (newly_constructed)
        g_object_notify_queue_thaw (object, nqueue);
    }

  /* run 'constructed' handler if there is a custom one */
  if (newly_constructed && CLASS_HAS_CUSTOM_CONSTRUCTED (class))
    class->constructed (object);

  /* set remaining properties */
  for (i = 0; i < n_params; i++)
    if (!(params[i].pspec->flags & (G_PARAM_CONSTRUCT | G_PARAM_CONSTRUCT_ONLY)))
      {
        consider_issuing_property_deprecation_warning (params[i].pspec);
        object_set_property (object, params[i].pspec, params[i].value, nqueue);
      }

  /* If nqueue is non-NULL then we are frozen.  Thaw it. */
  if (nqueue)
    g_object_notify_queue_thaw (object, nqueue);

  return object;
}

static gpointer
g_object_new_internal (GObjectClass          *class,
                       GObjectConstructParam *params,
                       guint                  n_params)
{
  GObjectNotifyQueue *nqueue = NULL;
  GObject *object;

  if G_UNLIKELY (CLASS_HAS_CUSTOM_CONSTRUCTOR (class))
    return g_object_new_with_custom_constructor (class, params, n_params);

  object = (GObject *) g_type_create_instance (class->g_type_class.g_type);

  if (CLASS_HAS_PROPS (class))
    {
      GSList *node;

      /* This will have been setup in g_object_init() */
      nqueue = g_datalist_id_get_data (&object->qdata, quark_notify_queue);
      g_assert (nqueue != NULL);

      /* We will set exactly n_construct_properties construct
       * properties, but they may come from either the class default
       * values or the passed-in parameter list.
       */
      for (node = class->construct_properties; node; node = node->next)
        {
          const GValue *value;
          GParamSpec *pspec;
          gint j;

          pspec = node->data;
          value = NULL; /* to silence gcc... */

          for (j = 0; j < n_params; j++)
            if (params[j].pspec == pspec)
              {
                consider_issuing_property_deprecation_warning (pspec);
                value = params[j].value;
                break;
              }

          if (j == n_params)
            value = g_param_spec_get_default_value (pspec);

          object_set_property (object, pspec, value, nqueue);
        }
    }

  /* run 'constructed' handler if there is a custom one */
  if (CLASS_HAS_CUSTOM_CONSTRUCTED (class))
    class->constructed (object);

  if (nqueue)
    {
      gint i;

      /* Set remaining properties.  The construct properties will
       * already have been taken, so set only the non-construct
       * ones.
       */
      for (i = 0; i < n_params; i++)
        if (!(params[i].pspec->flags & (G_PARAM_CONSTRUCT | G_PARAM_CONSTRUCT_ONLY)))
          {
            consider_issuing_property_deprecation_warning (params[i].pspec);
            object_set_property (object, params[i].pspec, params[i].value, nqueue);
          }

      g_object_notify_queue_thaw (object, nqueue);
    }

  return object;
}


static inline gboolean
g_object_new_is_valid_property (GType                  object_type,
                                GParamSpec            *pspec,
                                const char            *name,
                                GObjectConstructParam *params,
                                int                    n_params)
{
  gint i;
  if (G_UNLIKELY (pspec == NULL))
    {
      g_critical ("%s: object class '%s' has no property named '%s'",
                  G_STRFUNC, g_type_name (object_type), name);
      return FALSE;
    }

  if (G_UNLIKELY (~pspec->flags & G_PARAM_WRITABLE))
    {
      g_critical ("%s: property '%s' of object class '%s' is not writable",
                  G_STRFUNC, pspec->name, g_type_name (object_type));
      return FALSE;
    }

  if (G_UNLIKELY (pspec->flags & (G_PARAM_CONSTRUCT | G_PARAM_CONSTRUCT_ONLY)))
    {
      for (i = 0; i < n_params; i++)
        if (params[i].pspec == pspec)
          break;
      if (G_UNLIKELY (i != n_params))
        {
          g_critical ("%s: property '%s' for type '%s' cannot be set twice",
                      G_STRFUNC, name, g_type_name (object_type));
          return FALSE;
        }
    }
  return TRUE;
}


/**
 * g_object_new_with_properties: (rename-to g_object_new)
 * @object_type: the object type to instantiate
 * @n_properties: the number of properties
 * @names: (array length=n_properties): the names of each property to be set
 * @values: (array length=n_properties): the values of each property to be set
 *
 * Creates a new instance of a #GObject subtype and sets its properties using
 * the provided arrays. Both arrays must have exactly @n_properties elements,
 * and the names and values correspond by index.
 *
 * Construction parameters (see %G_PARAM_CONSTRUCT, %G_PARAM_CONSTRUCT_ONLY)
 * which are not explicitly specified are set to their default values.
 *
 * Returns: (type GObject.Object) (transfer full): a new instance of
 * @object_type
 *
 * Since: 2.54
 */
GObject *
g_object_new_with_properties (GType          object_type,
                              guint          n_properties,
                              const char    *names[],
                              const GValue   values[])
{
  GObjectClass *class, *unref_class = NULL;
  GObject *object;

  g_return_val_if_fail (G_TYPE_IS_OBJECT (object_type), NULL);

  /* Try to avoid thrashing the ref_count if we don't need to (since
   * it's a locked operation).
   */
  class = g_type_class_peek_static (object_type);

  if (class == NULL)
    class = unref_class = g_type_class_ref (object_type);

  if (n_properties > 0)
    {
      guint i, count = 0;
      GObjectConstructParam *params;

      params = g_newa (GObjectConstructParam, n_properties);
      for (i = 0; i < n_properties; i++)
        {
          GParamSpec *pspec;
          pspec = g_param_spec_pool_lookup (pspec_pool, names[i], object_type, TRUE);
          if (!g_object_new_is_valid_property (object_type, pspec, names[i], params, count))
            continue;
          params[count].pspec = pspec;

          /* Init GValue */
          params[count].value = g_newa (GValue, 1);
          memset (params[count].value, 0, sizeof (GValue));
          g_value_init (params[count].value, G_VALUE_TYPE (&values[i]));

          g_value_copy (&values[i], params[count].value);
          count++;
        }
      object = g_object_new_internal (class, params, count);

      while (count--)
        g_value_unset (params[count].value);
    }
  else
    object = g_object_new_internal (class, NULL, 0);

  if (unref_class != NULL)
    g_type_class_unref (unref_class);

  return object;
}

/**
 * g_object_newv:
 * @object_type: the type id of the #GObject subtype to instantiate
 * @n_parameters: the length of the @parameters array
 * @parameters: (array length=n_parameters): an array of #GParameter
 *
 * Creates a new instance of a #GObject subtype and sets its properties.
 *
 * Construction parameters (see #G_PARAM_CONSTRUCT, #G_PARAM_CONSTRUCT_ONLY)
 * which are not explicitly specified are set to their default values.
 *
 * Returns: (type GObject.Object) (transfer full): a new instance of
 * @object_type
 *
 * Deprecated: 2.54: Use g_object_new_with_properties() instead.
 * deprecated. See #GParameter for more information.
 */
gpointer
g_object_newv (GType       object_type,
               guint       n_parameters,
               GParameter *parameters)
{
  GObjectClass *class, *unref_class = NULL;
  GObject *object;

  g_return_val_if_fail (G_TYPE_IS_OBJECT (object_type), NULL);
  g_return_val_if_fail (n_parameters == 0 || parameters != NULL, NULL);

  /* Try to avoid thrashing the ref_count if we don't need to (since
   * it's a locked operation).
   */
  class = g_type_class_peek_static (object_type);

  if (!class)
    class = unref_class = g_type_class_ref (object_type);

  if (n_parameters)
    {
      GObjectConstructParam *cparams;
      guint i, j;

      cparams = g_newa (GObjectConstructParam, n_parameters);
      j = 0;

      for (i = 0; i < n_parameters; i++)
        {
          GParamSpec *pspec;

          pspec = g_param_spec_pool_lookup (pspec_pool, parameters[i].name, object_type, TRUE);
          if (!g_object_new_is_valid_property (object_type, pspec, parameters[i].name, cparams, j))
            continue;

          cparams[j].pspec = pspec;
          cparams[j].value = &parameters[i].value;
          j++;
        }

      object = g_object_new_internal (class, cparams, j);
    }
  else
    /* Fast case: no properties passed in. */
    object = g_object_new_internal (class, NULL, 0);

  if (unref_class)
    g_type_class_unref (unref_class);

  return object;
}

/**
 * g_object_new_valist: (skip)
 * @object_type: the type id of the #GObject subtype to instantiate
 * @first_property_name: the name of the first property
 * @var_args: the value of the first property, followed optionally by more
 *  name/value pairs, followed by %NULL
 *
 * Creates a new instance of a #GObject subtype and sets its properties.
 *
 * Construction parameters (see #G_PARAM_CONSTRUCT, #G_PARAM_CONSTRUCT_ONLY)
 * which are not explicitly specified are set to their default values.
 *
 * Returns: a new instance of @object_type
 */
GObject*
g_object_new_valist (GType        object_type,
                     const gchar *first_property_name,
                     va_list      var_args)
{
  GObjectClass *class, *unref_class = NULL;
  GObject *object;

  g_return_val_if_fail (G_TYPE_IS_OBJECT (object_type), NULL);

  /* Try to avoid thrashing the ref_count if we don't need to (since
   * it's a locked operation).
   */
  class = g_type_class_peek_static (object_type);

  if (!class)
    class = unref_class = g_type_class_ref (object_type);

  if (first_property_name)
    {
      GObjectConstructParam stack_params[16];
      GObjectConstructParam *params;
      const gchar *name;
      gint n_params = 0;

      name = first_property_name;
      params = stack_params;

      do
        {
          gchar *error = NULL;
          GParamSpec *pspec;

          pspec = g_param_spec_pool_lookup (pspec_pool, name, object_type, TRUE);

          if (!g_object_new_is_valid_property (object_type, pspec, name, params, n_params))
            break;

          if (n_params == 16)
            {
              params = g_new (GObjectConstructParam, n_params + 1);
              memcpy (params, stack_params, sizeof stack_params);
            }
          else if (n_params > 16)
            params = g_renew (GObjectConstructParam, params, n_params + 1);

          params[n_params].pspec = pspec;
          params[n_params].value = g_newa (GValue, 1);
          memset (params[n_params].value, 0, sizeof (GValue));

          G_VALUE_COLLECT_INIT (params[n_params].value, pspec->value_type, var_args, 0, &error);

          if (error)
            {
              g_critical ("%s: %s", G_STRFUNC, error);
              g_value_unset (params[n_params].value);
              g_free (error);
              break;
            }

          n_params++;
        }
      while ((name = va_arg (var_args, const gchar *)));

      object = g_object_new_internal (class, params, n_params);

      while (n_params--)
        g_value_unset (params[n_params].value);

      if (params != stack_params)
        g_free (params);
    }
  else
    /* Fast case: no properties passed in. */
    object = g_object_new_internal (class, NULL, 0);

  if (unref_class)
    g_type_class_unref (unref_class);

  return object;
}

static GObject*
g_object_constructor (GType                  type,
		      guint                  n_construct_properties,
		      GObjectConstructParam *construct_params)
{
  GObject *object;

  /* create object */
  object = (GObject*) g_type_create_instance (type);
  
  /* set construction parameters */
  if (n_construct_properties)
    {
      GObjectNotifyQueue *nqueue = g_object_notify_queue_freeze (object, FALSE);
      
      /* set construct properties */
      while (n_construct_properties--)
	{
	  GValue *value = construct_params->value;
	  GParamSpec *pspec = construct_params->pspec;

	  construct_params++;
	  object_set_property (object, pspec, value, nqueue);
	}
      g_object_notify_queue_thaw (object, nqueue);
      /* the notification queue is still frozen from g_object_init(), so
       * we don't need to handle it here, g_object_newv() takes
       * care of that
       */
    }

  return object;
}

static void
g_object_constructed (GObject *object)
{
  /* empty default impl to allow unconditional upchaining */
}

static inline gboolean
g_object_set_is_valid_property (GObject         *object,
                                GParamSpec      *pspec,
                                const char      *property_name)
{
  if (G_UNLIKELY (pspec == NULL))
    {
      g_warning ("%s: object class '%s' has no property named '%s'",
                 G_STRFUNC, G_OBJECT_TYPE_NAME (object), property_name);
      return FALSE;
    }
  if (G_UNLIKELY (!(pspec->flags & G_PARAM_WRITABLE)))
    {
      g_warning ("%s: property '%s' of object class '%s' is not writable",
                 G_STRFUNC, pspec->name, G_OBJECT_TYPE_NAME (object));
      return FALSE;
    }
  if (G_UNLIKELY (((pspec->flags & G_PARAM_CONSTRUCT_ONLY) && !object_in_construction (object))))
    {
      g_warning ("%s: construct property \"%s\" for object '%s' can't be set after construction",
                 G_STRFUNC, pspec->name, G_OBJECT_TYPE_NAME (object));
      return FALSE;
    }
  return TRUE;
}

/**
 * g_object_setv: (skip)
 * @object: a #GObject
 * @n_properties: the number of properties
 * @names: (array length=n_properties): the names of each property to be set
 * @values: (array length=n_properties): the values of each property to be set
 *
 * Sets @n_properties properties for an @object.
 * Properties to be set will be taken from @values. All properties must be
 * valid. Warnings will be emitted and undefined behaviour may result if invalid
 * properties are passed in.
 *
 * Since: 2.54
 */
void
g_object_setv (GObject       *object,
               guint          n_properties,
               const gchar   *names[],
               const GValue   values[])
{
  guint i;
  GObjectNotifyQueue *nqueue;
  GParamSpec *pspec;
  GType obj_type;

  g_return_if_fail (G_IS_OBJECT (object));

  if (n_properties == 0)
    return;

  g_object_ref (object);
  obj_type = G_OBJECT_TYPE (object);
  nqueue = g_object_notify_queue_freeze (object, FALSE);
  for (i = 0; i < n_properties; i++)
    {
      pspec = g_param_spec_pool_lookup (pspec_pool, names[i], obj_type, TRUE);

      if (!g_object_set_is_valid_property (object, pspec, names[i]))
        break;

      consider_issuing_property_deprecation_warning (pspec);
      object_set_property (object, pspec, &values[i], nqueue);
    }

  g_object_notify_queue_thaw (object, nqueue);
  g_object_unref (object);
}

/**
 * g_object_set_valist: (skip)
 * @object: a #GObject
 * @first_property_name: name of the first property to set
 * @var_args: value for the first property, followed optionally by more
 *  name/value pairs, followed by %NULL
 *
 * Sets properties on an object.
 */
void
g_object_set_valist (GObject	 *object,
		     const gchar *first_property_name,
		     va_list	  var_args)
{
  GObjectNotifyQueue *nqueue;
  const gchar *name;
  
  g_return_if_fail (G_IS_OBJECT (object));
  
  g_object_ref (object);
  nqueue = g_object_notify_queue_freeze (object, FALSE);
  
  name = first_property_name;
  while (name)
    {
      GValue value = G_VALUE_INIT;
      GParamSpec *pspec;
      gchar *error = NULL;
      
      pspec = g_param_spec_pool_lookup (pspec_pool,
					name,
					G_OBJECT_TYPE (object),
					TRUE);

      if (!g_object_set_is_valid_property (object, pspec, name))
        break;

      G_VALUE_COLLECT_INIT (&value, pspec->value_type, var_args,
			    0, &error);
      if (error)
	{
	  g_warning ("%s: %s", G_STRFUNC, error);
	  g_free (error);
          g_value_unset (&value);
	  break;
	}

      consider_issuing_property_deprecation_warning (pspec);
      object_set_property (object, pspec, &value, nqueue);
      g_value_unset (&value);

      name = va_arg (var_args, gchar*);
    }

  g_object_notify_queue_thaw (object, nqueue);
  g_object_unref (object);
}

static inline gboolean
g_object_get_is_valid_property (GObject          *object,
                                GParamSpec       *pspec,
                                const char       *property_name)
{
  if (G_UNLIKELY (pspec == NULL))
    {
      g_warning ("%s: object class '%s' has no property named '%s'",
                 G_STRFUNC, G_OBJECT_TYPE_NAME (object), property_name);
      return FALSE;
    }
  if (G_UNLIKELY (!(pspec->flags & G_PARAM_READABLE)))
    {
      g_warning ("%s: property '%s' of object class '%s' is not readable",
                 G_STRFUNC, pspec->name, G_OBJECT_TYPE_NAME (object));
      return FALSE;
    }
  return TRUE;
}

/**
 * g_object_getv:
 * @object: a #GObject
 * @n_properties: the number of properties
 * @names: (array length=n_properties): the names of each property to get
 * @values: (array length=n_properties): the values of each property to get
 *
 * Gets @n_properties properties for an @object.
 * Obtained properties will be set to @values. All properties must be valid.
 * Warnings will be emitted and undefined behaviour may result if invalid
 * properties are passed in.
 *
 * Since: 2.54
 */
void
g_object_getv (GObject      *object,
               guint         n_properties,
               const gchar  *names[],
               GValue        values[])
{
  guint i;
  GParamSpec *pspec;
  GType obj_type;

  g_return_if_fail (G_IS_OBJECT (object));

  if (n_properties == 0)
    return;

  g_object_ref (object);

  obj_type = G_OBJECT_TYPE (object);
  for (i = 0; i < n_properties; i++)
    {
      pspec = g_param_spec_pool_lookup (pspec_pool,
				        names[i],
				        obj_type,
				        TRUE);
      if (!g_object_get_is_valid_property (object, pspec, names[i]))
        break;

      memset (&values[i], 0, sizeof (GValue));
      g_value_init (&values[i], pspec->value_type);
      object_get_property (object, pspec, &values[i]);
    }
  g_object_unref (object);
}

/**
 * g_object_get_valist: (skip)
 * @object: a #GObject
 * @first_property_name: name of the first property to get
 * @var_args: return location for the first property, followed optionally by more
 *  name/return location pairs, followed by %NULL
 *
 * Gets properties of an object.
 *
 * In general, a copy is made of the property contents and the caller
 * is responsible for freeing the memory in the appropriate manner for
 * the type, for instance by calling g_free() or g_object_unref().
 *
 * See g_object_get().
 */
void
g_object_get_valist (GObject	 *object,
		     const gchar *first_property_name,
		     va_list	  var_args)
{
  const gchar *name;
  
  g_return_if_fail (G_IS_OBJECT (object));
  
  g_object_ref (object);
  
  name = first_property_name;
  
  while (name)
    {
      GValue value = G_VALUE_INIT;
      GParamSpec *pspec;
      gchar *error;
      
      pspec = g_param_spec_pool_lookup (pspec_pool,
					name,
					G_OBJECT_TYPE (object),
					TRUE);

      if (!g_object_get_is_valid_property (object, pspec, name))
        break;
      
      g_value_init (&value, pspec->value_type);
      
      object_get_property (object, pspec, &value);
      
      G_VALUE_LCOPY (&value, var_args, 0, &error);
      if (error)
	{
	  g_warning ("%s: %s", G_STRFUNC, error);
	  g_free (error);
	  g_value_unset (&value);
	  break;
	}
      
      g_value_unset (&value);
      
      name = va_arg (var_args, gchar*);
    }
  
  g_object_unref (object);
}

/**
 * g_object_set: (skip)
 * @object: (type GObject.Object): a #GObject
 * @first_property_name: name of the first property to set
 * @...: value for the first property, followed optionally by more
 *  name/value pairs, followed by %NULL
 *
 * Sets properties on an object.
 *
 * Note that the "notify" signals are queued and only emitted (in
 * reverse order) after all properties have been set. See
 * g_object_freeze_notify().
 */
void
g_object_set (gpointer     _object,
	      const gchar *first_property_name,
	      ...)
{
  GObject *object = _object;
  va_list var_args;
  
  g_return_if_fail (G_IS_OBJECT (object));
  
  va_start (var_args, first_property_name);
  g_object_set_valist (object, first_property_name, var_args);
  va_end (var_args);
}

/**
 * g_object_get: (skip)
 * @object: (type GObject.Object): a #GObject
 * @first_property_name: name of the first property to get
 * @...: return location for the first property, followed optionally by more
 *  name/return location pairs, followed by %NULL
 *
 * Gets properties of an object.
 *
 * In general, a copy is made of the property contents and the caller
 * is responsible for freeing the memory in the appropriate manner for
 * the type, for instance by calling g_free() or g_object_unref().
 *
 * Here is an example of using g_object_get() to get the contents
 * of three properties: an integer, a string and an object:
 * |[<!-- language="C" --> 
 *  gint intval;
 *  gchar *strval;
 *  GObject *objval;
 *
 *  g_object_get (my_object,
 *                "int-property", &intval,
 *                "str-property", &strval,
 *                "obj-property", &objval,
 *                NULL);
 *
 *  // Do something with intval, strval, objval
 *
 *  g_free (strval);
 *  g_object_unref (objval);
 *  ]|
 */
void
g_object_get (gpointer     _object,
	      const gchar *first_property_name,
	      ...)
{
  GObject *object = _object;
  va_list var_args;
  
  g_return_if_fail (G_IS_OBJECT (object));
  
  va_start (var_args, first_property_name);
  g_object_get_valist (object, first_property_name, var_args);
  va_end (var_args);
}

/**
 * g_object_set_property:
 * @object: a #GObject
 * @property_name: the name of the property to set
 * @value: the value
 *
 * Sets a property on an object.
 */
void
g_object_set_property (GObject	    *object,
		       const gchar  *property_name,
		       const GValue *value)
{
  g_object_setv (object, 1, &property_name, value);
}

/**
 * g_object_get_property:
 * @object: a #GObject
 * @property_name: the name of the property to get
 * @value: return location for the property value
 *
 * Gets a property of an object. @value must have been initialized to the
 * expected type of the property (or a type to which the expected type can be
 * transformed) using g_value_init().
 *
 * In general, a copy is made of the property contents and the caller is
 * responsible for freeing the memory by calling g_value_unset().
 *
 * Note that g_object_get_property() is really intended for language
 * bindings, g_object_get() is much more convenient for C programming.
 */
void
g_object_get_property (GObject	   *object,
		       const gchar *property_name,
		       GValue	   *value)
{
  GParamSpec *pspec;
  
  g_return_if_fail (G_IS_OBJECT (object));
  g_return_if_fail (property_name != NULL);
  g_return_if_fail (G_IS_VALUE (value));
  
  g_object_ref (object);
  
  pspec = g_param_spec_pool_lookup (pspec_pool,
				    property_name,
				    G_OBJECT_TYPE (object),
				    TRUE);

  if (g_object_get_is_valid_property (object, pspec, property_name))
    {
      GValue *prop_value, tmp_value = G_VALUE_INIT;
      
      /* auto-conversion of the callers value type
       */
      if (G_VALUE_TYPE (value) == pspec->value_type)
	{
	  g_value_reset (value);
	  prop_value = value;
	}
      else if (!g_value_type_transformable (pspec->value_type, G_VALUE_TYPE (value)))
	{
	  g_warning ("%s: can't retrieve property '%s' of type '%s' as value of type '%s'",
		     G_STRFUNC, pspec->name,
		     g_type_name (pspec->value_type),
		     G_VALUE_TYPE_NAME (value));
	  g_object_unref (object);
	  return;
	}
      else
	{
	  g_value_init (&tmp_value, pspec->value_type);
	  prop_value = &tmp_value;
	}
      object_get_property (object, pspec, prop_value);
      if (prop_value != value)
	{
	  g_value_transform (prop_value, value);
	  g_value_unset (&tmp_value);
	}
    }
  
  g_object_unref (object);
}

/**
 * g_object_connect: (skip)
 * @object: (type GObject.Object): a #GObject
 * @signal_spec: the spec for the first signal
 * @...: #GCallback for the first signal, followed by data for the
 *       first signal, followed optionally by more signal
 *       spec/callback/data triples, followed by %NULL
 *
 * A convenience function to connect multiple signals at once.
 *
 * The signal specs expected by this function have the form
 * "modifier::signal_name", where modifier can be one of the following:
 * * - signal: equivalent to g_signal_connect_data (..., NULL, 0)
 * - object-signal, object_signal: equivalent to g_signal_connect_object (..., 0)
 * - swapped-signal, swapped_signal: equivalent to g_signal_connect_data (..., NULL, G_CONNECT_SWAPPED)
 * - swapped_object_signal, swapped-object-signal: equivalent to g_signal_connect_object (..., G_CONNECT_SWAPPED)
 * - signal_after, signal-after: equivalent to g_signal_connect_data (..., NULL, G_CONNECT_AFTER)
 * - object_signal_after, object-signal-after: equivalent to g_signal_connect_object (..., G_CONNECT_AFTER)
 * - swapped_signal_after, swapped-signal-after: equivalent to g_signal_connect_data (..., NULL, G_CONNECT_SWAPPED | G_CONNECT_AFTER)
 * - swapped_object_signal_after, swapped-object-signal-after: equivalent to g_signal_connect_object (..., G_CONNECT_SWAPPED | G_CONNECT_AFTER)
 *
 * |[<!-- language="C" --> 
 *   menu->toplevel = g_object_connect (g_object_new (GTK_TYPE_WINDOW,
 * 						   "type", GTK_WINDOW_POPUP,
 * 						   "child", menu,
 * 						   NULL),
 * 				     "signal::event", gtk_menu_window_event, menu,
 * 				     "signal::size_request", gtk_menu_window_size_request, menu,
 * 				     "signal::destroy", gtk_widget_destroyed, &menu->toplevel,
 * 				     NULL);
 * ]|
 *
 * Returns: (transfer none) (type GObject.Object): @object
 */
gpointer
g_object_connect (gpointer     _object,
		  const gchar *signal_spec,
		  ...)
{
  GObject *object = _object;
  va_list var_args;

  g_return_val_if_fail (G_IS_OBJECT (object), NULL);
  g_return_val_if_fail (object->ref_count > 0, object);

  va_start (var_args, signal_spec);
  while (signal_spec)
    {
      GCallback callback = va_arg (var_args, GCallback);
      gpointer data = va_arg (var_args, gpointer);

      if (strncmp (signal_spec, "signal::", 8) == 0)
	g_signal_connect_data (object, signal_spec + 8,
			       callback, data, NULL,
			       0);
      else if (strncmp (signal_spec, "object_signal::", 15) == 0 ||
               strncmp (signal_spec, "object-signal::", 15) == 0)
	g_signal_connect_object (object, signal_spec + 15,
				 callback, data,
				 0);
      else if (strncmp (signal_spec, "swapped_signal::", 16) == 0 ||
               strncmp (signal_spec, "swapped-signal::", 16) == 0)
	g_signal_connect_data (object, signal_spec + 16,
			       callback, data, NULL,
			       G_CONNECT_SWAPPED);
      else if (strncmp (signal_spec, "swapped_object_signal::", 23) == 0 ||
               strncmp (signal_spec, "swapped-object-signal::", 23) == 0)
	g_signal_connect_object (object, signal_spec + 23,
				 callback, data,
				 G_CONNECT_SWAPPED);
      else if (strncmp (signal_spec, "signal_after::", 14) == 0 ||
               strncmp (signal_spec, "signal-after::", 14) == 0)
	g_signal_connect_data (object, signal_spec + 14,
			       callback, data, NULL,
			       G_CONNECT_AFTER);
      else if (strncmp (signal_spec, "object_signal_after::", 21) == 0 ||
               strncmp (signal_spec, "object-signal-after::", 21) == 0)
	g_signal_connect_object (object, signal_spec + 21,
				 callback, data,
				 G_CONNECT_AFTER);
      else if (strncmp (signal_spec, "swapped_signal_after::", 22) == 0 ||
               strncmp (signal_spec, "swapped-signal-after::", 22) == 0)
	g_signal_connect_data (object, signal_spec + 22,
			       callback, data, NULL,
			       G_CONNECT_SWAPPED | G_CONNECT_AFTER);
      else if (strncmp (signal_spec, "swapped_object_signal_after::", 29) == 0 ||
               strncmp (signal_spec, "swapped-object-signal-after::", 29) == 0)
	g_signal_connect_object (object, signal_spec + 29,
				 callback, data,
				 G_CONNECT_SWAPPED | G_CONNECT_AFTER);
      else
	{
	  g_warning ("%s: invalid signal spec \"%s\"", G_STRFUNC, signal_spec);
	  break;
	}
      signal_spec = va_arg (var_args, gchar*);
    }
  va_end (var_args);

  return object;
}

/**
 * g_object_disconnect: (skip)
 * @object: (type GObject.Object): a #GObject
 * @signal_spec: the spec for the first signal
 * @...: #GCallback for the first signal, followed by data for the first signal,
 *  followed optionally by more signal spec/callback/data triples,
 *  followed by %NULL
 *
 * A convenience function to disconnect multiple signals at once.
 *
 * The signal specs expected by this function have the form
 * "any_signal", which means to disconnect any signal with matching
 * callback and data, or "any_signal::signal_name", which only
 * disconnects the signal named "signal_name".
 */
void
g_object_disconnect (gpointer     _object,
		     const gchar *signal_spec,
		     ...)
{
  GObject *object = _object;
  va_list var_args;

  g_return_if_fail (G_IS_OBJECT (object));
  g_return_if_fail (object->ref_count > 0);

  va_start (var_args, signal_spec);
  while (signal_spec)
    {
      GCallback callback = va_arg (var_args, GCallback);
      gpointer data = va_arg (var_args, gpointer);
      guint sid = 0, detail = 0, mask = 0;

      if (strncmp (signal_spec, "any_signal::", 12) == 0 ||
          strncmp (signal_spec, "any-signal::", 12) == 0)
	{
	  signal_spec += 12;
	  mask = G_SIGNAL_MATCH_ID | G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA;
	}
      else if (strcmp (signal_spec, "any_signal") == 0 ||
               strcmp (signal_spec, "any-signal") == 0)
	{
	  signal_spec += 10;
	  mask = G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA;
	}
      else
	{
	  g_warning ("%s: invalid signal spec \"%s\"", G_STRFUNC, signal_spec);
	  break;
	}

      if ((mask & G_SIGNAL_MATCH_ID) &&
	  !g_signal_parse_name (signal_spec, G_OBJECT_TYPE (object), &sid, &detail, FALSE))
	g_warning ("%s: invalid signal name \"%s\"", G_STRFUNC, signal_spec);
      else if (!g_signal_handlers_disconnect_matched (object, mask | (detail ? G_SIGNAL_MATCH_DETAIL : 0),
						      sid, detail,
						      NULL, (gpointer)callback, data))
	g_warning ("%s: signal handler %p(%p) is not connected", G_STRFUNC, callback, data);
      signal_spec = va_arg (var_args, gchar*);
    }
  va_end (var_args);
}

typedef struct {
  GObject *object;
  guint n_weak_refs;
  struct {
    GWeakNotify notify;
    gpointer    data;
  } weak_refs[1];  /* flexible array */
} WeakRefStack;

static void
weak_refs_notify (gpointer data)
{
  WeakRefStack *wstack = data;
  guint i;

  for (i = 0; i < wstack->n_weak_refs; i++)
    wstack->weak_refs[i].notify (wstack->weak_refs[i].data, wstack->object);
  g_free (wstack);
}

/**
 * g_object_weak_ref: (skip)
 * @object: #GObject to reference weakly
 * @notify: callback to invoke before the object is freed
 * @data: extra data to pass to notify
 *
 * Adds a weak reference callback to an object. Weak references are
 * used for notification when an object is finalized. They are called
 * "weak references" because they allow you to safely hold a pointer
 * to an object without calling g_object_ref() (g_object_ref() adds a
 * strong reference, that is, forces the object to stay alive).
 *
 * Note that the weak references created by this method are not
 * thread-safe: they cannot safely be used in one thread if the
 * object's last g_object_unref() might happen in another thread.
 * Use #GWeakRef if thread-safety is required.
 */
void
g_object_weak_ref (GObject    *object,
		   GWeakNotify notify,
		   gpointer    data)
{
  WeakRefStack *wstack;
  guint i;
  
  g_return_if_fail (G_IS_OBJECT (object));
  g_return_if_fail (notify != NULL);
  g_return_if_fail (object->ref_count >= 1);

  G_LOCK (weak_refs_mutex);
  wstack = g_datalist_id_remove_no_notify (&object->qdata, quark_weak_refs);
  if (wstack)
    {
      i = wstack->n_weak_refs++;
      wstack = g_realloc (wstack, sizeof (*wstack) + sizeof (wstack->weak_refs[0]) * i);
    }
  else
    {
      wstack = g_renew (WeakRefStack, NULL, 1);
      wstack->object = object;
      wstack->n_weak_refs = 1;
      i = 0;
    }
  wstack->weak_refs[i].notify = notify;
  wstack->weak_refs[i].data = data;
  g_datalist_id_set_data_full (&object->qdata, quark_weak_refs, wstack, weak_refs_notify);
  G_UNLOCK (weak_refs_mutex);
}

/**
 * g_object_weak_unref: (skip)
 * @object: #GObject to remove a weak reference from
 * @notify: callback to search for
 * @data: data to search for
 *
 * Removes a weak reference callback to an object.
 */
void
g_object_weak_unref (GObject    *object,
		     GWeakNotify notify,
		     gpointer    data)
{
  WeakRefStack *wstack;
  gboolean found_one = FALSE;

  g_return_if_fail (G_IS_OBJECT (object));
  g_return_if_fail (notify != NULL);

  G_LOCK (weak_refs_mutex);
  wstack = g_datalist_id_get_data (&object->qdata, quark_weak_refs);
  if (wstack)
    {
      guint i;

      for (i = 0; i < wstack->n_weak_refs; i++)
	if (wstack->weak_refs[i].notify == notify &&
	    wstack->weak_refs[i].data == data)
	  {
	    found_one = TRUE;
	    wstack->n_weak_refs -= 1;
	    if (i != wstack->n_weak_refs)
	      wstack->weak_refs[i] = wstack->weak_refs[wstack->n_weak_refs];

	    break;
	  }
    }
  G_UNLOCK (weak_refs_mutex);
  if (!found_one)
    g_warning ("%s: couldn't find weak ref %p(%p)", G_STRFUNC, notify, data);
}

/**
 * g_object_add_weak_pointer: (skip)
 * @object: The object that should be weak referenced.
 * @weak_pointer_location: (inout) (not optional): The memory address
 *    of a pointer.
 *
 * Adds a weak reference from weak_pointer to @object to indicate that
 * the pointer located at @weak_pointer_location is only valid during
 * the lifetime of @object. When the @object is finalized,
 * @weak_pointer will be set to %NULL.
 *
 * Note that as with g_object_weak_ref(), the weak references created by
 * this method are not thread-safe: they cannot safely be used in one
 * thread if the object's last g_object_unref() might happen in another
 * thread. Use #GWeakRef if thread-safety is required.
 */
void
g_object_add_weak_pointer (GObject  *object, 
                           gpointer *weak_pointer_location)
{
  g_return_if_fail (G_IS_OBJECT (object));
  g_return_if_fail (weak_pointer_location != NULL);

  g_object_weak_ref (object, 
                     (GWeakNotify) g_nullify_pointer, 
                     weak_pointer_location);
}

/**
 * g_object_remove_weak_pointer: (skip)
 * @object: The object that is weak referenced.
 * @weak_pointer_location: (inout) (not optional): The memory address
 *    of a pointer.
 *
 * Removes a weak reference from @object that was previously added
 * using g_object_add_weak_pointer(). The @weak_pointer_location has
 * to match the one used with g_object_add_weak_pointer().
 */
void
g_object_remove_weak_pointer (GObject  *object, 
                              gpointer *weak_pointer_location)
{
  g_return_if_fail (G_IS_OBJECT (object));
  g_return_if_fail (weak_pointer_location != NULL);

  g_object_weak_unref (object, 
                       (GWeakNotify) g_nullify_pointer, 
                       weak_pointer_location);
}

static guint
object_floating_flag_handler (GObject        *object,
                              gint            job)
{
  switch (job)
    {
      gpointer oldvalue;
    case +1:    /* force floating if possible */
      do
        oldvalue = g_atomic_pointer_get (&object->qdata);
      while (!g_atomic_pointer_compare_and_exchange ((void**) &object->qdata, oldvalue,
                                                     (gpointer) ((gsize) oldvalue | OBJECT_FLOATING_FLAG)));
      return (gsize) oldvalue & OBJECT_FLOATING_FLAG;
    case -1:    /* sink if possible */
      do
        oldvalue = g_atomic_pointer_get (&object->qdata);
      while (!g_atomic_pointer_compare_and_exchange ((void**) &object->qdata, oldvalue,
                                                     (gpointer) ((gsize) oldvalue & ~(gsize) OBJECT_FLOATING_FLAG)));
      return (gsize) oldvalue & OBJECT_FLOATING_FLAG;
    default:    /* check floating */
      return 0 != ((gsize) g_atomic_pointer_get (&object->qdata) & OBJECT_FLOATING_FLAG);
    }
}

/**
 * g_object_is_floating:
 * @object: (type GObject.Object): a #GObject
 *
 * Checks whether @object has a [floating][floating-ref] reference.
 *
 * Since: 2.10
 *
 * Returns: %TRUE if @object has a floating reference
 */
gboolean
g_object_is_floating (gpointer _object)
{
  GObject *object = _object;
  g_return_val_if_fail (G_IS_OBJECT (object), FALSE);
  return floating_flag_handler (object, 0);
}

/**
 * g_object_ref_sink:
 * @object: (type GObject.Object): a #GObject
 *
 * Increase the reference count of @object, and possibly remove the
 * [floating][floating-ref] reference, if @object has a floating reference.
 *
 * In other words, if the object is floating, then this call "assumes
 * ownership" of the floating reference, converting it to a normal
 * reference by clearing the floating flag while leaving the reference
 * count unchanged.  If the object is not floating, then this call
 * adds a new normal reference increasing the reference count by one.
 *
 * Since GLib 2.56, the type of @object will be propagated to the return type
 * under the same conditions as for g_object_ref().
 *
 * Since: 2.10
 *
 * Returns: (type GObject.Object) (transfer none): @object
 */
gpointer
(g_object_ref_sink) (gpointer _object)
{
  GObject *object = _object;
  gboolean was_floating;
  g_return_val_if_fail (G_IS_OBJECT (object), object);
  g_return_val_if_fail (object->ref_count >= 1, object);
  g_object_ref (object);
  was_floating = floating_flag_handler (object, -1);
  if (was_floating)
    g_object_unref (object);
  return object;
}

/**
 * g_object_force_floating:
 * @object: a #GObject
 *
 * This function is intended for #GObject implementations to re-enforce
 * a [floating][floating-ref] object reference. Doing this is seldom
 * required: all #GInitiallyUnowneds are created with a floating reference
 * which usually just needs to be sunken by calling g_object_ref_sink().
 *
 * Since: 2.10
 */
void
g_object_force_floating (GObject *object)
{
  g_return_if_fail (G_IS_OBJECT (object));
  g_return_if_fail (object->ref_count >= 1);

  floating_flag_handler (object, +1);
}

typedef struct {
  GObject *object;
  guint n_toggle_refs;
  struct {
    GToggleNotify notify;
    gpointer    data;
  } toggle_refs[1];  /* flexible array */
} ToggleRefStack;

static void
toggle_refs_notify (GObject *object,
		    gboolean is_last_ref)
{
  ToggleRefStack tstack, *tstackptr;

  G_LOCK (toggle_refs_mutex);
  tstackptr = g_datalist_id_get_data (&object->qdata, quark_toggle_refs);
  tstack = *tstackptr;
  G_UNLOCK (toggle_refs_mutex);

  /* Reentrancy here is not as tricky as it seems, because a toggle reference
   * will only be notified when there is exactly one of them.
   */
  g_assert (tstack.n_toggle_refs == 1);
  tstack.toggle_refs[0].notify (tstack.toggle_refs[0].data, tstack.object, is_last_ref);
}

/**
 * g_object_add_toggle_ref: (skip)
 * @object: a #GObject
 * @notify: a function to call when this reference is the
 *  last reference to the object, or is no longer
 *  the last reference.
 * @data: data to pass to @notify
 *
 * Increases the reference count of the object by one and sets a
 * callback to be called when all other references to the object are
 * dropped, or when this is already the last reference to the object
 * and another reference is established.
 *
 * This functionality is intended for binding @object to a proxy
 * object managed by another memory manager. This is done with two
 * paired references: the strong reference added by
 * g_object_add_toggle_ref() and a reverse reference to the proxy
 * object which is either a strong reference or weak reference.
 *
 * The setup is that when there are no other references to @object,
 * only a weak reference is held in the reverse direction from @object
 * to the proxy object, but when there are other references held to
 * @object, a strong reference is held. The @notify callback is called
 * when the reference from @object to the proxy object should be
 * "toggled" from strong to weak (@is_last_ref true) or weak to strong
 * (@is_last_ref false).
 *
 * Since a (normal) reference must be held to the object before
 * calling g_object_add_toggle_ref(), the initial state of the reverse
 * link is always strong.
 *
 * Multiple toggle references may be added to the same gobject,
 * however if there are multiple toggle references to an object, none
 * of them will ever be notified until all but one are removed.  For
 * this reason, you should only ever use a toggle reference if there
 * is important state in the proxy object.
 *
 * Since: 2.8
 */
void
g_object_add_toggle_ref (GObject       *object,
			 GToggleNotify  notify,
			 gpointer       data)
{
  ToggleRefStack *tstack;
  guint i;
  
  g_return_if_fail (G_IS_OBJECT (object));
  g_return_if_fail (notify != NULL);
  g_return_if_fail (object->ref_count >= 1);

  g_object_ref (object);

  G_LOCK (toggle_refs_mutex);
  tstack = g_datalist_id_remove_no_notify (&object->qdata, quark_toggle_refs);
  if (tstack)
    {
      i = tstack->n_toggle_refs++;
      /* allocate i = tstate->n_toggle_refs - 1 positions beyond the 1 declared
       * in tstate->toggle_refs */
      tstack = g_realloc (tstack, sizeof (*tstack) + sizeof (tstack->toggle_refs[0]) * i);
    }
  else
    {
      tstack = g_renew (ToggleRefStack, NULL, 1);
      tstack->object = object;
      tstack->n_toggle_refs = 1;
      i = 0;
    }

  /* Set a flag for fast lookup after adding the first toggle reference */
  if (tstack->n_toggle_refs == 1)
    g_datalist_set_flags (&object->qdata, OBJECT_HAS_TOGGLE_REF_FLAG);
  
  tstack->toggle_refs[i].notify = notify;
  tstack->toggle_refs[i].data = data;
  g_datalist_id_set_data_full (&object->qdata, quark_toggle_refs, tstack,
			       (GDestroyNotify)g_free);
  G_UNLOCK (toggle_refs_mutex);
}

/**
 * g_object_remove_toggle_ref: (skip)
 * @object: a #GObject
 * @notify: a function to call when this reference is the
 *  last reference to the object, or is no longer
 *  the last reference.
 * @data: data to pass to @notify
 *
 * Removes a reference added with g_object_add_toggle_ref(). The
 * reference count of the object is decreased by one.
 *
 * Since: 2.8
 */
void
g_object_remove_toggle_ref (GObject       *object,
			    GToggleNotify  notify,
			    gpointer       data)
{
  ToggleRefStack *tstack;
  gboolean found_one = FALSE;

  g_return_if_fail (G_IS_OBJECT (object));
  g_return_if_fail (notify != NULL);

  G_LOCK (toggle_refs_mutex);
  tstack = g_datalist_id_get_data (&object->qdata, quark_toggle_refs);
  if (tstack)
    {
      guint i;

      for (i = 0; i < tstack->n_toggle_refs; i++)
	if (tstack->toggle_refs[i].notify == notify &&
	    tstack->toggle_refs[i].data == data)
	  {
	    found_one = TRUE;
	    tstack->n_toggle_refs -= 1;
	    if (i != tstack->n_toggle_refs)
	      tstack->toggle_refs[i] = tstack->toggle_refs[tstack->n_toggle_refs];

	    if (tstack->n_toggle_refs == 0)
	      g_datalist_unset_flags (&object->qdata, OBJECT_HAS_TOGGLE_REF_FLAG);

	    break;
	  }
    }
  G_UNLOCK (toggle_refs_mutex);

  if (found_one)
    g_object_unref (object);
  else
    g_warning ("%s: couldn't find toggle ref %p(%p)", G_STRFUNC, notify, data);
}

/**
 * g_object_ref:
 * @object: (type GObject.Object): a #GObject
 *
 * Increases the reference count of @object.
 *
 * Since GLib 2.56, if `GLIB_VERSION_MAX_ALLOWED` is 2.56 or greater, the type
 * of @object will be propagated to the return type (using the GCC typeof()
 * extension), so any casting the caller needs to do on the return type must be
 * explicit.
 *
 * Returns: (type GObject.Object) (transfer none): the same @object
 */
gpointer
(g_object_ref) (gpointer _object)
{
  GObject *object = _object;
  gint old_val;

  g_return_val_if_fail (G_IS_OBJECT (object), NULL);
  g_return_val_if_fail (object->ref_count > 0, NULL);
  
  old_val = g_atomic_int_add (&object->ref_count, 1);

  if (old_val == 1 && OBJECT_HAS_TOGGLE_REF (object))
    toggle_refs_notify (object, FALSE);

  TRACE (GOBJECT_OBJECT_REF(object,G_TYPE_FROM_INSTANCE(object),old_val));

  return object;
}

/**
 * g_object_unref:
 * @object: (type GObject.Object): a #GObject
 *
 * Decreases the reference count of @object. When its reference count
 * drops to 0, the object is finalized (i.e. its memory is freed).
 *
 * If the pointer to the #GObject may be reused in future (for example, if it is
 * an instance variable of another object), it is recommended to clear the
 * pointer to %NULL rather than retain a dangling pointer to a potentially
 * invalid #GObject instance. Use g_clear_object() for this.
 */
void
g_object_unref (gpointer _object)
{
  GObject *object = _object;
  gint old_ref;
  
  g_return_if_fail (G_IS_OBJECT (object));
  g_return_if_fail (object->ref_count > 0);
  
  /* here we want to atomically do: if (ref_count>1) { ref_count--; return; } */
 retry_atomic_decrement1:
  old_ref = g_atomic_int_get (&object->ref_count);
  if (old_ref > 1)
    {
      /* valid if last 2 refs are owned by this call to unref and the toggle_ref */
      gboolean has_toggle_ref = OBJECT_HAS_TOGGLE_REF (object);

      if (!g_atomic_int_compare_and_exchange ((int *)&object->ref_count, old_ref, old_ref - 1))
	goto retry_atomic_decrement1;

      TRACE (GOBJECT_OBJECT_UNREF(object,G_TYPE_FROM_INSTANCE(object),old_ref));

      /* if we went from 2->1 we need to notify toggle refs if any */
      if (old_ref == 2 && has_toggle_ref) /* The last ref being held in this case is owned by the toggle_ref */
	toggle_refs_notify (object, TRUE);
    }
  else
    {
      GSList **weak_locations;

      /* The only way that this object can live at this point is if
       * there are outstanding weak references already established
       * before we got here.
       *
       * If there were not already weak references then no more can be
       * established at this time, because the other thread would have
       * to hold a strong ref in order to call
       * g_object_add_weak_pointer() and then we wouldn't be here.
       */
      weak_locations = g_datalist_id_get_data (&object->qdata, quark_weak_locations);

      if (weak_locations != NULL)
        {
          g_rw_lock_writer_lock (&weak_locations_lock);

          /* It is possible that one of the weak references beat us to
           * the lock. Make sure the refcount is still what we expected
           * it to be.
           */
          old_ref = g_atomic_int_get (&object->ref_count);
          if (old_ref != 1)
            {
              g_rw_lock_writer_unlock (&weak_locations_lock);
              goto retry_atomic_decrement1;
            }

          /* We got the lock first, so the object will definitely die
           * now. Clear out all the weak references.
           */
          while (*weak_locations)
            {
              GWeakRef *weak_ref_location = (*weak_locations)->data;

              weak_ref_location->priv.p = NULL;
              *weak_locations = g_slist_delete_link (*weak_locations, *weak_locations);
            }

          g_rw_lock_writer_unlock (&weak_locations_lock);
        }

      /* we are about to remove the last reference */
      TRACE (GOBJECT_OBJECT_DISPOSE(object,G_TYPE_FROM_INSTANCE(object), 1));
      G_OBJECT_GET_CLASS (object)->dispose (object);
      TRACE (GOBJECT_OBJECT_DISPOSE_END(object,G_TYPE_FROM_INSTANCE(object), 1));

      /* may have been re-referenced meanwhile */
    retry_atomic_decrement2:
      old_ref = g_atomic_int_get ((int *)&object->ref_count);
      if (old_ref > 1)
        {
          /* valid if last 2 refs are owned by this call to unref and the toggle_ref */
          gboolean has_toggle_ref = OBJECT_HAS_TOGGLE_REF (object);

          if (!g_atomic_int_compare_and_exchange ((int *)&object->ref_count, old_ref, old_ref - 1))
	    goto retry_atomic_decrement2;

	  TRACE (GOBJECT_OBJECT_UNREF(object,G_TYPE_FROM_INSTANCE(object),old_ref));

          /* if we went from 2->1 we need to notify toggle refs if any */
          if (old_ref == 2 && has_toggle_ref) /* The last ref being held in this case is owned by the toggle_ref */
	    toggle_refs_notify (object, TRUE);

	  return;
	}

      /* we are still in the process of taking away the last ref */
      g_datalist_id_set_data (&object->qdata, quark_closure_array, NULL);
      g_signal_handlers_destroy (object);
      g_datalist_id_set_data (&object->qdata, quark_weak_refs, NULL);
      
      /* decrement the last reference */
      old_ref = g_atomic_int_add (&object->ref_count, -1);

      TRACE (GOBJECT_OBJECT_UNREF(object,G_TYPE_FROM_INSTANCE(object),old_ref));

      /* may have been re-referenced meanwhile */
      if (G_LIKELY (old_ref == 1))
	{
	  TRACE (GOBJECT_OBJECT_FINALIZE(object,G_TYPE_FROM_INSTANCE(object)));
          G_OBJECT_GET_CLASS (object)->finalize (object);

	  TRACE (GOBJECT_OBJECT_FINALIZE_END(object,G_TYPE_FROM_INSTANCE(object)));

          GOBJECT_IF_DEBUG (OBJECTS,
	    {
	      /* catch objects not chaining finalize handlers */
	      G_LOCK (debug_objects);
	      g_assert (!g_hash_table_contains (debug_objects_ht, object));
	      G_UNLOCK (debug_objects);
	    });
          g_type_free_instance ((GTypeInstance*) object);
	}
    }
}

/**
 * g_clear_object: (skip)
 * @object_ptr: a pointer to a #GObject reference
 *
 * Clears a reference to a #GObject.
 *
 * @object_ptr must not be %NULL.
 *
 * If the reference is %NULL then this function does nothing.
 * Otherwise, the reference count of the object is decreased and the
 * pointer is set to %NULL.
 *
 * A macro is also included that allows this function to be used without
 * pointer casts.
 *
 * Since: 2.28
 **/
#undef g_clear_object
void
g_clear_object (volatile GObject **object_ptr)
{
  g_clear_pointer (object_ptr, g_object_unref);
}

/**
 * g_object_get_qdata:
 * @object: The GObject to get a stored user data pointer from
 * @quark: A #GQuark, naming the user data pointer
 * 
 * This function gets back user data pointers stored via
 * g_object_set_qdata().
 * 
 * Returns: (transfer none) (nullable): The user data pointer set, or %NULL
 */
gpointer
g_object_get_qdata (GObject *object,
		    GQuark   quark)
{
  g_return_val_if_fail (G_IS_OBJECT (object), NULL);
  
  return quark ? g_datalist_id_get_data (&object->qdata, quark) : NULL;
}

/**
 * g_object_set_qdata: (skip)
 * @object: The GObject to set store a user data pointer
 * @quark: A #GQuark, naming the user data pointer
 * @data: (nullable): An opaque user data pointer
 *
 * This sets an opaque, named pointer on an object.
 * The name is specified through a #GQuark (retrived e.g. via
 * g_quark_from_static_string()), and the pointer
 * can be gotten back from the @object with g_object_get_qdata()
 * until the @object is finalized.
 * Setting a previously set user data pointer, overrides (frees)
 * the old pointer set, using #NULL as pointer essentially
 * removes the data stored.
 */
void
g_object_set_qdata (GObject *object,
		    GQuark   quark,
		    gpointer data)
{
  g_return_if_fail (G_IS_OBJECT (object));
  g_return_if_fail (quark > 0);

  g_datalist_id_set_data (&object->qdata, quark, data);
}

/**
 * g_object_dup_qdata: (skip)
 * @object: the #GObject to store user data on
 * @quark: a #GQuark, naming the user data pointer
 * @dup_func: (nullable): function to dup the value
 * @user_data: (nullable): passed as user_data to @dup_func
 *
 * This is a variant of g_object_get_qdata() which returns
 * a 'duplicate' of the value. @dup_func defines the
 * meaning of 'duplicate' in this context, it could e.g.
 * take a reference on a ref-counted object.
 *
 * If the @quark is not set on the object then @dup_func
 * will be called with a %NULL argument.
 *
 * Note that @dup_func is called while user data of @object
 * is locked.
 *
 * This function can be useful to avoid races when multiple
 * threads are using object data on the same key on the same
 * object.
 *
 * Returns: the result of calling @dup_func on the value
 *     associated with @quark on @object, or %NULL if not set.
 *     If @dup_func is %NULL, the value is returned
 *     unmodified.
 *
 * Since: 2.34
 */
gpointer
g_object_dup_qdata (GObject        *object,
                    GQuark          quark,
                    GDuplicateFunc   dup_func,
                    gpointer         user_data)
{
  g_return_val_if_fail (G_IS_OBJECT (object), NULL);
  g_return_val_if_fail (quark > 0, NULL);

  return g_datalist_id_dup_data (&object->qdata, quark, dup_func, user_data);
}

/**
 * g_object_replace_qdata: (skip)
 * @object: the #GObject to store user data on
 * @quark: a #GQuark, naming the user data pointer
 * @oldval: (nullable): the old value to compare against
 * @newval: (nullable): the new value
 * @destroy: (nullable): a destroy notify for the new value
 * @old_destroy: (out) (optional): destroy notify for the existing value
 *
 * Compares the user data for the key @quark on @object with
 * @oldval, and if they are the same, replaces @oldval with
 * @newval.
 *
 * This is like a typical atomic compare-and-exchange
 * operation, for user data on an object.
 *
 * If the previous value was replaced then ownership of the
 * old value (@oldval) is passed to the caller, including
 * the registered destroy notify for it (passed out in @old_destroy).
 * It’s up to the caller to free this as needed, which may
 * or may not include using @old_destroy as sometimes replacement
 * should not destroy the object in the normal way.
 *
 * Returns: %TRUE if the existing value for @quark was replaced
 *  by @newval, %FALSE otherwise.
 *
 * Since: 2.34
 */
gboolean
g_object_replace_qdata (GObject        *object,
                        GQuark          quark,
                        gpointer        oldval,
                        gpointer        newval,
                        GDestroyNotify  destroy,
                        GDestroyNotify *old_destroy)
{
  g_return_val_if_fail (G_IS_OBJECT (object), FALSE);
  g_return_val_if_fail (quark > 0, FALSE);

  return g_datalist_id_replace_data (&object->qdata, quark,
                                     oldval, newval, destroy,
                                     old_destroy);
}

/**
 * g_object_set_qdata_full: (skip)
 * @object: The GObject to set store a user data pointer
 * @quark: A #GQuark, naming the user data pointer
 * @data: (nullable): An opaque user data pointer
 * @destroy: (nullable): Function to invoke with @data as argument, when @data
 *           needs to be freed
 *
 * This function works like g_object_set_qdata(), but in addition,
 * a void (*destroy) (gpointer) function may be specified which is
 * called with @data as argument when the @object is finalized, or
 * the data is being overwritten by a call to g_object_set_qdata()
 * with the same @quark.
 */
void
g_object_set_qdata_full (GObject       *object,
			 GQuark		quark,
			 gpointer	data,
			 GDestroyNotify destroy)
{
  g_return_if_fail (G_IS_OBJECT (object));
  g_return_if_fail (quark > 0);
  
  g_datalist_id_set_data_full (&object->qdata, quark, data,
			       data ? destroy : (GDestroyNotify) NULL);
}

/**
 * g_object_steal_qdata:
 * @object: The GObject to get a stored user data pointer from
 * @quark: A #GQuark, naming the user data pointer
 *
 * This function gets back user data pointers stored via
 * g_object_set_qdata() and removes the @data from object
 * without invoking its destroy() function (if any was
 * set).
 * Usually, calling this function is only required to update
 * user data pointers with a destroy notifier, for example:
 * |[<!-- language="C" --> 
 * void
 * object_add_to_user_list (GObject     *object,
 *                          const gchar *new_string)
 * {
 *   // the quark, naming the object data
 *   GQuark quark_string_list = g_quark_from_static_string ("my-string-list");
 *   // retrive the old string list
 *   GList *list = g_object_steal_qdata (object, quark_string_list);
 *
 *   // prepend new string
 *   list = g_list_prepend (list, g_strdup (new_string));
 *   // this changed 'list', so we need to set it again
 *   g_object_set_qdata_full (object, quark_string_list, list, free_string_list);
 * }
 * static void
 * free_string_list (gpointer data)
 * {
 *   GList *node, *list = data;
 *
 *   for (node = list; node; node = node->next)
 *     g_free (node->data);
 *   g_list_free (list);
 * }
 * ]|
 * Using g_object_get_qdata() in the above example, instead of
 * g_object_steal_qdata() would have left the destroy function set,
 * and thus the partial string list would have been freed upon
 * g_object_set_qdata_full().
 *
 * Returns: (transfer full) (nullable): The user data pointer set, or %NULL
 */
gpointer
g_object_steal_qdata (GObject *object,
		      GQuark   quark)
{
  g_return_val_if_fail (G_IS_OBJECT (object), NULL);
  g_return_val_if_fail (quark > 0, NULL);
  
  return g_datalist_id_remove_no_notify (&object->qdata, quark);
}

/**
 * g_object_get_data:
 * @object: #GObject containing the associations
 * @key: name of the key for that association
 * 
 * Gets a named field from the objects table of associations (see g_object_set_data()).
 * 
 * Returns: (transfer none) (nullable): the data if found,
 *          or %NULL if no such data exists.
 */
gpointer
g_object_get_data (GObject     *object,
                   const gchar *key)
{
  g_return_val_if_fail (G_IS_OBJECT (object), NULL);
  g_return_val_if_fail (key != NULL, NULL);

  return g_datalist_get_data (&object->qdata, key);
}

/**
 * g_object_set_data:
 * @object: #GObject containing the associations.
 * @key: name of the key
 * @data: (nullable): data to associate with that key
 *
 * Each object carries around a table of associations from
 * strings to pointers.  This function lets you set an association.
 *
 * If the object already had an association with that name,
 * the old association will be destroyed.
 */
void
g_object_set_data (GObject     *object,
                   const gchar *key,
                   gpointer     data)
{
  g_return_if_fail (G_IS_OBJECT (object));
  g_return_if_fail (key != NULL);

  g_datalist_id_set_data (&object->qdata, g_quark_from_string (key), data);
}

/**
 * g_object_dup_data: (skip)
 * @object: the #GObject to store user data on
 * @key: a string, naming the user data pointer
 * @dup_func: (nullable): function to dup the value
 * @user_data: (nullable): passed as user_data to @dup_func
 *
 * This is a variant of g_object_get_data() which returns
 * a 'duplicate' of the value. @dup_func defines the
 * meaning of 'duplicate' in this context, it could e.g.
 * take a reference on a ref-counted object.
 *
 * If the @key is not set on the object then @dup_func
 * will be called with a %NULL argument.
 *
 * Note that @dup_func is called while user data of @object
 * is locked.
 *
 * This function can be useful to avoid races when multiple
 * threads are using object data on the same key on the same
 * object.
 *
 * Returns: the result of calling @dup_func on the value
 *     associated with @key on @object, or %NULL if not set.
 *     If @dup_func is %NULL, the value is returned
 *     unmodified.
 *
 * Since: 2.34
 */
gpointer
g_object_dup_data (GObject        *object,
                   const gchar    *key,
                   GDuplicateFunc   dup_func,
                   gpointer         user_data)
{
  g_return_val_if_fail (G_IS_OBJECT (object), NULL);
  g_return_val_if_fail (key != NULL, NULL);

  return g_datalist_id_dup_data (&object->qdata,
                                 g_quark_from_string (key),
                                 dup_func, user_data);
}

/**
 * g_object_replace_data: (skip)
 * @object: the #GObject to store user data on
 * @key: a string, naming the user data pointer
 * @oldval: (nullable): the old value to compare against
 * @newval: (nullable): the new value
 * @destroy: (nullable): a destroy notify for the new value
 * @old_destroy: (out) (optional): destroy notify for the existing value
 *
 * Compares the user data for the key @key on @object with
 * @oldval, and if they are the same, replaces @oldval with
 * @newval.
 *
 * This is like a typical atomic compare-and-exchange
 * operation, for user data on an object.
 *
 * If the previous value was replaced then ownership of the
 * old value (@oldval) is passed to the caller, including
 * the registered destroy notify for it (passed out in @old_destroy).
 * It’s up to the caller to free this as needed, which may
 * or may not include using @old_destroy as sometimes replacement
 * should not destroy the object in the normal way.
 *
 * Returns: %TRUE if the existing value for @key was replaced
 *  by @newval, %FALSE otherwise.
 *
 * Since: 2.34
 */
gboolean
g_object_replace_data (GObject        *object,
                       const gchar    *key,
                       gpointer        oldval,
                       gpointer        newval,
                       GDestroyNotify  destroy,
                       GDestroyNotify *old_destroy)
{
  g_return_val_if_fail (G_IS_OBJECT (object), FALSE);
  g_return_val_if_fail (key != NULL, FALSE);

  return g_datalist_id_replace_data (&object->qdata,
                                     g_quark_from_string (key),
                                     oldval, newval, destroy,
                                     old_destroy);
}

/**
 * g_object_set_data_full: (skip)
 * @object: #GObject containing the associations
 * @key: name of the key
 * @data: (nullable): data to associate with that key
 * @destroy: (nullable): function to call when the association is destroyed
 *
 * Like g_object_set_data() except it adds notification
 * for when the association is destroyed, either by setting it
 * to a different value or when the object is destroyed.
 *
 * Note that the @destroy callback is not called if @data is %NULL.
 */
void
g_object_set_data_full (GObject       *object,
                        const gchar   *key,
                        gpointer       data,
                        GDestroyNotify destroy)
{
  g_return_if_fail (G_IS_OBJECT (object));
  g_return_if_fail (key != NULL);

  g_datalist_id_set_data_full (&object->qdata, g_quark_from_string (key), data,
			       data ? destroy : (GDestroyNotify) NULL);
}

/**
 * g_object_steal_data:
 * @object: #GObject containing the associations
 * @key: name of the key
 *
 * Remove a specified datum from the object's data associations,
 * without invoking the association's destroy handler.
 *
 * Returns: (transfer full) (nullable): the data if found, or %NULL
 *          if no such data exists.
 */
gpointer
g_object_steal_data (GObject     *object,
                     const gchar *key)
{
  GQuark quark;

  g_return_val_if_fail (G_IS_OBJECT (object), NULL);
  g_return_val_if_fail (key != NULL, NULL);

  quark = g_quark_try_string (key);

  return quark ? g_datalist_id_remove_no_notify (&object->qdata, quark) : NULL;
}

static void
g_value_object_init (GValue *value)
{
  value->data[0].v_pointer = NULL;
}

static void
g_value_object_free_value (GValue *value)
{
  if (value->data[0].v_pointer)
    g_object_unref (value->data[0].v_pointer);
}

static void
g_value_object_copy_value (const GValue *src_value,
			   GValue	*dest_value)
{
  if (src_value->data[0].v_pointer)
    dest_value->data[0].v_pointer = g_object_ref (src_value->data[0].v_pointer);
  else
    dest_value->data[0].v_pointer = NULL;
}

static void
g_value_object_transform_value (const GValue *src_value,
				GValue       *dest_value)
{
  if (src_value->data[0].v_pointer && g_type_is_a (G_OBJECT_TYPE (src_value->data[0].v_pointer), G_VALUE_TYPE (dest_value)))
    dest_value->data[0].v_pointer = g_object_ref (src_value->data[0].v_pointer);
  else
    dest_value->data[0].v_pointer = NULL;
}

static gpointer
g_value_object_peek_pointer (const GValue *value)
{
  return value->data[0].v_pointer;
}

static gchar*
g_value_object_collect_value (GValue	  *value,
			      guint        n_collect_values,
			      GTypeCValue *collect_values,
			      guint        collect_flags)
{
  if (collect_values[0].v_pointer)
    {
      GObject *object = collect_values[0].v_pointer;
      
      if (object->g_type_instance.g_class == NULL)
	return g_strconcat ("invalid unclassed object pointer for value type '",
			    G_VALUE_TYPE_NAME (value),
			    "'",
			    NULL);
      else if (!g_value_type_compatible (G_OBJECT_TYPE (object), G_VALUE_TYPE (value)))
	return g_strconcat ("invalid object type '",
			    G_OBJECT_TYPE_NAME (object),
			    "' for value type '",
			    G_VALUE_TYPE_NAME (value),
			    "'",
			    NULL);
      /* never honour G_VALUE_NOCOPY_CONTENTS for ref-counted types */
      value->data[0].v_pointer = g_object_ref (object);
    }
  else
    value->data[0].v_pointer = NULL;
  
  return NULL;
}

static gchar*
g_value_object_lcopy_value (const GValue *value,
			    guint        n_collect_values,
			    GTypeCValue *collect_values,
			    guint        collect_flags)
{
  GObject **object_p = collect_values[0].v_pointer;
  
  if (!object_p)
    return g_strdup_printf ("value location for '%s' passed as NULL", G_VALUE_TYPE_NAME (value));

  if (!value->data[0].v_pointer)
    *object_p = NULL;
  else if (collect_flags & G_VALUE_NOCOPY_CONTENTS)
    *object_p = value->data[0].v_pointer;
  else
    *object_p = g_object_ref (value->data[0].v_pointer);
  
  return NULL;
}

/**
 * g_value_set_object:
 * @value: a valid #GValue of %G_TYPE_OBJECT derived type
 * @v_object: (type GObject.Object) (nullable): object value to be set
 *
 * Set the contents of a %G_TYPE_OBJECT derived #GValue to @v_object.
 *
 * g_value_set_object() increases the reference count of @v_object
 * (the #GValue holds a reference to @v_object).  If you do not wish
 * to increase the reference count of the object (i.e. you wish to
 * pass your current reference to the #GValue because you no longer
 * need it), use g_value_take_object() instead.
 *
 * It is important that your #GValue holds a reference to @v_object (either its
 * own, or one it has taken) to ensure that the object won't be destroyed while
 * the #GValue still exists).
 */
void
g_value_set_object (GValue   *value,
		    gpointer  v_object)
{
  GObject *old;
	
  g_return_if_fail (G_VALUE_HOLDS_OBJECT (value));

  old = value->data[0].v_pointer;
  
  if (v_object)
    {
      g_return_if_fail (G_IS_OBJECT (v_object));
      g_return_if_fail (g_value_type_compatible (G_OBJECT_TYPE (v_object), G_VALUE_TYPE (value)));

      value->data[0].v_pointer = v_object;
      g_object_ref (value->data[0].v_pointer);
    }
  else
    value->data[0].v_pointer = NULL;
  
  if (old)
    g_object_unref (old);
}

/**
 * g_value_set_object_take_ownership: (skip)
 * @value: a valid #GValue of %G_TYPE_OBJECT derived type
 * @v_object: (nullable): object value to be set
 *
 * This is an internal function introduced mainly for C marshallers.
 *
 * Deprecated: 2.4: Use g_value_take_object() instead.
 */
void
g_value_set_object_take_ownership (GValue  *value,
				   gpointer v_object)
{
  g_value_take_object (value, v_object);
}

/**
 * g_value_take_object: (skip)
 * @value: a valid #GValue of %G_TYPE_OBJECT derived type
 * @v_object: (nullable): object value to be set
 *
 * Sets the contents of a %G_TYPE_OBJECT derived #GValue to @v_object
 * and takes over the ownership of the callers reference to @v_object;
 * the caller doesn't have to unref it any more (i.e. the reference
 * count of the object is not increased).
 *
 * If you want the #GValue to hold its own reference to @v_object, use
 * g_value_set_object() instead.
 *
 * Since: 2.4
 */
void
g_value_take_object (GValue  *value,
		     gpointer v_object)
{
  g_return_if_fail (G_VALUE_HOLDS_OBJECT (value));

  if (value->data[0].v_pointer)
    {
      g_object_unref (value->data[0].v_pointer);
      value->data[0].v_pointer = NULL;
    }

  if (v_object)
    {
      g_return_if_fail (G_IS_OBJECT (v_object));
      g_return_if_fail (g_value_type_compatible (G_OBJECT_TYPE (v_object), G_VALUE_TYPE (value)));

      value->data[0].v_pointer = v_object; /* we take over the reference count */
    }
}

/**
 * g_value_get_object:
 * @value: a valid #GValue of %G_TYPE_OBJECT derived type
 * 
 * Get the contents of a %G_TYPE_OBJECT derived #GValue.
 * 
 * Returns: (type GObject.Object) (transfer none): object contents of @value
 */
gpointer
g_value_get_object (const GValue *value)
{
  g_return_val_if_fail (G_VALUE_HOLDS_OBJECT (value), NULL);
  
  return value->data[0].v_pointer;
}

/**
 * g_value_dup_object:
 * @value: a valid #GValue whose type is derived from %G_TYPE_OBJECT
 *
 * Get the contents of a %G_TYPE_OBJECT derived #GValue, increasing
 * its reference count. If the contents of the #GValue are %NULL, then
 * %NULL will be returned.
 *
 * Returns: (type GObject.Object) (transfer full): object content of @value,
 *          should be unreferenced when no longer needed.
 */
gpointer
g_value_dup_object (const GValue *value)
{
  g_return_val_if_fail (G_VALUE_HOLDS_OBJECT (value), NULL);
  
  return value->data[0].v_pointer ? g_object_ref (value->data[0].v_pointer) : NULL;
}

/**
 * g_signal_connect_object: (skip)
 * @instance: (type GObject.TypeInstance): the instance to connect to.
 * @detailed_signal: a string of the form "signal-name::detail".
 * @c_handler: the #GCallback to connect.
 * @gobject: (type GObject.Object) (nullable): the object to pass as data
 *    to @c_handler.
 * @connect_flags: a combination of #GConnectFlags.
 *
 * This is similar to g_signal_connect_data(), but uses a closure which
 * ensures that the @gobject stays alive during the call to @c_handler
 * by temporarily adding a reference count to @gobject.
 *
 * When the @gobject is destroyed the signal handler will be automatically
 * disconnected.  Note that this is not currently threadsafe (ie:
 * emitting a signal while @gobject is being destroyed in another thread
 * is not safe).
 *
 * Returns: the handler id.
 */
gulong
g_signal_connect_object (gpointer      instance,
			 const gchar  *detailed_signal,
			 GCallback     c_handler,
			 gpointer      gobject,
			 GConnectFlags connect_flags)
{
  g_return_val_if_fail (G_TYPE_CHECK_INSTANCE (instance), 0);
  g_return_val_if_fail (detailed_signal != NULL, 0);
  g_return_val_if_fail (c_handler != NULL, 0);

  if (gobject)
    {
      GClosure *closure;

      g_return_val_if_fail (G_IS_OBJECT (gobject), 0);

      closure = ((connect_flags & G_CONNECT_SWAPPED) ? g_cclosure_new_object_swap : g_cclosure_new_object) (c_handler, gobject);

      return g_signal_connect_closure (instance, detailed_signal, closure, connect_flags & G_CONNECT_AFTER);
    }
  else
    return g_signal_connect_data (instance, detailed_signal, c_handler, NULL, NULL, connect_flags);
}

typedef struct {
  GObject  *object;
  guint     n_closures;
  GClosure *closures[1]; /* flexible array */
} CArray;
/* don't change this structure without supplying an accessor for
 * watched closures, e.g.:
 * GSList* g_object_list_watched_closures (GObject *object)
 * {
 *   CArray *carray;
 *   g_return_val_if_fail (G_IS_OBJECT (object), NULL);
 *   carray = g_object_get_data (object, "GObject-closure-array");
 *   if (carray)
 *     {
 *       GSList *slist = NULL;
 *       guint i;
 *       for (i = 0; i < carray->n_closures; i++)
 *         slist = g_slist_prepend (slist, carray->closures[i]);
 *       return slist;
 *     }
 *   return NULL;
 * }
 */

static void
object_remove_closure (gpointer  data,
		       GClosure *closure)
{
  GObject *object = data;
  CArray *carray;
  guint i;
  
  G_LOCK (closure_array_mutex);
  carray = g_object_get_qdata (object, quark_closure_array);
  for (i = 0; i < carray->n_closures; i++)
    if (carray->closures[i] == closure)
      {
	carray->n_closures--;
	if (i < carray->n_closures)
	  carray->closures[i] = carray->closures[carray->n_closures];
	G_UNLOCK (closure_array_mutex);
	return;
      }
  G_UNLOCK (closure_array_mutex);
  g_assert_not_reached ();
}

static void
destroy_closure_array (gpointer data)
{
  CArray *carray = data;
  GObject *object = carray->object;
  guint i, n = carray->n_closures;
  
  for (i = 0; i < n; i++)
    {
      GClosure *closure = carray->closures[i];
      
      /* removing object_remove_closure() upfront is probably faster than
       * letting it fiddle with quark_closure_array which is empty anyways
       */
      g_closure_remove_invalidate_notifier (closure, object, object_remove_closure);
      g_closure_invalidate (closure);
    }
  g_free (carray);
}

/**
 * g_object_watch_closure:
 * @object: GObject restricting lifetime of @closure
 * @closure: GClosure to watch
 *
 * This function essentially limits the life time of the @closure to
 * the life time of the object. That is, when the object is finalized,
 * the @closure is invalidated by calling g_closure_invalidate() on
 * it, in order to prevent invocations of the closure with a finalized
 * (nonexisting) object. Also, g_object_ref() and g_object_unref() are
 * added as marshal guards to the @closure, to ensure that an extra
 * reference count is held on @object during invocation of the
 * @closure.  Usually, this function will be called on closures that
 * use this @object as closure data.
 */
void
g_object_watch_closure (GObject  *object,
			GClosure *closure)
{
  CArray *carray;
  guint i;
  
  g_return_if_fail (G_IS_OBJECT (object));
  g_return_if_fail (closure != NULL);
  g_return_if_fail (closure->is_invalid == FALSE);
  g_return_if_fail (closure->in_marshal == FALSE);
  g_return_if_fail (object->ref_count > 0);	/* this doesn't work on finalizing objects */
  
  g_closure_add_invalidate_notifier (closure, object, object_remove_closure);
  g_closure_add_marshal_guards (closure,
				object, (GClosureNotify) g_object_ref,
				object, (GClosureNotify) g_object_unref);
  G_LOCK (closure_array_mutex);
  carray = g_datalist_id_remove_no_notify (&object->qdata, quark_closure_array);
  if (!carray)
    {
      carray = g_renew (CArray, NULL, 1);
      carray->object = object;
      carray->n_closures = 1;
      i = 0;
    }
  else
    {
      i = carray->n_closures++;
      carray = g_realloc (carray, sizeof (*carray) + sizeof (carray->closures[0]) * i);
    }
  carray->closures[i] = closure;
  g_datalist_id_set_data_full (&object->qdata, quark_closure_array, carray, destroy_closure_array);
  G_UNLOCK (closure_array_mutex);
}

/**
 * g_closure_new_object:
 * @sizeof_closure: the size of the structure to allocate, must be at least
 *  `sizeof (GClosure)`
 * @object: a #GObject pointer to store in the @data field of the newly
 *  allocated #GClosure
 *
 * A variant of g_closure_new_simple() which stores @object in the
 * @data field of the closure and calls g_object_watch_closure() on
 * @object and the created closure. This function is mainly useful
 * when implementing new types of closures.
 *
 * Returns: (transfer full): a newly allocated #GClosure
 */
GClosure*
g_closure_new_object (guint    sizeof_closure,
		      GObject *object)
{
  GClosure *closure;

  g_return_val_if_fail (G_IS_OBJECT (object), NULL);
  g_return_val_if_fail (object->ref_count > 0, NULL);     /* this doesn't work on finalizing objects */

  closure = g_closure_new_simple (sizeof_closure, object);
  g_object_watch_closure (object, closure);

  return closure;
}

/**
 * g_cclosure_new_object: (skip)
 * @callback_func: the function to invoke
 * @object: a #GObject pointer to pass to @callback_func
 *
 * A variant of g_cclosure_new() which uses @object as @user_data and
 * calls g_object_watch_closure() on @object and the created
 * closure. This function is useful when you have a callback closely
 * associated with a #GObject, and want the callback to no longer run
 * after the object is is freed.
 *
 * Returns: a new #GCClosure
 */
GClosure*
g_cclosure_new_object (GCallback callback_func,
		       GObject  *object)
{
  GClosure *closure;

  g_return_val_if_fail (G_IS_OBJECT (object), NULL);
  g_return_val_if_fail (object->ref_count > 0, NULL);     /* this doesn't work on finalizing objects */
  g_return_val_if_fail (callback_func != NULL, NULL);

  closure = g_cclosure_new (callback_func, object, NULL);
  g_object_watch_closure (object, closure);

  return closure;
}

/**
 * g_cclosure_new_object_swap: (skip)
 * @callback_func: the function to invoke
 * @object: a #GObject pointer to pass to @callback_func
 *
 * A variant of g_cclosure_new_swap() which uses @object as @user_data
 * and calls g_object_watch_closure() on @object and the created
 * closure. This function is useful when you have a callback closely
 * associated with a #GObject, and want the callback to no longer run
 * after the object is is freed.
 *
 * Returns: a new #GCClosure
 */
GClosure*
g_cclosure_new_object_swap (GCallback callback_func,
			    GObject  *object)
{
  GClosure *closure;

  g_return_val_if_fail (G_IS_OBJECT (object), NULL);
  g_return_val_if_fail (object->ref_count > 0, NULL);     /* this doesn't work on finalizing objects */
  g_return_val_if_fail (callback_func != NULL, NULL);

  closure = g_cclosure_new_swap (callback_func, object, NULL);
  g_object_watch_closure (object, closure);

  return closure;
}

gsize
g_object_compat_control (gsize           what,
                         gpointer        data)
{
  switch (what)
    {
      gpointer *pp;
    case 1:     /* floating base type */
      return G_TYPE_INITIALLY_UNOWNED;
    case 2:     /* FIXME: remove this once GLib/Gtk+ break ABI again */
      floating_flag_handler = (guint(*)(GObject*,gint)) data;
      return 1;
    case 3:     /* FIXME: remove this once GLib/Gtk+ break ABI again */
      pp = data;
      *pp = floating_flag_handler;
      return 1;
    default:
      return 0;
    }
}

G_DEFINE_TYPE (GInitiallyUnowned, g_initially_unowned, G_TYPE_OBJECT)

static void
g_initially_unowned_init (GInitiallyUnowned *object)
{
  g_object_force_floating (object);
}

static void
g_initially_unowned_class_init (GInitiallyUnownedClass *klass)
{
}

/**
 * GWeakRef:
 *
 * A structure containing a weak reference to a #GObject.  It can either
 * be empty (i.e. point to %NULL), or point to an object for as long as
 * at least one "strong" reference to that object exists. Before the
 * object's #GObjectClass.dispose method is called, every #GWeakRef
 * associated with becomes empty (i.e. points to %NULL).
 *
 * Like #GValue, #GWeakRef can be statically allocated, stack- or
 * heap-allocated, or embedded in larger structures.
 *
 * Unlike g_object_weak_ref() and g_object_add_weak_pointer(), this weak
 * reference is thread-safe: converting a weak pointer to a reference is
 * atomic with respect to invalidation of weak pointers to destroyed
 * objects.
 *
 * If the object's #GObjectClass.dispose method results in additional
 * references to the object being held, any #GWeakRefs taken
 * before it was disposed will continue to point to %NULL.  If
 * #GWeakRefs are taken after the object is disposed and
 * re-referenced, they will continue to point to it until its refcount
 * goes back to zero, at which point they too will be invalidated.
 */

/**
 * g_weak_ref_init: (skip)
 * @weak_ref: (inout): uninitialized or empty location for a weak
 *    reference
 * @object: (type GObject.Object) (nullable): a #GObject or %NULL
 *
 * Initialise a non-statically-allocated #GWeakRef.
 *
 * This function also calls g_weak_ref_set() with @object on the
 * freshly-initialised weak reference.
 *
 * This function should always be matched with a call to
 * g_weak_ref_clear().  It is not necessary to use this function for a
 * #GWeakRef in static storage because it will already be
 * properly initialised.  Just use g_weak_ref_set() directly.
 *
 * Since: 2.32
 */
void
g_weak_ref_init (GWeakRef *weak_ref,
                 gpointer  object)
{
  weak_ref->priv.p = NULL;

  g_weak_ref_set (weak_ref, object);
}

/**
 * g_weak_ref_clear: (skip)
 * @weak_ref: (inout): location of a weak reference, which
 *  may be empty
 *
 * Frees resources associated with a non-statically-allocated #GWeakRef.
 * After this call, the #GWeakRef is left in an undefined state.
 *
 * You should only call this on a #GWeakRef that previously had
 * g_weak_ref_init() called on it.
 *
 * Since: 2.32
 */
void
g_weak_ref_clear (GWeakRef *weak_ref)
{
  g_weak_ref_set (weak_ref, NULL);

  /* be unkind */
  weak_ref->priv.p = (void *) 0xccccccccu;
}

/**
 * g_weak_ref_get: (skip)
 * @weak_ref: (inout): location of a weak reference to a #GObject
 *
 * If @weak_ref is not empty, atomically acquire a strong
 * reference to the object it points to, and return that reference.
 *
 * This function is needed because of the potential race between taking
 * the pointer value and g_object_ref() on it, if the object was losing
 * its last reference at the same time in a different thread.
 *
 * The caller should release the resulting reference in the usual way,
 * by using g_object_unref().
 *
 * Returns: (transfer full) (type GObject.Object): the object pointed to
 *     by @weak_ref, or %NULL if it was empty
 *
 * Since: 2.32
 */
gpointer
g_weak_ref_get (GWeakRef *weak_ref)
{
  gpointer object_or_null;

  g_return_val_if_fail (weak_ref!= NULL, NULL);

  g_rw_lock_reader_lock (&weak_locations_lock);

  object_or_null = weak_ref->priv.p;

  if (object_or_null != NULL)
    g_object_ref (object_or_null);

  g_rw_lock_reader_unlock (&weak_locations_lock);

  return object_or_null;
}

/**
 * g_weak_ref_set: (skip)
 * @weak_ref: location for a weak reference
 * @object: (type GObject.Object) (nullable): a #GObject or %NULL
 *
 * Change the object to which @weak_ref points, or set it to
 * %NULL.
 *
 * You must own a strong reference on @object while calling this
 * function.
 *
 * Since: 2.32
 */
void
g_weak_ref_set (GWeakRef *weak_ref,
                gpointer  object)
{
  GSList **weak_locations;
  GObject *new_object;
  GObject *old_object;

  g_return_if_fail (weak_ref != NULL);
  g_return_if_fail (object == NULL || G_IS_OBJECT (object));

  new_object = object;

  g_rw_lock_writer_lock (&weak_locations_lock);

  /* We use the extra level of indirection here so that if we have ever
   * had a weak pointer installed at any point in time on this object,
   * we can see that there is a non-NULL value associated with the
   * weak-pointer quark and know that this value will not change at any
   * point in the object's lifetime.
   *
   * Both properties are important for reducing the amount of times we
   * need to acquire locks and for decreasing the duration of time the
   * lock is held while avoiding some rather tricky races.
   *
   * Specifically: we can avoid having to do an extra unconditional lock
   * in g_object_unref() without worrying about some extremely tricky
   * races.
   */

  old_object = weak_ref->priv.p;
  if (new_object != old_object)
    {
      weak_ref->priv.p = new_object;

      /* Remove the weak ref from the old object */
      if (old_object != NULL)
        {
          weak_locations = g_datalist_id_get_data (&old_object->qdata, quark_weak_locations);
          /* for it to point to an object, the object must have had it added once */
          g_assert (weak_locations != NULL);

          *weak_locations = g_slist_remove (*weak_locations, weak_ref);
        }

      /* Add the weak ref to the new object */
      if (new_object != NULL)
        {
          weak_locations = g_datalist_id_get_data (&new_object->qdata, quark_weak_locations);

          if (weak_locations == NULL)
            {
              weak_locations = g_new0 (GSList *, 1);
              g_datalist_id_set_data_full (&new_object->qdata, quark_weak_locations, weak_locations, g_free);
            }

          *weak_locations = g_slist_prepend (*weak_locations, weak_ref);
        }
    }

  g_rw_lock_writer_unlock (&weak_locations_lock);
}
