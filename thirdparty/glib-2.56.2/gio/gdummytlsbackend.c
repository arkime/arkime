/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright (C) 2010 Red Hat, Inc.
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

#include "gdummytlsbackend.h"

#include <glib.h>

#include "gasyncresult.h"
#include "gcancellable.h"
#include "ginitable.h"
#include "gdtlsclientconnection.h"
#include "gdtlsconnection.h"
#include "gdtlsserverconnection.h"
#include "gtlsbackend.h"
#include "gtlscertificate.h"
#include "gtlsclientconnection.h"
#include "gtlsdatabase.h"
#include "gtlsfiledatabase.h"
#include "gtlsserverconnection.h"

#include "giomodule.h"
#include "giomodule-priv.h"

#include "glibintl.h"

static GType _g_dummy_tls_certificate_get_type (void);
static GType _g_dummy_tls_connection_get_type (void);
static GType _g_dummy_dtls_connection_get_type (void);
static GType _g_dummy_tls_database_get_type (void);

struct _GDummyTlsBackend {
  GObject       parent_instance;
  GTlsDatabase *database;
};

static void g_dummy_tls_backend_iface_init (GTlsBackendInterface *iface);

#define g_dummy_tls_backend_get_type _g_dummy_tls_backend_get_type
G_DEFINE_TYPE_WITH_CODE (GDummyTlsBackend, g_dummy_tls_backend, G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (G_TYPE_TLS_BACKEND,
						g_dummy_tls_backend_iface_init)
			 _g_io_modules_ensure_extension_points_registered ();
			 g_io_extension_point_implement (G_TLS_BACKEND_EXTENSION_POINT_NAME,
							 g_define_type_id,
							 "dummy",
							 -100);)

static void
g_dummy_tls_backend_init (GDummyTlsBackend *dummy)
{
}

static void
g_dummy_tls_backend_finalize (GObject *object)
{
  GDummyTlsBackend *dummy = G_DUMMY_TLS_BACKEND (object);

  g_clear_object (&dummy->database);

  G_OBJECT_CLASS (g_dummy_tls_backend_parent_class)->finalize (object);
}

static void
g_dummy_tls_backend_class_init (GDummyTlsBackendClass *backend_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (backend_class);

  object_class->finalize = g_dummy_tls_backend_finalize;
}

static GTlsDatabase *
g_dummy_tls_backend_get_default_database (GTlsBackend *backend)
{
  GDummyTlsBackend *dummy = G_DUMMY_TLS_BACKEND (backend);

  if (g_once_init_enter (&dummy->database))
    {
      GTlsDatabase *tlsdb;

      tlsdb = g_object_new (_g_dummy_tls_database_get_type (), NULL);
      g_once_init_leave (&dummy->database, tlsdb);
    }

  return g_object_ref (dummy->database);
}

static void
g_dummy_tls_backend_iface_init (GTlsBackendInterface *iface)
{
  iface->get_certificate_type = _g_dummy_tls_certificate_get_type;
  iface->get_client_connection_type = _g_dummy_tls_connection_get_type;
  iface->get_server_connection_type = _g_dummy_tls_connection_get_type;
  iface->get_dtls_client_connection_type = _g_dummy_dtls_connection_get_type;
  iface->get_dtls_server_connection_type = _g_dummy_dtls_connection_get_type;
  iface->get_file_database_type = _g_dummy_tls_database_get_type;
  iface->get_default_database = g_dummy_tls_backend_get_default_database;
}

/* Dummy certificate type */

typedef struct _GDummyTlsCertificate      GDummyTlsCertificate;
typedef struct _GDummyTlsCertificateClass GDummyTlsCertificateClass;

struct _GDummyTlsCertificate {
  GTlsCertificate parent_instance;
};

struct _GDummyTlsCertificateClass {
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

static void g_dummy_tls_certificate_initable_iface_init (GInitableIface *iface);

#define g_dummy_tls_certificate_get_type _g_dummy_tls_certificate_get_type
G_DEFINE_TYPE_WITH_CODE (GDummyTlsCertificate, g_dummy_tls_certificate, G_TYPE_TLS_CERTIFICATE,
			 G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
						g_dummy_tls_certificate_initable_iface_init))

static void
g_dummy_tls_certificate_get_property (GObject    *object,
				      guint       prop_id,
				      GValue     *value,
				      GParamSpec *pspec)
{
  /* We need to define this method to make GObject happy, but it will
   * never be possible to construct a working GDummyTlsCertificate, so
   * it doesn't have to do anything useful.
   */
}

