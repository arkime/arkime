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

#ifndef __G_LOCAL_FILE_H__
#define __G_LOCAL_FILE_H__

#include <gio/giotypes.h>

G_BEGIN_DECLS

#define G_TYPE_LOCAL_FILE         (_g_local_file_get_type ())
#define G_LOCAL_FILE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), G_TYPE_LOCAL_FILE, GLocalFile))
#define G_LOCAL_FILE_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), G_TYPE_LOCAL_FILE, GLocalFileClass))
#define G_IS_LOCAL_FILE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), G_TYPE_LOCAL_FILE))
#define G_IS_LOCAL_FILE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), G_TYPE_LOCAL_FILE))
#define G_LOCAL_FILE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), G_TYPE_LOCAL_FILE, GLocalFileClass))

typedef struct _GLocalFile        GLocalFile;
typedef struct _GLocalFileClass   GLocalFileClass;

struct _GLocalFileClass
{
  GObjectClass parent_class;
};

GType   _g_local_file_get_type (void) G_GNUC_CONST;

GFile * _g_local_file_new      (const char *filename);

const char * _g_local_file_get_filename (GLocalFile *file);

gboolean g_local_file_is_remote (const gchar *filename);

GFile * g_local_file_new_from_dirname_and_basename (const char *dirname,
                                                    const char *basename);

G_END_DECLS

#endif /* __G_LOCAL_FILE_H__ */
