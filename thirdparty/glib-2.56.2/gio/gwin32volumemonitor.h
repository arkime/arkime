/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright (C) 2006-2007 Red Hat, Inc.
 * Copyright (C) 2008 Hans Breuer
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
 *         David Zeuthen <davidz@redhat.com>
 *         Hans Breuer <hans@breuer.org>
 */

#ifndef __G_WIN32_VOLUME_MONITOR_H__
#define __G_WIN32_VOLUME_MONITOR_H__

#include <gio/gnativevolumemonitor.h>

G_BEGIN_DECLS

#define G_TYPE_WIN32_VOLUME_MONITOR        (_g_win32_volume_monitor_get_type ())
#define G_WIN32_VOLUME_MONITOR(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), G_TYPE_WIN32_VOLUME_MONITOR, GWin32VolumeMonitor))
#define G_WIN32_VOLUME_MONITOR_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), G_TYPE_WIN32_VOLUME_MONITOR, GWin32VolumeMonitorClass))
#define G_IS_WIN32_VOLUME_MONITOR(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), G_TYPE_WIN32_VOLUME_MONITOR))
#define G_IS_WIN32_VOLUME_MONITOR_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), G_TYPE_WIN32_VOLUME_MONITOR))

typedef struct _GWin32VolumeMonitor GWin32VolumeMonitor;
typedef struct _GWin32VolumeMonitorClass GWin32VolumeMonitorClass;

/* Forward definitions */

/**
 * GWin32Mount:
 *
 * Implementation of the #GMount interface for Win32 systems.
 */
typedef struct _GWin32Mount  GWin32Mount;
typedef struct _GWin32Volume GWin32Volume;

struct _GWin32VolumeMonitorClass
{
  GNativeVolumeMonitorClass parent_class;
};

GType            _g_win32_volume_monitor_get_type                     (void) G_GNUC_CONST;

GVolumeMonitor * _g_win32_volume_monitor_new                          (void);
GWin32Volume   * _g_win32_volume_monitor_lookup_volume_for_mount_path (GWin32VolumeMonitor *monitor,
                                                                       const char          *mount_path);

G_END_DECLS

#endif /* __G_WIN32_VOLUME_MONITOR_H__ */
