/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright 2014 Red Hat, Inc.
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

#include "config.h"

#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "gnetworkmonitornm.h"
#include "gioerror.h"
#include "ginitable.h"
#include "giomodule-priv.h"
#include "glibintl.h"
#include "glib/gstdio.h"
#include "gnetworkingprivate.h"
#include "gnetworkmonitor.h"
#include "gdbusproxy.h"

static void g_network_monitor_nm_iface_init (GNetworkMonitorInterface *iface);
static void g_network_monitor_nm_initable_iface_init (GInitableIface *iface);

enum
{
  PROP_0,

  PROP_NETWORK_AVAILABLE,
  PROP_NETWORK_METERED,
  PROP_CONNECTIVITY
};

typedef enum {
  NM_CONNECTIVITY_UNKNOWN,
  NM_CONNECTIVITY_NONE,
  NM_CONNECTIVITY_PORTAL,
  NM_CONNECTIVITY_LIMITED,
  NM_CONNECTIVITY_FULL
} NMConnectivityState;

struct _GNetworkMonitorNMPrivate
{
  GDBusProxy *proxy;

  GNetworkConnectivity connectivity;
  gboolean network_available;
  gboolean network_metered;
};

#define g_network_monitor_nm_get_type _g_network_monitor_nm_get_type
G_DEFINE_TYPE_WITH_CODE (GNetworkMonitorNM, g_network_monitor_nm, G_TYPE_NETWORK_MONITOR_NETLINK,
                         G_ADD_PRIVATE (GNetworkMonitorNM)
                         G_IMPLEMENT_INTERFACE (G_TYPE_NETWORK_MONITOR,
                                                g_network_monitor_nm_iface_init)
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                g_network_monitor_nm_initable_iface_init)
                         _g_io_modules_ensure_extension_points_registered ();
                         g_io_extension_point_implement (G_NETWORK_MONITOR_EXTENSION_POINT_NAME,
                                                         g_define_type_id,
                                                         "networkmanager",
                                                         30))

static void
g_network_monitor_nm_init (GNetworkMonitorNM *nm)
{
  nm->priv = g_network_monitor_nm_get_instance_private (nm);
}

