/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright (C) 2008 Red Hat, Inc.
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
#include <glib.h>
#include "glibintl.h"

#include "gnetworkservice.h"

#include "gcancellable.h"
#include "ginetaddress.h"
#include "ginetsocketaddress.h"
#include "gioerror.h"
#include "gnetworkaddress.h"
#include "gnetworkingprivate.h"
#include "gresolver.h"
#include "gtask.h"
#include "gsocketaddressenumerator.h"
#include "gsocketconnectable.h"
#include "gsrvtarget.h"

#include <stdlib.h>
#include <string.h>


/**
 * SECTION:gnetworkservice
 * @short_description: A GSocketConnectable for resolving SRV records
 * @include: gio/gio.h
 *
 * Like #GNetworkAddress does with hostnames, #GNetworkService
 * provides an easy way to resolve a SRV record, and then attempt to
 * connect to one of the hosts that implements that service, handling
 * service priority/weighting, multiple IP addresses, and multiple
 * address families.
 *
 * See #GSrvTarget for more information about SRV records, and see
 * #GSocketConnectable for and example of using the connectable
 * interface.
 */

/**
 * GNetworkService:
 *
 * A #GSocketConnectable for resolving a SRV record and connecting to
 * that service.
 */

struct _GNetworkServicePrivate
{
  gchar *service, *protocol, *domain, *scheme;
  GList *targets;
};

enum {
  PROP_0,
  PROP_SERVICE,
  PROP_PROTOCOL,
  PROP_DOMAIN,
  PROP_SCHEME
};

static void g_network_service_set_property (GObject      *object,
                                            guint         prop_id,
                                            const GValue *value,
                                            GParamSpec   *pspec);
static void g_network_service_get_property (GObject      *object,
                                            guint         prop_id,
                                            GValue       *value,
                                            GParamSpec   *pspec);

static void                      g_network_service_connectable_iface_init       (GSocketConnectableIface *iface);
static GSocketAddressEnumerator *g_network_service_connectable_enumerate        (GSocketConnectable      *connectable);
static GSocketAddressEnumerator *g_network_service_connectable_proxy_enumerate  (GSocketConnectable      *connectable);
static gchar                    *g_network_service_connectable_to_string        (GSocketConnectable      *connectable);

G_DEFINE_TYPE_WITH_CODE (GNetworkService, g_network_service, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (GNetworkService)
                         G_IMPLEMENT_INTERFACE (G_TYPE_SOCKET_CONNECTABLE,
                                                g_network_service_connectable_iface_init))

static void
g_network_service_finalize (GObject *object)
{
  GNetworkService *srv = G_NETWORK_SERVICE (object);

  g_free (srv->priv->service);
  g_free (srv->priv->protocol);
  g_free (srv->priv->domain);
  g_free (srv->priv->scheme);

  if (srv->priv->targets)
    g_resolver_free_targets (srv->priv->targets);

  G_OBJECT_CLASS (g_network_service_parent_class)->finalize (object);
}

