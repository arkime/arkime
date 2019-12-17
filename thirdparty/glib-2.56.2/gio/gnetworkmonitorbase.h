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

#ifndef __G_NETWORK_MONITOR_BASE_H__
#define __G_NETWORK_MONITOR_BASE_H__

#include <gio/giotypes.h>

G_BEGIN_DECLS

#define G_TYPE_NETWORK_MONITOR_BASE         (g_network_monitor_base_get_type ())
#define G_NETWORK_MONITOR_BASE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), G_TYPE_NETWORK_MONITOR_BASE, GNetworkMonitorBase))
#define G_NETWORK_MONITOR_BASE_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), G_TYPE_NETWORK_MONITOR_BASE, GNetworkMonitorBaseClass))
#define G_IS_NETWORK_MONITOR_BASE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), G_TYPE_NETWORK_MONITOR_BASE))
#define G_IS_NETWORK_MONITOR_BASE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), G_TYPE_NETWORK_MONITOR_BASE))
#define G_NETWORK_MONITOR_BASE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), G_TYPE_NETWORK_MONITOR_BASE, GNetworkMonitorBaseClass))

typedef struct _GNetworkMonitorBase        GNetworkMonitorBase;
typedef struct _GNetworkMonitorBaseClass   GNetworkMonitorBaseClass;
typedef struct _GNetworkMonitorBasePrivate GNetworkMonitorBasePrivate;

struct _GNetworkMonitorBase {
  GObject parent_instance;

  GNetworkMonitorBasePrivate *priv;
};

struct _GNetworkMonitorBaseClass {
  GObjectClass parent_class;

  /*< private >*/
  /* Padding for future expansion */
  gpointer padding[8];
};

GLIB_AVAILABLE_IN_ALL
GType g_network_monitor_base_get_type (void);

/*< protected >*/
GLIB_AVAILABLE_IN_2_32
void g_network_monitor_base_add_network    (GNetworkMonitorBase  *monitor,
					    GInetAddressMask     *network);
GLIB_AVAILABLE_IN_2_32
void g_network_monitor_base_remove_network (GNetworkMonitorBase  *monitor,
					    GInetAddressMask     *network);
GLIB_AVAILABLE_IN_ALL
void g_network_monitor_base_set_networks   (GNetworkMonitorBase  *monitor,
					    GInetAddressMask    **networks,
					    gint                  length);

G_END_DECLS

#endif /* __G_NETWORK_MONITOR_BASE_H__ */
