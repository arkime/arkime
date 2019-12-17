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

#ifndef __G_CONTENT_TYPE_PRIVATE_H__
#define __G_CONTENT_TYPE_PRIVATE_H__

#include "gcontenttype.h"

G_BEGIN_DECLS

gsize  _g_unix_content_type_get_sniff_len (void);
char * _g_unix_content_type_unalias       (const char *type);
char **_g_unix_content_type_get_parents   (const char *type);

G_END_DECLS

#endif /* __G_CONTENT_TYPE_PRIVATE_H__ */
