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
 * Author: Chun-wei Fan <fanc999@yahoo.com.tw>
 *
 */
#ifndef __G_WIN32_FILE_MONITOR_H__
#define __G_WIN32_FILE_MONITOR_H__

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>
#include <stdlib.h>
#include <string.h>

#include "gio/gfilemonitor.h"
#include "gio/glocalfilemonitor.h"
#include "gio/giomodule.h"

#include "gwin32fsmonitorutils.h"

G_BEGIN_DECLS

#define G_TYPE_WIN32_FILE_MONITOR (g_win32_file_monitor_get_type ())
#define G_WIN32_FILE_MONITOR(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), G_TYPE_WIN32_FILE_MONITOR, GWin32FileMonitor))
#define G_WIN32_FILE_MONITOR_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), G_TYPE_WIN32_FILE_MONITOR, GWin32FileMonitorClass))
#define G_IS_WIN32_FILE_MONITOR(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), G_TYPE_WIN32_FILE_MONITOR))
#define G_IS_WIN32_FILE_MONITOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), G_TYPE_WIN32_FILE_MONITOR))
#define G_WIN32_FILE_MONITOR_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), G_TYPE_WIN32_FILE_MONITOR, GWin32FileMonitorClass))

typedef struct _GWin32FileMonitor GWin32FileMonitor;
typedef struct _GWin32FileMonitorClass GWin32FileMonitorClass;
typedef struct _GWin32FileMonitorPrivate GWin32FileMonitorPrivate;

struct _GWin32FileMonitor {
  GLocalFileMonitor parent_instance;
  GWin32FSMonitorPrivate * priv;
};
struct _GWin32FileMonitorClass {
  GLocalFileMonitorClass parent_class;
};

GType g_win32_file_monitor_get_type (void);
void g_win32_file_monitor_register (GIOModule *module);

G_END_DECLS

#endif /* __G_WIN32_FILE_MONITOR_H__ */
