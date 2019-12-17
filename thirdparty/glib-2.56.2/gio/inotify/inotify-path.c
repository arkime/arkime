/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 2; tab-width: 8 -*- */

/* inotify-path.c - GVFS Monitor based on inotify.

   Copyright (C) 2006 John McCutchan
   Copyright (C) 2009 Codethink Limited

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
                 Ryan Lortie <desrt@desrt.ca>
*/

#include "config.h"

/* Don't put conflicting kernel types in the global namespace: */
#define __KERNEL_STRICT_NAMES

#include <sys/inotify.h>
#include <string.h>
#include <glib.h>
#include "inotify-kernel.h"
#include "inotify-path.h"
#include "inotify-missing.h"

#define IP_INOTIFY_DIR_MASK (IN_MODIFY|IN_ATTRIB|IN_MOVED_FROM|IN_MOVED_TO|IN_DELETE|IN_CREATE|IN_DELETE_SELF|IN_UNMOUNT|IN_MOVE_SELF|IN_CLOSE_WRITE)

#define IP_INOTIFY_FILE_MASK (IN_MODIFY|IN_ATTRIB|IN_CLOSE_WRITE)

/* Older libcs don't have this */
#ifndef IN_ONLYDIR
#define IN_ONLYDIR 0  
#endif

typedef struct ip_watched_file_s {
  gchar *filename;
  gchar *path;
  gint32 wd;

  GList *subs;
} ip_watched_file_t;

typedef struct ip_watched_dir_s {
  char *path;
  /* TODO: We need to maintain a tree of watched directories
   * so that we can deliver move/delete events to sub folders.
   * Or the application could do it...
   */
  struct ip_watched_dir_s* parent;
  GList*	 children;

  /* basename -> ip_watched_file_t
   * Maps basename to a ip_watched_file_t if the file is currently
   * being directly watched for changes (ie: 'hardlinks' mode).
   */
  GHashTable *files_hash;

  /* Inotify state */
  gint32 wd;
  
  /* List of inotify subscriptions */
  GList *subs;
} ip_watched_dir_t;

static gboolean     ip_debug_enabled = FALSE;
#define IP_W if (ip_debug_enabled) g_warning

/* path -> ip_watched_dir */
static GHashTable * path_dir_hash = NULL;
/* inotify_sub * -> ip_watched_dir *
 *
 * Each subscription is attached to a watched directory or it is on
 * the missing list
 */
static GHashTable * sub_dir_hash = NULL;
/* This hash holds GLists of ip_watched_dir_t *'s
 * We need to hold a list because symbolic links can share
 * the same wd
 */
static GHashTable * wd_dir_hash = NULL;
/* This hash holds GLists of ip_watched_file_t *'s
 * We need to hold a list because links can share
 * the same wd
 */
static GHashTable * wd_file_hash = NULL;

static ip_watched_dir_t *ip_watched_dir_new  (const char       *path,
					      int               wd);
static void              ip_watched_dir_free (ip_watched_dir_t *dir);
static gboolean          ip_event_callback   (ik_event_t       *event);


static gboolean (*event_callback)(ik_event_t *event, inotify_sub *sub, gboolean file_event);

gboolean
_ip_startup (gboolean (*cb)(ik_event_t *event, inotify_sub *sub, gboolean file_event))
{
  static gboolean initialized = FALSE;
  static gboolean result = FALSE;
  
  if (initialized == TRUE)
    return result;

  event_callback = cb;
  result = _ik_startup (ip_event_callback);

  if (!result)
    return FALSE;

  path_dir_hash = g_hash_table_new (g_str_hash, g_str_equal);
  sub_dir_hash = g_hash_table_new (g_direct_hash, g_direct_equal);
  wd_dir_hash = g_hash_table_new (g_direct_hash, g_direct_equal);
  wd_file_hash = g_hash_table_new (g_direct_hash, g_direct_equal);
  
  initialized = TRUE;
  return TRUE;
}

static void
ip_map_path_dir (const char       *path, 
                 ip_watched_dir_t *dir)
{
  g_assert (path && dir);
  g_hash_table_insert (path_dir_hash, dir->path, dir);
}

