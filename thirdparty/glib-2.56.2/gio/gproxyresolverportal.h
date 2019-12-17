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

#ifndef __G_PROXY_RESOLVER_PORTAL_H__
#define __G_PROXY_RESOLVER_PORTAL_H__

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define G_TYPE_PROXY_RESOLVER_PORTAL         (g_proxy_resolver_portal_get_type ())
#define G_PROXY_RESOLVER_PORTAL(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), G_TYPE_PROXY_RESOLVER_PORTAL, GProxyResolverPortal))
#define G_PROXY_RESOLVER_PORTAL_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), G_TYPE_PROXY_RESOLVER_PORTAL, GProxyResolverPortalClass))
#define G_IS_PROXY_RESOLVER_PORTAL(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), G_TYPE_PROXY_RESOLVER_PORTAL))
#define G_IS_PROXY_RESOLVER_PORTAL_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), G_TYPE_PROXY_RESOLVER_PORTAL))
#define G_PROXY_RESOLVER_PORTAL_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), G_TYPE_PROXY_RESOLVER_PORTAL, GProxyResolverPortalClass))

typedef struct _GProxyResolverPortal       GProxyResolverPortal;
typedef struct _GProxyResolverPortalClass  GProxyResolverPortalClass;

struct _GProxyResolverPortalClass {
  GObjectClass parent_class;
};

GType g_proxy_resolver_portal_get_type (void);

G_END_DECLS

#endif /* __G_PROXY_RESOLVER_PORTAL_H__ */

