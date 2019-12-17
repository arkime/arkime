/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * gdataset.c: Generic dataset mechanism, similar to GtkObject data.
 * Copyright (C) 1998 Tim Janik
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
 * Modified by the GLib Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GLib Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GLib at ftp://ftp.gtk.org/pub/gtk/.
 */

/*
 * MT safe ; except for g_data*_foreach()
 */

#include "config.h"

#include <string.h>

#include "gdataset.h"
#include "gbitlock.h"

#include "gslice.h"
#include "gdatasetprivate.h"
#include "ghash.h"
#include "gquark.h"
#include "gstrfuncs.h"
#include "gtestutils.h"
#include "gthread.h"
#include "glib_trace.h"

/**
 * SECTION:datasets
 * @title: Datasets
 * @short_description: associate groups of data elements with
 *                     particular memory locations
 *
 * Datasets associate groups of data elements with particular memory
 * locations. These are useful if you need to associate data with a
 * structure returned from an external library. Since you cannot modify
 * the structure, you use its location in memory as the key into a
 * dataset, where you can associate any number of data elements with it.
 *
 * There are two forms of most of the dataset functions. The first form
 * uses strings to identify the data elements associated with a
 * location. The second form uses #GQuark identifiers, which are
 * created with a call to g_quark_from_string() or
 * g_quark_from_static_string(). The second form is quicker, since it
 * does not require looking up the string in the hash table of #GQuark
 * identifiers.
 *
 * There is no function to create a dataset. It is automatically
 * created as soon as you add elements to it.
 *
 * To add data elements to a dataset use g_dataset_id_set_data(),
 * g_dataset_id_set_data_full(), g_dataset_set_data() and
 * g_dataset_set_data_full().
 *
 * To get data elements from a dataset use g_dataset_id_get_data() and
 * g_dataset_get_data().
 *
 * To iterate over all data elements in a dataset use
 * g_dataset_foreach() (not thread-safe).
 *
 * To remove data elements from a dataset use
 * g_dataset_id_remove_data() and g_dataset_remove_data().
 *
 * To destroy a dataset, use g_dataset_destroy().
 **/

/**
 * SECTION:datalist
 * @title: Keyed Data Lists
 * @short_description: lists of data elements which are accessible by a
 *                     string or GQuark identifier
 *
 * Keyed data lists provide lists of arbitrary data elements which can
 * be accessed either with a string or with a #GQuark corresponding to
 * the string.
 *
 * The #GQuark methods are quicker, since the strings have to be
 * converted to #GQuarks anyway.
 *
 * Data lists are used for associating arbitrary data with #GObjects,
 * using g_object_set_data() and related functions.
 *
 * To create a datalist, use g_datalist_init().
 *
 * To add data elements to a datalist use g_datalist_id_set_data(),
 * g_datalist_id_set_data_full(), g_datalist_set_data() and
 * g_datalist_set_data_full().
 *
 * To get data elements from a datalist use g_datalist_id_get_data()
 * and g_datalist_get_data().
 *
 * To iterate over all data elements in a datalist use
 * g_datalist_foreach() (not thread-safe).
 *
 * To remove data elements from a datalist use
 * g_datalist_id_remove_data() and g_datalist_remove_data().
 *
 * To remove all data elements from a datalist, use g_datalist_clear().
 **/

/**
 * GData:
 *
 * The #GData struct is an opaque data structure to represent a
 * [Keyed Data List][glib-Keyed-Data-Lists]. It should only be
 * accessed via the following functions.
 **/

/**
 * GDestroyNotify:
 * @data: the data element.
 *
 * Specifies the type of function which is called when a data element
 * is destroyed. It is passed the pointer to the data element and
 * should free any memory and resources allocated for it.
 **/

#define G_DATALIST_FLAGS_MASK_INTERNAL 0x7

/* datalist pointer accesses have to be carried out atomically */
#define G_DATALIST_GET_POINTER(datalist)						\
  ((GData*) ((gsize) g_atomic_pointer_get (datalist) & ~(gsize) G_DATALIST_FLAGS_MASK_INTERNAL))

#define G_DATALIST_SET_POINTER(datalist, pointer)       G_STMT_START {                  \
  gpointer _oldv, _newv;                                                                \
  do {                                                                                  \
    _oldv = g_atomic_pointer_get (datalist);                                            \
    _newv = (gpointer) (((gsize) _oldv & G_DATALIST_FLAGS_MASK_INTERNAL) | (gsize) pointer);     \
  } while (!g_atomic_pointer_compare_and_exchange ((void**) datalist, _oldv, _newv));   \
} G_STMT_END

