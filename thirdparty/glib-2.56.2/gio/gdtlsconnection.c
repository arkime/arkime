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

#include "gdtlsconnection.h"
#include "gcancellable.h"
#include "gioenumtypes.h"
#include "gsocket.h"
#include "gtlsbackend.h"
#include "gtlscertificate.h"
#include "gdtlsclientconnection.h"
#include "gtlsdatabase.h"
#include "gtlsinteraction.h"
#include "glibintl.h"

/**
 * SECTION:gdtlsconnection
 * @short_description: DTLS connection type
 * @include: gio/gio.h
 *
 * #GDtlsConnection is the base DTLS connection class type, which wraps
 * a #GDatagramBased and provides DTLS encryption on top of it. Its
 * subclasses, #GDtlsClientConnection and #GDtlsServerConnection,
 * implement client-side and server-side DTLS, respectively.
 *
 * For TLS support, see #GTlsConnection.
 *
 * As DTLS is datagram based, #GDtlsConnection implements #GDatagramBased,
 * presenting a datagram-socket-like API for the encrypted connection. This
 * operates over a base datagram connection, which is also a #GDatagramBased
 * (#GDtlsConnection:base-socket).
 *
 * To close a DTLS connection, use g_dtls_connection_close().
 *
 * Neither #GDtlsServerConnection or #GDtlsClientConnection set the peer address
 * on their base #GDatagramBased if it is a #GSocket — it is up to the caller to
 * do that if they wish. If they do not, and g_socket_close() is called on the
 * base socket, the #GDtlsConnection will not raise a %G_IO_ERROR_NOT_CONNECTED
 * error on further I/O.
 *
 * Since: 2.48
 */

/**
 * GDtlsConnection:
 *
 * Abstract base class for the backend-specific #GDtlsClientConnection
 * and #GDtlsServerConnection types.
 *
 * Since: 2.48
 */

G_DEFINE_INTERFACE (GDtlsConnection, g_dtls_connection, G_TYPE_DATAGRAM_BASED)