static void
ip_map_sub_dir (inotify_sub      *sub, 
                ip_watched_dir_t *dir)
{
  /* Associate subscription and directory */
  g_assert (dir && sub);
  g_hash_table_insert (sub_dir_hash, sub, dir);
  dir->subs = g_list_prepend (dir->subs, sub);
}

static void
ip_map_wd_dir (gint32            wd, 
               ip_watched_dir_t *dir)
{
  GList *dir_list;
  
  g_assert (wd >= 0 && dir);
  dir_list = g_hash_table_lookup (wd_dir_hash, GINT_TO_POINTER (wd));
  dir_list = g_list_prepend (dir_list, dir);
  g_hash_table_replace (wd_dir_hash, GINT_TO_POINTER (dir->wd), dir_list);
}

static void
ip_map_wd_file (gint32             wd,
                ip_watched_file_t *file)
{
  GList *file_list;

  g_assert (wd >= 0 && file);
  file_list = g_hash_table_lookup (wd_file_hash, GINT_TO_POINTER (wd));
  file_list = g_list_prepend (file_list, file);
  g_hash_table_replace (wd_file_hash, GINT_TO_POINTER (wd), file_list);
}

static void
ip_unmap_wd_file (gint32             wd,
                  ip_watched_file_t *file)
{
  GList *file_list = g_hash_table_lookup (wd_file_hash, GINT_TO_POINTER (wd));

  if (!file_list)
    return;

  g_assert (wd >= 0 && file);
  file_list = g_list_remove (file_list, file);
  if (file_list == NULL)
    g_hash_table_remove (wd_file_hash, GINT_TO_POINTER (wd));
  else
    g_hash_table_replace (wd_file_hash, GINT_TO_POINTER (wd), file_list);
}


static ip_watched_file_t *
ip_watched_file_new (const gchar *dirname,
                     const gchar *filename)
{
  ip_watched_file_t *file;

  file = g_new0 (ip_watched_file_t, 1);
  file->path = g_strjoin ("/", dirname, filename, NULL);
  file->filename = g_strdup (filename);
  file->wd = -1;

  return file;
}

static void
ip_watched_file_free (ip_watched_file_t *file)
{
  g_assert (file->subs == NULL);
  g_free (file->filename);
  g_free (file->path);
}

static void
ip_watched_file_add_sub (ip_watched_file_t *file,
                         inotify_sub       *sub)
{
  file->subs = g_list_prepend (file->subs, sub);
}

static void
ip_watched_file_start (ip_watched_file_t *file)
{
  if (file->wd < 0)
    {
      gint err;

      file->wd = _ik_watch (file->path,
                            IP_INOTIFY_FILE_MASK,
                            &err);

      if (file->wd >= 0)
        ip_map_wd_file (file->wd, file);
    }
}

static void
ip_watched_file_stop (ip_watched_file_t *file)
{
  if (file->wd >= 0)
    {
      _ik_ignore (file->path, file->wd);
      ip_unmap_wd_file (file->wd, file);
      file->wd = -1;
    }
}

gboolean
_ip_start_watching (inotify_sub *sub)
{
  gint32 wd;
  int err;
  ip_watched_dir_t *dir;
  
  g_assert (sub);
  g_assert (!sub->cancelled);
  g_assert (sub->dirname);
  
  IP_W ("Starting to watch %s\n", sub->dirname);
  dir = g_hash_table_lookup (path_dir_hash, sub->dirname);

  if (dir == NULL)
    {
      IP_W ("Trying to add inotify watch ");
      wd = _ik_watch (sub->dirname, IP_INOTIFY_DIR_MASK|IN_ONLYDIR, &err);
      if (wd < 0)
        {
          IP_W ("Failed\n");
          return FALSE;
        }
      else
        {
          /* Create new watched directory and associate it with the
           * wd hash and path hash
           */
          IP_W ("Success\n");
          dir = ip_watched_dir_new (sub->dirname, wd);
          ip_map_wd_dir (wd, dir);
          ip_map_path_dir (sub->dirname, dir);
        }
    }
  else
    IP_W ("Already watching\n");

  if (sub->hardlinks)
    {
      ip_watched_file_t *file;

      file = g_hash_table_lookup (dir->files_hash, sub->filename);

      if (file == NULL)
        {
          file = ip_watched_file_new (sub->dirname, sub->filename);
          g_hash_table_insert (dir->files_hash, file->filename, file);
        }

      ip_watched_file_add_sub (file, sub);
      ip_watched_file_start (file);
    }

  ip_map_sub_dir (sub, dir);
  
  return TRUE;
}

