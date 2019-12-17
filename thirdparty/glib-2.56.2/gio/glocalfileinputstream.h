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

#ifndef __G_LOCAL_FILE_INPUT_STREAM_H__
#define __G_LOCAL_FILE_INPUT_STREAM_H__

#include <gio/gfileinputstream.h>

G_BEGIN_DECLS

#define G_TYPE_LOCAL_FILE_INPUT_STREAM         (_g_local_file_input_stream_get_type ())
#define G_LOCAL_FILE_INPUT_STREAM(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), G_TYPE_LOCAL_FILE_INPUT_STREAM, GLocalFileInputStream))
#define G_LOCAL_FILE_INPUT_STREAM_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), G_TYPE_LOCAL_FILE_INPUT_STREAM, GLocalFileInputStreamClass))
#define G_IS_LOCAL_FILE_INPUT_STREAM(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), G_TYPE_LOCAL_FILE_INPUT_STREAM))
#define G_IS_LOCAL_FILE_INPUT_STREAM_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), G_TYPE_LOCAL_FILE_INPUT_STREAM))
#define G_LOCAL_FILE_INPUT_STREAM_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), G_TYPE_LOCAL_FILE_INPUT_STREAM, GLocalFileInputStreamClass))

typedef struct _GLocalFileInputStream         GLocalFileInputStream;
typedef struct _GLocalFileInputStreamClass    GLocalFileInputStreamClass;
typedef struct _GLocalFileInputStreamPrivate  GLocalFileInputStreamPrivate;

struct _GLocalFileInputStream
{
  GFileInputStream parent_instance;

  /*< private >*/
  GLocalFileInputStreamPrivate *priv;
};

struct _GLocalFileInputStreamClass
{
  GFileInputStreamClass parent_class;
};

GType              _g_local_file_input_stream_get_type (void) G_GNUC_CONST;

GFileInputStream *_g_local_file_input_stream_new          (int                    fd);
void              _g_local_file_input_stream_set_do_close (GLocalFileInputStream *in,
							   gboolean               do_close);


G_END_DECLS

#endif /* __G_LOCAL_FILE_INPUT_STREAM_H__ */
