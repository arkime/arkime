/* GIO - GLib Input, Output and Streaming Library
 * 
 * Copyright (C) 2012 Red Hat, Inc.
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
 * Author: Colin Walters <walters@verbum.org>
 */

#ifndef __G_IO_WIN32_PRIV_H__
#define __G_IO_WIN32_PRIV_H__

#include "gwin32inputstream.h"
#include "gwin32outputstream.h"

G_BEGIN_DECLS

G_GNUC_INTERNAL
GInputStream *
g_win32_input_stream_new_from_fd (gint      fd,
				  gboolean  close_fd);

GOutputStream *
g_win32_output_stream_new_from_fd (gint      fd,
				   gboolean  close_fd);

G_END_DECLS

#endif /* __G_IO_MODULE_PRIV_H__ */