/* --- structures --- */
typedef struct {
  GQuark          key;
  gpointer        data;
  GDestroyNotify  destroy;
} GDataElt;

typedef struct _GDataset GDataset;
struct _GData
{
  guint32  len;     /* Number of elements */
  guint32  alloc;   /* Number of allocated elements */
  GDataElt data[1]; /* Flexible array */
};

struct _GDataset
{
  gconstpointer location;
  GData        *datalist;
};


/* --- prototypes --- */
static inline GDataset*	g_dataset_lookup		(gconstpointer	  dataset_location);
static inline void	g_datalist_clear_i		(GData		**datalist);
static void		g_dataset_destroy_internal	(GDataset	 *dataset);
static inline gpointer	g_data_set_internal		(GData     	**datalist,
							 GQuark   	  key_id,
							 gpointer         data,
							 GDestroyNotify   destroy_func,
							 GDataset	 *dataset);
static void		g_data_initialize		(void);

/* Locking model:
 * Each standalone GDataList is protected by a bitlock in the datalist pointer,
 * which protects that modification of the non-flags part of the datalist pointer
 * and the contents of the datalist.
 *
 * For GDataSet we have a global lock g_dataset_global that protects
 * the global dataset hash and cache, and additionally it protects the
 * datalist such that we can avoid to use the bit lock in a few places
 * where it is easy.
 */

/* --- variables --- */
G_LOCK_DEFINE_STATIC (g_dataset_global);
static GHashTable   *g_dataset_location_ht = NULL;
static GDataset     *g_dataset_cached = NULL; /* should this be
						 thread specific? */

/* --- functions --- */

#define DATALIST_LOCK_BIT 2

static void
g_datalist_lock (GData **datalist)
{
  g_pointer_bit_lock ((void **)datalist, DATALIST_LOCK_BIT);
}

static void
g_datalist_unlock (GData **datalist)
{
  g_pointer_bit_unlock ((void **)datalist, DATALIST_LOCK_BIT);
}

/* Called with the datalist lock held, or the dataset global
 * lock for dataset lists
 */
static void
g_datalist_clear_i (GData **datalist)
{
  GData *data;
  gint i;

  data = G_DATALIST_GET_POINTER (datalist);
  G_DATALIST_SET_POINTER (datalist, NULL);

  if (data)
    {
      G_UNLOCK (g_dataset_global);
      for (i = 0; i < data->len; i++)
        {
          if (data->data[i].data && data->data[i].destroy)
            data->data[i].destroy (data->data[i].data);
        }
      G_LOCK (g_dataset_global);

      g_free (data);
    }

}

/**
 * g_datalist_clear: (skip)
 * @datalist: a datalist.
 *
 * Frees all the data elements of the datalist.
 * The data elements' destroy functions are called
 * if they have been set.
 **/
void
g_datalist_clear (GData **datalist)
{
  GData *data;
  gint i;

  g_return_if_fail (datalist != NULL);

  g_datalist_lock (datalist);

  data = G_DATALIST_GET_POINTER (datalist);
  G_DATALIST_SET_POINTER (datalist, NULL);

  g_datalist_unlock (datalist);

  if (data)
    {
      for (i = 0; i < data->len; i++)
        {
          if (data->data[i].data && data->data[i].destroy)
            data->data[i].destroy (data->data[i].data);
        }

      g_free (data);
    }
}

/* HOLDS: g_dataset_global_lock */
static inline GDataset*
g_dataset_lookup (gconstpointer	dataset_location)
{
  GDataset *dataset;
  
  if (g_dataset_cached && g_dataset_cached->location == dataset_location)
    return g_dataset_cached;
  
  dataset = g_hash_table_lookup (g_dataset_location_ht, dataset_location);
  if (dataset)
    g_dataset_cached = dataset;
  
  return dataset;
}

/* HOLDS: g_dataset_global_lock */
static void
g_dataset_destroy_internal (GDataset *dataset)
{
  gconstpointer dataset_location;
  
  dataset_location = dataset->location;
  while (dataset)
    {
      if (G_DATALIST_GET_POINTER(&dataset->datalist) == NULL)
	{
	  if (dataset == g_dataset_cached)
	    g_dataset_cached = NULL;
	  g_hash_table_remove (g_dataset_location_ht, dataset_location);
	  g_slice_free (GDataset, dataset);
	  break;
	}
      
      g_datalist_clear_i (&dataset->datalist);
      dataset = g_dataset_lookup (dataset_location);
    }
}

