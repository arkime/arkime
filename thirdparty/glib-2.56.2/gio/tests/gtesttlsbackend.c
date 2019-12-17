/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright (C) 2011 Collabora Ltd.
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

#include "gtesttlsbackend.h"

#include <glib.h>

static GType _g_test_tls_certificate_get_type (void);
static GType _g_test_tls_connection_get_type (void);

struct _GTestTlsBackend {
  GObject parent_instance;
};

static void g_test_tls_backend_iface_init (GTlsBackendInterface *iface);

#define g_test_tls_backend_get_type _g_test_tls_backend_get_type
G_DEFINE_TYPE_WITH_CODE (GTestTlsBackend, g_test_tls_backend, G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (G_TYPE_TLS_BACKEND,
						g_test_tls_backend_iface_init)
                         g_io_extension_point_set_required_type (
                           g_io_extension_point_register (G_TLS_BACKEND_EXTENSION_POINT_NAME),
                           G_TYPE_TLS_BACKEND);
			 g_io_extension_point_implement (G_TLS_BACKEND_EXTENSION_POINT_NAME,
							 g_define_type_id,
							 "test",
							 999);)

static void
g_test_tls_backend_init (GTestTlsBackend *backend)
{
}

static void
g_test_tls_backend_class_init (GTestTlsBackendClass *backend_class)
{
}

static void
g_test_tls_backend_iface_init (GTlsBackendInterface *iface)
{
  iface->get_certificate_type = _g_test_tls_certificate_get_type;
  iface->get_client_connection_type = _g_test_tls_connection_get_type;
  iface->get_server_connection_type = _g_test_tls_connection_get_type;
}

/* Test certificate type */

typedef struct _GTestTlsCertificate      GTestTlsCertificate;
typedef struct _GTestTlsCertificateClass GTestTlsCertificateClass;

struct _GTestTlsCertificate {
  GTlsCertificate parent_instance;
  gchar *key_pem;
  gchar *cert_pem;
  GTlsCertificate *issuer;
};

struct _GTestTlsCertificateClass {
  GTlsCertificateClass parent_class;
};

enum
{
  PROP_CERTIFICATE_0,

  PROP_CERT_CERTIFICATE,
  PROP_CERT_CERTIFICATE_PEM,
  PROP_CERT_PRIVATE_KEY,
  PROP_CERT_PRIVATE_KEY_PEM,
  PROP_CERT_ISSUER
};

static void g_test_tls_certificate_initable_iface_init (GInitableIface *iface);

#define g_test_tls_certificate_get_type _g_test_tls_certificate_get_type
G_DEFINE_TYPE_WITH_CODE (GTestTlsCertificate, g_test_tls_certificate, G_TYPE_TLS_CERTIFICATE,
			 G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
						g_test_tls_certificate_initable_iface_init))

static GTlsCertificateFlags
g_test_tls_certificate_verify (GTlsCertificate     *cert,
                               GSocketConnectable  *identity,
                               GTlsCertificate     *trusted_ca)
{
  /* For now, all of the tests expect the certificate to verify */
  return 0;
}

static void
g_test_tls_certificate_get_property (GObject    *object,
				      guint       prop_id,
				      GValue     *value,
				      GParamSpec *pspec)
{
  GTestTlsCertificate *cert = (GTestTlsCertificate *) object;

  switch (prop_id)
    {
    case PROP_CERT_CERTIFICATE_PEM:
      g_value_set_string (value, cert->cert_pem);
      break;
    case PROP_CERT_PRIVATE_KEY_PEM:
      g_value_set_string (value, cert->key_pem);
      break;
    case PROP_CERT_ISSUER:
      g_value_set_object (value, cert->issuer);
      break;
    default:
      g_assert_not_reached ();
      break;
    }
}

static void
g_test_tls_certificate_set_property (GObject      *object,
				      guint         prop_id,
				      const GValue *value,
				      GParamSpec   *pspec)
{
  GTestTlsCertificate *cert = (GTestTlsCertificate *) object;

  switch (prop_id)
    {
    case PROP_CERT_CERTIFICATE_PEM:
      cert->cert_pem = g_value_dup_string (value);
      break;
    case PROP_CERT_PRIVATE_KEY_PEM:
      cert->key_pem = g_value_dup_string (value);
      break;
    case PROP_CERT_ISSUER:
      cert->issuer = g_value_dup_object (value);
      break;
    case PROP_CERT_CERTIFICATE:
    case PROP_CERT_PRIVATE_KEY:
      /* ignore */
      break;
    default:
      g_assert_not_reached ();
      break;
    }
}

static void
g_test_tls_certificate_finalize (GObject *object)
{
  GTestTlsCertificate *cert = (GTestTlsCertificate *) object;

  g_free (cert->cert_pem);
  g_free (cert->key_pem);
  g_clear_object (&cert->issuer);
}

static void
g_test_tls_certificate_class_init (GTestTlsCertificateClass *test_class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (test_class);
  GTlsCertificateClass *certificate_class = G_TLS_CERTIFICATE_CLASS (test_class);

  gobject_class->get_property = g_test_tls_certificate_get_property;
  gobject_class->set_property = g_test_tls_certificate_set_property;
  gobject_class->finalize = g_test_tls_certificate_finalize;

  certificate_class->verify = g_test_tls_certificate_verify;

  g_object_class_override_property (gobject_class, PROP_CERT_CERTIFICATE, "certificate");
  g_object_class_override_property (gobject_class, PROP_CERT_CERTIFICATE_PEM, "certificate-pem");
  g_object_class_override_property (gobject_class, PROP_CERT_PRIVATE_KEY, "private-key");
  g_object_class_override_property (gobject_class, PROP_CERT_PRIVATE_KEY_PEM, "private-key-pem");
  g_object_class_override_property (gobject_class, PROP_CERT_ISSUER, "issuer");
}

