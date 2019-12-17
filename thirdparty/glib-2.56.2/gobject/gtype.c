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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

/*
 * MT safe
 */

#include "config.h"

#include "../glib/valgrind.h"
#include <string.h>

#include "gtype.h"
#include "gtype-private.h"
#include "gtypeplugin.h"
#include "gvaluecollector.h"
#include "gatomicarray.h"
#include "gobject_trace.h"

#include "glib-private.h"
#include "gconstructor.h"

#ifdef G_OS_WIN32
#include <windows.h>
#endif

#ifdef	G_ENABLE_DEBUG
#define	IF_DEBUG(debug_type)	if (_g_type_debug_flags & G_TYPE_DEBUG_ ## debug_type)
#endif

/**
 * SECTION:gtype
 * @short_description: The GLib Runtime type identification and
 *     management system
 * @title:Type Information
 *
 * The GType API is the foundation of the GObject system.  It provides the
 * facilities for registering and managing all fundamental data types,
 * user-defined object and interface types.
 *
 * For type creation and registration purposes, all types fall into one of
 * two categories: static or dynamic.  Static types are never loaded or
 * unloaded at run-time as dynamic types may be.  Static types are created
 * with g_type_register_static() that gets type specific information passed
 * in via a #GTypeInfo structure.
 *
 * Dynamic types are created with g_type_register_dynamic() which takes a
 * #GTypePlugin structure instead. The remaining type information (the
 * #GTypeInfo structure) is retrieved during runtime through #GTypePlugin
 * and the g_type_plugin_*() API.
 *
 * These registration functions are usually called only once from a
 * function whose only purpose is to return the type identifier for a
 * specific class.  Once the type (or class or interface) is registered,
 * it may be instantiated, inherited, or implemented depending on exactly
 * what sort of type it is.
 *
 * There is also a third registration function for registering fundamental
 * types called g_type_register_fundamental() which requires both a #GTypeInfo
 * structure and a #GTypeFundamentalInfo structure but it is seldom used
 * since most fundamental types are predefined rather than user-defined.
 *
 * Type instance and class structs are limited to a total of 64 KiB,
 * including all parent types. Similarly, type instances' private data
 * (as created by g_type_class_add_private()) are limited to a total of
 * 64 KiB. If a type instance needs a large static buffer, allocate it
 * separately (typically by using #GArray or #GPtrArray) and put a pointer
 * to the buffer in the structure.
 *
 * As mentioned in the [GType conventions][gtype-conventions], type names must
 * be at least three characters long. There is no upper length limit. The first
 * character must be a letter (a–z or A–Z) or an underscore (‘_’). Subsequent
 * characters can be letters, numbers or any of ‘-_+’.
 */


/* NOTE: some functions (some internal variants and exported ones)
 * invalidate data portions of the TypeNodes. if external functions/callbacks
 * are called, pointers to memory maintained by TypeNodes have to be looked up
 * again. this affects most of the struct TypeNode fields, e.g. ->children or
 * CLASSED_NODE_IFACES_ENTRIES() respectively IFACE_NODE_PREREQUISITES() (but
 * not ->supers[]), as all those memory portions can get realloc()ed during
 * callback invocation.
 *
 * LOCKING:
 * lock handling issues when calling static functions are indicated by
 * uppercase letter postfixes, all static functions have to have
 * one of the below postfixes:
 * - _I:	[Indifferent about locking]
 *   function doesn't care about locks at all
 * - _U:	[Unlocked invocation]
 *   no read or write lock has to be held across function invocation
 *   (locks may be acquired and released during invocation though)
 * - _L:	[Locked invocation]
 *   a write lock or more than 0 read locks have to be held across
 *   function invocation
 * - _W:	[Write-locked invocation]
 *   a write lock has to be held across function invocation
 * - _Wm:	[Write-locked invocation, mutatable]
 *   like _W, but the write lock might be released and reacquired
 *   during invocation, watch your pointers
 * - _WmREC:    [Write-locked invocation, mutatable, recursive]
 *   like _Wm, but also acquires recursive mutex class_init_rec_mutex
 */

#ifdef LOCK_DEBUG
#define G_READ_LOCK(rw_lock)    do { g_printerr (G_STRLOC ": readL++\n"); g_rw_lock_reader_lock (rw_lock); } while (0)
#define G_READ_UNLOCK(rw_lock)  do { g_printerr (G_STRLOC ": readL--\n"); g_rw_lock_reader_unlock (rw_lock); } while (0)
#define G_WRITE_LOCK(rw_lock)   do { g_printerr (G_STRLOC ": writeL++\n"); g_rw_lock_writer_lock (rw_lock); } while (0)
#define G_WRITE_UNLOCK(rw_lock) do { g_printerr (G_STRLOC ": writeL--\n"); g_rw_lock_writer_unlock (rw_lock); } while (0)
#else
#define G_READ_LOCK(rw_lock)    g_rw_lock_reader_lock (rw_lock)
#define G_READ_UNLOCK(rw_lock)  g_rw_lock_reader_unlock (rw_lock)
#define G_WRITE_LOCK(rw_lock)   g_rw_lock_writer_lock (rw_lock)
#define G_WRITE_UNLOCK(rw_lock) g_rw_lock_writer_unlock (rw_lock)
#endif
#define	INVALID_RECURSION(func, arg, type_name) G_STMT_START{ \
    static const gchar _action[] = " invalidly modified type ";  \
    gpointer _arg = (gpointer) (arg); const gchar *_tname = (type_name), *_fname = (func); \
    if (_arg) \
      g_error ("%s(%p)%s'%s'", _fname, _arg, _action, _tname); \
    else \
      g_error ("%s()%s'%s'", _fname, _action, _tname); \
}G_STMT_END
#define g_assert_type_system_initialized() \
  g_assert (static_quark_type_flags)

#define TYPE_FUNDAMENTAL_FLAG_MASK (G_TYPE_FLAG_CLASSED | \
				    G_TYPE_FLAG_INSTANTIATABLE | \
				    G_TYPE_FLAG_DERIVABLE | \
				    G_TYPE_FLAG_DEEP_DERIVABLE)
#define	TYPE_FLAG_MASK		   (G_TYPE_FLAG_ABSTRACT | G_TYPE_FLAG_VALUE_ABSTRACT)
#define	SIZEOF_FUNDAMENTAL_INFO	   ((gssize) MAX (MAX (sizeof (GTypeFundamentalInfo), \
						       sizeof (gpointer)), \
                                                  sizeof (glong)))

/* The 2*sizeof(size_t) alignment here is borrowed from
 * GNU libc, so it should be good most everywhere.
 * It is more conservative than is needed on some 64-bit
 * platforms, but ia64 does require a 16-byte alignment.
 * The SIMD extensions for x86 and ppc32 would want a
 * larger alignment than this, but we don't need to
 * do better than malloc.
 */
#define STRUCT_ALIGNMENT (2 * sizeof (gsize))
#define ALIGN_STRUCT(offset) \
      ((offset + (STRUCT_ALIGNMENT - 1)) & -STRUCT_ALIGNMENT)


/* --- typedefs --- */
typedef struct _TypeNode        TypeNode;
typedef struct _CommonData      CommonData;
typedef struct _BoxedData       BoxedData;
typedef struct _IFaceData       IFaceData;
typedef struct _ClassData       ClassData;
typedef struct _InstanceData    InstanceData;
typedef union  _TypeData        TypeData;
typedef struct _IFaceEntries    IFaceEntries;
typedef struct _IFaceEntry      IFaceEntry;
typedef struct _IFaceHolder	IFaceHolder;


/* --- prototypes --- */
static inline GTypeFundamentalInfo*	type_node_fundamental_info_I	(TypeNode		*node);
static	      void			type_add_flags_W		(TypeNode		*node,
									 GTypeFlags		 flags);
static	      void			type_data_make_W		(TypeNode		*node,
									 const GTypeInfo	*info,
									 const GTypeValueTable	*value_table);
static inline void			type_data_ref_Wm		(TypeNode		*node);
static inline void			type_data_unref_U               (TypeNode		*node,
									 gboolean		 uncached);
static void				type_data_last_unref_Wm		(TypeNode *              node,
									 gboolean		 uncached);
static inline gpointer			type_get_qdata_L		(TypeNode		*node,
									 GQuark			 quark);
static inline void			type_set_qdata_W		(TypeNode		*node,
									 GQuark			 quark,
									 gpointer		 data);
static IFaceHolder*			type_iface_peek_holder_L	(TypeNode		*iface,
									 GType			 instance_type);
static gboolean                         type_iface_vtable_base_init_Wm  (TypeNode               *iface,
                                                                         TypeNode               *node);
static void                             type_iface_vtable_iface_init_Wm (TypeNode               *iface,
                                                                         TypeNode               *node);
static gboolean				type_node_is_a_L		(TypeNode		*node,
									 TypeNode		*iface_node);


/* --- enumeration --- */

/* The InitState enumeration is used to track the progress of initializing
 * both classes and interface vtables. Keeping the state of initialization
 * is necessary to handle new interfaces being added while we are initializing
 * the class or other interfaces.
 */
typedef enum
{
  UNINITIALIZED,
  BASE_CLASS_INIT,
  BASE_IFACE_INIT,
  CLASS_INIT,
  IFACE_INIT,
  INITIALIZED
} InitState;

/* --- structures --- */
struct _TypeNode
{
  guint volatile ref_count;
#ifdef G_ENABLE_DEBUG
  guint volatile instance_count;
#endif
  GTypePlugin *plugin;
  guint        n_children; /* writable with lock */
  guint        n_supers : 8;
  guint        n_prerequisites : 9;
  guint        is_classed : 1;
  guint        is_instantiatable : 1;
  guint        mutatable_check_cache : 1;	/* combines some common path checks */
  GType       *children; /* writable with lock */
  TypeData * volatile data;
  GQuark       qname;
  GData       *global_gdata;
  union {
    GAtomicArray iface_entries;		/* for !iface types */
    GAtomicArray offsets;
  } _prot;
  GType       *prerequisites;
  GType        supers[1]; /* flexible array */
};

#define SIZEOF_BASE_TYPE_NODE()			(G_STRUCT_OFFSET (TypeNode, supers))
#define MAX_N_SUPERS				(255)
#define MAX_N_CHILDREN				(G_MAXUINT)
#define	MAX_N_INTERFACES			(255) /* Limited by offsets being 8 bits */
#define	MAX_N_PREREQUISITES			(511)
#define NODE_TYPE(node)				(node->supers[0])
#define NODE_PARENT_TYPE(node)			(node->supers[1])
#define NODE_FUNDAMENTAL_TYPE(node)		(node->supers[node->n_supers])
#define NODE_NAME(node)				(g_quark_to_string (node->qname))
#define NODE_REFCOUNT(node)                     ((guint) g_atomic_int_get ((int *) &(node)->ref_count))
#define	NODE_IS_BOXED(node)			(NODE_FUNDAMENTAL_TYPE (node) == G_TYPE_BOXED)
#define	NODE_IS_IFACE(node)			(NODE_FUNDAMENTAL_TYPE (node) == G_TYPE_INTERFACE)
#define	CLASSED_NODE_IFACES_ENTRIES(node)	(&(node)->_prot.iface_entries)
#define	CLASSED_NODE_IFACES_ENTRIES_LOCKED(node)(G_ATOMIC_ARRAY_GET_LOCKED(CLASSED_NODE_IFACES_ENTRIES((node)), IFaceEntries))
#define	IFACE_NODE_N_PREREQUISITES(node)	((node)->n_prerequisites)
#define	IFACE_NODE_PREREQUISITES(node)		((node)->prerequisites)
#define	iface_node_get_holders_L(node)		((IFaceHolder*) type_get_qdata_L ((node), static_quark_iface_holder))
#define	iface_node_set_holders_W(node, holders)	(type_set_qdata_W ((node), static_quark_iface_holder, (holders)))
#define	iface_node_get_dependants_array_L(n)	((GType*) type_get_qdata_L ((n), static_quark_dependants_array))
#define	iface_node_set_dependants_array_W(n,d)	(type_set_qdata_W ((n), static_quark_dependants_array, (d)))
#define	TYPE_ID_MASK				((GType) ((1 << G_TYPE_FUNDAMENTAL_SHIFT) - 1))

#define NODE_IS_ANCESTOR(ancestor, node)                                                    \
        ((ancestor)->n_supers <= (node)->n_supers &&                                        \
	 (node)->supers[(node)->n_supers - (ancestor)->n_supers] == NODE_TYPE (ancestor))

struct _IFaceHolder
{
  GType           instance_type;
  GInterfaceInfo *info;
  GTypePlugin    *plugin;
  IFaceHolder    *next;
};

struct _IFaceEntry
{
  GType           iface_type;
  GTypeInterface *vtable;
  InitState       init_state;
};

struct _IFaceEntries {
  guint offset_index;
  IFaceEntry entry[1];
};

#define IFACE_ENTRIES_HEADER_SIZE (sizeof(IFaceEntries) - sizeof(IFaceEntry))
#define IFACE_ENTRIES_N_ENTRIES(_entries) ( (G_ATOMIC_ARRAY_DATA_SIZE((_entries)) - IFACE_ENTRIES_HEADER_SIZE) / sizeof(IFaceEntry) )

struct _CommonData
{
  GTypeValueTable  *value_table;
};

struct _BoxedData
{
  CommonData         data;
  GBoxedCopyFunc     copy_func;
  GBoxedFreeFunc     free_func;
};

struct _IFaceData
{
  CommonData         common;
  guint16            vtable_size;
  GBaseInitFunc      vtable_init_base;
  GBaseFinalizeFunc  vtable_finalize_base;
  GClassInitFunc     dflt_init;
  GClassFinalizeFunc dflt_finalize;
  gconstpointer      dflt_data;
  gpointer           dflt_vtable;
};

struct _ClassData
{
  CommonData         common;
  guint16            class_size;
  guint16            class_private_size;
  int volatile       init_state; /* atomic - g_type_class_ref reads it unlocked */
  GBaseInitFunc      class_init_base;
  GBaseFinalizeFunc  class_finalize_base;
  GClassInitFunc     class_init;
  GClassFinalizeFunc class_finalize;
  gconstpointer      class_data;
  gpointer           class;
};

struct _InstanceData
{
  CommonData         common;
  guint16            class_size;
  guint16            class_private_size;
  int volatile       init_state; /* atomic - g_type_class_ref reads it unlocked */
  GBaseInitFunc      class_init_base;
  GBaseFinalizeFunc  class_finalize_base;
  GClassInitFunc     class_init;
  GClassFinalizeFunc class_finalize;
  gconstpointer      class_data;
  gpointer           class;
  guint16            instance_size;
  guint16            private_size;
  guint16            n_preallocs;
  GInstanceInitFunc  instance_init;
};

union _TypeData
{
  CommonData         common;
  BoxedData          boxed;
  IFaceData          iface;
  ClassData          class;
  InstanceData       instance;
};

typedef struct {
  gpointer            cache_data;
  GTypeClassCacheFunc cache_func;
} ClassCacheFunc;

typedef struct {
  gpointer                check_data;
  GTypeInterfaceCheckFunc check_func;
} IFaceCheckFunc;


/* --- variables --- */
static GRWLock         type_rw_lock;
static GRecMutex       class_init_rec_mutex;
static guint           static_n_class_cache_funcs = 0;
static ClassCacheFunc *static_class_cache_funcs = NULL;
static guint           static_n_iface_check_funcs = 0;
static IFaceCheckFunc *static_iface_check_funcs = NULL;
static GQuark          static_quark_type_flags = 0;
static GQuark          static_quark_iface_holder = 0;
static GQuark          static_quark_dependants_array = 0;
static guint           type_registration_serial = 0;
GTypeDebugFlags	       _g_type_debug_flags = 0;

/* --- type nodes --- */
static GHashTable       *static_type_nodes_ht = NULL;
static TypeNode		*static_fundamental_type_nodes[(G_TYPE_FUNDAMENTAL_MAX >> G_TYPE_FUNDAMENTAL_SHIFT) + 1] = { NULL, };
static GType		 static_fundamental_next = G_TYPE_RESERVED_USER_FIRST;

static inline TypeNode*
lookup_type_node_I (GType utype)
{
  if (utype > G_TYPE_FUNDAMENTAL_MAX)
    return (TypeNode*) (utype & ~TYPE_ID_MASK);
  else
    return static_fundamental_type_nodes[utype >> G_TYPE_FUNDAMENTAL_SHIFT];
}

/**
 * g_type_get_type_registration_serial:
 *
 * Returns an opaque serial number that represents the state of the set
 * of registered types. Any time a type is registered this serial changes,
 * which means you can cache information based on type lookups (such as
 * g_type_from_name()) and know if the cache is still valid at a later
 * time by comparing the current serial with the one at the type lookup.
 *
 * Since: 2.36
 *
 * Returns: An unsigned int, representing the state of type registrations
 */
guint
g_type_get_type_registration_serial (void)
{
  return (guint)g_atomic_int_get ((gint *)&type_registration_serial);
}

static TypeNode*
type_node_any_new_W (TypeNode             *pnode,
		     GType                 ftype,
		     const gchar          *name,
		     GTypePlugin          *plugin,
		     GTypeFundamentalFlags type_flags)
{
  guint n_supers;
  GType type;
  TypeNode *node;
  guint i, node_size = 0;

  n_supers = pnode ? pnode->n_supers + 1 : 0;
  
  if (!pnode)
    node_size += SIZEOF_FUNDAMENTAL_INFO;	      /* fundamental type info */
  node_size += SIZEOF_BASE_TYPE_NODE ();	      /* TypeNode structure */
  node_size += (sizeof (GType) * (1 + n_supers + 1)); /* self + ancestors + (0) for ->supers[] */
  node = g_malloc0 (node_size);
  if (!pnode)					      /* offset fundamental types */
    {
      node = G_STRUCT_MEMBER_P (node, SIZEOF_FUNDAMENTAL_INFO);
      static_fundamental_type_nodes[ftype >> G_TYPE_FUNDAMENTAL_SHIFT] = node;
      type = ftype;
    }
  else
    type = (GType) node;
  
  g_assert ((type & TYPE_ID_MASK) == 0);
  
  node->n_supers = n_supers;
  if (!pnode)
    {
      node->supers[0] = type;
      node->supers[1] = 0;
      
      node->is_classed = (type_flags & G_TYPE_FLAG_CLASSED) != 0;
      node->is_instantiatable = (type_flags & G_TYPE_FLAG_INSTANTIATABLE) != 0;
      
      if (NODE_IS_IFACE (node))
	{
          IFACE_NODE_N_PREREQUISITES (node) = 0;
	  IFACE_NODE_PREREQUISITES (node) = NULL;
	}
      else
	_g_atomic_array_init (CLASSED_NODE_IFACES_ENTRIES (node));
    }
  else
    {
      node->supers[0] = type;
      memcpy (node->supers + 1, pnode->supers, sizeof (GType) * (1 + pnode->n_supers + 1));
      
      node->is_classed = pnode->is_classed;
      node->is_instantiatable = pnode->is_instantiatable;
      
      if (NODE_IS_IFACE (node))
	{
	  IFACE_NODE_N_PREREQUISITES (node) = 0;
	  IFACE_NODE_PREREQUISITES (node) = NULL;
	}
      else
	{
	  guint j;
	  IFaceEntries *entries;

	  entries = _g_atomic_array_copy (CLASSED_NODE_IFACES_ENTRIES (pnode),
					  IFACE_ENTRIES_HEADER_SIZE,
					  0);
	  if (entries)
	    {
	      for (j = 0; j < IFACE_ENTRIES_N_ENTRIES (entries); j++)
		{
		  entries->entry[j].vtable = NULL;
		  entries->entry[j].init_state = UNINITIALIZED;
		}
	      _g_atomic_array_update (CLASSED_NODE_IFACES_ENTRIES (node),
				      entries);
	    }
	}

      i = pnode->n_children++;
      pnode->children = g_renew (GType, pnode->children, pnode->n_children);
      pnode->children[i] = type;
    }

  TRACE(GOBJECT_TYPE_NEW(name, node->supers[1], type));

  node->plugin = plugin;
  node->n_children = 0;
  node->children = NULL;
  node->data = NULL;
  node->qname = g_quark_from_string (name);
  node->global_gdata = NULL;
  g_hash_table_insert (static_type_nodes_ht,
		       (gpointer) g_quark_to_string (node->qname),
		       (gpointer) type);

  g_atomic_int_inc ((gint *)&type_registration_serial);

  return node;
}

