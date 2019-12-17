/* GDBus - GLib D-Bus Library
 *
 * Copyright (C) 2008-2010 Red Hat, Inc.
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
 * Author: David Zeuthen <davidz@redhat.com>
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "giotypes.h"
#include "gioerror.h"
#include "gdbusaddress.h"
#include "gdbusutils.h"
#include "gdbusconnection.h"
#include "gdbusserver.h"
#include "gioenumtypes.h"
#include "gdbusprivate.h"
#include "gdbusauthobserver.h"
#include "ginitable.h"
#include "gsocketservice.h"
#include "gthreadedsocketservice.h"
#include "gresolver.h"
#include "glib/gstdio.h"
#include "ginetaddress.h"
#include "ginetsocketaddress.h"
#include "ginputstream.h"
#include "giostream.h"

#ifdef G_OS_UNIX
#include <unistd.h>
#endif
#ifdef G_OS_WIN32
#include <io.h>
#endif

#ifdef G_OS_UNIX
#include "gunixsocketaddress.h"
#endif

#include "glibintl.h"

/**
 * SECTION:gdbusserver
 * @short_description: Helper for accepting connections
 * @include: gio/gio.h
 *
 * #GDBusServer is a helper for listening to and accepting D-Bus
 * connections. This can be used to create a new D-Bus server, allowing two
 * peers to use the D-Bus protocol for their own specialized communication.
 * A server instance provided in this way will not perform message routing or
 * implement the org.freedesktop.DBus interface.
 *
 * To just export an object on a well-known name on a message bus, such as the
 * session or system bus, you should instead use g_bus_own_name().
 *
 * An example of peer-to-peer communication with G-DBus can be found
 * in [gdbus-example-peer.c](https://git.gnome.org/browse/glib/tree/gio/tests/gdbus-example-peer.c).
 */

/**
 * GDBusServer:
 *
 * The #GDBusServer structure contains only private data and
 * should only be accessed using the provided API.
 *
 * Since: 2.26
 */
struct _GDBusServer
{
  /*< private >*/
  GObject parent_instance;

  GDBusServerFlags flags;
  gchar *address;
  gchar *guid;

  guchar *nonce;
  gchar *nonce_file;

  gchar *client_address;

  GSocketListener *listener;
  gboolean is_using_listener;
  gulong run_signal_handler_id;

  /* The result of g_main_context_ref_thread_default() when the object
   * was created (the GObject _init() function) - this is used for delivery
   * of the :new-connection GObject signal.
   */
  GMainContext *main_context_at_construction;

  gboolean active;

  GDBusAuthObserver *authentication_observer;
};

typedef struct _GDBusServerClass GDBusServerClass;

/**
 * GDBusServerClass:
 * @new_connection: Signal class handler for the #GDBusServer::new-connection signal.
 *
 * Class structure for #GDBusServer.
 *
 * Since: 2.26
 */
struct _GDBusServerClass
{
  /*< private >*/
  GObjectClass parent_class;

  /*< public >*/
  /* Signals */
  gboolean (*new_connection) (GDBusServer      *server,
                              GDBusConnection  *connection);
};

enum
{
  PROP_0,
  PROP_ADDRESS,
  PROP_CLIENT_ADDRESS,
  PROP_FLAGS,
  PROP_GUID,
  PROP_ACTIVE,
  PROP_AUTHENTICATION_OBSERVER,
};

enum
{
  NEW_CONNECTION_SIGNAL,
  LAST_SIGNAL,
};

static guint _signals[LAST_SIGNAL] = {0};

static void initable_iface_init       (GInitableIface *initable_iface);

G_DEFINE_TYPE_WITH_CODE (GDBusServer, g_dbus_server, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, initable_iface_init))

