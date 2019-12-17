/*******************************************************************************
  Copyright (c) 2011, 2012 Dmitry Matveev <me@dmitrymatveev.co.uk>

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
*******************************************************************************/

#include <glib.h>

#include "kqueue-helper.h"
#include "kqueue-sub.h"
#include "kqueue-missing.h"


#define SCAN_MISSING_TIME 4 /* 1/4 Hz */

static gboolean km_scan_missing (gpointer user_data);

static gboolean km_debug_enabled = FALSE;
#define KM_W if (km_debug_enabled) g_warning

static GSList *missing_subs_list = NULL;
G_LOCK_DEFINE_STATIC (missing_lock);

static volatile gboolean scan_missing_running = FALSE;
static on_create_cb file_appeared_callback;


/**
 * _km_init:
 * @cb: a callback function. It will be called when a watched file
 *     will appear.
 *
 * Initialize the kqueue-missing module (optional).
 **/
void
_km_init (on_create_cb cb)
{
  file_appeared_callback = cb;
}


/**
 * _km_add_missing:
 * @sub: a #kqueue_sub
 *
 * Adds a subscription to the missing files list.
 **/
void
_km_add_missing (kqueue_sub *sub)
{
  G_LOCK (missing_lock);
  if (g_slist_find (missing_subs_list, sub))
    {
      KM_W ("asked to add %s to missing list but it's already on the list!\n", sub->filename);
      G_UNLOCK (missing_lock);
      return;
    }

  KM_W ("adding %s to missing list\n", sub->filename);
  missing_subs_list = g_slist_prepend (missing_subs_list, sub);
  G_UNLOCK (missing_lock);

  if (!scan_missing_running)
    {
      scan_missing_running = TRUE;
      g_timeout_add_seconds (SCAN_MISSING_TIME, km_scan_missing, NULL);
    }
}


/**
 * km_scan_missing:
 * @user_data: unused
 *
 * The core missing files watching routine.
 *
 * Traverses through a list of missing files, tries to start watching each with
 * kqueue, removes the appropriate entry and invokes a user callback if the file
 * has appeared.
 *
 * Returns: %FALSE if no missing files left, %TRUE otherwise.
 **/
static gboolean
km_scan_missing (gpointer user_data)
{
  GSList *head;
  GSList *not_missing = NULL;
  gboolean retval = FALSE;
  
  G_LOCK (missing_lock);

  if (missing_subs_list)
    KM_W ("we have a job");

  for (head = missing_subs_list; head; head = head->next)
    {
      kqueue_sub *sub = (kqueue_sub *) head->data;
      g_assert (sub != NULL);
      g_assert (sub->filename != NULL);

      if (_kh_start_watching (sub))
        {
          KM_W ("file %s now exists, starting watching", sub->filename);
          if (file_appeared_callback)
            file_appeared_callback (sub);
          not_missing = g_slist_prepend (not_missing, head);
        }
    }

  for (head = not_missing; head; head = head->next)
    {
      GSList *link = (GSList *) head->data;
      missing_subs_list = g_slist_remove_link (missing_subs_list, link);
    }
  g_slist_free (not_missing);

  if (missing_subs_list == NULL)
    {
      scan_missing_running = FALSE;
      retval = FALSE;
    }
  else
    retval = TRUE;

  G_UNLOCK (missing_lock);
  return retval;
}


/**
 * _km_remove:
 * @sub: a #kqueue_sub
 *
 * Removes a subscription from a list of missing files.
 **/
void
_km_remove (kqueue_sub *sub)
{
  G_LOCK (missing_lock);
  missing_subs_list = g_slist_remove (missing_subs_list, sub);
  G_UNLOCK (missing_lock);
}