static inline GTypeFundamentalInfo*
type_node_fundamental_info_I (TypeNode *node)
{
  GType ftype = NODE_FUNDAMENTAL_TYPE (node);
  
  if (ftype != NODE_TYPE (node))
    node = lookup_type_node_I (ftype);
  
  return node ? G_STRUCT_MEMBER_P (node, -SIZEOF_FUNDAMENTAL_INFO) : NULL;
}

static TypeNode*
type_node_fundamental_new_W (GType                 ftype,
			     const gchar          *name,
			     GTypeFundamentalFlags type_flags)
{
  GTypeFundamentalInfo *finfo;
  TypeNode *node;
  
  g_assert ((ftype & TYPE_ID_MASK) == 0);
  g_assert (ftype <= G_TYPE_FUNDAMENTAL_MAX);
  
  if (ftype >> G_TYPE_FUNDAMENTAL_SHIFT == static_fundamental_next)
    static_fundamental_next++;
  
  type_flags &= TYPE_FUNDAMENTAL_FLAG_MASK;
  
  node = type_node_any_new_W (NULL, ftype, name, NULL, type_flags);
  
  finfo = type_node_fundamental_info_I (node);
  finfo->type_flags = type_flags;
  
  return node;
}

static TypeNode*
type_node_new_W (TypeNode    *pnode,
		 const gchar *name,
		 GTypePlugin *plugin)
     
{
  g_assert (pnode);
  g_assert (pnode->n_supers < MAX_N_SUPERS);
  g_assert (pnode->n_children < MAX_N_CHILDREN);
  
  return type_node_any_new_W (pnode, NODE_FUNDAMENTAL_TYPE (pnode), name, plugin, 0);
}

static inline IFaceEntry*
lookup_iface_entry_I (volatile IFaceEntries *entries,
		      TypeNode *iface_node)
{
  guint8 *offsets;
  guint offset_index;
  IFaceEntry *check;
  int index;
  IFaceEntry *entry;

  if (entries == NULL)
    return NULL;

  G_ATOMIC_ARRAY_DO_TRANSACTION
    (&iface_node->_prot.offsets, guint8,

     entry = NULL;
     offsets = transaction_data;
     offset_index = entries->offset_index;
     if (offsets != NULL &&
	 offset_index < G_ATOMIC_ARRAY_DATA_SIZE(offsets))
       {
	 index = offsets[offset_index];
	 if (index > 0)
	   {
	     /* zero means unset, subtract one to get real index */
	     index -= 1;

	     if (index < IFACE_ENTRIES_N_ENTRIES (entries))
	       {
		 check = (IFaceEntry *)&entries->entry[index];
		 if (check->iface_type == NODE_TYPE (iface_node))
		   entry = check;
	       }
	   }
       }
     );

 return entry;
}

static inline IFaceEntry*
type_lookup_iface_entry_L (TypeNode *node,
			   TypeNode *iface_node)
{
  if (!NODE_IS_IFACE (iface_node))
    return NULL;

  return lookup_iface_entry_I (CLASSED_NODE_IFACES_ENTRIES_LOCKED (node),
			       iface_node);
}


static inline gboolean
type_lookup_iface_vtable_I (TypeNode *node,
			    TypeNode *iface_node,
			    gpointer *vtable_ptr)
{
  IFaceEntry *entry;
  gboolean res;

  if (!NODE_IS_IFACE (iface_node))
    {
      if (vtable_ptr)
	*vtable_ptr = NULL;
      return FALSE;
    }

  G_ATOMIC_ARRAY_DO_TRANSACTION
    (CLASSED_NODE_IFACES_ENTRIES (node), IFaceEntries,

     entry = lookup_iface_entry_I (transaction_data, iface_node);
     res = entry != NULL;
     if (vtable_ptr)
       {
	 if (entry)
	   *vtable_ptr = entry->vtable;
	 else
	   *vtable_ptr = NULL;
       }
     );

  return res;
}

static inline gboolean
type_lookup_prerequisite_L (TypeNode *iface,
			    GType     prerequisite_type)
{
  if (NODE_IS_IFACE (iface) && IFACE_NODE_N_PREREQUISITES (iface))
    {
      GType *prerequisites = IFACE_NODE_PREREQUISITES (iface) - 1;
      guint n_prerequisites = IFACE_NODE_N_PREREQUISITES (iface);
      
      do
	{
	  guint i;
	  GType *check;
	  
	  i = (n_prerequisites + 1) >> 1;
	  check = prerequisites + i;
	  if (prerequisite_type == *check)
	    return TRUE;
	  else if (prerequisite_type > *check)
	    {
	      n_prerequisites -= i;
	      prerequisites = check;
	    }
	  else /* if (prerequisite_type < *check) */
	    n_prerequisites = i - 1;
	}
      while (n_prerequisites);
    }
  return FALSE;
}

static const gchar*
type_descriptive_name_I (GType type)
{
  if (type)
    {
      TypeNode *node = lookup_type_node_I (type);
      
      return node ? NODE_NAME (node) : "<unknown>";
    }
  else
    return "<invalid>";
}


/* --- type consistency checks --- */
static gboolean
check_plugin_U (GTypePlugin *plugin,
		gboolean     need_complete_type_info,
		gboolean     need_complete_interface_info,
		const gchar *type_name)
{
  /* G_IS_TYPE_PLUGIN() and G_TYPE_PLUGIN_GET_CLASS() are external calls: _U 
   */
  if (!plugin)
    {
      g_warning ("plugin handle for type '%s' is NULL",
		 type_name);
      return FALSE;
    }
  if (!G_IS_TYPE_PLUGIN (plugin))
    {
      g_warning ("plugin pointer (%p) for type '%s' is invalid",
		 plugin, type_name);
      return FALSE;
    }
  if (need_complete_type_info && !G_TYPE_PLUGIN_GET_CLASS (plugin)->complete_type_info)
    {
      g_warning ("plugin for type '%s' has no complete_type_info() implementation",
		 type_name);
      return FALSE;
    }
  if (need_complete_interface_info && !G_TYPE_PLUGIN_GET_CLASS (plugin)->complete_interface_info)
    {
      g_warning ("plugin for type '%s' has no complete_interface_info() implementation",
		 type_name);
      return FALSE;
    }
  return TRUE;
}

static gboolean
check_type_name_I (const gchar *type_name)
{
  static const gchar extra_chars[] = "-_+";
  const gchar *p = type_name;
  gboolean name_valid;
  
  if (!type_name[0] || !type_name[1] || !type_name[2])
    {
      g_warning ("type name '%s' is too short", type_name);
      return FALSE;
    }
  /* check the first letter */
  name_valid = (p[0] >= 'A' && p[0] <= 'Z') || (p[0] >= 'a' && p[0] <= 'z') || p[0] == '_';
  for (p = type_name + 1; *p; p++)
    name_valid &= ((p[0] >= 'A' && p[0] <= 'Z') ||
		   (p[0] >= 'a' && p[0] <= 'z') ||
		   (p[0] >= '0' && p[0] <= '9') ||
		   strchr (extra_chars, p[0]));
  if (!name_valid)
    {
      g_warning ("type name '%s' contains invalid characters", type_name);
      return FALSE;
    }
  if (g_type_from_name (type_name))
    {
      g_warning ("cannot register existing type '%s'", type_name);
      return FALSE;
    }
  
  return TRUE;
}

static gboolean
check_derivation_I (GType        parent_type,
		    const gchar *type_name)
{
  TypeNode *pnode;
  GTypeFundamentalInfo* finfo;
  
  pnode = lookup_type_node_I (parent_type);
  if (!pnode)
    {
      g_warning ("cannot derive type '%s' from invalid parent type '%s'",
		 type_name,
		 type_descriptive_name_I (parent_type));
      return FALSE;
    }
  finfo = type_node_fundamental_info_I (pnode);
  /* ensure flat derivability */
  if (!(finfo->type_flags & G_TYPE_FLAG_DERIVABLE))
    {
      g_warning ("cannot derive '%s' from non-derivable parent type '%s'",
		 type_name,
		 NODE_NAME (pnode));
      return FALSE;
    }
  /* ensure deep derivability */
  if (parent_type != NODE_FUNDAMENTAL_TYPE (pnode) &&
      !(finfo->type_flags & G_TYPE_FLAG_DEEP_DERIVABLE))
    {
      g_warning ("cannot derive '%s' from non-fundamental parent type '%s'",
		 type_name,
		 NODE_NAME (pnode));
      return FALSE;
    }
  
  return TRUE;
}

static gboolean
check_collect_format_I (const gchar *collect_format)
{
  const gchar *p = collect_format;
  gchar valid_format[] = { G_VALUE_COLLECT_INT, G_VALUE_COLLECT_LONG,
			   G_VALUE_COLLECT_INT64, G_VALUE_COLLECT_DOUBLE,
			   G_VALUE_COLLECT_POINTER, 0 };
  
  while (*p)
    if (!strchr (valid_format, *p++))
      return FALSE;
  return p - collect_format <= G_VALUE_COLLECT_FORMAT_MAX_LENGTH;
}

static gboolean
check_value_table_I (const gchar           *type_name,
		     const GTypeValueTable *value_table)
{
  if (!value_table)
    return FALSE;
  else if (value_table->value_init == NULL)
    {
      if (value_table->value_free || value_table->value_copy ||
	  value_table->value_peek_pointer ||
	  value_table->collect_format || value_table->collect_value ||
	  value_table->lcopy_format || value_table->lcopy_value)
	g_warning ("cannot handle uninitializable values of type '%s'",
		   type_name);
      return FALSE;
    }
  else /* value_table->value_init != NULL */
    {
      if (!value_table->value_free)
	{
	  /* +++ optional +++
	   * g_warning ("missing 'value_free()' for type '%s'", type_name);
	   * return FALSE;
	   */
	}
      if (!value_table->value_copy)
	{
	  g_warning ("missing 'value_copy()' for type '%s'", type_name);
	  return FALSE;
	}
      if ((value_table->collect_format || value_table->collect_value) &&
	  (!value_table->collect_format || !value_table->collect_value))
	{
	  g_warning ("one of 'collect_format' and 'collect_value()' is unspecified for type '%s'",
		     type_name);
	  return FALSE;
	}
      if (value_table->collect_format && !check_collect_format_I (value_table->collect_format))
	{
	  g_warning ("the '%s' specification for type '%s' is too long or invalid",
		     "collect_format",
		     type_name);
	  return FALSE;
	}
      if ((value_table->lcopy_format || value_table->lcopy_value) &&
	  (!value_table->lcopy_format || !value_table->lcopy_value))
	{
	  g_warning ("one of 'lcopy_format' and 'lcopy_value()' is unspecified for type '%s'",
		     type_name);
	  return FALSE;
	}
      if (value_table->lcopy_format && !check_collect_format_I (value_table->lcopy_format))
	{
	  g_warning ("the '%s' specification for type '%s' is too long or invalid",
		     "lcopy_format",
		     type_name);
	  return FALSE;
	}
    }
  return TRUE;
}

static gboolean
check_type_info_I (TypeNode        *pnode,
		   GType            ftype,
		   const gchar     *type_name,
		   const GTypeInfo *info)
{
  GTypeFundamentalInfo *finfo = type_node_fundamental_info_I (lookup_type_node_I (ftype));
  gboolean is_interface = ftype == G_TYPE_INTERFACE;
  
  g_assert (ftype <= G_TYPE_FUNDAMENTAL_MAX && !(ftype & TYPE_ID_MASK));
  
  /* check instance members */
  if (!(finfo->type_flags & G_TYPE_FLAG_INSTANTIATABLE) &&
      (info->instance_size || info->n_preallocs || info->instance_init))
    {
      if (pnode)
	g_warning ("cannot instantiate '%s', derived from non-instantiatable parent type '%s'",
		   type_name,
		   NODE_NAME (pnode));
      else
	g_warning ("cannot instantiate '%s' as non-instantiatable fundamental",
		   type_name);
      return FALSE;
    }
  /* check class & interface members */
  if (!((finfo->type_flags & G_TYPE_FLAG_CLASSED) || is_interface) &&
      (info->class_init || info->class_finalize || info->class_data ||
       info->class_size || info->base_init || info->base_finalize))
    {
      if (pnode)
	g_warning ("cannot create class for '%s', derived from non-classed parent type '%s'",
		   type_name,
                   NODE_NAME (pnode));
      else
	g_warning ("cannot create class for '%s' as non-classed fundamental",
		   type_name);
      return FALSE;
    }
  /* check interface size */
  if (is_interface && info->class_size < sizeof (GTypeInterface))
    {
      g_warning ("specified interface size for type '%s' is smaller than 'GTypeInterface' size",
		 type_name);
      return FALSE;
    }
  /* check class size */
  if (finfo->type_flags & G_TYPE_FLAG_CLASSED)
    {
      if (info->class_size < sizeof (GTypeClass))
	{
	  g_warning ("specified class size for type '%s' is smaller than 'GTypeClass' size",
		     type_name);
	  return FALSE;
	}
      if (pnode && info->class_size < pnode->data->class.class_size)
	{
	  g_warning ("specified class size for type '%s' is smaller "
		     "than the parent type's '%s' class size",
		     type_name,
		     NODE_NAME (pnode));
	  return FALSE;
	}
    }
  /* check instance size */
  if (finfo->type_flags & G_TYPE_FLAG_INSTANTIATABLE)
    {
      if (info->instance_size < sizeof (GTypeInstance))
	{
	  g_warning ("specified instance size for type '%s' is smaller than 'GTypeInstance' size",
		     type_name);
	  return FALSE;
	}
      if (pnode && info->instance_size < pnode->data->instance.instance_size)
	{
	  g_warning ("specified instance size for type '%s' is smaller "
		     "than the parent type's '%s' instance size",
		     type_name,
		     NODE_NAME (pnode));
	  return FALSE;
	}
    }
  
  return TRUE;
}

static TypeNode*
find_conforming_child_type_L (TypeNode *pnode,
			      TypeNode *iface)
{
  TypeNode *node = NULL;
  guint i;
  
  if (type_lookup_iface_entry_L (pnode, iface))
    return pnode;
  
  for (i = 0; i < pnode->n_children && !node; i++)
    node = find_conforming_child_type_L (lookup_type_node_I (pnode->children[i]), iface);
  
  return node;
}

static gboolean
check_add_interface_L (GType instance_type,
		       GType iface_type)
{
  TypeNode *node = lookup_type_node_I (instance_type);
  TypeNode *iface = lookup_type_node_I (iface_type);
  IFaceEntry *entry;
  TypeNode *tnode;
  GType *prerequisites;
  guint i;

  
  if (!node || !node->is_instantiatable)
    {
      g_warning ("cannot add interfaces to invalid (non-instantiatable) type '%s'",
		 type_descriptive_name_I (instance_type));
      return FALSE;
    }
  if (!iface || !NODE_IS_IFACE (iface))
    {
      g_warning ("cannot add invalid (non-interface) type '%s' to type '%s'",
		 type_descriptive_name_I (iface_type),
		 NODE_NAME (node));
      return FALSE;
    }
  if (node->data && node->data->class.class)
    {
      g_warning ("attempting to add an interface (%s) to class (%s) after class_init",
                 NODE_NAME (iface), NODE_NAME (node));
      return FALSE;
    }
  tnode = lookup_type_node_I (NODE_PARENT_TYPE (iface));
  if (NODE_PARENT_TYPE (tnode) && !type_lookup_iface_entry_L (node, tnode))
    {
      /* 2001/7/31:timj: erk, i guess this warning is junk as interface derivation is flat */
      g_warning ("cannot add sub-interface '%s' to type '%s' which does not conform to super-interface '%s'",
		 NODE_NAME (iface),
		 NODE_NAME (node),
		 NODE_NAME (tnode));
      return FALSE;
    }
  /* allow overriding of interface type introduced for parent type */
  entry = type_lookup_iface_entry_L (node, iface);
  if (entry && entry->vtable == NULL && !type_iface_peek_holder_L (iface, NODE_TYPE (node)))
    {
      /* ok, we do conform to this interface already, but the interface vtable was not
       * yet intialized, and we just conform to the interface because it got added to
       * one of our parents. so we allow overriding of holder info here.
       */
      return TRUE;
    }
  /* check whether one of our children already conforms (or whether the interface
   * got added to this node already)
   */
  tnode = find_conforming_child_type_L (node, iface);  /* tnode is_a node */
  if (tnode)
    {
      g_warning ("cannot add interface type '%s' to type '%s', since type '%s' already conforms to interface",
		 NODE_NAME (iface),
		 NODE_NAME (node),
		 NODE_NAME (tnode));
      return FALSE;
    }
  prerequisites = IFACE_NODE_PREREQUISITES (iface);
  for (i = 0; i < IFACE_NODE_N_PREREQUISITES (iface); i++)
    {
      tnode = lookup_type_node_I (prerequisites[i]);
      if (!type_node_is_a_L (node, tnode))
	{
	  g_warning ("cannot add interface type '%s' to type '%s' which does not conform to prerequisite '%s'",
		     NODE_NAME (iface),
		     NODE_NAME (node),
		     NODE_NAME (tnode));
	  return FALSE;
	}
    }
  return TRUE;
}

static gboolean
check_interface_info_I (TypeNode             *iface,
			GType                 instance_type,
			const GInterfaceInfo *info)
{
  if ((info->interface_finalize || info->interface_data) && !info->interface_init)
    {
      g_warning ("interface type '%s' for type '%s' comes without initializer",
		 NODE_NAME (iface),
		 type_descriptive_name_I (instance_type));
      return FALSE;
    }
  
  return TRUE;
}

