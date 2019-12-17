/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * GHook: Callback maintenance functions
 * Copyright (C) 1998 Tim Janik
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

#include "ghook.h"

#include "gtestutils.h"
#include "gslice.h"

/**
 * SECTION:hooks
 * @title: Hook Functions
 * @short_description: support for manipulating lists of hook functions
 *
 * The #GHookList, #GHook and their related functions provide support for
 * lists of hook functions. Functions can be added and removed from the lists,
 * and the list of hook functions can be invoked.
 */

/**
 * GHookList:
 * @seq_id: the next free #GHook id
 * @hook_size: the size of the #GHookList elements, in bytes
 * @is_setup: 1 if the #GHookList has been initialized
 * @hooks: the first #GHook element in the list
 * @dummy3: unused
 * @finalize_hook: the function to call to finalize a #GHook element.
 *     The default behaviour is to call the hooks @destroy function
 * @dummy: unused
 *
 * The #GHookList struct represents a list of hook functions.
 */

/**
 * GHookFinalizeFunc:
 * @hook_list: a #GHookList
 * @hook: the hook in @hook_list that gets finalized
 *
 * Defines the type of function to be called when a hook in a
 * list of hooks gets finalized.
 */

/**
 * GHookFlagMask:
 * @G_HOOK_FLAG_ACTIVE: set if the hook has not been destroyed
 * @G_HOOK_FLAG_IN_CALL: set if the hook is currently being run
 * @G_HOOK_FLAG_MASK: A mask covering all bits reserved for
 *   hook flags; see %G_HOOK_FLAG_USER_SHIFT
 *
 * Flags used internally in the #GHook implementation.
 */

/**
 * G_HOOK_FLAGS:
 * @hook: a #GHook
 *
 * Gets the flags of a hook.
 */

/**
 * G_HOOK_FLAG_USER_SHIFT:
 *
 * The position of the first bit which is not reserved for internal
 * use be the #GHook implementation, i.e.
 * `1 << G_HOOK_FLAG_USER_SHIFT` is the first
 * bit which can be used for application-defined flags.
 */

/**
 * G_HOOK:
 * @hook: a pointer
 *
 * Casts a pointer to a `GHook*`.
 */

/**
 * G_HOOK_IS_VALID:
 * @hook: a #GHook
 *
 * Returns %TRUE if the #GHook is valid, i.e. it is in a #GHookList,
 * it is active and it has not been destroyed.
 *
 * Returns: %TRUE if the #GHook is valid
 */

/**
 * G_HOOK_ACTIVE:
 * @hook: a #GHook
 *
 * Returns %TRUE if the #GHook is active, which is normally the case
 * until the #GHook is destroyed.
 *
 * Returns: %TRUE if the #GHook is active
 */

/**
 * G_HOOK_IN_CALL:
 * @hook: a #GHook
 *
 * Returns %TRUE if the #GHook function is currently executing.
 *
 * Returns: %TRUE if the #GHook function is currently executing
 */

/**
 * G_HOOK_IS_UNLINKED:
 * @hook: a #GHook
 *
 * Returns %TRUE if the #GHook is not in a #GHookList.
 *
 * Returns: %TRUE if the #GHook is not in a #GHookList
 */

/**
 * GHook:
 * @data: data which is passed to func when this hook is invoked
 * @next: pointer to the next hook in the list
 * @prev: pointer to the previous hook in the list
 * @ref_count: the reference count of this hook
 * @hook_id: the id of this hook, which is unique within its list
 * @flags: flags which are set for this hook. See #GHookFlagMask for
 *     predefined flags
 * @func: the function to call when this hook is invoked. The possible
 *     signatures for this function are #GHookFunc and #GHookCheckFunc
 * @destroy: the default @finalize_hook function of a #GHookList calls
 *     this member of the hook that is being finalized
 *
 * The #GHook struct represents a single hook function in a #GHookList.
 */

/**
 * GHookFunc:
 * @data: the data field of the #GHook is passed to the hook function here
 *
 * Defines the type of a hook function that can be invoked
 * by g_hook_list_invoke().
 */

