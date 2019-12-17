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

#include "config.h"

#include "gnetworkmonitorbase.h"
#include "ginetaddress.h"
#include "ginetaddressmask.h"
#include "ginetsocketaddress.h"
#include "ginitable.h"
#include "gioerror.h"
#include "giomodule-priv.h"
#include "gnetworkmonitor.h"
#include "gsocketaddressenumerator.h"
#include "gsocketconnectable.h"
#include "gtask.h"
#include "glibintl.h"

static void g_network_monitor_base_iface_init (GNetworkMonitorInterface *iface);
static void g_network_monitor_base_initable_iface_init (GInitableIface *iface);

enum
{
  PROP_0,

  PROP_NETWORK_AVAILABLE,
  PROP_NETWORK_METERED,
  PROP_CONNECTIVITY
};

struct _GNetworkMonitorBasePrivate
{
  GPtrArray    *networks;
  gboolean      have_ipv4_default_route;
  gboolean      have_ipv6_default_route;
  gboolean      is_available;

  GMainContext *context;
  GSource      *network_changed_source;
  gboolean      initializing;
};

static guint network_changed_signal = 0;

static void queue_network_changed (GNetworkMonitorBase *monitor);

G_DEFINE_TYPE_WITH_CODE (GNetworkMonitorBase, g_network_monitor_base, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (GNetworkMonitorBase)
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                g_network_monitor_base_initable_iface_init)
                         G_IMPLEMENT_INTERFACE (G_TYPE_NETWORK_MONITOR,
                                                g_network_monitor_base_iface_init)
                         _g_io_modules_ensure_extension_points_registered ();
                         g_io_extension_point_implement (G_NETWORK_MONITOR_EXTENSION_POINT_NAME,
                                                         g_define_type_id,
                                                         "base",
                                                         0))

static void
g_network_monitor_base_init (GNetworkMonitorBase *monitor)
{
  monitor->priv = g_network_monitor_base_get_instance_private (monitor);
  monitor->priv->networks = g_ptr_array_new_with_free_func (g_object_unref);
  monitor->priv->context = g_main_context_get_thread_default ();
  if (monitor->priv->context)
    g_main_context_ref (monitor->priv->context);

  monitor->priv->initializing = TRUE;
}

static void
g_network_monitor_base_constructed (GObject *object)
{
  GNetworkMonitorBase *monitor = G_NETWORK_MONITOR_BASE (object);

  if (G_OBJECT_TYPE (monitor) == G_TYPE_NETWORK_MONITOR_BASE)
    {
      GInetAddressMask *mask;

      /* We're the dumb base class, not a smarter subclass. So just
       * assume that the network is available.
       */
      mask = g_inet_address_mask_new_from_string ("0.0.0.0/0", NULL);
      g_network_monitor_base_add_network (monitor, mask);
      g_object_unref (mask);

      mask = g_inet_address_mask_new_from_string ("::/0", NULL);
      if (mask)
        {
          /* On some environments (for example Windows without IPv6 support
           * enabled) the string "::/0" can't be processed and causes
           * g_inet_address_mask_new_from_string to return NULL */
          g_network_monitor_base_add_network (monitor, mask);
          g_object_unref (mask);
        }
    }
}

static void
g_network_monitor_base_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  GNetworkMonitorBase *monitor = G_NETWORK_MONITOR_BASE (object);

  switch (prop_id)
    {
    case PROP_NETWORK_AVAILABLE:
      g_value_set_boolean (value, monitor->priv->is_available);
      break;

    case PROP_NETWORK_METERED:
      /* Default to FALSE in the unknown case. */
      g_value_set_boolean (value, FALSE);
      break;

    case PROP_CONNECTIVITY:
      g_value_set_enum (value,
                        monitor->priv->is_available ?
                        G_NETWORK_CONNECTIVITY_FULL :
                        G_NETWORK_CONNECTIVITY_LOCAL);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }

}

