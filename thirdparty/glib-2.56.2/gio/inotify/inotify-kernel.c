/*
   Copyright (C) 2005 John McCutchan
   Copyright © 2015 Canonical Limited

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
     Ryan Lortie <desrt@desrt.ca>
     John McCutchan <john@johnmccutchan.com>
*/

#include "config.h"

#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <glib.h>
#include "inotify-kernel.h"
#include <sys/inotify.h>
#include <glib/glib-unix.h>

#include "glib-private.h"

/* From inotify(7) */
#define MAX_EVENT_SIZE       (sizeof(struct inotify_event) + NAME_MAX + 1)

/* Amount of time to sleep on receipt of uninteresting events */
#define BOREDOM_SLEEP_TIME   (100 * G_TIME_SPAN_MILLISECOND)

/* Define limits on the maximum amount of time and maximum amount of
 * interceding events between FROM/TO that can be merged.
 */
#define MOVE_PAIR_DELAY      (10 * G_TIME_SPAN_MILLISECOND)
#define MOVE_PAIR_DISTANCE   (100)

/* We use the lock from inotify-helper.c
 *
 * We only have to take it on our read callback.
 *
 * The rest of locking is taken care of in inotify-helper.c
 */
G_LOCK_EXTERN (inotify_lock);

static ik_event_t *
ik_event_new (struct inotify_event *kevent,
              gint64                now)
{
  ik_event_t *event = g_new0 (ik_event_t, 1);

  event->wd = kevent->wd;
  event->mask = kevent->mask;
  event->cookie = kevent->cookie;
  event->len = kevent->len;
  event->timestamp = now;
  if (event->len)
    event->name = g_strdup (kevent->name);
  else
    event->name = NULL;

  return event;
}

void
_ik_event_free (ik_event_t *event)
{
  if (event->pair)
    {
      event->pair->pair = NULL;
      _ik_event_free (event->pair);
    }

  g_free (event->name);
  g_free (event);
}

typedef struct
{
  GSource     source;

  GQueue      queue;
  gpointer    fd_tag;
  gint        fd;

  GHashTable *unmatched_moves;
  gboolean    is_bored;
} InotifyKernelSource;

static InotifyKernelSource *inotify_source;

static gint64
ik_source_get_dispatch_time (InotifyKernelSource *iks)
{
  ik_event_t *head;

  head = g_queue_peek_head (&iks->queue);

  /* nothing in the queue: not ready */
  if (!head)
    return -1;

  /* if it's not an unpaired move, it is ready now */
  if (~head->mask & IN_MOVED_FROM || head->pair)
    return 0;

  /* if the queue is too long then it's ready now */
  if (iks->queue.length > MOVE_PAIR_DISTANCE)
    return 0;

  /* otherwise, it's ready after the delay */
  return head->timestamp + MOVE_PAIR_DELAY;
}

static gboolean
ik_source_can_dispatch_now (InotifyKernelSource *iks,
                            gint64               now)
{
  gint64 dispatch_time;

  dispatch_time = ik_source_get_dispatch_time (iks);

  return 0 <= dispatch_time && dispatch_time <= now;
}

static gsize
ik_source_read_some_events (InotifyKernelSource *iks,
                            gchar               *buffer,
                            gsize                buffer_len)
{
  gssize result;
  int errsv;

again:
  result = read (iks->fd, buffer, buffer_len);
  errsv = errno;

  if (result < 0)
    {
      if (errsv == EINTR)
        goto again;

      if (errsv == EAGAIN)
        return 0;

      g_error ("inotify read(): %s", g_strerror (errsv));
    }
  else if (result == 0)
    g_error ("inotify unexpectedly hit eof");

  return result;
}

