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

#ifndef __G_LOCAL_FILE_ENUMERATOR_H__
#define __G_LOCAL_FILE_ENUMERATOR_H__

#include <gfileenumerator.h>
#include <glocalfile.h>

G_BEGIN_DECLS

#define G_TYPE_LOCAL_FILE_ENUMERATOR         (_g_local_file_enumerator_get_type ())
#define G_LOCAL_FILE_ENUMERATOR(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), G_TYPE_LOCAL_FILE_ENUMERATOR, GLocalFileEnumerator))
#define G_LOCAL_FILE_ENUMERATOR_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), G_TYPE_LOCAL_FILE_ENUMERATOR, GLocalFileEnumeratorClass))
#define G_IS_LOCAL_FILE_ENUMERATOR(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), G_TYPE_LOCAL_FILE_ENUMERATOR))
#define G_IS_LOCAL_FILE_ENUMERATOR_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), G_TYPE_LOCAL_FILE_ENUMERATOR))
#define G_LOCAL_FILE_ENUMERATOR_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), G_TYPE_LOCAL_FILE_ENUMERATOR, GLocalFileEnumeratorClass))

typedef struct _GLocalFileEnumerator         GLocalFileEnumerator;
typedef struct _GLocalFileEnumeratorClass    GLocalFileEnumeratorClass;
typedef struct _GLocalFileEnumeratorPrivate  GLocalFileEnumeratorPrivate;

struct _GLocalFileEnumeratorClass
{
  GFileEnumeratorClass parent_class;
};

GType             _g_local_file_enumerator_get_type (void) G_GNUC_CONST;

GFileEnumerator * _g_local_file_enumerator_new      (GLocalFile           *file,
                                                     const char           *attributes,
                                                     GFileQueryInfoFlags   flags,
                                                     GCancellable         *cancellable,
                                                     GError              **error);

G_END_DECLS

#endif /* __G_FILE_LOCAL_FILE_ENUMERATOR_H__ */
