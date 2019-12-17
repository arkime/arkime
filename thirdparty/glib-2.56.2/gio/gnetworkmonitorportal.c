/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright 2016 Red Hat, Inc.
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

#include "gnetworkmonitorportal.h"
#include "ginitable.h"
#include "giomodule-priv.h"
#include "xdp-dbus.h"
#include "gportalsupport.h"

static GInitableIface *initable_parent_iface;
static void g_network_monitor_portal_iface_init (GNetworkMonitorInterface *iface);
static void g_network_monitor_portal_initable_iface_init (GInitableIface *iface);

enum
{
  PROP_0,
  PROP_NETWORK_AVAILABLE,
  PROP_NETWORK_METERED,
  PROP_CONNECTIVITY
};

struct _GNetworkMonitorPortalPrivate
{
  GDBusProxy *proxy;
  gboolean has_network;
  int version;

  gboolean available;
  gboolean metered;
  GNetworkConnectivity connectivity;
};

G_DEFINE_TYPE_WITH_CODE (GNetworkMonitorPortal, g_network_monitor_portal, G_TYPE_NETWORK_MONITOR_BASE,
                         G_ADD_PRIVATE (GNetworkMonitorPortal)
                         G_IMPLEMENT_INTERFACE (G_TYPE_NETWORK_MONITOR,
                                                g_network_monitor_portal_iface_init)
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                g_network_monitor_portal_initable_iface_init)
                         _g_io_modules_ensure_extension_points_registered ();
                         g_io_extension_point_implement (G_NETWORK_MONITOR_EXTENSION_POINT_NAME,
                                                         g_define_type_id,
                                                         "portal",
                                                         40))

static void
g_network_monitor_portal_init (GNetworkMonitorPortal *nm)
{
  nm->priv = g_network_monitor_portal_get_instance_private (nm);
}