/**
 * GHookCheckFunc:
 * @data: the data field of the #GHook is passed to the hook function here
 *
 * Defines the type of a hook function that can be invoked
 * by g_hook_list_invoke_check().
 *
 * Returns: %FALSE if the #GHook should be destroyed
 */

/* --- functions --- */
static void
default_finalize_hook (GHookList *hook_list,
		       GHook     *hook)
{
  GDestroyNotify destroy = hook->destroy;

  if (destroy)
    {
      hook->destroy = NULL;
      destroy (hook->data);
    }
}

/**
 * g_hook_list_init:
 * @hook_list: a #GHookList
 * @hook_size: the size of each element in the #GHookList,
 *     typically `sizeof (GHook)`.
 *
 * Initializes a #GHookList.
 * This must be called before the #GHookList is used.
 */
void
g_hook_list_init (GHookList *hook_list,
		  guint	     hook_size)
{
  g_return_if_fail (hook_list != NULL);
  g_return_if_fail (hook_size >= sizeof (GHook));
  
  hook_list->seq_id = 1;
  hook_list->hook_size = hook_size;
  hook_list->is_setup = TRUE;
  hook_list->hooks = NULL;
  hook_list->dummy3 = NULL;
  hook_list->finalize_hook = default_finalize_hook;
  hook_list->dummy[0] = NULL;
  hook_list->dummy[1] = NULL;
}

/**
 * g_hook_list_clear:
 * @hook_list: a #GHookList
 *
 * Removes all the #GHook elements from a #GHookList.
 */
void
g_hook_list_clear (GHookList *hook_list)
{
  g_return_if_fail (hook_list != NULL);
  
  if (hook_list->is_setup)
    {
      GHook *hook;
      
      hook_list->is_setup = FALSE;
      
      hook = hook_list->hooks;
      if (!hook)
	{
	  /* destroy hook_list->hook_memchunk */
	}
      else
	do
	  {
	    GHook *tmp;
	    
	    g_hook_ref (hook_list, hook);
	    g_hook_destroy_link (hook_list, hook);
	    tmp = hook->next;
	    g_hook_unref (hook_list, hook);
	    hook = tmp;
	  }
	while (hook);
    }
}

/**
 * g_hook_alloc:
 * @hook_list: a #GHookList
 *
 * Allocates space for a #GHook and initializes it.
 *
 * Returns: a new #GHook
 */
GHook*
g_hook_alloc (GHookList *hook_list)
{
  GHook *hook;
  
  g_return_val_if_fail (hook_list != NULL, NULL);
  g_return_val_if_fail (hook_list->is_setup, NULL);
  
  hook = g_slice_alloc0 (hook_list->hook_size);
  hook->data = NULL;
  hook->next = NULL;
  hook->prev = NULL;
  hook->flags = G_HOOK_FLAG_ACTIVE;
  hook->ref_count = 0;
  hook->hook_id = 0;
  hook->func = NULL;
  hook->destroy = NULL;
  
  return hook;
}
/**
 * g_hook_free:
 * @hook_list: a #GHookList
 * @hook: the #GHook to free
 *
 * Calls the #GHookList @finalize_hook function if it exists,
 * and frees the memory allocated for the #GHook.
 */
void
g_hook_free (GHookList *hook_list,
	     GHook     *hook)
{
  g_return_if_fail (hook_list != NULL);
  g_return_if_fail (hook_list->is_setup);
  g_return_if_fail (hook != NULL);
  g_return_if_fail (G_HOOK_IS_UNLINKED (hook));
  g_return_if_fail (!G_HOOK_IN_CALL (hook));

  if(hook_list->finalize_hook != NULL)
      hook_list->finalize_hook (hook_list, hook);
  g_slice_free1 (hook_list->hook_size, hook);
}

/**
 * g_hook_destroy_link:
 * @hook_list: a #GHookList
 * @hook: the #GHook to remove
 *
 * Removes one #GHook from a #GHookList, marking it
 * inactive and calling g_hook_unref() on it.
 */