/* --- type info (type node data) --- */
static void
type_data_make_W (TypeNode              *node,
		  const GTypeInfo       *info,
		  const GTypeValueTable *value_table)
{
  TypeData *data;
  GTypeValueTable *vtable = NULL;
  guint vtable_size = 0;
  
  g_assert (node->data == NULL && info != NULL);
  
  if (!value_table)
    {
      TypeNode *pnode = lookup_type_node_I (NODE_PARENT_TYPE (node));
      
      if (pnode)
	vtable = pnode->data->common.value_table;
      else
	{
	  static const GTypeValueTable zero_vtable = { NULL, };
	  
	  value_table = &zero_vtable;
	}
    }
  if (value_table)
    {
      /* need to setup vtable_size since we have to allocate it with data in one chunk */
      vtable_size = sizeof (GTypeValueTable);
      if (value_table->collect_format)
	vtable_size += strlen (value_table->collect_format);
      if (value_table->lcopy_format)
	vtable_size += strlen (value_table->lcopy_format);
      vtable_size += 2;
    }
   
  if (node->is_instantiatable) /* careful, is_instantiatable is also is_classed */
    {
      TypeNode *pnode = lookup_type_node_I (NODE_PARENT_TYPE (node));

      data = g_malloc0 (sizeof (InstanceData) + vtable_size);
      if (vtable_size)
	vtable = G_STRUCT_MEMBER_P (data, sizeof (InstanceData));
      data->instance.class_size = info->class_size;
      data->instance.class_init_base = info->base_init;
      data->instance.class_finalize_base = info->base_finalize;
      data->instance.class_init = info->class_init;
      data->instance.class_finalize = info->class_finalize;
      data->instance.class_data = info->class_data;
      data->instance.class = NULL;
      data->instance.init_state = UNINITIALIZED;
      data->instance.instance_size = info->instance_size;
      /* We'll set the final value for data->instance.private size
       * after the parent class has been initialized
       */
      data->instance.private_size = 0;
      data->instance.class_private_size = 0;
      if (pnode)
        data->instance.class_private_size = pnode->data->instance.class_private_size;
#ifdef	DISABLE_MEM_POOLS
      data->instance.n_preallocs = 0;
#else	/* !DISABLE_MEM_POOLS */
      data->instance.n_preallocs = MIN (info->n_preallocs, 1024);
#endif	/* !DISABLE_MEM_POOLS */
      data->instance.instance_init = info->instance_init;
    }
  else if (node->is_classed) /* only classed */
    {
      TypeNode *pnode = lookup_type_node_I (NODE_PARENT_TYPE (node));

      data = g_malloc0 (sizeof (ClassData) + vtable_size);
      if (vtable_size)
	vtable = G_STRUCT_MEMBER_P (data, sizeof (ClassData));
      data->class.class_size = info->class_size;
      data->class.class_init_base = info->base_init;
      data->class.class_finalize_base = info->base_finalize;
      data->class.class_init = info->class_init;
      data->class.class_finalize = info->class_finalize;
      data->class.class_data = info->class_data;
      data->class.class = NULL;
      data->class.class_private_size = 0;
      if (pnode)
        data->class.class_private_size = pnode->data->class.class_private_size;
      data->class.init_state = UNINITIALIZED;
    }
  else if (NODE_IS_IFACE (node))
    {
      data = g_malloc0 (sizeof (IFaceData) + vtable_size);
      if (vtable_size)
	vtable = G_STRUCT_MEMBER_P (data, sizeof (IFaceData));
      data->iface.vtable_size = info->class_size;
      data->iface.vtable_init_base = info->base_init;
      data->iface.vtable_finalize_base = info->base_finalize;
      data->iface.dflt_init = info->class_init;
      data->iface.dflt_finalize = info->class_finalize;
      data->iface.dflt_data = info->class_data;
      data->iface.dflt_vtable = NULL;
    }
  else if (NODE_IS_BOXED (node))
    {
      data = g_malloc0 (sizeof (BoxedData) + vtable_size);
      if (vtable_size)
	vtable = G_STRUCT_MEMBER_P (data, sizeof (BoxedData));
    }
  else
    {
      data = g_malloc0 (sizeof (CommonData) + vtable_size);
      if (vtable_size)
	vtable = G_STRUCT_MEMBER_P (data, sizeof (CommonData));
    }
  
  node->data = data;
  
  if (vtable_size)
    {
      gchar *p;
      
      /* we allocate the vtable and its strings together with the type data, so
       * children can take over their parent's vtable pointer, and we don't
       * need to worry freeing it or not when the child data is destroyed
       */
      *vtable = *value_table;
      p = G_STRUCT_MEMBER_P (vtable, sizeof (*vtable));
      p[0] = 0;
      vtable->collect_format = p;
      if (value_table->collect_format)
	{
	  strcat (p, value_table->collect_format);
	  p += strlen (value_table->collect_format);
	}
      p++;
      p[0] = 0;
      vtable->lcopy_format = p;
      if (value_table->lcopy_format)
	strcat  (p, value_table->lcopy_format);
    }
  node->data->common.value_table = vtable;
  node->mutatable_check_cache = (node->data->common.value_table->value_init != NULL &&
				 !((G_TYPE_FLAG_VALUE_ABSTRACT | G_TYPE_FLAG_ABSTRACT) &
				   GPOINTER_TO_UINT (type_get_qdata_L (node, static_quark_type_flags))));
  
  g_assert (node->data->common.value_table != NULL); /* paranoid */

  g_atomic_int_set ((int *) &node->ref_count, 1);
}

static inline void
type_data_ref_Wm (TypeNode *node)
{
  if (!node->data)
    {
      TypeNode *pnode = lookup_type_node_I (NODE_PARENT_TYPE (node));
      GTypeInfo tmp_info;
      GTypeValueTable tmp_value_table;
      
      g_assert (node->plugin != NULL);
      
      if (pnode)
	{
	  type_data_ref_Wm (pnode);
	  if (node->data)
	    INVALID_RECURSION ("g_type_plugin_*", node->plugin, NODE_NAME (node));
	}
      
      memset (&tmp_info, 0, sizeof (tmp_info));
      memset (&tmp_value_table, 0, sizeof (tmp_value_table));
      
      G_WRITE_UNLOCK (&type_rw_lock);
      g_type_plugin_use (node->plugin);
      g_type_plugin_complete_type_info (node->plugin, NODE_TYPE (node), &tmp_info, &tmp_value_table);
      G_WRITE_LOCK (&type_rw_lock);
      if (node->data)
	INVALID_RECURSION ("g_type_plugin_*", node->plugin, NODE_NAME (node));
      
      check_type_info_I (pnode, NODE_FUNDAMENTAL_TYPE (node), NODE_NAME (node), &tmp_info);
      type_data_make_W (node, &tmp_info,
			check_value_table_I (NODE_NAME (node),
					     &tmp_value_table) ? &tmp_value_table : NULL);
    }
  else
    {
      g_assert (NODE_REFCOUNT (node) > 0);
      
      g_atomic_int_inc ((int *) &node->ref_count);
    }
}

static inline gboolean
type_data_ref_U (TypeNode *node)
{
  guint current;

  do {
    current = NODE_REFCOUNT (node);

    if (current < 1)
      return FALSE;
  } while (!g_atomic_int_compare_and_exchange ((int *) &node->ref_count, current, current + 1));

  return TRUE;
}

static gboolean
iface_node_has_available_offset_L (TypeNode *iface_node,
				   int offset,
				   int for_index)
{
  guint8 *offsets;

  offsets = G_ATOMIC_ARRAY_GET_LOCKED (&iface_node->_prot.offsets, guint8);
  if (offsets == NULL)
    return TRUE;

  if (G_ATOMIC_ARRAY_DATA_SIZE (offsets) <= offset)
    return TRUE;

  if (offsets[offset] == 0 ||
      offsets[offset] == for_index+1)
    return TRUE;

  return FALSE;
}

static int
find_free_iface_offset_L (IFaceEntries *entries)
{
  IFaceEntry *entry;
  TypeNode *iface_node;
  int offset;
  int i;
  int n_entries;

  n_entries = IFACE_ENTRIES_N_ENTRIES (entries);
  offset = -1;
  do
    {
      offset++;
      for (i = 0; i < n_entries; i++)
	{
	  entry = &entries->entry[i];
	  iface_node = lookup_type_node_I (entry->iface_type);

	  if (!iface_node_has_available_offset_L (iface_node, offset, i))
	    break;
	}
    }
  while (i != n_entries);

  return offset;
}

static void
iface_node_set_offset_L (TypeNode *iface_node,
			 int offset,
			 int index)
{
  guint8 *offsets, *old_offsets;
  int new_size, old_size;
  int i;

  old_offsets = G_ATOMIC_ARRAY_GET_LOCKED (&iface_node->_prot.offsets, guint8);
  if (old_offsets == NULL)
    old_size = 0;
  else
    {
      old_size = G_ATOMIC_ARRAY_DATA_SIZE (old_offsets);
      if (offset < old_size &&
	  old_offsets[offset] == index + 1)
	return; /* Already set to this index, return */
    }
  new_size = MAX (old_size, offset + 1);

  offsets = _g_atomic_array_copy (&iface_node->_prot.offsets,
				  0, new_size - old_size);

  /* Mark new area as unused */
  for (i = old_size; i < new_size; i++)
    offsets[i] = 0;

  offsets[offset] = index + 1;

  _g_atomic_array_update (&iface_node->_prot.offsets, offsets);
}

static void
type_node_add_iface_entry_W (TypeNode   *node,
			     GType       iface_type,
                             IFaceEntry *parent_entry)
{
  IFaceEntries *entries;
  IFaceEntry *entry;
  TypeNode *iface_node;
  guint i, j;
  int num_entries;

  g_assert (node->is_instantiatable);

  entries = CLASSED_NODE_IFACES_ENTRIES_LOCKED (node);
  if (entries != NULL)
    {
      num_entries = IFACE_ENTRIES_N_ENTRIES (entries);

      g_assert (num_entries < MAX_N_INTERFACES);

      for (i = 0; i < num_entries; i++)
	{
	  entry = &entries->entry[i];
	  if (entry->iface_type == iface_type)
	    {
	      /* this can happen in two cases:
	       * - our parent type already conformed to iface_type and node
	       *   got its own holder info. here, our children already have
	       *   entries and NULL vtables, since this will only work for
	       *   uninitialized classes.
	       * - an interface type is added to an ancestor after it was
	       *   added to a child type.
	       */
	      if (!parent_entry)
		g_assert (entry->vtable == NULL && entry->init_state == UNINITIALIZED);
	      else
		{
		  /* sick, interface is added to ancestor *after* child type;
		   * nothing todo, the entry and our children were already setup correctly
		   */
		}
	      return;
	    }
	}
    }

  entries = _g_atomic_array_copy (CLASSED_NODE_IFACES_ENTRIES (node),
				  IFACE_ENTRIES_HEADER_SIZE,
				  sizeof (IFaceEntry));
  num_entries = IFACE_ENTRIES_N_ENTRIES (entries);
  i = num_entries - 1;
  if (i == 0)
    entries->offset_index = 0;
  entries->entry[i].iface_type = iface_type;
  entries->entry[i].vtable = NULL;
  entries->entry[i].init_state = UNINITIALIZED;

  if (parent_entry)
    {
      if (node->data && node->data->class.init_state >= BASE_IFACE_INIT)
        {
          entries->entry[i].init_state = INITIALIZED;
          entries->entry[i].vtable = parent_entry->vtable;
        }
    }

  /* Update offsets in iface */
  iface_node = lookup_type_node_I (iface_type);

  if (iface_node_has_available_offset_L (iface_node,
					 entries->offset_index,
					 i))
    {
      iface_node_set_offset_L (iface_node,
			       entries->offset_index, i);
    }
  else
   {
      entries->offset_index =
	find_free_iface_offset_L (entries);
      for (j = 0; j < IFACE_ENTRIES_N_ENTRIES (entries); j++)
	{
	  entry = &entries->entry[j];
	  iface_node =
	    lookup_type_node_I (entry->iface_type);
	  iface_node_set_offset_L (iface_node,
				   entries->offset_index, j);
	}
    }

  _g_atomic_array_update (CLASSED_NODE_IFACES_ENTRIES (node), entries);

  if (parent_entry)
    {
      for (i = 0; i < node->n_children; i++)
        type_node_add_iface_entry_W (lookup_type_node_I (node->children[i]), iface_type, &entries->entry[i]);
    }
}

static void
type_add_interface_Wm (TypeNode             *node,
                       TypeNode             *iface,
                       const GInterfaceInfo *info,
                       GTypePlugin          *plugin)
{
  IFaceHolder *iholder = g_new0 (IFaceHolder, 1);
  IFaceEntry *entry;
  guint i;

  g_assert (node->is_instantiatable && NODE_IS_IFACE (iface) && ((info && !plugin) || (!info && plugin)));
  
  iholder->next = iface_node_get_holders_L (iface);
  iface_node_set_holders_W (iface, iholder);
  iholder->instance_type = NODE_TYPE (node);
  iholder->info = info ? g_memdup (info, sizeof (*info)) : NULL;
  iholder->plugin = plugin;

  /* create an iface entry for this type */
  type_node_add_iface_entry_W (node, NODE_TYPE (iface), NULL);
  
  /* if the class is already (partly) initialized, we may need to base
   * initalize and/or initialize the new interface.
   */
  if (node->data)
    {
      InitState class_state = node->data->class.init_state;
      
      if (class_state >= BASE_IFACE_INIT)
        type_iface_vtable_base_init_Wm (iface, node);
      
      if (class_state >= IFACE_INIT)
        type_iface_vtable_iface_init_Wm (iface, node);
    }
  
  /* create iface entries for children of this type */
  entry = type_lookup_iface_entry_L (node, iface);
  for (i = 0; i < node->n_children; i++)
    type_node_add_iface_entry_W (lookup_type_node_I (node->children[i]), NODE_TYPE (iface), entry);
}

static void
type_iface_add_prerequisite_W (TypeNode *iface,
			       TypeNode *prerequisite_node)
{
  GType prerequisite_type = NODE_TYPE (prerequisite_node);
  GType *prerequisites, *dependants;
  guint n_dependants, i;
  
  g_assert (NODE_IS_IFACE (iface) &&
	    IFACE_NODE_N_PREREQUISITES (iface) < MAX_N_PREREQUISITES &&
	    (prerequisite_node->is_instantiatable || NODE_IS_IFACE (prerequisite_node)));
  
  prerequisites = IFACE_NODE_PREREQUISITES (iface);
  for (i = 0; i < IFACE_NODE_N_PREREQUISITES (iface); i++)
    if (prerequisites[i] == prerequisite_type)
      return;			/* we already have that prerequisiste */
    else if (prerequisites[i] > prerequisite_type)
      break;
  IFACE_NODE_N_PREREQUISITES (iface) += 1;
  IFACE_NODE_PREREQUISITES (iface) = g_renew (GType,
					      IFACE_NODE_PREREQUISITES (iface),
					      IFACE_NODE_N_PREREQUISITES (iface));
  prerequisites = IFACE_NODE_PREREQUISITES (iface);
  memmove (prerequisites + i + 1, prerequisites + i,
           sizeof (prerequisites[0]) * (IFACE_NODE_N_PREREQUISITES (iface) - i - 1));
  prerequisites[i] = prerequisite_type;
  
  /* we want to get notified when prerequisites get added to prerequisite_node */
  if (NODE_IS_IFACE (prerequisite_node))
    {
      dependants = iface_node_get_dependants_array_L (prerequisite_node);
      n_dependants = dependants ? dependants[0] : 0;
      n_dependants += 1;
      dependants = g_renew (GType, dependants, n_dependants + 1);
      dependants[n_dependants] = NODE_TYPE (iface);
      dependants[0] = n_dependants;
      iface_node_set_dependants_array_W (prerequisite_node, dependants);
    }
  
  /* we need to notify all dependants */
  dependants = iface_node_get_dependants_array_L (iface);
  n_dependants = dependants ? dependants[0] : 0;
  for (i = 1; i <= n_dependants; i++)
    type_iface_add_prerequisite_W (lookup_type_node_I (dependants[i]), prerequisite_node);
}

/**
 * g_type_interface_add_prerequisite:
 * @interface_type: #GType value of an interface type
 * @prerequisite_type: #GType value of an interface or instantiatable type
 *
 * Adds @prerequisite_type to the list of prerequisites of @interface_type.
 * This means that any type implementing @interface_type must also implement
 * @prerequisite_type. Prerequisites can be thought of as an alternative to
 * interface derivation (which GType doesn't support). An interface can have
 * at most one instantiatable prerequisite type.
 */
void
g_type_interface_add_prerequisite (GType interface_type,
				   GType prerequisite_type)
{
  TypeNode *iface, *prerequisite_node;
  IFaceHolder *holders;
  
  g_return_if_fail (G_TYPE_IS_INTERFACE (interface_type));	/* G_TYPE_IS_INTERFACE() is an external call: _U */
  g_return_if_fail (!g_type_is_a (interface_type, prerequisite_type));
  g_return_if_fail (!g_type_is_a (prerequisite_type, interface_type));
  
  iface = lookup_type_node_I (interface_type);
  prerequisite_node = lookup_type_node_I (prerequisite_type);
  if (!iface || !prerequisite_node || !NODE_IS_IFACE (iface))
    {
      g_warning ("interface type '%s' or prerequisite type '%s' invalid",
		 type_descriptive_name_I (interface_type),
		 type_descriptive_name_I (prerequisite_type));
      return;
    }
  G_WRITE_LOCK (&type_rw_lock);
  holders = iface_node_get_holders_L (iface);
  if (holders)
    {
      G_WRITE_UNLOCK (&type_rw_lock);
      g_warning ("unable to add prerequisite '%s' to interface '%s' which is already in use for '%s'",
		 type_descriptive_name_I (prerequisite_type),
		 type_descriptive_name_I (interface_type),
		 type_descriptive_name_I (holders->instance_type));
      return;
    }
  if (prerequisite_node->is_instantiatable)
    {
      guint i;
      
      /* can have at most one publicly installable instantiatable prerequisite */
      for (i = 0; i < IFACE_NODE_N_PREREQUISITES (iface); i++)
	{
	  TypeNode *prnode = lookup_type_node_I (IFACE_NODE_PREREQUISITES (iface)[i]);
	  
	  if (prnode->is_instantiatable)
	    {
	      G_WRITE_UNLOCK (&type_rw_lock);
	      g_warning ("adding prerequisite '%s' to interface '%s' conflicts with existing prerequisite '%s'",
			 type_descriptive_name_I (prerequisite_type),
			 type_descriptive_name_I (interface_type),
			 type_descriptive_name_I (NODE_TYPE (prnode)));
	      return;
	    }
	}
      
      for (i = 0; i < prerequisite_node->n_supers + 1; i++)
	type_iface_add_prerequisite_W (iface, lookup_type_node_I (prerequisite_node->supers[i]));
      G_WRITE_UNLOCK (&type_rw_lock);
    }
  else if (NODE_IS_IFACE (prerequisite_node))
    {
      GType *prerequisites;
      guint i;
      
      prerequisites = IFACE_NODE_PREREQUISITES (prerequisite_node);
      for (i = 0; i < IFACE_NODE_N_PREREQUISITES (prerequisite_node); i++)
	type_iface_add_prerequisite_W (iface, lookup_type_node_I (prerequisites[i]));
      type_iface_add_prerequisite_W (iface, prerequisite_node);
      G_WRITE_UNLOCK (&type_rw_lock);
    }
  else
    {
      G_WRITE_UNLOCK (&type_rw_lock);
      g_warning ("prerequisite '%s' for interface '%s' is neither instantiatable nor interface",
		 type_descriptive_name_I (prerequisite_type),
		 type_descriptive_name_I (interface_type));
    }
}

/**
 * g_type_interface_prerequisites:
 * @interface_type: an interface type
 * @n_prerequisites: (out) (optional): location to return the number
 *     of prerequisites, or %NULL
 *
 * Returns the prerequisites of an interfaces type.
 *
 * Since: 2.2
 *
 * Returns: (array length=n_prerequisites) (transfer full): a
 *     newly-allocated zero-terminated array of #GType containing
 *     the prerequisites of @interface_type
 */
GType*
g_type_interface_prerequisites (GType  interface_type,
				guint *n_prerequisites)
{
  TypeNode *iface;
  
  g_return_val_if_fail (G_TYPE_IS_INTERFACE (interface_type), NULL);

  iface = lookup_type_node_I (interface_type);
  if (iface)
    {
      GType *types;
      TypeNode *inode = NULL;
      guint i, n = 0;
      
      G_READ_LOCK (&type_rw_lock);
      types = g_new0 (GType, IFACE_NODE_N_PREREQUISITES (iface) + 1);
      for (i = 0; i < IFACE_NODE_N_PREREQUISITES (iface); i++)
	{
	  GType prerequisite = IFACE_NODE_PREREQUISITES (iface)[i];
	  TypeNode *node = lookup_type_node_I (prerequisite);
	  if (node->is_instantiatable)
            {
              if (!inode || type_node_is_a_L (node, inode))
	        inode = node;
            }
	  else
	    types[n++] = NODE_TYPE (node);
	}
      if (inode)
	types[n++] = NODE_TYPE (inode);
      
      if (n_prerequisites)
	*n_prerequisites = n;
      G_READ_UNLOCK (&type_rw_lock);
      
      return types;
    }
  else
    {
      if (n_prerequisites)
	*n_prerequisites = 0;
      
      return NULL;
    }
}


static IFaceHolder*
type_iface_peek_holder_L (TypeNode *iface,
			  GType     instance_type)
{
  IFaceHolder *iholder;
  
  g_assert (NODE_IS_IFACE (iface));
  
  iholder = iface_node_get_holders_L (iface);
  while (iholder && iholder->instance_type != instance_type)
    iholder = iholder->next;
  return iholder;
}