enum {
  ACCEPT_CERTIFICATE,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

enum {
  PROP_BASE_SOCKET = 1,
  PROP_REQUIRE_CLOSE_NOTIFY,
  PROP_REHANDSHAKE_MODE,
  PROP_DATABASE,
  PROP_INTERACTION,
  PROP_CERTIFICATE,
  PROP_PEER_CERTIFICATE,
  PROP_PEER_CERTIFICATE_ERRORS,
};

static void
g_dtls_connection_default_init (GDtlsConnectionInterface *iface)
{
  /**
   * GDtlsConnection:base-socket:
   *
   * The #GDatagramBased that the connection wraps. Note that this may be any
   * implementation of #GDatagramBased, not just a #GSocket.
   *
   * Since: 2.48
   */
  g_object_interface_install_property (iface,
                                       g_param_spec_object ("base-socket",
                                                            P_("Base Socket"),
                                                            P_("The GDatagramBased that the connection wraps"),
                                                            G_TYPE_DATAGRAM_BASED,
                                                            G_PARAM_READWRITE |
                                                            G_PARAM_CONSTRUCT_ONLY |
                                                            G_PARAM_STATIC_STRINGS));
  /**
   * GDtlsConnection:database:
   *
   * The certificate database to use when verifying this TLS connection.
   * If no certificate database is set, then the default database will be
   * used. See g_tls_backend_get_default_database().
   *
   * Since: 2.48
   */
  g_object_interface_install_property (iface,
                                       g_param_spec_object ("database",
                                                            P_("Database"),
                                                            P_("Certificate database to use for looking up or verifying certificates"),
                                                            G_TYPE_TLS_DATABASE,
                                                            G_PARAM_READWRITE |
                                                            G_PARAM_STATIC_STRINGS));
  /**
   * GDtlsConnection:interaction:
   *
   * A #GTlsInteraction object to be used when the connection or certificate
   * database need to interact with the user. This will be used to prompt the
   * user for passwords where necessary.
   *
   * Since: 2.48
   */
  g_object_interface_install_property (iface,
                                       g_param_spec_object ("interaction",
                                                            P_("Interaction"),
                                                            P_("Optional object for user interaction"),
                                                            G_TYPE_TLS_INTERACTION,
                                                            G_PARAM_READWRITE |
                                                            G_PARAM_STATIC_STRINGS));
  /**
   * GDtlsConnection:require-close-notify:
   *
   * Whether or not proper TLS close notification is required.
   * See g_dtls_connection_set_require_close_notify().
   *
   * Since: 2.48
   */
  g_object_interface_install_property (iface,
                                       g_param_spec_boolean ("require-close-notify",
                                                             P_("Require close notify"),
                                                             P_("Whether to require proper TLS close notification"),
                                                             TRUE,
                                                             G_PARAM_READWRITE |
                                                             G_PARAM_CONSTRUCT |
                                                             G_PARAM_STATIC_STRINGS));
  /**
   * GDtlsConnection:rehandshake-mode:
   *
   * The rehandshaking mode. See
   * g_dtls_connection_set_rehandshake_mode().
   *
   * Since: 2.48
   */
  g_object_interface_install_property (iface,
                                       g_param_spec_enum ("rehandshake-mode",
                                                          P_("Rehandshake mode"),
                                                          P_("When to allow rehandshaking"),
                                                          G_TYPE_TLS_REHANDSHAKE_MODE,
                                                          G_TLS_REHANDSHAKE_NEVER,
                                                          G_PARAM_READWRITE |
                                                          G_PARAM_CONSTRUCT |
                                                          G_PARAM_STATIC_STRINGS));
  /**
   * GDtlsConnection:certificate:
   *
   * The connection's certificate; see
   * g_dtls_connection_set_certificate().
   *
   * Since: 2.48
   */
  g_object_interface_install_property (iface,
                                       g_param_spec_object ("certificate",
                                                            P_("Certificate"),
                                                            P_("The connection’s certificate"),
                                                            G_TYPE_TLS_CERTIFICATE,
                                                            G_PARAM_READWRITE |
                                                            G_PARAM_STATIC_STRINGS));
  /**
   * GDtlsConnection:peer-certificate:
   *
   * The connection's peer's certificate, after the TLS handshake has
   * completed and the certificate has been accepted. Note in
   * particular that this is not yet set during the emission of
   * #GDtlsConnection::accept-certificate.
   *
   * (You can watch for a #GObject::notify signal on this property to
   * detect when a handshake has occurred.)
   *
   * Since: 2.48
   */
  g_object_interface_install_property (iface,
                                       g_param_spec_object ("peer-certificate",
                                                            P_("Peer Certificate"),
                                                            P_("The connection’s peer’s certificate"),
                                                            G_TYPE_TLS_CERTIFICATE,
                                                            G_PARAM_READABLE |
                                                            G_PARAM_STATIC_STRINGS));
  /**
   * GDtlsConnection:peer-certificate-errors:
   *
   * The errors noticed-and-ignored while verifying
   * #GDtlsConnection:peer-certificate. Normally this should be 0, but
   * it may not be if #GDtlsClientConnection:validation-flags is not
   * %G_TLS_CERTIFICATE_VALIDATE_ALL, or if
   * #GDtlsConnection::accept-certificate overrode the default
   * behavior.
   *
   * Since: 2.48
   */
  g_object_interface_install_property (iface,
                                       g_param_spec_flags ("peer-certificate-errors",
                                                           P_("Peer Certificate Errors"),
                                                           P_("Errors found with the peer’s certificate"),
                                                           G_TYPE_TLS_CERTIFICATE_FLAGS,
                                                           0,
                                                           G_PARAM_READABLE |
                                                           G_PARAM_STATIC_STRINGS));

  /**
   * GDtlsConnection::accept-certificate:
   * @conn: a #GDtlsConnection
   * @peer_cert: the peer's #GTlsCertificate
   * @errors: the problems with @peer_cert.
   *
   * Emitted during the TLS handshake after the peer certificate has
   * been received. You can examine @peer_cert's certification path by
   * calling g_tls_certificate_get_issuer() on it.
   *
   * For a client-side connection, @peer_cert is the server's
   * certificate, and the signal will only be emitted if the
   * certificate was not acceptable according to @conn's
   * #GDtlsClientConnection:validation_flags. If you would like the
   * certificate to be accepted despite @errors, return %TRUE from the
   * signal handler. Otherwise, if no handler accepts the certificate,
   * the handshake will fail with %G_TLS_ERROR_BAD_CERTIFICATE.
   *
   * For a server-side connection, @peer_cert is the certificate
   * presented by the client, if this was requested via the server's
   * #GDtlsServerConnection:authentication_mode. On the server side,
   * the signal is always emitted when the client presents a
   * certificate, and the certificate will only be accepted if a
   * handler returns %TRUE.
   *
   * Note that if this signal is emitted as part of asynchronous I/O
   * in the main thread, then you should not attempt to interact with
   * the user before returning from the signal handler. If you want to
   * let the user decide whether or not to accept the certificate, you
   * would have to return %FALSE from the signal handler on the first
   * attempt, and then after the connection attempt returns a
   * %G_TLS_ERROR_HANDSHAKE, you can interact with the user, and if
   * the user decides to accept the certificate, remember that fact,
   * create a new connection, and return %TRUE from the signal handler
   * the next time.
   *
   * If you are doing I/O in another thread, you do not
   * need to worry about this, and can simply block in the signal
   * handler until the UI thread returns an answer.
   *
   * Returns: %TRUE to accept @peer_cert (which will also
   * immediately end the signal emission). %FALSE to allow the signal
   * emission to continue, which will cause the handshake to fail if
   * no one else overrides it.
   *
   * Since: 2.48
   */
  signals[ACCEPT_CERTIFICATE] =
    g_signal_new (I_("accept-certificate"),
                  G_TYPE_DTLS_CONNECTION,
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GDtlsConnectionInterface, accept_certificate),
                  g_signal_accumulator_true_handled, NULL,
                  NULL,
                  G_TYPE_BOOLEAN, 2,
                  G_TYPE_TLS_CERTIFICATE,
                  G_TYPE_TLS_CERTIFICATE_FLAGS);
}