void
g_hook_destroy_link (GHookList *hook_list,
		     GHook     *hook)
{
  g_return_if_fail (hook_list != NULL);
  g_return_if_fail (hook != NULL);

  hook->flags &= ~G_HOOK_FLAG_ACTIVE;
  if (hook->hook_id)
    {
      hook->hook_id = 0;
      g_hook_unref (hook_list, hook); /* counterpart to g_hook_insert_before */
    }
}

/**
 * g_hook_destroy:
 * @hook_list: a #GHookList
 * @hook_id: a hook ID
 *
 * Destroys a #GHook, given its ID.
 *
 * Returns: %TRUE if the #GHook was found in the #GHookList and destroyed
 */
gboolean
g_hook_destroy (GHookList   *hook_list,
		gulong	     hook_id)
{
  GHook *hook;
  
  g_return_val_if_fail (hook_list != NULL, FALSE);
  g_return_val_if_fail (hook_id > 0, FALSE);
  
  hook = g_hook_get (hook_list, hook_id);
  if (hook)
    {
      g_hook_destroy_link (hook_list, hook);
      return TRUE;
    }
  
  return FALSE;
}

/**
 * g_hook_unref:
 * @hook_list: a #GHookList
 * @hook: the #GHook to unref
 *
 * Decrements the reference count of a #GHook.
 * If the reference count falls to 0, the #GHook is removed
 * from the #GHookList and g_hook_free() is called to free it.
 */
void
g_hook_unref (GHookList *hook_list,
	      GHook	*hook)
{
  g_return_if_fail (hook_list != NULL);
  g_return_if_fail (hook != NULL);
  g_return_if_fail (hook->ref_count > 0);
  
  hook->ref_count--;
  if (!hook->ref_count)
    {
      g_return_if_fail (hook->hook_id == 0);
      g_return_if_fail (!G_HOOK_IN_CALL (hook));

      if (hook->prev)
	hook->prev->next = hook->next;
      else
	hook_list->hooks = hook->next;
      if (hook->next)
	{
	  hook->next->prev = hook->prev;
	  hook->next = NULL;
	}
      hook->prev = NULL;

      if (!hook_list->is_setup)
	{
	  hook_list->is_setup = TRUE;
	  g_hook_free (hook_list, hook);
	  hook_list->is_setup = FALSE;
      
	  if (!hook_list->hooks)
	    {
	      /* destroy hook_list->hook_memchunk */
	    }
	}
      else
	g_hook_free (hook_list, hook);
    }
}

/**
 * g_hook_ref:
 * @hook_list: a #GHookList
 * @hook: the #GHook to increment the reference count of
 *
 * Increments the reference count for a #GHook.
 *
 * Returns: the @hook that was passed in (since 2.6)
 */
GHook *
g_hook_ref (GHookList *hook_list,
	    GHook     *hook)
{
  g_return_val_if_fail (hook_list != NULL, NULL);
  g_return_val_if_fail (hook != NULL, NULL);
  g_return_val_if_fail (hook->ref_count > 0, NULL);
  
  hook->ref_count++;

  return hook;
}

/**
 * g_hook_append:
 * @hook_list: a #GHookList
 * @hook: the #GHook to add to the end of @hook_list
 *
 * Appends a #GHook onto the end of a #GHookList.
 */

/**
 * g_hook_prepend:
 * @hook_list: a #GHookList
 * @hook: the #GHook to add to the start of @hook_list
 *
 * Prepends a #GHook on the start of a #GHookList.
 */
void
g_hook_prepend (GHookList *hook_list,
		GHook	  *hook)
{
  g_return_if_fail (hook_list != NULL);
  
  g_hook_insert_before (hook_list, hook_list->hooks, hook);
}

/**
 * g_hook_insert_before:
 * @hook_list: a #GHookList
 * @sibling: (nullable): the #GHook to insert the new #GHook before
 * @hook: the #GHook to insert
 *
 * Inserts a #GHook into a #GHookList, before a given #GHook.
 */