static void
g_network_service_class_init (GNetworkServiceClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = g_network_service_set_property;
  gobject_class->get_property = g_network_service_get_property;
  gobject_class->finalize = g_network_service_finalize;

  g_object_class_install_property (gobject_class, PROP_SERVICE,
                                   g_param_spec_string ("service",
                                                        P_("Service"),
                                                        P_("Service name, eg \"ldap\""),
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_PROTOCOL,
                                   g_param_spec_string ("protocol",
                                                        P_("Protocol"),
                                                        P_("Network protocol, eg \"tcp\""),
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_DOMAIN,
                                   g_param_spec_string ("domain",
                                                        P_("Domain"),
                                                        P_("Network domain, eg, \"example.com\""),
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_DOMAIN,
                                   g_param_spec_string ("scheme",
                                                        P_("Scheme"),
                                                        P_("Network scheme (default is to use service)"),
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));

}

static void
g_network_service_connectable_iface_init (GSocketConnectableIface *connectable_iface)
{
  connectable_iface->enumerate = g_network_service_connectable_enumerate;
  connectable_iface->proxy_enumerate = g_network_service_connectable_proxy_enumerate;
  connectable_iface->to_string = g_network_service_connectable_to_string;
}

static void
g_network_service_init (GNetworkService *srv)
{
  srv->priv = g_network_service_get_instance_private (srv);
}

static void
g_network_service_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GNetworkService *srv = G_NETWORK_SERVICE (object);

  switch (prop_id)
    {
    case PROP_SERVICE:
      srv->priv->service = g_value_dup_string (value);
      break;

    case PROP_PROTOCOL:
      srv->priv->protocol = g_value_dup_string (value);
      break;

    case PROP_DOMAIN:
      srv->priv->domain = g_value_dup_string (value);
      break;

    case PROP_SCHEME:
      g_network_service_set_scheme (srv, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
g_network_service_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  GNetworkService *srv = G_NETWORK_SERVICE (object);

  switch (prop_id)
    {
    case PROP_SERVICE:
      g_value_set_string (value, g_network_service_get_service (srv));
      break;

    case PROP_PROTOCOL:
      g_value_set_string (value, g_network_service_get_protocol (srv));
      break;

    case PROP_DOMAIN:
      g_value_set_string (value, g_network_service_get_domain (srv));
      break;

    case PROP_SCHEME:
      g_value_set_string (value, g_network_service_get_scheme (srv));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/**
 * g_network_service_new:
 * @service: the service type to look up (eg, "ldap")
 * @protocol: the networking protocol to use for @service (eg, "tcp")
 * @domain: the DNS domain to look up the service in
 *
 * Creates a new #GNetworkService representing the given @service,
 * @protocol, and @domain. This will initially be unresolved; use the
 * #GSocketConnectable interface to resolve it.
 *
 * Returns: (transfer full) (type GNetworkService): a new #GNetworkService
 *
 * Since: 2.22
 */
GSocketConnectable *
g_network_service_new (const gchar *service,
                       const gchar *protocol,
                       const gchar *domain)
{
  return g_object_new (G_TYPE_NETWORK_SERVICE,
                       "service", service,
                       "protocol", protocol,
                       "domain", domain,
                       NULL);
}

/**
 * g_network_service_get_service:
 * @srv: a #GNetworkService
 *
 * Gets @srv's service name (eg, "ldap").
 *
 * Returns: @srv's service name
 *
 * Since: 2.22
 */
const gchar *
g_network_service_get_service (GNetworkService *srv)
{
  g_return_val_if_fail (G_IS_NETWORK_SERVICE (srv), NULL);

  return srv->priv->service;
}

/**
 * g_network_service_get_protocol:
 * @srv: a #GNetworkService
 *
 * Gets @srv's protocol name (eg, "tcp").
 *
 * Returns: @srv's protocol name
 *
 * Since: 2.22
 */
const gchar *
g_network_service_get_protocol (GNetworkService *srv)
{
  g_return_val_if_fail (G_IS_NETWORK_SERVICE (srv), NULL);

  return srv->priv->protocol;
}

/**
 * g_network_service_get_domain:
 * @srv: a #GNetworkService
 *
 * Gets the domain that @srv serves. This might be either UTF-8 or
 * ASCII-encoded, depending on what @srv was created with.
 *
 * Returns: @srv's domain name
 *
 * Since: 2.22
 */
const gchar *
g_network_service_get_domain (GNetworkService *srv)
{
  g_return_val_if_fail (G_IS_NETWORK_SERVICE (srv), NULL);

  return srv->priv->domain;
}

/**
 * g_network_service_get_scheme:
 * @srv: a #GNetworkService
 *
 * Get's the URI scheme used to resolve proxies. By default, the service name
 * is used as scheme.
 *
 * Returns: @srv's scheme name
 *
 * Since: 2.26
 */
const gchar *
g_network_service_get_scheme (GNetworkService *srv)
{
  g_return_val_if_fail (G_IS_NETWORK_SERVICE (srv), NULL);

  if (srv->priv->scheme)
    return srv->priv->scheme;
  else
    return srv->priv->service;
}

/**
 * g_network_service_set_scheme:
 * @srv: a #GNetworkService
 * @scheme: a URI scheme
 *
 * Set's the URI scheme used to resolve proxies. By default, the service name
 * is used as scheme.
 *
 * Since: 2.26
 */
void
g_network_service_set_scheme (GNetworkService *srv,
                              const gchar     *scheme)
{
  g_return_if_fail (G_IS_NETWORK_SERVICE (srv));

  g_free (srv->priv->scheme);
  srv->priv->scheme = g_strdup (scheme);

  g_object_notify (G_OBJECT (srv), "scheme");
}

static GList *
g_network_service_fallback_targets (GNetworkService *srv)
{
  GSrvTarget *target;
  struct servent *entry;
  guint16 port;

  entry = getservbyname (srv->priv->service, "tcp");
  port = entry ? g_ntohs (entry->s_port) : 0;
#ifdef HAVE_ENDSERVENT
  endservent ();
#endif

  if (entry == NULL)
      return NULL;

  target = g_srv_target_new (srv->priv->domain, port, 0, 0);
  return g_list_append (NULL, target);
}

#define G_TYPE_NETWORK_SERVICE_ADDRESS_ENUMERATOR (_g_network_service_address_enumerator_get_type ())
#define G_NETWORK_SERVICE_ADDRESS_ENUMERATOR(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), G_TYPE_NETWORK_SERVICE_ADDRESS_ENUMERATOR, GNetworkServiceAddressEnumerator))

typedef struct {
  GSocketAddressEnumerator parent_instance;

  GResolver *resolver;
  GNetworkService *srv;
  GSocketAddressEnumerator *addr_enum;
  GList *t;
  gboolean use_proxy;

  GError *error;

} GNetworkServiceAddressEnumerator;

typedef struct {
  GSocketAddressEnumeratorClass parent_class;

} GNetworkServiceAddressEnumeratorClass;

static GType _g_network_service_address_enumerator_get_type (void);
G_DEFINE_TYPE (GNetworkServiceAddressEnumerator, _g_network_service_address_enumerator, G_TYPE_SOCKET_ADDRESS_ENUMERATOR)

static GSocketAddress *
g_network_service_address_enumerator_next (GSocketAddressEnumerator  *enumerator,
                                           GCancellable              *cancellable,
                                           GError                   **error)
{
  GNetworkServiceAddressEnumerator *srv_enum =
    G_NETWORK_SERVICE_ADDRESS_ENUMERATOR (enumerator);
  GSocketAddress *ret = NULL;

  /* If we haven't yet resolved srv, do that */
  if (!srv_enum->srv->priv->targets)
    {
      GList *targets;
      GError *my_error = NULL;

      targets = g_resolver_lookup_service (srv_enum->resolver,
                                           srv_enum->srv->priv->service,
                                           srv_enum->srv->priv->protocol,
                                           srv_enum->srv->priv->domain,
                                           cancellable, &my_error);
      if (!targets && g_error_matches (my_error, G_RESOLVER_ERROR,
                                       G_RESOLVER_ERROR_NOT_FOUND))
        {
          targets = g_network_service_fallback_targets (srv_enum->srv);
          if (targets)
            g_clear_error (&my_error);
        }

      if (my_error)
        {
          g_propagate_error (error, my_error);
          return NULL;
        }

      srv_enum->srv->priv->targets = targets;
      srv_enum->t = srv_enum->srv->priv->targets;
    }

  /* Delegate to GNetworkAddress */
  do
    {
      if (srv_enum->addr_enum == NULL && srv_enum->t)
        {
          GError *error = NULL;
          gchar *uri;
          gchar *hostname;
          GSocketConnectable *addr;
          GSrvTarget *target = srv_enum->t->data;

          srv_enum->t = g_list_next (srv_enum->t);

          hostname = g_hostname_to_ascii (g_srv_target_get_hostname (target));

          if (hostname == NULL)
            {
              if (srv_enum->error == NULL)
                srv_enum->error =
                  g_error_new (G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                               "Received invalid hostname '%s' from GSrvTarget",
                               g_srv_target_get_hostname (target));
              continue;
            }

          uri = _g_uri_from_authority (g_network_service_get_scheme (srv_enum->srv),
                                       hostname,
                                       g_srv_target_get_port (target),
                                       NULL);
          g_free (hostname);

          addr = g_network_address_parse_uri (uri,
                                              g_srv_target_get_port (target),
                                              &error);
          g_free (uri);

          if (addr == NULL)
            {
              if (srv_enum->error == NULL)
                srv_enum->error = error;
              else
                g_error_free (error);
              continue;
            }

          if (srv_enum->use_proxy)
            srv_enum->addr_enum = g_socket_connectable_proxy_enumerate (addr);
          else
            srv_enum->addr_enum = g_socket_connectable_enumerate (addr);
          g_object_unref (addr);
        }

      if (srv_enum->addr_enum)
        {
          GError *error = NULL;

          ret = g_socket_address_enumerator_next (srv_enum->addr_enum,
                                                  cancellable,
                                                  &error);

          if (error)
            {
              if (srv_enum->error == NULL)
                srv_enum->error = error;
              else
                g_error_free (error);
            }

          if (!ret)
            {
              g_object_unref (srv_enum->addr_enum);
              srv_enum->addr_enum = NULL;
            }
        }
    }
  while (srv_enum->addr_enum == NULL && srv_enum->t);

  if (ret == NULL && srv_enum->error)
    {
      g_propagate_error (error, srv_enum->error);
      srv_enum->error = NULL;
    }

  return ret;
}

static void next_async_resolved_targets   (GObject      *source_object,
                                           GAsyncResult *result,
                                           gpointer      user_data);
static void next_async_have_targets       (GTask        *srv_enum);
static void next_async_have_address       (GObject      *source_object,
                                           GAsyncResult *result,
                                           gpointer      user_data);

static void
g_network_service_address_enumerator_next_async (GSocketAddressEnumerator  *enumerator,
                                                 GCancellable              *cancellable,
                                                 GAsyncReadyCallback        callback,
                                                 gpointer                   user_data)
{
  GNetworkServiceAddressEnumerator *srv_enum =
    G_NETWORK_SERVICE_ADDRESS_ENUMERATOR (enumerator);
  GTask *task;

  task = g_task_new (enumerator, cancellable, callback, user_data);
  g_task_set_source_tag (task, g_network_service_address_enumerator_next_async);

  /* If we haven't yet resolved srv, do that */
  if (!srv_enum->srv->priv->targets)
    {
      g_resolver_lookup_service_async (srv_enum->resolver,
                                       srv_enum->srv->priv->service,
                                       srv_enum->srv->priv->protocol,
                                       srv_enum->srv->priv->domain,
                                       cancellable,
                                       next_async_resolved_targets,
                                       task);
    }
  else
    next_async_have_targets (task);
}

static void
next_async_resolved_targets (GObject      *source_object,
                             GAsyncResult *result,
                             gpointer      user_data)
{
  GTask *task = user_data;
  GNetworkServiceAddressEnumerator *srv_enum = g_task_get_source_object (task);
  GError *error = NULL;
  GList *targets;

  targets = g_resolver_lookup_service_finish (srv_enum->resolver,
                                              result, &error);

  if (!targets && g_error_matches (error, G_RESOLVER_ERROR,
                                   G_RESOLVER_ERROR_NOT_FOUND))
    {
      targets = g_network_service_fallback_targets (srv_enum->srv);
      if (targets)
        g_clear_error (&error);
    }

  if (error)
    {
      g_task_return_error (task, error);
      g_object_unref (task);
    }
  else
    {
      srv_enum->t = srv_enum->srv->priv->targets = targets;
      next_async_have_targets (task);
    }
}

static void
next_async_have_targets (GTask *task)
{
  GNetworkServiceAddressEnumerator *srv_enum = g_task_get_source_object (task);

  /* Delegate to GNetworkAddress */
  if (srv_enum->addr_enum == NULL && srv_enum->t)
    {
      GSocketConnectable *addr;
      GSrvTarget *target = srv_enum->t->data;

      srv_enum->t = g_list_next (srv_enum->t);
      addr = g_network_address_new (g_srv_target_get_hostname (target),
                                    (guint16) g_srv_target_get_port (target));

      if (srv_enum->use_proxy)
        srv_enum->addr_enum = g_socket_connectable_proxy_enumerate (addr);
      else
        srv_enum->addr_enum = g_socket_connectable_enumerate (addr);

      g_object_unref (addr);
    }

  if (srv_enum->addr_enum)
    {
      g_socket_address_enumerator_next_async (srv_enum->addr_enum,
                                              g_task_get_cancellable (task),
                                              next_async_have_address,
                                              task);
    }
  else
    {
      if (srv_enum->error)
        {
          g_task_return_error (task, srv_enum->error);
          srv_enum->error = NULL;
        }
      else
        g_task_return_pointer (task, NULL, NULL);

      g_object_unref (task);
    }
}

static void
next_async_have_address (GObject      *source_object,
                         GAsyncResult *result,
                         gpointer      user_data)
{
  GTask *task = user_data;
  GNetworkServiceAddressEnumerator *srv_enum = g_task_get_source_object (task);
  GSocketAddress *address;
  GError *error = NULL;
  
  address = g_socket_address_enumerator_next_finish (srv_enum->addr_enum,
                                                     result,
                                                     &error);

  if (error)
    {
      if (srv_enum->error == NULL)
        srv_enum->error = error;
      else
        g_error_free (error);
    }

  if (!address)
    {
      g_object_unref (srv_enum->addr_enum);
      srv_enum->addr_enum = NULL;

      next_async_have_targets (task);
    }
  else
    {
      g_task_return_pointer (task, address, g_object_unref);
      g_object_unref (task);
    }
}

static GSocketAddress *
g_network_service_address_enumerator_next_finish (GSocketAddressEnumerator  *enumerator,
                                                  GAsyncResult              *result,
                                                  GError                   **error)
{
  return g_task_propagate_pointer (G_TASK (result), error);
}

static void
_g_network_service_address_enumerator_init (GNetworkServiceAddressEnumerator *enumerator)
{
}

static void
g_network_service_address_enumerator_finalize (GObject *object)
{
  GNetworkServiceAddressEnumerator *srv_enum =
    G_NETWORK_SERVICE_ADDRESS_ENUMERATOR (object);

  if (srv_enum->srv)
    g_object_unref (srv_enum->srv);

  if (srv_enum->addr_enum)
    g_object_unref (srv_enum->addr_enum);

  if (srv_enum->resolver)
    g_object_unref (srv_enum->resolver);

  if (srv_enum->error)
    g_error_free (srv_enum->error);

  G_OBJECT_CLASS (_g_network_service_address_enumerator_parent_class)->finalize (object);
}

static void
_g_network_service_address_enumerator_class_init (GNetworkServiceAddressEnumeratorClass *srvenum_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (srvenum_class);
  GSocketAddressEnumeratorClass *enumerator_class =
    G_SOCKET_ADDRESS_ENUMERATOR_CLASS (srvenum_class);

  enumerator_class->next        = g_network_service_address_enumerator_next;
  enumerator_class->next_async  = g_network_service_address_enumerator_next_async;
  enumerator_class->next_finish = g_network_service_address_enumerator_next_finish;

  object_class->finalize = g_network_service_address_enumerator_finalize;
}

static GSocketAddressEnumerator *
g_network_service_connectable_enumerate (GSocketConnectable *connectable)
{
  GNetworkServiceAddressEnumerator *srv_enum;

  srv_enum = g_object_new (G_TYPE_NETWORK_SERVICE_ADDRESS_ENUMERATOR, NULL);
  srv_enum->srv = g_object_ref (G_NETWORK_SERVICE (connectable));
  srv_enum->resolver = g_resolver_get_default ();
  srv_enum->use_proxy = FALSE;

  return G_SOCKET_ADDRESS_ENUMERATOR (srv_enum);
}

static GSocketAddressEnumerator *
g_network_service_connectable_proxy_enumerate (GSocketConnectable *connectable)
{
  GSocketAddressEnumerator *addr_enum;
  GNetworkServiceAddressEnumerator *srv_enum;

  addr_enum = g_network_service_connectable_enumerate (connectable);
  srv_enum = G_NETWORK_SERVICE_ADDRESS_ENUMERATOR (addr_enum);
  srv_enum->use_proxy = TRUE;

  return addr_enum;
}

static gchar *
g_network_service_connectable_to_string (GSocketConnectable *connectable)
{
  GNetworkService *service;

  service = G_NETWORK_SERVICE (connectable);

  return g_strdup_printf ("(%s, %s, %s, %s)", service->priv->service,
                          service->priv->protocol, service->priv->domain,
                          service->priv->scheme);
}