static gchar *
ik_source_read_all_the_events (InotifyKernelSource *iks,
                               gchar               *buffer,
                               gsize                buffer_len,
                               gsize               *length_out)
{
  gsize n_read;

  n_read = ik_source_read_some_events (iks, buffer, buffer_len);

  /* Check if we might have gotten another event if we had passed in a
   * bigger buffer...
   */
  if (n_read + MAX_EVENT_SIZE > buffer_len)
    {
      gchar *new_buffer;
      guint n_readable;
      gint result;
      int errsv;

      /* figure out how many more bytes there are to read */
      result = ioctl (iks->fd, FIONREAD, &n_readable);
      errsv = errno;
      if (result != 0)
        g_error ("inotify ioctl(FIONREAD): %s", g_strerror (errsv));

      if (n_readable != 0)
        {
          /* there is in fact more data.  allocate a new buffer, copy
           * the existing data, and then append the remaining.
           */
          new_buffer = g_malloc (n_read + n_readable);
          memcpy (new_buffer, buffer, n_read);
          n_read += ik_source_read_some_events (iks, new_buffer + n_read, n_readable);

          buffer = new_buffer;

          /* There may be new events in the buffer that were added after
           * the FIONREAD was performed, but we can't risk getting into
           * a loop.  We'll get them next time.
           */
        }
    }

  *length_out = n_read;

  return buffer;
}

static gboolean
ik_source_dispatch (GSource     *source,
                    GSourceFunc  func,
                    gpointer     user_data)
{
  InotifyKernelSource *iks = (InotifyKernelSource *) source;
  gboolean (*user_callback) (ik_event_t *event) = (void *) func;
  gboolean interesting = FALSE;
  gint64 now;

  now = g_source_get_time (source);

  if (iks->is_bored || g_source_query_unix_fd (source, iks->fd_tag))
    {
      gchar stack_buffer[4096];
      gsize buffer_len;
      gchar *buffer;
      gsize offset;

      /* We want to read all of the available events.
       *
       * We need to do it in a finite number of steps so that we don't
       * get caught in a loop of read() with another process
       * continuously adding events each time we drain them.
       *
       * In the normal case we will have only a few events in the queue,
       * so start out by reading into a small stack-allocated buffer.
       * Even though we're on a fresh stack frame, there is no need to
       * pointlessly blow up with the size of the worker thread stack
       * with a huge buffer here.
       *
       * If the result is large enough to cause us to suspect that
       * another event may be pending then we allocate a buffer on the
       * heap that can hold all of the events and read (once!) into that
       * buffer.
       */
      buffer = ik_source_read_all_the_events (iks, stack_buffer, sizeof stack_buffer, &buffer_len);

      offset = 0;

      while (offset < buffer_len)
        {
          struct inotify_event *kevent = (struct inotify_event *) (buffer + offset);
          ik_event_t *event;

          event = ik_event_new (kevent, now);

          offset += sizeof (struct inotify_event) + event->len;

          if (event->mask & IN_MOVED_TO)
            {
              ik_event_t *pair;

              pair = g_hash_table_lookup (iks->unmatched_moves, GUINT_TO_POINTER (event->cookie));
              if (pair != NULL)
                {
                  g_assert (!pair->pair);

                  g_hash_table_remove (iks->unmatched_moves, GUINT_TO_POINTER (event->cookie));
                  event->is_second_in_pair = TRUE;
                  event->pair = pair;
                  pair->pair = event;
                  continue;
                }

              interesting = TRUE;
            }

          else if (event->mask & IN_MOVED_FROM)
            {
              gboolean new;

              new = g_hash_table_insert (iks->unmatched_moves, GUINT_TO_POINTER (event->cookie), event);
              if G_UNLIKELY (!new)
                g_warning ("inotify: got IN_MOVED_FROM event with already-pending cookie %#x", event->cookie);

              interesting = TRUE;
            }

          g_queue_push_tail (&iks->queue, event);
        }

      if (buffer_len == 0)
        {
          /* We can end up reading nothing if we arrived here due to a
           * boredom timer but the stream of events stopped meanwhile.
           *
           * In that case, we need to switch back to polling the file
           * descriptor in the usual way.
           */
          g_assert (iks->is_bored);
          interesting = TRUE;
        }

      if (buffer != stack_buffer)
        g_free (buffer);
    }

  while (ik_source_can_dispatch_now (iks, now))
    {
      ik_event_t *event;

      /* callback will free the event */
      event = g_queue_pop_head (&iks->queue);

      if (event->mask & IN_MOVED_FROM && !event->pair)
        g_hash_table_remove (iks->unmatched_moves, GUINT_TO_POINTER (event->cookie));

      G_LOCK (inotify_lock);

      interesting |= (* user_callback) (event);

      G_UNLOCK (inotify_lock);
    }

  /* The queue gets blocked iff we have unmatched moves */
  g_assert ((iks->queue.length > 0) == (g_hash_table_size (iks->unmatched_moves) > 0));

  /* Here's where we decide what will wake us up next.
   *
   * If the last event was interesting then we will wake up on the fd or
   * when the timeout is reached on an unpaired move (if any).
   *
   * If the last event was uninteresting then we will wake up after the
   * shorter of the boredom sleep or any timeout for a unpaired move.
   */
  if (interesting)
    {
      if (iks->is_bored)
        {
          g_source_modify_unix_fd (source, iks->fd_tag, G_IO_IN);
          iks->is_bored = FALSE;
        }

      g_source_set_ready_time (source, ik_source_get_dispatch_time (iks));
    }
  else
    {
      guint64 dispatch_time = ik_source_get_dispatch_time (iks);
      guint64 boredom_time = now + BOREDOM_SLEEP_TIME;

      if (!iks->is_bored)
        {
          g_source_modify_unix_fd (source, iks->fd_tag, 0);
          iks->is_bored = TRUE;
        }

      g_source_set_ready_time (source, MIN (dispatch_time, boredom_time));
    }

  return TRUE;
}

