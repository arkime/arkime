/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright © 2008 Christian Kellner, Samuel Cormier-Iijima
 *           © 2008 codethink
 * Copyright © 2009 Red Hat, Inc
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
 * Authors: Christian Kellner <gicmo@gnome.org>
 *          Samuel Cormier-Iijima <sciyoshi@gmail.com>
 *          Ryan Lortie <desrt@desrt.ca>
 *          Alexander Larsson <alexl@redhat.com>
 */

#include "config.h"

#include "gsocketconnection.h"

#include "gsocketoutputstream.h"
#include "gsocketinputstream.h"
#include "gioprivate.h"
#include <gio/giostream.h>
#include <gio/gtask.h>
#include "gunixconnection.h"
#include "gtcpconnection.h"
#include "glibintl.h"


/**
 * SECTION:gsocketconnection
 * @short_description: A socket connection
 * @include: gio/gio.h
 * @see_also: #GIOStream, #GSocketClient, #GSocketListener
 *
 * #GSocketConnection is a #GIOStream for a connected socket. They
 * can be created either by #GSocketClient when connecting to a host,
 * or by #GSocketListener when accepting a new client.
 *
 * The type of the #GSocketConnection object returned from these calls
 * depends on the type of the underlying socket that is in use. For
 * instance, for a TCP/IP connection it will be a #GTcpConnection.
 *
 * Choosing what type of object to construct is done with the socket
 * connection factory, and it is possible for 3rd parties to register
 * custom socket connection types for specific combination of socket
 * family/type/protocol using g_socket_connection_factory_register_type().
 *
 * To close a #GSocketConnection, use g_io_stream_close(). Closing both
 * substreams of the #GIOStream separately will not close the underlying
 * #GSocket.
 *
 * Since: 2.22
 */

enum
{
  PROP_NONE,
  PROP_SOCKET,
};

struct _GSocketConnectionPrivate
{
  GSocket       *socket;
  GInputStream  *input_stream;
  GOutputStream *output_stream;

  GSocketAddress *cached_remote_address;

  gboolean       in_dispose;
};

static gboolean g_socket_connection_close         (GIOStream            *stream,
						   GCancellable         *cancellable,
						   GError              **error);
static void     g_socket_connection_close_async   (GIOStream            *stream,
						   int                   io_priority,
						   GCancellable         *cancellable,
						   GAsyncReadyCallback   callback,
						   gpointer              user_data);
static gboolean g_socket_connection_close_finish  (GIOStream            *stream,
						   GAsyncResult         *result,
						   GError              **error);

G_DEFINE_TYPE_WITH_PRIVATE (GSocketConnection, g_socket_connection, G_TYPE_IO_STREAM)

static GInputStream *
g_socket_connection_get_input_stream (GIOStream *io_stream)
{
  GSocketConnection *connection = G_SOCKET_CONNECTION (io_stream);

  if (connection->priv->input_stream == NULL)
    connection->priv->input_stream = (GInputStream *)
      _g_socket_input_stream_new (connection->priv->socket);

  return connection->priv->input_stream;
}

static GOutputStream *
g_socket_connection_get_output_stream (GIOStream *io_stream)
{
  GSocketConnection *connection = G_SOCKET_CONNECTION (io_stream);

  if (connection->priv->output_stream == NULL)
    connection->priv->output_stream = (GOutputStream *)
      _g_socket_output_stream_new (connection->priv->socket);

  return connection->priv->output_stream;
}

/**
 * g_socket_connection_is_connected:
 * @connection: a #GSocketConnection
 *
 * Checks if @connection is connected. This is equivalent to calling
 * g_socket_is_connected() on @connection's underlying #GSocket.
 *
 * Returns: whether @connection is connected
 *
 * Since: 2.32
 */
gboolean
g_socket_connection_is_connected (GSocketConnection  *connection)
{
  return g_socket_is_connected (connection->priv->socket);
}

