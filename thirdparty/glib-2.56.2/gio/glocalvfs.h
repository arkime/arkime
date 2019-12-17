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

#ifndef __G_LOCAL_VFS_H__
#define __G_LOCAL_VFS_H__

#include <gio/giotypes.h>

G_BEGIN_DECLS

#define G_TYPE_LOCAL_VFS            (_g_local_vfs_get_type ())
#define G_LOCAL_VFS(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), G_TYPE_LOCAL_VFS, GLocalVfs))
#define G_LOCAL_VFS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), G_TYPE_LOCAL_VFS, GLocalVfsClass))
#define G_IS_LOCAL_VFS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), G_TYPE_LOCAL_VFS))
#define G_IS_LOCAL_VFS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), G_TYPE_LOCAL_VFS))
#define G_LOCAL_VFS_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), G_TYPE_LOCAL_VFS, GLocalVfsClass))

typedef struct _GLocalVfs       GLocalVfs;
typedef struct _GLocalVfsClass  GLocalVfsClass;

GType   _g_local_vfs_get_type  (void) G_GNUC_CONST;

GVfs  * _g_local_vfs_new       (void);

G_END_DECLS

#endif /* __G_LOCAL_VFS_H__ */
