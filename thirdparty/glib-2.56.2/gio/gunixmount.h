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

#ifndef __G_UNIX_MOUNT_H__
#define __G_UNIX_MOUNT_H__

#include <gio/giotypes.h>

#include "gunixmounts.h"

G_BEGIN_DECLS

#define G_TYPE_UNIX_MOUNT        (_g_unix_mount_get_type ())
#define G_UNIX_MOUNT(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), G_TYPE_UNIX_MOUNT, GUnixMount))
#define G_UNIX_MOUNT_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), G_TYPE_UNIX_MOUNT, GUnixMountClass))
#define G_IS_UNIX_MOUNT(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), G_TYPE_UNIX_MOUNT))
#define G_IS_UNIX_MOUNT_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), G_TYPE_UNIX_MOUNT))
G_DEFINE_AUTOPTR_CLEANUP_FUNC(GUnixMount, g_object_unref)

typedef struct _GUnixMountClass GUnixMountClass;

struct _GUnixMountClass
{
  GObjectClass parent_class;
};

GType        _g_unix_mount_get_type       (void) G_GNUC_CONST;

GUnixMount * _g_unix_mount_new            (GVolumeMonitor  *volume_monitor,
                                           GUnixMountEntry *mount_entry,
                                           GUnixVolume     *volume);
gboolean     _g_unix_mount_has_mount_path (GUnixMount      *mount,
                                           const char      *mount_path);
void         _g_unix_mount_unset_volume   (GUnixMount      *mount,
                                           GUnixVolume     *volume);
void         _g_unix_mount_unmounted      (GUnixMount      *mount);

G_END_DECLS

#endif /* __G_UNIX_MOUNT_H__ */
