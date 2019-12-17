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

#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "gnetworkmonitornetlink.h"
#include "gcredentials.h"
#include "ginetaddressmask.h"
#include "ginitable.h"
#include "giomodule-priv.h"
#include "glibintl.h"
#include "glib/gstdio.h"
#include "gnetworkingprivate.h"
#include "gnetworkmonitor.h"
#include "gsocket.h"
#include "gunixcredentialsmessage.h"

/* must come at the end to pick system includes from
 * gnetworkingprivate.h */
#include <linux/netlink.h>
#include <linux/rtnetlink.h>

static GInitableIface *initable_parent_iface;
static void g_network_monitor_netlink_iface_init (GNetworkMonitorInterface *iface);
static void g_network_monitor_netlink_initable_iface_init (GInitableIface *iface);

struct _GNetworkMonitorNetlinkPrivate
{
  GSocket *sock;
  GSource *source, *dump_source;
  GMainContext *context;

  GPtrArray *dump_networks;
};

static gboolean read_netlink_messages (GSocket             *socket,
                                       GIOCondition         condition,
                                       gpointer             user_data);
static gboolean request_dump (GNetworkMonitorNetlink  *nl,
                              GError                 **error);

#define g_network_monitor_netlink_get_type _g_network_monitor_netlink_get_type
G_DEFINE_TYPE_WITH_CODE (GNetworkMonitorNetlink, g_network_monitor_netlink, G_TYPE_NETWORK_MONITOR_BASE,
                         G_ADD_PRIVATE (GNetworkMonitorNetlink)
                         G_IMPLEMENT_INTERFACE (G_TYPE_NETWORK_MONITOR,
                                                g_network_monitor_netlink_iface_init)
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                g_network_monitor_netlink_initable_iface_init)
                         _g_io_modules_ensure_extension_points_registered ();
                         g_io_extension_point_implement (G_NETWORK_MONITOR_EXTENSION_POINT_NAME,
                                                         g_define_type_id,
                                                         "netlink",
                                                         20))

static void
g_network_monitor_netlink_init (GNetworkMonitorNetlink *nl)
{
  nl->priv = g_network_monitor_netlink_get_instance_private (nl);
}

static gboolean
g_network_monitor_netlink_initable_init (GInitable     *initable,
                                         GCancellable  *cancellable,
                                         GError       **error)
{
  GNetworkMonitorNetlink *nl = G_NETWORK_MONITOR_NETLINK (initable);
  gint sockfd;
  struct sockaddr_nl snl;

  /* We create the socket the old-school way because sockaddr_netlink
   * can't be represented as a GSocketAddress
   */
  sockfd = g_socket (PF_NETLINK, SOCK_RAW, NETLINK_ROUTE, NULL);
  if (sockfd == -1)
    {
      int errsv = errno;
      g_set_error (error, G_IO_ERROR, g_io_error_from_errno (errsv),
                   _("Could not create network monitor: %s"),
                   g_strerror (errsv));
      return FALSE;
    }

  snl.nl_family = AF_NETLINK;
  snl.nl_pid = snl.nl_pad = 0;
  snl.nl_groups = RTMGRP_IPV4_ROUTE | RTMGRP_IPV6_ROUTE;
  if (bind (sockfd, (struct sockaddr *)&snl, sizeof (snl)) != 0)
    {
      int errsv = errno;
      g_set_error (error, G_IO_ERROR, g_io_error_from_errno (errsv),
                   _("Could not create network monitor: %s"),
                   g_strerror (errsv));
      (void) g_close (sockfd, NULL);
      return FALSE;
    }

  nl->priv->sock = g_socket_new_from_fd (sockfd, error);
  if (error)
    {
      g_prefix_error (error, "%s", _("Could not create network monitor: "));
      (void) g_close (sockfd, NULL);
      return FALSE;
    }

  if (!g_socket_set_option (nl->priv->sock, SOL_SOCKET, SO_PASSCRED,
			    TRUE, NULL))
    {
      int errsv = errno;
      g_set_error (error, G_IO_ERROR, g_io_error_from_errno (errsv),
                   _("Could not create network monitor: %s"),
                   g_strerror (errsv));
      return FALSE;
    }

  /* Request the current state */
  if (!request_dump (nl, error))
    return FALSE;

  /* And read responses; since we haven't yet marked the socket
   * non-blocking, each call will block until a message is received.
   */
  while (nl->priv->dump_networks)
    {
      if (!read_netlink_messages (NULL, G_IO_IN, nl))
        break;
    }

  g_socket_set_blocking (nl->priv->sock, FALSE);
  nl->priv->context = g_main_context_ref_thread_default ();
  nl->priv->source = g_socket_create_source (nl->priv->sock, G_IO_IN, NULL);
  g_source_set_callback (nl->priv->source,
                         (GSourceFunc) read_netlink_messages, nl, NULL);
  g_source_attach (nl->priv->source, nl->priv->context);

  return initable_parent_iface->init (initable, cancellable, error);
}