void
g_hook_insert_before (GHookList *hook_list,
		      GHook	*sibling,
		      GHook	*hook)
{
  g_return_if_fail (hook_list != NULL);
  g_return_if_fail (hook_list->is_setup);
  g_return_if_fail (hook != NULL);
  g_return_if_fail (G_HOOK_IS_UNLINKED (hook));
  g_return_if_fail (hook->ref_count == 0);
  
  hook->hook_id = hook_list->seq_id++;
  hook->ref_count = 1; /* counterpart to g_hook_destroy_link */
  
  if (sibling)
    {
      if (sibling->prev)
	{
	  hook->prev = sibling->prev;
	  hook->prev->next = hook;
	  hook->next = sibling;
	  sibling->prev = hook;
	}
      else
	{
	  hook_list->hooks = hook;
	  hook->next = sibling;
	  sibling->prev = hook;
	}
    }
  else
    {
      if (hook_list->hooks)
	{
	  sibling = hook_list->hooks;
	  while (sibling->next)
	    sibling = sibling->next;
	  hook->prev = sibling;
	  sibling->next = hook;
	}
      else
	hook_list->hooks = hook;
    }
}

/**
 * g_hook_list_invoke:
 * @hook_list: a #GHookList
 * @may_recurse: %TRUE if functions which are already running
 *     (e.g. in another thread) can be called. If set to %FALSE,
 *     these are skipped
 *
 * Calls all of the #GHook functions in a #GHookList.
 */
void
g_hook_list_invoke (GHookList *hook_list,
		    gboolean   may_recurse)
{
  GHook *hook;
  
  g_return_if_fail (hook_list != NULL);
  g_return_if_fail (hook_list->is_setup);

  hook = g_hook_first_valid (hook_list, may_recurse);
  while (hook)
    {
      GHookFunc func;
      gboolean was_in_call;
      
      func = (GHookFunc) hook->func;
      
      was_in_call = G_HOOK_IN_CALL (hook);
      hook->flags |= G_HOOK_FLAG_IN_CALL;
      func (hook->data);
      if (!was_in_call)
	hook->flags &= ~G_HOOK_FLAG_IN_CALL;
      
      hook = g_hook_next_valid (hook_list, hook, may_recurse);
    }
}

/**
 * g_hook_list_invoke_check:
 * @hook_list: a #GHookList
 * @may_recurse: %TRUE if functions which are already running
 *     (e.g. in another thread) can be called. If set to %FALSE,
 *     these are skipped
 *
 * Calls all of the #GHook functions in a #GHookList.
 * Any function which returns %FALSE is removed from the #GHookList.
 */
void
g_hook_list_invoke_check (GHookList *hook_list,
			  gboolean   may_recurse)
{
  GHook *hook;
  
  g_return_if_fail (hook_list != NULL);
  g_return_if_fail (hook_list->is_setup);
  
  hook = g_hook_first_valid (hook_list, may_recurse);
  while (hook)
    {
      GHookCheckFunc func;
      gboolean was_in_call;
      gboolean need_destroy;
      
      func = (GHookCheckFunc) hook->func;
      
      was_in_call = G_HOOK_IN_CALL (hook);
      hook->flags |= G_HOOK_FLAG_IN_CALL;
      need_destroy = !func (hook->data);
      if (!was_in_call)
	hook->flags &= ~G_HOOK_FLAG_IN_CALL;
      if (need_destroy)
	g_hook_destroy_link (hook_list, hook);
      
      hook = g_hook_next_valid (hook_list, hook, may_recurse);
    }
}

/**
 * GHookCheckMarshaller:
 * @hook: a #GHook
 * @marshal_data: user data
 *
 * Defines the type of function used by g_hook_list_marshal_check().
 *
 * Returns: %FALSE if @hook should be destroyed
 */

/**
 * g_hook_list_marshal_check:
 * @hook_list: a #GHookList
 * @may_recurse: %TRUE if hooks which are currently running
 *     (e.g. in another thread) are considered valid. If set to %FALSE,
 *     these are skipped
 * @marshaller: the function to call for each #GHook
 * @marshal_data: data to pass to @marshaller
 *
 * Calls a function on each valid #GHook and destroys it if the
 * function returns %FALSE.
 */
