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

#include "config.h"
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <unistd.h>
#include <errno.h>
#include <glib.h>

#include "kqueue-thread.h"
#include "kqueue-sub.h"
#include "kqueue-utils.h"

static gboolean kt_debug_enabled = FALSE;
#define KT_W if (kt_debug_enabled) g_warning

static GQueue pick_up_fds_queue = G_QUEUE_INIT;
G_LOCK_DEFINE_STATIC (pick_up_lock);

static GSList *remove_fds_list = NULL;
G_LOCK_DEFINE_STATIC (remove_lock);

/* GIO does not have analogues for NOTE_LINK and(?) NOTE_REVOKE, so
 * we do not ask kqueue() to watch for these events for now. */
const uint32_t KQUEUE_VNODE_FLAGS =
  NOTE_DELETE | NOTE_WRITE | NOTE_EXTEND | NOTE_ATTRIB | NOTE_RENAME;

extern int get_kqueue_descriptor(void);

/**
 * _kqueue_thread_collect_fds:
 * @events: a #kevents - the list of events to monitor. Will be extended
 *     with new items.
 *
 * Picks up new file descriptors for monitoring from a global queue.
 *
 * To add new items to the list, use _kqueue_thread_push_fd().
 */
static void
_kqueue_thread_collect_fds (kevents *events)
{
  g_assert (events != NULL);
  gint length = 0;

  G_LOCK (pick_up_lock);
  if ((length = g_queue_get_length (&pick_up_fds_queue)) != 0)
    {
      gpointer fdp = NULL;
      kevents_extend_sz (events, length);

      while ((fdp = g_queue_pop_head (&pick_up_fds_queue)) != NULL)
        {
          struct kevent *pevent = &events->memory[events->kq_size++];
          EV_SET (pevent,
                  GPOINTER_TO_INT (fdp),
                  EVFILT_VNODE,
                  EV_ADD | EV_ENABLE | EV_ONESHOT,
                  KQUEUE_VNODE_FLAGS,
                  0,
                  0);
        }
    }
  G_UNLOCK (pick_up_lock);
}


/**
 * _kqueue_thread_cleanup_fds:
 * @events: a #kevents -- list of events to monitor. Cancelled
 *     subscriptions will be removed from it, and its size
 *     probably will be reduced.
 *
 * Removes file descriptors from monitoring.
 *
 * This function will pick up file descriptors from a global list
 * to cancel monitoring on them. The list will be freed then.
 *
 * To add new items to the list, use _kqueue_thread_remove_fd().
 */
static void
_kqueue_thread_cleanup_fds (kevents *events)
{
  g_assert (events != NULL);

  G_LOCK (remove_lock);
  if (remove_fds_list)
    {
      size_t oldsize = events->kq_size;
      int i, j;

      for (i = 1, j = 1; i < oldsize; i++)
        {
          int fd = events->memory[i].ident;
          GSList *elem = g_slist_find (remove_fds_list, GINT_TO_POINTER (fd));
          if (elem == NULL)
            {
              if (i != j)
                events->memory[j] = events->memory[i];
              ++j;
            }
          else if (close (fd) == -1)
            KT_W ("Failed to close fd %d, error %d", fd, errno);
        }

      KT_W ("FD Clean up complete, kq_size now %d\n", j);
      events->kq_size = j;
      kevents_reduce (events);
      g_slist_free (remove_fds_list);
      remove_fds_list = NULL;
    }
  G_UNLOCK (remove_lock);
}


/**
 * _kqueue_thread_drop_fd:
 * @events: a #kevents -- list of events to monitor. Cancelled
 *     subscriptions will be removed from it, and its size
 *     probably will be reduced.
 *
 * Removes a concrete file descriptor from monitoring.
 */
static void
_kqueue_thread_drop_fd (kevents *events, int fd)
{
  g_assert (events != NULL);

  int i;
  for (i = 1; i < events->kq_size; i++)
    {
      if (events->memory[i].ident == fd)
        {
          if (close (fd) == -1)
            KT_W ("Failed to close fd %d, error %d", fd, errno);

          events->memory[i] = events->memory[--events->kq_size];
          return;
        }
    } 
}