static gboolean
request_dump (GNetworkMonitorNetlink  *nl,
              GError                 **error)
{
  struct nlmsghdr *n;
  struct rtgenmsg *gen;
  gchar buf[NLMSG_SPACE (sizeof (*gen))];

  memset (buf, 0, sizeof (buf));
  n = (struct nlmsghdr*) buf;
  n->nlmsg_len = NLMSG_LENGTH (sizeof (*gen));
  n->nlmsg_type = RTM_GETROUTE;
  n->nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
  n->nlmsg_pid = 0;
  gen = NLMSG_DATA (n);
  gen->rtgen_family = AF_UNSPEC;

  if (g_socket_send (nl->priv->sock, buf, sizeof (buf),
                     NULL, error) < 0)
    {
      g_prefix_error (error, "%s", _("Could not get network status: "));
      return FALSE;
    }

  nl->priv->dump_networks = g_ptr_array_new_with_free_func (g_object_unref);
  return TRUE;
}

static gboolean
timeout_request_dump (gpointer user_data)
{
  GNetworkMonitorNetlink *nl = user_data;

  g_source_destroy (nl->priv->dump_source);
  g_source_unref (nl->priv->dump_source);
  nl->priv->dump_source = NULL;

  request_dump (nl, NULL);

  return FALSE;
}

static void
queue_request_dump (GNetworkMonitorNetlink *nl)
{
  if (nl->priv->dump_networks)
    return;

  if (nl->priv->dump_source)
    {
      g_source_destroy (nl->priv->dump_source);
      g_source_unref (nl->priv->dump_source);
    }

  nl->priv->dump_source = g_timeout_source_new_seconds (1);
  g_source_set_callback (nl->priv->dump_source,
                         (GSourceFunc) timeout_request_dump, nl, NULL);
  g_source_attach (nl->priv->dump_source, nl->priv->context);
}

static GInetAddressMask *
create_inet_address_mask (GSocketFamily  family,
                          const guint8  *dest,
                          gsize          dest_len)
{
  GInetAddress *dest_addr;
  GInetAddressMask *network;

  if (dest)
    dest_addr = g_inet_address_new_from_bytes (dest, family);
  else
    dest_addr = g_inet_address_new_any (family);
  network = g_inet_address_mask_new (dest_addr, dest_len, NULL);
  g_object_unref (dest_addr);

  return network;
}

static void
add_network (GNetworkMonitorNetlink *nl,
             GSocketFamily           family,
             const guint8           *dest,
             gsize                   dest_len)
{
  GInetAddressMask *network = create_inet_address_mask (family, dest, dest_len);
  g_return_if_fail (network != NULL);

  if (nl->priv->dump_networks)
    g_ptr_array_add (nl->priv->dump_networks, g_object_ref (network));
  else
    g_network_monitor_base_add_network (G_NETWORK_MONITOR_BASE (nl), network);

  g_object_unref (network);
}

static void
remove_network (GNetworkMonitorNetlink *nl,
                GSocketFamily           family,
                const guint8           *dest,
                gsize                   dest_len)
{
  GInetAddressMask *network = create_inet_address_mask (family, dest, dest_len);
  g_return_if_fail (network != NULL);

  if (nl->priv->dump_networks)
    {
      GInetAddressMask **dump_networks = (GInetAddressMask **)nl->priv->dump_networks->pdata;
      int i;

      for (i = 0; i < nl->priv->dump_networks->len; i++)
        {
          if (g_inet_address_mask_equal (network, dump_networks[i]))
            g_ptr_array_remove_index_fast (nl->priv->dump_networks, i--);
        }
    }
  else
    {
      g_network_monitor_base_remove_network (G_NETWORK_MONITOR_BASE (nl), network);
    }

  g_object_unref (network);
}

static void
finish_dump (GNetworkMonitorNetlink *nl)
{
  g_network_monitor_base_set_networks (G_NETWORK_MONITOR_BASE (nl),
                                       (GInetAddressMask **)nl->priv->dump_networks->pdata,
                                       nl->priv->dump_networks->len);
  g_ptr_array_free (nl->priv->dump_networks, TRUE);
  nl->priv->dump_networks = NULL;
}