static void
ip_unmap_path_dir (const char       *path, 
                   ip_watched_dir_t *dir)
{
  g_assert (path && dir);
  g_hash_table_remove (path_dir_hash, dir->path);
}

static void
ip_unmap_wd_dir (gint32            wd, 
                 ip_watched_dir_t *dir)
{
  GList *dir_list = g_hash_table_lookup (wd_dir_hash, GINT_TO_POINTER (wd));
  
  if (!dir_list)
    return;
  
  g_assert (wd >= 0 && dir);
  dir_list = g_list_remove (dir_list, dir);
  if (dir_list == NULL) 
    g_hash_table_remove (wd_dir_hash, GINT_TO_POINTER (dir->wd));
  else
    g_hash_table_replace (wd_dir_hash, GINT_TO_POINTER (dir->wd), dir_list);
}

static void
ip_unmap_wd (gint32 wd)
{
  GList *dir_list = g_hash_table_lookup (wd_dir_hash, GINT_TO_POINTER (wd));
  if (!dir_list)
    return;
  g_assert (wd >= 0);
  g_hash_table_remove (wd_dir_hash, GINT_TO_POINTER (wd));
  g_list_free (dir_list);
}

static void
ip_unmap_sub_dir (inotify_sub      *sub,
                  ip_watched_dir_t *dir)
{
  g_assert (sub && dir);
  g_hash_table_remove (sub_dir_hash, sub);
  dir->subs = g_list_remove (dir->subs, sub);

  if (sub->hardlinks)
    {
      ip_watched_file_t *file;

      file = g_hash_table_lookup (dir->files_hash, sub->filename);
      file->subs = g_list_remove (file->subs, sub);

      if (file->subs == NULL)
        {
          g_hash_table_remove (dir->files_hash, sub->filename);
          ip_watched_file_stop (file);
          ip_watched_file_free (file);
        }
    }
 }

static void
ip_unmap_all_subs (ip_watched_dir_t *dir)
{
  while (dir->subs != NULL)
    ip_unmap_sub_dir (dir->subs->data, dir);
}

gboolean
_ip_stop_watching (inotify_sub *sub)
{
  ip_watched_dir_t *dir = NULL;
  
  dir = g_hash_table_lookup (sub_dir_hash, sub);
  if (!dir) 
    return TRUE;
  
  ip_unmap_sub_dir (sub, dir);
  
  /* No one is subscribing to this directory any more */
  if (dir->subs == NULL)
    {
      _ik_ignore (dir->path, dir->wd);
      ip_unmap_wd_dir (dir->wd, dir);
      ip_unmap_path_dir (dir->path, dir);
      ip_watched_dir_free (dir);
    }
  
  return TRUE;
}


static ip_watched_dir_t *
ip_watched_dir_new (const char *path, 
                    gint32      wd)
{
  ip_watched_dir_t *dir = g_new0 (ip_watched_dir_t, 1);
  
  dir->path = g_strdup (path);
  dir->files_hash = g_hash_table_new (g_str_hash, g_str_equal);
  dir->wd = wd;
  
  return dir;
}

static void
ip_watched_dir_free (ip_watched_dir_t *dir)
{
  g_assert_cmpint (g_hash_table_size (dir->files_hash), ==, 0);
  g_assert (dir->subs == NULL);
  g_free (dir->path);
  g_hash_table_unref (dir->files_hash);
  g_free (dir);
}

static void
ip_wd_delete (gpointer data, 
              gpointer user_data)
{
  ip_watched_dir_t *dir = data;
  GList *l = NULL;
  
  for (l = dir->subs; l; l = l->next)
    {
      inotify_sub *sub = l->data;
      /* Add subscription to missing list */
      _im_add (sub);
    }
  ip_unmap_all_subs (dir);
  /* Unassociate the path and the directory */
  ip_unmap_path_dir (dir->path, dir);
  ip_watched_dir_free (dir);
}

