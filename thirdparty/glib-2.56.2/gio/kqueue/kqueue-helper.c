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
#include <sys/socket.h>
#include <gio/glocalfile.h>
#include <gio/glocalfilemonitor.h>
#include <gio/gfile.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include "kqueue-helper.h"
#include "kqueue-utils.h"
#include "kqueue-thread.h"
#include "kqueue-missing.h"
#include "kqueue-exclusions.h"

static gboolean kh_debug_enabled = FALSE;
#define KH_W if (kh_debug_enabled) g_warning

static GHashTable *subs_hash_table = NULL;
G_LOCK_DEFINE_STATIC (hash_lock);

static int kqueue_descriptor = -1;
static int kqueue_socket_pair[] = {-1, -1};
static pthread_t kqueue_thread;


void _kh_file_appeared_cb (kqueue_sub *sub);

/**
 * accessor function for kqueue_descriptor
 **/
int
get_kqueue_descriptor()
{
  return kqueue_descriptor;
}

/**
 * convert_kqueue_events_to_gio:
 * @flags: a set of kqueue filter flags
 * @done: a pointer to #gboolean indicating that the
 *      conversion has been done (out)
 *
 * Translates kqueue filter flags into GIO event flags.
 *
 * Returns: a #GFileMonitorEvent
 **/
static GFileMonitorEvent
convert_kqueue_events_to_gio (uint32_t flags, gboolean *done)
{
  g_assert (done != NULL);
  *done = FALSE;

  /* TODO: The following notifications should be emulated, if possible:
   * - G_FILE_MONITOR_EVENT_PRE_UNMOUNT
   */
  if (flags & NOTE_DELETE)
    {    
      *done = TRUE;
      return G_FILE_MONITOR_EVENT_DELETED;
    }
  if (flags & NOTE_ATTRIB)
    {
      *done = TRUE;
      return G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED;
    }
  if (flags & (NOTE_WRITE | NOTE_EXTEND))
    {
      *done = TRUE;
      return G_FILE_MONITOR_EVENT_CHANGED;
    }
  if (flags & NOTE_RENAME)
    {
      /* Since there’s apparently no way to get the new name of the file out of
       * kqueue(), all we can do is say that this one has been deleted. */
      *done = TRUE;
      return G_FILE_MONITOR_EVENT_DELETED;
    }
  if (flags & NOTE_REVOKE)
    {
      *done = TRUE;
      return G_FILE_MONITOR_EVENT_UNMOUNTED;
    }

  /* done is FALSE */
  return 0;
}

typedef struct {
  kqueue_sub *sub;
  GFileMonitorSource *source;
} handle_ctx;

/**
 * handle_created: 
 * @udata: a pointer to user data (#handle_context).
 * @path: file name of a new file.
 * @inode: inode number of a new file.
 *
 * A callback function for the directory diff calculation routine,
 * produces G_FILE_MONITOR_EVENT_CREATED event for a created file.
 **/
static void
handle_created (void *udata, const char *path, ino_t inode)
{
  handle_ctx *ctx = NULL;

  (void) inode;
  ctx = (handle_ctx *) udata;
  g_assert (udata != NULL);
  g_assert (ctx->sub != NULL);
  g_assert (ctx->source != NULL);

  g_file_monitor_source_handle_event (ctx->source, G_FILE_MONITOR_EVENT_CREATED, path,
                                      NULL, NULL, g_get_monotonic_time ());
}

/**
 * handle_deleted:
 * @udata: a pointer to user data (#handle_context).
 * @path: file name of the removed file.
 * @inode: inode number of the removed file.
 *
 * A callback function for the directory diff calculation routine,
 * produces G_FILE_MONITOR_EVENT_DELETED event for a deleted file.
 **/
