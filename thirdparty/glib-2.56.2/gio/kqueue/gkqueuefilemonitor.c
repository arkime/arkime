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

#include "gkqueuefilemonitor.h"
#include "kqueue-helper.h"
#include "kqueue-exclusions.h"
#include <gio/gpollfilemonitor.h>
#include <gio/gfile.h>
#include <gio/giomodule.h>


struct _GKqueueFileMonitor
{
  GLocalFileMonitor parent_instance;

  kqueue_sub *sub;

  GFileMonitor *fallback;
  GFile *fbfile;
};

static gboolean g_kqueue_file_monitor_cancel (GFileMonitor* monitor);

G_DEFINE_TYPE_WITH_CODE (GKqueueFileMonitor, g_kqueue_file_monitor, G_TYPE_LOCAL_FILE_MONITOR,
       g_io_extension_point_implement (G_LOCAL_FILE_MONITOR_EXTENSION_POINT_NAME,
               g_define_type_id,
               "kqueue",
               20))


static void
_fallback_callback (GFileMonitor      *unused,
                    GFile             *first,
                    GFile             *second,
                    GFileMonitorEvent  event,
                    gpointer           udata)
{
  GKqueueFileMonitor *kq_mon = G_KQUEUE_FILE_MONITOR (udata);
  GFileMonitor *mon = G_FILE_MONITOR (kq_mon);
  g_assert (kq_mon != NULL);
  g_assert (mon != NULL);
  (void) unused;

  if (event == G_FILE_MONITOR_EVENT_CHANGED)
    {
      GLocalFileMonitor *local_monitor = G_LOCAL_FILE_MONITOR (kq_mon);

      _kh_dir_diff (kq_mon->sub, local_monitor->source);
    }
  else
    g_file_monitor_emit_event (mon, first, second, event);
}


static void
g_kqueue_file_monitor_finalize (GObject *object)
{
  GKqueueFileMonitor *kqueue_monitor = G_KQUEUE_FILE_MONITOR (object);

  if (kqueue_monitor->sub)
    {
      _kh_cancel_sub (kqueue_monitor->sub);
      _kh_sub_free (kqueue_monitor->sub);
      kqueue_monitor->sub = NULL;
    }

  if (kqueue_monitor->fallback)
    g_object_unref (kqueue_monitor->fallback);

  if (kqueue_monitor->fbfile)
    g_object_unref (kqueue_monitor->fbfile);

  if (G_OBJECT_CLASS (g_kqueue_file_monitor_parent_class)->finalize)
    (*G_OBJECT_CLASS (g_kqueue_file_monitor_parent_class)->finalize) (object);
}

static void
g_kqueue_file_monitor_start (GLocalFileMonitor *local_monitor,
                             const gchar *dirname,
                             const gchar *basename,
                             const gchar *filename,
                             GFileMonitorSource *source)
{
  GKqueueFileMonitor *kqueue_monitor = G_KQUEUE_FILE_MONITOR (local_monitor);
  GObject *obj;
  GKqueueFileMonitorClass *klass;
  GObjectClass *parent_class;
  kqueue_sub *sub = NULL;
  gboolean ret_kh_startup = FALSE;
  const gchar *path = NULL; 


  ret_kh_startup = _kh_startup ();
  g_assert (ret_kh_startup);

  path = filename;
  if (!path)
    path = dirname;

  /* For a directory monitor, create a subscription object anyway.
   * It will be used for directory diff calculation routines. 
   * Wait, directory diff in a GKqueueFileMonitor?
   * Yes, it is. When a file monitor is started on an non-existent
   * file, GIO uses a GKqueueFileMonitor object for that. If a directory
   * will be created under that path, GKqueueFileMonitor will have to
   * handle the directory notifications. */

  sub = _kh_sub_new (path, TRUE, source);

  /* FIXME: what to do about errors here? we can't return NULL or another
   * kind of error and an assertion is probably too hard (same issue as in
   * the inotify backend) */
  g_assert (sub != NULL);
  kqueue_monitor->sub = sub;

  if (!_ke_is_excluded (path))
    _kh_add_sub (sub);
  else
    {
      GFile *file = g_file_new_for_path (path);
      kqueue_monitor->fbfile = file;
      kqueue_monitor->fallback = _g_poll_file_monitor_new (file);
      g_signal_connect (kqueue_monitor->fallback,
                        "changed",
                        G_CALLBACK (_fallback_callback),
                        kqueue_monitor);
    }
}

static gboolean
g_kqueue_file_monitor_is_supported (void)
{
  return _kh_startup ();
}

static void
g_kqueue_file_monitor_class_init (GKqueueFileMonitorClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GFileMonitorClass *file_monitor_class = G_FILE_MONITOR_CLASS (klass);
  GLocalFileMonitorClass *local_file_monitor_class = G_LOCAL_FILE_MONITOR_CLASS (klass);

  gobject_class->finalize = g_kqueue_file_monitor_finalize;
  file_monitor_class->cancel = g_kqueue_file_monitor_cancel;

  local_file_monitor_class->is_supported = g_kqueue_file_monitor_is_supported;
  local_file_monitor_class->start = g_kqueue_file_monitor_start;
  local_file_monitor_class->mount_notify = TRUE; /* TODO: ??? */
}

static void
g_kqueue_file_monitor_init (GKqueueFileMonitor *monitor)
{
}

static gboolean
g_kqueue_file_monitor_cancel (GFileMonitor *monitor)
{
  GKqueueFileMonitor *kqueue_monitor = G_KQUEUE_FILE_MONITOR (monitor);

  if (kqueue_monitor->sub)
    {
      _kh_cancel_sub (kqueue_monitor->sub);
      _kh_sub_free (kqueue_monitor->sub);
      kqueue_monitor->sub = NULL;
    }
  else if (kqueue_monitor->fallback)
    {
      g_signal_handlers_disconnect_by_func (kqueue_monitor->fallback, _fallback_callback, kqueue_monitor);
      g_file_monitor_cancel (kqueue_monitor->fallback);
    }

  if (G_FILE_MONITOR_CLASS (g_kqueue_file_monitor_parent_class)->cancel)
    (*G_FILE_MONITOR_CLASS (g_kqueue_file_monitor_parent_class)->cancel) (monitor);

  return TRUE;
}