static void
g_dummy_tls_certificate_set_property (GObject      *object,
				      guint         prop_id,
				      const GValue *value,
				      GParamSpec   *pspec)
{
  /* Just ignore all attempts to set properties. */
}

static void
g_dummy_tls_certificate_class_init (GDummyTlsCertificateClass *certificate_class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (certificate_class);

  gobject_class->get_property = g_dummy_tls_certificate_get_property;
  gobject_class->set_property = g_dummy_tls_certificate_set_property;

  g_object_class_override_property (gobject_class, PROP_CERT_CERTIFICATE, "certificate");
  g_object_class_override_property (gobject_class, PROP_CERT_CERTIFICATE_PEM, "certificate-pem");
  g_object_class_override_property (gobject_class, PROP_CERT_PRIVATE_KEY, "private-key");
  g_object_class_override_property (gobject_class, PROP_CERT_PRIVATE_KEY_PEM, "private-key-pem");
  g_object_class_override_property (gobject_class, PROP_CERT_ISSUER, "issuer");
}

static void
g_dummy_tls_certificate_init (GDummyTlsCertificate *certificate)
{
}

static gboolean
g_dummy_tls_certificate_initable_init (GInitable       *initable,
				       GCancellable    *cancellable,
				       GError         **error)
{
  g_set_error_literal (error, G_TLS_ERROR, G_TLS_ERROR_UNAVAILABLE,
		       _("TLS support is not available"));
  return FALSE;
}

static void
g_dummy_tls_certificate_initable_iface_init (GInitableIface  *iface)
{
  iface->init = g_dummy_tls_certificate_initable_init;
}

/* Dummy connection type; since GTlsClientConnection and
 * GTlsServerConnection are just interfaces, we can implement them
 * both on a single object.
 */

typedef struct _GDummyTlsConnection      GDummyTlsConnection;
typedef struct _GDummyTlsConnectionClass GDummyTlsConnectionClass;

struct _GDummyTlsConnection {
  GTlsConnection parent_instance;
};

struct _GDummyTlsConnectionClass {
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
  PROP_CONN_DATABASE,
  PROP_CONN_INTERACTION,
  PROP_CONN_PEER_CERTIFICATE,
  PROP_CONN_PEER_CERTIFICATE_ERRORS,
  PROP_CONN_VALIDATION_FLAGS,
  PROP_CONN_SERVER_IDENTITY,
  PROP_CONN_USE_SSL3,
  PROP_CONN_ACCEPTED_CAS,
  PROP_CONN_AUTHENTICATION_MODE
};

static void g_dummy_tls_connection_initable_iface_init (GInitableIface *iface);

#define g_dummy_tls_connection_get_type _g_dummy_tls_connection_get_type
G_DEFINE_TYPE_WITH_CODE (GDummyTlsConnection, g_dummy_tls_connection, G_TYPE_TLS_CONNECTION,
			 G_IMPLEMENT_INTERFACE (G_TYPE_TLS_CLIENT_CONNECTION, NULL)
			 G_IMPLEMENT_INTERFACE (G_TYPE_TLS_SERVER_CONNECTION, NULL)
			 G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
						g_dummy_tls_connection_initable_iface_init))

static void
g_dummy_tls_connection_get_property (GObject    *object,
				     guint       prop_id,
				     GValue     *value,
				     GParamSpec *pspec)
{
}

static void
g_dummy_tls_connection_set_property (GObject      *object,
				     guint         prop_id,
				     const GValue *value,
				     GParamSpec   *pspec)
{
}

static gboolean
g_dummy_tls_connection_close (GIOStream     *stream,
			      GCancellable  *cancellable,
			      GError       **error)
{
  return TRUE;
}