static void
handle_deleted (void *udata, const char *path, ino_t inode)
{
  handle_ctx *ctx = NULL;

  (void) inode;
  ctx = (handle_ctx *) udata;
  g_assert (udata != NULL);
  g_assert (ctx->sub != NULL);
  g_assert (ctx->source != NULL);

  g_file_monitor_source_handle_event (ctx->source, G_FILE_MONITOR_EVENT_DELETED, path,
                                      NULL, NULL, g_get_monotonic_time ());
}

/**
 * handle_moved:
 * @udata: a pointer to user data (#handle_context).
 * @from_path: file name of the source file.
 * @from_inode: inode number of the source file.
 * @to_path: file name of the replaced file.
 * @to_inode: inode number of the replaced file.
 *
 * A callback function for the directory diff calculation routine,
 * produces G_FILE_MONITOR_EVENT_RENAMED event on a move.
 **/
static void
handle_moved (void       *udata,
              const char *from_path,
              ino_t       from_inode,
              const char *to_path,
              ino_t       to_inode)
{
  handle_ctx *ctx = NULL;

  (void) from_inode;
  (void) to_inode;

  ctx = (handle_ctx *) udata;
  g_assert (udata != NULL);
  g_assert (ctx->sub != NULL);
  g_assert (ctx->source != NULL);

  g_file_monitor_source_handle_event (ctx->source, G_FILE_MONITOR_EVENT_RENAMED,
                                      from_path, to_path, NULL, g_get_monotonic_time ());
}

/**
 * handle_overwritten:
 * @data: a pointer to user data (#handle_context).
 * @path: file name of the overwritten file.
 * @node: inode number of the overwritten file.
 *
 * A callback function for the directory diff calculation routine,
 * produces G_FILE_MONITOR_EVENT_DELETED/CREATED event pair when
 * an overwrite occurs in the directory (see dep-list for details).
 **/
static void
handle_overwritten (void *udata, const char *path, ino_t inode)
{
  handle_ctx *ctx = NULL;

  (void) inode;
  ctx = (handle_ctx *) udata;
  g_assert (udata != NULL);
  g_assert (ctx->sub != NULL);
  g_assert (ctx->source != NULL);

  g_file_monitor_source_handle_event (ctx->source, G_FILE_MONITOR_EVENT_DELETED,
                                      path, NULL, NULL, g_get_monotonic_time ());

  g_file_monitor_source_handle_event (ctx->source, G_FILE_MONITOR_EVENT_CREATED,
                                      path, NULL, NULL, g_get_monotonic_time ());
}

static const traverse_cbs cbs = {
  handle_created,
  handle_deleted,
  handle_moved,
  handle_overwritten,
  handle_moved,
  NULL, /* many added */
  NULL, /* many removed */
  NULL, /* names updated */
};


void
_kh_dir_diff (kqueue_sub *sub, GFileMonitorSource *source)
{
  dep_list *was;
  handle_ctx ctx;

  g_assert (sub != NULL);
  g_assert (source != NULL);

  memset (&ctx, 0, sizeof (handle_ctx));
  ctx.sub = sub;
  ctx.source = source;

  was = sub->deps;
  sub->deps = dl_listing (sub->filename);
 
  dl_calculate (was, sub->deps, &cbs, &ctx);

  dl_free (was);
}


/**
 * process_kqueue_notifications:
 * @gioc: unused.
 * @cond: unused.
 * @data: unused.
 *
 * Processes notifications, coming from the kqueue thread.
 *
 * Reads notifications from the command file descriptor, emits the
 * "changed" event on the appropriate monitor.
 *
 * A typical GIO Channel callback function.
 *
 * Returns: %TRUE
 **/