static IFaceHolder*
type_iface_retrieve_holder_info_Wm (TypeNode *iface,
				    GType     instance_type,
				    gboolean  need_info)
{
  IFaceHolder *iholder = type_iface_peek_holder_L (iface, instance_type);
  
  if (iholder && !iholder->info && need_info)
    {
      GInterfaceInfo tmp_info;
      
      g_assert (iholder->plugin != NULL);
      
      type_data_ref_Wm (iface);
      if (iholder->info)
	INVALID_RECURSION ("g_type_plugin_*", iface->plugin, NODE_NAME (iface));
      
      memset (&tmp_info, 0, sizeof (tmp_info));
      
      G_WRITE_UNLOCK (&type_rw_lock);
      g_type_plugin_use (iholder->plugin);
      g_type_plugin_complete_interface_info (iholder->plugin, instance_type, NODE_TYPE (iface), &tmp_info);
      G_WRITE_LOCK (&type_rw_lock);
      if (iholder->info)
        INVALID_RECURSION ("g_type_plugin_*", iholder->plugin, NODE_NAME (iface));
      
      check_interface_info_I (iface, instance_type, &tmp_info);
      iholder->info = g_memdup (&tmp_info, sizeof (tmp_info));
    }
  
  return iholder;	/* we don't modify write lock upon returning NULL */
}

static void
type_iface_blow_holder_info_Wm (TypeNode *iface,
				GType     instance_type)
{
  IFaceHolder *iholder = iface_node_get_holders_L (iface);
  
  g_assert (NODE_IS_IFACE (iface));
  
  while (iholder->instance_type != instance_type)
    iholder = iholder->next;
  
  if (iholder->info && iholder->plugin)
    {
      g_free (iholder->info);
      iholder->info = NULL;
      
      G_WRITE_UNLOCK (&type_rw_lock);
      g_type_plugin_unuse (iholder->plugin);
      type_data_unref_U (iface, FALSE);
      G_WRITE_LOCK (&type_rw_lock);
    }
}

/**
 * g_type_create_instance: (skip)
 * @type: an instantiatable type to create an instance for
 *
 * Creates and initializes an instance of @type if @type is valid and
 * can be instantiated. The type system only performs basic allocation
 * and structure setups for instances: actual instance creation should
 * happen through functions supplied by the type's fundamental type
 * implementation.  So use of g_type_create_instance() is reserved for
 * implementators of fundamental types only. E.g. instances of the
 * #GObject hierarchy should be created via g_object_new() and never
 * directly through g_type_create_instance() which doesn't handle things
 * like singleton objects or object construction.
 *
 * The extended members of the returned instance are guaranteed to be filled
 * with zeros.
 *
 * Note: Do not use this function, unless you're implementing a
 * fundamental type. Also language bindings should not use this
 * function, but g_object_new() instead.
 *
 * Returns: an allocated and initialized instance, subject to further
 *     treatment by the fundamental type implementation
 */
GTypeInstance*
g_type_create_instance (GType type)
{
  TypeNode *node;
  GTypeInstance *instance;
  GTypeClass *class;
  gchar *allocated;
  gint private_size;
  gint ivar_size;
  guint i;

  node = lookup_type_node_I (type);
  if (!node || !node->is_instantiatable)
    {
      g_error ("cannot create new instance of invalid (non-instantiatable) type '%s'",
		 type_descriptive_name_I (type));
    }
  /* G_TYPE_IS_ABSTRACT() is an external call: _U */
  if (!node->mutatable_check_cache && G_TYPE_IS_ABSTRACT (type))
    {
      g_error ("cannot create instance of abstract (non-instantiatable) type '%s'",
		 type_descriptive_name_I (type));
    }
  
  class = g_type_class_ref (type);

  /* We allocate the 'private' areas before the normal instance data, in
   * reverse order.  This allows the private area of a particular class
   * to always be at a constant relative address to the instance data.
   * If we stored the private data after the instance data this would
   * not be the case (since a subclass that added more instance
   * variables would push the private data further along).
   *
   * This presents problems for valgrindability, of course, so we do a
   * workaround for that case.  We identify the start of the object to
   * valgrind as an allocated block (so that pointers to objects show up
   * as 'reachable' instead of 'possibly lost').  We then add an extra
   * pointer at the end of the object, after all instance data, back to
   * the start of the private area so that it is also recorded as
   * reachable.  We also add extra private space at the start because
   * valgrind doesn't seem to like us claiming to have allocated an
   * address that it saw allocated by malloc().
   */
  private_size = node->data->instance.private_size;
  ivar_size = node->data->instance.instance_size;

  if (private_size && RUNNING_ON_VALGRIND)
    {
      private_size += ALIGN_STRUCT (1);

      /* Allocate one extra pointer size... */
      allocated = g_slice_alloc0 (private_size + ivar_size + sizeof (gpointer));
      /* ... and point it back to the start of the private data. */
      *(gpointer *) (allocated + private_size + ivar_size) = allocated + ALIGN_STRUCT (1);

      /* Tell valgrind that it should treat the object itself as such */
      VALGRIND_MALLOCLIKE_BLOCK (allocated + private_size, ivar_size + sizeof (gpointer), 0, TRUE);
      VALGRIND_MALLOCLIKE_BLOCK (allocated + ALIGN_STRUCT (1), private_size - ALIGN_STRUCT (1), 0, TRUE);
    }
  else
    allocated = g_slice_alloc0 (private_size + ivar_size);

  instance = (GTypeInstance *) (allocated + private_size);

  for (i = node->n_supers; i > 0; i--)
    {
      TypeNode *pnode;
      
      pnode = lookup_type_node_I (node->supers[i]);
      if (pnode->data->instance.instance_init)
	{
	  instance->g_class = pnode->data->instance.class;
	  pnode->data->instance.instance_init (instance, class);
	}
    }

  instance->g_class = class;
  if (node->data->instance.instance_init)
    node->data->instance.instance_init (instance, class);

#ifdef	G_ENABLE_DEBUG
  IF_DEBUG (INSTANCE_COUNT)
    {
      g_atomic_int_inc ((int *) &node->instance_count);
    }
#endif

  TRACE(GOBJECT_OBJECT_NEW(instance, type));

  return instance;
}

/**
 * g_type_free_instance:
 * @instance: an instance of a type
 *
 * Frees an instance of a type, returning it to the instance pool for
 * the type, if there is one.
 *
 * Like g_type_create_instance(), this function is reserved for
 * implementors of fundamental types.
 */
void
g_type_free_instance (GTypeInstance *instance)
{
  TypeNode *node;
  GTypeClass *class;
  gchar *allocated;
  gint private_size;
  gint ivar_size;

  g_return_if_fail (instance != NULL && instance->g_class != NULL);
  
  class = instance->g_class;
  node = lookup_type_node_I (class->g_type);
  if (!node || !node->is_instantiatable || !node->data || node->data->class.class != (gpointer) class)
    {
      g_warning ("cannot free instance of invalid (non-instantiatable) type '%s'",
		 type_descriptive_name_I (class->g_type));
      return;
    }
  /* G_TYPE_IS_ABSTRACT() is an external call: _U */
  if (!node->mutatable_check_cache && G_TYPE_IS_ABSTRACT (NODE_TYPE (node)))
    {
      g_warning ("cannot free instance of abstract (non-instantiatable) type '%s'",
		 NODE_NAME (node));
      return;
    }
  
  instance->g_class = NULL;
  private_size = node->data->instance.private_size;
  ivar_size = node->data->instance.instance_size;
  allocated = ((gchar *) instance) - private_size;

#ifdef G_ENABLE_DEBUG
  memset (allocated, 0xaa, ivar_size + private_size);
#endif

  /* See comment in g_type_create_instance() about what's going on here.
   * We're basically unwinding what we put into motion there.
   */
  if (private_size && RUNNING_ON_VALGRIND)
    {
      private_size += ALIGN_STRUCT (1);
      allocated -= ALIGN_STRUCT (1);

      /* Clear out the extra pointer... */
      *(gpointer *) (allocated + private_size + ivar_size) = NULL;
      /* ... and ensure we include it in the size we free. */
      g_slice_free1 (private_size + ivar_size + sizeof (gpointer), allocated);

      VALGRIND_FREELIKE_BLOCK (allocated + ALIGN_STRUCT (1), 0);
      VALGRIND_FREELIKE_BLOCK (instance, 0);
    }
  else
    g_slice_free1 (private_size + ivar_size, allocated);

#ifdef	G_ENABLE_DEBUG
  IF_DEBUG (INSTANCE_COUNT)
    {
      g_atomic_int_add ((int *) &node->instance_count, -1);
    }
#endif

  g_type_class_unref (class);
}

static void
type_iface_ensure_dflt_vtable_Wm (TypeNode *iface)
{
  g_assert (iface->data);

  if (!iface->data->iface.dflt_vtable)
    {
      GTypeInterface *vtable = g_malloc0 (iface->data->iface.vtable_size);
      iface->data->iface.dflt_vtable = vtable;
      vtable->g_type = NODE_TYPE (iface);
      vtable->g_instance_type = 0;
      if (iface->data->iface.vtable_init_base ||
          iface->data->iface.dflt_init)
        {
          G_WRITE_UNLOCK (&type_rw_lock);
          if (iface->data->iface.vtable_init_base)
            iface->data->iface.vtable_init_base (vtable);
          if (iface->data->iface.dflt_init)
            iface->data->iface.dflt_init (vtable, (gpointer) iface->data->iface.dflt_data);
          G_WRITE_LOCK (&type_rw_lock);
        }
    }
}


/* This is called to allocate and do the first part of initializing
 * the interface vtable; type_iface_vtable_iface_init_Wm() does the remainder.
 *
 * A FALSE return indicates that we didn't find an init function for
 * this type/iface pair, so the vtable from the parent type should
 * be used. Note that the write lock is not modified upon a FALSE
 * return.
 */
static gboolean
type_iface_vtable_base_init_Wm (TypeNode *iface,
				TypeNode *node)
{
  IFaceEntry *entry;
  IFaceHolder *iholder;
  GTypeInterface *vtable = NULL;
  TypeNode *pnode;
  
  /* type_iface_retrieve_holder_info_Wm() doesn't modify write lock for returning NULL */
  iholder = type_iface_retrieve_holder_info_Wm (iface, NODE_TYPE (node), TRUE);
  if (!iholder)
    return FALSE;	/* we don't modify write lock upon FALSE */

  type_iface_ensure_dflt_vtable_Wm (iface);

  entry = type_lookup_iface_entry_L (node, iface);

  g_assert (iface->data && entry && entry->vtable == NULL && iholder && iholder->info);
  
  entry->init_state = IFACE_INIT;

  pnode = lookup_type_node_I (NODE_PARENT_TYPE (node));
  if (pnode)	/* want to copy over parent iface contents */
    {
      IFaceEntry *pentry = type_lookup_iface_entry_L (pnode, iface);
      
      if (pentry)
	vtable = g_memdup (pentry->vtable, iface->data->iface.vtable_size);
    }
  if (!vtable)
    vtable = g_memdup (iface->data->iface.dflt_vtable, iface->data->iface.vtable_size);
  entry->vtable = vtable;
  vtable->g_type = NODE_TYPE (iface);
  vtable->g_instance_type = NODE_TYPE (node);
  
  if (iface->data->iface.vtable_init_base)
    {
      G_WRITE_UNLOCK (&type_rw_lock);
      iface->data->iface.vtable_init_base (vtable);
      G_WRITE_LOCK (&type_rw_lock);
    }
  return TRUE;	/* initialized the vtable */
}

/* Finishes what type_iface_vtable_base_init_Wm started by
 * calling the interface init function.
 * this function may only be called for types with their
 * own interface holder info, i.e. types for which
 * g_type_add_interface*() was called and not children thereof.
 */
static void
type_iface_vtable_iface_init_Wm (TypeNode *iface,
				 TypeNode *node)
{
  IFaceEntry *entry = type_lookup_iface_entry_L (node, iface);
  IFaceHolder *iholder = type_iface_peek_holder_L (iface, NODE_TYPE (node));
  GTypeInterface *vtable = NULL;
  guint i;
  
  /* iholder->info should have been filled in by type_iface_vtable_base_init_Wm() */
  g_assert (iface->data && entry && iholder && iholder->info);
  g_assert (entry->init_state == IFACE_INIT); /* assert prior base_init() */
  
  entry->init_state = INITIALIZED;
      
  vtable = entry->vtable;

  if (iholder->info->interface_init)
    {
      G_WRITE_UNLOCK (&type_rw_lock);
      if (iholder->info->interface_init)
	iholder->info->interface_init (vtable, iholder->info->interface_data);
      G_WRITE_LOCK (&type_rw_lock);
    }
  
  for (i = 0; i < static_n_iface_check_funcs; i++)
    {
      GTypeInterfaceCheckFunc check_func = static_iface_check_funcs[i].check_func;
      gpointer check_data = static_iface_check_funcs[i].check_data;

      G_WRITE_UNLOCK (&type_rw_lock);
      check_func (check_data, (gpointer)vtable);
      G_WRITE_LOCK (&type_rw_lock);      
    }
}

static gboolean
type_iface_vtable_finalize_Wm (TypeNode       *iface,
			       TypeNode       *node,
			       GTypeInterface *vtable)
{
  IFaceEntry *entry = type_lookup_iface_entry_L (node, iface);
  IFaceHolder *iholder;
  
  /* type_iface_retrieve_holder_info_Wm() doesn't modify write lock for returning NULL */
  iholder = type_iface_retrieve_holder_info_Wm (iface, NODE_TYPE (node), FALSE);
  if (!iholder)
    return FALSE;	/* we don't modify write lock upon FALSE */
  
  g_assert (entry && entry->vtable == vtable && iholder->info);
  
  entry->vtable = NULL;
  entry->init_state = UNINITIALIZED;
  if (iholder->info->interface_finalize || iface->data->iface.vtable_finalize_base)
    {
      G_WRITE_UNLOCK (&type_rw_lock);
      if (iholder->info->interface_finalize)
	iholder->info->interface_finalize (vtable, iholder->info->interface_data);
      if (iface->data->iface.vtable_finalize_base)
	iface->data->iface.vtable_finalize_base (vtable);
      G_WRITE_LOCK (&type_rw_lock);
    }
  vtable->g_type = 0;
  vtable->g_instance_type = 0;
  g_free (vtable);
  
  type_iface_blow_holder_info_Wm (iface, NODE_TYPE (node));
  
  return TRUE;	/* write lock modified */
}

static void
type_class_init_Wm (TypeNode   *node,
		    GTypeClass *pclass)
{
  GSList *slist, *init_slist = NULL;
  GTypeClass *class;
  IFaceEntries *entries;
  IFaceEntry *entry;
  TypeNode *bnode, *pnode;
  guint i;
  
  /* Accessing data->class will work for instantiable types
   * too because ClassData is a subset of InstanceData
   */
  g_assert (node->is_classed && node->data &&
	    node->data->class.class_size &&
	    !node->data->class.class &&
	    node->data->class.init_state == UNINITIALIZED);
  if (node->data->class.class_private_size)
    class = g_malloc0 (ALIGN_STRUCT (node->data->class.class_size) + node->data->class.class_private_size);
  else
    class = g_malloc0 (node->data->class.class_size);
  node->data->class.class = class;
  g_atomic_int_set (&node->data->class.init_state, BASE_CLASS_INIT);
  
  if (pclass)
    {
      TypeNode *pnode = lookup_type_node_I (pclass->g_type);
      
      memcpy (class, pclass, pnode->data->class.class_size);
      memcpy (G_STRUCT_MEMBER_P (class, ALIGN_STRUCT (node->data->class.class_size)), G_STRUCT_MEMBER_P (pclass, ALIGN_STRUCT (pnode->data->class.class_size)), pnode->data->class.class_private_size);

      if (node->is_instantiatable)
	{
	  /* We need to initialize the private_size here rather than in
	   * type_data_make_W() since the class init for the parent
	   * class may have changed pnode->data->instance.private_size.
	   */
	  node->data->instance.private_size = pnode->data->instance.private_size;
	}
    }
  class->g_type = NODE_TYPE (node);
  
  G_WRITE_UNLOCK (&type_rw_lock);
  
  /* stack all base class initialization functions, so we
   * call them in ascending order.
   */
  for (bnode = node; bnode; bnode = lookup_type_node_I (NODE_PARENT_TYPE (bnode)))
    if (bnode->data->class.class_init_base)
      init_slist = g_slist_prepend (init_slist, (gpointer) bnode->data->class.class_init_base);
  for (slist = init_slist; slist; slist = slist->next)
    {
      GBaseInitFunc class_init_base = (GBaseInitFunc) slist->data;
      
      class_init_base (class);
    }
  g_slist_free (init_slist);
  
  G_WRITE_LOCK (&type_rw_lock);

  g_atomic_int_set (&node->data->class.init_state, BASE_IFACE_INIT);
  
  /* Before we initialize the class, base initialize all interfaces, either
   * from parent, or through our holder info
   */
  pnode = lookup_type_node_I (NODE_PARENT_TYPE (node));

  i = 0;
  while ((entries = CLASSED_NODE_IFACES_ENTRIES_LOCKED (node)) != NULL &&
	  i < IFACE_ENTRIES_N_ENTRIES (entries))
    {
      entry = &entries->entry[i];
      while (i < IFACE_ENTRIES_N_ENTRIES (entries) &&
	     entry->init_state == IFACE_INIT)
	{
	  entry++;
	  i++;
	}

      if (i == IFACE_ENTRIES_N_ENTRIES (entries))
	break;

      if (!type_iface_vtable_base_init_Wm (lookup_type_node_I (entry->iface_type), node))
	{
	  guint j;
	  IFaceEntries *pentries = CLASSED_NODE_IFACES_ENTRIES_LOCKED (pnode);
	  
	  /* need to get this interface from parent, type_iface_vtable_base_init_Wm()
	   * doesn't modify write lock upon FALSE, so entry is still valid; 
	   */
	  g_assert (pnode != NULL);

	  if (pentries)
	    for (j = 0; j < IFACE_ENTRIES_N_ENTRIES (pentries); j++)
	      {
		IFaceEntry *pentry = &pentries->entry[j];

		if (pentry->iface_type == entry->iface_type)
		  {
		    entry->vtable = pentry->vtable;
		    entry->init_state = INITIALIZED;
		    break;
		  }
	      }
	  g_assert (entry->vtable != NULL);
	}

      /* If the write lock was released, additional interface entries might
       * have been inserted into CLASSED_NODE_IFACES_ENTRIES (node); they'll
       * be base-initialized when inserted, so we don't have to worry that
       * we might miss them. Uninitialized entries can only be moved higher
       * when new ones are inserted.
       */
      i++;
    }
  
  g_atomic_int_set (&node->data->class.init_state, CLASS_INIT);
  
  G_WRITE_UNLOCK (&type_rw_lock);

  if (node->data->class.class_init)
    node->data->class.class_init (class, (gpointer) node->data->class.class_data);
  
  G_WRITE_LOCK (&type_rw_lock);
  
  g_atomic_int_set (&node->data->class.init_state, IFACE_INIT);
  
  /* finish initializing the interfaces through our holder info.
   * inherited interfaces are already init_state == INITIALIZED, because
   * they either got setup in the above base_init loop, or during
   * class_init from within type_add_interface_Wm() for this or
   * an anchestor type.
   */
  i = 0;
  while ((entries = CLASSED_NODE_IFACES_ENTRIES_LOCKED (node)) != NULL)
    {
      entry = &entries->entry[i];
      while (i < IFACE_ENTRIES_N_ENTRIES (entries) &&
	     entry->init_state == INITIALIZED)
	{
	  entry++;
	  i++;
	}

      if (i == IFACE_ENTRIES_N_ENTRIES (entries))
	break;

      type_iface_vtable_iface_init_Wm (lookup_type_node_I (entry->iface_type), node);
      
      /* As in the loop above, additional initialized entries might be inserted
       * if the write lock is released, but that's harmless because the entries
       * we need to initialize only move higher in the list.
       */
      i++;
    }
  
  g_atomic_int_set (&node->data->class.init_state, INITIALIZED);
}

static void
type_data_finalize_class_ifaces_Wm (TypeNode *node)
{
  guint i;
  IFaceEntries *entries;

  g_assert (node->is_instantiatable && node->data && node->data->class.class && NODE_REFCOUNT (node) == 0);

 reiterate:
  entries = CLASSED_NODE_IFACES_ENTRIES_LOCKED (node);
  for (i = 0; entries != NULL && i < IFACE_ENTRIES_N_ENTRIES (entries); i++)
    {
      IFaceEntry *entry = &entries->entry[i];
      if (entry->vtable)
	{
          if (type_iface_vtable_finalize_Wm (lookup_type_node_I (entry->iface_type), node, entry->vtable))
            {
              /* refetch entries, IFACES_ENTRIES might be modified */
              goto reiterate;
            }
          else
            {
              /* type_iface_vtable_finalize_Wm() doesn't modify write lock upon FALSE,
               * iface vtable came from parent
               */
              entry->vtable = NULL;
              entry->init_state = UNINITIALIZED;
            }
	}
    }
}

