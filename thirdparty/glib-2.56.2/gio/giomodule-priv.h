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

#ifndef __G_IO_MODULE_PRIV_H__
#define __G_IO_MODULE_PRIV_H__

#include "giomodule.h"

G_BEGIN_DECLS

void _g_io_modules_ensure_extension_points_registered (void);
void _g_io_modules_ensure_loaded                      (void);

typedef gboolean (*GIOModuleVerifyFunc) (gpointer);
gpointer _g_io_module_get_default (const gchar         *extension_point,
				   const gchar         *envvar,
				   GIOModuleVerifyFunc  verify_func);

GType    _g_io_module_get_default_type (const gchar *extension_point,
                                        const gchar *envvar,
                                        guint        is_supported_offset);

#ifdef G_PLATFORM_WIN32
void *_g_io_win32_get_module (void);
#endif

gchar *_g_io_module_extract_name (const char *filename);


G_END_DECLS

#endif /* __G_IO_MODULE_PRIV_H__ */