void
g_hook_list_marshal_check (GHookList	       *hook_list,
			   gboolean		may_recurse,
			   GHookCheckMarshaller marshaller,
			   gpointer		data)
{
  GHook *hook;
  
  g_return_if_fail (hook_list != NULL);
  g_return_if_fail (hook_list->is_setup);
  g_return_if_fail (marshaller != NULL);
  
  hook = g_hook_first_valid (hook_list, may_recurse);
  while (hook)
    {
      gboolean was_in_call;
      gboolean need_destroy;
      
      was_in_call = G_HOOK_IN_CALL (hook);
      hook->flags |= G_HOOK_FLAG_IN_CALL;
      need_destroy = !marshaller (hook, data);
      if (!was_in_call)
	hook->flags &= ~G_HOOK_FLAG_IN_CALL;
      if (need_destroy)
	g_hook_destroy_link (hook_list, hook);
      
      hook = g_hook_next_valid (hook_list, hook, may_recurse);
    }
}

/**
 * GHookMarshaller:
 * @hook: a #GHook
 * @marshal_data: user data
 *
 * Defines the type of function used by g_hook_list_marshal().
 */

/**
 * g_hook_list_marshal:
 * @hook_list: a #GHookList
 * @may_recurse: %TRUE if hooks which are currently running
 *     (e.g. in another thread) are considered valid. If set to %FALSE,
 *     these are skipped
 * @marshaller: the function to call for each #GHook
 * @marshal_data: data to pass to @marshaller
 *
 * Calls a function on each valid #GHook.
 */
void
g_hook_list_marshal (GHookList		     *hook_list,
		     gboolean		      may_recurse,
		     GHookMarshaller	      marshaller,
		     gpointer		      data)
{
  GHook *hook;
  
  g_return_if_fail (hook_list != NULL);
  g_return_if_fail (hook_list->is_setup);
  g_return_if_fail (marshaller != NULL);
  
  hook = g_hook_first_valid (hook_list, may_recurse);
  while (hook)
    {
      gboolean was_in_call;
      
      was_in_call = G_HOOK_IN_CALL (hook);
      hook->flags |= G_HOOK_FLAG_IN_CALL;
      marshaller (hook, data);
      if (!was_in_call)
	hook->flags &= ~G_HOOK_FLAG_IN_CALL;
      
      hook = g_hook_next_valid (hook_list, hook, may_recurse);
    }
}

/**
 * g_hook_first_valid:
 * @hook_list: a #GHookList
 * @may_be_in_call: %TRUE if hooks which are currently running
 *     (e.g. in another thread) are considered valid. If set to %FALSE,
 *     these are skipped
 *
 * Returns the first #GHook in a #GHookList which has not been destroyed.
 * The reference count for the #GHook is incremented, so you must call
 * g_hook_unref() to restore it when no longer needed. (Or call
 * g_hook_next_valid() if you are stepping through the #GHookList.)
 *
 * Returns: the first valid #GHook, or %NULL if none are valid
 */
GHook*
g_hook_first_valid (GHookList *hook_list,
		    gboolean   may_be_in_call)
{
  g_return_val_if_fail (hook_list != NULL, NULL);
  
  if (hook_list->is_setup)
    {
      GHook *hook;
      
      hook = hook_list->hooks;
      if (hook)
	{
	  g_hook_ref (hook_list, hook);
	  if (G_HOOK_IS_VALID (hook) && (may_be_in_call || !G_HOOK_IN_CALL (hook)))
	    return hook;
	  else
	    return g_hook_next_valid (hook_list, hook, may_be_in_call);
	}
    }
  
  return NULL;
}

/**
 * g_hook_next_valid:
 * @hook_list: a #GHookList
 * @hook: the current #GHook
 * @may_be_in_call: %TRUE if hooks which are currently running
 *     (e.g. in another thread) are considered valid. If set to %FALSE,
 *     these are skipped
 *
 * Returns the next #GHook in a #GHookList which has not been destroyed.
 * The reference count for the #GHook is incremented, so you must call
 * g_hook_unref() to restore it when no longer needed. (Or continue to call
 * g_hook_next_valid() until %NULL is returned.)
 *
 * Returns: the next valid #GHook, or %NULL if none are valid
 */