/**
 * g_dtls_connection_set_database:
 * @conn: a #GDtlsConnection
 * @database: a #GTlsDatabase
 *
 * Sets the certificate database that is used to verify peer certificates.
 * This is set to the default database by default. See
 * g_tls_backend_get_default_database(). If set to %NULL, then
 * peer certificate validation will always set the
 * %G_TLS_CERTIFICATE_UNKNOWN_CA error (meaning
 * #GDtlsConnection::accept-certificate will always be emitted on
 * client-side connections, unless that bit is not set in
 * #GDtlsClientConnection:validation-flags).
 *
 * Since: 2.48
 */
void
g_dtls_connection_set_database (GDtlsConnection *conn,
                                GTlsDatabase    *database)
{
  g_return_if_fail (G_IS_DTLS_CONNECTION (conn));
  g_return_if_fail (database == NULL || G_IS_TLS_DATABASE (database));

  g_object_set (G_OBJECT (conn),
                "database", database,
                NULL);
}

/**
 * g_dtls_connection_get_database:
 * @conn: a #GDtlsConnection
 *
 * Gets the certificate database that @conn uses to verify
 * peer certificates. See g_dtls_connection_set_database().
 *
 * Returns: (transfer none): the certificate database that @conn uses or %NULL
 *
 * Since: 2.48
 */
GTlsDatabase*
g_dtls_connection_get_database (GDtlsConnection *conn)
{
  GTlsDatabase *database = NULL;

  g_return_val_if_fail (G_IS_DTLS_CONNECTION (conn), NULL);

  g_object_get (G_OBJECT (conn),
                "database", &database,
                NULL);
  if (database)
    g_object_unref (database);
  return database;
}

/**
 * g_dtls_connection_set_certificate:
 * @conn: a #GDtlsConnection
 * @certificate: the certificate to use for @conn
 *
 * This sets the certificate that @conn will present to its peer
 * during the TLS handshake. For a #GDtlsServerConnection, it is
 * mandatory to set this, and that will normally be done at construct
 * time.
 *
 * For a #GDtlsClientConnection, this is optional. If a handshake fails
 * with %G_TLS_ERROR_CERTIFICATE_REQUIRED, that means that the server
 * requires a certificate, and if you try connecting again, you should
 * call this method first. You can call
 * g_dtls_client_connection_get_accepted_cas() on the failed connection
 * to get a list of Certificate Authorities that the server will
 * accept certificates from.
 *
 * (It is also possible that a server will allow the connection with
 * or without a certificate; in that case, if you don't provide a
 * certificate, you can tell that the server requested one by the fact
 * that g_dtls_client_connection_get_accepted_cas() will return
 * non-%NULL.)
 *
 * Since: 2.48
 */
