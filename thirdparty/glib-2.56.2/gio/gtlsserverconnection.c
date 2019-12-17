/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright © 2010 Red Hat, Inc
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
#include "glib.h"

#include "gtlsserverconnection.h"
#include "ginitable.h"
#include "gioenumtypes.h"
#include "gsocket.h"
#include "gtlsbackend.h"
#include "gtlscertificate.h"
#include "glibintl.h"

/**
 * SECTION:gtlsserverconnection
 * @short_description: TLS server-side connection
 * @include: gio/gio.h
 *
 * #GTlsServerConnection is the server-side subclass of #GTlsConnection,
 * representing a server-side TLS connection.
 *
 * Since: 2.28
 */

G_DEFINE_INTERFACE (GTlsServerConnection, g_tls_server_connection, G_TYPE_TLS_CONNECTION)

static void
g_tls_server_connection_default_init (GTlsServerConnectionInterface *iface)
{
  /**
   * GTlsServerConnection:authentication-mode:
   *
   * The #GTlsAuthenticationMode for the server. This can be changed
   * before calling g_tls_connection_handshake() if you want to
   * rehandshake with a different mode from the initial handshake.
   *
   * Since: 2.28
   */
  g_object_interface_install_property (iface,
				       g_param_spec_enum ("authentication-mode",
							  P_("Authentication Mode"),
							  P_("The client authentication mode"),
							  G_TYPE_TLS_AUTHENTICATION_MODE,
							  G_TLS_AUTHENTICATION_NONE,
							  G_PARAM_READWRITE |
							  G_PARAM_STATIC_STRINGS));
}

/**
 * g_tls_server_connection_new:
 * @base_io_stream: the #GIOStream to wrap
 * @certificate: (nullable): the default server certificate, or %NULL
 * @error: #GError for error reporting, or %NULL to ignore.
 *
 * Creates a new #GTlsServerConnection wrapping @base_io_stream (which
 * must have pollable input and output streams).
 *
 * See the documentation for #GTlsConnection:base-io-stream for restrictions
 * on when application code can run operations on the @base_io_stream after
 * this function has returned.
 *
 * Returns: (transfer full) (type GTlsServerConnection): the new
 * #GTlsServerConnection, or %NULL on error
 *
 * Since: 2.28
 */
GIOStream *
g_tls_server_connection_new (GIOStream        *base_io_stream,
			     GTlsCertificate  *certificate,
			     GError          **error)
{
  GObject *conn;
  GTlsBackend *backend;

  backend = g_tls_backend_get_default ();
  conn = g_initable_new (g_tls_backend_get_server_connection_type (backend),
			 NULL, error,
			 "base-io-stream", base_io_stream,
			 "certificate", certificate,
			 NULL);
  return G_IO_STREAM (conn);
}
