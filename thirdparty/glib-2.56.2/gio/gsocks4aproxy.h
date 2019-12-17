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

#ifndef __G_SOCKS4A_PROXY_H__
#define __G_SOCKS4A_PROXY_H__

#include <gio/giotypes.h>

G_BEGIN_DECLS

#define G_TYPE_SOCKS4A_PROXY         (_g_socks4a_proxy_get_type ())
#define G_SOCKS4A_PROXY(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), G_TYPE_SOCKS4A_PROXY, GSocks4aProxy))
#define G_SOCKS4A_PROXY_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), G_TYPE_SOCKS4A_PROXY, GSocks4aProxyClass))
#define G_IS_SOCKS4A_PROXY(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), G_TYPE_SOCKS4A_PROXY))
#define G_IS_SOCKS4A_PROXY_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), G_TYPE_SOCKS4A_PROXY))
#define G_SOCKS4A_PROXY_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), G_TYPE_SOCKS4A_PROXY, GSocks4aProxyClass))

typedef struct _GSocks4aProxy	    GSocks4aProxy;
typedef struct _GSocks4aProxyClass  GSocks4aProxyClass;

struct _GSocks4aProxy
{
  GObject parent;
  gboolean supports_hostname;
};

struct _GSocks4aProxyClass
{
  GObjectClass parent_class;
};

GType _g_socks4a_proxy_get_type (void);

G_END_DECLS

#endif /* __SOCKS5_PROXY_H__ */