/**
 * g_dataset_destroy:
 * @dataset_location: (not nullable): the location identifying the dataset.
 *
 * Destroys the dataset, freeing all memory allocated, and calling any
 * destroy functions set for data elements.
 */
void
g_dataset_destroy (gconstpointer  dataset_location)
{
  g_return_if_fail (dataset_location != NULL);
  
  G_LOCK (g_dataset_global);
  if (g_dataset_location_ht)
    {
      GDataset *dataset;

      dataset = g_dataset_lookup (dataset_location);
      if (dataset)
	g_dataset_destroy_internal (dataset);
    }
  G_UNLOCK (g_dataset_global);
}

/* HOLDS: g_dataset_global_lock if dataset != null */
static inline gpointer
g_data_set_internal (GData	  **datalist,
		     GQuark         key_id,
		     gpointer       new_data,
		     GDestroyNotify new_destroy_func,
		     GDataset	   *dataset)
{
  GData *d, *old_d;
  GDataElt old, *data, *data_last, *data_end;

  g_datalist_lock (datalist);

  d = G_DATALIST_GET_POINTER (datalist);

  if (new_data == NULL) /* remove */
    {
      if (d)
	{
	  data = d->data;
	  data_last = data + d->len - 1;
	  while (data <= data_last)
	    {
	      if (data->key == key_id)
		{
		  old = *data;
		  if (data != data_last)
		    *data = *data_last;
		  d->len--;

		  /* We don't bother to shrink, but if all data are now gone
		   * we at least free the memory
                   */
		  if (d->len == 0)
		    {
		      G_DATALIST_SET_POINTER (datalist, NULL);
		      g_free (d);
		      /* datalist may be situated in dataset, so must not be
		       * unlocked after we free it
		       */
		      g_datalist_unlock (datalist);

		      /* the dataset destruction *must* be done
		       * prior to invocation of the data destroy function
		       */
		      if (dataset)
			g_dataset_destroy_internal (dataset);
		    }
		  else
		    {
		      g_datalist_unlock (datalist);
		    }

		  /* We found and removed an old value
		   * the GData struct *must* already be unlinked
		   * when invoking the destroy function.
		   * we use (new_data==NULL && new_destroy_func!=NULL) as
		   * a special hint combination to "steal"
		   * data without destroy notification
		   */
		  if (old.destroy && !new_destroy_func)
		    {
		      if (dataset)
			G_UNLOCK (g_dataset_global);
		      old.destroy (old.data);
		      if (dataset)
			G_LOCK (g_dataset_global);
		      old.data = NULL;
		    }

		  return old.data;
		}
	      data++;
	    }
	}
    }
  else
    {
      old.data = NULL;
      if (d)
	{
	  data = d->data;
	  data_end = data + d->len;
	  while (data < data_end)
	    {
	      if (data->key == key_id)
		{
		  if (!data->destroy)
		    {
		      data->data = new_data;
		      data->destroy = new_destroy_func;
		      g_datalist_unlock (datalist);
		    }
		  else
		    {
		      old = *data;
		      data->data = new_data;
		      data->destroy = new_destroy_func;

		      g_datalist_unlock (datalist);

		      /* We found and replaced an old value
		       * the GData struct *must* already be unlinked
		       * when invoking the destroy function.
		       */
		      if (dataset)
			G_UNLOCK (g_dataset_global);
		      old.destroy (old.data);
		      if (dataset)
			G_LOCK (g_dataset_global);
		    }
		  return NULL;
		}
	      data++;
	    }
	}

      /* The key was not found, insert it */
      old_d = d;
      if (d == NULL)
	{
	  d = g_malloc (sizeof (GData));
	  d->len = 0;
	  d->alloc = 1;
	}
      else if (d->len == d->alloc)
	{
	  d->alloc = d->alloc * 2;
	  d = g_realloc (d, sizeof (GData) + (d->alloc - 1) * sizeof (GDataElt));
	}
      if (old_d != d)
	G_DATALIST_SET_POINTER (datalist, d);

      d->data[d->len].key = key_id;
      d->data[d->len].data = new_data;
      d->data[d->len].destroy = new_destroy_func;
      d->len++;
    }

  g_datalist_unlock (datalist);

  return NULL;

}

