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
 *         David Zeuthen <davidz@redhat.com>
 */

#ifndef __G_UNIX_VOLUME_MONITOR_H__
#define __G_UNIX_VOLUME_MONITOR_H__

#include <gio/gnativevolumemonitor.h>

G_BEGIN_DECLS

#define G_TYPE_UNIX_VOLUME_MONITOR        (_g_unix_volume_monitor_get_type ())
#define G_UNIX_VOLUME_MONITOR(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), G_TYPE_UNIX_VOLUME_MONITOR, GUnixVolumeMonitor))
#define G_UNIX_VOLUME_MONITOR_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), G_TYPE_UNIX_VOLUME_MONITOR, GUnixVolumeMonitorClass))
#define G_IS_UNIX_VOLUME_MONITOR(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), G_TYPE_UNIX_VOLUME_MONITOR))
#define G_IS_UNIX_VOLUME_MONITOR_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), G_TYPE_UNIX_VOLUME_MONITOR))

typedef struct _GUnixVolumeMonitor      GUnixVolumeMonitor;
typedef struct _GUnixVolumeMonitorClass GUnixVolumeMonitorClass;

/* Forward definitions */

/**
 * GUnixMount:
 *
 * Implementation of the #GMount interface for Unix systems.
 */
typedef struct _GUnixMount GUnixMount;
typedef struct _GUnixVolume GUnixVolume;

struct _GUnixVolumeMonitorClass
{
  GNativeVolumeMonitorClass parent_class;
};

GType            _g_unix_volume_monitor_get_type                     (void) G_GNUC_CONST;

GVolumeMonitor * _g_unix_volume_monitor_new                          (void);
GUnixVolume    * _g_unix_volume_monitor_lookup_volume_for_mount_path (GUnixVolumeMonitor *monitor,
                                                                      const char         *mount_path);
void             _g_unix_volume_monitor_update                       (GUnixVolumeMonitor *monitor);

G_END_DECLS

#endif /* __G_UNIX_VOLUME_MONITOR_H__ */