/**
 * g_socket_connection_connect:
 * @connection: a #GSocketConnection
 * @address: a #GSocketAddress specifying the remote address.
 * @cancellable: (nullable): a %GCancellable or %NULL
 * @error: #GError for error reporting, or %NULL to ignore.
 *
 * Connect @connection to the specified remote address.
 *
 * Returns: %TRUE if the connection succeeded, %FALSE on error
 *
 * Since: 2.32
 */
gboolean
g_socket_connection_connect (GSocketConnection  *connection,
			     GSocketAddress     *address,
			     GCancellable       *cancellable,
			     GError            **error)
{
  g_return_val_if_fail (G_IS_SOCKET_CONNECTION (connection), FALSE);
  g_return_val_if_fail (G_IS_SOCKET_ADDRESS (address), FALSE);

  return g_socket_connect (connection->priv->socket, address,
			   cancellable, error);
}

static gboolean g_socket_connection_connect_callback (GSocket      *socket,
						      GIOCondition  condition,
						      gpointer      user_data);

/**
 * g_socket_connection_connect_async:
 * @connection: a #GSocketConnection
 * @address: a #GSocketAddress specifying the remote address.
 * @cancellable: (nullable): a %GCancellable or %NULL
 * @callback: (scope async): a #GAsyncReadyCallback
 * @user_data: (closure): user data for the callback
 *
 * Asynchronously connect @connection to the specified remote address.
 *
 * This clears the #GSocket:blocking flag on @connection's underlying
 * socket if it is currently set.
 *
 * Use g_socket_connection_connect_finish() to retrieve the result.
 *
 * Since: 2.32
 */
void
g_socket_connection_connect_async (GSocketConnection   *connection,
				   GSocketAddress      *address,
				   GCancellable        *cancellable,
				   GAsyncReadyCallback  callback,
				   gpointer             user_data)
{
  GTask *task;
  GError *tmp_error = NULL;

  g_return_if_fail (G_IS_SOCKET_CONNECTION (connection));
  g_return_if_fail (G_IS_SOCKET_ADDRESS (address));

  task = g_task_new (connection, cancellable, callback, user_data);
  g_task_set_source_tag (task, g_socket_connection_connect_async);

  g_socket_set_blocking (connection->priv->socket, FALSE);

  if (g_socket_connect (connection->priv->socket, address,
			cancellable, &tmp_error))
    {
      g_task_return_boolean (task, TRUE);
      g_object_unref (task);
    }
  else if (g_error_matches (tmp_error, G_IO_ERROR, G_IO_ERROR_PENDING))
    {
      GSource *source;

      g_error_free (tmp_error);
      source = g_socket_create_source (connection->priv->socket,
				       G_IO_OUT, cancellable);
      g_task_attach_source (task, source,
			    (GSourceFunc) g_socket_connection_connect_callback);
      g_source_unref (source);
    }
  else
    {
      g_task_return_error (task, tmp_error);
      g_object_unref (task);
    }
}

static gboolean
g_socket_connection_connect_callback (GSocket      *socket,
				      GIOCondition  condition,
				      gpointer      user_data)
{
  GTask *task = user_data;
  GSocketConnection *connection = g_task_get_source_object (task);
  GError *error = NULL;

  if (g_socket_check_connect_result (connection->priv->socket, &error))
    g_task_return_boolean (task, TRUE);
  else
    g_task_return_error (task, error);

  g_object_unref (task);
  return FALSE;
}

/**
 * g_socket_connection_connect_finish:
 * @connection: a #GSocketConnection
 * @result: the #GAsyncResult
 * @error: #GError for error reporting, or %NULL to ignore.
 *
 * Gets the result of a g_socket_connection_connect_async() call.
 *
 * Returns: %TRUE if the connection succeeded, %FALSE on error
 *
 * Since: 2.32
 */
