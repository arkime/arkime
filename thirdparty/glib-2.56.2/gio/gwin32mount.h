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

#ifndef __G_WIN32_MOUNT_H__
#define __G_WIN32_MOUNT_H__

#include <gio/giotypes.h>

G_BEGIN_DECLS

#define G_TYPE_WIN32_MOUNT        (_g_win32_mount_get_type ())
#define G_WIN32_MOUNT(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), G_TYPE_WIN32_MOUNT, GWin32Mount))
#define G_WIN32_MOUNT_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), G_TYPE_WIN32_MOUNT, GWin32MountClass))
#define G_IS_WIN32_MOUNT(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), G_TYPE_WIN32_MOUNT))
#define G_IS_WIN32_MOUNT_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), G_TYPE_WIN32_MOUNT))
G_DEFINE_AUTOPTR_CLEANUP_FUNC(GWin32Mount, g_object_unref)

typedef struct _GWin32MountClass GWin32MountClass;

struct _GWin32MountClass
{
  GObjectClass parent_class;
};

GType         _g_win32_mount_get_type     (void) G_GNUC_CONST;

GWin32Mount * _g_win32_mount_new          (GVolumeMonitor *volume_monitor,
                                           const char     *path,
                                           GWin32Volume   *volume);
void          _g_win32_mount_unset_volume (GWin32Mount    *mount,
                                           GWin32Volume   *volume);
void          _g_win32_mount_unmounted    (GWin32Mount    *mount);

G_END_DECLS

#endif /* __G_WIN32_MOUNT_H__ */
