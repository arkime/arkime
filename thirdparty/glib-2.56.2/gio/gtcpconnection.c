/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright © 2008, 2009 Codethink Limited
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * See the included COPYING file for more information.
 */

/**
 * SECTION:gtcpconnection
 * @title: GTcpConnection
 * @short_description: A TCP GSocketConnection
 * @include: gio/gio.h
 * @see_also: #GSocketConnection.
 *
 * This is the subclass of #GSocketConnection that is created
 * for TCP/IP sockets.
 *
 * Since: 2.22
 */

#include "config.h"
#include "gtcpconnection.h"
#include "gasyncresult.h"
#include "gtask.h"
#include "giostream.h"
#include "glibintl.h"

struct _GTcpConnectionPrivate
{
  guint graceful_disconnect : 1;
};

G_DEFINE_TYPE_WITH_CODE (GTcpConnection, g_tcp_connection,
			 G_TYPE_SOCKET_CONNECTION,
                         G_ADD_PRIVATE (GTcpConnection)
  g_socket_connection_factory_register_type (g_define_type_id,
					     G_SOCKET_FAMILY_IPV4,
					     G_SOCKET_TYPE_STREAM,
					     G_SOCKET_PROTOCOL_DEFAULT);
  g_socket_connection_factory_register_type (g_define_type_id,
					     G_SOCKET_FAMILY_IPV6,
					     G_SOCKET_TYPE_STREAM,
					     G_SOCKET_PROTOCOL_DEFAULT);
  g_socket_connection_factory_register_type (g_define_type_id,
					     G_SOCKET_FAMILY_IPV4,
					     G_SOCKET_TYPE_STREAM,
					     G_SOCKET_PROTOCOL_TCP);
  g_socket_connection_factory_register_type (g_define_type_id,
					     G_SOCKET_FAMILY_IPV6,
					     G_SOCKET_TYPE_STREAM,
					     G_SOCKET_PROTOCOL_TCP);
			 );

static gboolean g_tcp_connection_close       (GIOStream            *stream,
					      GCancellable         *cancellable,
					      GError              **error);
static void     g_tcp_connection_close_async (GIOStream            *stream,
					      int                   io_priority,
					      GCancellable         *cancellable,
					      GAsyncReadyCallback   callback,
					      gpointer              user_data);


enum
{
  PROP_0,
  PROP_GRACEFUL_DISCONNECT
};

static void
g_tcp_connection_init (GTcpConnection *connection)
{
  connection->priv = g_tcp_connection_get_instance_private (connection);
  connection->priv->graceful_disconnect = FALSE;
}