void
g_dtls_connection_set_certificate (GDtlsConnection *conn,
                                   GTlsCertificate *certificate)
{
  g_return_if_fail (G_IS_DTLS_CONNECTION (conn));
  g_return_if_fail (G_IS_TLS_CERTIFICATE (certificate));

  g_object_set (G_OBJECT (conn), "certificate", certificate, NULL);
}

/**
 * g_dtls_connection_get_certificate:
 * @conn: a #GDtlsConnection
 *
 * Gets @conn's certificate, as set by
 * g_dtls_connection_set_certificate().
 *
 * Returns: (transfer none): @conn's certificate, or %NULL
 *
 * Since: 2.48
 */
GTlsCertificate *
g_dtls_connection_get_certificate (GDtlsConnection *conn)
{
  GTlsCertificate *certificate;

  g_return_val_if_fail (G_IS_DTLS_CONNECTION (conn), NULL);

  g_object_get (G_OBJECT (conn), "certificate", &certificate, NULL);
  if (certificate)
    g_object_unref (certificate);

  return certificate;
}

/**
 * g_dtls_connection_set_interaction:
 * @conn: a connection
 * @interaction: (nullable): an interaction object, or %NULL
 *
 * Set the object that will be used to interact with the user. It will be used
 * for things like prompting the user for passwords.
 *
 * The @interaction argument will normally be a derived subclass of
 * #GTlsInteraction. %NULL can also be provided if no user interaction
 * should occur for this connection.
 *
 * Since: 2.48
 */
void
g_dtls_connection_set_interaction (GDtlsConnection *conn,
                                   GTlsInteraction *interaction)
{
  g_return_if_fail (G_IS_DTLS_CONNECTION (conn));
  g_return_if_fail (interaction == NULL || G_IS_TLS_INTERACTION (interaction));

  g_object_set (G_OBJECT (conn), "interaction", interaction, NULL);
}

/**
 * g_dtls_connection_get_interaction:
 * @conn: a connection
 *
 * Get the object that will be used to interact with the user. It will be used
 * for things like prompting the user for passwords. If %NULL is returned, then
 * no user interaction will occur for this connection.
 *
 * Returns: (transfer none): The interaction object.
 *
 * Since: 2.48
 */
GTlsInteraction *
g_dtls_connection_get_interaction (GDtlsConnection       *conn)
{
  GTlsInteraction *interaction = NULL;

  g_return_val_if_fail (G_IS_DTLS_CONNECTION (conn), NULL);

  g_object_get (G_OBJECT (conn), "interaction", &interaction, NULL);
  if (interaction)
    g_object_unref (interaction);

  return interaction;
}

/**
 * g_dtls_connection_get_peer_certificate:
 * @conn: a #GDtlsConnection
 *
 * Gets @conn's peer's certificate after the handshake has completed.
 * (It is not set during the emission of
 * #GDtlsConnection::accept-certificate.)
 *
 * Returns: (transfer none): @conn's peer's certificate, or %NULL
 *
 * Since: 2.48
 */
GTlsCertificate *
g_dtls_connection_get_peer_certificate (GDtlsConnection *conn)
{
  GTlsCertificate *peer_certificate;

  g_return_val_if_fail (G_IS_DTLS_CONNECTION (conn), NULL);

  g_object_get (G_OBJECT (conn), "peer-certificate", &peer_certificate, NULL);
  if (peer_certificate)
    g_object_unref (peer_certificate);

  return peer_certificate;
}

/**
 * g_dtls_connection_get_peer_certificate_errors:
 * @conn: a #GDtlsConnection
 *
 * Gets the errors associated with validating @conn's peer's
 * certificate, after the handshake has completed. (It is not set
 * during the emission of #GDtlsConnection::accept-certificate.)
 *
 * Returns: @conn's peer's certificate errors
 *
 * Since: 2.48
 */