/**
 * g_dataset_id_set_data_full: (skip)
 * @dataset_location: (not nullable): the location identifying the dataset.
 * @key_id: the #GQuark id to identify the data element.
 * @data: the data element.
 * @destroy_func: the function to call when the data element is
 *                removed. This function will be called with the data
 *                element and can be used to free any memory allocated
 *                for it.
 *
 * Sets the data element associated with the given #GQuark id, and also
 * the function to call when the data element is destroyed. Any
 * previous data with the same key is removed, and its destroy function
 * is called.
 **/
/**
 * g_dataset_set_data_full: (skip)
 * @l: the location identifying the dataset.
 * @k: the string to identify the data element.
 * @d: the data element.
 * @f: the function to call when the data element is removed. This
 *     function will be called with the data element and can be used to
 *     free any memory allocated for it.
 *
 * Sets the data corresponding to the given string identifier, and the
 * function to call when the data element is destroyed.
 **/
/**
 * g_dataset_id_set_data:
 * @l: the location identifying the dataset.
 * @k: the #GQuark id to identify the data element.
 * @d: the data element.
 *
 * Sets the data element associated with the given #GQuark id. Any
 * previous data with the same key is removed, and its destroy function
 * is called.
 **/
/**
 * g_dataset_set_data:
 * @l: the location identifying the dataset.
 * @k: the string to identify the data element.
 * @d: the data element.
 *
 * Sets the data corresponding to the given string identifier.
 **/
/**
 * g_dataset_id_remove_data:
 * @l: the location identifying the dataset.
 * @k: the #GQuark id identifying the data element.
 *
 * Removes a data element from a dataset. The data element's destroy
 * function is called if it has been set.
 **/
/**
 * g_dataset_remove_data:
 * @l: the location identifying the dataset.
 * @k: the string identifying the data element.
 *
 * Removes a data element corresponding to a string. Its destroy
 * function is called if it has been set.
 **/
void
g_dataset_id_set_data_full (gconstpointer  dataset_location,
			    GQuark         key_id,
			    gpointer       data,
			    GDestroyNotify destroy_func)
{
  GDataset *dataset;
  
  g_return_if_fail (dataset_location != NULL);
  if (!data)
    g_return_if_fail (destroy_func == NULL);
  if (!key_id)
    {
      if (data)
	g_return_if_fail (key_id > 0);
      else
	return;
    }
  
  G_LOCK (g_dataset_global);
  if (!g_dataset_location_ht)
    g_data_initialize ();
 
  dataset = g_dataset_lookup (dataset_location);
  if (!dataset)
    {
      dataset = g_slice_new (GDataset);
      dataset->location = dataset_location;
      g_datalist_init (&dataset->datalist);
      g_hash_table_insert (g_dataset_location_ht, 
			   (gpointer) dataset->location,
			   dataset);
    }
  
  g_data_set_internal (&dataset->datalist, key_id, data, destroy_func, dataset);
  G_UNLOCK (g_dataset_global);
}

/**
 * g_datalist_id_set_data_full: (skip)
 * @datalist: a datalist.
 * @key_id: the #GQuark to identify the data element.
 * @data: (nullable): the data element or %NULL to remove any previous element
 *        corresponding to @key_id.
 * @destroy_func: (nullable): the function to call when the data element is
 *                removed. This function will be called with the data
 *                element and can be used to free any memory allocated
 *                for it. If @data is %NULL, then @destroy_func must
 *                also be %NULL.
 *
 * Sets the data corresponding to the given #GQuark id, and the
 * function to be called when the element is removed from the datalist.
 * Any previous data with the same key is removed, and its destroy
 * function is called.
 **/
/**
 * g_datalist_set_data_full: (skip)
 * @dl: a datalist.
 * @k: the string to identify the data element.
 * @d: (nullable): the data element, or %NULL to remove any previous element
 *     corresponding to @k.
 * @f: (nullable): the function to call when the data element is removed.
 *     This function will be called with the data element and can be used to
 *     free any memory allocated for it. If @d is %NULL, then @f must
 *     also be %NULL.
 *
 * Sets the data element corresponding to the given string identifier,
 * and the function to be called when the data element is removed.
 **/
/**
 * g_datalist_id_set_data:
 * @dl: a datalist.
 * @q: the #GQuark to identify the data element.
 * @d: (nullable): the data element, or %NULL to remove any previous element
 *     corresponding to @q.
 *
 * Sets the data corresponding to the given #GQuark id. Any previous
 * data with the same key is removed, and its destroy function is
 * called.
 **/
