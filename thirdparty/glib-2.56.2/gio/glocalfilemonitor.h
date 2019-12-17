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

#ifndef __G_LOCAL_FILE_MONITOR_H__
#define __G_LOCAL_FILE_MONITOR_H__

#include <gio/gfilemonitor.h>
#include "gunixmounts.h"

G_BEGIN_DECLS

#define G_TYPE_LOCAL_FILE_MONITOR		(g_local_file_monitor_get_type ())
#define G_LOCAL_FILE_MONITOR(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), G_TYPE_LOCAL_FILE_MONITOR, GLocalFileMonitor))
#define G_LOCAL_FILE_MONITOR_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST ((k), G_TYPE_LOCAL_FILE_MONITOR, GLocalFileMonitorClass))
#define G_IS_LOCAL_FILE_MONITOR(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), G_TYPE_LOCAL_FILE_MONITOR))
#define G_IS_LOCAL_FILE_MONITOR_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), G_TYPE_LOCAL_FILE_MONITOR))
#define G_LOCAL_FILE_MONITOR_GET_CLASS(o)       (G_TYPE_INSTANCE_GET_CLASS ((o), G_TYPE_LOCAL_FILE_MONITOR, GLocalFileMonitorClass))

#define G_LOCAL_FILE_MONITOR_EXTENSION_POINT_NAME "gio-local-file-monitor"
#define G_NFS_FILE_MONITOR_EXTENSION_POINT_NAME   "gio-nfs-file-monitor"

typedef struct _GLocalFileMonitor      GLocalFileMonitor;
typedef struct _GLocalFileMonitorClass GLocalFileMonitorClass;
typedef struct _GFileMonitorSource     GFileMonitorSource;

struct _GLocalFileMonitor
{
  GFileMonitor parent_instance;

  GFileMonitorSource *source;
  GUnixMountMonitor  *mount_monitor;
  gboolean            was_mounted;
};

struct _GLocalFileMonitorClass
{
  GFileMonitorClass parent_class;

  gboolean (* is_supported) (void);
  void     (* start)        (GLocalFileMonitor *local_monitor,
                             const gchar *dirname,
                             const gchar *basename,
                             const gchar *filename,
                             GFileMonitorSource *source);

  gboolean mount_notify;
};

#ifdef G_OS_UNIX
GLIB_AVAILABLE_IN_ALL
#endif
GType           g_local_file_monitor_get_type (void) G_GNUC_CONST;

/* for glocalfile.c */
GFileMonitor *
g_local_file_monitor_new_for_path (const gchar        *pathname,
                                   gboolean            is_directory,
                                   GFileMonitorFlags   flags,
                                   GError            **error);

/* for various users in glib */
typedef void (* GFileMonitorCallback) (GFileMonitor      *monitor,
                                       GFile             *child,
                                       GFile             *other,
                                       GFileMonitorEvent  event,
                                       gpointer           user_data);
GFileMonitor *
g_local_file_monitor_new_in_worker (const gchar           *pathname,
                                    gboolean               is_directory,
                                    GFileMonitorFlags      flags,
                                    GFileMonitorCallback   callback,
                                    gpointer               user_data,
                                    GError               **error);

/* for implementations of GLocalFileMonitor */
GLIB_AVAILABLE_IN_2_44
gboolean
g_file_monitor_source_handle_event (GFileMonitorSource *fms,
                                    GFileMonitorEvent   event_type,
                                    const gchar        *child,
                                    const gchar        *rename_to,
                                    GFile              *other,
                                    gint64              event_time);


G_END_DECLS

#endif /* __G_LOCAL_FILE_MONITOR_H__ */