static void
g_test_tls_certificate_init (GTestTlsCertificate *certificate)
{
}

static gboolean
g_test_tls_certificate_initable_init (GInitable       *initable,
				       GCancellable    *cancellable,
				       GError         **error)
{
  return TRUE;
}

static void
g_test_tls_certificate_initable_iface_init (GInitableIface  *iface)
{
  iface->init = g_test_tls_certificate_initable_init;
}

/* Dummy connection type; since GTlsClientConnection and
 * GTlsServerConnection are just interfaces, we can implement them
 * both on a single object.
 */

typedef struct _GTestTlsConnection      GTestTlsConnection;
typedef struct _GTestTlsConnectionClass GTestTlsConnectionClass;

struct _GTestTlsConnection {
  GTlsConnection parent_instance;
};

struct _GTestTlsConnectionClass {
  GTlsConnectionClass parent_class;
};

enum
{
  PROP_CONNECTION_0,

  PROP_CONN_BASE_IO_STREAM,
  PROP_CONN_USE_SYSTEM_CERTDB,
  PROP_CONN_REQUIRE_CLOSE_NOTIFY,
  PROP_CONN_REHANDSHAKE_MODE,
  PROP_CONN_CERTIFICATE,
  PROP_CONN_PEER_CERTIFICATE,
  PROP_CONN_PEER_CERTIFICATE_ERRORS,
  PROP_CONN_VALIDATION_FLAGS,
  PROP_CONN_SERVER_IDENTITY,
  PROP_CONN_USE_SSL3,
  PROP_CONN_ACCEPTED_CAS,
  PROP_CONN_AUTHENTICATION_MODE
};

static void g_test_tls_connection_initable_iface_init (GInitableIface *iface);

#define g_test_tls_connection_get_type _g_test_tls_connection_get_type
G_DEFINE_TYPE_WITH_CODE (GTestTlsConnection, g_test_tls_connection, G_TYPE_TLS_CONNECTION,
			 G_IMPLEMENT_INTERFACE (G_TYPE_TLS_CLIENT_CONNECTION, NULL)
			 G_IMPLEMENT_INTERFACE (G_TYPE_TLS_SERVER_CONNECTION, NULL)
			 G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
						g_test_tls_connection_initable_iface_init))

static void
g_test_tls_connection_get_property (GObject    *object,
				     guint       prop_id,
				     GValue     *value,
				     GParamSpec *pspec)
{
}

static void
g_test_tls_connection_set_property (GObject      *object,
				     guint         prop_id,
				     const GValue *value,
				     GParamSpec   *pspec)
{
}

static gboolean
g_test_tls_connection_close (GIOStream     *stream,
			      GCancellable  *cancellable,
			      GError       **error)
{
  return TRUE;
}

static void
g_test_tls_connection_class_init (GTestTlsConnectionClass *connection_class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (connection_class);
  GIOStreamClass *io_stream_class = G_IO_STREAM_CLASS (connection_class);

  gobject_class->get_property = g_test_tls_connection_get_property;
  gobject_class->set_property = g_test_tls_connection_set_property;

  /* Need to override this because when initable_init fails it will
   * dispose the connection, which will close it, which would
   * otherwise try to close its input/output streams, which don't
   * exist.
   */
  io_stream_class->close_fn = g_test_tls_connection_close;

  g_object_class_override_property (gobject_class, PROP_CONN_BASE_IO_STREAM, "base-io-stream");
  g_object_class_override_property (gobject_class, PROP_CONN_USE_SYSTEM_CERTDB, "use-system-certdb");
  g_object_class_override_property (gobject_class, PROP_CONN_REQUIRE_CLOSE_NOTIFY, "require-close-notify");
  g_object_class_override_property (gobject_class, PROP_CONN_REHANDSHAKE_MODE, "rehandshake-mode");
  g_object_class_override_property (gobject_class, PROP_CONN_CERTIFICATE, "certificate");
  g_object_class_override_property (gobject_class, PROP_CONN_PEER_CERTIFICATE, "peer-certificate");
  g_object_class_override_property (gobject_class, PROP_CONN_PEER_CERTIFICATE_ERRORS, "peer-certificate-errors");
  g_object_class_override_property (gobject_class, PROP_CONN_VALIDATION_FLAGS, "validation-flags");
  g_object_class_override_property (gobject_class, PROP_CONN_SERVER_IDENTITY, "server-identity");
  g_object_class_override_property (gobject_class, PROP_CONN_USE_SSL3, "use-ssl3");
  g_object_class_override_property (gobject_class, PROP_CONN_ACCEPTED_CAS, "accepted-cas");
  g_object_class_override_property (gobject_class, PROP_CONN_AUTHENTICATION_MODE, "authentication-mode");
}

static void
g_test_tls_connection_init (GTestTlsConnection *connection)
{
}

static gboolean
g_test_tls_connection_initable_init (GInitable       *initable,
				      GCancellable    *cancellable,
				      GError         **error)
{
  g_set_error_literal (error, G_TLS_ERROR, G_TLS_ERROR_UNAVAILABLE,
		       "TLS Connection support is not available");
  return FALSE;
}

static void
g_test_tls_connection_initable_iface_init (GInitableIface  *iface)
{
  iface->init = g_test_tls_connection_initable_init;
}

const gchar *
g_test_tls_connection_get_private_key_pem (GTlsCertificate *cert)
{
  return ((GTestTlsCertificate *)cert)->key_pem;
}