/**
 * g_datalist_set_data:
 * @dl: a datalist.
 * @k: the string to identify the data element.
 * @d: (nullable): the data element, or %NULL to remove any previous element
 *     corresponding to @k.
 *
 * Sets the data element corresponding to the given string identifier.
 **/
/**
 * g_datalist_id_remove_data:
 * @dl: a datalist.
 * @q: the #GQuark identifying the data element.
 *
 * Removes an element, using its #GQuark identifier.
 **/
/**
 * g_datalist_remove_data:
 * @dl: a datalist.
 * @k: the string identifying the data element.
 *
 * Removes an element using its string identifier. The data element's
 * destroy function is called if it has been set.
 **/
void
g_datalist_id_set_data_full (GData	  **datalist,
			     GQuark         key_id,
			     gpointer       data,
			     GDestroyNotify destroy_func)
{
  g_return_if_fail (datalist != NULL);
  if (!data)
    g_return_if_fail (destroy_func == NULL);
  if (!key_id)
    {
      if (data)
	g_return_if_fail (key_id > 0);
      else
	return;
    }

  g_data_set_internal (datalist, key_id, data, destroy_func, NULL);
}

/**
 * g_dataset_id_remove_no_notify: (skip)
 * @dataset_location: (not nullable): the location identifying the dataset.
 * @key_id: the #GQuark ID identifying the data element.
 *
 * Removes an element, without calling its destroy notification
 * function.
 *
 * Returns: (nullable): the data previously stored at @key_id,
 *          or %NULL if none.
 **/
/**
 * g_dataset_remove_no_notify: (skip)
 * @l: the location identifying the dataset.
 * @k: the string identifying the data element.
 *
 * Removes an element, without calling its destroy notifier.
 **/
gpointer
g_dataset_id_remove_no_notify (gconstpointer  dataset_location,
			       GQuark         key_id)
{
  gpointer ret_data = NULL;

  g_return_val_if_fail (dataset_location != NULL, NULL);
  
  G_LOCK (g_dataset_global);
  if (key_id && g_dataset_location_ht)
    {
      GDataset *dataset;
  
      dataset = g_dataset_lookup (dataset_location);
      if (dataset)
	ret_data = g_data_set_internal (&dataset->datalist, key_id, NULL, (GDestroyNotify) 42, dataset);
    } 
  G_UNLOCK (g_dataset_global);

  return ret_data;
}

/**
 * g_datalist_id_remove_no_notify: (skip)
 * @datalist: a datalist.
 * @key_id: the #GQuark identifying a data element.
 *
 * Removes an element, without calling its destroy notification
 * function.
 *
 * Returns: (nullable): the data previously stored at @key_id,
 *          or %NULL if none.
 **/
/**
 * g_datalist_remove_no_notify: (skip)
 * @dl: a datalist.
 * @k: the string identifying the data element.
 *
 * Removes an element, without calling its destroy notifier.
 **/
gpointer
g_datalist_id_remove_no_notify (GData	**datalist,
				GQuark    key_id)
{
  gpointer ret_data = NULL;

  g_return_val_if_fail (datalist != NULL, NULL);

  if (key_id)
    ret_data = g_data_set_internal (datalist, key_id, NULL, (GDestroyNotify) 42, NULL);

  return ret_data;
}

/**
 * g_dataset_id_get_data:
 * @dataset_location: (not nullable): the location identifying the dataset.
 * @key_id: the #GQuark id to identify the data element.
 *
 * Gets the data element corresponding to a #GQuark.
 *
 * Returns: (transfer none) (nullable): the data element corresponding to
 *          the #GQuark, or %NULL if it is not found.
 **/
/**
 * g_dataset_get_data:
 * @l: the location identifying the dataset.
 * @k: the string identifying the data element.
 *
 * Gets the data element corresponding to a string.
 *
 * Returns: (transfer none) (nullable): the data element corresponding to
 *          the string, or %NULL if it is not found.
 **/
gpointer
g_dataset_id_get_data (gconstpointer  dataset_location,
		       GQuark         key_id)
{
  gpointer retval = NULL;

  g_return_val_if_fail (dataset_location != NULL, NULL);
  
  G_LOCK (g_dataset_global);
  if (key_id && g_dataset_location_ht)
    {
      GDataset *dataset;
      
      dataset = g_dataset_lookup (dataset_location);
      if (dataset)
	retval = g_datalist_id_get_data (&dataset->datalist, key_id);
    }
  G_UNLOCK (g_dataset_global);
 
  return retval;
}

