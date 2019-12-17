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

#ifndef __G_UNIX_VOLUME_H__
#define __G_UNIX_VOLUME_H__

#include <gio/giotypes.h>
#include <gio/gunixvolumemonitor.h>
#include <gio/gunixmounts.h>

G_BEGIN_DECLS

#define G_TYPE_UNIX_VOLUME        (_g_unix_volume_get_type ())
#define G_UNIX_VOLUME(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), G_TYPE_UNIX_VOLUME, GUnixVolume))
#define G_UNIX_VOLUME_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), G_TYPE_UNIX_VOLUME, GUnixVolumeClass))
#define G_IS_UNIX_VOLUME(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), G_TYPE_UNIX_VOLUME))
#define G_IS_UNIX_VOLUME_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), G_TYPE_UNIX_VOLUME))
G_DEFINE_AUTOPTR_CLEANUP_FUNC(GUnixVolume, g_object_unref)

typedef struct _GUnixVolumeClass GUnixVolumeClass;

struct _GUnixVolumeClass
{
  GObjectClass parent_class;
};

GType         _g_unix_volume_get_type       (void) G_GNUC_CONST;

GUnixVolume * _g_unix_volume_new            (GVolumeMonitor  *volume_monitor,
                                             GUnixMountPoint *mountpoint);
gboolean      _g_unix_volume_has_mount_path (GUnixVolume     *volume,
                                             const char      *mount_path);
void          _g_unix_volume_set_mount      (GUnixVolume     *volume,
                                             GUnixMount      *mount);
void          _g_unix_volume_unset_mount    (GUnixVolume     *volume,
                                             GUnixMount      *mount);
void          _g_unix_volume_disconnected   (GUnixVolume     *volume);

G_END_DECLS

#endif /* __G_UNIX_VOLUME_H__ */