static void
g_dbus_server_finalize (GObject *object)
{
  GDBusServer *server = G_DBUS_SERVER (object);

  if (server->authentication_observer != NULL)
    g_object_unref (server->authentication_observer);

  if (server->run_signal_handler_id > 0)
    g_signal_handler_disconnect (server->listener, server->run_signal_handler_id);

  if (server->listener != NULL)
    g_object_unref (server->listener);

  g_free (server->address);
  g_free (server->guid);
  g_free (server->client_address);
  if (server->nonce != NULL)
    {
      memset (server->nonce, '\0', 16);
      g_free (server->nonce);
    }
  /* we could unlink the nonce file but I don't
   * think it's really worth the effort/risk
   */
  g_free (server->nonce_file);

  g_main_context_unref (server->main_context_at_construction);

  G_OBJECT_CLASS (g_dbus_server_parent_class)->finalize (object);
}

static void
g_dbus_server_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  GDBusServer *server = G_DBUS_SERVER (object);

  switch (prop_id)
    {
    case PROP_FLAGS:
      g_value_set_flags (value, server->flags);
      break;

    case PROP_GUID:
      g_value_set_string (value, server->guid);
      break;

    case PROP_ADDRESS:
      g_value_set_string (value, server->address);
      break;

    case PROP_CLIENT_ADDRESS:
      g_value_set_string (value, server->client_address);
      break;

    case PROP_ACTIVE:
      g_value_set_boolean (value, server->active);
      break;

    case PROP_AUTHENTICATION_OBSERVER:
      g_value_set_object (value, server->authentication_observer);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
g_dbus_server_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GDBusServer *server = G_DBUS_SERVER (object);

  switch (prop_id)
    {
    case PROP_FLAGS:
      server->flags = g_value_get_flags (value);
      break;

    case PROP_GUID:
      server->guid = g_value_dup_string (value);
      break;

    case PROP_ADDRESS:
      server->address = g_value_dup_string (value);
      break;

    case PROP_AUTHENTICATION_OBSERVER:
      server->authentication_observer = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
g_dbus_server_class_init (GDBusServerClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize     = g_dbus_server_finalize;
  gobject_class->set_property = g_dbus_server_set_property;
  gobject_class->get_property = g_dbus_server_get_property;

  /**
   * GDBusServer:flags:
   *
   * Flags from the #GDBusServerFlags enumeration.
   *
   * Since: 2.26
   */
  g_object_class_install_property (gobject_class,
                                   PROP_FLAGS,
                                   g_param_spec_flags ("flags",
                                                       P_("Flags"),
                                                       P_("Flags for the server"),
                                                       G_TYPE_DBUS_SERVER_FLAGS,
                                                       G_DBUS_SERVER_FLAGS_NONE,
                                                       G_PARAM_READABLE |
                                                       G_PARAM_WRITABLE |
                                                       G_PARAM_CONSTRUCT_ONLY |
                                                       G_PARAM_STATIC_NAME |
                                                       G_PARAM_STATIC_BLURB |
                                                       G_PARAM_STATIC_NICK));

  /**
   * GDBusServer:guid:
   *
   * The guid of the server.
   *
   * Since: 2.26
   */
  g_object_class_install_property (gobject_class,
                                   PROP_GUID,
                                   g_param_spec_string ("guid",
                                                        P_("GUID"),
                                                        P_("The guid of the server"),
                                                        NULL,
                                                        G_PARAM_READABLE |
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_BLURB |
                                                        G_PARAM_STATIC_NICK));

  /**
   * GDBusServer:address:
   *
   * The D-Bus address to listen on.
   *
   * Since: 2.26
   */
  g_object_class_install_property (gobject_class,
                                   PROP_ADDRESS,
                                   g_param_spec_string ("address",
                                                        P_("Address"),
                                                        P_("The address to listen on"),
                                                        NULL,
                                                        G_PARAM_READABLE |
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_BLURB |
                                                        G_PARAM_STATIC_NICK));

  /**
   * GDBusServer:client-address:
   *
   * The D-Bus address that clients can use.
   *
   * Since: 2.26
   */
  g_object_class_install_property (gobject_class,
                                   PROP_CLIENT_ADDRESS,
                                   g_param_spec_string ("client-address",
                                                        P_("Client Address"),
                                                        P_("The address clients can use"),
                                                        NULL,
                                                        G_PARAM_READABLE |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_BLURB |
                                                        G_PARAM_STATIC_NICK));

  /**
   * GDBusServer:active:
   *
   * Whether the server is currently active.
   *
   * Since: 2.26
   */
  g_object_class_install_property (gobject_class,
                                   PROP_ACTIVE,
                                   g_param_spec_boolean ("active",
                                                         P_("Active"),
                                                         P_("Whether the server is currently active"),
                                                         FALSE,
                                                         G_PARAM_READABLE |
                                                         G_PARAM_STATIC_NAME |
                                                         G_PARAM_STATIC_BLURB |
                                                         G_PARAM_STATIC_NICK));

  /**
   * GDBusServer:authentication-observer:
   *
   * A #GDBusAuthObserver object to assist in the authentication process or %NULL.
   *
   * Since: 2.26
   */
  g_object_class_install_property (gobject_class,
                                   PROP_AUTHENTICATION_OBSERVER,
                                   g_param_spec_object ("authentication-observer",
                                                        P_("Authentication Observer"),
                                                        P_("Object used to assist in the authentication process"),
                                                        G_TYPE_DBUS_AUTH_OBSERVER,
                                                        G_PARAM_READABLE |
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_BLURB |
                                                        G_PARAM_STATIC_NICK));

  /**
   * GDBusServer::new-connection:
   * @server: The #GDBusServer emitting the signal.
   * @connection: A #GDBusConnection for the new connection.
   *
   * Emitted when a new authenticated connection has been made. Use
   * g_dbus_connection_get_peer_credentials() to figure out what
   * identity (if any), was authenticated.
   *
   * If you want to accept the connection, take a reference to the
   * @connection object and return %TRUE. When you are done with the
   * connection call g_dbus_connection_close() and give up your
   * reference. Note that the other peer may disconnect at any time -
   * a typical thing to do when accepting a connection is to listen to
   * the #GDBusConnection::closed signal.
   *
   * If #GDBusServer:flags contains %G_DBUS_SERVER_FLAGS_RUN_IN_THREAD
   * then the signal is emitted in a new thread dedicated to the
   * connection. Otherwise the signal is emitted in the
   * [thread-default main context][g-main-context-push-thread-default]
   * of the thread that @server was constructed in.
   *
   * You are guaranteed that signal handlers for this signal runs
   * before incoming messages on @connection are processed. This means
   * that it's suitable to call g_dbus_connection_register_object() or
   * similar from the signal handler.
   *
   * Returns: %TRUE to claim @connection, %FALSE to let other handlers
   * run.
   *
   * Since: 2.26
   */
  _signals[NEW_CONNECTION_SIGNAL] = g_signal_new (I_("new-connection"),
                                                  G_TYPE_DBUS_SERVER,
                                                  G_SIGNAL_RUN_LAST,
                                                  G_STRUCT_OFFSET (GDBusServerClass, new_connection),
                                                  g_signal_accumulator_true_handled,
                                                  NULL, /* accu_data */
                                                  NULL,
                                                  G_TYPE_BOOLEAN,
                                                  1,
                                                  G_TYPE_DBUS_CONNECTION);
}

static void
g_dbus_server_init (GDBusServer *server)
{
  server->main_context_at_construction = g_main_context_ref_thread_default ();
}

static gboolean
on_run (GSocketService    *service,
        GSocketConnection *socket_connection,
        GObject           *source_object,
        gpointer           user_data);

/**
 * g_dbus_server_new_sync:
 * @address: A D-Bus address.
 * @flags: Flags from the #GDBusServerFlags enumeration.
 * @guid: A D-Bus GUID.
 * @observer: (nullable): A #GDBusAuthObserver or %NULL.
 * @cancellable: (nullable): A #GCancellable or %NULL.
 * @error: Return location for server or %NULL.
 *
 * Creates a new D-Bus server that listens on the first address in
 * @address that works.
 *
 * Once constructed, you can use g_dbus_server_get_client_address() to
 * get a D-Bus address string that clients can use to connect.
 *
 * Connect to the #GDBusServer::new-connection signal to handle
 * incoming connections.
 *
 * The returned #GDBusServer isn't active - you have to start it with
 * g_dbus_server_start().
 *
 * #GDBusServer is used in this [example][gdbus-peer-to-peer].
 *
 * This is a synchronous failable constructor. See
 * g_dbus_server_new() for the asynchronous version.
 *
 * Returns: A #GDBusServer or %NULL if @error is set. Free with
 * g_object_unref().
 *
 * Since: 2.26
 */
GDBusServer *
g_dbus_server_new_sync (const gchar        *address,
                        GDBusServerFlags    flags,
                        const gchar        *guid,
                        GDBusAuthObserver  *observer,
                        GCancellable       *cancellable,
                        GError            **error)
{
  GDBusServer *server;

  g_return_val_if_fail (address != NULL, NULL);
  g_return_val_if_fail (g_dbus_is_guid (guid), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  server = g_initable_new (G_TYPE_DBUS_SERVER,
                           cancellable,
                           error,
                           "address", address,
                           "flags", flags,
                           "guid", guid,
                           "authentication-observer", observer,
                           NULL);

  return server;
}

/**
 * g_dbus_server_get_client_address:
 * @server: A #GDBusServer.
 *
 * Gets a
 * [D-Bus address](https://dbus.freedesktop.org/doc/dbus-specification.html#addresses)
 * string that can be used by clients to connect to @server.
 *
 * Returns: A D-Bus address string. Do not free, the string is owned
 * by @server.
 *
 * Since: 2.26
 */
const gchar *
g_dbus_server_get_client_address (GDBusServer *server)
{
  g_return_val_if_fail (G_IS_DBUS_SERVER (server), NULL);
  return server->client_address;
}

/**
 * g_dbus_server_get_guid:
 * @server: A #GDBusServer.
 *
 * Gets the GUID for @server.
 *
 * Returns: A D-Bus GUID. Do not free this string, it is owned by @server.
 *
 * Since: 2.26
 */
const gchar *
g_dbus_server_get_guid (GDBusServer *server)
{
  g_return_val_if_fail (G_IS_DBUS_SERVER (server), NULL);
  return server->guid;
}

/**
 * g_dbus_server_get_flags:
 * @server: A #GDBusServer.
 *
 * Gets the flags for @server.
 *
 * Returns: A set of flags from the #GDBusServerFlags enumeration.
 *
 * Since: 2.26
 */
GDBusServerFlags
g_dbus_server_get_flags (GDBusServer *server)
{
  g_return_val_if_fail (G_IS_DBUS_SERVER (server), G_DBUS_SERVER_FLAGS_NONE);
  return server->flags;
}

/**
 * g_dbus_server_is_active:
 * @server: A #GDBusServer.
 *
 * Gets whether @server is active.
 *
 * Returns: %TRUE if server is active, %FALSE otherwise.
 *
 * Since: 2.26
 */
gboolean
g_dbus_server_is_active (GDBusServer *server)
{
  g_return_val_if_fail (G_IS_DBUS_SERVER (server), G_DBUS_SERVER_FLAGS_NONE);
  return server->active;
}

/**
 * g_dbus_server_start:
 * @server: A #GDBusServer.
 *
 * Starts @server.
 *
 * Since: 2.26
 */
void
g_dbus_server_start (GDBusServer *server)
{
  g_return_if_fail (G_IS_DBUS_SERVER (server));
  if (server->active)
    return;
  /* Right now we don't have any transport not using the listener... */
  g_assert (server->is_using_listener);
  g_socket_service_start (G_SOCKET_SERVICE (server->listener));
  server->active = TRUE;
  g_object_notify (G_OBJECT (server), "active");
}

/**
 * g_dbus_server_stop:
 * @server: A #GDBusServer.
 *
 * Stops @server.
 *
 * Since: 2.26
 */
void
g_dbus_server_stop (GDBusServer *server)
{
  g_return_if_fail (G_IS_DBUS_SERVER (server));
  if (!server->active)
    return;
  /* Right now we don't have any transport not using the listener... */
  g_assert (server->is_using_listener);
  g_assert (server->run_signal_handler_id > 0);
  g_signal_handler_disconnect (server->listener, server->run_signal_handler_id);
  server->run_signal_handler_id = 0;
  g_socket_service_stop (G_SOCKET_SERVICE (server->listener));
  server->active = FALSE;
  g_object_notify (G_OBJECT (server), "active");
}

/* ---------------------------------------------------------------------------------------------------- */

#ifdef G_OS_UNIX

static gint
random_ascii (void)
{
  gint ret;
  ret = g_random_int_range (0, 60);
  if (ret < 25)
    ret += 'A';
  else if (ret < 50)
    ret += 'a' - 25;
  else
    ret += '0' - 50;
  return ret;
}

/* note that address_entry has already been validated => exactly one of path, tmpdir or abstract keys are set */
static gboolean
try_unix (GDBusServer  *server,
          const gchar  *address_entry,
          GHashTable   *key_value_pairs,
          GError      **error)
{
  gboolean ret;
  const gchar *path;
  const gchar *tmpdir;
  const gchar *abstract;
  GSocketAddress *address;

  ret = FALSE;
  address = NULL;

  path = g_hash_table_lookup (key_value_pairs, "path");
  tmpdir = g_hash_table_lookup (key_value_pairs, "tmpdir");
  abstract = g_hash_table_lookup (key_value_pairs, "abstract");

  if (path != NULL)
    {
      address = g_unix_socket_address_new (path);
    }
  else if (tmpdir != NULL)
    {
      gint n;
      GString *s;
      GError *local_error;

    retry:
      s = g_string_new (tmpdir);
      g_string_append (s, "/dbus-");
      for (n = 0; n < 8; n++)
        g_string_append_c (s, random_ascii ());

      /* prefer abstract namespace if available */
      if (g_unix_socket_address_abstract_names_supported ())
        address = g_unix_socket_address_new_with_type (s->str,
                                                       -1,
                                                       G_UNIX_SOCKET_ADDRESS_ABSTRACT);
      else
        address = g_unix_socket_address_new (s->str);
      g_string_free (s, TRUE);

      local_error = NULL;
      if (!g_socket_listener_add_address (server->listener,
                                          address,
                                          G_SOCKET_TYPE_STREAM,
                                          G_SOCKET_PROTOCOL_DEFAULT,
                                          NULL, /* source_object */
                                          NULL, /* effective_address */
                                          &local_error))
        {
          if (local_error->domain == G_IO_ERROR && local_error->code == G_IO_ERROR_ADDRESS_IN_USE)
            {
              g_error_free (local_error);
              goto retry;
            }
          g_propagate_error (error, local_error);
          goto out;
        }
      ret = TRUE;
      goto out;
    }
  else if (abstract != NULL)
    {
      if (!g_unix_socket_address_abstract_names_supported ())
        {
          g_set_error_literal (error,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_SUPPORTED,
                               _("Abstract name space not supported"));
          goto out;
        }
      address = g_unix_socket_address_new_with_type (abstract,
                                                     -1,
                                                     G_UNIX_SOCKET_ADDRESS_ABSTRACT);
    }
  else
    {
      g_assert_not_reached ();
    }

  if (!g_socket_listener_add_address (server->listener,
                                      address,
                                      G_SOCKET_TYPE_STREAM,
                                      G_SOCKET_PROTOCOL_DEFAULT,
                                      NULL, /* source_object */
                                      NULL, /* effective_address */
                                      error))
    goto out;

  ret = TRUE;

 out:

  if (address != NULL)
    {
      /* Fill out client_address if the connection attempt worked */
      if (ret)
        {
          server->is_using_listener = TRUE;

          switch (g_unix_socket_address_get_address_type (G_UNIX_SOCKET_ADDRESS (address)))
            {
            case G_UNIX_SOCKET_ADDRESS_ABSTRACT:
              server->client_address = g_strdup_printf ("unix:abstract=%s",
                                                        g_unix_socket_address_get_path (G_UNIX_SOCKET_ADDRESS (address)));
              break;

            case G_UNIX_SOCKET_ADDRESS_PATH:
              server->client_address = g_strdup_printf ("unix:path=%s",
                                                        g_unix_socket_address_get_path (G_UNIX_SOCKET_ADDRESS (address)));
              break;

            default:
              g_assert_not_reached ();
              break;
            }
        }
      g_object_unref (address);
    }
  return ret;
}
#endif

/* ---------------------------------------------------------------------------------------------------- */

/* note that address_entry has already been validated =>
 *  both host and port (guaranteed to be a number in [0, 65535]) are set (family is optional)
 */
static gboolean
try_tcp (GDBusServer  *server,
         const gchar  *address_entry,
         GHashTable   *key_value_pairs,
         gboolean      do_nonce,
         GError      **error)
{
  gboolean ret;
  const gchar *host;
  const gchar *port;
  gint port_num;
  GResolver *resolver;
  GList *resolved_addresses;
  GList *l;

  ret = FALSE;
  resolver = NULL;
  resolved_addresses = NULL;

  host = g_hash_table_lookup (key_value_pairs, "host");
  port = g_hash_table_lookup (key_value_pairs, "port");
  /* family = g_hash_table_lookup (key_value_pairs, "family"); */
  if (g_hash_table_lookup (key_value_pairs, "noncefile") != NULL)
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_INVALID_ARGUMENT,
                           _("Cannot specify nonce file when creating a server"));
      goto out;
    }

  if (host == NULL)
    host = "localhost";
  if (port == NULL)
    port = "0";
  port_num = strtol (port, NULL, 10);

  resolver = g_resolver_get_default ();
  resolved_addresses = g_resolver_lookup_by_name (resolver,
                                                  host,
                                                  NULL,
                                                  error);
  if (resolved_addresses == NULL)
    goto out;

  /* TODO: handle family */
  for (l = resolved_addresses; l != NULL; l = l->next)
    {
      GInetAddress *address = G_INET_ADDRESS (l->data);
      GSocketAddress *socket_address;
      GSocketAddress *effective_address;

      socket_address = g_inet_socket_address_new (address, port_num);
      if (!g_socket_listener_add_address (server->listener,
                                          socket_address,
                                          G_SOCKET_TYPE_STREAM,
                                          G_SOCKET_PROTOCOL_TCP,
                                          NULL, /* GObject *source_object */
                                          &effective_address,
                                          error))
        {
          g_object_unref (socket_address);
          goto out;
        }
      if (port_num == 0)
        /* make sure we allocate the same port number for other listeners */
        port_num = g_inet_socket_address_get_port (G_INET_SOCKET_ADDRESS (effective_address));

      g_object_unref (effective_address);
      g_object_unref (socket_address);
    }

  if (do_nonce)
    {
      gint fd;
      guint n;
      gsize bytes_written;
      gsize bytes_remaining;
      char *file_escaped;

      server->nonce = g_new0 (guchar, 16);
      for (n = 0; n < 16; n++)
        server->nonce[n] = g_random_int_range (0, 256);
      fd = g_file_open_tmp ("gdbus-nonce-file-XXXXXX",
                            &server->nonce_file,
                            error);
      if (fd == -1)
        {
          g_socket_listener_close (server->listener);
          goto out;
        }
    again:
      bytes_written = 0;
      bytes_remaining = 16;
      while (bytes_remaining > 0)
        {
          gssize ret;
          int errsv;

          ret = write (fd, server->nonce + bytes_written, bytes_remaining);
          errsv = errno;
          if (ret == -1)
            {
              if (errsv == EINTR)
                goto again;
              g_set_error (error,
                           G_IO_ERROR,
                           g_io_error_from_errno (errsv),
                           _("Error writing nonce file at “%s”: %s"),
                           server->nonce_file,
                           g_strerror (errsv));
              goto out;
            }
          bytes_written += ret;
          bytes_remaining -= ret;
        }
      if (!g_close (fd, error))
        goto out;
      file_escaped = g_uri_escape_string (server->nonce_file, "/\\", FALSE);
      server->client_address = g_strdup_printf ("nonce-tcp:host=%s,port=%d,noncefile=%s",
                                                host,
                                                port_num,
                                                file_escaped);
      g_free (file_escaped);
    }
  else
    {
      server->client_address = g_strdup_printf ("tcp:host=%s,port=%d", host, port_num);
    }
  server->is_using_listener = TRUE;
  ret = TRUE;

 out:
  g_list_free_full (resolved_addresses, g_object_unref);
  if (resolver)
    g_object_unref (resolver);
  return ret;
}

/* ---------------------------------------------------------------------------------------------------- */

typedef struct
{
  GDBusServer *server;
  GDBusConnection *connection;
} EmitIdleData;

static void
emit_idle_data_free (EmitIdleData *data)
{
  g_object_unref (data->server);
  g_object_unref (data->connection);
  g_free (data);
}

static gboolean
emit_new_connection_in_idle (gpointer user_data)
{
  EmitIdleData *data = user_data;
  gboolean claimed;

  claimed = FALSE;
  g_signal_emit (data->server,
                 _signals[NEW_CONNECTION_SIGNAL],
                 0,
                 data->connection,
                 &claimed);

  if (claimed)
    g_dbus_connection_start_message_processing (data->connection);
  g_object_unref (data->connection);

  return FALSE;
}

/* Called in new thread */
static gboolean
on_run (GSocketService    *service,
        GSocketConnection *socket_connection,
        GObject           *source_object,
        gpointer           user_data)
{
  GDBusServer *server = G_DBUS_SERVER (user_data);
  GDBusConnection *connection;
  GDBusConnectionFlags connection_flags;

  if (server->nonce != NULL)
    {
      gchar buf[16];
      gsize bytes_read;

      if (!g_input_stream_read_all (g_io_stream_get_input_stream (G_IO_STREAM (socket_connection)),
                                    buf,
                                    16,
                                    &bytes_read,
                                    NULL,  /* GCancellable */
                                    NULL)) /* GError */
        goto out;

      if (bytes_read != 16)
        goto out;

      if (memcmp (buf, server->nonce, 16) != 0)
        goto out;
    }

  connection_flags =
    G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_SERVER |
    G_DBUS_CONNECTION_FLAGS_DELAY_MESSAGE_PROCESSING;
  if (server->flags & G_DBUS_SERVER_FLAGS_AUTHENTICATION_ALLOW_ANONYMOUS)
    connection_flags |= G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_ALLOW_ANONYMOUS;

  connection = g_dbus_connection_new_sync (G_IO_STREAM (socket_connection),
                                           server->guid,
                                           connection_flags,
                                           server->authentication_observer,
                                           NULL,  /* GCancellable */
                                           NULL); /* GError */
  if (connection == NULL)
      goto out;

  if (server->flags & G_DBUS_SERVER_FLAGS_RUN_IN_THREAD)
    {
      gboolean claimed;

      claimed = FALSE;
      g_signal_emit (server,
                     _signals[NEW_CONNECTION_SIGNAL],
                     0,
                     connection,
                     &claimed);
      if (claimed)
        g_dbus_connection_start_message_processing (connection);
      g_object_unref (connection);
    }
  else
    {
      GSource *idle_source;
      EmitIdleData *data;

      data = g_new0 (EmitIdleData, 1);
      data->server = g_object_ref (server);
      data->connection = g_object_ref (connection);

      idle_source = g_idle_source_new ();
      g_source_set_priority (idle_source, G_PRIORITY_DEFAULT);
      g_source_set_callback (idle_source,
                             emit_new_connection_in_idle,
                             data,
                             (GDestroyNotify) emit_idle_data_free);
      g_source_set_name (idle_source, "[gio] emit_new_connection_in_idle");
      g_source_attach (idle_source, server->main_context_at_construction);
      g_source_unref (idle_source);
    }

 out:
  return TRUE;
}

static gboolean
initable_init (GInitable     *initable,
               GCancellable  *cancellable,
               GError       **error)
{
  GDBusServer *server = G_DBUS_SERVER (initable);
  gboolean ret;
  guint n;
  gchar **addr_array;
  GError *last_error;

  ret = FALSE;
  addr_array = NULL;
  last_error = NULL;

  if (!g_dbus_is_guid (server->guid))
    {
      g_set_error (&last_error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_ARGUMENT,
                   _("The string “%s” is not a valid D-Bus GUID"),
                   server->guid);
      goto out;
    }

  server->listener = G_SOCKET_LISTENER (g_threaded_socket_service_new (-1));

  addr_array = g_strsplit (server->address, ";", 0);
  last_error = NULL;
  for (n = 0; addr_array != NULL && addr_array[n] != NULL; n++)
    {
      const gchar *address_entry = addr_array[n];
      GHashTable *key_value_pairs;
      gchar *transport_name;
      GError *this_error;

      this_error = NULL;
      if (g_dbus_is_supported_address (address_entry,
                                       &this_error) &&
          _g_dbus_address_parse_entry (address_entry,
                                       &transport_name,
                                       &key_value_pairs,
                                       &this_error))
        {

          if (FALSE)
            {
            }
#ifdef G_OS_UNIX
          else if (g_strcmp0 (transport_name, "unix") == 0)
            ret = try_unix (server, address_entry, key_value_pairs, &this_error);
#endif
          else if (g_strcmp0 (transport_name, "tcp") == 0)
            ret = try_tcp (server, address_entry, key_value_pairs, FALSE, &this_error);
          else if (g_strcmp0 (transport_name, "nonce-tcp") == 0)
            ret = try_tcp (server, address_entry, key_value_pairs, TRUE, &this_error);
          else
            g_set_error (&this_error,
                         G_IO_ERROR,
                         G_IO_ERROR_INVALID_ARGUMENT,
                         _("Cannot listen on unsupported transport “%s”"),
                         transport_name);

          g_free (transport_name);
          if (key_value_pairs != NULL)
            g_hash_table_unref (key_value_pairs);

          if (ret)
            {
              g_assert (this_error == NULL);
              goto out;
            }
        }

      if (this_error != NULL)
        {
          if (last_error != NULL)
            g_error_free (last_error);
          last_error = this_error;
        }
    }

 out:

  g_strfreev (addr_array);

  if (ret)
    {
      if (last_error != NULL)
        g_error_free (last_error);

      /* Right now we don't have any transport not using the listener... */
      g_assert (server->is_using_listener);
      server->run_signal_handler_id = g_signal_connect (G_SOCKET_SERVICE (server->listener),
                                                        "run",
                                                        G_CALLBACK (on_run),
                                                        server);
    }
  else
    {
      g_assert (last_error != NULL);
      g_propagate_error (error, last_error);
    }
  return ret;
}


static void
initable_iface_init (GInitableIface *initable_iface)
{
  initable_iface->init = initable_init;
}

/* ---------------------------------------------------------------------------------------------------- */
