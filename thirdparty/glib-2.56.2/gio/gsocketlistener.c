/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright © 2008 Christian Kellner, Samuel Cormier-Iijima
 * Copyright © 2009 codethink
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
#include "gsocketlistener.h"

#include <gio/gioenumtypes.h>
#include <gio/gtask.h>
#include <gio/gcancellable.h>
#include <gio/gsocketaddress.h>
#include <gio/ginetaddress.h>
#include <gio/gioerror.h>
#include <gio/gsocket.h>
#include <gio/gsocketconnection.h>
#include <gio/ginetsocketaddress.h>
#include "glibintl.h"


/**
 * SECTION:gsocketlistener
 * @title: GSocketListener
 * @short_description: Helper for accepting network client connections
 * @include: gio/gio.h
 * @see_also: #GThreadedSocketService, #GSocketService.
 *
 * A #GSocketListener is an object that keeps track of a set
 * of server sockets and helps you accept sockets from any of the
 * socket, either sync or async.
 *
 * If you want to implement a network server, also look at #GSocketService
 * and #GThreadedSocketService which are subclass of #GSocketListener
 * that makes this even easier.
 *
 * Since: 2.22
 */

enum
{
  PROP_0,
  PROP_LISTEN_BACKLOG
};

enum
{
  EVENT,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static GQuark source_quark = 0;

struct _GSocketListenerPrivate
{
  GPtrArray           *sockets;
  GMainContext        *main_context;
  int                 listen_backlog;
  guint               closed : 1;
};

G_DEFINE_TYPE_WITH_PRIVATE (GSocketListener, g_socket_listener, G_TYPE_OBJECT)

static void
g_socket_listener_finalize (GObject *object)
{
  GSocketListener *listener = G_SOCKET_LISTENER (object);

  if (listener->priv->main_context)
    g_main_context_unref (listener->priv->main_context);

  /* Do not explicitly close the sockets. Instead, let them close themselves if
   * their final reference is dropped, but keep them open if a reference is
   * held externally to the GSocketListener (which is possible if
   * g_socket_listener_add_socket() was used).
   */
  g_ptr_array_free (listener->priv->sockets, TRUE);

  G_OBJECT_CLASS (g_socket_listener_parent_class)
    ->finalize (object);
}

static void
g_socket_listener_get_property (GObject    *object,
				guint       prop_id,
				GValue     *value,
				GParamSpec *pspec)
{
  GSocketListener *listener = G_SOCKET_LISTENER (object);

  switch (prop_id)
    {
      case PROP_LISTEN_BACKLOG:
        g_value_set_int (value, listener->priv->listen_backlog);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
g_socket_listener_set_property (GObject      *object,
				guint         prop_id,
				const GValue *value,
				GParamSpec   *pspec)
{
  GSocketListener *listener = G_SOCKET_LISTENER (object);

  switch (prop_id)
    {
      case PROP_LISTEN_BACKLOG:
	g_socket_listener_set_backlog (listener, g_value_get_int (value));
	break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
g_socket_listener_class_init (GSocketListenerClass *klass)
{
  GObjectClass *gobject_class G_GNUC_UNUSED = G_OBJECT_CLASS (klass);

  gobject_class->finalize = g_socket_listener_finalize;
  gobject_class->set_property = g_socket_listener_set_property;
  gobject_class->get_property = g_socket_listener_get_property;
  g_object_class_install_property (gobject_class, PROP_LISTEN_BACKLOG,
                                   g_param_spec_int ("listen-backlog",
                                                     P_("Listen backlog"),
                                                     P_("outstanding connections in the listen queue"),
                                                     0,
                                                     2000,
                                                     10,
                                                     G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GSocketListener::event:
   * @listener: the #GSocketListener
   * @event: the event that is occurring
   * @socket: the #GSocket the event is occurring on
   *
   * Emitted when @listener's activity on @socket changes state.
   * Note that when @listener is used to listen on both IPv4 and
   * IPv6, a separate set of signals will be emitted for each, and
   * the order they happen in is undefined.
   *
   * Since: 2.46
   */
  signals[EVENT] =
    g_signal_new (I_("event"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GSocketListenerClass, event),
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 2,
                  G_TYPE_SOCKET_LISTENER_EVENT,
                  G_TYPE_SOCKET);

  source_quark = g_quark_from_static_string ("g-socket-listener-source");
}

static void
g_socket_listener_init (GSocketListener *listener)
{
  listener->priv = g_socket_listener_get_instance_private (listener);
  listener->priv->sockets =
    g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
  listener->priv->listen_backlog = 10;
}

/**
 * g_socket_listener_new:
 *
 * Creates a new #GSocketListener with no sockets to listen for.
 * New listeners can be added with e.g. g_socket_listener_add_address()
 * or g_socket_listener_add_inet_port().
 *
 * Returns: a new #GSocketListener.
 *
 * Since: 2.22
 */
GSocketListener *
g_socket_listener_new (void)
{
  return g_object_new (G_TYPE_SOCKET_LISTENER, NULL);
}

static gboolean
check_listener (GSocketListener *listener,
		GError **error)
{
  if (listener->priv->closed)
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_CLOSED,
			   _("Listener is already closed"));
      return FALSE;
    }

  return TRUE;
}

/**
 * g_socket_listener_add_socket:
 * @listener: a #GSocketListener
 * @socket: a listening #GSocket
 * @source_object: (nullable): Optional #GObject identifying this source
 * @error: #GError for error reporting, or %NULL to ignore.
 *
 * Adds @socket to the set of sockets that we try to accept
 * new clients from. The socket must be bound to a local
 * address and listened to.
 *
 * @source_object will be passed out in the various calls
 * to accept to identify this particular source, which is
 * useful if you're listening on multiple addresses and do
 * different things depending on what address is connected to.
 *
 * The @socket will not be automatically closed when the @listener is finalized
 * unless the listener held the final reference to the socket. Before GLib 2.42,
 * the @socket was automatically closed on finalization of the @listener, even
 * if references to it were held elsewhere.
 *
 * Returns: %TRUE on success, %FALSE on error.
 *
 * Since: 2.22
 */
gboolean
g_socket_listener_add_socket (GSocketListener  *listener,
			      GSocket          *socket,
			      GObject          *source_object,
			      GError          **error)
{
  if (!check_listener (listener, error))
    return FALSE;

  /* TODO: Check that socket it is bound & not closed? */

  if (g_socket_is_closed (socket))
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
			   _("Added socket is closed"));
      return FALSE;
    }

  g_object_ref (socket);
  g_ptr_array_add (listener->priv->sockets, socket);

  if (source_object)
    g_object_set_qdata_full (G_OBJECT (socket), source_quark,
			     g_object_ref (source_object), g_object_unref);


  if (G_SOCKET_LISTENER_GET_CLASS (listener)->changed)
    G_SOCKET_LISTENER_GET_CLASS (listener)->changed (listener);

  return TRUE;
}

/**
 * g_socket_listener_add_address:
 * @listener: a #GSocketListener
 * @address: a #GSocketAddress
 * @type: a #GSocketType
 * @protocol: a #GSocketProtocol
 * @source_object: (nullable): Optional #GObject identifying this source
 * @effective_address: (out) (optional): location to store the address that was bound to, or %NULL.
 * @error: #GError for error reporting, or %NULL to ignore.
 *
 * Creates a socket of type @type and protocol @protocol, binds
 * it to @address and adds it to the set of sockets we're accepting
 * sockets from.
 *
 * Note that adding an IPv6 address, depending on the platform,
 * may or may not result in a listener that also accepts IPv4
 * connections.  For more deterministic behavior, see
 * g_socket_listener_add_inet_port().
 *
 * @source_object will be passed out in the various calls
 * to accept to identify this particular source, which is
 * useful if you're listening on multiple addresses and do
 * different things depending on what address is connected to.
 *
 * If successful and @effective_address is non-%NULL then it will
 * be set to the address that the binding actually occurred at.  This
 * is helpful for determining the port number that was used for when
 * requesting a binding to port 0 (ie: "any port").  This address, if
 * requested, belongs to the caller and must be freed.
 *
 * Returns: %TRUE on success, %FALSE on error.
 *
 * Since: 2.22
 */
gboolean
g_socket_listener_add_address (GSocketListener  *listener,
			       GSocketAddress   *address,
			       GSocketType       type,
			       GSocketProtocol   protocol,
			       GObject          *source_object,
                               GSocketAddress  **effective_address,
			       GError          **error)
{
  GSocketAddress *local_address;
  GSocketFamily family;
  GSocket *socket;

  if (!check_listener (listener, error))
    return FALSE;

  family = g_socket_address_get_family (address);
  socket = g_socket_new (family, type, protocol, error);
  if (socket == NULL)
    return FALSE;

  g_socket_set_listen_backlog (socket, listener->priv->listen_backlog);

  g_signal_emit (listener, signals[EVENT], 0,
                 G_SOCKET_LISTENER_BINDING, socket);

  if (!g_socket_bind (socket, address, TRUE, error))
    {
      g_object_unref (socket);
      return FALSE;
    }

  g_signal_emit (listener, signals[EVENT], 0,
                 G_SOCKET_LISTENER_BOUND, socket);
  g_signal_emit (listener, signals[EVENT], 0,
                 G_SOCKET_LISTENER_LISTENING, socket);

  if (!g_socket_listen (socket, error))
    {
      g_object_unref (socket);
      return FALSE;
    }

  g_signal_emit (listener, signals[EVENT], 0,
                 G_SOCKET_LISTENER_LISTENED, socket);

  local_address = NULL;
  if (effective_address)
    {
      local_address = g_socket_get_local_address (socket, error);
      if (local_address == NULL)
	{
	  g_object_unref (socket);
	  return FALSE;
	}
    }

  if (!g_socket_listener_add_socket (listener, socket,
				     source_object,
				     error))
    {
      if (local_address)
	g_object_unref (local_address);
      g_object_unref (socket);
      return FALSE;
    }

  if (effective_address)
    *effective_address = local_address;

  g_object_unref (socket); /* add_socket refs this */

  return TRUE;
}

/**
 * g_socket_listener_add_inet_port:
 * @listener: a #GSocketListener
 * @port: an IP port number (non-zero)
 * @source_object: (nullable): Optional #GObject identifying this source
 * @error: #GError for error reporting, or %NULL to ignore.
 *
 * Helper function for g_socket_listener_add_address() that
 * creates a TCP/IP socket listening on IPv4 and IPv6 (if
 * supported) on the specified port on all interfaces.
 *
 * @source_object will be passed out in the various calls
 * to accept to identify this particular source, which is
 * useful if you're listening on multiple addresses and do
 * different things depending on what address is connected to.
 *
 * Returns: %TRUE on success, %FALSE on error.
 *
 * Since: 2.22
 */
gboolean
g_socket_listener_add_inet_port (GSocketListener  *listener,
				 guint16           port,
				 GObject          *source_object,
				 GError          **error)
{
  gboolean need_ipv4_socket = TRUE;
  GSocket *socket4 = NULL;
  GSocket *socket6;

  g_return_val_if_fail (listener != NULL, FALSE);
  g_return_val_if_fail (port != 0, FALSE);

  if (!check_listener (listener, error))
    return FALSE;

  /* first try to create an IPv6 socket */
  socket6 = g_socket_new (G_SOCKET_FAMILY_IPV6,
                          G_SOCKET_TYPE_STREAM,
                          G_SOCKET_PROTOCOL_DEFAULT,
                          NULL);

  if (socket6 != NULL)
    /* IPv6 is supported on this platform, so if we fail now it is
     * a result of being unable to bind to our port.  Don't fail
     * silently as a result of this!
     */
    {
      GInetAddress *inet_address;
      GSocketAddress *address;

      inet_address = g_inet_address_new_any (G_SOCKET_FAMILY_IPV6);
      address = g_inet_socket_address_new (inet_address, port);
      g_object_unref (inet_address);

      g_socket_set_listen_backlog (socket6, listener->priv->listen_backlog);

      g_signal_emit (listener, signals[EVENT], 0,
                     G_SOCKET_LISTENER_BINDING, socket6);

      if (!g_socket_bind (socket6, address, TRUE, error))
        {
          g_object_unref (address);
          g_object_unref (socket6);
          return FALSE;
        }

      g_object_unref (address);

      g_signal_emit (listener, signals[EVENT], 0,
                     G_SOCKET_LISTENER_BOUND, socket6);
      g_signal_emit (listener, signals[EVENT], 0,
                     G_SOCKET_LISTENER_LISTENING, socket6);

      if (!g_socket_listen (socket6, error))
        {
          g_object_unref (socket6);
          return FALSE;
        }

      g_signal_emit (listener, signals[EVENT], 0,
                     G_SOCKET_LISTENER_LISTENED, socket6);

      if (source_object)
        g_object_set_qdata_full (G_OBJECT (socket6), source_quark,
                                 g_object_ref (source_object),
                                 g_object_unref);

      /* If this socket already speaks IPv4 then we are done. */
      if (g_socket_speaks_ipv4 (socket6))
        need_ipv4_socket = FALSE;
    }

  if (need_ipv4_socket)
    /* We are here for exactly one of the following reasons:
     *
     *   - our platform doesn't support IPv6
     *   - we successfully created an IPv6 socket but it's V6ONLY
     *
     * In either case, we need to go ahead and create an IPv4 socket
     * and fail the call if we can't bind to it.
     */
    {
      socket4 = g_socket_new (G_SOCKET_FAMILY_IPV4,
                              G_SOCKET_TYPE_STREAM,
                              G_SOCKET_PROTOCOL_DEFAULT,
                              error);

      if (socket4 != NULL)
        /* IPv4 is supported on this platform, so if we fail now it is
         * a result of being unable to bind to our port.  Don't fail
         * silently as a result of this!
         */
        {
          GInetAddress *inet_address;
          GSocketAddress *address;

          inet_address = g_inet_address_new_any (G_SOCKET_FAMILY_IPV4);
          address = g_inet_socket_address_new (inet_address, port);
          g_object_unref (inet_address);

          g_socket_set_listen_backlog (socket4,
                                       listener->priv->listen_backlog);

          g_signal_emit (listener, signals[EVENT], 0,
                         G_SOCKET_LISTENER_BINDING, socket4);

          if (!g_socket_bind (socket4, address, TRUE, error))
            {
              g_object_unref (address);
              g_object_unref (socket4);
              if (socket6 != NULL)
                g_object_unref (socket6);

              return FALSE;
            }

          g_object_unref (address);

          g_signal_emit (listener, signals[EVENT], 0,
                         G_SOCKET_LISTENER_BOUND, socket4);
          g_signal_emit (listener, signals[EVENT], 0,
                         G_SOCKET_LISTENER_LISTENING, socket4);

          if (!g_socket_listen (socket4, error))
            {
              g_object_unref (socket4);
              if (socket6 != NULL)
                g_object_unref (socket6);

              return FALSE;
            }

          g_signal_emit (listener, signals[EVENT], 0,
                         G_SOCKET_LISTENER_LISTENED, socket4);

          if (source_object)
            g_object_set_qdata_full (G_OBJECT (socket4), source_quark,
                                     g_object_ref (source_object),
                                     g_object_unref);
        }
      else
        /* Ok.  So IPv4 is not supported on this platform.  If we
         * succeeded at creating an IPv6 socket then that's OK, but
         * otherwise we need to tell the user we failed.
         */
        {
          if (socket6 != NULL)
            g_clear_error (error);
          else
            return FALSE;
        }
    }

  g_assert (socket6 != NULL || socket4 != NULL);

  if (socket6 != NULL)
    g_ptr_array_add (listener->priv->sockets, socket6);

  if (socket4 != NULL)
    g_ptr_array_add (listener->priv->sockets, socket4);

  if (G_SOCKET_LISTENER_GET_CLASS (listener)->changed)
    G_SOCKET_LISTENER_GET_CLASS (listener)->changed (listener);

  return TRUE;
}

static GList *
add_sources (GSocketListener   *listener,
	     GSocketSourceFunc  callback,
	     gpointer           callback_data,
	     GCancellable      *cancellable,
	     GMainContext      *context)
{
  GSocket *socket;
  GSource *source;
  GList *sources;
  int i;

  sources = NULL;
  for (i = 0; i < listener->priv->sockets->len; i++)
    {
      socket = listener->priv->sockets->pdata[i];

      source = g_socket_create_source (socket, G_IO_IN, cancellable);
      g_source_set_callback (source,
                             (GSourceFunc) callback,
                             callback_data, NULL);
      g_source_attach (source, context);

      sources = g_list_prepend (sources, source);
    }

  return sources;
}

static void
free_sources (GList *sources)
{
  GSource *source;
  while (sources != NULL)
    {
      source = sources->data;
      sources = g_list_delete_link (sources, sources);
      g_source_destroy (source);
      g_source_unref (source);
    }
}

struct AcceptData {
  GMainLoop *loop;
  GSocket *socket;
};

static gboolean
accept_callback (GSocket      *socket,
		 GIOCondition  condition,
		 gpointer      user_data)
{
  struct AcceptData *data = user_data;

  data->socket = socket;
  g_main_loop_quit (data->loop);

  return TRUE;
}

/**
 * g_socket_listener_accept_socket:
 * @listener: a #GSocketListener
 * @source_object: (out) (transfer none) (optional) (nullable): location where #GObject pointer will be stored, or %NULL.
 * @cancellable: (nullable): optional #GCancellable object, %NULL to ignore.
 * @error: #GError for error reporting, or %NULL to ignore.
 *
 * Blocks waiting for a client to connect to any of the sockets added
 * to the listener. Returns the #GSocket that was accepted.
 *
 * If you want to accept the high-level #GSocketConnection, not a #GSocket,
 * which is often the case, then you should use g_socket_listener_accept()
 * instead.
 *
 * If @source_object is not %NULL it will be filled out with the source
 * object specified when the corresponding socket or address was added
 * to the listener.
 *
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned.
 *
 * Returns: (transfer full): a #GSocket on success, %NULL on error.
 *
 * Since: 2.22
 */
GSocket *
g_socket_listener_accept_socket (GSocketListener  *listener,
				 GObject         **source_object,
				 GCancellable     *cancellable,
				 GError          **error)
{
  GSocket *accept_socket, *socket;

  g_return_val_if_fail (G_IS_SOCKET_LISTENER (listener), NULL);

  if (!check_listener (listener, error))
    return NULL;

  if (listener->priv->sockets->len == 1)
    {
      accept_socket = listener->priv->sockets->pdata[0];
      if (!g_socket_condition_wait (accept_socket, G_IO_IN,
				    cancellable, error))
	return NULL;
    }
  else
    {
      GList *sources;
      struct AcceptData data;
      GMainLoop *loop;

      if (listener->priv->main_context == NULL)
	listener->priv->main_context = g_main_context_new ();

      loop = g_main_loop_new (listener->priv->main_context, FALSE);
      data.loop = loop;
      sources = add_sources (listener,
			     accept_callback,
			     &data,
			     cancellable,
			     listener->priv->main_context);
      g_main_loop_run (loop);
      accept_socket = data.socket;
      free_sources (sources);
      g_main_loop_unref (loop);
    }

  if (!(socket = g_socket_accept (accept_socket, cancellable, error)))
    return NULL;

  if (source_object)
    *source_object = g_object_get_qdata (G_OBJECT (accept_socket), source_quark);

  return socket;
}

/**
 * g_socket_listener_accept:
 * @listener: a #GSocketListener
 * @source_object: (out) (transfer none) (optional) (nullable): location where #GObject pointer will be stored, or %NULL
 * @cancellable: (nullable): optional #GCancellable object, %NULL to ignore.
 * @error: #GError for error reporting, or %NULL to ignore.
 *
 * Blocks waiting for a client to connect to any of the sockets added
 * to the listener. Returns a #GSocketConnection for the socket that was
 * accepted.
 *
 * If @source_object is not %NULL it will be filled out with the source
 * object specified when the corresponding socket or address was added
 * to the listener.
 *
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned.
 *
 * Returns: (transfer full): a #GSocketConnection on success, %NULL on error.
 *
 * Since: 2.22
 */
GSocketConnection *
g_socket_listener_accept (GSocketListener  *listener,
			  GObject         **source_object,
			  GCancellable     *cancellable,
			  GError          **error)
{
  GSocketConnection *connection;
  GSocket *socket;

  socket = g_socket_listener_accept_socket (listener,
					    source_object,
					    cancellable,
					    error);
  if (socket == NULL)
    return NULL;

  connection = g_socket_connection_factory_create_connection (socket);
  g_object_unref (socket);

  return connection;
}

static gboolean
accept_ready (GSocket      *accept_socket,
	      GIOCondition  condition,
	      gpointer      user_data)
{
  GTask *task = user_data;
  GError *error = NULL;
  GSocket *socket;
  GObject *source_object;

  socket = g_socket_accept (accept_socket, g_task_get_cancellable (task), &error);
  if (socket)
    {
      source_object = g_object_get_qdata (G_OBJECT (accept_socket), source_quark);
      if (source_object)
	g_object_set_qdata_full (G_OBJECT (task),
				 source_quark,
				 g_object_ref (source_object), g_object_unref);
      g_task_return_pointer (task, socket, g_object_unref);
    }
  else
    {
      g_task_return_error (task, error);
    }

  g_object_unref (task);
  return FALSE;
}

/**
 * g_socket_listener_accept_socket_async:
 * @listener: a #GSocketListener
 * @cancellable: (nullable): a #GCancellable, or %NULL
 * @callback: (scope async): a #GAsyncReadyCallback
 * @user_data: (closure): user data for the callback
 *
 * This is the asynchronous version of g_socket_listener_accept_socket().
 *
 * When the operation is finished @callback will be
 * called. You can then call g_socket_listener_accept_socket_finish()
 * to get the result of the operation.
 *
 * Since: 2.22
 */
void
g_socket_listener_accept_socket_async (GSocketListener     *listener,
				       GCancellable        *cancellable,
				       GAsyncReadyCallback  callback,
				       gpointer             user_data)
{
  GTask *task;
  GList *sources;
  GError *error = NULL;

  task = g_task_new (listener, cancellable, callback, user_data);
  g_task_set_source_tag (task, g_socket_listener_accept_socket_async);

  if (!check_listener (listener, &error))
    {
      g_task_return_error (task, error);
      g_object_unref (task);
      return;
    }

  sources = add_sources (listener,
			 accept_ready,
			 task,
			 cancellable,
			 g_main_context_get_thread_default ());
  g_task_set_task_data (task, sources, (GDestroyNotify) free_sources);
}

/**
 * g_socket_listener_accept_socket_finish:
 * @listener: a #GSocketListener
 * @result: a #GAsyncResult.
 * @source_object: (out) (transfer none) (optional) (nullable): Optional #GObject identifying this source
 * @error: a #GError location to store the error occurring, or %NULL to
 * ignore.
 *
 * Finishes an async accept operation. See g_socket_listener_accept_socket_async()
 *
 * Returns: (transfer full): a #GSocket on success, %NULL on error.
 *
 * Since: 2.22
 */
GSocket *
g_socket_listener_accept_socket_finish (GSocketListener  *listener,
					GAsyncResult     *result,
					GObject         **source_object,
					GError          **error)
{
  g_return_val_if_fail (G_IS_SOCKET_LISTENER (listener), NULL);
  g_return_val_if_fail (g_task_is_valid (result, listener), NULL);

  if (source_object)
    *source_object = g_object_get_qdata (G_OBJECT (result), source_quark);

  return g_task_propagate_pointer (G_TASK (result), error);
}

/**
 * g_socket_listener_accept_async:
 * @listener: a #GSocketListener
 * @cancellable: (nullable): a #GCancellable, or %NULL
 * @callback: (scope async): a #GAsyncReadyCallback
 * @user_data: (closure): user data for the callback
 *
 * This is the asynchronous version of g_socket_listener_accept().
 *
 * When the operation is finished @callback will be
 * called. You can then call g_socket_listener_accept_socket()
 * to get the result of the operation.
 *
 * Since: 2.22
 */
void
g_socket_listener_accept_async (GSocketListener     *listener,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             user_data)
{
  g_socket_listener_accept_socket_async (listener,
					 cancellable,
					 callback,
					 user_data);
}

/**
 * g_socket_listener_accept_finish:
 * @listener: a #GSocketListener
 * @result: a #GAsyncResult.
 * @source_object: (out) (transfer none) (optional) (nullable): Optional #GObject identifying this source
 * @error: a #GError location to store the error occurring, or %NULL to
 * ignore.
 *
 * Finishes an async accept operation. See g_socket_listener_accept_async()
 *
 * Returns: (transfer full): a #GSocketConnection on success, %NULL on error.
 *
 * Since: 2.22
 */
GSocketConnection *
g_socket_listener_accept_finish (GSocketListener  *listener,
				 GAsyncResult     *result,
				 GObject         **source_object,
				 GError          **error)
{
  GSocket *socket;
  GSocketConnection *connection;

  socket = g_socket_listener_accept_socket_finish (listener,
						   result,
						   source_object,
						   error);
  if (socket == NULL)
    return NULL;

  connection = g_socket_connection_factory_create_connection (socket);
  g_object_unref (socket);
  return connection;
}

/**
 * g_socket_listener_set_backlog:
 * @listener: a #GSocketListener
 * @listen_backlog: an integer
 *
 * Sets the listen backlog on the sockets in the listener.
 *
 * See g_socket_set_listen_backlog() for details
 *
 * Since: 2.22
 */
void
g_socket_listener_set_backlog (GSocketListener *listener,
			       int              listen_backlog)
{
  GSocket *socket;
  int i;

  if (listener->priv->closed)
    return;

  listener->priv->listen_backlog = listen_backlog;

  for (i = 0; i < listener->priv->sockets->len; i++)
    {
      socket = listener->priv->sockets->pdata[i];
      g_socket_set_listen_backlog (socket, listen_backlog);
    }
}

/**
 * g_socket_listener_close:
 * @listener: a #GSocketListener
 *
 * Closes all the sockets in the listener.
 *
 * Since: 2.22
 */
void
g_socket_listener_close (GSocketListener *listener)
{
  GSocket *socket;
  int i;

  g_return_if_fail (G_IS_SOCKET_LISTENER (listener));

  if (listener->priv->closed)
    return;

  for (i = 0; i < listener->priv->sockets->len; i++)
    {
      socket = listener->priv->sockets->pdata[i];
      g_socket_close (socket, NULL);
    }
  listener->priv->closed = TRUE;
}

/**
 * g_socket_listener_add_any_inet_port:
 * @listener: a #GSocketListener
 * @source_object: (nullable): Optional #GObject identifying this source
 * @error: a #GError location to store the error occurring, or %NULL to
 * ignore.
 *
 * Listens for TCP connections on any available port number for both
 * IPv6 and IPv4 (if each is available).
 *
 * This is useful if you need to have a socket for incoming connections
 * but don't care about the specific port number.
 *
 * @source_object will be passed out in the various calls
 * to accept to identify this particular source, which is
 * useful if you're listening on multiple addresses and do
 * different things depending on what address is connected to.
 *
 * Returns: the port number, or 0 in case of failure.
 *
 * Since: 2.24
 **/
guint16
g_socket_listener_add_any_inet_port (GSocketListener  *listener,
				     GObject          *source_object,
                                     GError          **error)
{
  GSList *sockets_to_close = NULL;
  guint16 candidate_port = 0;
  GSocket *socket6 = NULL;
  GSocket *socket4 = NULL;
  gint attempts = 37;

  /*
   * multi-step process:
   *  - first, create an IPv6 socket.
   *  - if that fails, create an IPv4 socket and bind it to port 0 and
   *    that's it.  no retries if that fails (why would it?).
   *  - if our IPv6 socket also speaks IPv4 then we are done.
   *  - if not, then we need to create a IPv4 socket with the same port
   *    number.  this might fail, of course.  so we try this a bunch of
   *    times -- leaving the old IPv6 sockets open so that we get a
   *    different port number to try each time.
   *  - if all that fails then just give up.
   */

  while (attempts--)
    {
      GInetAddress *inet_address;
      GSocketAddress *address;
      gboolean result;

      g_assert (socket6 == NULL);
      socket6 = g_socket_new (G_SOCKET_FAMILY_IPV6,
                              G_SOCKET_TYPE_STREAM,
                              G_SOCKET_PROTOCOL_DEFAULT,
                              NULL);

      if (socket6 != NULL)
        {
          inet_address = g_inet_address_new_any (G_SOCKET_FAMILY_IPV6);
          address = g_inet_socket_address_new (inet_address, 0);
          g_object_unref (inet_address);

          g_signal_emit (listener, signals[EVENT], 0,
                         G_SOCKET_LISTENER_BINDING, socket6);

          result = g_socket_bind (socket6, address, TRUE, error);
          g_object_unref (address);

          if (!result ||
              !(address = g_socket_get_local_address (socket6, error)))
            {
              g_object_unref (socket6);
              socket6 = NULL;
              break;
            }

          g_signal_emit (listener, signals[EVENT], 0,
                         G_SOCKET_LISTENER_BOUND, socket6);

          g_assert (G_IS_INET_SOCKET_ADDRESS (address));
          candidate_port =
            g_inet_socket_address_get_port (G_INET_SOCKET_ADDRESS (address));
          g_assert (candidate_port != 0);
          g_object_unref (address);

          if (g_socket_speaks_ipv4 (socket6))
            break;
        }

      g_assert (socket4 == NULL);
      socket4 = g_socket_new (G_SOCKET_FAMILY_IPV4,
                              G_SOCKET_TYPE_STREAM,
                              G_SOCKET_PROTOCOL_DEFAULT,
                              socket6 ? NULL : error);

      if (socket4 == NULL)
        /* IPv4 not supported.
         * if IPv6 is supported then candidate_port will be non-zero
         *   (and the error parameter above will have been NULL)
         * if IPv6 is unsupported then candidate_port will be zero
         *   (and error will have been set by the above call)
         */
        break;

      inet_address = g_inet_address_new_any (G_SOCKET_FAMILY_IPV4);
      address = g_inet_socket_address_new (inet_address, candidate_port);
      g_object_unref (inet_address);

      g_signal_emit (listener, signals[EVENT], 0,
                     G_SOCKET_LISTENER_BINDING, socket4);

      /* a note on the 'error' clause below:
       *
       * if candidate_port is 0 then we report the error right away
       * since it is strange that this binding would fail at all.
       * otherwise, we ignore the error message (ie: NULL).
       *
       * the exception to this rule is the last time through the loop
       * (ie: attempts == 0) in which case we want to set the error
       * because failure here means that the entire call will fail and
       * we need something to show to the user.
       *
       * an english summary of the situation:  "if we gave a candidate
       * port number AND we have more attempts to try, then ignore the
       * error for now".
       */
      result = g_socket_bind (socket4, address, TRUE,
                              (candidate_port && attempts) ? NULL : error);
      g_object_unref (address);

      if (candidate_port)
        {
          g_assert (socket6 != NULL);

          if (result)
            /* got our candidate port successfully */
            {
              g_signal_emit (listener, signals[EVENT], 0,
                             G_SOCKET_LISTENER_BOUND, socket4);
              break;
            }
          else
            /* we failed to bind to the specified port.  try again. */
            {
              g_object_unref (socket4);
              socket4 = NULL;

              /* keep this open so we get a different port number */
              sockets_to_close = g_slist_prepend (sockets_to_close,
                                                  socket6);
              candidate_port = 0;
              socket6 = NULL;
            }
        }
      else
        /* we didn't tell it a port.  this means two things.
         *  - if we failed, then something really bad happened.
         *  - if we succeeded, then we need to find out the port number.
         */
        {
          g_assert (socket6 == NULL);

          if (!result ||
              !(address = g_socket_get_local_address (socket4, error)))
            {
              g_object_unref (socket4);
              socket4 = NULL;
              break;
            }

            g_signal_emit (listener, signals[EVENT], 0,
                           G_SOCKET_LISTENER_BOUND, socket4);

            g_assert (G_IS_INET_SOCKET_ADDRESS (address));
            candidate_port =
              g_inet_socket_address_get_port (G_INET_SOCKET_ADDRESS (address));
            g_assert (candidate_port != 0);
            g_object_unref (address);
            break;
        }
    }

  /* should only be non-zero if we have a socket */
  g_assert ((candidate_port != 0) == (socket4 || socket6));

  while (sockets_to_close)
    {
      g_object_unref (sockets_to_close->data);
      sockets_to_close = g_slist_delete_link (sockets_to_close,
                                              sockets_to_close);
    }

  /* now we actually listen() the sockets and add them to the listener */
  if (socket6 != NULL)
    {
      g_socket_set_listen_backlog (socket6, listener->priv->listen_backlog);

      g_signal_emit (listener, signals[EVENT], 0,
                     G_SOCKET_LISTENER_LISTENING, socket6);

      if (!g_socket_listen (socket6, error))
        {
          g_object_unref (socket6);
          if (socket4)
            g_object_unref (socket4);

          return 0;
        }

      g_signal_emit (listener, signals[EVENT], 0,
                     G_SOCKET_LISTENER_LISTENED, socket6);

      if (source_object)
        g_object_set_qdata_full (G_OBJECT (socket6), source_quark,
                                 g_object_ref (source_object),
                                 g_object_unref);

      g_ptr_array_add (listener->priv->sockets, socket6);
    }

   if (socket4 != NULL)
    {
      g_socket_set_listen_backlog (socket4, listener->priv->listen_backlog);

      g_signal_emit (listener, signals[EVENT], 0,
                     G_SOCKET_LISTENER_LISTENING, socket4);

      if (!g_socket_listen (socket4, error))
        {
          g_object_unref (socket4);
          if (socket6)
            g_object_unref (socket6);

          return 0;
        }

      g_signal_emit (listener, signals[EVENT], 0,
                     G_SOCKET_LISTENER_LISTENED, socket4);

      if (source_object)
        g_object_set_qdata_full (G_OBJECT (socket4), source_quark,
                                 g_object_ref (source_object),
                                 g_object_unref);

      g_ptr_array_add (listener->priv->sockets, socket4);
    }

  if ((socket4 != NULL || socket6 != NULL) &&
      G_SOCKET_LISTENER_GET_CLASS (listener)->changed)
    G_SOCKET_LISTENER_GET_CLASS (listener)->changed (listener);

  return candidate_port;
}