static void
g_network_monitor_base_finalize (GObject *object)
{
  GNetworkMonitorBase *monitor = G_NETWORK_MONITOR_BASE (object);

  g_ptr_array_free (monitor->priv->networks, TRUE);
  if (monitor->priv->network_changed_source)
    {
      g_source_destroy (monitor->priv->network_changed_source);
      g_source_unref (monitor->priv->network_changed_source);
    }
  if (monitor->priv->context)
    g_main_context_unref (monitor->priv->context);

  G_OBJECT_CLASS (g_network_monitor_base_parent_class)->finalize (object);
}

static void
g_network_monitor_base_class_init (GNetworkMonitorBaseClass *monitor_class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (monitor_class);

  gobject_class->constructed  = g_network_monitor_base_constructed;
  gobject_class->get_property = g_network_monitor_base_get_property;
  gobject_class->finalize     = g_network_monitor_base_finalize;

  g_object_class_override_property (gobject_class, PROP_NETWORK_AVAILABLE, "network-available");
  g_object_class_override_property (gobject_class, PROP_NETWORK_METERED, "network-metered");
  g_object_class_override_property (gobject_class, PROP_CONNECTIVITY, "connectivity");
}

static gboolean
g_network_monitor_base_can_reach_sockaddr (GNetworkMonitorBase *base,
                                           GSocketAddress *sockaddr)
{
  GInetAddress *iaddr;
  int i;

  if (!G_IS_INET_SOCKET_ADDRESS (sockaddr))
    return FALSE;

  iaddr = g_inet_socket_address_get_address (G_INET_SOCKET_ADDRESS (sockaddr));
  for (i = 0; i < base->priv->networks->len; i++)
    {
      if (g_inet_address_mask_matches (base->priv->networks->pdata[i], iaddr))
        return TRUE;
    }

  return FALSE;
}

static gboolean
g_network_monitor_base_can_reach (GNetworkMonitor      *monitor,
                                  GSocketConnectable   *connectable,
                                  GCancellable         *cancellable,
                                  GError              **error)
{
  GNetworkMonitorBase *base = G_NETWORK_MONITOR_BASE (monitor);
  GSocketAddressEnumerator *enumerator;
  GSocketAddress *addr;

  if (base->priv->networks->len == 0)
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NETWORK_UNREACHABLE,
                           _("Network unreachable"));
      return FALSE;
    }

  enumerator = g_socket_connectable_proxy_enumerate (connectable);
  addr = g_socket_address_enumerator_next (enumerator, cancellable, error);
  if (!addr)
    {
      /* Either the user cancelled, or DNS resolution failed */
      g_object_unref (enumerator);
      return FALSE;
    }

  if (base->priv->have_ipv4_default_route &&
      base->priv->have_ipv6_default_route)
    {
      g_object_unref (enumerator);
      g_object_unref (addr);
      return TRUE;
    }

  while (addr)
    {
      if (g_network_monitor_base_can_reach_sockaddr (base, addr))
        {
          g_object_unref (addr);
          g_object_unref (enumerator);
          return TRUE;
        }

      g_object_unref (addr);
      addr = g_socket_address_enumerator_next (enumerator, cancellable, error);
    }
  g_object_unref (enumerator);

  if (error && !*error)
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_HOST_UNREACHABLE,
                           _("Host unreachable"));
    }
  return FALSE;
}

