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

#ifndef __G_NETWORK_MONITOR_NETLINK_H__
#define __G_NETWORK_MONITOR_NETLINK_H__

#include "gnetworkmonitorbase.h"

G_BEGIN_DECLS

#define G_TYPE_NETWORK_MONITOR_NETLINK         (_g_network_monitor_netlink_get_type ())
#define G_NETWORK_MONITOR_NETLINK(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), G_TYPE_NETWORK_MONITOR_NETLINK, GNetworkMonitorNetlink))
#define G_NETWORK_MONITOR_NETLINK_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), G_TYPE_NETWORK_MONITOR_NETLINK, GNetworkMonitorNetlinkClass))
#define G_IS_NETWORK_MONITOR_NETLINK(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), G_TYPE_NETWORK_MONITOR_NETLINK))
#define G_IS_NETWORK_MONITOR_NETLINK_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), G_TYPE_NETWORK_MONITOR_NETLINK))
#define G_NETWORK_MONITOR_NETLINK_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), G_TYPE_NETWORK_MONITOR_NETLINK, GNetworkMonitorNetlinkClass))

typedef struct _GNetworkMonitorNetlink        GNetworkMonitorNetlink;
typedef struct _GNetworkMonitorNetlinkClass   GNetworkMonitorNetlinkClass;
typedef struct _GNetworkMonitorNetlinkPrivate GNetworkMonitorNetlinkPrivate;

struct _GNetworkMonitorNetlink {
  GNetworkMonitorBase parent_instance;

  GNetworkMonitorNetlinkPrivate *priv;
};

struct _GNetworkMonitorNetlinkClass {
  GNetworkMonitorBaseClass parent_class;

  /*< private >*/
  /* Padding for future expansion */
  gpointer padding[8];
};

GType _g_network_monitor_netlink_get_type (void);

G_END_DECLS

#endif /* __G_NETWORK_MONITOR_NETLINK_H__ */