GHook*
g_hook_next_valid (GHookList *hook_list,
		   GHook     *hook,
		   gboolean   may_be_in_call)
{
  GHook *ohook = hook;

  g_return_val_if_fail (hook_list != NULL, NULL);

  if (!hook)
    return NULL;
  
  hook = hook->next;
  while (hook)
    {
      if (G_HOOK_IS_VALID (hook) && (may_be_in_call || !G_HOOK_IN_CALL (hook)))
	{
	  g_hook_ref (hook_list, hook);
	  g_hook_unref (hook_list, ohook);
	  
	  return hook;
	}
      hook = hook->next;
    }
  g_hook_unref (hook_list, ohook);

  return NULL;
}

/**
 * g_hook_get:
 * @hook_list: a #GHookList
 * @hook_id: a hook id
 *
 * Returns the #GHook with the given id, or %NULL if it is not found.
 *
 * Returns: the #GHook with the given id, or %NULL if it is not found
 */
GHook*
g_hook_get (GHookList *hook_list,
	    gulong     hook_id)
{
  GHook *hook;
  
  g_return_val_if_fail (hook_list != NULL, NULL);
  g_return_val_if_fail (hook_id > 0, NULL);
  
  hook = hook_list->hooks;
  while (hook)
    {
      if (hook->hook_id == hook_id)
	return hook;
      hook = hook->next;
    }
  
  return NULL;
}

/**
 * GHookFindFunc:
 * @hook: a #GHook
 * @data: user data passed to g_hook_find_func()
 *
 * Defines the type of the function passed to g_hook_find().
 *
 * Returns: %TRUE if the required #GHook has been found
 */

/**
 * g_hook_find:
 * @hook_list: a #GHookList
 * @need_valids: %TRUE if #GHook elements which have been destroyed
 *     should be skipped
 * @func: the function to call for each #GHook, which should return
 *     %TRUE when the #GHook has been found
 * @data: the data to pass to @func
 *
 * Finds a #GHook in a #GHookList using the given function to
 * test for a match.
 *
 * Returns: the found #GHook or %NULL if no matching #GHook is found
 */
GHook*
g_hook_find (GHookList	  *hook_list,
	     gboolean	   need_valids,
	     GHookFindFunc func,
	     gpointer	   data)
{
  GHook *hook;
  
  g_return_val_if_fail (hook_list != NULL, NULL);
  g_return_val_if_fail (func != NULL, NULL);
  
  hook = hook_list->hooks;
  while (hook)
    {
      GHook *tmp;

      /* test only non-destroyed hooks */
      if (!hook->hook_id)
	{
	  hook = hook->next;
	  continue;
	}
      
      g_hook_ref (hook_list, hook);
      
      if (func (hook, data) && hook->hook_id && (!need_valids || G_HOOK_ACTIVE (hook)))
	{
	  g_hook_unref (hook_list, hook);
	  
	  return hook;
	}

      tmp = hook->next;
      g_hook_unref (hook_list, hook);
      hook = tmp;
    }
  
  return NULL;
}

/**
 * g_hook_find_data:
 * @hook_list: a #GHookList
 * @need_valids: %TRUE if #GHook elements which have been destroyed
 *     should be skipped
 * @data: the data to find
 *
 * Finds a #GHook in a #GHookList with the given data.
 *
 * Returns: the #GHook with the given @data or %NULL if no matching
 *     #GHook is found
 */
GHook*
g_hook_find_data (GHookList *hook_list,
		  gboolean   need_valids,
		  gpointer   data)
{
  GHook *hook;
  
  g_return_val_if_fail (hook_list != NULL, NULL);
  
  hook = hook_list->hooks;
  while (hook)
    {
      /* test only non-destroyed hooks */
      if (hook->data == data &&
	  hook->hook_id &&
	  (!need_valids || G_HOOK_ACTIVE (hook)))
	return hook;

      hook = hook->next;
    }
  
  return NULL;
}

/**
 * g_hook_find_func:
 * @hook_list: a #GHookList
 * @need_valids: %TRUE if #GHook elements which have been destroyed
 *     should be skipped
 * @func: the function to find
 *
 * Finds a #GHook in a #GHookList with the given function.
 *
 * Returns: the #GHook with the given @func or %NULL if no matching
 *     #GHook is found
 */