static gboolean
read_netlink_messages (GSocket      *socket,
                       GIOCondition  condition,
                       gpointer      user_data)
{
  GNetworkMonitorNetlink *nl = user_data;
  GInputVector iv;
  gssize len;
  gint flags;
  GError *error = NULL;
  GSocketAddress *addr = NULL;
  struct nlmsghdr *msg;
  struct rtmsg *rtmsg;
  struct rtattr *attr;
  struct sockaddr_nl source_sockaddr;
  gsize attrlen;
  guint8 *dest, *gateway, *oif;
  gboolean retval = TRUE;

  iv.buffer = NULL;
  iv.size = 0;

  flags = MSG_PEEK | MSG_TRUNC;
  len = g_socket_receive_message (nl->priv->sock, NULL, &iv, 1,
                                  NULL, NULL, &flags, NULL, &error);
  if (len < 0)
    {
      g_warning ("Error on netlink socket: %s", error->message);
      g_clear_error (&error);
      retval = FALSE;
      goto done;
    }

  iv.buffer = g_malloc (len);
  iv.size = len;
  len = g_socket_receive_message (nl->priv->sock, &addr, &iv, 1,
                                  NULL, NULL, NULL, NULL, &error);
  if (len < 0)
    {
      g_warning ("Error on netlink socket: %s", error->message);
      g_clear_error (&error);
      retval = FALSE;
      goto done;
    }

  if (!g_socket_address_to_native (addr, &source_sockaddr, sizeof (source_sockaddr), &error))
    {
      g_warning ("Error on netlink socket: %s", error->message);
      g_clear_error (&error);
      retval = FALSE;
      goto done;
    }

  /* If the sender port id is 0 (not fakeable) then the message is from the kernel */
  if (source_sockaddr.nl_pid != 0)
    goto done;

  msg = (struct nlmsghdr *) iv.buffer;
  for (; len > 0; msg = NLMSG_NEXT (msg, len))
    {
      if (!NLMSG_OK (msg, (size_t) len))
        {
          g_warning ("netlink message was truncated; shouldn't happen...");
          retval = FALSE;
          goto done;
        }

      switch (msg->nlmsg_type)
        {
        case RTM_NEWROUTE:
        case RTM_DELROUTE:
          rtmsg = NLMSG_DATA (msg);

          if (rtmsg->rtm_family != AF_INET && rtmsg->rtm_family != AF_INET6)
            continue;
          if (rtmsg->rtm_type == RTN_UNREACHABLE)
            continue;

          attrlen = NLMSG_PAYLOAD (msg, sizeof (struct rtmsg));
          attr = RTM_RTA (rtmsg);
          dest = gateway = oif = NULL;
          while (RTA_OK (attr, attrlen))
            {
              if (attr->rta_type == RTA_DST)
                dest = RTA_DATA (attr);
              else if (attr->rta_type == RTA_GATEWAY)
                gateway = RTA_DATA (attr);
              else if (attr->rta_type == RTA_OIF)
                oif = RTA_DATA (attr);
              attr = RTA_NEXT (attr, attrlen);
            }

          if (dest || gateway || oif)
            {
              /* Unless we're processing the results of a dump, ignore
               * IPv6 link-local multicast routes, which are added and
               * removed all the time for some reason.
               */
#define UNALIGNED_IN6_IS_ADDR_MC_LINKLOCAL(a)           \
              ((a[0] == 0xff) && ((a[1] & 0xf) == 0x2))

              if (!nl->priv->dump_networks &&
                  rtmsg->rtm_family == AF_INET6 &&
                  rtmsg->rtm_dst_len != 0 &&
                  UNALIGNED_IN6_IS_ADDR_MC_LINKLOCAL (dest))
                continue;

              if (msg->nlmsg_type == RTM_NEWROUTE)
                add_network (nl, rtmsg->rtm_family, dest, rtmsg->rtm_dst_len);
              else
                remove_network (nl, rtmsg->rtm_family, dest, rtmsg->rtm_dst_len);
              queue_request_dump (nl);
            }
          break;

        case NLMSG_DONE:
          finish_dump (nl);
          goto done;

        case NLMSG_ERROR:
          {
            struct nlmsgerr *e = NLMSG_DATA (msg);

            g_warning ("netlink error: %s", g_strerror (-e->error));
          }
          retval = FALSE;
          goto done;

        default:
          g_warning ("unexpected netlink message %d", msg->nlmsg_type);
          retval = FALSE;
          goto done;
        }
    }

 done:
  g_free (iv.buffer);
  g_clear_object (&addr);

  if (!retval && nl->priv->dump_networks)
    finish_dump (nl);
  return retval;
}

static void
g_network_monitor_netlink_finalize (GObject *object)
{
  GNetworkMonitorNetlink *nl = G_NETWORK_MONITOR_NETLINK (object);

  if (nl->priv->sock)
    {
      g_socket_close (nl->priv->sock, NULL);
      g_object_unref (nl->priv->sock);
    }

  if (nl->priv->source)
    {
      g_source_destroy (nl->priv->source);
      g_source_unref (nl->priv->source);
    }

  if (nl->priv->dump_source)
    {
      g_source_destroy (nl->priv->dump_source);
      g_source_unref (nl->priv->dump_source);
    }

  g_clear_pointer (&nl->priv->context, g_main_context_unref);
  g_clear_pointer (&nl->priv->dump_networks, g_ptr_array_unref);

  G_OBJECT_CLASS (g_network_monitor_netlink_parent_class)->finalize (object);
}

static void
g_network_monitor_netlink_class_init (GNetworkMonitorNetlinkClass *nl_class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (nl_class);

  gobject_class->finalize = g_network_monitor_netlink_finalize;
}

static void
g_network_monitor_netlink_iface_init (GNetworkMonitorInterface *monitor_iface)
{
}

static void
g_network_monitor_netlink_initable_iface_init (GInitableIface *iface)
{
  initable_parent_iface = g_type_interface_peek_parent (iface);

  iface->init = g_network_monitor_netlink_initable_init;
}