/**
 * g_datalist_id_get_data:
 * @datalist: a datalist.
 * @key_id: the #GQuark identifying a data element.
 *
 * Retrieves the data element corresponding to @key_id.
 *
 * Returns: (transfer none) (nullable): the data element, or %NULL if
 *          it is not found.
 */
gpointer
g_datalist_id_get_data (GData  **datalist,
			GQuark   key_id)
{
  return g_datalist_id_dup_data (datalist, key_id, NULL, NULL);
}

/**
 * GDuplicateFunc:
 * @data: the data to duplicate
 * @user_data: (closure): user data that was specified in
 *             g_datalist_id_dup_data()
 *
 * The type of functions that are used to 'duplicate' an object.
 * What this means depends on the context, it could just be
 * incrementing the reference count, if @data is a ref-counted
 * object.
 *
 * Returns: a duplicate of data
 */

/**
 * g_datalist_id_dup_data: (skip)
 * @datalist: location of a datalist
 * @key_id: the #GQuark identifying a data element
 * @dup_func: (nullable) (scope call): function to duplicate the old value
 * @user_data: (closure): passed as user_data to @dup_func
 *
 * This is a variant of g_datalist_id_get_data() which
 * returns a 'duplicate' of the value. @dup_func defines the
 * meaning of 'duplicate' in this context, it could e.g.
 * take a reference on a ref-counted object.
 *
 * If the @key_id is not set in the datalist then @dup_func
 * will be called with a %NULL argument.
 *
 * Note that @dup_func is called while the datalist is locked, so it
 * is not allowed to read or modify the datalist.
 *
 * This function can be useful to avoid races when multiple
 * threads are using the same datalist and the same key.
 *
 * Returns: (nullable): the result of calling @dup_func on the value
 *     associated with @key_id in @datalist, or %NULL if not set.
 *     If @dup_func is %NULL, the value is returned unmodified.
 *
 * Since: 2.34
 */
gpointer
g_datalist_id_dup_data (GData          **datalist,
                        GQuark           key_id,
                        GDuplicateFunc   dup_func,
                        gpointer         user_data)
{
  gpointer val = NULL;
  gpointer retval = NULL;
  GData *d;
  GDataElt *data, *data_end;

  g_datalist_lock (datalist);

  d = G_DATALIST_GET_POINTER (datalist);
  if (d)
    {
      data = d->data;
      data_end = data + d->len;
      do
        {
          if (data->key == key_id)
            {
              val = data->data;
              break;
            }
          data++;
        }
      while (data < data_end);
    }

  if (dup_func)
    retval = dup_func (val, user_data);
  else
    retval = val;

  g_datalist_unlock (datalist);

  return retval;
}

/**
 * g_datalist_id_replace_data: (skip)
 * @datalist: location of a datalist
 * @key_id: the #GQuark identifying a data element
 * @oldval: (nullable): the old value to compare against
 * @newval: (nullable): the new value to replace it with
 * @destroy: (nullable): destroy notify for the new value
 * @old_destroy: (out) (optional): destroy notify for the existing value
 *
 * Compares the member that is associated with @key_id in
 * @datalist to @oldval, and if they are the same, replace
 * @oldval with @newval.
 *
 * This is like a typical atomic compare-and-exchange
 * operation, for a member of @datalist.
 *
 * If the previous value was replaced then ownership of the
 * old value (@oldval) is passed to the caller, including
 * the registered destroy notify for it (passed out in @old_destroy).
 * Its up to the caller to free this as he wishes, which may
 * or may not include using @old_destroy as sometimes replacement
 * should not destroy the object in the normal way.
 *
 * Returns: %TRUE if the existing value for @key_id was replaced
 *  by @newval, %FALSE otherwise.
 *
 * Since: 2.34
 */