static void
g_dummy_tls_connection_class_init (GDummyTlsConnectionClass *connection_class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (connection_class);
  GIOStreamClass *io_stream_class = G_IO_STREAM_CLASS (connection_class);

  gobject_class->get_property = g_dummy_tls_connection_get_property;
  gobject_class->set_property = g_dummy_tls_connection_set_property;

  /* Need to override this because when initable_init fails it will
   * dispose the connection, which will close it, which would
   * otherwise try to close its input/output streams, which don't
   * exist.
   */
  io_stream_class->close_fn = g_dummy_tls_connection_close;

  g_object_class_override_property (gobject_class, PROP_CONN_BASE_IO_STREAM, "base-io-stream");
  g_object_class_override_property (gobject_class, PROP_CONN_USE_SYSTEM_CERTDB, "use-system-certdb");
  g_object_class_override_property (gobject_class, PROP_CONN_REQUIRE_CLOSE_NOTIFY, "require-close-notify");
  g_object_class_override_property (gobject_class, PROP_CONN_REHANDSHAKE_MODE, "rehandshake-mode");
  g_object_class_override_property (gobject_class, PROP_CONN_CERTIFICATE, "certificate");
  g_object_class_override_property (gobject_class, PROP_CONN_DATABASE, "database");
  g_object_class_override_property (gobject_class, PROP_CONN_INTERACTION, "interaction");
  g_object_class_override_property (gobject_class, PROP_CONN_PEER_CERTIFICATE, "peer-certificate");
  g_object_class_override_property (gobject_class, PROP_CONN_PEER_CERTIFICATE_ERRORS, "peer-certificate-errors");
  g_object_class_override_property (gobject_class, PROP_CONN_VALIDATION_FLAGS, "validation-flags");
  g_object_class_override_property (gobject_class, PROP_CONN_SERVER_IDENTITY, "server-identity");
  g_object_class_override_property (gobject_class, PROP_CONN_USE_SSL3, "use-ssl3");
  g_object_class_override_property (gobject_class, PROP_CONN_ACCEPTED_CAS, "accepted-cas");
  g_object_class_override_property (gobject_class, PROP_CONN_AUTHENTICATION_MODE, "authentication-mode");
}

static void
g_dummy_tls_connection_init (GDummyTlsConnection *connection)
{
}

static gboolean
g_dummy_tls_connection_initable_init (GInitable       *initable,
				      GCancellable    *cancellable,
				      GError         **error)
{
  g_set_error_literal (error, G_TLS_ERROR, G_TLS_ERROR_UNAVAILABLE,
		       _("TLS support is not available"));
  return FALSE;
}

static void
g_dummy_tls_connection_initable_iface_init (GInitableIface  *iface)
{
  iface->init = g_dummy_tls_connection_initable_init;
}

/* Dummy DTLS connection type; since GDtlsClientConnection and
 * GDtlsServerConnection are just interfaces, we can implement them
 * both on a single object.
 */

typedef struct _GDummyDtlsConnection      GDummyDtlsConnection;
typedef struct _GDummyDtlsConnectionClass GDummyDtlsConnectionClass;

struct _GDummyDtlsConnection {
  GObject parent_instance;
};

struct _GDummyDtlsConnectionClass {
  GObjectClass parent_class;
};

enum
{
  PROP_DTLS_CONN_BASE_SOCKET = 1,
  PROP_DTLS_CONN_REQUIRE_CLOSE_NOTIFY,
  PROP_DTLS_CONN_REHANDSHAKE_MODE,
  PROP_DTLS_CONN_CERTIFICATE,
  PROP_DTLS_CONN_DATABASE,
  PROP_DTLS_CONN_INTERACTION,
  PROP_DTLS_CONN_PEER_CERTIFICATE,
  PROP_DTLS_CONN_PEER_CERTIFICATE_ERRORS,
  PROP_DTLS_CONN_VALIDATION_FLAGS,
  PROP_DTLS_CONN_SERVER_IDENTITY,
  PROP_DTLS_CONN_ENABLE_NEGOTIATION,
  PROP_DTLS_CONN_ACCEPTED_CAS,
  PROP_DTLS_CONN_AUTHENTICATION_MODE,
};

static void g_dummy_dtls_connection_initable_iface_init (GInitableIface *iface);

#define g_dummy_dtls_connection_get_type _g_dummy_dtls_connection_get_type
G_DEFINE_TYPE_WITH_CODE (GDummyDtlsConnection, g_dummy_dtls_connection, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_DTLS_CONNECTION, NULL);
                         G_IMPLEMENT_INTERFACE (G_TYPE_DTLS_CLIENT_CONNECTION, NULL);
                         G_IMPLEMENT_INTERFACE (G_TYPE_DTLS_SERVER_CONNECTION, NULL);
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                g_dummy_dtls_connection_initable_iface_init);)

static void
g_dummy_dtls_connection_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
}

static void
g_dummy_dtls_connection_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
}

