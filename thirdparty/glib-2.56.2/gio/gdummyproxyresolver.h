/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright (C) 2010 Collabora, Ltd.
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
 *
 * Author: Nicolas Dufresne <nicolas.dufresne@collabora.co.uk>
 */

#ifndef __G_DUMMY_PROXY_RESOLVER_H__
#define __G_DUMMY_PROXY_RESOLVER_H__

#if !defined (__GIO_GIO_H_INSIDE__) && !defined (GIO_COMPILATION)
#error "Only <gio/gio.h> can be included directly."
#endif

#include <gio/giotypes.h>

G_BEGIN_DECLS

#define G_TYPE_DUMMY_PROXY_RESOLVER         (_g_dummy_proxy_resolver_get_type ())
#define G_DUMMY_PROXY_RESOLVER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), G_TYPE_DUMMY_PROXY_RESOLVER, GDummyProxyResolver))
#define G_DUMMY_PROXY_RESOLVER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), G_TYPE_DUMMY_PROXY_RESOLVER, GDummyProxyResolverClass))
#define G_IS_DUMMY_PROXY_RESOLVER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), G_TYPE_DUMMY_PROXY_RESOLVER))
#define G_IS_DUMMY_PROXY_RESOLVER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), G_TYPE_DUMMY_PROXY_RESOLVER))
#define G_DUMMY_PROXY_RESOLVER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), G_TYPE_DUMMY_PROXY_RESOLVER, GDummyProxyResolverClass))

typedef struct _GDummyProxyResolver       GDummyProxyResolver;
typedef struct _GDummyProxyResolverClass  GDummyProxyResolverClass;


struct _GDummyProxyResolverClass {
  GObjectClass parent_class;
};

GType		_g_dummy_proxy_resolver_get_type       (void);


G_END_DECLS

#endif /* __G_DUMMY_PROXY_RESOLVER_H__ */