static void
g_tcp_connection_get_property (GObject    *object,
			       guint       prop_id,
			       GValue     *value,
			       GParamSpec *pspec)
{
  GTcpConnection *connection = G_TCP_CONNECTION (object);

  switch (prop_id)
    {
      case PROP_GRACEFUL_DISCONNECT:
	g_value_set_boolean (value, connection->priv->graceful_disconnect);
	break;

      default:
	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
g_tcp_connection_set_property (GObject      *object,
			       guint         prop_id,
			       const GValue *value,
			       GParamSpec   *pspec)
{
  GTcpConnection *connection = G_TCP_CONNECTION (object);

  switch (prop_id)
    {
      case PROP_GRACEFUL_DISCONNECT:
	g_tcp_connection_set_graceful_disconnect (connection,
						  g_value_get_boolean (value));
	break;

      default:
	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
g_tcp_connection_class_init (GTcpConnectionClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GIOStreamClass *stream_class = G_IO_STREAM_CLASS (class);

  gobject_class->set_property = g_tcp_connection_set_property;
  gobject_class->get_property = g_tcp_connection_get_property;

  stream_class->close_fn = g_tcp_connection_close;
  stream_class->close_async = g_tcp_connection_close_async;

  g_object_class_install_property (gobject_class, PROP_GRACEFUL_DISCONNECT,
				   g_param_spec_boolean ("graceful-disconnect",
							 P_("Graceful Disconnect"),
							 P_("Whether or not close does a graceful disconnect"),
							 FALSE,
							 G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

}

static gboolean
g_tcp_connection_close (GIOStream     *stream,
			GCancellable  *cancellable,
			GError       **error)
{
  GTcpConnection *connection = G_TCP_CONNECTION (stream);
  GSocket *socket;
  char buffer[1024];
  gssize ret;
  gboolean had_error;

  socket = g_socket_connection_get_socket (G_SOCKET_CONNECTION (stream));
  had_error = FALSE;

  if (connection->priv->graceful_disconnect &&
      !g_cancellable_is_cancelled (cancellable) /* Cancelled -> close fast */)
    {
      if (!g_socket_shutdown (socket, FALSE, TRUE, error))
	{
	  error = NULL; /* Ignore further errors */
	  had_error = TRUE;
	}
      else
	{
	  while (TRUE)
	    {
	      ret = g_socket_receive_with_blocking (socket,  buffer, sizeof (buffer),
						    TRUE, cancellable, error);
	      if (ret < 0)
		{
		  had_error = TRUE;
		  error = NULL;
		  break;
		}
	      if (ret == 0)
		break;
	    }
	}
    }

  return G_IO_STREAM_CLASS (g_tcp_connection_parent_class)
    ->close_fn (stream, cancellable, error) && !had_error;
}

/* consumes @error */
static void
async_close_finish (GTask    *task,
                    GError   *error)
{
  GIOStreamClass *parent = G_IO_STREAM_CLASS (g_tcp_connection_parent_class);
  GIOStream *stream = g_task_get_source_object (task);
  GCancellable *cancellable = g_task_get_cancellable (task);

  /* Close underlying stream, ignoring further errors if we already
   * have one.
   */
  if (error)
    parent->close_fn (stream, cancellable, NULL);
  else
    parent->close_fn (stream, cancellable, &error);

  if (error)
    g_task_return_error (task, error);
  else
    g_task_return_boolean (task, TRUE);
}


static gboolean
close_read_ready (GSocket        *socket,
		  GIOCondition    condition,
		  GTask          *task)
{
  GError *error = NULL;
  char buffer[1024];
  gssize ret;

  ret = g_socket_receive_with_blocking (socket,  buffer, sizeof (buffer),
                                        FALSE, g_task_get_cancellable (task),
                                        &error);
  if (ret < 0)
    {
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_WOULD_BLOCK))
	{
	  g_error_free (error);
	  return TRUE;
	}
      else
	{
	  async_close_finish (task, error);
	  g_object_unref (task);
	  return FALSE;
	}
    }

  if (ret == 0)
    {
      async_close_finish (task, NULL);
      return FALSE;
    }

  return TRUE;
}


static void
g_tcp_connection_close_async (GIOStream           *stream,
			      int                  io_priority,
			      GCancellable        *cancellable,
			      GAsyncReadyCallback  callback,
			      gpointer             user_data)
{
  GTcpConnection *connection = G_TCP_CONNECTION (stream);
  GSocket *socket;
  GSource *source;
  GError *error;
  GTask *task;

  if (connection->priv->graceful_disconnect &&
      !g_cancellable_is_cancelled (cancellable) /* Cancelled -> close fast */)
    {
      task = g_task_new (stream, cancellable, callback, user_data);
      g_task_set_source_tag (task, g_tcp_connection_close_async);
      g_task_set_priority (task, io_priority);

      socket = g_socket_connection_get_socket (G_SOCKET_CONNECTION (stream));

      error = NULL;
      if (!g_socket_shutdown (socket, FALSE, TRUE, &error))
	{
	  g_task_return_error (task, error);
	  g_object_unref (task);
	  return;
	}

      source = g_socket_create_source (socket, G_IO_IN, cancellable);
      g_task_attach_source (task, source, (GSourceFunc) close_read_ready);
      g_source_unref (source);

      return;
    }

  G_IO_STREAM_CLASS (g_tcp_connection_parent_class)
    ->close_async (stream, io_priority, cancellable, callback, user_data);
}

/**
 * g_tcp_connection_set_graceful_disconnect:
 * @connection: a #GTcpConnection
 * @graceful_disconnect: Whether to do graceful disconnects or not
 *
 * This enables graceful disconnects on close. A graceful disconnect
 * means that we signal the receiving end that the connection is terminated
 * and wait for it to close the connection before closing the connection.
 *
 * A graceful disconnect means that we can be sure that we successfully sent
 * all the outstanding data to the other end, or get an error reported.
 * However, it also means we have to wait for all the data to reach the
 * other side and for it to acknowledge this by closing the socket, which may
 * take a while. For this reason it is disabled by default.
 *
 * Since: 2.22
 */
void
g_tcp_connection_set_graceful_disconnect (GTcpConnection *connection,
					  gboolean        graceful_disconnect)
{
  graceful_disconnect = !!graceful_disconnect;
  if (graceful_disconnect != connection->priv->graceful_disconnect)
    {
      connection->priv->graceful_disconnect = graceful_disconnect;
      g_object_notify (G_OBJECT (connection), "graceful-disconnect");
    }
}

/**
 * g_tcp_connection_get_graceful_disconnect:
 * @connection: a #GTcpConnection
 *
 * Checks if graceful disconnects are used. See
 * g_tcp_connection_set_graceful_disconnect().
 *
 * Returns: %TRUE if graceful disconnect is used on close, %FALSE otherwise
 *
 * Since: 2.22
 */
gboolean
g_tcp_connection_get_graceful_disconnect (GTcpConnection *connection)
{
  return connection->priv->graceful_disconnect;
}