static void
g_network_monitor_portal_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  GNetworkMonitorPortal *nm = G_NETWORK_MONITOR_PORTAL (object);

  switch (prop_id)
    {
    case PROP_NETWORK_AVAILABLE:
      g_value_set_boolean (value, nm->priv->available);
      break;

    case PROP_NETWORK_METERED:
      g_value_set_boolean (value, nm->priv->metered);
      break;

    case PROP_CONNECTIVITY:
      g_value_set_enum (value, nm->priv->connectivity);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
got_available (GObject *source,
               GAsyncResult *res,
               gpointer data)
{
  GDBusProxy *proxy = G_DBUS_PROXY (source);
  GNetworkMonitorPortal *nm = G_NETWORK_MONITOR_PORTAL (data);
  GError *error = NULL;
  GVariant *ret;
  gboolean available;
  
  ret = g_dbus_proxy_call_finish (proxy, res, &error);
  if (ret == NULL)
    {
      g_warning ("%s", error->message);
      g_clear_error (&error);
      return;
    }

  g_variant_get (ret, "(b)", &available);
  g_variant_unref (ret);

  if (nm->priv->available != available)
    {
      nm->priv->available = available;
      g_object_notify (G_OBJECT (nm), "network-available");
      g_signal_emit_by_name (nm, "network-changed", available);
    }
}

static void
got_metered (GObject *source,
             GAsyncResult *res,
             gpointer data)
{
  GDBusProxy *proxy = G_DBUS_PROXY (source);
  GNetworkMonitorPortal *nm = G_NETWORK_MONITOR_PORTAL (data);
  GError *error = NULL;
  GVariant *ret;
  gboolean metered;
  
  ret = g_dbus_proxy_call_finish (proxy, res, &error);
  if (ret == NULL)
    {
      g_warning ("%s", error->message);
      g_clear_error (&error);
      return;
    }

  g_variant_get (ret, "(b)", &metered);
  g_variant_unref (ret);

  if (nm->priv->metered != metered)
    {
      nm->priv->metered = metered;
      g_object_notify (G_OBJECT (nm), "network-metered");
    }
}

static void
got_connectivity (GObject *source,
                  GAsyncResult *res,
                  gpointer data)
{
  GDBusProxy *proxy = G_DBUS_PROXY (source);
  GNetworkMonitorPortal *nm = G_NETWORK_MONITOR_PORTAL (data);
  GError *error = NULL;
  GVariant *ret;
  GNetworkConnectivity connectivity;
  
  ret = g_dbus_proxy_call_finish (proxy, res, &error);
  if (ret == NULL)
    {
      g_warning ("%s", error->message);
      g_clear_error (&error);
      return;
    }

  g_variant_get (ret, "(u)", &connectivity);
  g_variant_unref (ret);

  if (nm->priv->connectivity != connectivity)
    {
      nm->priv->connectivity = connectivity;
      g_object_notify (G_OBJECT (nm), "connectivity");
    }
}

static void
update_properties (GDBusProxy *proxy,
                   GNetworkMonitorPortal *nm)
{
  g_dbus_proxy_call (proxy, "GetConnectivity", NULL, 0, -1, NULL, got_connectivity, nm);
  g_dbus_proxy_call (proxy, "GetMetered", NULL, 0, -1, NULL, got_metered, nm);
  g_dbus_proxy_call (proxy, "GetAvailable", NULL, 0, -1, NULL, got_available, nm);
}

static void
proxy_signal (GDBusProxy *proxy,
              const char *sender,
              const char *signal,
              GVariant *parameters,
              GNetworkMonitorPortal *nm)
{
  if (!nm->priv->has_network)
    return;

  if (nm->priv->version == 1)
    {
      gboolean available;

      g_variant_get (parameters, "(b)", &available);
      g_signal_emit_by_name (nm, "network-changed", available);
    }
  else if (nm->priv->version == 2)
    {
      update_properties (proxy, nm);
    }
}

static void
proxy_properties_changed (GDBusProxy *proxy,
                          GVariant *changed,
                          GVariant *invalidated,
                          GNetworkMonitorPortal *nm)
{
  if (!nm->priv->has_network)
    return;

  if (nm->priv->version == 1)
    {
      GVariant *ret;

      ret = g_dbus_proxy_get_cached_property (proxy, "connectivity");
      if (ret)
        {
          GNetworkConnectivity connectivity = g_variant_get_uint32 (ret);
          if (nm->priv->connectivity != connectivity)
            {
              nm->priv->connectivity = connectivity;
              g_object_notify (G_OBJECT (nm), "connectivity");
            }
          g_variant_unref (ret);
        }

      ret = g_dbus_proxy_get_cached_property (proxy, "metered");
      if (ret)
        {
          gboolean metered = g_variant_get_boolean (ret);
          if (nm->priv->metered != metered)
            {
              nm->priv->metered = metered;
              g_object_notify (G_OBJECT (nm), "network-metered");
            }
          g_variant_unref (ret);
        }

      ret = g_dbus_proxy_get_cached_property (proxy, "available");
      if (ret)
        {
          gboolean available = g_variant_get_boolean (ret);
          if (nm->priv->available != available)
            {
              nm->priv->available = available;
              g_object_notify (G_OBJECT (nm), "network-available");
              g_signal_emit_by_name (nm, "network-changed", available);
            }
          g_variant_unref (ret);
        }
    }
}
                           
static gboolean
g_network_monitor_portal_initable_init (GInitable     *initable,
                                        GCancellable  *cancellable,
                                        GError       **error)
{
  GNetworkMonitorPortal *nm = G_NETWORK_MONITOR_PORTAL (initable);
  GDBusProxy *proxy;
  gchar *name_owner = NULL;
  int version;
  GVariant *ret;

  nm->priv->available = FALSE;
  nm->priv->metered = FALSE;
  nm->priv->connectivity = G_NETWORK_CONNECTIVITY_LOCAL;

  if (!glib_should_use_portal ())
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, "Not using portals");
      return FALSE;
    }

  proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                         G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START
                                         | G_DBUS_PROXY_FLAGS_GET_INVALIDATED_PROPERTIES,
                                         NULL,
                                         "org.freedesktop.portal.Desktop",
                                         "/org/freedesktop/portal/desktop",
					 "org.freedesktop.portal.NetworkMonitor",
                                         cancellable,
                                         error);
  if (!proxy)
    return FALSE;

  name_owner = g_dbus_proxy_get_name_owner (proxy);

  if (!name_owner)
    {
      g_object_unref (proxy);
      g_set_error (error,
                   G_DBUS_ERROR,
                   G_DBUS_ERROR_NAME_HAS_NO_OWNER,
                   "Desktop portal not found");
      return FALSE;
    }

  g_free (name_owner);

  ret = g_dbus_proxy_get_cached_property (proxy, "version");
  g_variant_get (ret, "u", &version);
  g_variant_unref (ret);

  if (version != 1 && version != 2)
    {
      g_object_unref (proxy);
      g_set_error (error,
                   G_DBUS_ERROR,
                   G_DBUS_ERROR_NAME_HAS_NO_OWNER,
                   "NetworkMonitor portal unsupported version: %d", version);
      return FALSE;
    }

  g_signal_connect (proxy, "g-signal", G_CALLBACK (proxy_signal), nm);
  g_signal_connect (proxy, "g-properties-changed", G_CALLBACK (proxy_properties_changed), nm);

  nm->priv->proxy = proxy;
  nm->priv->has_network = glib_network_available_in_sandbox ();
  nm->priv->version = version;

  if (!initable_parent_iface->init (initable, cancellable, error))
    return FALSE;

  if (nm->priv->has_network && nm->priv->version == 2)
    update_properties (proxy, nm);

  return TRUE;
}

static void
g_network_monitor_portal_finalize (GObject *object)
{
  GNetworkMonitorPortal *nm = G_NETWORK_MONITOR_PORTAL (object);

  g_clear_object (&nm->priv->proxy);

  G_OBJECT_CLASS (g_network_monitor_portal_parent_class)->finalize (object);
}

static void
g_network_monitor_portal_class_init (GNetworkMonitorPortalClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  gobject_class->finalize  = g_network_monitor_portal_finalize;
  gobject_class->get_property = g_network_monitor_portal_get_property;

  g_object_class_override_property (gobject_class, PROP_NETWORK_AVAILABLE, "network-available");
  g_object_class_override_property (gobject_class, PROP_NETWORK_METERED, "network-metered");
  g_object_class_override_property (gobject_class, PROP_CONNECTIVITY, "connectivity");
}

static void
g_network_monitor_portal_iface_init (GNetworkMonitorInterface *monitor_iface)
{
}

static void
g_network_monitor_portal_initable_iface_init (GInitableIface *iface)
{
  initable_parent_iface = g_type_interface_peek_parent (iface);

  iface->init = g_network_monitor_portal_initable_init;
}