static InotifyKernelSource *
ik_source_new (gboolean (* callback) (ik_event_t *event))
{
  static GSourceFuncs source_funcs = {
    NULL, NULL,
    ik_source_dispatch
    /* should have a finalize, but it will never happen */
  };
  InotifyKernelSource *iks;
  GSource *source;

  source = g_source_new (&source_funcs, sizeof (InotifyKernelSource));
  iks = (InotifyKernelSource *) source;

  g_source_set_name (source, "inotify kernel source");

  iks->unmatched_moves = g_hash_table_new (NULL, NULL);
  iks->fd = inotify_init1 (IN_CLOEXEC);

  if (iks->fd < 0)
    iks->fd = inotify_init ();

  if (iks->fd >= 0)
    {
      GError *error = NULL;

      g_unix_set_fd_nonblocking (iks->fd, TRUE, &error);
      g_assert_no_error (error);

      iks->fd_tag = g_source_add_unix_fd (source, iks->fd, G_IO_IN);
    }

  g_source_set_callback (source, (GSourceFunc) callback, NULL, NULL);

  g_source_attach (source, GLIB_PRIVATE_CALL (g_get_worker_context) ());

  return iks;
}

gboolean
_ik_startup (gboolean (*cb)(ik_event_t *event))
{
  if (g_once_init_enter (&inotify_source))
    g_once_init_leave (&inotify_source, ik_source_new (cb));

  return inotify_source->fd >= 0;
}

gint32
_ik_watch (const char *path,
           guint32     mask,
           int        *err)
{
  gint32 wd = -1;

  g_assert (path != NULL);
  g_assert (inotify_source && inotify_source->fd >= 0);

  wd = inotify_add_watch (inotify_source->fd, path, mask);

  if (wd < 0)
    {
      int e = errno;
      /* FIXME: debug msg failed to add watch */
      if (err)
        *err = e;
      return wd;
    }

  g_assert (wd >= 0);
  return wd;
}

int
_ik_ignore (const char *path,
            gint32      wd)
{
  g_assert (wd >= 0);
  g_assert (inotify_source && inotify_source->fd >= 0);

  if (inotify_rm_watch (inotify_source->fd, wd) < 0)
    {
      /* int e = errno; */
      /* failed to rm watch */
      return -1;
    }

  return 0;
}
