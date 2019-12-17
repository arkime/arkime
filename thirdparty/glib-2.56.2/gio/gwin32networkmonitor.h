/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright 2014-2018 Jan-Michael Brummer <jan.brummer@tabos.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __G_WIN32_NETWORK_MONITOR_H__
#define __G_WIN32_NETWORK_MONITOR_H__

#include "gnetworkmonitorbase.h"

G_BEGIN_DECLS

#define G_TYPE_WIN32_NETWORK_MONITOR         (_g_win32_network_monitor_get_type ())
#define G_WIN32_NETWORK_MONITOR(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), G_TYPE_WIN32_NETWORK_MONITOR, GWin32NetworkMonitor))
#define G_WIN32_NETWORK_MONITOR_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), G_TYPE_WIN32_NETWORK_MONITOR, GWin32NetworkMonitorClass))
#define G_IS_WIN32_NETWORK_MONITOR(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), G_TYPE_WIN32_NETWORK_MONITOR))
#define G_IS_WIN32_NETWORK_MONITOR_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), G_TYPE_WIN32_NETWORK_MONITOR))
#define G_WIN32_NETWORK_MONITOR_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), G_TYPE_WIN32_NETWORK_MONITOR, GWin32NetworkMonitorClass))

typedef struct _GWin32NetworkMonitor        GWin32NetworkMonitor;
typedef struct _GWin32NetworkMonitorClass   GWin32NetworkMonitorClass;
typedef struct _GWin32NetworkMonitorPrivate GWin32NetworkMonitorPrivate;

struct _GWin32NetworkMonitor {
  GNetworkMonitorBase parent_instance;

  GWin32NetworkMonitorPrivate *priv;
};

struct _GWin32NetworkMonitorClass {
  GNetworkMonitorBaseClass parent_class;
};

GType _g_win32_network_monitor_get_type (void);

G_END_DECLS

#endif /* __G_WIN32_NETWORK_MONITOR_H__ */
