/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright © 2010 Red Hat, Inc
 * Copyright © 2015 Collabora, Ltd.
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

#include "gdtlsclientconnection.h"
#include "ginitable.h"
#include "gioenumtypes.h"
#include "gsocket.h"
#include "gsocketconnectable.h"
#include "gtlsbackend.h"
#include "gtlscertificate.h"
#include "glibintl.h"

/**
 * SECTION:gdtlsclientconnection
 * @short_description: DTLS client-side connection
 * @include: gio/gio.h
 *
 * #GDtlsClientConnection is the client-side subclass of
 * #GDtlsConnection, representing a client-side DTLS connection.
 *
 * Since: 2.48
 */

/**
 * GDtlsClientConnection:
 *
 * Abstract base class for the backend-specific client connection
 * type.
 *
 * Since: 2.48
 */

G_DEFINE_INTERFACE (GDtlsClientConnection, g_dtls_client_connection,
                    G_TYPE_DTLS_CONNECTION)

static void
g_dtls_client_connection_default_init (GDtlsClientConnectionInterface *iface)
{
  /**
   * GDtlsClientConnection:validation-flags:
   *
   * What steps to perform when validating a certificate received from
   * a server. Server certificates that fail to validate in all of the
   * ways indicated here will be rejected unless the application
   * overrides the default via #GDtlsConnection::accept-certificate.
   *
   * Since: 2.48
   */
  g_object_interface_install_property (iface,
                                       g_param_spec_flags ("validation-flags",
                                                           P_("Validation flags"),
                                                           P_("What certificate validation to perform"),
                                                           G_TYPE_TLS_CERTIFICATE_FLAGS,
                                                           G_TLS_CERTIFICATE_VALIDATE_ALL,
                                                           G_PARAM_READWRITE |
                                                           G_PARAM_CONSTRUCT |
                                                           G_PARAM_STATIC_STRINGS));

  /**
   * GDtlsClientConnection:server-identity:
   *
   * A #GSocketConnectable describing the identity of the server that
   * is expected on the other end of the connection.
   *
   * If the %G_TLS_CERTIFICATE_BAD_IDENTITY flag is set in
   * #GDtlsClientConnection:validation-flags, this object will be used
   * to determine the expected identify of the remote end of the
   * connection; if #GDtlsClientConnection:server-identity is not set,
   * or does not match the identity presented by the server, then the
   * %G_TLS_CERTIFICATE_BAD_IDENTITY validation will fail.
   *
   * In addition to its use in verifying the server certificate,
   * this is also used to give a hint to the server about what
   * certificate we expect, which is useful for servers that serve
   * virtual hosts.
   *
   * Since: 2.48
   */
  g_object_interface_install_property (iface,
                                       g_param_spec_object ("server-identity",
                                                            P_("Server identity"),
                                                            P_("GSocketConnectable identifying the server"),
                                                            G_TYPE_SOCKET_CONNECTABLE,
                                                            G_PARAM_READWRITE |
                                                            G_PARAM_CONSTRUCT |
                                                            G_PARAM_STATIC_STRINGS));

  /**
   * GDtlsClientConnection:accepted-cas: (type GLib.List) (element-type GLib.ByteArray)
   *
   * A list of the distinguished names of the Certificate Authorities
   * that the server will accept client certificates signed by. If the
   * server requests a client certificate during the handshake, then
   * this property will be set after the handshake completes.
   *
   * Each item in the list is a #GByteArray which contains the complete
   * subject DN of the certificate authority.
   *
   * Since: 2.48
   */
  g_object_interface_install_property (iface,
                                       g_param_spec_pointer ("accepted-cas",
                                                             P_("Accepted CAs"),
                                                             P_("Distinguished names of the CAs the server accepts certificates from"),
                                                             G_PARAM_READABLE |
                                                             G_PARAM_STATIC_STRINGS));
}

/**
 * g_dtls_client_connection_new:
 * @base_socket: the #GDatagramBased to wrap
 * @server_identity: (nullable): the expected identity of the server
 * @error: #GError for error reporting, or %NULL to ignore.
 *
 * Creates a new #GDtlsClientConnection wrapping @base_socket which is
 * assumed to communicate with the server identified by @server_identity.
 *
 * Returns: (transfer full) (type GDtlsClientConnection): the new
 *   #GDtlsClientConnection, or %NULL on error
 *
 * Since: 2.48
 */