static void
type_data_finalize_class_U (TypeNode  *node,
			    ClassData *cdata)
{
  GTypeClass *class = cdata->class;
  TypeNode *bnode;
  
  g_assert (cdata->class && NODE_REFCOUNT (node) == 0);
  
  if (cdata->class_finalize)
    cdata->class_finalize (class, (gpointer) cdata->class_data);
  
  /* call all base class destruction functions in descending order
   */
  if (cdata->class_finalize_base)
    cdata->class_finalize_base (class);
  for (bnode = lookup_type_node_I (NODE_PARENT_TYPE (node)); bnode; bnode = lookup_type_node_I (NODE_PARENT_TYPE (bnode)))
    if (bnode->data->class.class_finalize_base)
      bnode->data->class.class_finalize_base (class);
  
  g_free (cdata->class);
}

static void
type_data_last_unref_Wm (TypeNode *node,
			 gboolean  uncached)
{
  g_return_if_fail (node != NULL && node->plugin != NULL);
  
  if (!node->data || NODE_REFCOUNT (node) == 0)
    {
      g_warning ("cannot drop last reference to unreferenced type '%s'",
		 NODE_NAME (node));
      return;
    }

  /* call class cache hooks */
  if (node->is_classed && node->data && node->data->class.class && static_n_class_cache_funcs && !uncached)
    {
      guint i;
      
      G_WRITE_UNLOCK (&type_rw_lock);
      G_READ_LOCK (&type_rw_lock);
      for (i = 0; i < static_n_class_cache_funcs; i++)
	{
	  GTypeClassCacheFunc cache_func = static_class_cache_funcs[i].cache_func;
	  gpointer cache_data = static_class_cache_funcs[i].cache_data;
	  gboolean need_break;
	  
	  G_READ_UNLOCK (&type_rw_lock);
	  need_break = cache_func (cache_data, node->data->class.class);
	  G_READ_LOCK (&type_rw_lock);
	  if (!node->data || NODE_REFCOUNT (node) == 0)
	    INVALID_RECURSION ("GType class cache function ", cache_func, NODE_NAME (node));
	  if (need_break)
	    break;
	}
      G_READ_UNLOCK (&type_rw_lock);
      G_WRITE_LOCK (&type_rw_lock);
    }
  
  /* may have been re-referenced meanwhile */
  if (g_atomic_int_dec_and_test ((int *) &node->ref_count))
    {
      GType ptype = NODE_PARENT_TYPE (node);
      TypeData *tdata;
      
      if (node->is_instantiatable)
	{
	  /* destroy node->data->instance.mem_chunk */
	}
      
      tdata = node->data;
      if (node->is_classed && tdata->class.class)
	{
	  if (CLASSED_NODE_IFACES_ENTRIES_LOCKED (node) != NULL)
	    type_data_finalize_class_ifaces_Wm (node);
	  node->mutatable_check_cache = FALSE;
	  node->data = NULL;
	  G_WRITE_UNLOCK (&type_rw_lock);
	  type_data_finalize_class_U (node, &tdata->class);
	  G_WRITE_LOCK (&type_rw_lock);
	}
      else if (NODE_IS_IFACE (node) && tdata->iface.dflt_vtable)
        {
          node->mutatable_check_cache = FALSE;
          node->data = NULL;
          if (tdata->iface.dflt_finalize || tdata->iface.vtable_finalize_base)
            {
              G_WRITE_UNLOCK (&type_rw_lock);
              if (tdata->iface.dflt_finalize)
                tdata->iface.dflt_finalize (tdata->iface.dflt_vtable, (gpointer) tdata->iface.dflt_data);
              if (tdata->iface.vtable_finalize_base)
                tdata->iface.vtable_finalize_base (tdata->iface.dflt_vtable);
              G_WRITE_LOCK (&type_rw_lock);
            }
          g_free (tdata->iface.dflt_vtable);
        }
      else
        {
          node->mutatable_check_cache = FALSE;
          node->data = NULL;
        }

      /* freeing tdata->common.value_table and its contents is taken care of
       * by allocating it in one chunk with tdata
       */
      g_free (tdata);
      
      G_WRITE_UNLOCK (&type_rw_lock);
      g_type_plugin_unuse (node->plugin);
      if (ptype)
	type_data_unref_U (lookup_type_node_I (ptype), FALSE);
      G_WRITE_LOCK (&type_rw_lock);
    }
}

static inline void
type_data_unref_U (TypeNode *node,
                   gboolean  uncached)
{
  guint current;

  do {
    current = NODE_REFCOUNT (node);

    if (current <= 1)
    {
      if (!node->plugin)
	{
	  g_warning ("static type '%s' unreferenced too often",
		     NODE_NAME (node));
	  return;
	}
      else
        {
          /* This is the last reference of a type from a plugin.  We are
           * experimentally disabling support for unloading type
           * plugins, so don't allow the last ref to drop.
           */
          return;
        }

      g_assert (current > 0);

      g_rec_mutex_lock (&class_init_rec_mutex); /* required locking order: 1) class_init_rec_mutex, 2) type_rw_lock */
      G_WRITE_LOCK (&type_rw_lock);
      type_data_last_unref_Wm (node, uncached);
      G_WRITE_UNLOCK (&type_rw_lock);
      g_rec_mutex_unlock (&class_init_rec_mutex);
      return;
    }
  } while (!g_atomic_int_compare_and_exchange ((int *) &node->ref_count, current, current - 1));
}

/**
 * g_type_add_class_cache_func: (skip)
 * @cache_data: data to be passed to @cache_func
 * @cache_func: a #GTypeClassCacheFunc
 *
 * Adds a #GTypeClassCacheFunc to be called before the reference count of a
 * class goes from one to zero. This can be used to prevent premature class
 * destruction. All installed #GTypeClassCacheFunc functions will be chained
 * until one of them returns %TRUE. The functions have to check the class id
 * passed in to figure whether they actually want to cache the class of this
 * type, since all classes are routed through the same #GTypeClassCacheFunc
 * chain.
 */
void
g_type_add_class_cache_func (gpointer            cache_data,
			     GTypeClassCacheFunc cache_func)
{
  guint i;
  
  g_return_if_fail (cache_func != NULL);
  
  G_WRITE_LOCK (&type_rw_lock);
  i = static_n_class_cache_funcs++;
  static_class_cache_funcs = g_renew (ClassCacheFunc, static_class_cache_funcs, static_n_class_cache_funcs);
  static_class_cache_funcs[i].cache_data = cache_data;
  static_class_cache_funcs[i].cache_func = cache_func;
  G_WRITE_UNLOCK (&type_rw_lock);
}

/**
 * g_type_remove_class_cache_func: (skip)
 * @cache_data: data that was given when adding @cache_func
 * @cache_func: a #GTypeClassCacheFunc
 *
 * Removes a previously installed #GTypeClassCacheFunc. The cache
 * maintained by @cache_func has to be empty when calling
 * g_type_remove_class_cache_func() to avoid leaks.
 */
void
g_type_remove_class_cache_func (gpointer            cache_data,
				GTypeClassCacheFunc cache_func)
{
  gboolean found_it = FALSE;
  guint i;
  
  g_return_if_fail (cache_func != NULL);
  
  G_WRITE_LOCK (&type_rw_lock);
  for (i = 0; i < static_n_class_cache_funcs; i++)
    if (static_class_cache_funcs[i].cache_data == cache_data &&
	static_class_cache_funcs[i].cache_func == cache_func)
      {
	static_n_class_cache_funcs--;
	memmove (static_class_cache_funcs + i,
                 static_class_cache_funcs + i + 1,
                 sizeof (static_class_cache_funcs[0]) * (static_n_class_cache_funcs - i));
	static_class_cache_funcs = g_renew (ClassCacheFunc, static_class_cache_funcs, static_n_class_cache_funcs);
	found_it = TRUE;
	break;
      }
  G_WRITE_UNLOCK (&type_rw_lock);
  
  if (!found_it)
    g_warning (G_STRLOC ": cannot remove unregistered class cache func %p with data %p",
	       cache_func, cache_data);
}


/**
 * g_type_add_interface_check: (skip)
 * @check_data: data to pass to @check_func
 * @check_func: function to be called after each interface
 *     is initialized
 *
 * Adds a function to be called after an interface vtable is
 * initialized for any class (i.e. after the @interface_init
 * member of #GInterfaceInfo has been called).
 *
 * This function is useful when you want to check an invariant
 * that depends on the interfaces of a class. For instance, the
 * implementation of #GObject uses this facility to check that an
 * object implements all of the properties that are defined on its
 * interfaces.
 *
 * Since: 2.4
 */
void
g_type_add_interface_check (gpointer	            check_data,
			    GTypeInterfaceCheckFunc check_func)
{
  guint i;
  
  g_return_if_fail (check_func != NULL);
  
  G_WRITE_LOCK (&type_rw_lock);
  i = static_n_iface_check_funcs++;
  static_iface_check_funcs = g_renew (IFaceCheckFunc, static_iface_check_funcs, static_n_iface_check_funcs);
  static_iface_check_funcs[i].check_data = check_data;
  static_iface_check_funcs[i].check_func = check_func;
  G_WRITE_UNLOCK (&type_rw_lock);
}

/**
 * g_type_remove_interface_check: (skip)
 * @check_data: callback data passed to g_type_add_interface_check()
 * @check_func: callback function passed to g_type_add_interface_check()
 *
 * Removes an interface check function added with
 * g_type_add_interface_check().
 *
 * Since: 2.4
 */
void
g_type_remove_interface_check (gpointer                check_data,
			       GTypeInterfaceCheckFunc check_func)
{
  gboolean found_it = FALSE;
  guint i;
  
  g_return_if_fail (check_func != NULL);
  
  G_WRITE_LOCK (&type_rw_lock);
  for (i = 0; i < static_n_iface_check_funcs; i++)
    if (static_iface_check_funcs[i].check_data == check_data &&
	static_iface_check_funcs[i].check_func == check_func)
      {
	static_n_iface_check_funcs--;
	memmove (static_iface_check_funcs + i,
                 static_iface_check_funcs + i + 1,
                 sizeof (static_iface_check_funcs[0]) * (static_n_iface_check_funcs - i));
	static_iface_check_funcs = g_renew (IFaceCheckFunc, static_iface_check_funcs, static_n_iface_check_funcs);
	found_it = TRUE;
	break;
      }
  G_WRITE_UNLOCK (&type_rw_lock);
  
  if (!found_it)
    g_warning (G_STRLOC ": cannot remove unregistered class check func %p with data %p",
	       check_func, check_data);
}

/* --- type registration --- */
/**
 * g_type_register_fundamental:
 * @type_id: a predefined type identifier
 * @type_name: 0-terminated string used as the name of the new type
 * @info: #GTypeInfo structure for this type
 * @finfo: #GTypeFundamentalInfo structure for this type
 * @flags: bitwise combination of #GTypeFlags values
 *
 * Registers @type_id as the predefined identifier and @type_name as the
 * name of a fundamental type. If @type_id is already registered, or a
 * type named @type_name is already registered, the behaviour is undefined.
 * The type system uses the information contained in the #GTypeInfo structure
 * pointed to by @info and the #GTypeFundamentalInfo structure pointed to by
 * @finfo to manage the type and its instances. The value of @flags determines
 * additional characteristics of the fundamental type.
 *
 * Returns: the predefined type identifier
 */
GType
g_type_register_fundamental (GType                       type_id,
			     const gchar                *type_name,
			     const GTypeInfo            *info,
			     const GTypeFundamentalInfo *finfo,
			     GTypeFlags			 flags)
{
  TypeNode *node;
  
  g_assert_type_system_initialized ();
  g_return_val_if_fail (type_id > 0, 0);
  g_return_val_if_fail (type_name != NULL, 0);
  g_return_val_if_fail (info != NULL, 0);
  g_return_val_if_fail (finfo != NULL, 0);
  
  if (!check_type_name_I (type_name))
    return 0;
  if ((type_id & TYPE_ID_MASK) ||
      type_id > G_TYPE_FUNDAMENTAL_MAX)
    {
      g_warning ("attempt to register fundamental type '%s' with invalid type id (%" G_GSIZE_FORMAT ")",
		 type_name,
		 type_id);
      return 0;
    }
  if ((finfo->type_flags & G_TYPE_FLAG_INSTANTIATABLE) &&
      !(finfo->type_flags & G_TYPE_FLAG_CLASSED))
    {
      g_warning ("cannot register instantiatable fundamental type '%s' as non-classed",
		 type_name);
      return 0;
    }
  if (lookup_type_node_I (type_id))
    {
      g_warning ("cannot register existing fundamental type '%s' (as '%s')",
		 type_descriptive_name_I (type_id),
		 type_name);
      return 0;
    }
  
  G_WRITE_LOCK (&type_rw_lock);
  node = type_node_fundamental_new_W (type_id, type_name, finfo->type_flags);
  type_add_flags_W (node, flags);
  
  if (check_type_info_I (NULL, NODE_FUNDAMENTAL_TYPE (node), type_name, info))
    type_data_make_W (node, info,
		      check_value_table_I (type_name, info->value_table) ? info->value_table : NULL);
  G_WRITE_UNLOCK (&type_rw_lock);
  
  return NODE_TYPE (node);
}

/**
 * g_type_register_static_simple: (skip)
 * @parent_type: type from which this type will be derived
 * @type_name: 0-terminated string used as the name of the new type
 * @class_size: size of the class structure (see #GTypeInfo)
 * @class_init: location of the class initialization function (see #GTypeInfo)
 * @instance_size: size of the instance structure (see #GTypeInfo)
 * @instance_init: location of the instance initialization function (see #GTypeInfo)
 * @flags: bitwise combination of #GTypeFlags values
 *
 * Registers @type_name as the name of a new static type derived from
 * @parent_type.  The value of @flags determines the nature (e.g.
 * abstract or not) of the type. It works by filling a #GTypeInfo
 * struct and calling g_type_register_static().
 *
 * Since: 2.12
 *
 * Returns: the new type identifier
 */
GType
g_type_register_static_simple (GType             parent_type,
			       const gchar      *type_name,
			       guint             class_size,
			       GClassInitFunc    class_init,
			       guint             instance_size,
			       GInstanceInitFunc instance_init,
			       GTypeFlags	 flags)
{
  GTypeInfo info;

  /* Instances are not allowed to be larger than this. If you have a big
   * fixed-length array or something, point to it instead.
   */
  g_return_val_if_fail (class_size <= G_MAXUINT16, G_TYPE_INVALID);
  g_return_val_if_fail (instance_size <= G_MAXUINT16, G_TYPE_INVALID);

  info.class_size = class_size;
  info.base_init = NULL;
  info.base_finalize = NULL;
  info.class_init = class_init;
  info.class_finalize = NULL;
  info.class_data = NULL;
  info.instance_size = instance_size;
  info.n_preallocs = 0;
  info.instance_init = instance_init;
  info.value_table = NULL;

  return g_type_register_static (parent_type, type_name, &info, flags);
}

/**
 * g_type_register_static:
 * @parent_type: type from which this type will be derived
 * @type_name: 0-terminated string used as the name of the new type
 * @info: #GTypeInfo structure for this type
 * @flags: bitwise combination of #GTypeFlags values
 *
 * Registers @type_name as the name of a new static type derived from
 * @parent_type. The type system uses the information contained in the
 * #GTypeInfo structure pointed to by @info to manage the type and its
 * instances (if not abstract). The value of @flags determines the nature
 * (e.g. abstract or not) of the type.
 *
 * Returns: the new type identifier
 */
GType
g_type_register_static (GType            parent_type,
			const gchar     *type_name,
			const GTypeInfo *info,
			GTypeFlags	 flags)
{
  TypeNode *pnode, *node;
  GType type = 0;
  
  g_assert_type_system_initialized ();
  g_return_val_if_fail (parent_type > 0, 0);
  g_return_val_if_fail (type_name != NULL, 0);
  g_return_val_if_fail (info != NULL, 0);
  
  if (!check_type_name_I (type_name) ||
      !check_derivation_I (parent_type, type_name))
    return 0;
  if (info->class_finalize)
    {
      g_warning ("class finalizer specified for static type '%s'",
		 type_name);
      return 0;
    }
  
  pnode = lookup_type_node_I (parent_type);
  G_WRITE_LOCK (&type_rw_lock);
  type_data_ref_Wm (pnode);
  if (check_type_info_I (pnode, NODE_FUNDAMENTAL_TYPE (pnode), type_name, info))
    {
      node = type_node_new_W (pnode, type_name, NULL);
      type_add_flags_W (node, flags);
      type = NODE_TYPE (node);
      type_data_make_W (node, info,
			check_value_table_I (type_name, info->value_table) ? info->value_table : NULL);
    }
  G_WRITE_UNLOCK (&type_rw_lock);
  
  return type;
}

/**
 * g_type_register_dynamic:
 * @parent_type: type from which this type will be derived
 * @type_name: 0-terminated string used as the name of the new type
 * @plugin: #GTypePlugin structure to retrieve the #GTypeInfo from
 * @flags: bitwise combination of #GTypeFlags values
 *
 * Registers @type_name as the name of a new dynamic type derived from
 * @parent_type.  The type system uses the information contained in the
 * #GTypePlugin structure pointed to by @plugin to manage the type and its
 * instances (if not abstract).  The value of @flags determines the nature
 * (e.g. abstract or not) of the type.
 *
 * Returns: the new type identifier or #G_TYPE_INVALID if registration failed
 */
GType
g_type_register_dynamic (GType        parent_type,
			 const gchar *type_name,
			 GTypePlugin *plugin,
			 GTypeFlags   flags)
{
  TypeNode *pnode, *node;
  GType type;
  
  g_assert_type_system_initialized ();
  g_return_val_if_fail (parent_type > 0, 0);
  g_return_val_if_fail (type_name != NULL, 0);
  g_return_val_if_fail (plugin != NULL, 0);
  
  if (!check_type_name_I (type_name) ||
      !check_derivation_I (parent_type, type_name) ||
      !check_plugin_U (plugin, TRUE, FALSE, type_name))
    return 0;
  
  G_WRITE_LOCK (&type_rw_lock);
  pnode = lookup_type_node_I (parent_type);
  node = type_node_new_W (pnode, type_name, plugin);
  type_add_flags_W (node, flags);
  type = NODE_TYPE (node);
  G_WRITE_UNLOCK (&type_rw_lock);
  
  return type;
}

/**
 * g_type_add_interface_static:
 * @instance_type: #GType value of an instantiable type
 * @interface_type: #GType value of an interface type
 * @info: #GInterfaceInfo structure for this
 *        (@instance_type, @interface_type) combination
 *
 * Adds the static @interface_type to @instantiable_type.
 * The information contained in the #GInterfaceInfo structure
 * pointed to by @info is used to manage the relationship.
 */
void
g_type_add_interface_static (GType                 instance_type,
			     GType                 interface_type,
			     const GInterfaceInfo *info)
{
  /* G_TYPE_IS_INSTANTIATABLE() is an external call: _U */
  g_return_if_fail (G_TYPE_IS_INSTANTIATABLE (instance_type));
  g_return_if_fail (g_type_parent (interface_type) == G_TYPE_INTERFACE);

  /* we only need to lock class_init_rec_mutex if instance_type already has its
   * class initialized, however this function is rarely enough called to take
   * the simple route and always acquire class_init_rec_mutex.
   */
  g_rec_mutex_lock (&class_init_rec_mutex); /* required locking order: 1) class_init_rec_mutex, 2) type_rw_lock */
  G_WRITE_LOCK (&type_rw_lock);
  if (check_add_interface_L (instance_type, interface_type))
    {
      TypeNode *node = lookup_type_node_I (instance_type);
      TypeNode *iface = lookup_type_node_I (interface_type);
      if (check_interface_info_I (iface, NODE_TYPE (node), info))
        type_add_interface_Wm (node, iface, info, NULL);
    }
  G_WRITE_UNLOCK (&type_rw_lock);
  g_rec_mutex_unlock (&class_init_rec_mutex);
}