static void
can_reach_async_got_address (GObject      *object,
                             GAsyncResult *result,
                             gpointer      user_data)
{
  GSocketAddressEnumerator *enumerator = G_SOCKET_ADDRESS_ENUMERATOR (object);
  GTask *task = user_data;
  GNetworkMonitorBase *base = g_task_get_source_object (task);
  GSocketAddress *addr;
  GError *error = NULL;

  addr = g_socket_address_enumerator_next_finish (enumerator, result, &error);
  if (!addr)
    {
      if (error)
        {
          /* Either the user cancelled, or DNS resolution failed */
          g_task_return_error (task, error);
          g_object_unref (task);
          return;
        }
      else
        {
          /* Resolved all addresses, none matched */
          g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_HOST_UNREACHABLE,
                                   _("Host unreachable"));
          g_object_unref (task);
          return;
        }
    }

  if (g_network_monitor_base_can_reach_sockaddr (base, addr))
    {
      g_object_unref (addr);
      g_task_return_boolean (task, TRUE);
      g_object_unref (task);
      return;
    }
  g_object_unref (addr);

  g_socket_address_enumerator_next_async (enumerator,
                                          g_task_get_cancellable (task),
                                          can_reach_async_got_address, task);
}

static void
g_network_monitor_base_can_reach_async (GNetworkMonitor     *monitor,
                                        GSocketConnectable  *connectable,
                                        GCancellable        *cancellable,
                                        GAsyncReadyCallback  callback,
                                        gpointer             user_data)
{
  GTask *task;
  GSocketAddressEnumerator *enumerator;

  task = g_task_new (monitor, cancellable, callback, user_data);
  g_task_set_source_tag (task, g_network_monitor_base_can_reach_async);

  if (G_NETWORK_MONITOR_BASE (monitor)->priv->networks->len == 0)
    {
      g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_NETWORK_UNREACHABLE,
                               _("Network unreachable"));
      g_object_unref (task);
      return;
    }

  enumerator = g_socket_connectable_proxy_enumerate (connectable);
  g_socket_address_enumerator_next_async (enumerator, cancellable,
                                          can_reach_async_got_address, task);
  g_object_unref (enumerator);
}

