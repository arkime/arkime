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

#ifndef __G_ASYNC_HELPER_H__
#define __G_ASYNC_HELPER_H__

#include <gio/gio.h>

#ifdef G_OS_WIN32
#include <windows.h>
#endif

G_BEGIN_DECLS

#ifdef G_OS_WIN32
gboolean _g_win32_overlap_wait_result (HANDLE           hfile,
                                       OVERLAPPED      *overlap,
                                       DWORD           *transferred,
                                       GCancellable    *cancellable);
#endif

G_END_DECLS

#endif /* __G_ASYNC_HELPER_H__ */
