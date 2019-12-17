/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 2; tab-width: 8 -*- */

/* inotify-missing.c - GVFS Monitor based on inotify.

   Copyright (C) 2005 John McCutchan

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this library; if not, see <http://www.gnu.org/licenses/>.

   Authors: 
		 John McCutchan <john@johnmccutchan.com>
*/

#include "config.h"
#include <glib.h>
#include "inotify-missing.h"
#include "inotify-path.h"
#include "glib-private.h"

#define SCAN_MISSING_TIME 4 /* 1/4 Hz */

static gboolean im_debug_enabled = FALSE;
#define IM_W if (im_debug_enabled) g_warning

/* We put inotify_sub's that are missing on this list */
static GList *missing_sub_list = NULL;
static gboolean im_scan_missing (gpointer user_data);
static gboolean scan_missing_running = FALSE;
static void (*missing_cb)(inotify_sub *sub) = NULL;

G_LOCK_EXTERN (inotify_lock);

/* inotify_lock must be held before calling */
void
_im_startup (void (*callback)(inotify_sub *sub))
{
  static gboolean initialized = FALSE;
  
  if (!initialized)
    {
      missing_cb = callback;
      initialized = TRUE;
    }
}

/* inotify_lock must be held before calling */
void
_im_add (inotify_sub *sub)
{
  if (g_list_find (missing_sub_list, sub))
    {
      IM_W ("asked to add %s to missing list but it's already on the list!\n", sub->dirname);
      return;
    }

  IM_W ("adding %s to missing list\n", sub->dirname);
  missing_sub_list = g_list_prepend (missing_sub_list, sub);

  /* If the timeout is turned off, we turn it back on */
  if (!scan_missing_running)
    {
      GSource *source;

      scan_missing_running = TRUE;
      source = g_timeout_source_new_seconds (SCAN_MISSING_TIME);
      g_source_set_callback (source, im_scan_missing, NULL, NULL);
      g_source_attach (source, GLIB_PRIVATE_CALL (g_get_worker_context) ());
      g_source_unref (source);
    }
}

/* inotify_lock must be held before calling */
void
_im_rm (inotify_sub *sub)
{
  GList *link;
  
  link = g_list_find (missing_sub_list, sub);

  if (!link)
    {
      IM_W ("asked to remove %s from missing list but it isn't on the list!\n", sub->dirname);
      return;
    }

  IM_W ("removing %s from missing list\n", sub->dirname);

  missing_sub_list = g_list_remove_link (missing_sub_list, link);
  g_list_free_1 (link);
}

/* Scans the list of missing subscriptions checking if they
 * are available yet.
 */
static gboolean
im_scan_missing (gpointer user_data)
{
  GList *nolonger_missing = NULL;
  GList *l;
  
  G_LOCK (inotify_lock);
  
  IM_W ("scanning missing list with %d items\n", g_list_length (missing_sub_list));
  for (l = missing_sub_list; l; l = l->next)
    {
      inotify_sub *sub = l->data;
      gboolean not_m = FALSE;
      
      IM_W ("checking %p\n", sub);
      g_assert (sub);
      g_assert (sub->dirname);
      not_m = _ip_start_watching (sub);

      if (not_m)
	{
	  missing_cb (sub);
	  IM_W ("removed %s from missing list\n", sub->dirname);
	  /* We have to build a list of list nodes to remove from the
	   * missing_sub_list. We do the removal outside of this loop.
	   */
	  nolonger_missing = g_list_prepend (nolonger_missing, l);
	} 
    }

  for (l = nolonger_missing; l ; l = l->next)
    {
      GList *llink = l->data;
      missing_sub_list = g_list_remove_link (missing_sub_list, llink);
      g_list_free_1 (llink);
    }

  g_list_free (nolonger_missing);
  
  /* If the missing list is now empty, we disable the timeout */
  if (missing_sub_list == NULL)
    {
      scan_missing_running = FALSE;
      G_UNLOCK (inotify_lock);
      return FALSE;
    }
  else
    {
      G_UNLOCK (inotify_lock);
      return TRUE;
    }
}