static gboolean
g_network_monitor_base_can_reach_finish (GNetworkMonitor  *monitor,
                                         GAsyncResult     *result,
                                         GError          **error)
{
  g_return_val_if_fail (g_task_is_valid (result, monitor), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
g_network_monitor_base_iface_init (GNetworkMonitorInterface *monitor_iface)
{
  monitor_iface->can_reach = g_network_monitor_base_can_reach;
  monitor_iface->can_reach_async = g_network_monitor_base_can_reach_async;
  monitor_iface->can_reach_finish = g_network_monitor_base_can_reach_finish;

  network_changed_signal = g_signal_lookup ("network-changed", G_TYPE_NETWORK_MONITOR);
}

static gboolean
g_network_monitor_base_initable_init (GInitable     *initable,
                                      GCancellable  *cancellable,
                                      GError       **error)
{
  GNetworkMonitorBase *base = G_NETWORK_MONITOR_BASE (initable);

  base->priv->initializing = FALSE;

  return TRUE;
}

static void
g_network_monitor_base_initable_iface_init (GInitableIface *iface)
{
  iface->init = g_network_monitor_base_initable_init;
}

static gboolean
emit_network_changed (gpointer user_data)
{
  GNetworkMonitorBase *monitor = user_data;
  gboolean is_available;

  if (g_source_is_destroyed (g_main_current_source ()))
    return FALSE;

  g_object_ref (monitor);

  is_available = (monitor->priv->have_ipv4_default_route ||
                  monitor->priv->have_ipv6_default_route);
  if (monitor->priv->is_available != is_available)
    {
      monitor->priv->is_available = is_available;
      g_object_notify (G_OBJECT (monitor), "network-available");
    }

  g_signal_emit (monitor, network_changed_signal, 0, is_available);

  g_source_unref (monitor->priv->network_changed_source);
  monitor->priv->network_changed_source = NULL;

  g_object_unref (monitor);
  return FALSE;
}

static void
queue_network_changed (GNetworkMonitorBase *monitor)
{
  if (!monitor->priv->network_changed_source &&
      !monitor->priv->initializing)
    {
      GSource *source;

      source = g_idle_source_new ();
      /* Use G_PRIORITY_HIGH_IDLE priority so that multiple
       * network-change-related notifications coming in at
       * G_PRIORITY_DEFAULT will get coalesced into one signal
       * emission.
       */
      g_source_set_priority (source, G_PRIORITY_HIGH_IDLE);
      g_source_set_callback (source, emit_network_changed, monitor, NULL);
      g_source_set_name (source, "[gio] emit_network_changed");
      g_source_attach (source, monitor->priv->context);
      monitor->priv->network_changed_source = source;
    }

  /* Normally we wait to update is_available until we emit the signal,
   * to keep things consistent. But when we're first creating the
   * object, we want it to be correct right away.
   */
  if (monitor->priv->initializing)
    {
      monitor->priv->is_available = (monitor->priv->have_ipv4_default_route ||
                                     monitor->priv->have_ipv6_default_route);
    }
}

/**
 * g_network_monitor_base_add_network:
 * @monitor: the #GNetworkMonitorBase
 * @network: a #GInetAddressMask
 *
 * Adds @network to @monitor's list of available networks.
 *
 * Since: 2.32
 */
void
g_network_monitor_base_add_network (GNetworkMonitorBase *monitor,
                                    GInetAddressMask    *network)
{
  int i;

  for (i = 0; i < monitor->priv->networks->len; i++)
    {
      if (g_inet_address_mask_equal (monitor->priv->networks->pdata[i], network))
        return;
    }

  g_ptr_array_add (monitor->priv->networks, g_object_ref (network));
  if (g_inet_address_mask_get_length (network) == 0)
    {
      switch (g_inet_address_mask_get_family (network))
        {
        case G_SOCKET_FAMILY_IPV4:
          monitor->priv->have_ipv4_default_route = TRUE;
          break;
        case G_SOCKET_FAMILY_IPV6:
          monitor->priv->have_ipv6_default_route = TRUE;
          break;
        default:
          break;
        }
    }

  /* Don't emit network-changed when multicast-link-local routing
   * changes. This rather arbitrary decision is mostly because it
   * seems to change quite often...
   */
  if (g_inet_address_get_is_mc_link_local (g_inet_address_mask_get_address (network)))
    return;

  queue_network_changed (monitor);
}

/**
 * g_network_monitor_base_remove_network:
 * @monitor: the #GNetworkMonitorBase
 * @network: a #GInetAddressMask
 *
 * Removes @network from @monitor's list of available networks.
 *
 * Since: 2.32
 */
void
g_network_monitor_base_remove_network (GNetworkMonitorBase *monitor,
                                       GInetAddressMask    *network)
{
  int i;

  for (i = 0; i < monitor->priv->networks->len; i++)
    {
      if (g_inet_address_mask_equal (monitor->priv->networks->pdata[i], network))
        {
          g_ptr_array_remove_index_fast (monitor->priv->networks, i);

          if (g_inet_address_mask_get_length (network) == 0)
            {
              switch (g_inet_address_mask_get_family (network))
                {
                case G_SOCKET_FAMILY_IPV4:
                  monitor->priv->have_ipv4_default_route = FALSE;
                  break;
                case G_SOCKET_FAMILY_IPV6:
                  monitor->priv->have_ipv6_default_route = FALSE;
                  break;
                default:
                  break;
                }
            }

          queue_network_changed (monitor);
          return;
        }
    }
}

/**
 * g_network_monitor_base_set_networks:
 * @monitor: the #GNetworkMonitorBase
 * @networks: (array length=length): an array of #GInetAddressMask
 * @length: length of @networks
 *
 * Drops @monitor's current list of available networks and replaces
 * it with @networks.
 */
void
g_network_monitor_base_set_networks (GNetworkMonitorBase  *monitor,
                                     GInetAddressMask    **networks,
                                     gint                  length)
{
  int i;

  g_ptr_array_set_size (monitor->priv->networks, 0);
  monitor->priv->have_ipv4_default_route = FALSE;
  monitor->priv->have_ipv6_default_route = FALSE;

  for (i = 0; i < length; i++)
    g_network_monitor_base_add_network (monitor, networks[i]);
}
