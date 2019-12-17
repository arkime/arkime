/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright 2011 Red Hat, Inc.
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
 */

#ifndef __G_NETWORK_MONITOR_NM_H__
#define __G_NETWORK_MONITOR_NM_H__

#include "gnetworkmonitornetlink.h"

G_BEGIN_DECLS

#define G_TYPE_NETWORK_MONITOR_NM         (_g_network_monitor_nm_get_type ())
#define G_NETWORK_MONITOR_NM(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), G_TYPE_NETWORK_MONITOR_NM, GNetworkMonitorNM))
#define G_NETWORK_MONITOR_NM_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), G_TYPE_NETWORK_MONITOR_NM, GNetworkMonitorNMClass))
#define G_IS_NETWORK_MONITOR_NM(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), G_TYPE_NETWORK_MONITOR_NM))
#define G_IS_NETWORK_MONITOR_NM_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), G_TYPE_NETWORK_MONITOR_NM))
#define G_NETWORK_MONITOR_NM_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), G_TYPE_NETWORK_MONITOR_NM, GNetworkMonitorNMClass))
#define G_NETWORK_MONITOR_NM_GET_INITABLE_IFACE(o) (G_TYPE_INSTANCE_GET_INTERFACE ((o), G_TYPE_INITABLE, GInitable))


typedef struct _GNetworkMonitorNM        GNetworkMonitorNM;
typedef struct _GNetworkMonitorNMClass   GNetworkMonitorNMClass;
typedef struct _GNetworkMonitorNMPrivate GNetworkMonitorNMPrivate;

struct _GNetworkMonitorNM {
  GNetworkMonitorNetlink parent_instance;

  GNetworkMonitorNMPrivate *priv;
};

struct _GNetworkMonitorNMClass {
  GNetworkMonitorNetlinkClass parent_class;
};

GType _g_network_monitor_nm_get_type (void);

G_END_DECLS

#endif /* __G_NETWORK_MONITOR_NM_H__ */
