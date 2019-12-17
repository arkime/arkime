/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
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

/* we know we are deprecated here, no need for warnings */
#define GLIB_DISABLE_DEPRECATION_WARNINGS

#include "gcompletion.h"

#include <glib/gstrfuncs.h>
#include <glib/gmessages.h>
#include <glib/gunicode.h>

#include <string.h>

/**
 * SECTION:completion
 * @title: Automatic String Completion
 * @short_description: support for automatic completion using a group
 *                     of target strings
 *
 * #GCompletion provides support for automatic completion of a string
 * using any group of target strings. It is typically used for file
 * name completion as is common in many UNIX shells.
 *
 * A #GCompletion is created using g_completion_new(). Target items are
 * added and removed with g_completion_add_items(),
 * g_completion_remove_items() and g_completion_clear_items(). A
 * completion attempt is requested with g_completion_complete() or
 * g_completion_complete_utf8(). When no longer needed, the
 * #GCompletion is freed with g_completion_free().
 *
 * Items in the completion can be simple strings (e.g. filenames), or
 * pointers to arbitrary data structures. If data structures are used
 * you must provide a #GCompletionFunc in g_completion_new(), which
 * retrieves the item's string from the data structure. You can change
 * the way in which strings are compared by setting a different
 * #GCompletionStrncmpFunc in g_completion_set_compare().
 *
 * GCompletion has been marked as deprecated, since this API is rarely
 * used and not very actively maintained.
 **/

/**
 * GCompletion:
 * @items: list of target items (strings or data structures).
 * @func: function which is called to get the string associated with a
 *        target item. It is %NULL if the target items are strings.
 * @prefix: the last prefix passed to g_completion_complete() or
 *          g_completion_complete_utf8().
 * @cache: the list of items which begin with @prefix.
 * @strncmp_func: The function to use when comparing strings.  Use
 *                g_completion_set_compare() to modify this function.
 *
 * The data structure used for automatic completion.
 **/

/**
 * GCompletionFunc:
 * @Param1: the completion item.
 *
 * Specifies the type of the function passed to g_completion_new(). It
 * should return the string corresponding to the given target item.
 * This is used when you use data structures as #GCompletion items.
 *
 * Returns: the string corresponding to the item.
 **/

/**
 * GCompletionStrncmpFunc:
 * @s1: string to compare with @s2.
 * @s2: string to compare with @s1.
 * @n: maximal number of bytes to compare.
 *
 * Specifies the type of the function passed to
 * g_completion_set_compare(). This is used when you use strings as
 * #GCompletion items.
 *
 * Returns: an integer less than, equal to, or greater than zero if
 *          the first @n bytes of @s1 is found, respectively, to be
 *          less than, to match, or to be greater than the first @n
 *          bytes of @s2.
 **/

static void completion_check_cache (GCompletion* cmp,
				    gchar**	 new_prefix);

/**
 * g_completion_new:
 * @func: the function to be called to return the string representing
 *        an item in the #GCompletion, or %NULL if strings are going to
 *        be used as the #GCompletion items.
 *
 * Creates a new #GCompletion.
 *
 * Returns: the new #GCompletion.
 **/
GCompletion* 
g_completion_new (GCompletionFunc func)
{
  GCompletion* gcomp;
  
  gcomp = g_new (GCompletion, 1);
  gcomp->items = NULL;
  gcomp->cache = NULL;
  gcomp->prefix = NULL;
  gcomp->func = func;
  gcomp->strncmp_func = strncmp;

  return gcomp;
}

/**
 * g_completion_add_items:
 * @cmp: the #GCompletion.
 * @items: (transfer none): the list of items to add.
 *
 * Adds items to the #GCompletion.
 *
 * Deprecated: 2.26: Rarely used API
 **/
void 
g_completion_add_items (GCompletion* cmp,
			GList*	     items)
{
  GList* it;
  
  g_return_if_fail (cmp != NULL);
  
  /* optimize adding to cache? */
  if (cmp->cache)
    {
      g_list_free (cmp->cache);
      cmp->cache = NULL;
    }

  if (cmp->prefix)
    {
      g_free (cmp->prefix);
      cmp->prefix = NULL;
    }
  
  it = items;
  while (it)
    {
      cmp->items = g_list_prepend (cmp->items, it->data);
      it = it->next;
    }
}