GDatagramBased *
g_dtls_client_connection_new (GDatagramBased      *base_socket,
                              GSocketConnectable  *server_identity,
                              GError             **error)
{
  GObject *conn;
  GTlsBackend *backend;

  backend = g_tls_backend_get_default ();
  conn = g_initable_new (g_tls_backend_get_dtls_client_connection_type (backend),
                         NULL, error,
                         "base-socket", base_socket,
                         "server-identity", server_identity,
                         NULL);
  return G_DATAGRAM_BASED (conn);
}

/**
 * g_dtls_client_connection_get_validation_flags:
 * @conn: the #GDtlsClientConnection
 *
 * Gets @conn's validation flags
 *
 * Returns: the validation flags
 *
 * Since: 2.48
 */
GTlsCertificateFlags
g_dtls_client_connection_get_validation_flags (GDtlsClientConnection *conn)
{
  GTlsCertificateFlags flags = 0;

  g_return_val_if_fail (G_IS_DTLS_CLIENT_CONNECTION (conn), 0);

  g_object_get (G_OBJECT (conn), "validation-flags", &flags, NULL);
  return flags;
}

/**
 * g_dtls_client_connection_set_validation_flags:
 * @conn: the #GDtlsClientConnection
 * @flags: the #GTlsCertificateFlags to use
 *
 * Sets @conn's validation flags, to override the default set of
 * checks performed when validating a server certificate. By default,
 * %G_TLS_CERTIFICATE_VALIDATE_ALL is used.
 *
 * Since: 2.48
 */
void
g_dtls_client_connection_set_validation_flags (GDtlsClientConnection  *conn,
                                               GTlsCertificateFlags   flags)
{
  g_return_if_fail (G_IS_DTLS_CLIENT_CONNECTION (conn));

  g_object_set (G_OBJECT (conn), "validation-flags", flags, NULL);
}

/**
 * g_dtls_client_connection_get_server_identity:
 * @conn: the #GDtlsClientConnection
 *
 * Gets @conn's expected server identity
 *
 * Returns: (transfer none): a #GSocketConnectable describing the
 * expected server identity, or %NULL if the expected identity is not
 * known.
 *
 * Since: 2.48
 */
GSocketConnectable *
g_dtls_client_connection_get_server_identity (GDtlsClientConnection *conn)
{
  GSocketConnectable *identity = NULL;

  g_return_val_if_fail (G_IS_DTLS_CLIENT_CONNECTION (conn), 0);

  g_object_get (G_OBJECT (conn), "server-identity", &identity, NULL);
  if (identity)
    g_object_unref (identity);
  return identity;
}

/**
 * g_dtls_client_connection_set_server_identity:
 * @conn: the #GDtlsClientConnection
 * @identity: a #GSocketConnectable describing the expected server identity
 *
 * Sets @conn's expected server identity, which is used both to tell
 * servers on virtual hosts which certificate to present, and also
 * to let @conn know what name to look for in the certificate when
 * performing %G_TLS_CERTIFICATE_BAD_IDENTITY validation, if enabled.
 *
 * Since: 2.48
 */
void
g_dtls_client_connection_set_server_identity (GDtlsClientConnection *conn,
                                              GSocketConnectable    *identity)
{
  g_return_if_fail (G_IS_DTLS_CLIENT_CONNECTION (conn));

  g_object_set (G_OBJECT (conn), "server-identity", identity, NULL);
}

/**
 * g_dtls_client_connection_get_accepted_cas:
 * @conn: the #GDtlsClientConnection
 *
 * Gets the list of distinguished names of the Certificate Authorities
 * that the server will accept certificates from. This will be set
 * during the TLS handshake if the server requests a certificate.
 * Otherwise, it will be %NULL.
 *
 * Each item in the list is a #GByteArray which contains the complete
 * subject DN of the certificate authority.
 *
 * Returns: (element-type GByteArray) (transfer full): the list of
 * CA DNs. You should unref each element with g_byte_array_unref() and then
 * the free the list with g_list_free().
 *
 * Since: 2.48
 */
GList *
g_dtls_client_connection_get_accepted_cas (GDtlsClientConnection *conn)
{
  GList *accepted_cas = NULL;

  g_return_val_if_fail (G_IS_DTLS_CLIENT_CONNECTION (conn), NULL);

  g_object_get (G_OBJECT (conn), "accepted-cas", &accepted_cas, NULL);
  return accepted_cas;
}