GHook*
g_hook_find_func (GHookList *hook_list,
		  gboolean   need_valids,
		  gpointer   func)
{
  GHook *hook;
  
  g_return_val_if_fail (hook_list != NULL, NULL);
  g_return_val_if_fail (func != NULL, NULL);
  
  hook = hook_list->hooks;
  while (hook)
    {
      /* test only non-destroyed hooks */
      if (hook->func == func &&
	  hook->hook_id &&
	  (!need_valids || G_HOOK_ACTIVE (hook)))
	return hook;

      hook = hook->next;
    }
  
  return NULL;
}

/**
 * g_hook_find_func_data:
 * @hook_list: a #GHookList
 * @need_valids: %TRUE if #GHook elements which have been destroyed
 *     should be skipped
 * @func: (not nullable): the function to find
 * @data: the data to find
 *
 * Finds a #GHook in a #GHookList with the given function and data.
 *
 * Returns: the #GHook with the given @func and @data or %NULL if
 *     no matching #GHook is found
 */
GHook*
g_hook_find_func_data (GHookList *hook_list,
		       gboolean	  need_valids,
		       gpointer	  func,
		       gpointer	  data)
{
  GHook *hook;
  
  g_return_val_if_fail (hook_list != NULL, NULL);
  g_return_val_if_fail (func != NULL, NULL);
  
  hook = hook_list->hooks;
  while (hook)
    {
      /* test only non-destroyed hooks */
      if (hook->data == data &&
	  hook->func == func &&
	  hook->hook_id &&
	  (!need_valids || G_HOOK_ACTIVE (hook)))
	return hook;

      hook = hook->next;
    }
  
  return NULL;
}

/**
 * GHookCompareFunc:
 * @new_hook: the #GHook being inserted
 * @sibling: the #GHook to compare with @new_hook
 *
 * Defines the type of function used to compare #GHook elements in
 * g_hook_insert_sorted().
 *
 * Returns: a value <= 0 if @new_hook should be before @sibling
 */

/**
 * g_hook_insert_sorted:
 * @hook_list: a #GHookList
 * @hook: the #GHook to insert
 * @func: the comparison function used to sort the #GHook elements
 *
 * Inserts a #GHook into a #GHookList, sorted by the given function.
 */
void
g_hook_insert_sorted (GHookList	      *hook_list,
		      GHook	      *hook,
		      GHookCompareFunc func)
{
  GHook *sibling;
  
  g_return_if_fail (hook_list != NULL);
  g_return_if_fail (hook_list->is_setup);
  g_return_if_fail (hook != NULL);
  g_return_if_fail (G_HOOK_IS_UNLINKED (hook));
  g_return_if_fail (hook->func != NULL);
  g_return_if_fail (func != NULL);

  /* first non-destroyed hook */
  sibling = hook_list->hooks;
  while (sibling && !sibling->hook_id)
    sibling = sibling->next;
  
  while (sibling)
    {
      GHook *tmp;
      
      g_hook_ref (hook_list, sibling);
      if (func (hook, sibling) <= 0 && sibling->hook_id)
	{
	  g_hook_unref (hook_list, sibling);
	  break;
	}

      /* next non-destroyed hook */
      tmp = sibling->next;
      while (tmp && !tmp->hook_id)
	tmp = tmp->next;

      g_hook_unref (hook_list, sibling);
      sibling = tmp;
   
 }
  
  g_hook_insert_before (hook_list, sibling, hook);
}

/**
 * g_hook_compare_ids:
 * @new_hook: a #GHook
 * @sibling: a #GHook to compare with @new_hook
 *
 * Compares the ids of two #GHook elements, returning a negative value
 * if the second id is greater than the first.
 *
 * Returns: a value <= 0 if the id of @sibling is >= the id of @new_hook
 */
gint
g_hook_compare_ids (GHook *new_hook,
		    GHook *sibling)
{
  if (new_hook->hook_id < sibling->hook_id)
    return -1;
  else if (new_hook->hook_id > sibling->hook_id)
    return 1;
  
  return 0;
}