/**
 * g_completion_remove_items:
 * @cmp: the #GCompletion.
 * @items: (transfer none): the items to remove.
 *
 * Removes items from a #GCompletion. The items are not freed, so if the memory
 * was dynamically allocated, free @items with g_list_free_full() after calling
 * this function.
 *
 * Deprecated: 2.26: Rarely used API
 **/
void 
g_completion_remove_items (GCompletion* cmp,
			   GList*	items)
{
  GList* it;
  
  g_return_if_fail (cmp != NULL);
  
  it = items;
  while (cmp->items && it)
    {
      cmp->items = g_list_remove (cmp->items, it->data);
      it = it->next;
    }

  it = items;
  while (cmp->cache && it)
    {
      cmp->cache = g_list_remove(cmp->cache, it->data);
      it = it->next;
    }
}

/**
 * g_completion_clear_items:
 * @cmp: the #GCompletion.
 *
 * Removes all items from the #GCompletion. The items are not freed, so if the
 * memory was dynamically allocated, it should be freed after calling this
 * function.
 *
 * Deprecated: 2.26: Rarely used API
 **/
void 
g_completion_clear_items (GCompletion* cmp)
{
  g_return_if_fail (cmp != NULL);
  
  g_list_free (cmp->items);
  cmp->items = NULL;
  g_list_free (cmp->cache);
  cmp->cache = NULL;
  g_free (cmp->prefix);
  cmp->prefix = NULL;
}

static void   
completion_check_cache (GCompletion* cmp,
			gchar**	     new_prefix)
{
  GList* list;
  gsize len;
  gsize i;
  gsize plen;
  gchar* postfix;
  gchar* s;
  
  if (!new_prefix)
    return;
  if (!cmp->cache)
    {
      *new_prefix = NULL;
      return;
    }
  
  len = strlen(cmp->prefix);
  list = cmp->cache;
  s = cmp->func ? cmp->func (list->data) : (gchar*) list->data;
  postfix = s + len;
  plen = strlen (postfix);
  list = list->next;
  
  while (list && plen)
    {
      s = cmp->func ? cmp->func (list->data) : (gchar*) list->data;
      s += len;
      for (i = 0; i < plen; ++i)
	{
	  if (postfix[i] != s[i])
	    break;
	}
      plen = i;
      list = list->next;
    }
  
  *new_prefix = g_new0 (gchar, len + plen + 1);
  strncpy (*new_prefix, cmp->prefix, len);
  strncpy (*new_prefix + len, postfix, plen);
}

/**
 * g_completion_complete_utf8:
 * @cmp: the #GCompletion
 * @prefix: the prefix string, typically used by the user, which is compared
 *    with each of the items
 * @new_prefix: if non-%NULL, returns the longest prefix which is common to all
 *    items that matched @prefix, or %NULL if no items matched @prefix.
 *    This string should be freed when no longer needed.
 *
 * Attempts to complete the string @prefix using the #GCompletion target items.
 * In contrast to g_completion_complete(), this function returns the largest common
 * prefix that is a valid UTF-8 string, omitting a possible common partial 
 * character.
 *
 * You should use this function instead of g_completion_complete() if your 
 * items are UTF-8 strings.
 *
 * Returns: (element-type utf8) (transfer none): the list of items whose strings begin with @prefix. This should
 * not be changed.
 *
 * Since: 2.4
 *
 * Deprecated: 2.26: Rarely used API
 **/
GList*
g_completion_complete_utf8 (GCompletion  *cmp,
			    const gchar  *prefix,
			    gchar       **new_prefix)
{
  GList *list;
  gchar *p, *q;

  list = g_completion_complete (cmp, prefix, new_prefix);

  if (new_prefix && *new_prefix)
    {
      p = *new_prefix + strlen (*new_prefix);
      q = g_utf8_find_prev_char (*new_prefix, p);
      
      switch (g_utf8_get_char_validated (q, p - q))
	{
	case (gunichar)-2:
	case (gunichar)-1:
	  *q = 0;
	  break;
	default: ;
	}

    }

  return list;
}