GTlsCertificateFlags
g_dtls_connection_get_peer_certificate_errors (GDtlsConnection *conn)
{
  GTlsCertificateFlags errors;

  g_return_val_if_fail (G_IS_DTLS_CONNECTION (conn), 0);

  g_object_get (G_OBJECT (conn), "peer-certificate-errors", &errors, NULL);
  return errors;
}

/**
 * g_dtls_connection_set_require_close_notify:
 * @conn: a #GDtlsConnection
 * @require_close_notify: whether or not to require close notification
 *
 * Sets whether or not @conn expects a proper TLS close notification
 * before the connection is closed. If this is %TRUE (the default),
 * then @conn will expect to receive a TLS close notification from its
 * peer before the connection is closed, and will return a
 * %G_TLS_ERROR_EOF error if the connection is closed without proper
 * notification (since this may indicate a network error, or
 * man-in-the-middle attack).
 *
 * In some protocols, the application will know whether or not the
 * connection was closed cleanly based on application-level data
 * (because the application-level data includes a length field, or is
 * somehow self-delimiting); in this case, the close notify is
 * redundant and may be omitted. You
 * can use g_dtls_connection_set_require_close_notify() to tell @conn
 * to allow an "unannounced" connection close, in which case the close
 * will show up as a 0-length read, as in a non-TLS
 * #GDatagramBased, and it is up to the application to check that
 * the data has been fully received.
 *
 * Note that this only affects the behavior when the peer closes the
 * connection; when the application calls g_dtls_connection_close_async() on
 * @conn itself, this will send a close notification regardless of the
 * setting of this property. If you explicitly want to do an unclean
 * close, you can close @conn's #GDtlsConnection:base-socket rather
 * than closing @conn itself.
 *
 * Since: 2.48
 */
void
g_dtls_connection_set_require_close_notify (GDtlsConnection *conn,
                                            gboolean         require_close_notify)
{
  g_return_if_fail (G_IS_DTLS_CONNECTION (conn));

  g_object_set (G_OBJECT (conn),
                "require-close-notify", require_close_notify,
                NULL);
}

/**
 * g_dtls_connection_get_require_close_notify:
 * @conn: a #GDtlsConnection
 *
 * Tests whether or not @conn expects a proper TLS close notification
 * when the connection is closed. See
 * g_dtls_connection_set_require_close_notify() for details.
 *
 * Returns: %TRUE if @conn requires a proper TLS close notification.
 *
 * Since: 2.48
 */
gboolean
g_dtls_connection_get_require_close_notify (GDtlsConnection *conn)
{
  gboolean require_close_notify;

  g_return_val_if_fail (G_IS_DTLS_CONNECTION (conn), TRUE);

  g_object_get (G_OBJECT (conn),
                "require-close-notify", &require_close_notify,
                NULL);
  return require_close_notify;
}

/**
 * g_dtls_connection_set_rehandshake_mode:
 * @conn: a #GDtlsConnection
 * @mode: the rehandshaking mode
 *
 * Sets how @conn behaves with respect to rehandshaking requests.
 *
 * %G_TLS_REHANDSHAKE_NEVER means that it will never agree to
 * rehandshake after the initial handshake is complete. (For a client,
 * this means it will refuse rehandshake requests from the server, and
 * for a server, this means it will close the connection with an error
 * if the client attempts to rehandshake.)
 *
 * %G_TLS_REHANDSHAKE_SAFELY means that the connection will allow a
 * rehandshake only if the other end of the connection supports the
 * TLS `renegotiation_info` extension. This is the default behavior,
 * but means that rehandshaking will not work against older
 * implementations that do not support that extension.
 *
 * %G_TLS_REHANDSHAKE_UNSAFELY means that the connection will allow
 * rehandshaking even without the `renegotiation_info` extension. On
 * the server side in particular, this is not recommended, since it
 * leaves the server open to certain attacks. However, this mode is
 * necessary if you need to allow renegotiation with older client
 * software.
 *
 * Since: 2.48
 */
void
g_dtls_connection_set_rehandshake_mode (GDtlsConnection     *conn,
                                        GTlsRehandshakeMode  mode)
{
  g_return_if_fail (G_IS_DTLS_CONNECTION (conn));

  g_object_set (G_OBJECT (conn),
                "rehandshake-mode", mode,
                NULL);
}