gboolean
g_datalist_id_replace_data (GData          **datalist,
                            GQuark           key_id,
                            gpointer         oldval,
                            gpointer         newval,
                            GDestroyNotify   destroy,
                            GDestroyNotify  *old_destroy)
{
  gpointer val = NULL;
  GData *d;
  GDataElt *data, *data_end;

  g_return_val_if_fail (datalist != NULL, FALSE);
  g_return_val_if_fail (key_id != 0, FALSE);

  if (old_destroy)
    *old_destroy = NULL;

  g_datalist_lock (datalist);

  d = G_DATALIST_GET_POINTER (datalist);
  if (d)
    {
      data = d->data;
      data_end = data + d->len - 1;
      while (data <= data_end)
        {
          if (data->key == key_id)
            {
              val = data->data;
              if (val == oldval)
                {
                  if (old_destroy)
                    *old_destroy = data->destroy;
                  if (newval != NULL)
                    {
                      data->data = newval;
                      data->destroy = destroy;
                    }
                  else
                   {
                     if (data != data_end)
                       *data = *data_end;
                     d->len--;

                     /* We don't bother to shrink, but if all data are now gone
                      * we at least free the memory
                      */
                     if (d->len == 0)
                       {
                         G_DATALIST_SET_POINTER (datalist, NULL);
                         g_free (d);
                       }
                   }
                }
              break;
            }
          data++;
        }
    }

  if (val == NULL && oldval == NULL && newval != NULL)
    {
      GData *old_d;

      /* insert newval */
      old_d = d;
      if (d == NULL)
	{
          d = g_malloc (sizeof (GData));
          d->len = 0;
          d->alloc = 1;
        }
      else if (d->len == d->alloc)
        {
          d->alloc = d->alloc * 2;
          d = g_realloc (d, sizeof (GData) + (d->alloc - 1) * sizeof (GDataElt));
        }
      if (old_d != d)
        G_DATALIST_SET_POINTER (datalist, d);

      d->data[d->len].key = key_id;
      d->data[d->len].data = newval;
      d->data[d->len].destroy = destroy;
      d->len++;
    }

  g_datalist_unlock (datalist);

  return val == oldval;
}

/**
 * g_datalist_get_data:
 * @datalist: a datalist.
 * @key: the string identifying a data element.
 *
 * Gets a data element, using its string identifier. This is slower than
 * g_datalist_id_get_data() because it compares strings.
 *
 * Returns: (transfer none) (nullable): the data element, or %NULL if it
 *          is not found.
 **/
gpointer
g_datalist_get_data (GData	 **datalist,
		     const gchar *key)
{
  gpointer res = NULL;
  GData *d;
  GDataElt *data, *data_end;

  g_return_val_if_fail (datalist != NULL, NULL);

  g_datalist_lock (datalist);

  d = G_DATALIST_GET_POINTER (datalist);
  if (d)
    {
      data = d->data;
      data_end = data + d->len;
      while (data < data_end)
	{
	  if (g_strcmp0 (g_quark_to_string (data->key), key) == 0)
	    {
	      res = data->data;
	      break;
	    }
	  data++;
	}
    }

  g_datalist_unlock (datalist);

  return res;
}

/**
 * GDataForeachFunc:
 * @key_id: the #GQuark id to identifying the data element.
 * @data: the data element.
 * @user_data: (closure): user data passed to g_dataset_foreach().
 *
 * Specifies the type of function passed to g_dataset_foreach(). It is
 * called with each #GQuark id and associated data element, together
 * with the @user_data parameter supplied to g_dataset_foreach().
 **/

/**
 * g_dataset_foreach:
 * @dataset_location: (not nullable): the location identifying the dataset.
 * @func: (scope call): the function to call for each data element.
 * @user_data: (closure): user data to pass to the function.
 *
 * Calls the given function for each data element which is associated
 * with the given location. Note that this function is NOT thread-safe.
 * So unless @dataset_location can be protected from any modifications
 * during invocation of this function, it should not be called.
 *
 * @func can make changes to the dataset, but the iteration will not
 * reflect changes made during the g_dataset_foreach() call, other
 * than skipping over elements that are removed.
 **/
void
g_dataset_foreach (gconstpointer    dataset_location,
		   GDataForeachFunc func,
		   gpointer         user_data)
{
  GDataset *dataset;
  
  g_return_if_fail (dataset_location != NULL);
  g_return_if_fail (func != NULL);

  G_LOCK (g_dataset_global);
  if (g_dataset_location_ht)
    {
      dataset = g_dataset_lookup (dataset_location);
      G_UNLOCK (g_dataset_global);
      if (dataset)
	g_datalist_foreach (&dataset->datalist, func, user_data);
    }
  else
    {
      G_UNLOCK (g_dataset_global);
    }
}

