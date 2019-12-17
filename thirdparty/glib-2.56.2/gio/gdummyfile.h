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

#ifndef __G_DUMMY_FILE_H__
#define __G_DUMMY_FILE_H__

#include <gio/gio.h>

G_BEGIN_DECLS

#define G_TYPE_DUMMY_FILE         (_g_dummy_file_get_type ())
#define G_DUMMY_FILE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), G_TYPE_DUMMY_FILE, GDummyFile))
#define G_DUMMY_FILE_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), G_TYPE_DUMMY_FILE, GDummyFileClass))
#define G_IS_DUMMY_FILE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), G_TYPE_DUMMY_FILE))
#define G_IS_DUMMY_FILE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), G_TYPE_DUMMY_FILE))
#define G_DUMMY_FILE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), G_TYPE_DUMMY_FILE, GDummyFileClass))

typedef struct _GDummyFile        GDummyFile;
typedef struct _GDummyFileClass   GDummyFileClass;

struct _GDummyFileClass
{
  GObjectClass parent_class;
};

GType   _g_dummy_file_get_type (void) G_GNUC_CONST;

GFile * _g_dummy_file_new      (const char *uri);

G_END_DECLS

#endif /* __G_DUMMY_FILE_H__ */