/**
 * _kqueue_thread_func:
 * @arg: a pointer to int -- control file descriptor.
 *
 * The thread communicates with the outside world through a so-called
 * command file descriptor. The thread reads control commands from it
 * and writes the notifications into it.
 *
 * Control commands are single-byte characters:
 * - 'A' - pick up new file descriptors to monitor
 * - 'R' - remove some descriptors from monitoring.
 *
 * For details, see _kqueue_thread_collect_fds() and
 * _kqueue_thread_cleanup_fds().
 *
 * Notifications, that thread writes into the command file descriptor,
 * are represented with #kqueue_notification objects.
 *
 * Returns: %NULL
 */
void*
_kqueue_thread_func (void *arg)
{
  int fd, kqueue_descriptor;
  kevents waiting;

  g_assert (arg != NULL);
  kevents_init_sz (&waiting, 1);

  fd = *(int *) arg;

  kqueue_descriptor = get_kqueue_descriptor();
  if (kqueue_descriptor == -1)
    {
      KT_W ("fatal: kqueue is not initialized!\n");
      return NULL;
    }

  EV_SET (&waiting.memory[0],
          fd,
          EVFILT_READ,
          EV_ADD | EV_ENABLE | EV_ONESHOT,
          NOTE_LOWAT,
          1,
          0);
  waiting.kq_size = 1;

  for (;;)
    {
      /* TODO: Provide more items in the 'eventlist' to kqueue(2).
       * Currently the backend takes notifications from the kernel one
       * by one, i.e. there will be a lot of system calls and context
       * switches when the application will monitor a lot of files with
       * high filesystem activity on each. */
     
      struct kevent received;
      KT_W ("Watching for %zi items", waiting.kq_size);
      int ret = kevent (kqueue_descriptor, waiting.memory, waiting.kq_size, &received, 1, NULL);
      int kevent_errno = errno;
      KT_W ("Awoken.");

      if (ret == -1)
        {
          KT_W ("kevent failed: %d", kevent_errno);
          if (kevent_errno == EINTR)
            continue;
          else
            return NULL;
        }

      if (received.ident == fd)
        {
          char c;
            if (!_ku_read (fd, &c, 1))
              {
                KT_W ("Failed to read command, error %d", errno);
                continue;
              }
          if (c == 'A')
            _kqueue_thread_collect_fds (&waiting);
          else if (c == 'R')
            _kqueue_thread_cleanup_fds (&waiting);
        }
      else 
        {
          struct kqueue_notification kn;
          kn.fd = received.ident;

          if (received.flags & EV_ERROR)
            {
              kn.flags = NOTE_REVOKE;
              _kqueue_thread_drop_fd (&waiting, received.ident);
            }
          else
            kn.flags = (received.fflags & ~NOTE_REVOKE);

          if (!_ku_write (fd, &kn, sizeof (struct kqueue_notification)))
            KT_W ("Failed to write a kqueue notification, error %d", errno);
        }
    }
  kevents_free (&waiting);
  return NULL;
}


/**
 * _kqueue_thread_push_fd:
 * @fd: a file descriptor
 *
 * Puts a new file descriptor into the pick up list for monitroing.
 *
 * The kqueue thread will not start monitoring on it immediately, it
 * should be bumped via its command file descriptor manually.
 * See kqueue_thread() and _kqueue_thread_collect_fds() for details.
 */
void
_kqueue_thread_push_fd (int fd)
{
  G_LOCK (pick_up_lock);
  g_queue_push_tail (&pick_up_fds_queue, GINT_TO_POINTER (fd));
  G_UNLOCK (pick_up_lock);
}


/**
 * _kqueue_thread_remove_fd:
 * @fd: a file descriptor
 *
 * Puts a new file descriptor into the remove list to cancel monitoring
 * on it.
 *
 * The kqueue thread will not stop monitoring on it immediately, it
 * should be bumped via its command file descriptor manually.
 * See kqueue_thread() and _kqueue_thread_collect_fds() for details.
 */
void
_kqueue_thread_remove_fd (int fd)
{
  G_LOCK (remove_lock);
  remove_fds_list = g_slist_prepend (remove_fds_list, GINT_TO_POINTER (fd));
  G_UNLOCK (remove_lock);
}