static gboolean
ip_event_dispatch (GList      *dir_list, 
                   GList      *file_list,
                   ik_event_t *event)
{
  gboolean interesting = FALSE;

  GList *l;
  
  if (!event)
    return FALSE;

  for (l = dir_list; l; l = l->next)
    {
      GList *subl;
      ip_watched_dir_t *dir = l->data;
      
      for (subl = dir->subs; subl; subl = subl->next)
	{
	  inotify_sub *sub = subl->data;
	  
	  /* If the subscription and the event
	   * contain a filename and they don't
	   * match, we don't deliver this event.
	   */
	  if (sub->filename &&
	      event->name &&
	      strcmp (sub->filename, event->name) &&
              (!event->pair || !event->pair->name || strcmp (sub->filename, event->pair->name)))
	    continue;
	  
	  /* If the subscription has a filename
	   * but this event doesn't, we don't
	   * deliver this event.
	   */
	  if (sub->filename && !event->name)
	    continue;
	  
	  /* If we're also watching the file directly
	   * don't report events that will also be
	   * reported on the file itself.
	   */
	  if (sub->hardlinks)
	    {
	      event->mask &= ~IP_INOTIFY_FILE_MASK;
	      if (!event->mask)
		continue;
	    }
	  
	  /* FIXME: We might need to synthesize
	   * DELETE/UNMOUNT events when
	   * the filename doesn't match
	   */
	  
	  interesting |= event_callback (event, sub, FALSE);

          if (sub->hardlinks)
            {
              ip_watched_file_t *file;

              file = g_hash_table_lookup (dir->files_hash, sub->filename);

              if (file != NULL)
                {
                  if (event->mask & (IN_MOVED_FROM | IN_DELETE))
                    ip_watched_file_stop (file);

                  if (event->mask & (IN_MOVED_TO | IN_CREATE))
                    ip_watched_file_start (file);
                }
            }
        }
    }

  for (l = file_list; l; l = l->next)
    {
      ip_watched_file_t *file = l->data;
      GList *subl;

      for (subl = file->subs; subl; subl = subl->next)
        {
	  inotify_sub *sub = subl->data;

	  interesting |= event_callback (event, sub, TRUE);
        }
    }

  return interesting;
}

static gboolean
ip_event_callback (ik_event_t *event)
{
  gboolean interesting = FALSE;
  GList* dir_list = NULL;
  GList *file_list = NULL;

  /* We can ignore the IGNORED events. Likewise, if the event queue overflowed,
   * there is not much we can do to recover. */
  if (event->mask & (IN_IGNORED | IN_Q_OVERFLOW))
    {
      _ik_event_free (event);
      return TRUE;
    }

  dir_list = g_hash_table_lookup (wd_dir_hash, GINT_TO_POINTER (event->wd));
  file_list = g_hash_table_lookup (wd_file_hash, GINT_TO_POINTER (event->wd));

  if (event->mask & IP_INOTIFY_DIR_MASK)
    interesting |= ip_event_dispatch (dir_list, file_list, event);

  /* Only deliver paired events if the wds are separate */
  if (event->pair && event->pair->wd != event->wd)
    {
      dir_list = g_hash_table_lookup (wd_dir_hash, GINT_TO_POINTER (event->pair->wd));
      file_list = g_hash_table_lookup (wd_file_hash, GINT_TO_POINTER (event->pair->wd));

      if (event->pair->mask & IP_INOTIFY_DIR_MASK)
        interesting |= ip_event_dispatch (dir_list, file_list, event->pair);
    }

  /* We have to manage the missing list
   * when we get an event that means the
   * file has been deleted/moved/unmounted.
   */
  if (event->mask & IN_DELETE_SELF ||
      event->mask & IN_MOVE_SELF ||
      event->mask & IN_UNMOUNT)
    {
      /* Add all subscriptions to missing list */
      g_list_foreach (dir_list, ip_wd_delete, NULL);
      /* Unmap all directories attached to this wd */
      ip_unmap_wd (event->wd);
    }
  
  _ik_event_free (event);

  return interesting;
}

const char *
_ip_get_path_for_wd (gint32 wd)
{
  GList *dir_list;
  ip_watched_dir_t *dir;

  g_assert (wd >= 0);
  dir_list = g_hash_table_lookup (wd_dir_hash, GINT_TO_POINTER (wd));
  if (dir_list)
    {
      dir = dir_list->data;
      if (dir)
	return dir->path;
    }

  return NULL;
}
