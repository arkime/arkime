/*
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

   Authors:.
		John McCutchan <john@johnmccutchan.com>
*/

#ifndef __INOTIFY_KERNEL_H
#define __INOTIFY_KERNEL_H

typedef struct ik_event_s {
  gint32 wd;
  guint32 mask;
  guint32 original_mask;
  guint32 cookie;
  guint32 len;
  char *  name;
  /* TRUE if this event is the last element of a pair
   * (e.g., MOVE_TO in a pair of MOVE_FROM, MOVE_TO events) */
  gboolean is_second_in_pair;
  /* if event1 and event2 are two paired events
   * (e.g., MOVE_FROM and MOVE_TO events related to the same file move),
   * then event1->pair == event2 and event2->pair == NULL.
   * It will result also in event1->pair->is_second_in_pair == TRUE */
  struct ik_event_s *pair;
  gint64 timestamp; /* monotonic time that this was created */
} ik_event_t;

gboolean _ik_startup (gboolean (*cb) (ik_event_t *event));

ik_event_t *_ik_event_new_dummy (const char *name,
				 gint32      wd,
				 guint32     mask);
void        _ik_event_free      (ik_event_t *event);

gint32      _ik_watch           (const char *path,
				 guint32     mask,
				 int        *err);
int         _ik_ignore          (const char *path,
				 gint32      wd);


/* The miss count will probably be enflated */
void        _ik_move_stats     (guint32 *matches,
				guint32 *misses);
const char *_ik_mask_to_string (guint32  mask);


#endif