static void
g_dummy_dtls_connection_class_init (GDummyDtlsConnectionClass *connection_class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (connection_class);

  gobject_class->get_property = g_dummy_dtls_connection_get_property;
  gobject_class->set_property = g_dummy_dtls_connection_set_property;

  g_object_class_override_property (gobject_class, PROP_DTLS_CONN_BASE_SOCKET, "base-socket");
  g_object_class_override_property (gobject_class, PROP_DTLS_CONN_REQUIRE_CLOSE_NOTIFY, "require-close-notify");
  g_object_class_override_property (gobject_class, PROP_DTLS_CONN_REHANDSHAKE_MODE, "rehandshake-mode");
  g_object_class_override_property (gobject_class, PROP_DTLS_CONN_CERTIFICATE, "certificate");
  g_object_class_override_property (gobject_class, PROP_DTLS_CONN_DATABASE, "database");
  g_object_class_override_property (gobject_class, PROP_DTLS_CONN_INTERACTION, "interaction");
  g_object_class_override_property (gobject_class, PROP_DTLS_CONN_PEER_CERTIFICATE, "peer-certificate");
  g_object_class_override_property (gobject_class, PROP_DTLS_CONN_PEER_CERTIFICATE_ERRORS, "peer-certificate-errors");
  g_object_class_override_property (gobject_class, PROP_DTLS_CONN_VALIDATION_FLAGS, "validation-flags");
  g_object_class_override_property (gobject_class, PROP_DTLS_CONN_SERVER_IDENTITY, "server-identity");
  g_object_class_override_property (gobject_class, PROP_DTLS_CONN_ACCEPTED_CAS, "accepted-cas");
  g_object_class_override_property (gobject_class, PROP_DTLS_CONN_AUTHENTICATION_MODE, "authentication-mode");
}

static void
g_dummy_dtls_connection_init (GDummyDtlsConnection *connection)
{
}

static gboolean
g_dummy_dtls_connection_initable_init (GInitable       *initable,
                                       GCancellable    *cancellable,
                                       GError         **error)
{
  g_set_error_literal (error, G_TLS_ERROR, G_TLS_ERROR_UNAVAILABLE,
                       _("DTLS support is not available"));
  return FALSE;
}

static void
g_dummy_dtls_connection_initable_iface_init (GInitableIface  *iface)
{
  iface->init = g_dummy_dtls_connection_initable_init;
}

/* Dummy database type.
 */

typedef struct _GDummyTlsDatabase      GDummyTlsDatabase;
typedef struct _GDummyTlsDatabaseClass GDummyTlsDatabaseClass;

struct _GDummyTlsDatabase {
  GTlsDatabase parent_instance;
};

struct _GDummyTlsDatabaseClass {
  GTlsDatabaseClass parent_class;
};

enum
{
  PROP_DATABASE_0,

  PROP_ANCHORS,
};

static void g_dummy_tls_database_file_database_iface_init (GTlsFileDatabaseInterface *iface);
static void g_dummy_tls_database_initable_iface_init (GInitableIface *iface);

#define g_dummy_tls_database_get_type _g_dummy_tls_database_get_type
G_DEFINE_TYPE_WITH_CODE (GDummyTlsDatabase, g_dummy_tls_database, G_TYPE_TLS_DATABASE,
                         G_IMPLEMENT_INTERFACE (G_TYPE_TLS_FILE_DATABASE,
                                                g_dummy_tls_database_file_database_iface_init)
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                g_dummy_tls_database_initable_iface_init))


static void
g_dummy_tls_database_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  /* We need to define this method to make GObject happy, but it will
   * never be possible to construct a working GDummyTlsDatabase, so
   * it doesn't have to do anything useful.
   */
}

static void
g_dummy_tls_database_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  /* Just ignore all attempts to set properties. */
}

static void
g_dummy_tls_database_class_init (GDummyTlsDatabaseClass *database_class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (database_class);

  gobject_class->get_property = g_dummy_tls_database_get_property;
  gobject_class->set_property = g_dummy_tls_database_set_property;

  g_object_class_override_property (gobject_class, PROP_ANCHORS, "anchors");
}

static void
g_dummy_tls_database_init (GDummyTlsDatabase *database)
{
}

static void
g_dummy_tls_database_file_database_iface_init (GTlsFileDatabaseInterface  *iface)
{
}

static gboolean
g_dummy_tls_database_initable_init (GInitable       *initable,
                                    GCancellable    *cancellable,
                                    GError         **error)
{
  g_set_error_literal (error, G_TLS_ERROR, G_TLS_ERROR_UNAVAILABLE,
                       _("TLS support is not available"));
  return FALSE;
}

static void
g_dummy_tls_database_initable_iface_init (GInitableIface  *iface)
{
  iface->init = g_dummy_tls_database_initable_init;
}