gboolean
g_socket_connection_connect_finish (GSocketConnection  *connection,
				    GAsyncResult       *result,
				    GError            **error)
{
  g_return_val_if_fail (G_IS_SOCKET_CONNECTION (connection), FALSE);
  g_return_val_if_fail (g_task_is_valid (result, connection), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

/**
 * g_socket_connection_get_socket:
 * @connection: a #GSocketConnection
 *
 * Gets the underlying #GSocket object of the connection.
 * This can be useful if you want to do something unusual on it
 * not supported by the #GSocketConnection APIs.
 *
 * Returns: (transfer none): a #GSocket or %NULL on error.
 *
 * Since: 2.22
 */
GSocket *
g_socket_connection_get_socket (GSocketConnection *connection)
{
  g_return_val_if_fail (G_IS_SOCKET_CONNECTION (connection), NULL);

  return connection->priv->socket;
}

/**
 * g_socket_connection_get_local_address:
 * @connection: a #GSocketConnection
 * @error: #GError for error reporting, or %NULL to ignore.
 *
 * Try to get the local address of a socket connection.
 *
 * Returns: (transfer full): a #GSocketAddress or %NULL on error.
 *     Free the returned object with g_object_unref().
 *
 * Since: 2.22
 */
GSocketAddress *
g_socket_connection_get_local_address (GSocketConnection  *connection,
				       GError            **error)
{
  return g_socket_get_local_address (connection->priv->socket, error);
}

/**
 * g_socket_connection_get_remote_address:
 * @connection: a #GSocketConnection
 * @error: #GError for error reporting, or %NULL to ignore.
 *
 * Try to get the remote address of a socket connection.
 *
 * Since GLib 2.40, when used with g_socket_client_connect() or
 * g_socket_client_connect_async(), during emission of
 * %G_SOCKET_CLIENT_CONNECTING, this function will return the remote
 * address that will be used for the connection.  This allows
 * applications to print e.g. "Connecting to example.com
 * (10.42.77.3)...".
 *
 * Returns: (transfer full): a #GSocketAddress or %NULL on error.
 *     Free the returned object with g_object_unref().
 *
 * Since: 2.22
 */
GSocketAddress *
g_socket_connection_get_remote_address (GSocketConnection  *connection,
					GError            **error)
{
  if (!g_socket_is_connected (connection->priv->socket))
    {
      return connection->priv->cached_remote_address ?
        g_object_ref (connection->priv->cached_remote_address) : NULL;
    }
  return g_socket_get_remote_address (connection->priv->socket, error);
}

/* Private API allowing applications to retrieve the resolved address
 * now, before we start connecting.
 *
 * https://bugzilla.gnome.org/show_bug.cgi?id=712547
 */
void
g_socket_connection_set_cached_remote_address (GSocketConnection *connection,
                                               GSocketAddress    *address)
{
  g_clear_object (&connection->priv->cached_remote_address);
  connection->priv->cached_remote_address = address ? g_object_ref (address) : NULL;
}

static void
g_socket_connection_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  GSocketConnection *connection = G_SOCKET_CONNECTION (object);

  switch (prop_id)
    {
     case PROP_SOCKET:
      g_value_set_object (value, connection->priv->socket);
      break;

     default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
g_socket_connection_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GSocketConnection *connection = G_SOCKET_CONNECTION (object);

  switch (prop_id)
    {
     case PROP_SOCKET:
      connection->priv->socket = G_SOCKET (g_value_dup_object (value));
      break;

     default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
g_socket_connection_constructed (GObject *object)
{
  GSocketConnection *connection = G_SOCKET_CONNECTION (object);

  g_assert (connection->priv->socket != NULL);
}

static void
g_socket_connection_dispose (GObject *object)
{
  GSocketConnection *connection = G_SOCKET_CONNECTION (object);

  connection->priv->in_dispose = TRUE;

  g_clear_object (&connection->priv->cached_remote_address);

  G_OBJECT_CLASS (g_socket_connection_parent_class)
    ->dispose (object);

  connection->priv->in_dispose = FALSE;
}

static void
g_socket_connection_finalize (GObject *object)
{
  GSocketConnection *connection = G_SOCKET_CONNECTION (object);

  if (connection->priv->input_stream)
    g_object_unref (connection->priv->input_stream);

  if (connection->priv->output_stream)
    g_object_unref (connection->priv->output_stream);

  g_object_unref (connection->priv->socket);

  G_OBJECT_CLASS (g_socket_connection_parent_class)
    ->finalize (object);
}

static void
g_socket_connection_class_init (GSocketConnectionClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GIOStreamClass *stream_class = G_IO_STREAM_CLASS (klass);

  gobject_class->set_property = g_socket_connection_set_property;
  gobject_class->get_property = g_socket_connection_get_property;
  gobject_class->constructed = g_socket_connection_constructed;
  gobject_class->finalize = g_socket_connection_finalize;
  gobject_class->dispose = g_socket_connection_dispose;

  stream_class->get_input_stream = g_socket_connection_get_input_stream;
  stream_class->get_output_stream = g_socket_connection_get_output_stream;
  stream_class->close_fn = g_socket_connection_close;
  stream_class->close_async = g_socket_connection_close_async;
  stream_class->close_finish = g_socket_connection_close_finish;

  g_object_class_install_property (gobject_class,
                                   PROP_SOCKET,
                                   g_param_spec_object ("socket",
			                                P_("Socket"),
			                                P_("The underlying GSocket"),
                                                        G_TYPE_SOCKET,
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));
}

static void
g_socket_connection_init (GSocketConnection *connection)
{
  connection->priv = g_socket_connection_get_instance_private (connection);
}

static gboolean
g_socket_connection_close (GIOStream     *stream,
			   GCancellable  *cancellable,
			   GError       **error)
{
  GSocketConnection *connection = G_SOCKET_CONNECTION (stream);

  if (connection->priv->output_stream)
    g_output_stream_close (connection->priv->output_stream,
			   cancellable, NULL);
  if (connection->priv->input_stream)
    g_input_stream_close (connection->priv->input_stream,
			  cancellable, NULL);

  /* Don't close the underlying socket if this is being called
   * as part of dispose(); when destroying the GSocketConnection,
   * we only want to close the socket if we're holding the last
   * reference on it, and in that case it will close itself when
   * we unref it in finalize().
   */
  if (connection->priv->in_dispose)
    return TRUE;

  return g_socket_close (connection->priv->socket, error);
}


static void
g_socket_connection_close_async (GIOStream           *stream,
				 int                  io_priority,
				 GCancellable        *cancellable,
				 GAsyncReadyCallback  callback,
				 gpointer             user_data)
{
  GTask *task;
  GIOStreamClass *class;
  GError *error;

  class = G_IO_STREAM_GET_CLASS (stream);

  task = g_task_new (stream, cancellable, callback, user_data);
  g_task_set_source_tag (task, g_socket_connection_close_async);

  /* socket close is not blocked, just do it! */
  error = NULL;
  if (class->close_fn &&
      !class->close_fn (stream, cancellable, &error))
    g_task_return_error (task, error);
  else
    g_task_return_boolean (task, TRUE);

  g_object_unref (task);
}

static gboolean
g_socket_connection_close_finish (GIOStream     *stream,
				  GAsyncResult  *result,
				  GError       **error)
{
  return g_task_propagate_boolean (G_TASK (result), error);
}

typedef struct {
  GSocketFamily socket_family;
  GSocketType socket_type;
  int protocol;
  GType implementation;
} ConnectionFactory;

static guint
connection_factory_hash (gconstpointer key)
{
  const ConnectionFactory *factory = key;
  guint h;

  h = factory->socket_family ^ (factory->socket_type << 4) ^ (factory->protocol << 8);
  /* This is likely to be small, so spread over whole
     hash space to get some distribution */
  h = h ^ (h << 8) ^ (h << 16) ^ (h << 24);

  return h;
}

static gboolean
connection_factory_equal (gconstpointer _a,
			  gconstpointer _b)
{
  const ConnectionFactory *a = _a;
  const ConnectionFactory *b = _b;

  if (a->socket_family != b->socket_family)
    return FALSE;

  if (a->socket_type != b->socket_type)
    return FALSE;

  if (a->protocol != b->protocol)
    return FALSE;

  return TRUE;
}

static GHashTable *connection_factories = NULL;
G_LOCK_DEFINE_STATIC(connection_factories);

/**
 * g_socket_connection_factory_register_type:
 * @g_type: a #GType, inheriting from %G_TYPE_SOCKET_CONNECTION
 * @family: a #GSocketFamily
 * @type: a #GSocketType
 * @protocol: a protocol id
 *
 * Looks up the #GType to be used when creating socket connections on
 * sockets with the specified @family, @type and @protocol.
 *
 * If no type is registered, the #GSocketConnection base type is returned.
 *
 * Since: 2.22
 */
void
g_socket_connection_factory_register_type (GType         g_type,
					   GSocketFamily family,
					   GSocketType   type,
					   gint          protocol)
{
  ConnectionFactory *factory;

  g_return_if_fail (g_type_is_a (g_type, G_TYPE_SOCKET_CONNECTION));

  G_LOCK (connection_factories);

  if (connection_factories == NULL)
    connection_factories = g_hash_table_new_full (connection_factory_hash,
						  connection_factory_equal,
						  (GDestroyNotify)g_free,
						  NULL);

  factory = g_new0 (ConnectionFactory, 1);
  factory->socket_family = family;
  factory->socket_type = type;
  factory->protocol = protocol;
  factory->implementation = g_type;

  g_hash_table_insert (connection_factories,
		       factory, factory);

  G_UNLOCK (connection_factories);
}

static void
init_builtin_types (void)
{
#ifndef G_OS_WIN32
  g_type_ensure (G_TYPE_UNIX_CONNECTION);
#endif
  g_type_ensure (G_TYPE_TCP_CONNECTION);
}

/**
 * g_socket_connection_factory_lookup_type:
 * @family: a #GSocketFamily
 * @type: a #GSocketType
 * @protocol_id: a protocol id
 *
 * Looks up the #GType to be used when creating socket connections on
 * sockets with the specified @family, @type and @protocol_id.
 *
 * If no type is registered, the #GSocketConnection base type is returned.
 *
 * Returns: a #GType
 *
 * Since: 2.22
 */
GType
g_socket_connection_factory_lookup_type (GSocketFamily family,
					 GSocketType   type,
					 gint          protocol_id)
{
  ConnectionFactory *factory, key;
  GType g_type;

  init_builtin_types ();

  G_LOCK (connection_factories);

  g_type = G_TYPE_SOCKET_CONNECTION;

  if (connection_factories)
    {
      key.socket_family = family;
      key.socket_type = type;
      key.protocol = protocol_id;

      factory = g_hash_table_lookup (connection_factories, &key);
      if (factory)
	g_type = factory->implementation;
    }

  G_UNLOCK (connection_factories);

  return g_type;
}

/**
 * g_socket_connection_factory_create_connection:
 * @socket: a #GSocket
 *
 * Creates a #GSocketConnection subclass of the right type for
 * @socket.
 *
 * Returns: (transfer full): a #GSocketConnection
 *
 * Since: 2.22
 */
GSocketConnection *
g_socket_connection_factory_create_connection (GSocket *socket)
{
  GType type;

  type = g_socket_connection_factory_lookup_type (g_socket_get_family (socket),
						  g_socket_get_socket_type (socket),
						  g_socket_get_protocol (socket));
  return g_object_new (type, "socket", socket, NULL);
}
