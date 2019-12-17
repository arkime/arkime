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
 * Public License along with this library; if not, see
 * <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "xdp-dbus.h"
#include "giomodule-priv.h"
#include "gportalsupport.h"
#include "gproxyresolverportal.h"

struct _GProxyResolverPortal {
  GObject parent_instance;

  GXdpProxyResolver *resolver;
  gboolean network_available;
};

static void g_proxy_resolver_portal_iface_init (GProxyResolverInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GProxyResolverPortal, g_proxy_resolver_portal, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_PROXY_RESOLVER,
                                                g_proxy_resolver_portal_iface_init)
                         _g_io_modules_ensure_extension_points_registered ();
                         g_io_extension_point_implement (G_PROXY_RESOLVER_EXTENSION_POINT_NAME,
                                                         g_define_type_id,
                                                         "portal",
                                                         90))

static gboolean
ensure_resolver_proxy (GProxyResolverPortal *resolver)
{
  if (resolver->resolver)
    return TRUE;

  if (!glib_should_use_portal ())
    return FALSE;

  resolver->resolver = gxdp_proxy_resolver_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                                                   G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START,
                                                                   "org.freedesktop.portal.Desktop",
                                                                   "/org/freedesktop/portal/desktop",
                                                                   NULL,
                                                                   NULL);

  resolver->network_available = glib_network_available_in_sandbox ();

  return resolver->resolver != NULL;
}

static void
g_proxy_resolver_portal_init (GProxyResolverPortal *resolver)
{
}

static gboolean
g_proxy_resolver_portal_is_supported (GProxyResolver *object)
{
  GProxyResolverPortal *resolver = G_PROXY_RESOLVER_PORTAL (object);
  char *name_owner;
  gboolean has_portal;

  if (!ensure_resolver_proxy (resolver))
    return FALSE;

  name_owner = g_dbus_proxy_get_name_owner (G_DBUS_PROXY (resolver->resolver));
  has_portal = name_owner != NULL;
  g_free (name_owner);

  return has_portal;
}

static const char *no_proxy[2] = { "direct://", NULL };

static gchar **
g_proxy_resolver_portal_lookup (GProxyResolver *proxy_resolver,
                                const gchar     *uri,
                                GCancellable    *cancellable,
                                GError         **error)
{
  GProxyResolverPortal *resolver = G_PROXY_RESOLVER_PORTAL (proxy_resolver);
  char **proxy = NULL;

  ensure_resolver_proxy (resolver);
  g_assert (resolver->resolver);

  if (!gxdp_proxy_resolver_call_lookup_sync (resolver->resolver,
                                             uri,
                                             &proxy,
                                             cancellable,
                                             error))
    return NULL;

  if (!resolver->network_available)
    {
      g_strfreev (proxy);
      proxy = g_strdupv ((gchar **)no_proxy);
    }

  return proxy;
}

static void
lookup_done (GObject      *source,
             GAsyncResult *result,
             gpointer      data)
{
  GTask *task = data;
  GError *error = NULL;
  gchar **proxies = NULL;

  if (!gxdp_proxy_resolver_call_lookup_finish (GXDP_PROXY_RESOLVER (source),
                                               &proxies,
                                               result,
                                               &error))
    g_task_return_error (task, error);
  else
    g_task_return_pointer (task, proxies, NULL);

  g_object_unref (task);
}

static void
g_proxy_resolver_portal_lookup_async (GProxyResolver      *proxy_resolver,
                                      const gchar         *uri,
                                      GCancellable        *cancellable,
                                      GAsyncReadyCallback  callback,
                                      gpointer             user_data)
{
  GProxyResolverPortal *resolver = G_PROXY_RESOLVER_PORTAL (proxy_resolver);
  GTask *task;

  ensure_resolver_proxy (resolver);
  g_assert (resolver->resolver);

  task = g_task_new (proxy_resolver, cancellable, callback, user_data);
  gxdp_proxy_resolver_call_lookup (resolver->resolver,
                                   uri,
                                   cancellable,
                                   lookup_done,
                                   g_object_ref (task));
  g_object_unref (task);
}

static gchar **
g_proxy_resolver_portal_lookup_finish (GProxyResolver  *proxy_resolver,
                                       GAsyncResult    *result,
                                       GError         **error)
{
  GProxyResolverPortal *resolver = G_PROXY_RESOLVER_PORTAL (proxy_resolver);
  GTask *task = G_TASK (result);
  char **proxies;

  proxies = g_task_propagate_pointer (task, error);
  if (proxies == NULL)
    return NULL;

  if (!resolver->network_available)
    {
      g_strfreev (proxies);
      proxies = g_strdupv ((gchar **)no_proxy);
    }

  return proxies;
}

static void
g_proxy_resolver_portal_finalize (GObject *object)
{
  GProxyResolverPortal *resolver = G_PROXY_RESOLVER_PORTAL (object);

  g_clear_object (&resolver->resolver);

  G_OBJECT_CLASS (g_proxy_resolver_portal_parent_class)->finalize (object);
}

static void
g_proxy_resolver_portal_class_init (GProxyResolverPortalClass *resolver_class)
{
  GObjectClass *object_class;
 
  object_class = G_OBJECT_CLASS (resolver_class);
  object_class->finalize = g_proxy_resolver_portal_finalize;
}

static void
g_proxy_resolver_portal_iface_init (GProxyResolverInterface *iface)
{
  iface->is_supported = g_proxy_resolver_portal_is_supported;
  iface->lookup = g_proxy_resolver_portal_lookup;
  iface->lookup_async = g_proxy_resolver_portal_lookup_async;
  iface->lookup_finish = g_proxy_resolver_portal_lookup_finish;
}