/**
 * g_type_add_interface_dynamic:
 * @instance_type: #GType value of an instantiable type
 * @interface_type: #GType value of an interface type
 * @plugin: #GTypePlugin structure to retrieve the #GInterfaceInfo from
 *
 * Adds the dynamic @interface_type to @instantiable_type. The information
 * contained in the #GTypePlugin structure pointed to by @plugin
 * is used to manage the relationship.
 */
void
g_type_add_interface_dynamic (GType        instance_type,
			      GType        interface_type,
			      GTypePlugin *plugin)
{
  TypeNode *node;
  /* G_TYPE_IS_INSTANTIATABLE() is an external call: _U */
  g_return_if_fail (G_TYPE_IS_INSTANTIATABLE (instance_type));
  g_return_if_fail (g_type_parent (interface_type) == G_TYPE_INTERFACE);

  node = lookup_type_node_I (instance_type);
  if (!check_plugin_U (plugin, FALSE, TRUE, NODE_NAME (node)))
    return;

  /* see comment in g_type_add_interface_static() about class_init_rec_mutex */
  g_rec_mutex_lock (&class_init_rec_mutex); /* required locking order: 1) class_init_rec_mutex, 2) type_rw_lock */
  G_WRITE_LOCK (&type_rw_lock);
  if (check_add_interface_L (instance_type, interface_type))
    {
      TypeNode *iface = lookup_type_node_I (interface_type);
      type_add_interface_Wm (node, iface, NULL, plugin);
    }
  G_WRITE_UNLOCK (&type_rw_lock);
  g_rec_mutex_unlock (&class_init_rec_mutex);
}


/* --- public API functions --- */
/**
 * g_type_class_ref:
 * @type: type ID of a classed type
 *
 * Increments the reference count of the class structure belonging to
 * @type. This function will demand-create the class if it doesn't
 * exist already.
 *
 * Returns: (type GObject.TypeClass) (transfer none): the #GTypeClass
 *     structure for the given type ID
 */
gpointer
g_type_class_ref (GType type)
{
  TypeNode *node;
  GType ptype;
  gboolean holds_ref;
  GTypeClass *pclass;

  /* optimize for common code path */
  node = lookup_type_node_I (type);
  if (!node || !node->is_classed)
    {
      g_warning ("cannot retrieve class for invalid (unclassed) type '%s'",
		 type_descriptive_name_I (type));
      return NULL;
    }

  if (G_LIKELY (type_data_ref_U (node)))
    {
      if (G_LIKELY (g_atomic_int_get (&node->data->class.init_state) == INITIALIZED))
        return node->data->class.class;
      holds_ref = TRUE;
    }
  else
    holds_ref = FALSE;
  
  /* here, we either have node->data->class.class == NULL, or a recursive
   * call to g_type_class_ref() with a partly initialized class, or
   * node->data->class.init_state == INITIALIZED, because any
   * concurrently running initialization was guarded by class_init_rec_mutex.
   */
  g_rec_mutex_lock (&class_init_rec_mutex); /* required locking order: 1) class_init_rec_mutex, 2) type_rw_lock */

  /* we need an initialized parent class for initializing derived classes */
  ptype = NODE_PARENT_TYPE (node);
  pclass = ptype ? g_type_class_ref (ptype) : NULL;

  G_WRITE_LOCK (&type_rw_lock);

  if (!holds_ref)
    type_data_ref_Wm (node);

  if (!node->data->class.class) /* class uninitialized */
    type_class_init_Wm (node, pclass);

  G_WRITE_UNLOCK (&type_rw_lock);

  if (pclass)
    g_type_class_unref (pclass);

  g_rec_mutex_unlock (&class_init_rec_mutex);

  return node->data->class.class;
}

/**
 * g_type_class_unref:
 * @g_class: (type GObject.TypeClass): a #GTypeClass structure to unref
 *
 * Decrements the reference count of the class structure being passed in.
 * Once the last reference count of a class has been released, classes
 * may be finalized by the type system, so further dereferencing of a
 * class pointer after g_type_class_unref() are invalid.
 */
void
g_type_class_unref (gpointer g_class)
{
  TypeNode *node;
  GTypeClass *class = g_class;
  
  g_return_if_fail (g_class != NULL);
  
  node = lookup_type_node_I (class->g_type);
  if (node && node->is_classed && NODE_REFCOUNT (node))
    type_data_unref_U (node, FALSE);
  else
    g_warning ("cannot unreference class of invalid (unclassed) type '%s'",
	       type_descriptive_name_I (class->g_type));
}

/**
 * g_type_class_unref_uncached: (skip)
 * @g_class: (type GObject.TypeClass): a #GTypeClass structure to unref
 *
 * A variant of g_type_class_unref() for use in #GTypeClassCacheFunc
 * implementations. It unreferences a class without consulting the chain
 * of #GTypeClassCacheFuncs, avoiding the recursion which would occur
 * otherwise.
 */
void
g_type_class_unref_uncached (gpointer g_class)
{
  TypeNode *node;
  GTypeClass *class = g_class;
  
  g_return_if_fail (g_class != NULL);
  
  node = lookup_type_node_I (class->g_type);
  if (node && node->is_classed && NODE_REFCOUNT (node))
    type_data_unref_U (node, TRUE);
  else
    g_warning ("cannot unreference class of invalid (unclassed) type '%s'",
	       type_descriptive_name_I (class->g_type));
}

/**
 * g_type_class_peek:
 * @type: type ID of a classed type
 *
 * This function is essentially the same as g_type_class_ref(),
 * except that the classes reference count isn't incremented.
 * As a consequence, this function may return %NULL if the class
 * of the type passed in does not currently exist (hasn't been
 * referenced before).
 *
 * Returns: (type GObject.TypeClass) (transfer none): the #GTypeClass
 *     structure for the given type ID or %NULL if the class does not
 *     currently exist
 */
gpointer
g_type_class_peek (GType type)
{
  TypeNode *node;
  gpointer class;
  
  node = lookup_type_node_I (type);
  if (node && node->is_classed && NODE_REFCOUNT (node) &&
      g_atomic_int_get (&node->data->class.init_state) == INITIALIZED)
    /* ref_count _may_ be 0 */
    class = node->data->class.class;
  else
    class = NULL;
  
  return class;
}

/**
 * g_type_class_peek_static:
 * @type: type ID of a classed type
 *
 * A more efficient version of g_type_class_peek() which works only for
 * static types.
 * 
 * Returns: (type GObject.TypeClass) (transfer none): the #GTypeClass
 *     structure for the given type ID or %NULL if the class does not
 *     currently exist or is dynamically loaded
 *
 * Since: 2.4
 */
gpointer
g_type_class_peek_static (GType type)
{
  TypeNode *node;
  gpointer class;
  
  node = lookup_type_node_I (type);
  if (node && node->is_classed && NODE_REFCOUNT (node) &&
      /* peek only static types: */ node->plugin == NULL &&
      g_atomic_int_get (&node->data->class.init_state) == INITIALIZED)
    /* ref_count _may_ be 0 */
    class = node->data->class.class;
  else
    class = NULL;
  
  return class;
}

/**
 * g_type_class_peek_parent:
 * @g_class: (type GObject.TypeClass): the #GTypeClass structure to
 *     retrieve the parent class for
 *
 * This is a convenience function often needed in class initializers.
 * It returns the class structure of the immediate parent type of the
 * class passed in.  Since derived classes hold a reference count on
 * their parent classes as long as they are instantiated, the returned
 * class will always exist.
 *
 * This function is essentially equivalent to:
 * g_type_class_peek (g_type_parent (G_TYPE_FROM_CLASS (g_class)))
 *
 * Returns: (type GObject.TypeClass) (transfer none): the parent class
 *     of @g_class
 */
gpointer
g_type_class_peek_parent (gpointer g_class)
{
  TypeNode *node;
  gpointer class = NULL;
  
  g_return_val_if_fail (g_class != NULL, NULL);
  
  node = lookup_type_node_I (G_TYPE_FROM_CLASS (g_class));
  /* We used to acquire a read lock here. That is not necessary, since 
   * parent->data->class.class is constant as long as the derived class
   * exists. 
   */
  if (node && node->is_classed && node->data && NODE_PARENT_TYPE (node))
    {
      node = lookup_type_node_I (NODE_PARENT_TYPE (node));
      class = node->data->class.class;
    }
  else if (NODE_PARENT_TYPE (node))
    g_warning (G_STRLOC ": invalid class pointer '%p'", g_class);
  
  return class;
}

/**
 * g_type_interface_peek:
 * @instance_class: (type GObject.TypeClass): a #GTypeClass structure
 * @iface_type: an interface ID which this class conforms to
 *
 * Returns the #GTypeInterface structure of an interface to which the
 * passed in class conforms.
 *
 * Returns: (type GObject.TypeInterface) (transfer none): the #GTypeInterface
 *     structure of @iface_type if implemented by @instance_class, %NULL
 *     otherwise
 */
gpointer
g_type_interface_peek (gpointer instance_class,
		       GType    iface_type)
{
  TypeNode *node;
  TypeNode *iface;
  gpointer vtable = NULL;
  GTypeClass *class = instance_class;
  
  g_return_val_if_fail (instance_class != NULL, NULL);
  
  node = lookup_type_node_I (class->g_type);
  iface = lookup_type_node_I (iface_type);
  if (node && node->is_instantiatable && iface)
    type_lookup_iface_vtable_I (node, iface, &vtable);
  else
    g_warning (G_STRLOC ": invalid class pointer '%p'", class);
  
  return vtable;
}

/**
 * g_type_interface_peek_parent:
 * @g_iface: (type GObject.TypeInterface): a #GTypeInterface structure
 *
 * Returns the corresponding #GTypeInterface structure of the parent type
 * of the instance type to which @g_iface belongs. This is useful when
 * deriving the implementation of an interface from the parent type and
 * then possibly overriding some methods.
 *
 * Returns: (transfer none) (type GObject.TypeInterface): the
 *     corresponding #GTypeInterface structure of the parent type of the
 *     instance type to which @g_iface belongs, or %NULL if the parent
 *     type doesn't conform to the interface
 */
gpointer
g_type_interface_peek_parent (gpointer g_iface)
{
  TypeNode *node;
  TypeNode *iface;
  gpointer vtable = NULL;
  GTypeInterface *iface_class = g_iface;
  
  g_return_val_if_fail (g_iface != NULL, NULL);
  
  iface = lookup_type_node_I (iface_class->g_type);
  node = lookup_type_node_I (iface_class->g_instance_type);
  if (node)
    node = lookup_type_node_I (NODE_PARENT_TYPE (node));
  if (node && node->is_instantiatable && iface)
    type_lookup_iface_vtable_I (node, iface, &vtable);
  else if (node)
    g_warning (G_STRLOC ": invalid interface pointer '%p'", g_iface);
  
  return vtable;
}

/**
 * g_type_default_interface_ref:
 * @g_type: an interface type
 *
 * Increments the reference count for the interface type @g_type,
 * and returns the default interface vtable for the type.
 *
 * If the type is not currently in use, then the default vtable
 * for the type will be created and initalized by calling
 * the base interface init and default vtable init functions for
 * the type (the @base_init and @class_init members of #GTypeInfo).
 * Calling g_type_default_interface_ref() is useful when you
 * want to make sure that signals and properties for an interface
 * have been installed.
 *
 * Since: 2.4
 *
 * Returns: (type GObject.TypeInterface) (transfer none): the default
 *     vtable for the interface; call g_type_default_interface_unref()
 *     when you are done using the interface.
 */
gpointer
g_type_default_interface_ref (GType g_type)
{
  TypeNode *node;
  gpointer dflt_vtable;

  G_WRITE_LOCK (&type_rw_lock);

  node = lookup_type_node_I (g_type);
  if (!node || !NODE_IS_IFACE (node) ||
      (node->data && NODE_REFCOUNT (node) == 0))
    {
      G_WRITE_UNLOCK (&type_rw_lock);
      g_warning ("cannot retrieve default vtable for invalid or non-interface type '%s'",
		 type_descriptive_name_I (g_type));
      return NULL;
    }

  if (!node->data || !node->data->iface.dflt_vtable)
    {
      G_WRITE_UNLOCK (&type_rw_lock);
      g_rec_mutex_lock (&class_init_rec_mutex); /* required locking order: 1) class_init_rec_mutex, 2) type_rw_lock */
      G_WRITE_LOCK (&type_rw_lock);
      node = lookup_type_node_I (g_type);
      type_data_ref_Wm (node);
      type_iface_ensure_dflt_vtable_Wm (node);
      g_rec_mutex_unlock (&class_init_rec_mutex);
    }
  else
    type_data_ref_Wm (node); /* ref_count >= 1 already */

  dflt_vtable = node->data->iface.dflt_vtable;
  G_WRITE_UNLOCK (&type_rw_lock);

  return dflt_vtable;
}

/**
 * g_type_default_interface_peek:
 * @g_type: an interface type
 *
 * If the interface type @g_type is currently in use, returns its
 * default interface vtable.
 *
 * Since: 2.4
 *
 * Returns: (type GObject.TypeInterface) (transfer none): the default
 *     vtable for the interface, or %NULL if the type is not currently
 *     in use
 */
gpointer
g_type_default_interface_peek (GType g_type)
{
  TypeNode *node;
  gpointer vtable;
  
  node = lookup_type_node_I (g_type);
  if (node && NODE_IS_IFACE (node) && NODE_REFCOUNT (node))
    vtable = node->data->iface.dflt_vtable;
  else
    vtable = NULL;
  
  return vtable;
}

/**
 * g_type_default_interface_unref:
 * @g_iface: (type GObject.TypeInterface): the default vtable
 *     structure for a interface, as returned by g_type_default_interface_ref()
 *
 * Decrements the reference count for the type corresponding to the
 * interface default vtable @g_iface. If the type is dynamic, then
 * when no one is using the interface and all references have
 * been released, the finalize function for the interface's default
 * vtable (the @class_finalize member of #GTypeInfo) will be called.
 *
 * Since: 2.4
 */
void
g_type_default_interface_unref (gpointer g_iface)
{
  TypeNode *node;
  GTypeInterface *vtable = g_iface;
  
  g_return_if_fail (g_iface != NULL);
  
  node = lookup_type_node_I (vtable->g_type);
  if (node && NODE_IS_IFACE (node))
    type_data_unref_U (node, FALSE);
  else
    g_warning ("cannot unreference invalid interface default vtable for '%s'",
	       type_descriptive_name_I (vtable->g_type));
}

/**
 * g_type_name:
 * @type: type to return name for
 *
 * Get the unique name that is assigned to a type ID.  Note that this
 * function (like all other GType API) cannot cope with invalid type
 * IDs. %G_TYPE_INVALID may be passed to this function, as may be any
 * other validly registered type ID, but randomized type IDs should
 * not be passed in and will most likely lead to a crash.
 *
 * Returns: static type name or %NULL
 */
const gchar *
g_type_name (GType type)
{
  TypeNode *node;
  
  g_assert_type_system_initialized ();
  
  node = lookup_type_node_I (type);
  
  return node ? NODE_NAME (node) : NULL;
}

/**
 * g_type_qname:
 * @type: type to return quark of type name for
 *
 * Get the corresponding quark of the type IDs name.
 *
 * Returns: the type names quark or 0
 */
GQuark
g_type_qname (GType type)
{
  TypeNode *node;
  
  node = lookup_type_node_I (type);
  
  return node ? node->qname : 0;
}

/**
 * g_type_from_name:
 * @name: type name to lookup
 *
 * Lookup the type ID from a given type name, returning 0 if no type
 * has been registered under this name (this is the preferred method
 * to find out by name whether a specific type has been registered
 * yet).
 *
 * Returns: corresponding type ID or 0
 */
GType
g_type_from_name (const gchar *name)
{
  GType type = 0;
  
  g_return_val_if_fail (name != NULL, 0);
  
  G_READ_LOCK (&type_rw_lock);
  type = (GType) g_hash_table_lookup (static_type_nodes_ht, name);
  G_READ_UNLOCK (&type_rw_lock);
  
  return type;
}

/**
 * g_type_parent:
 * @type: the derived type
 *
 * Return the direct parent type of the passed in type. If the passed
 * in type has no parent, i.e. is a fundamental type, 0 is returned.
 *
 * Returns: the parent type
 */
GType
g_type_parent (GType type)
{
  TypeNode *node;
  
  node = lookup_type_node_I (type);
  
  return node ? NODE_PARENT_TYPE (node) : 0;
}

/**
 * g_type_depth:
 * @type: a #GType
 *
 * Returns the length of the ancestry of the passed in type. This
 * includes the type itself, so that e.g. a fundamental type has depth 1.
 *
 * Returns: the depth of @type
 */
guint
g_type_depth (GType type)
{
  TypeNode *node;
  
  node = lookup_type_node_I (type);
  
  return node ? node->n_supers + 1 : 0;
}

/**
 * g_type_next_base:
 * @leaf_type: descendant of @root_type and the type to be returned
 * @root_type: immediate parent of the returned type
 *
 * Given a @leaf_type and a @root_type which is contained in its
 * anchestry, return the type that @root_type is the immediate parent
 * of. In other words, this function determines the type that is
 * derived directly from @root_type which is also a base class of
 * @leaf_type.  Given a root type and a leaf type, this function can
 * be used to determine the types and order in which the leaf type is
 * descended from the root type.
 *
 * Returns: immediate child of @root_type and anchestor of @leaf_type
 */
GType
g_type_next_base (GType type,
		  GType base_type)
{
  GType atype = 0;
  TypeNode *node;
  
  node = lookup_type_node_I (type);
  if (node)
    {
      TypeNode *base_node = lookup_type_node_I (base_type);
      
      if (base_node && base_node->n_supers < node->n_supers)
	{
	  guint n = node->n_supers - base_node->n_supers;
	  
	  if (node->supers[n] == base_type)
	    atype = node->supers[n - 1];
	}
    }
  
  return atype;
}

static inline gboolean
type_node_check_conformities_UorL (TypeNode *node,
				   TypeNode *iface_node,
				   /*        support_inheritance */
				   gboolean  support_interfaces,
				   gboolean  support_prerequisites,
				   gboolean  have_lock)
{
  gboolean match;

  if (/* support_inheritance && */
      NODE_IS_ANCESTOR (iface_node, node))
    return TRUE;

  support_interfaces = support_interfaces && node->is_instantiatable && NODE_IS_IFACE (iface_node);
  support_prerequisites = support_prerequisites && NODE_IS_IFACE (node);
  match = FALSE;
  if (support_interfaces)
    {
      if (have_lock)
	{
	  if (type_lookup_iface_entry_L (node, iface_node))
	    match = TRUE;
	}
      else
	{
	  if (type_lookup_iface_vtable_I (node, iface_node, NULL))
	    match = TRUE;
	}
    }
  if (!match &&
      support_prerequisites)
    {
      if (!have_lock)
	G_READ_LOCK (&type_rw_lock);
      if (support_prerequisites && type_lookup_prerequisite_L (node, NODE_TYPE (iface_node)))
	match = TRUE;
      if (!have_lock)
	G_READ_UNLOCK (&type_rw_lock);
    }
  return match;
}

static gboolean
type_node_is_a_L (TypeNode *node,
		  TypeNode *iface_node)
{
  return type_node_check_conformities_UorL (node, iface_node, TRUE, TRUE, TRUE);
}

static inline gboolean
type_node_conforms_to_U (TypeNode *node,
			 TypeNode *iface_node,
			 gboolean  support_interfaces,
			 gboolean  support_prerequisites)
{
  return type_node_check_conformities_UorL (node, iface_node, support_interfaces, support_prerequisites, FALSE);
}

/**
 * g_type_is_a:
 * @type: type to check anchestry for
 * @is_a_type: possible anchestor of @type or interface that @type
 *     could conform to
 *
 * If @is_a_type is a derivable type, check whether @type is a
 * descendant of @is_a_type. If @is_a_type is an interface, check
 * whether @type conforms to it.
 *
 * Returns: %TRUE if @type is a @is_a_type
 */