/**
 * g_datalist_foreach:
 * @datalist: a datalist.
 * @func: (scope call): the function to call for each data element.
 * @user_data: (closure): user data to pass to the function.
 *
 * Calls the given function for each data element of the datalist. The
 * function is called with each data element's #GQuark id and data,
 * together with the given @user_data parameter. Note that this
 * function is NOT thread-safe. So unless @datalist can be protected
 * from any modifications during invocation of this function, it should
 * not be called.
 *
 * @func can make changes to @datalist, but the iteration will not
 * reflect changes made during the g_datalist_foreach() call, other
 * than skipping over elements that are removed.
 **/
void
g_datalist_foreach (GData	   **datalist,
		    GDataForeachFunc func,
		    gpointer         user_data)
{
  GData *d;
  int i, j, len;
  GQuark *keys;

  g_return_if_fail (datalist != NULL);
  g_return_if_fail (func != NULL);

  d = G_DATALIST_GET_POINTER (datalist);
  if (d == NULL) 
    return;

  /* We make a copy of the keys so that we can handle it changing
     in the callback */
  len = d->len;
  keys = g_new (GQuark, len);
  for (i = 0; i < len; i++)
    keys[i] = d->data[i].key;
  
  for (i = 0; i < len; i++)
    {
      /* A previous callback might have removed a later item, so always check that
	 it still exists before calling */
      d = G_DATALIST_GET_POINTER (datalist);
      
      if (d == NULL)
	break;
      for (j = 0; j < d->len; j++)
	{
	  if (d->data[j].key == keys[i]) {
	    func (d->data[i].key, d->data[i].data, user_data);
	    break;
	  }
	}
    }
  g_free (keys);
}

/**
 * g_datalist_init: (skip)
 * @datalist: a pointer to a pointer to a datalist.
 *
 * Resets the datalist to %NULL. It does not free any memory or call
 * any destroy functions.
 **/
void
g_datalist_init (GData **datalist)
{
  g_return_if_fail (datalist != NULL);

  g_atomic_pointer_set (datalist, NULL);
}

/**
 * g_datalist_set_flags:
 * @datalist: pointer to the location that holds a list
 * @flags: the flags to turn on. The values of the flags are
 *   restricted by %G_DATALIST_FLAGS_MASK (currently
 *   3; giving two possible boolean flags).
 *   A value for @flags that doesn't fit within the mask is
 *   an error.
 * 
 * Turns on flag values for a data list. This function is used
 * to keep a small number of boolean flags in an object with
 * a data list without using any additional space. It is
 * not generally useful except in circumstances where space
 * is very tight. (It is used in the base #GObject type, for
 * example.)
 *
 * Since: 2.8
 **/
void
g_datalist_set_flags (GData **datalist,
		      guint   flags)
{
  g_return_if_fail (datalist != NULL);
  g_return_if_fail ((flags & ~G_DATALIST_FLAGS_MASK) == 0);

  g_atomic_pointer_or (datalist, (gsize)flags);
}

/**
 * g_datalist_unset_flags:
 * @datalist: pointer to the location that holds a list
 * @flags: the flags to turn off. The values of the flags are
 *   restricted by %G_DATALIST_FLAGS_MASK (currently
 *   3: giving two possible boolean flags).
 *   A value for @flags that doesn't fit within the mask is
 *   an error.
 * 
 * Turns off flag values for a data list. See g_datalist_unset_flags()
 *
 * Since: 2.8
 **/
void
g_datalist_unset_flags (GData **datalist,
			guint   flags)
{
  g_return_if_fail (datalist != NULL);
  g_return_if_fail ((flags & ~G_DATALIST_FLAGS_MASK) == 0);

  g_atomic_pointer_and (datalist, ~(gsize)flags);
}

/**
 * g_datalist_get_flags:
 * @datalist: pointer to the location that holds a list
 * 
 * Gets flags values packed in together with the datalist.
 * See g_datalist_set_flags().
 * 
 * Returns: the flags of the datalist
 *
 * Since: 2.8
 **/
guint
g_datalist_get_flags (GData **datalist)
{
  g_return_val_if_fail (datalist != NULL, 0);
  
  return G_DATALIST_GET_FLAGS (datalist); /* atomic macro */
}

/* HOLDS: g_dataset_global_lock */
static void
g_data_initialize (void)
{
  g_return_if_fail (g_dataset_location_ht == NULL);

  g_dataset_location_ht = g_hash_table_new (g_direct_hash, NULL);
  g_dataset_cached = NULL;
}
