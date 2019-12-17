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
 */

#ifndef __G_INOTIFY_FILE_MONITOR_H__
#define __G_INOTIFY_FILE_MONITOR_H__

#include <glib-object.h>
#include <string.h>
#include <gio/gfilemonitor.h>
#include <gio/glocalfilemonitor.h>
#include <gio/giomodule.h>

G_BEGIN_DECLS

#define G_TYPE_INOTIFY_FILE_MONITOR		(g_inotify_file_monitor_get_type ())
#define G_INOTIFY_FILE_MONITOR(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), G_TYPE_INOTIFY_FILE_MONITOR, GInotifyFileMonitor))
#define G_INOTIFY_FILE_MONITOR_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST ((k), G_TYPE_INOTIFY_FILE_MONITOR, GInotifyFileMonitorClass))
#define G_IS_INOTIFY_FILE_MONITOR(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), G_TYPE_INOTIFY_FILE_MONITOR))
#define G_IS_INOTIFY_FILE_MONITOR_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), G_TYPE_INOTIFY_FILE_MONITOR))

typedef struct _GInotifyFileMonitor      GInotifyFileMonitor;
typedef struct _GInotifyFileMonitorClass GInotifyFileMonitorClass;

struct _GInotifyFileMonitorClass {
  GLocalFileMonitorClass parent_class;
};

GType g_inotify_file_monitor_get_type (void);

G_END_DECLS

#endif /* __G_INOTIFY_FILE_MONITOR_H__ */