static gboolean
process_kqueue_notifications (GIOChannel   *gioc,
                              GIOCondition  cond,
                              gpointer      data)
{
  struct kqueue_notification n;
  kqueue_sub *sub = NULL;
  GFileMonitorSource *source = NULL;
  GFileMonitorEvent mask = 0;
  
  g_assert (kqueue_socket_pair[0] != -1);
  if (!_ku_read (kqueue_socket_pair[0], &n, sizeof (struct kqueue_notification)))
    {
      KH_W ("Failed to read a kqueue notification, error %d", errno);
      return TRUE;
    }

  G_LOCK (hash_lock);
  sub = (kqueue_sub *) g_hash_table_lookup (subs_hash_table, GINT_TO_POINTER (n.fd));
  G_UNLOCK (hash_lock);

  if (sub == NULL)
    {
      KH_W ("Got a notification for a deleted or non-existing subscription %d",
             n.fd);
      return TRUE;
    }

  source = sub->user_data;
  g_assert (source != NULL);

  if (n.flags & (NOTE_DELETE | NOTE_REVOKE))
    {
      if (sub->deps)
        {
          dl_free (sub->deps);
          sub->deps = NULL;  
        }  
      _km_add_missing (sub);

      if (!(n.flags & NOTE_REVOKE))
        {
          /* Note that NOTE_REVOKE is issued by the kqueue thread
           * on EV_ERROR kevent. In this case, a file descriptor is
           * already closed from the kqueue thread, no need to close
           * it manually */ 
          _kh_cancel_sub (sub);
        }
    }

  if (sub->is_dir && n.flags & (NOTE_WRITE | NOTE_EXTEND))
    {
      _kh_dir_diff (sub, source);
      n.flags &= ~(NOTE_WRITE | NOTE_EXTEND);
    }

  if (n.flags)
    {
      gboolean done = FALSE;
      mask = convert_kqueue_events_to_gio (n.flags, &done);
      if (done == TRUE)
        g_file_monitor_source_handle_event (source, mask, NULL, NULL, NULL, g_get_monotonic_time ());
    }

  return TRUE;
}


/*
 * _kh_startup_impl:
 * @unused: unused
 *
 * Kqueue backend startup code. Should be called only once.
 *
 * Returns: %TRUE on success, %FALSE otherwise.
 **/
static gpointer
_kh_startup_impl (gpointer unused)
{
  GIOChannel *channel = NULL;
  gboolean result = FALSE;

  kqueue_descriptor = kqueue ();
  result = (kqueue_descriptor != -1);
  if (!result)
    {
      KH_W ("Failed to initialize kqueue\n!");
      return GINT_TO_POINTER (FALSE);
    }

  result = socketpair (AF_UNIX, SOCK_STREAM, 0, kqueue_socket_pair);
  if (result != 0)
    {
      KH_W ("Failed to create socket pair\n!");
      return GINT_TO_POINTER (FALSE) ;
    }

  result = pthread_create (&kqueue_thread,
                           NULL,
                           _kqueue_thread_func,
                           &kqueue_socket_pair[1]);
  if (result != 0)
    {
      KH_W ("Failed to run kqueue thread\n!");
      return GINT_TO_POINTER (FALSE);
    }

  _km_init (_kh_file_appeared_cb);

  channel = g_io_channel_unix_new (kqueue_socket_pair[0]);
  g_io_add_watch (channel, G_IO_IN, process_kqueue_notifications, NULL);

  subs_hash_table = g_hash_table_new (g_direct_hash, g_direct_equal);

  KH_W ("started gio kqueue backend\n");
  return GINT_TO_POINTER (TRUE);
}


/*
 * _kh_startup:
 * Kqueue backend initialization.
 *
 * Returns: %TRUE on success, %FALSE otherwise.
 **/
gboolean
_kh_startup (void)
{
  static GOnce init_once = G_ONCE_INIT;
  g_once (&init_once, _kh_startup_impl, NULL);
  return GPOINTER_TO_INT (init_once.retval);
}


/**
 * _kh_start_watching:
 * @sub: a #kqueue_sub
 *
 * Starts watching on a subscription.
 *
 * Returns: %TRUE on success, %FALSE otherwise.
 **/
