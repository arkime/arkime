/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright (C) 2011 Red Hat, Inc.
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

#ifndef __G_RESOURCE_FILE_H__
#define __G_RESOURCE_FILE_H__

#include <gio/giotypes.h>

G_BEGIN_DECLS

#define G_TYPE_RESOURCE_FILE         (_g_resource_file_get_type ())
#define G_RESOURCE_FILE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), G_TYPE_RESOURCE_FILE, GResourceFile))
#define G_RESOURCE_FILE_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), G_TYPE_RESOURCE_FILE, GResourceFileClass))
#define G_IS_RESOURCE_FILE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), G_TYPE_RESOURCE_FILE))
#define G_IS_RESOURCE_FILE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), G_TYPE_RESOURCE_FILE))
#define G_RESOURCE_FILE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), G_TYPE_RESOURCE_FILE, GResourceFileClass))

typedef struct _GResourceFile        GResourceFile;
typedef struct _GResourceFileClass   GResourceFileClass;

struct _GResourceFileClass
{
  GObjectClass parent_class;
};

GType   _g_resource_file_get_type (void) G_GNUC_CONST;

GFile * _g_resource_file_new      (const char *uri);

G_END_DECLS

#endif /* __G_RESOURCE_FILE_H__ */
