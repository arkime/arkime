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

#ifndef __G_FILE_ATTRIBUTE_PRIV_H__
#define __G_FILE_ATTRIBUTE_PRIV_H__

#include "gfileattribute.h"
#include "gfileinfo.h"

#define G_FILE_ATTRIBUTE_VALUE_INIT {0}

typedef struct  {
  GFileAttributeType type : 8;
  GFileAttributeStatus status : 8;
  union {
    gboolean boolean;
    gint32 int32;
    guint32 uint32;
    gint64 int64;
    guint64 uint64;
    char *string;
    GObject *obj;
    char **stringv;
  } u;
} GFileAttributeValue;

GFileAttributeValue *_g_file_attribute_value_new             (void);
void                 _g_file_attribute_value_free            (GFileAttributeValue *attr);
void                 _g_file_attribute_value_clear           (GFileAttributeValue *attr);
void                 _g_file_attribute_value_set             (GFileAttributeValue *attr,
							      const GFileAttributeValue *new_value);
GFileAttributeValue *_g_file_attribute_value_dup             (const GFileAttributeValue *other);
gpointer             _g_file_attribute_value_peek_as_pointer (GFileAttributeValue *attr);

char *               _g_file_attribute_value_as_string       (const GFileAttributeValue *attr);

const char *         _g_file_attribute_value_get_string      (const GFileAttributeValue *attr);
const char *         _g_file_attribute_value_get_byte_string (const GFileAttributeValue *attr);
gboolean             _g_file_attribute_value_get_boolean     (const GFileAttributeValue *attr);
guint32              _g_file_attribute_value_get_uint32      (const GFileAttributeValue *attr);
gint32               _g_file_attribute_value_get_int32       (const GFileAttributeValue *attr);
guint64              _g_file_attribute_value_get_uint64      (const GFileAttributeValue *attr);
gint64               _g_file_attribute_value_get_int64       (const GFileAttributeValue *attr);
GObject *            _g_file_attribute_value_get_object      (const GFileAttributeValue *attr);
char **              _g_file_attribute_value_get_stringv     (const GFileAttributeValue *attr);

void                 _g_file_attribute_value_set_from_pointer(GFileAttributeValue *attr,
							      GFileAttributeType   type,
							      gpointer             value_p,
							      gboolean             dup);
void                 _g_file_attribute_value_set_string      (GFileAttributeValue *attr,
							      const char          *string);
void                 _g_file_attribute_value_set_byte_string (GFileAttributeValue *attr,
							      const char          *string);
void                 _g_file_attribute_value_set_boolean     (GFileAttributeValue *attr,
							      gboolean             value);
void                 _g_file_attribute_value_set_uint32      (GFileAttributeValue *attr,
							      guint32              value);
void                 _g_file_attribute_value_set_int32       (GFileAttributeValue *attr,
							      gint32               value);
void                 _g_file_attribute_value_set_uint64      (GFileAttributeValue *attr,
							      guint64              value);
void                 _g_file_attribute_value_set_int64       (GFileAttributeValue *attr,
							      gint64               value);
void                 _g_file_attribute_value_set_object      (GFileAttributeValue *attr,
							      GObject             *obj);
void                 _g_file_attribute_value_set_stringv     (GFileAttributeValue *attr,
							      char               **value);


GFileAttributeValue *_g_file_info_get_attribute_value (GFileInfo  *info,
						       const char *attribute);

#endif /* __G_FILE_ATTRIBUTE_PRIV_H__ */