gboolean
_kh_start_watching (kqueue_sub *sub)
{
  g_assert (kqueue_socket_pair[0] != -1);
  g_assert (sub != NULL);
  g_assert (sub->filename != NULL);

  /* kqueue requires a file descriptor to monitor. Sad but true */
#if defined (O_EVTONLY)
  sub->fd = open (sub->filename, O_EVTONLY);
#else
  sub->fd = open (sub->filename, O_RDONLY);
#endif

  if (sub->fd == -1)
    {
      KH_W ("failed to open file %s (error %d)", sub->filename, errno);
      return FALSE;
    }

  _ku_file_information (sub->fd, &sub->is_dir, NULL);
  if (sub->is_dir)
    {
      /* I know, it is very bad to make such decisions in this way and here.
       * We already do have an user_data at the #kqueue_sub, and it may point to
       * GKqueueFileMonitor or GKqueueDirectoryMonitor. For a directory case,
       * we need to scan in contents for the further diffs. Ideally this process
       * should be delegated to the GKqueueDirectoryMonitor, but for now I will
       * do it in a dirty way right here. */
      if (sub->deps)
        dl_free (sub->deps);

      sub->deps = dl_listing (sub->filename);  
    }

  G_LOCK (hash_lock);
  g_hash_table_insert (subs_hash_table, GINT_TO_POINTER (sub->fd), sub);
  G_UNLOCK (hash_lock);

  _kqueue_thread_push_fd (sub->fd);
  
  /* Bump the kqueue thread. It will pick up a new sub entry to monitor */
  if (!_ku_write (kqueue_socket_pair[0], "A", 1))
    KH_W ("Failed to bump the kqueue thread (add fd, error %d)", errno);
  return TRUE;
}


/**
 * _kh_add_sub:
 * @sub: a #kqueue_sub
 *
 * Adds a subscription for monitoring.
 *
 * This funciton tries to start watching a subscription with
 * _kh_start_watching(). On failure, i.e. when a file does not exist yet,
 * the subscription will be added to a list of missing files to continue
 * watching when the file will appear.
 *
 * Returns: %TRUE
 **/
gboolean
_kh_add_sub (kqueue_sub *sub)
{
  g_assert (sub != NULL);

  if (!_kh_start_watching (sub))
    _km_add_missing (sub);

  return TRUE;
}


/**
 * _kh_cancel_sub:
 * @sub a #kqueue_sub
 *
 * Stops monitoring on a subscription.
 *
 * Returns: %TRUE
 **/
gboolean
_kh_cancel_sub (kqueue_sub *sub)
{
  gboolean removed = FALSE;
  g_assert (kqueue_socket_pair[0] != -1);
  g_assert (sub != NULL);

  _km_remove (sub);

  G_LOCK (hash_lock);
  removed = g_hash_table_remove (subs_hash_table, GINT_TO_POINTER (sub->fd));
  G_UNLOCK (hash_lock);

  if (removed)
    {
      /* fd will be closed in the kqueue thread */
      _kqueue_thread_remove_fd (sub->fd);

      /* Bump the kqueue thread. It will pick up a new sub entry to remove*/
      if (!_ku_write (kqueue_socket_pair[0], "R", 1))
        KH_W ("Failed to bump the kqueue thread (remove fd, error %d)", errno);
    }

  return TRUE;
}


/**
 * _kh_file_appeared_cb:
 * @sub: a #kqueue_sub
 *
 * A callback function for kqueue-missing subsystem.
 *
 * Signals that a missing file has finally appeared in the filesystem.
 * Emits %G_FILE_MONITOR_EVENT_CREATED.
 **/
void
_kh_file_appeared_cb (kqueue_sub *sub)
{
  GFile* child;

  g_assert (sub != NULL);
  g_assert (sub->filename);

  if (!g_file_test (sub->filename, G_FILE_TEST_EXISTS))
    return;

  child = g_file_new_for_path (sub->filename);

  g_file_monitor_emit_event (G_FILE_MONITOR (sub->user_data),
                             child,
                             NULL,
                             G_FILE_MONITOR_EVENT_CREATED);

  g_object_unref (child);
}