/**
 * g_dtls_connection_get_rehandshake_mode:
 * @conn: a #GDtlsConnection
 *
 * Gets @conn rehandshaking mode. See
 * g_dtls_connection_set_rehandshake_mode() for details.
 *
 * Returns: @conn's rehandshaking mode
 *
 * Since: 2.48
 */
GTlsRehandshakeMode
g_dtls_connection_get_rehandshake_mode (GDtlsConnection       *conn)
{
  GTlsRehandshakeMode mode;

  g_return_val_if_fail (G_IS_DTLS_CONNECTION (conn), G_TLS_REHANDSHAKE_NEVER);

  g_object_get (G_OBJECT (conn),
                "rehandshake-mode", &mode,
                NULL);
  return mode;
}

/**
 * g_dtls_connection_handshake:
 * @conn: a #GDtlsConnection
 * @cancellable: (nullable): a #GCancellable, or %NULL
 * @error: a #GError, or %NULL
 *
 * Attempts a TLS handshake on @conn.
 *
 * On the client side, it is never necessary to call this method;
 * although the connection needs to perform a handshake after
 * connecting (or after sending a "STARTTLS"-type command) and may
 * need to rehandshake later if the server requests it,
 * #GDtlsConnection will handle this for you automatically when you try
 * to send or receive data on the connection. However, you can call
 * g_dtls_connection_handshake() manually if you want to know for sure
 * whether the initial handshake succeeded or failed (as opposed to
 * just immediately trying to write to @conn, in which
 * case if it fails, it may not be possible to tell if it failed
 * before or after completing the handshake).
 *
 * Likewise, on the server side, although a handshake is necessary at
 * the beginning of the communication, you do not need to call this
 * function explicitly unless you want clearer error reporting.
 * However, you may call g_dtls_connection_handshake() later on to
 * renegotiate parameters (encryption methods, etc) with the client.
 *
 * #GDtlsConnection::accept_certificate may be emitted during the
 * handshake.
 *
 * Returns: success or failure
 *
 * Since: 2.48
 */
gboolean
g_dtls_connection_handshake (GDtlsConnection  *conn,
                             GCancellable     *cancellable,
                             GError          **error)
{
  g_return_val_if_fail (G_IS_DTLS_CONNECTION (conn), FALSE);

  return G_DTLS_CONNECTION_GET_INTERFACE (conn)->handshake (conn, cancellable,
                                                            error);
}

/**
 * g_dtls_connection_handshake_async:
 * @conn: a #GDtlsConnection
 * @io_priority: the [I/O priority][io-priority] of the request
 * @cancellable: (nullable): a #GCancellable, or %NULL
 * @callback: callback to call when the handshake is complete
 * @user_data: the data to pass to the callback function
 *
 * Asynchronously performs a TLS handshake on @conn. See
 * g_dtls_connection_handshake() for more information.
 *
 * Since: 2.48
 */
void
g_dtls_connection_handshake_async (GDtlsConnection      *conn,
                                   int                   io_priority,
                                   GCancellable         *cancellable,
                                   GAsyncReadyCallback   callback,
                                   gpointer              user_data)
{
  g_return_if_fail (G_IS_DTLS_CONNECTION (conn));

  G_DTLS_CONNECTION_GET_INTERFACE (conn)->handshake_async (conn, io_priority,
                                                           cancellable,
                                                           callback, user_data);
}

/**
 * g_dtls_connection_handshake_finish:
 * @conn: a #GDtlsConnection
 * @result: a #GAsyncResult.
 * @error: a #GError pointer, or %NULL
 *
 * Finish an asynchronous TLS handshake operation. See
 * g_dtls_connection_handshake() for more information.
 *
 * Returns: %TRUE on success, %FALSE on failure, in which
 * case @error will be set.
 *
 * Since: 2.48
 */
gboolean
g_dtls_connection_handshake_finish (GDtlsConnection  *conn,
                                    GAsyncResult     *result,
                                    GError          **error)
{
  g_return_val_if_fail (G_IS_DTLS_CONNECTION (conn), FALSE);

  return G_DTLS_CONNECTION_GET_INTERFACE (conn)->handshake_finish (conn,
                                                                   result,
                                                                   error);
}

