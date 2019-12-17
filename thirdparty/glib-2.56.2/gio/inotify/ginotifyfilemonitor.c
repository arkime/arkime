/* GIO - GLib Input, Output and Streaming Library
 * 
 * Copyright (C) 2006-2007 Red Hat, Inc.
 * Copyright (C) 2007 Sebastian Dröge.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Alexander Larsson <alexl@redhat.com>
 *          John McCutchan <john@johnmccutchan.com> 
 *          Sebastian Dröge <slomo@circular-chaos.org>
 *          Ryan Lortie <desrt@desrt.ca>
 */

#include "config.h"

#include "ginotifyfilemonitor.h"
#include <gio/giomodule.h>

#define USE_INOTIFY 1
#include "inotify-helper.h"

struct _GInotifyFileMonitor
{
  GLocalFileMonitor parent_instance;

  inotify_sub *sub;
};

G_DEFINE_TYPE_WITH_CODE (GInotifyFileMonitor, g_inotify_file_monitor, G_TYPE_LOCAL_FILE_MONITOR,
                         g_io_extension_point_implement (G_LOCAL_FILE_MONITOR_EXTENSION_POINT_NAME,
                                                         g_define_type_id, "inotify", 20))

static gboolean
g_inotify_file_monitor_is_supported (void)
{
  return _ih_startup ();
}

static void
g_inotify_file_monitor_start (GLocalFileMonitor  *local_monitor,
                              const gchar        *dirname,
                              const gchar        *basename,
                              const gchar        *filename,
                              GFileMonitorSource *source)
{
  GInotifyFileMonitor *inotify_monitor = G_INOTIFY_FILE_MONITOR (local_monitor);
  gboolean success;

  /* should already have been called, from is_supported() */
  success = _ih_startup ();
  g_assert (success);

  inotify_monitor->sub = _ih_sub_new (dirname, basename, filename, source);
  _ih_sub_add (inotify_monitor->sub);
}

static gboolean
g_inotify_file_monitor_cancel (GFileMonitor *monitor)
{
  GInotifyFileMonitor *inotify_monitor = G_INOTIFY_FILE_MONITOR (monitor);

  if (inotify_monitor->sub)
    {
      _ih_sub_cancel (inotify_monitor->sub);
      _ih_sub_free (inotify_monitor->sub);
      inotify_monitor->sub = NULL;
    }

  return TRUE;
}

static void
g_inotify_file_monitor_finalize (GObject *object)
{
  GInotifyFileMonitor *inotify_monitor = G_INOTIFY_FILE_MONITOR (object);

  /* must surely have been cancelled already */
  g_assert (!inotify_monitor->sub);

  G_OBJECT_CLASS (g_inotify_file_monitor_parent_class)->finalize (object);
}

static void
g_inotify_file_monitor_init (GInotifyFileMonitor* monitor)
{
}

static void
g_inotify_file_monitor_class_init (GInotifyFileMonitorClass* klass)
{
  GObjectClass* gobject_class = G_OBJECT_CLASS (klass);
  GFileMonitorClass *file_monitor_class = G_FILE_MONITOR_CLASS (klass);
  GLocalFileMonitorClass *local_file_monitor_class = G_LOCAL_FILE_MONITOR_CLASS (klass);

  local_file_monitor_class->is_supported = g_inotify_file_monitor_is_supported;
  local_file_monitor_class->start = g_inotify_file_monitor_start;
  local_file_monitor_class->mount_notify = TRUE;
  file_monitor_class->cancel = g_inotify_file_monitor_cancel;

  gobject_class->finalize = g_inotify_file_monitor_finalize;
}