gboolean
g_type_is_a (GType type,
	     GType iface_type)
{
  TypeNode *node, *iface_node;
  gboolean is_a;

  if (type == iface_type)
    return TRUE;
  
  node = lookup_type_node_I (type);
  iface_node = lookup_type_node_I (iface_type);
  is_a = node && iface_node && type_node_conforms_to_U (node, iface_node, TRUE, TRUE);
  
  return is_a;
}

/**
 * g_type_children:
 * @type: the parent type
 * @n_children: (out) (optional): location to store the length of
 *     the returned array, or %NULL
 *
 * Return a newly allocated and 0-terminated array of type IDs, listing
 * the child types of @type.
 *
 * Returns: (array length=n_children) (transfer full): Newly allocated
 *     and 0-terminated array of child types, free with g_free()
 */
GType*
g_type_children (GType  type,
		 guint *n_children)
{
  TypeNode *node;
  
  node = lookup_type_node_I (type);
  if (node)
    {
      GType *children;
      
      G_READ_LOCK (&type_rw_lock);	/* ->children is relocatable */
      children = g_new (GType, node->n_children + 1);
      if (node->n_children != 0)
        memcpy (children, node->children, sizeof (GType) * node->n_children);
      children[node->n_children] = 0;
      
      if (n_children)
	*n_children = node->n_children;
      G_READ_UNLOCK (&type_rw_lock);
      
      return children;
    }
  else
    {
      if (n_children)
	*n_children = 0;
      
      return NULL;
    }
}

/**
 * g_type_interfaces:
 * @type: the type to list interface types for
 * @n_interfaces: (out) (optional): location to store the length of
 *     the returned array, or %NULL
 *
 * Return a newly allocated and 0-terminated array of type IDs, listing
 * the interface types that @type conforms to.
 *
 * Returns: (array length=n_interfaces) (transfer full): Newly allocated
 *     and 0-terminated array of interface types, free with g_free()
 */
GType*
g_type_interfaces (GType  type,
		   guint *n_interfaces)
{
  TypeNode *node;
  
  node = lookup_type_node_I (type);
  if (node && node->is_instantiatable)
    {
      IFaceEntries *entries;
      GType *ifaces;
      guint i;
      
      G_READ_LOCK (&type_rw_lock);
      entries = CLASSED_NODE_IFACES_ENTRIES_LOCKED (node);
      if (entries)
	{
	  ifaces = g_new (GType, IFACE_ENTRIES_N_ENTRIES (entries) + 1);
	  for (i = 0; i < IFACE_ENTRIES_N_ENTRIES (entries); i++)
	    ifaces[i] = entries->entry[i].iface_type;
	}
      else
	{
	  ifaces = g_new (GType, 1);
	  i = 0;
	}
      ifaces[i] = 0;
      
      if (n_interfaces)
	*n_interfaces = i;
      G_READ_UNLOCK (&type_rw_lock);
      
      return ifaces;
    }
  else
    {
      if (n_interfaces)
	*n_interfaces = 0;
      
      return NULL;
    }
}

typedef struct _QData QData;
struct _GData
{
  guint  n_qdatas;
  QData *qdatas;
};
struct _QData
{
  GQuark   quark;
  gpointer data;
};

static inline gpointer
type_get_qdata_L (TypeNode *node,
		  GQuark    quark)
{
  GData *gdata = node->global_gdata;
  
  if (quark && gdata && gdata->n_qdatas)
    {
      QData *qdatas = gdata->qdatas - 1;
      guint n_qdatas = gdata->n_qdatas;
      
      do
	{
	  guint i;
	  QData *check;
	  
	  i = (n_qdatas + 1) / 2;
	  check = qdatas + i;
	  if (quark == check->quark)
	    return check->data;
	  else if (quark > check->quark)
	    {
	      n_qdatas -= i;
	      qdatas = check;
	    }
	  else /* if (quark < check->quark) */
	    n_qdatas = i - 1;
	}
      while (n_qdatas);
    }
  return NULL;
}

/**
 * g_type_get_qdata:
 * @type: a #GType
 * @quark: a #GQuark id to identify the data
 *
 * Obtains data which has previously been attached to @type
 * with g_type_set_qdata().
 *
 * Note that this does not take subtyping into account; data
 * attached to one type with g_type_set_qdata() cannot
 * be retrieved from a subtype using g_type_get_qdata().
 *
 * Returns: (transfer none): the data, or %NULL if no data was found
 */
gpointer
g_type_get_qdata (GType  type,
		  GQuark quark)
{
  TypeNode *node;
  gpointer data;
  
  node = lookup_type_node_I (type);
  if (node)
    {
      G_READ_LOCK (&type_rw_lock);
      data = type_get_qdata_L (node, quark);
      G_READ_UNLOCK (&type_rw_lock);
    }
  else
    {
      g_return_val_if_fail (node != NULL, NULL);
      data = NULL;
    }
  return data;
}

static inline void
type_set_qdata_W (TypeNode *node,
		  GQuark    quark,
		  gpointer  data)
{
  GData *gdata;
  QData *qdata;
  guint i;
  
  /* setup qdata list if necessary */
  if (!node->global_gdata)
    node->global_gdata = g_new0 (GData, 1);
  gdata = node->global_gdata;
  
  /* try resetting old data */
  qdata = gdata->qdatas;
  for (i = 0; i < gdata->n_qdatas; i++)
    if (qdata[i].quark == quark)
      {
	qdata[i].data = data;
	return;
      }
  
  /* add new entry */
  gdata->n_qdatas++;
  gdata->qdatas = g_renew (QData, gdata->qdatas, gdata->n_qdatas);
  qdata = gdata->qdatas;
  for (i = 0; i < gdata->n_qdatas - 1; i++)
    if (qdata[i].quark > quark)
      break;
  memmove (qdata + i + 1, qdata + i, sizeof (qdata[0]) * (gdata->n_qdatas - i - 1));
  qdata[i].quark = quark;
  qdata[i].data = data;
}

/**
 * g_type_set_qdata:
 * @type: a #GType
 * @quark: a #GQuark id to identify the data
 * @data: the data
 *
 * Attaches arbitrary data to a type.
 */
void
g_type_set_qdata (GType    type,
		  GQuark   quark,
		  gpointer data)
{
  TypeNode *node;
  
  g_return_if_fail (quark != 0);
  
  node = lookup_type_node_I (type);
  if (node)
    {
      G_WRITE_LOCK (&type_rw_lock);
      type_set_qdata_W (node, quark, data);
      G_WRITE_UNLOCK (&type_rw_lock);
    }
  else
    g_return_if_fail (node != NULL);
}

static void
type_add_flags_W (TypeNode  *node,
		  GTypeFlags flags)
{
  guint dflags;
  
  g_return_if_fail ((flags & ~TYPE_FLAG_MASK) == 0);
  g_return_if_fail (node != NULL);
  
  if ((flags & TYPE_FLAG_MASK) && node->is_classed && node->data && node->data->class.class)
    g_warning ("tagging type '%s' as abstract after class initialization", NODE_NAME (node));
  dflags = GPOINTER_TO_UINT (type_get_qdata_L (node, static_quark_type_flags));
  dflags |= flags;
  type_set_qdata_W (node, static_quark_type_flags, GUINT_TO_POINTER (dflags));
}

/**
 * g_type_query:
 * @type: #GType of a static, classed type
 * @query: (out caller-allocates): a user provided structure that is
 *     filled in with constant values upon success
 *
 * Queries the type system for information about a specific type.
 * This function will fill in a user-provided structure to hold
 * type-specific information. If an invalid #GType is passed in, the
 * @type member of the #GTypeQuery is 0. All members filled into the
 * #GTypeQuery structure should be considered constant and have to be
 * left untouched.
 */
void
g_type_query (GType       type,
	      GTypeQuery *query)
{
  TypeNode *node;
  
  g_return_if_fail (query != NULL);
  
  /* if node is not static and classed, we won't allow query */
  query->type = 0;
  node = lookup_type_node_I (type);
  if (node && node->is_classed && !node->plugin)
    {
      /* type is classed and probably even instantiatable */
      G_READ_LOCK (&type_rw_lock);
      if (node->data)	/* type is static or referenced */
	{
	  query->type = NODE_TYPE (node);
	  query->type_name = NODE_NAME (node);
	  query->class_size = node->data->class.class_size;
	  query->instance_size = node->is_instantiatable ? node->data->instance.instance_size : 0;
	}
      G_READ_UNLOCK (&type_rw_lock);
    }
}

/**
 * g_type_get_instance_count:
 * @type: a #GType
 *
 * Returns the number of instances allocated of the particular type;
 * this is only available if GLib is built with debugging support and
 * the instance_count debug flag is set (by setting the GOBJECT_DEBUG
 * variable to include instance-count).
 *
 * Returns: the number of instances allocated of the given type;
 *   if instance counts are not available, returns 0.
 *
 * Since: 2.44
 */
int
g_type_get_instance_count (GType type)
{
#ifdef G_ENABLE_DEBUG
  TypeNode *node;

  node = lookup_type_node_I (type);
  g_return_val_if_fail (node != NULL, 0);

  return g_atomic_int_get (&node->instance_count);
#else
  return 0;
#endif
}

/* --- implementation details --- */
gboolean
g_type_test_flags (GType type,
		   guint flags)
{
  TypeNode *node;
  gboolean result = FALSE;
  
  node = lookup_type_node_I (type);
  if (node)
    {
      guint fflags = flags & TYPE_FUNDAMENTAL_FLAG_MASK;
      guint tflags = flags & TYPE_FLAG_MASK;
      
      if (fflags)
	{
	  GTypeFundamentalInfo *finfo = type_node_fundamental_info_I (node);
	  
	  fflags = (finfo->type_flags & fflags) == fflags;
	}
      else
	fflags = TRUE;
      
      if (tflags)
	{
	  G_READ_LOCK (&type_rw_lock);
	  tflags = (tflags & GPOINTER_TO_UINT (type_get_qdata_L (node, static_quark_type_flags))) == tflags;
	  G_READ_UNLOCK (&type_rw_lock);
	}
      else
	tflags = TRUE;
      
      result = tflags && fflags;
    }
  
  return result;
}

/**
 * g_type_get_plugin:
 * @type: #GType to retrieve the plugin for
 *
 * Returns the #GTypePlugin structure for @type.
 *
 * Returns: (transfer none): the corresponding plugin
 *     if @type is a dynamic type, %NULL otherwise
 */
GTypePlugin*
g_type_get_plugin (GType type)
{
  TypeNode *node;
  
  node = lookup_type_node_I (type);
  
  return node ? node->plugin : NULL;
}

/**
 * g_type_interface_get_plugin:
 * @instance_type: #GType of an instantiatable type
 * @interface_type: #GType of an interface type
 *
 * Returns the #GTypePlugin structure for the dynamic interface
 * @interface_type which has been added to @instance_type, or %NULL
 * if @interface_type has not been added to @instance_type or does
 * not have a #GTypePlugin structure. See g_type_add_interface_dynamic().
 *
 * Returns: (transfer none): the #GTypePlugin for the dynamic
 *     interface @interface_type of @instance_type
 */
GTypePlugin*
g_type_interface_get_plugin (GType instance_type,
			     GType interface_type)
{
  TypeNode *node;
  TypeNode *iface;
  
  g_return_val_if_fail (G_TYPE_IS_INTERFACE (interface_type), NULL);	/* G_TYPE_IS_INTERFACE() is an external call: _U */
  
  node = lookup_type_node_I (instance_type);  
  iface = lookup_type_node_I (interface_type);
  if (node && iface)
    {
      IFaceHolder *iholder;
      GTypePlugin *plugin;
      
      G_READ_LOCK (&type_rw_lock);
      
      iholder = iface_node_get_holders_L (iface);
      while (iholder && iholder->instance_type != instance_type)
	iholder = iholder->next;
      plugin = iholder ? iholder->plugin : NULL;
      
      G_READ_UNLOCK (&type_rw_lock);
      
      return plugin;
    }
  
  g_return_val_if_fail (node == NULL, NULL);
  g_return_val_if_fail (iface == NULL, NULL);
  
  g_warning (G_STRLOC ": attempt to look up plugin for invalid instance/interface type pair.");
  
  return NULL;
}

/**
 * g_type_fundamental_next:
 *
 * Returns the next free fundamental type id which can be used to
 * register a new fundamental type with g_type_register_fundamental().
 * The returned type ID represents the highest currently registered
 * fundamental type identifier.
 *
 * Returns: the next available fundamental type ID to be registered,
 *     or 0 if the type system ran out of fundamental type IDs
 */
GType
g_type_fundamental_next (void)
{
  GType type;
  
  G_READ_LOCK (&type_rw_lock);
  type = static_fundamental_next;
  G_READ_UNLOCK (&type_rw_lock);
  type = G_TYPE_MAKE_FUNDAMENTAL (type);
  return type <= G_TYPE_FUNDAMENTAL_MAX ? type : 0;
}

/**
 * g_type_fundamental:
 * @type_id: valid type ID
 * 
 * Internal function, used to extract the fundamental type ID portion.
 * Use G_TYPE_FUNDAMENTAL() instead.
 * 
 * Returns: fundamental type ID
 */
GType
g_type_fundamental (GType type_id)
{
  TypeNode *node = lookup_type_node_I (type_id);
  
  return node ? NODE_FUNDAMENTAL_TYPE (node) : 0;
}

gboolean
g_type_check_instance_is_a (GTypeInstance *type_instance,
			    GType          iface_type)
{
  TypeNode *node, *iface;
  gboolean check;
  
  if (!type_instance || !type_instance->g_class)
    return FALSE;
  
  node = lookup_type_node_I (type_instance->g_class->g_type);
  iface = lookup_type_node_I (iface_type);
  check = node && node->is_instantiatable && iface && type_node_conforms_to_U (node, iface, TRUE, FALSE);
  
  return check;
}

gboolean
g_type_check_instance_is_fundamentally_a (GTypeInstance *type_instance,
                                          GType          fundamental_type)
{
  TypeNode *node;
  if (!type_instance || !type_instance->g_class)
    return FALSE;
  node = lookup_type_node_I (type_instance->g_class->g_type);
  return node && (NODE_FUNDAMENTAL_TYPE(node) == fundamental_type);
}

gboolean
g_type_check_class_is_a (GTypeClass *type_class,
			 GType       is_a_type)
{
  TypeNode *node, *iface;
  gboolean check;
  
  if (!type_class)
    return FALSE;
  
  node = lookup_type_node_I (type_class->g_type);
  iface = lookup_type_node_I (is_a_type);
  check = node && node->is_classed && iface && type_node_conforms_to_U (node, iface, FALSE, FALSE);
  
  return check;
}

GTypeInstance*
g_type_check_instance_cast (GTypeInstance *type_instance,
			    GType          iface_type)
{
  if (type_instance)
    {
      if (type_instance->g_class)
	{
	  TypeNode *node, *iface;
	  gboolean is_instantiatable, check;
	  
	  node = lookup_type_node_I (type_instance->g_class->g_type);
	  is_instantiatable = node && node->is_instantiatable;
	  iface = lookup_type_node_I (iface_type);
	  check = is_instantiatable && iface && type_node_conforms_to_U (node, iface, TRUE, FALSE);
	  if (check)
	    return type_instance;
	  
	  if (is_instantiatable)
	    g_warning ("invalid cast from '%s' to '%s'",
		       type_descriptive_name_I (type_instance->g_class->g_type),
		       type_descriptive_name_I (iface_type));
	  else
	    g_warning ("invalid uninstantiatable type '%s' in cast to '%s'",
		       type_descriptive_name_I (type_instance->g_class->g_type),
		       type_descriptive_name_I (iface_type));
	}
      else
	g_warning ("invalid unclassed pointer in cast to '%s'",
		   type_descriptive_name_I (iface_type));
    }
  
  return type_instance;
}

GTypeClass*
g_type_check_class_cast (GTypeClass *type_class,
			 GType       is_a_type)
{
  if (type_class)
    {
      TypeNode *node, *iface;
      gboolean is_classed, check;
      
      node = lookup_type_node_I (type_class->g_type);
      is_classed = node && node->is_classed;
      iface = lookup_type_node_I (is_a_type);
      check = is_classed && iface && type_node_conforms_to_U (node, iface, FALSE, FALSE);
      if (check)
	return type_class;
      
      if (is_classed)
	g_warning ("invalid class cast from '%s' to '%s'",
		   type_descriptive_name_I (type_class->g_type),
		   type_descriptive_name_I (is_a_type));
      else
	g_warning ("invalid unclassed type '%s' in class cast to '%s'",
		   type_descriptive_name_I (type_class->g_type),
		   type_descriptive_name_I (is_a_type));
    }
  else
    g_warning ("invalid class cast from (NULL) pointer to '%s'",
	       type_descriptive_name_I (is_a_type));
  return type_class;
}

/**
 * g_type_check_instance:
 * @instance: a valid #GTypeInstance structure
 *
 * Private helper function to aid implementation of the
 * G_TYPE_CHECK_INSTANCE() macro.
 *
 * Returns: %TRUE if @instance is valid, %FALSE otherwise
 */
gboolean
g_type_check_instance (GTypeInstance *type_instance)
{
  /* this function is just here to make the signal system
   * conveniently elaborated on instance checks
   */
  if (type_instance)
    {
      if (type_instance->g_class)
	{
	  TypeNode *node = lookup_type_node_I (type_instance->g_class->g_type);
	  
	  if (node && node->is_instantiatable)
	    return TRUE;
	  
	  g_warning ("instance of invalid non-instantiatable type '%s'",
		     type_descriptive_name_I (type_instance->g_class->g_type));
	}
      else
	g_warning ("instance with invalid (NULL) class pointer");
    }
  else
    g_warning ("invalid (NULL) pointer instance");
  
  return FALSE;
}

static inline gboolean
type_check_is_value_type_U (GType type)
{
  GTypeFlags tflags = G_TYPE_FLAG_VALUE_ABSTRACT;
  TypeNode *node;
  
  /* common path speed up */
  node = lookup_type_node_I (type);
  if (node && node->mutatable_check_cache)
    return TRUE;
  
  G_READ_LOCK (&type_rw_lock);
 restart_check:
  if (node)
    {
      if (node->data && NODE_REFCOUNT (node) > 0 &&
	  node->data->common.value_table->value_init)
	tflags = GPOINTER_TO_UINT (type_get_qdata_L (node, static_quark_type_flags));
      else if (NODE_IS_IFACE (node))
	{
	  guint i;
	  
	  for (i = 0; i < IFACE_NODE_N_PREREQUISITES (node); i++)
	    {
	      GType prtype = IFACE_NODE_PREREQUISITES (node)[i];
	      TypeNode *prnode = lookup_type_node_I (prtype);
	      
	      if (prnode->is_instantiatable)
		{
		  type = prtype;
		  node = lookup_type_node_I (type);
		  goto restart_check;
		}
	    }
	}
    }
  G_READ_UNLOCK (&type_rw_lock);
  
  return !(tflags & G_TYPE_FLAG_VALUE_ABSTRACT);
}

gboolean
g_type_check_is_value_type (GType type)
{
  return type_check_is_value_type_U (type);
}

gboolean
g_type_check_value (const GValue *value)
{
  return value && type_check_is_value_type_U (value->g_type);
}

gboolean
g_type_check_value_holds (const GValue *value,
			  GType         type)
{
  return value && type_check_is_value_type_U (value->g_type) && g_type_is_a (value->g_type, type);
}

/**
 * g_type_value_table_peek: (skip)
 * @type: a #GType
 *
 * Returns the location of the #GTypeValueTable associated with @type.
 *
 * Note that this function should only be used from source code
 * that implements or has internal knowledge of the implementation of
 * @type.
 *
 * Returns: location of the #GTypeValueTable associated with @type or
 *     %NULL if there is no #GTypeValueTable associated with @type
 */