/**
 * g_dtls_connection_shutdown:
 * @conn: a #GDtlsConnection
 * @shutdown_read: %TRUE to stop reception of incoming datagrams
 * @shutdown_write: %TRUE to stop sending outgoing datagrams
 * @cancellable: (nullable): a #GCancellable, or %NULL
 * @error: a #GError, or %NULL
 *
 * Shut down part or all of a DTLS connection.
 *
 * If @shutdown_read is %TRUE then the receiving side of the connection is shut
 * down, and further reading is disallowed. Subsequent calls to
 * g_datagram_based_receive_messages() will return %G_IO_ERROR_CLOSED.
 *
 * If @shutdown_write is %TRUE then the sending side of the connection is shut
 * down, and further writing is disallowed. Subsequent calls to
 * g_datagram_based_send_messages() will return %G_IO_ERROR_CLOSED.
 *
 * It is allowed for both @shutdown_read and @shutdown_write to be TRUE — this
 * is equivalent to calling g_dtls_connection_close().
 *
 * If @cancellable is cancelled, the #GDtlsConnection may be left
 * partially-closed and any pending untransmitted data may be lost. Call
 * g_dtls_connection_shutdown() again to complete closing the #GDtlsConnection.
 *
 * Returns: %TRUE on success, %FALSE otherwise
 *
 * Since: 2.48
 */
gboolean
g_dtls_connection_shutdown (GDtlsConnection  *conn,
                            gboolean          shutdown_read,
                            gboolean          shutdown_write,
                            GCancellable     *cancellable,
                            GError          **error)
{
  GDtlsConnectionInterface *iface;

  g_return_val_if_fail (G_IS_DTLS_CONNECTION (conn), FALSE);
  g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable),
                        FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  if (!shutdown_read && !shutdown_write)
    return TRUE;

  iface = G_DTLS_CONNECTION_GET_INTERFACE (conn);
  g_assert (iface->shutdown != NULL);

  return iface->shutdown (conn, shutdown_read, shutdown_write,
                          cancellable, error);
}

/**
 * g_dtls_connection_shutdown_async:
 * @conn: a #GDtlsConnection
 * @shutdown_read: %TRUE to stop reception of incoming datagrams
 * @shutdown_write: %TRUE to stop sending outgoing datagrams
 * @io_priority: the [I/O priority][io-priority] of the request
 * @cancellable: (nullable): a #GCancellable, or %NULL
 * @callback: callback to call when the shutdown operation is complete
 * @user_data: the data to pass to the callback function
 *
 * Asynchronously shut down part or all of the DTLS connection. See
 * g_dtls_connection_shutdown() for more information.
 *
 * Since: 2.48
 */
void
g_dtls_connection_shutdown_async (GDtlsConnection      *conn,
                                  gboolean              shutdown_read,
                                  gboolean              shutdown_write,
                                  int                   io_priority,
                                  GCancellable         *cancellable,
                                  GAsyncReadyCallback   callback,
                                  gpointer              user_data)
{
  GDtlsConnectionInterface *iface;

  g_return_if_fail (G_IS_DTLS_CONNECTION (conn));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  iface = G_DTLS_CONNECTION_GET_INTERFACE (conn);
  g_assert (iface->shutdown_async != NULL);

  iface->shutdown_async (conn, TRUE, TRUE, io_priority, cancellable,
                         callback, user_data);
}

/**
 * g_dtls_connection_shutdown_finish:
 * @conn: a #GDtlsConnection
 * @result: a #GAsyncResult
 * @error: a #GError pointer, or %NULL
 *
 * Finish an asynchronous TLS shutdown operation. See
 * g_dtls_connection_shutdown() for more information.
 *
 * Returns: %TRUE on success, %FALSE on failure, in which
 * case @error will be set
 *
 * Since: 2.48
 */
