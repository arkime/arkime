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
 * Author:  Nicolas Dufresne <nicolas.dufresne@collabora.co.uk>
 */

#ifndef __G_HTTP_PROXY_H__
#define __G_HTTP_PROXY_H__

#include <gio/giotypes.h>

G_BEGIN_DECLS

#define G_TYPE_HTTP_PROXY         (_g_http_proxy_get_type ())
#define G_HTTP_PROXY(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), G_TYPE_HTTP_PROXY, GHttpProxy))
#define G_HTTP_PROXY_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), G_TYPE_HTTP_PROXY, GHttpProxyClass))
#define G_IS_HTTP_PROXY(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), G_TYPE_HTTP_PROXY))
#define G_IS_HTTP_PROXY_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), G_TYPE_HTTP_PROXY))
#define G_HTTP_PROXY_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), G_TYPE_HTTP_PROXY, GHttpProxyClass))

typedef struct _GHttpProxy        GHttpProxy;
typedef struct _GHttpProxyClass   GHttpProxyClass;

GType _g_http_proxy_get_type (void);

#define G_TYPE_HTTPS_PROXY         (_g_https_proxy_get_type ())
#define G_HTTPS_PROXY(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), G_TYPE_HTTPS_PROXY, GHttpsProxy))
#define G_HTTPS_PROXY_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), G_TYPE_HTTPS_PROXY, GHttpsProxyClass))
#define G_IS_HTTPS_PROXY(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), G_TYPE_HTTPS_PROXY))
#define G_IS_HTTPS_PROXY_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), G_TYPE_HTTPS_PROXY))
#define G_HTTPS_PROXY_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), G_TYPE_HTTPS_PROXY, GHttpsProxyClass))

typedef struct _GHttpsProxy        GHttpsProxy;
typedef struct _GHttpsProxyClass   GHttpsProxyClass;

GType _g_https_proxy_get_type (void);

G_END_DECLS

#endif /* __G_HTTP_PROXY_H__ */
