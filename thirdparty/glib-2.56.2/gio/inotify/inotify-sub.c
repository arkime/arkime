/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 2; tab-width: 8 -*- */

/* inotify-sub.c - GVFS Monitor based on inotify.

   Copyright (C) 2006 John McCutchan

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
#include <string.h>
#include <glib.h>

#include "inotify-sub.h"

static gboolean is_debug_enabled = FALSE;
#define IS_W if (is_debug_enabled) g_warning

static gchar*
dup_dirname (const gchar *dirname)
{
  gchar *d_dirname = g_strdup (dirname);
  size_t len = strlen (d_dirname);
  
  if (d_dirname[len - 1] == '/')
    d_dirname[len - 1] = '\0';
  
  return d_dirname;
}

inotify_sub*
_ih_sub_new (const gchar *dirname, 
             const gchar *basename,
             const gchar *filename,
             gpointer     user_data)
{
  inotify_sub *sub = NULL;
  
  sub = g_new0 (inotify_sub, 1);

  if (filename)
    {
      sub->dirname = g_path_get_dirname (filename);
      sub->filename = g_path_get_basename (filename);
      sub->hardlinks = TRUE;
    }
  else
    {
      sub->dirname = dup_dirname (dirname);
      sub->filename = g_strdup (basename);
      sub->hardlinks = FALSE;
    }

  sub->user_data = user_data;

  IS_W ("new subscription for %s being setup\n", sub->dirname);
  
  return sub;
}

void
_ih_sub_free (inotify_sub *sub)
{
  g_free (sub->dirname);
  g_free (sub->filename);
  g_free (sub);
}
