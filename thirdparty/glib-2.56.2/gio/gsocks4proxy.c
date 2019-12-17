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

#include "config.h"

#include "gsocks4proxy.h"

#include "giomodule.h"
#include "giomodule-priv.h"
#include "gproxy.h"
#include "gsocks4aproxy.h"

struct _GSocks4Proxy
{
  GSocks4aProxy parent;
};

struct _GSocks4ProxyClass
{
  GSocks4aProxyClass parent_class;
};

#define g_socks4_proxy_get_type _g_socks4_proxy_get_type
G_DEFINE_TYPE_WITH_CODE (GSocks4Proxy, g_socks4_proxy, G_TYPE_SOCKS4A_PROXY,
			 _g_io_modules_ensure_extension_points_registered ();
			 g_io_extension_point_implement (G_PROXY_EXTENSION_POINT_NAME,
							 g_define_type_id,
							 "socks4",
							 0))

static void
g_socks4_proxy_finalize (GObject *object)
{
  /* must chain up */
  G_OBJECT_CLASS (g_socks4_proxy_parent_class)->finalize (object);
}

static void
g_socks4_proxy_init (GSocks4Proxy *proxy)
{
  G_SOCKS4A_PROXY (proxy)->supports_hostname = FALSE;
}


static void
g_socks4_proxy_class_init (GSocks4ProxyClass *class)
{
  GObjectClass *object_class;

  object_class = (GObjectClass *) class;
  object_class->finalize = g_socks4_proxy_finalize;
}
