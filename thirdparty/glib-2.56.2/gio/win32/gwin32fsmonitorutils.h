/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright (C) 2006-2007 Red Hat, Inc.
 * Copyright (C) 2014 Chun-wei Fan
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
 * Author: Vlad Grecescu <b100dian@gmail.com>
 * Author: Chun-wei Fan <fanc999@yahoo.com.tw>
 *
 */

#ifndef __G_WIN32_FS_MONITOR_UTILS_H__
#define __G_WIN32_FS_MONITOR_UTILS_H__

#include <windows.h>

#include "gio/glocalfilemonitor.h"

#include "gio/gfilemonitor.h"

G_BEGIN_DECLS

typedef struct _GWin32FSMonitorPrivate GWin32FSMonitorPrivate;

struct _GWin32FSMonitorPrivate
{
  OVERLAPPED overlapped;
  DWORD buffer_allocated_bytes;
  PFILE_NOTIFY_INFORMATION file_notify_buffer;
  DWORD buffer_filled_bytes;
  HANDLE hDirectory;
  gboolean isfile;
  wchar_t *wfullpath_with_long_prefix;
  wchar_t *wfilename_short;
  wchar_t *wfilename_long;
  DWORD file_attribs;
  PFILE_NOTIFY_INFORMATION pfni_prev;
  /* Needed in the APC where we only have this private struct */
  GFileMonitor *self;
  GFileMonitorSource *fms;
};

enum GWin32FileMonitorFileAlias
{
  G_WIN32_FILE_MONITOR_NO_ALIAS = 0,
  G_WIN32_FILE_MONITOR_LONG_FILENAME,
  G_WIN32_FILE_MONITOR_SHORT_FILENAME,
  G_WIN32_FILE_MONITOR_NO_MATCH_FOUND
};

GWin32FSMonitorPrivate* g_win32_fs_monitor_create (gboolean isfile);

void g_win32_fs_monitor_init (GWin32FSMonitorPrivate *monitor,
                              const gchar *dirname,
                              const gchar *filename,
                              gboolean isfile);

void g_win32_fs_monitor_finalize (GWin32FSMonitorPrivate *monitor);

void g_win32_fs_monitor_close_handle (GWin32FSMonitorPrivate *monitor);

G_END_DECLS

#endif