static void
g_network_monitor_nm_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  GNetworkMonitorNM *nm = G_NETWORK_MONITOR_NM (object);

  switch (prop_id)
    {
    case PROP_NETWORK_AVAILABLE:
      g_value_set_boolean (value, nm->priv->network_available);
      break;

    case PROP_NETWORK_METERED:
      g_value_set_boolean (value, nm->priv->network_metered);
      break;

    case PROP_CONNECTIVITY:
      g_value_set_enum (value, nm->priv->connectivity);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static GNetworkConnectivity
nm_conn_to_g_conn (int nm_state)
{
  switch (nm_state)
    {
      case NM_CONNECTIVITY_UNKNOWN:
        return G_NETWORK_CONNECTIVITY_LOCAL;
      case NM_CONNECTIVITY_NONE:
        return G_NETWORK_CONNECTIVITY_LOCAL;
      case NM_CONNECTIVITY_PORTAL:
        return G_NETWORK_CONNECTIVITY_PORTAL;
      case NM_CONNECTIVITY_LIMITED:
        return G_NETWORK_CONNECTIVITY_LIMITED;
      case NM_CONNECTIVITY_FULL:
        return G_NETWORK_CONNECTIVITY_FULL;
      default:
        g_warning ("Unknown NM connectivity state %d", nm_state);
        return G_NETWORK_CONNECTIVITY_LOCAL;
    }
}

static gboolean
nm_metered_to_bool (guint nm_metered)
{
  switch (nm_metered)
    {
      case 1: /* yes */
      case 3: /* guess-yes */
        return TRUE;
      case 0: /* unknown */
        /* We default to FALSE in the unknown-because-you're-not-running-NM
         * case, so we should return FALSE in the
         * unknown-when-you-are-running-NM case too. */
      case 2: /* no */
      case 4: /* guess-no */
        return FALSE;
      default:
        g_warning ("Unknown NM metered state %d", nm_metered);
        return FALSE;
    }
}

static void
sync_properties (GNetworkMonitorNM *nm,
                 gboolean           emit_signals)
{
  GVariant *v;
  NMConnectivityState nm_connectivity;
  gboolean new_network_available;
  gboolean new_network_metered;
  GNetworkConnectivity new_connectivity;

  v = g_dbus_proxy_get_cached_property (nm->priv->proxy, "Connectivity");
  if (!v)
    return;

  nm_connectivity = g_variant_get_uint32 (v);
  g_variant_unref (v);

  if (nm_connectivity == NM_CONNECTIVITY_NONE)
    {
      new_network_available = FALSE;
      new_network_metered = FALSE;
      new_connectivity = G_NETWORK_CONNECTIVITY_LOCAL;
    }
  else
    {

      /* this is only available post NM 1.0 */
      v = g_dbus_proxy_get_cached_property (nm->priv->proxy, "Metered");
      if (v == NULL)
        {
          new_network_metered = FALSE;
        }
      else
        {
          new_network_metered = nm_metered_to_bool (g_variant_get_uint32 (v));
          g_variant_unref (v);
        }

      new_network_available = TRUE;
      new_connectivity = nm_conn_to_g_conn (nm_connectivity);
    }

  if (!emit_signals)
    {
      nm->priv->network_metered = new_network_metered;
      nm->priv->network_available = new_network_available;
      nm->priv->connectivity = new_connectivity;
      return;
    }

  if (new_network_available != nm->priv->network_available)
    {
      nm->priv->network_available = new_network_available;
      g_object_notify (G_OBJECT (nm), "network-available");
    }
  if (new_network_metered != nm->priv->network_metered)
    {
      nm->priv->network_metered = new_network_metered;
      g_object_notify (G_OBJECT (nm), "network-metered");
    }
  if (new_connectivity != nm->priv->connectivity)
    {
      nm->priv->connectivity = new_connectivity;
      g_object_notify (G_OBJECT (nm), "connectivity");
    }
}

static void
update_cached_property (GDBusProxy   *proxy,
                        const char   *property_name,
                        GVariantDict *dict)
{
  GVariant *v;

  v = g_variant_dict_lookup_value (dict, property_name, NULL);
  if (!v)
    return;
  g_dbus_proxy_set_cached_property (proxy, property_name, v);
  g_variant_unref (v);
}

static void
proxy_signal_cb (GDBusProxy        *proxy,
                 gchar             *sender_name,
                 gchar             *signal_name,
                 GVariant          *parameters,
                 GNetworkMonitorNM *nm)
{
  GVariant *asv;
  GVariantDict *dict;

  if (g_strcmp0 (signal_name, "PropertiesChanged") != 0)
    return;

  g_variant_get (parameters, "(@a{sv})", &asv);
  if (!asv)
    return;

  dict = g_variant_dict_new (asv);
  g_variant_unref (asv);
  if (!dict)
    {
      g_warning ("Failed to handle PropertiesChanged signal from NetworkManager");
      return;
    }

  update_cached_property (nm->priv->proxy, "Connectivity", dict);

  g_variant_dict_unref (dict);

  sync_properties (nm, TRUE);
}

static gboolean
has_property (GDBusProxy *proxy,
              const char *property_name)
{
  char **props;
  gboolean prop_found = FALSE;

  props = g_dbus_proxy_get_cached_property_names (proxy);

  if (!props)
    return FALSE;

  prop_found = g_strv_contains ((const gchar * const *) props, property_name);
  g_strfreev (props);
  return prop_found;
}

static gboolean
g_network_monitor_nm_initable_init (GInitable     *initable,
                                    GCancellable  *cancellable,
                                    GError       **error)
{
  GNetworkMonitorNM *nm = G_NETWORK_MONITOR_NM (initable);
  GDBusProxy *proxy;
  GInitableIface *parent_iface;
  gchar *name_owner = NULL;

  parent_iface = g_type_interface_peek_parent (G_NETWORK_MONITOR_NM_GET_INITABLE_IFACE (initable));
  if (!parent_iface->init (initable, cancellable, error))
    return FALSE;

  proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
                                         G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START | G_DBUS_PROXY_FLAGS_GET_INVALIDATED_PROPERTIES,
                                         NULL,
                                         "org.freedesktop.NetworkManager",
                                         "/org/freedesktop/NetworkManager",
                                         "org.freedesktop.NetworkManager",
                                         cancellable,
                                         error);
  if (!proxy)
    return FALSE;

  name_owner = g_dbus_proxy_get_name_owner (proxy);

  if (!name_owner)
    {
      g_object_unref (proxy);
      return FALSE;
    }

  g_free (name_owner);

  /* Verify it has the PrimaryConnection and Connectivity properties */
  if (!has_property (proxy, "Connectivity"))
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   _("NetworkManager version too old"));
      g_object_unref (proxy);
      return FALSE;
    }

  g_signal_connect (G_OBJECT (proxy), "g-signal",
                    G_CALLBACK (proxy_signal_cb), nm);
  nm->priv->proxy = proxy;
  sync_properties (nm, FALSE);

  return TRUE;
}

static void
g_network_monitor_nm_finalize (GObject *object)
{
  GNetworkMonitorNM *nm = G_NETWORK_MONITOR_NM (object);

  g_clear_object (&nm->priv->proxy);

  G_OBJECT_CLASS (g_network_monitor_nm_parent_class)->finalize (object);
}

static void
g_network_monitor_nm_class_init (GNetworkMonitorNMClass *nl_class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (nl_class);

  gobject_class->finalize  = g_network_monitor_nm_finalize;
  gobject_class->get_property = g_network_monitor_nm_get_property;

  g_object_class_override_property (gobject_class, PROP_NETWORK_AVAILABLE, "network-available");
  g_object_class_override_property (gobject_class, PROP_NETWORK_METERED, "network-metered");
  g_object_class_override_property (gobject_class, PROP_CONNECTIVITY, "connectivity");
}

static void
g_network_monitor_nm_iface_init (GNetworkMonitorInterface *monitor_iface)
{
}

static void
g_network_monitor_nm_initable_iface_init (GInitableIface *iface)
{
  iface->init = g_network_monitor_nm_initable_init;
}
