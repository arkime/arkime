/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright (C) 2006-2007 Red Hat, Inc.
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
 * Author: Vlad Grecescu <b100dian@gmail.com>
 * Author: Chun-wei Fan <fanc999@yahoo.com.tw>
 *
 */

#include "config.h"

#include "gwin32filemonitor.h"
#include "gwin32fsmonitorutils.h"

#include <windows.h>

G_DEFINE_TYPE_WITH_CODE (GWin32FileMonitor, g_win32_file_monitor, G_TYPE_LOCAL_FILE_MONITOR,
                         g_io_extension_point_implement (G_LOCAL_FILE_MONITOR_EXTENSION_POINT_NAME,
                                                         g_define_type_id, "win32filemonitor", 20))

static void
g_win32_file_monitor_start (GLocalFileMonitor  *monitor,
                            const gchar        *dirname,
                            const gchar        *basename,
                            const gchar        *filename,
                            GFileMonitorSource *source)
{
  GWin32FileMonitor *win32_monitor = G_WIN32_FILE_MONITOR (monitor);

  win32_monitor->priv->fms = source;

  if (filename == NULL && basename == NULL)
    g_win32_fs_monitor_init (win32_monitor->priv, dirname, NULL, FALSE);
  else if (basename != NULL)
    g_win32_fs_monitor_init (win32_monitor->priv, dirname, basename, TRUE);
  else
    g_win32_fs_monitor_init (win32_monitor->priv, NULL, filename, TRUE);
}

static gboolean
g_win32_file_monitor_is_supported (void)
{
  return TRUE;
}

static void
g_win32_file_monitor_init (GWin32FileMonitor *monitor)
{
  monitor->priv = g_win32_fs_monitor_create (TRUE);

  monitor->priv->self = G_FILE_MONITOR (monitor);
}

static void
g_win32_file_monitor_finalize (GObject *object)
{
  GWin32FileMonitor *monitor;

  monitor = G_WIN32_FILE_MONITOR (object);

  g_win32_fs_monitor_finalize (monitor->priv);

  G_OBJECT_CLASS (g_win32_file_monitor_parent_class)->finalize (object);
}

static gboolean
g_win32_file_monitor_cancel (GFileMonitor* monitor)
{
  GWin32FileMonitor *file_monitor;

  file_monitor = G_WIN32_FILE_MONITOR (monitor);

  g_win32_fs_monitor_close_handle (file_monitor->priv);

  return TRUE;
}

static void
g_win32_file_monitor_class_init (GWin32FileMonitorClass *klass)
{
  GObjectClass* gobject_class = G_OBJECT_CLASS (klass);
  GFileMonitorClass *file_monitor_class = G_FILE_MONITOR_CLASS (klass);
  GLocalFileMonitorClass *local_file_monitor_class = G_LOCAL_FILE_MONITOR_CLASS (klass);

  gobject_class->finalize = g_win32_file_monitor_finalize;
  file_monitor_class->cancel = g_win32_file_monitor_cancel;

  local_file_monitor_class->is_supported = g_win32_file_monitor_is_supported;
  local_file_monitor_class->start = g_win32_file_monitor_start;
}
