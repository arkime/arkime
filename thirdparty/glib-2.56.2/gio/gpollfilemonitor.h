/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright (C) 2006-2007 Red Hat, Inc.
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
 * Author: Alexander Larsson <alexl@redhat.com>
 */

#ifndef __G_POLL_FILE_MONITOR_H__
#define __G_POLL_FILE_MONITOR_H__

#include <gio/gfilemonitor.h>

G_BEGIN_DECLS

#define G_TYPE_POLL_FILE_MONITOR		(_g_poll_file_monitor_get_type ())
#define G_POLL_FILE_MONITOR(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), G_TYPE_POLL_FILE_MONITOR, GPollFileMonitor))
#define G_POLL_FILE_MONITOR_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST ((k), G_TYPE_POLL_FILE_MONITOR, GPollFileMonitorClass))
#define G_IS_POLL_FILE_MONITOR(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), G_TYPE_POLL_FILE_MONITOR))
#define G_IS_POLL_FILE_MONITOR_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), G_TYPE_POLL_FILE_MONITOR))

typedef struct _GPollFileMonitor      GPollFileMonitor;
typedef struct _GPollFileMonitorClass GPollFileMonitorClass;

struct _GPollFileMonitorClass
{
  GFileMonitorClass parent_class;
};

GType          _g_poll_file_monitor_get_type (void) G_GNUC_CONST;

GFileMonitor * _g_poll_file_monitor_new      (GFile *file);

G_END_DECLS

#endif /* __G_POLL_FILE_MONITOR_H__ */