GTypeValueTable*
g_type_value_table_peek (GType type)
{
  GTypeValueTable *vtable = NULL;
  TypeNode *node = lookup_type_node_I (type);
  gboolean has_refed_data, has_table;

  if (node && NODE_REFCOUNT (node) && node->mutatable_check_cache)
    return node->data->common.value_table;

  G_READ_LOCK (&type_rw_lock);
  
 restart_table_peek:
  has_refed_data = node && node->data && NODE_REFCOUNT (node) > 0;
  has_table = has_refed_data && node->data->common.value_table->value_init;
  if (has_refed_data)
    {
      if (has_table)
	vtable = node->data->common.value_table;
      else if (NODE_IS_IFACE (node))
	{
	  guint i;
	  
	  for (i = 0; i < IFACE_NODE_N_PREREQUISITES (node); i++)
	    {
	      GType prtype = IFACE_NODE_PREREQUISITES (node)[i];
	      TypeNode *prnode = lookup_type_node_I (prtype);
	      
	      if (prnode->is_instantiatable)
		{
		  type = prtype;
		  node = lookup_type_node_I (type);
		  goto restart_table_peek;
		}
	    }
	}
    }
  
  G_READ_UNLOCK (&type_rw_lock);
  
  if (vtable)
    return vtable;
  
  if (!node)
    g_warning (G_STRLOC ": type id '%" G_GSIZE_FORMAT "' is invalid", type);
  if (!has_refed_data)
    g_warning ("can't peek value table for type '%s' which is not currently referenced",
	       type_descriptive_name_I (type));
  
  return NULL;
}

const gchar *
g_type_name_from_instance (GTypeInstance *instance)
{
  if (!instance)
    return "<NULL-instance>";
  else
    return g_type_name_from_class (instance->g_class);
}

const gchar *
g_type_name_from_class (GTypeClass *g_class)
{
  if (!g_class)
    return "<NULL-class>";
  else
    return g_type_name (g_class->g_type);
}


/* --- private api for gboxed.c --- */
gpointer
_g_type_boxed_copy (GType type, gpointer value)
{
  TypeNode *node = lookup_type_node_I (type);

  return node->data->boxed.copy_func (value);
}

void
_g_type_boxed_free (GType type, gpointer value)
{
  TypeNode *node = lookup_type_node_I (type);

  node->data->boxed.free_func (value);
}

void
_g_type_boxed_init (GType          type,
                    GBoxedCopyFunc copy_func,
                    GBoxedFreeFunc free_func)
{
  TypeNode *node = lookup_type_node_I (type);

  node->data->boxed.copy_func = copy_func;
  node->data->boxed.free_func = free_func;
}

/* --- initialization --- */
/**
 * g_type_init_with_debug_flags:
 * @debug_flags: bitwise combination of #GTypeDebugFlags values for
 *     debugging purposes
 *
 * This function used to initialise the type system with debugging
 * flags.  Since GLib 2.36, the type system is initialised automatically
 * and this function does nothing.
 *
 * If you need to enable debugging features, use the GOBJECT_DEBUG
 * environment variable.
 *
 * Deprecated: 2.36: the type system is now initialised automatically
 */
void
g_type_init_with_debug_flags (GTypeDebugFlags debug_flags)
{
  g_assert_type_system_initialized ();

  if (debug_flags)
    g_message ("g_type_init_with_debug_flags() is no longer supported.  Use the GOBJECT_DEBUG environment variable.");
}

/**
 * g_type_init:
 *
 * This function used to initialise the type system.  Since GLib 2.36,
 * the type system is initialised automatically and this function does
 * nothing.
 *
 * Deprecated: 2.36: the type system is now initialised automatically
 */
void
g_type_init (void)
{
  g_assert_type_system_initialized ();
}

static void
gobject_init (void)
{
  const gchar *env_string;
  GTypeInfo info;
  TypeNode *node;
  GType type;

  /* Ensure GLib is initialized first, see
   * https://bugzilla.gnome.org/show_bug.cgi?id=756139
   */
  GLIB_PRIVATE_CALL (glib_init) ();

  G_WRITE_LOCK (&type_rw_lock);

  /* setup GObject library wide debugging flags */
  env_string = g_getenv ("GOBJECT_DEBUG");
  if (env_string != NULL)
    {
      GDebugKey debug_keys[] = {
        { "objects", G_TYPE_DEBUG_OBJECTS },
        { "instance-count", G_TYPE_DEBUG_INSTANCE_COUNT },
        { "signals", G_TYPE_DEBUG_SIGNALS },
      };

      _g_type_debug_flags = g_parse_debug_string (env_string, debug_keys, G_N_ELEMENTS (debug_keys));
    }

  /* quarks */
  static_quark_type_flags = g_quark_from_static_string ("-g-type-private--GTypeFlags");
  static_quark_iface_holder = g_quark_from_static_string ("-g-type-private--IFaceHolder");
  static_quark_dependants_array = g_quark_from_static_string ("-g-type-private--dependants-array");

  /* type qname hash table */
  static_type_nodes_ht = g_hash_table_new (g_str_hash, g_str_equal);

  /* invalid type G_TYPE_INVALID (0)
   */
  static_fundamental_type_nodes[0] = NULL;

  /* void type G_TYPE_NONE
   */
  node = type_node_fundamental_new_W (G_TYPE_NONE, g_intern_static_string ("void"), 0);
  type = NODE_TYPE (node);
  g_assert (type == G_TYPE_NONE);

  /* interface fundamental type G_TYPE_INTERFACE (!classed)
   */
  memset (&info, 0, sizeof (info));
  node = type_node_fundamental_new_W (G_TYPE_INTERFACE, g_intern_static_string ("GInterface"), G_TYPE_FLAG_DERIVABLE);
  type = NODE_TYPE (node);
  type_data_make_W (node, &info, NULL);
  g_assert (type == G_TYPE_INTERFACE);

  G_WRITE_UNLOCK (&type_rw_lock);

  _g_value_c_init ();

  /* G_TYPE_TYPE_PLUGIN
   */
  g_type_ensure (g_type_plugin_get_type ());

  /* G_TYPE_* value types
   */
  _g_value_types_init ();

  /* G_TYPE_ENUM & G_TYPE_FLAGS
   */
  _g_enum_types_init ();

  /* G_TYPE_BOXED
   */
  _g_boxed_type_init ();

  /* G_TYPE_PARAM
   */
  _g_param_type_init ();

  /* G_TYPE_OBJECT
   */
  _g_object_type_init ();

  /* G_TYPE_PARAM_* pspec types
   */
  _g_param_spec_types_init ();

  /* Value Transformations
   */
  _g_value_transforms_init ();

  /* Signal system
   */
  _g_signal_init ();
}

#if defined (G_OS_WIN32)

BOOL WINAPI DllMain (HINSTANCE hinstDLL,
                     DWORD     fdwReason,
                     LPVOID    lpvReserved);

BOOL WINAPI
DllMain (HINSTANCE hinstDLL,
         DWORD     fdwReason,
         LPVOID    lpvReserved)
{
  switch (fdwReason)
    {
    case DLL_PROCESS_ATTACH:
      gobject_init ();
      break;

    default:
      /* do nothing */
      ;
    }

  return TRUE;
}

#elif defined (G_HAS_CONSTRUCTORS)
#ifdef G_DEFINE_CONSTRUCTOR_NEEDS_PRAGMA
#pragma G_DEFINE_CONSTRUCTOR_PRAGMA_ARGS(gobject_init_ctor)
#endif
G_DEFINE_CONSTRUCTOR(gobject_init_ctor)

static void
gobject_init_ctor (void)
{
  gobject_init ();
}

#else
# error Your platform/compiler is missing constructor support
#endif

/**
 * g_type_class_add_private:
 * @g_class: (type GObject.TypeClass): class structure for an instantiatable
 *    type
 * @private_size: size of private structure
 *
 * Registers a private structure for an instantiatable type.
 *
 * When an object is allocated, the private structures for
 * the type and all of its parent types are allocated
 * sequentially in the same memory block as the public
 * structures, and are zero-filled.
 *
 * Note that the accumulated size of the private structures of
 * a type and all its parent types cannot exceed 64 KiB.
 *
 * This function should be called in the type's class_init() function.
 * The private structure can be retrieved using the
 * G_TYPE_INSTANCE_GET_PRIVATE() macro.
 *
 * The following example shows attaching a private structure
 * MyObjectPrivate to an object MyObject defined in the standard
 * GObject fashion in the type's class_init() function.
 *
 * Note the use of a structure member "priv" to avoid the overhead
 * of repeatedly calling MY_OBJECT_GET_PRIVATE().
 *
 * |[<!-- language="C" --> 
 * typedef struct _MyObject        MyObject;
 * typedef struct _MyObjectPrivate MyObjectPrivate;
 *
 * struct _MyObject {
 *  GObject parent;
 *
 *  MyObjectPrivate *priv;
 * };
 *
 * struct _MyObjectPrivate {
 *   int some_field;
 * };
 *
 * static void
 * my_object_class_init (MyObjectClass *klass)
 * {
 *   g_type_class_add_private (klass, sizeof (MyObjectPrivate));
 * }
 *
 * static void
 * my_object_init (MyObject *my_object)
 * {
 *   my_object->priv = G_TYPE_INSTANCE_GET_PRIVATE (my_object,
 *                                                  MY_TYPE_OBJECT,
 *                                                  MyObjectPrivate);
 *   // my_object->priv->some_field will be automatically initialised to 0
 * }
 *
 * static int
 * my_object_get_some_field (MyObject *my_object)
 * {
 *   MyObjectPrivate *priv;
 *
 *   g_return_val_if_fail (MY_IS_OBJECT (my_object), 0);
 *
 *   priv = my_object->priv;
 *
 *   return priv->some_field;
 * }
 * ]|
 *
 * Since: 2.4
 */
void
g_type_class_add_private (gpointer g_class,
			  gsize    private_size)
{
  GType instance_type = ((GTypeClass *)g_class)->g_type;
  TypeNode *node = lookup_type_node_I (instance_type);

  g_return_if_fail (private_size > 0);
  g_return_if_fail (private_size <= 0xffff);

  if (!node || !node->is_instantiatable || !node->data || node->data->class.class != g_class)
    {
      g_warning ("cannot add private field to invalid (non-instantiatable) type '%s'",
		 type_descriptive_name_I (instance_type));
      return;
    }

  if (NODE_PARENT_TYPE (node))
    {
      TypeNode *pnode = lookup_type_node_I (NODE_PARENT_TYPE (node));
      if (node->data->instance.private_size != pnode->data->instance.private_size)
	{
	  g_warning ("g_type_class_add_private() called multiple times for the same type");
	  return;
	}
    }
  
  G_WRITE_LOCK (&type_rw_lock);

  private_size = ALIGN_STRUCT (node->data->instance.private_size + private_size);
  g_assert (private_size <= 0xffff);
  node->data->instance.private_size = private_size;
  
  G_WRITE_UNLOCK (&type_rw_lock);
}

/* semi-private, called only by the G_ADD_PRIVATE macro */
gint
g_type_add_instance_private (GType class_gtype,
                             gsize private_size)
{
  TypeNode *node = lookup_type_node_I (class_gtype);

  g_return_val_if_fail (private_size > 0, 0);
  g_return_val_if_fail (private_size <= 0xffff, 0);

  if (!node || !node->is_classed || !node->is_instantiatable || !node->data)
    {
      g_warning ("cannot add private field to invalid (non-instantiatable) type '%s'",
		 type_descriptive_name_I (class_gtype));
      return 0;
    }

  if (node->plugin != NULL)
    {
      g_warning ("cannot use g_type_add_instance_private() with dynamic type '%s'",
                 type_descriptive_name_I (class_gtype));
      return 0;
    }

  /* in the future, we want to register the private data size of a type
   * directly from the get_type() implementation so that we can take full
   * advantage of the type definition macros that we already have.
   *
   * unfortunately, this does not behave correctly if a class in the middle
   * of the type hierarchy uses the "old style" of private data registration
   * from the class_init() implementation, as the private data offset is not
   * going to be known until the full class hierarchy is initialized.
   *
   * in order to transition our code to the Glorious New Future™, we proceed
   * with a two-step implementation: first, we provide this new function to
   * register the private data size in the get_type() implementation and we
   * hide it behind a macro. the function will return the private size, instead
   * of the offset, which will be stored inside a static variable defined by
   * the G_DEFINE_TYPE_EXTENDED macro. the G_DEFINE_TYPE_EXTENDED macro will
   * check the variable and call g_type_class_add_instance_private(), which
   * will use the data size and actually register the private data, then
   * return the computed offset of the private data, which will be stored
   * inside the static variable, so we can use it to retrieve the pointer
   * to the private data structure.
   *
   * once all our code has been migrated to the new idiomatic form of private
   * data registration, we will change the g_type_add_instance_private()
   * function to actually perform the registration and return the offset
   * of the private data; g_type_class_add_instance_private() already checks
   * if the passed argument is negative (meaning that it's an offset in the
   * GTypeInstance allocation) and becomes a no-op if that's the case. this
   * should make the migration fully transparent even if we're effectively
   * copying this macro into everybody's code.
   */
  return private_size;
}

/* semi-private function, should only be used by G_DEFINE_TYPE_EXTENDED */
void
g_type_class_adjust_private_offset (gpointer  g_class,
                                    gint     *private_size_or_offset)
{
  GType class_gtype = ((GTypeClass *) g_class)->g_type;
  TypeNode *node = lookup_type_node_I (class_gtype);
  gssize private_size;

  g_return_if_fail (private_size_or_offset != NULL);

  /* if we have been passed the offset instead of the private data size,
   * then we consider this as a no-op, and just return the value. see the
   * comment in g_type_add_instance_private() for the full explanation.
   */
  if (*private_size_or_offset > 0)
    g_return_if_fail (*private_size_or_offset <= 0xffff);
  else
    return;

  if (!node || !node->is_classed || !node->is_instantiatable || !node->data)
    {
      g_warning ("cannot add private field to invalid (non-instantiatable) type '%s'",
		 type_descriptive_name_I (class_gtype));
      *private_size_or_offset = 0;
      return;
    }

  if (NODE_PARENT_TYPE (node))
    {
      TypeNode *pnode = lookup_type_node_I (NODE_PARENT_TYPE (node));
      if (node->data->instance.private_size != pnode->data->instance.private_size)
	{
	  g_warning ("g_type_add_instance_private() called multiple times for the same type");
          *private_size_or_offset = 0;
	  return;
	}
    }

  G_WRITE_LOCK (&type_rw_lock);

  private_size = ALIGN_STRUCT (node->data->instance.private_size + *private_size_or_offset);
  g_assert (private_size <= 0xffff);
  node->data->instance.private_size = private_size;

  *private_size_or_offset = -(gint) node->data->instance.private_size;

  G_WRITE_UNLOCK (&type_rw_lock);
}

gpointer
g_type_instance_get_private (GTypeInstance *instance,
			     GType          private_type)
{
  TypeNode *node;

  g_return_val_if_fail (instance != NULL && instance->g_class != NULL, NULL);

  node = lookup_type_node_I (private_type);
  if (G_UNLIKELY (!node || !node->is_instantiatable))
    {
      g_warning ("instance of invalid non-instantiatable type '%s'",
                 type_descriptive_name_I (instance->g_class->g_type));
      return NULL;
    }

  return ((gchar *) instance) - node->data->instance.private_size;
}

/**
 * g_type_class_get_instance_private_offset: (skip)
 * @g_class: (type GObject.TypeClass): a #GTypeClass
 *
 * Gets the offset of the private data for instances of @g_class.
 *
 * This is how many bytes you should add to the instance pointer of a
 * class in order to get the private data for the type represented by
 * @g_class.
 *
 * You can only call this function after you have registered a private
 * data area for @g_class using g_type_class_add_private().
 *
 * Returns: the offset, in bytes
 *
 * Since: 2.38
 **/
gint
g_type_class_get_instance_private_offset (gpointer g_class)
{
  GType instance_type;
  guint16 parent_size;
  TypeNode *node;

  g_assert (g_class != NULL);

  instance_type = ((GTypeClass *) g_class)->g_type;
  node = lookup_type_node_I (instance_type);

  g_assert (node != NULL);
  g_assert (node->is_instantiatable);

  if (NODE_PARENT_TYPE (node))
    {
      TypeNode *pnode = lookup_type_node_I (NODE_PARENT_TYPE (node));

      parent_size = pnode->data->instance.private_size;
    }
  else
    parent_size = 0;

  if (node->data->instance.private_size == parent_size)
    g_error ("g_type_class_get_instance_private_offset() called on class %s but it has no private data",
             g_type_name (instance_type));

  return -(gint) node->data->instance.private_size;
}

/**
 * g_type_add_class_private:
 * @class_type: GType of an classed type
 * @private_size: size of private structure
 *
 * Registers a private class structure for a classed type;
 * when the class is allocated, the private structures for
 * the class and all of its parent types are allocated
 * sequentially in the same memory block as the public
 * structures, and are zero-filled.
 *
 * This function should be called in the
 * type's get_type() function after the type is registered.
 * The private structure can be retrieved using the
 * G_TYPE_CLASS_GET_PRIVATE() macro.
 *
 * Since: 2.24
 */
void
g_type_add_class_private (GType    class_type,
			  gsize    private_size)
{
  TypeNode *node = lookup_type_node_I (class_type);
  gsize offset;

  g_return_if_fail (private_size > 0);

  if (!node || !node->is_classed || !node->data)
    {
      g_warning ("cannot add class private field to invalid type '%s'",
		 type_descriptive_name_I (class_type));
      return;
    }

  if (NODE_PARENT_TYPE (node))
    {
      TypeNode *pnode = lookup_type_node_I (NODE_PARENT_TYPE (node));
      if (node->data->class.class_private_size != pnode->data->class.class_private_size)
	{
	  g_warning ("g_type_add_class_private() called multiple times for the same type");
	  return;
	}
    }
  
  G_WRITE_LOCK (&type_rw_lock);

  offset = ALIGN_STRUCT (node->data->class.class_private_size);
  node->data->class.class_private_size = offset + private_size;

  G_WRITE_UNLOCK (&type_rw_lock);
}

gpointer
g_type_class_get_private (GTypeClass *klass,
			  GType       private_type)
{
  TypeNode *class_node;
  TypeNode *private_node;
  TypeNode *parent_node;
  gsize offset;

  g_return_val_if_fail (klass != NULL, NULL);

  class_node = lookup_type_node_I (klass->g_type);
  if (G_UNLIKELY (!class_node || !class_node->is_classed))
    {
      g_warning ("class of invalid type '%s'",
		 type_descriptive_name_I (klass->g_type));
      return NULL;
    }

  private_node = lookup_type_node_I (private_type);
  if (G_UNLIKELY (!private_node || !NODE_IS_ANCESTOR (private_node, class_node)))
    {
      g_warning ("attempt to retrieve private data for invalid type '%s'",
		 type_descriptive_name_I (private_type));
      return NULL;
    }

  offset = ALIGN_STRUCT (class_node->data->class.class_size);

  if (NODE_PARENT_TYPE (private_node))
    {
      parent_node = lookup_type_node_I (NODE_PARENT_TYPE (private_node));
      g_assert (parent_node->data && NODE_REFCOUNT (parent_node) > 0);

      if (G_UNLIKELY (private_node->data->class.class_private_size == parent_node->data->class.class_private_size))
	{
	  g_warning ("g_type_instance_get_class_private() requires a prior call to g_type_add_class_private()");
	  return NULL;
	}

      offset += ALIGN_STRUCT (parent_node->data->class.class_private_size);
    }

  return G_STRUCT_MEMBER_P (klass, offset);
}

/**
 * g_type_ensure:
 * @type: a #GType
 *
 * Ensures that the indicated @type has been registered with the
 * type system, and its _class_init() method has been run.
 *
 * In theory, simply calling the type's _get_type() method (or using
 * the corresponding macro) is supposed take care of this. However,
 * _get_type() methods are often marked %G_GNUC_CONST for performance
 * reasons, even though this is technically incorrect (since
 * %G_GNUC_CONST requires that the function not have side effects,
 * which _get_type() methods do on the first call). As a result, if
 * you write a bare call to a _get_type() macro, it may get optimized
 * out by the compiler. Using g_type_ensure() guarantees that the
 * type's _get_type() method is called.
 *
 * Since: 2.34
 */
void
g_type_ensure (GType type)
{
  /* In theory, @type has already been resolved and so there's nothing
   * to do here. But this protects us in the case where the function
   * gets inlined (as it might in gobject_init_ctor() above).
   */
  if (G_UNLIKELY (type == (GType)-1))
    g_error ("can't happen");
}