/**
 * g_completion_complete:
 * @cmp: the #GCompletion.
 * @prefix: the prefix string, typically typed by the user, which is
 *          compared with each of the items.
 * @new_prefix: if non-%NULL, returns the longest prefix which is
 *              common to all items that matched @prefix, or %NULL if
 *              no items matched @prefix.  This string should be freed
 *              when no longer needed.
 *
 * Attempts to complete the string @prefix using the #GCompletion
 * target items.
 *
 * Returns: (transfer none): the list of items whose strings begin with
 *          @prefix. This should not be changed.
 *
 * Deprecated: 2.26: Rarely used API
 **/
GList* 
g_completion_complete (GCompletion* cmp,
		       const gchar* prefix,
		       gchar**	    new_prefix)
{
  gsize plen, len;
  gboolean done = FALSE;
  GList* list;
  
  g_return_val_if_fail (cmp != NULL, NULL);
  g_return_val_if_fail (prefix != NULL, NULL);
  
  len = strlen (prefix);
  if (cmp->prefix && cmp->cache)
    {
      plen = strlen (cmp->prefix);
      if (plen <= len && ! cmp->strncmp_func (prefix, cmp->prefix, plen))
	{ 
	  /* use the cache */
	  list = cmp->cache;
	  while (list)
	    {
	      GList *next = list->next;
	      
	      if (cmp->strncmp_func (prefix,
				     cmp->func ? cmp->func (list->data) : (gchar*) list->data,
				     len))
		cmp->cache = g_list_delete_link (cmp->cache, list);

	      list = next;
	    }
	  done = TRUE;
	}
    }
  
  if (!done)
    {
      /* normal code */
      g_list_free (cmp->cache);
      cmp->cache = NULL;
      list = cmp->items;
      while (*prefix && list)
	{
	  if (!cmp->strncmp_func (prefix,
			cmp->func ? cmp->func (list->data) : (gchar*) list->data,
			len))
	    cmp->cache = g_list_prepend (cmp->cache, list->data);
	  list = list->next;
	}
    }
  if (cmp->prefix)
    {
      g_free (cmp->prefix);
      cmp->prefix = NULL;
    }
  if (cmp->cache)
    cmp->prefix = g_strdup (prefix);
  completion_check_cache (cmp, new_prefix);
  
  return *prefix ? cmp->cache : cmp->items;
}

/**
 * g_completion_free:
 * @cmp: the #GCompletion.
 *
 * Frees all memory used by the #GCompletion. The items are not freed, so if
 * the memory was dynamically allocated, it should be freed after calling this
 * function.
 *
 * Deprecated: 2.26: Rarely used API
 **/
void 
g_completion_free (GCompletion* cmp)
{
  g_return_if_fail (cmp != NULL);
  
  g_completion_clear_items (cmp);
  g_free (cmp);
}

/**
 * g_completion_set_compare:
 * @cmp: a #GCompletion.
 * @strncmp_func: the string comparison function.
 *
 * Sets the function to use for string comparisons. The default string
 * comparison function is strncmp().
 *
 * Deprecated: 2.26: Rarely used API
 **/
void
g_completion_set_compare(GCompletion *cmp,
			 GCompletionStrncmpFunc strncmp_func)
{
  cmp->strncmp_func = strncmp_func;
}

#ifdef TEST_COMPLETION
#include <stdio.h>
int
main (int   argc,
      char* argv[])
{
  FILE *file;
  gchar buf[1024];
  GList *list;
  GList *result;
  GList *tmp;
  GCompletion *cmp;
  gint i;
  gchar *longp = NULL;
  
  if (argc < 3)
    {
      g_warning ("Usage: %s filename prefix1 [prefix2 ...]\n", argv[0]);
      return 1;
    }
  
  file = fopen (argv[1], "r");
  if (!file)
    {
      g_warning ("Cannot open %s\n", argv[1]);
      return 1;
    }
  
  cmp = g_completion_new (NULL);
  list = g_list_alloc ();
  while (fgets (buf, 1024, file))
    {
      list->data = g_strdup (buf);
      g_completion_add_items (cmp, list);
    }
  fclose (file);
  
  for (i = 2; i < argc; ++i)
    {
      printf ("COMPLETING: %s\n", argv[i]);
      result = g_completion_complete (cmp, argv[i], &longp);
      g_list_foreach (result, (GFunc) printf, NULL);
      printf ("LONG MATCH: %s\n", longp);
      g_free (longp);
      longp = NULL;
    }
  
  g_list_foreach (cmp->items, (GFunc) g_free, NULL);
  g_completion_free (cmp);
  g_list_free (list);
  
  return 0;
}
#endif