gboolean
g_dtls_connection_shutdown_finish (GDtlsConnection  *conn,
                                   GAsyncResult     *result,
                                   GError          **error)
{
  GDtlsConnectionInterface *iface;

  g_return_val_if_fail (G_IS_DTLS_CONNECTION (conn), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  iface = G_DTLS_CONNECTION_GET_INTERFACE (conn);
  g_assert (iface->shutdown_finish != NULL);

  return iface->shutdown_finish (conn, result, error);
}

/**
 * g_dtls_connection_close:
 * @conn: a #GDtlsConnection
 * @cancellable: (nullable): a #GCancellable, or %NULL
 * @error: a #GError, or %NULL
 *
 * Close the DTLS connection. This is equivalent to calling
 * g_dtls_connection_shutdown() to shut down both sides of the connection.
 *
 * Closing a #GDtlsConnection waits for all buffered but untransmitted data to
 * be sent before it completes. It then sends a `close_notify` DTLS alert to the
 * peer and may wait for a `close_notify` to be received from the peer. It does
 * not close the underlying #GDtlsConnection:base-socket; that must be closed
 * separately.
 *
 * Once @conn is closed, all other operations will return %G_IO_ERROR_CLOSED.
 * Closing a #GDtlsConnection multiple times will not return an error.
 *
 * #GDtlsConnections will be automatically closed when the last reference is
 * dropped, but you might want to call this function to make sure resources are
 * released as early as possible.
 *
 * If @cancellable is cancelled, the #GDtlsConnection may be left
 * partially-closed and any pending untransmitted data may be lost. Call
 * g_dtls_connection_close() again to complete closing the #GDtlsConnection.
 *
 * Returns: %TRUE on success, %FALSE otherwise
 *
 * Since: 2.48
 */
gboolean
g_dtls_connection_close (GDtlsConnection  *conn,
                         GCancellable     *cancellable,
                         GError          **error)
{
  g_return_val_if_fail (G_IS_DTLS_CONNECTION (conn), FALSE);
  g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable),
                        FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return G_DTLS_CONNECTION_GET_INTERFACE (conn)->shutdown (conn, TRUE, TRUE,
                                                           cancellable, error);
}

/**
 * g_dtls_connection_close_async:
 * @conn: a #GDtlsConnection
 * @io_priority: the [I/O priority][io-priority] of the request
 * @cancellable: (nullable): a #GCancellable, or %NULL
 * @callback: callback to call when the close operation is complete
 * @user_data: the data to pass to the callback function
 *
 * Asynchronously close the DTLS connection. See g_dtls_connection_close() for
 * more information.
 *
 * Since: 2.48
 */
void
g_dtls_connection_close_async (GDtlsConnection      *conn,
                               int                   io_priority,
                               GCancellable         *cancellable,
                               GAsyncReadyCallback   callback,
                               gpointer              user_data)
{
  g_return_if_fail (G_IS_DTLS_CONNECTION (conn));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));

  G_DTLS_CONNECTION_GET_INTERFACE (conn)->shutdown_async (conn, TRUE, TRUE,
                                                          io_priority,
                                                          cancellable,
                                                          callback, user_data);
}

/**
 * g_dtls_connection_close_finish:
 * @conn: a #GDtlsConnection
 * @result: a #GAsyncResult
 * @error: a #GError pointer, or %NULL
 *
 * Finish an asynchronous TLS close operation. See g_dtls_connection_close()
 * for more information.
 *
 * Returns: %TRUE on success, %FALSE on failure, in which
 * case @error will be set
 *
 * Since: 2.48
 */
gboolean
g_dtls_connection_close_finish (GDtlsConnection  *conn,
                                GAsyncResult     *result,
                                GError          **error)
{
  g_return_val_if_fail (G_IS_DTLS_CONNECTION (conn), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return G_DTLS_CONNECTION_GET_INTERFACE (conn)->shutdown_finish (conn, result,
                                                                  error);
}

/**
 * g_dtls_connection_emit_accept_certificate:
 * @conn: a #GDtlsConnection
 * @peer_cert: the peer's #GTlsCertificate
 * @errors: the problems with @peer_cert
 *
 * Used by #GDtlsConnection implementations to emit the
 * #GDtlsConnection::accept-certificate signal.
 *
 * Returns: %TRUE if one of the signal handlers has returned
 *     %TRUE to accept @peer_cert
 *
 * Since: 2.48
 */
gboolean
g_dtls_connection_emit_accept_certificate (GDtlsConnection      *conn,
                                           GTlsCertificate      *peer_cert,
                                           GTlsCertificateFlags  errors)
{
  gboolean accept = FALSE;

  g_signal_emit (conn, signals[ACCEPT_CERTIFICATE], 0,
                 peer_cert, errors, &accept);
  return accept;
}
