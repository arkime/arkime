/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright © 2009 Codethink Limited
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
 * Authors: Ryan Lortie <desrt@desrt.ca>
 *          Alexander Larsson <alexl@redhat.com>
 */

/**
 * SECTION:gsocketservice
 * @title: GSocketService
 * @short_description: Make it easy to implement a network service
 * @include: gio/gio.h
 * @see_also: #GThreadedSocketService, #GSocketListener.
 *
 * A #GSocketService is an object that represents a service that
 * is provided to the network or over local sockets.  When a new
 * connection is made to the service the #GSocketService::incoming
 * signal is emitted.
 *
 * A #GSocketService is a subclass of #GSocketListener and you need
 * to add the addresses you want to accept connections on with the
 * #GSocketListener APIs.
 *
 * There are two options for implementing a network service based on
 * #GSocketService. The first is to create the service using
 * g_socket_service_new() and to connect to the #GSocketService::incoming
 * signal. The second is to subclass #GSocketService and override the
 * default signal handler implementation.
 *
 * In either case, the handler must immediately return, or else it
 * will block additional incoming connections from being serviced.
 * If you are interested in writing connection handlers that contain
 * blocking code then see #GThreadedSocketService.
 *
 * The socket service runs on the main loop of the 
 * [thread-default context][g-main-context-push-thread-default-context]
 * of the thread it is created in, and is not
 * threadsafe in general. However, the calls to start and stop the
 * service are thread-safe so these can be used from threads that
 * handle incoming clients.
 *
 * Since: 2.22
 */

#include "config.h"
#include "gsocketservice.h"

#include <gio/gio.h>
#include "gsocketlistener.h"
#include "gsocketconnection.h"
#include "glibintl.h"

struct _GSocketServicePrivate
{
  GCancellable *cancellable;
  guint active : 1;
  guint outstanding_accept : 1;
};

static guint g_socket_service_incoming_signal;

G_LOCK_DEFINE_STATIC(active);

G_DEFINE_TYPE_WITH_PRIVATE (GSocketService, g_socket_service, G_TYPE_SOCKET_LISTENER)

enum
{
  PROP_0,
  PROP_ACTIVE
};

static void g_socket_service_ready (GObject      *object,
				    GAsyncResult *result,
				    gpointer      user_data);

static gboolean
g_socket_service_real_incoming (GSocketService    *service,
                                GSocketConnection *connection,
                                GObject           *source_object)
{
  return FALSE;
}

static void
g_socket_service_init (GSocketService *service)
{
  service->priv = g_socket_service_get_instance_private (service);
  service->priv->cancellable = g_cancellable_new ();
  service->priv->active = TRUE;
}

static void
g_socket_service_finalize (GObject *object)
{
  GSocketService *service = G_SOCKET_SERVICE (object);

  g_object_unref (service->priv->cancellable);

  G_OBJECT_CLASS (g_socket_service_parent_class)
    ->finalize (object);
}

static void
do_accept (GSocketService  *service)
{
  g_socket_listener_accept_async (G_SOCKET_LISTENER (service),
				  service->priv->cancellable,
				  g_socket_service_ready, NULL);
  service->priv->outstanding_accept = TRUE;
}

static gboolean
get_active (GSocketService *service)
{
  gboolean active;

  G_LOCK (active);
  active = service->priv->active;
  G_UNLOCK (active);

  return active;
}

static void
set_active (GSocketService *service, gboolean active)
{
  gboolean notify = FALSE;

  active = !!active;

  G_LOCK (active);

  if (active != service->priv->active)
    {
      service->priv->active = active;
      notify = TRUE;

      if (active)
        {
          if (service->priv->outstanding_accept)
            g_cancellable_cancel (service->priv->cancellable);
          else
            do_accept (service);
        }
      else
        {
          if (service->priv->outstanding_accept)
            g_cancellable_cancel (service->priv->cancellable);
        }
    }

  G_UNLOCK (active);

  if (notify)
    g_object_notify (G_OBJECT (service), "active");
}

static void
g_socket_service_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  GSocketService *service = G_SOCKET_SERVICE (object);

  switch (prop_id)
    {
    case PROP_ACTIVE:
      g_value_set_boolean (value, get_active (service));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
g_socket_service_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  GSocketService *service = G_SOCKET_SERVICE (object);

  switch (prop_id)
    {
    case PROP_ACTIVE:
      set_active (service, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
g_socket_service_changed (GSocketListener *listener)
{
  GSocketService  *service = G_SOCKET_SERVICE (listener);

  G_LOCK (active);

  if (service->priv->active)
    {
      if (service->priv->outstanding_accept)
	g_cancellable_cancel (service->priv->cancellable);
      else
	do_accept (service);
    }

  G_UNLOCK (active);
}

/**
 * g_socket_service_is_active:
 * @service: a #GSocketService
 *
 * Check whether the service is active or not. An active
 * service will accept new clients that connect, while
 * a non-active service will let connecting clients queue
 * up until the service is started.
 *
 * Returns: %TRUE if the service is active, %FALSE otherwise
 *
 * Since: 2.22
 */
gboolean
g_socket_service_is_active (GSocketService *service)
{
  g_return_val_if_fail (G_IS_SOCKET_SERVICE (service), FALSE);

  return get_active (service);
}

/**
 * g_socket_service_start:
 * @service: a #GSocketService
 *
 * Restarts the service, i.e. start accepting connections
 * from the added sockets when the mainloop runs. This only needs
 * to be called after the service has been stopped from
 * g_socket_service_stop().
 *
 * This call is thread-safe, so it may be called from a thread
 * handling an incoming client request.
 *
 * Since: 2.22
 */
void
g_socket_service_start (GSocketService *service)
{
  g_return_if_fail (G_IS_SOCKET_SERVICE (service));

  set_active (service, TRUE);
}

/**
 * g_socket_service_stop:
 * @service: a #GSocketService
 *
 * Stops the service, i.e. stops accepting connections
 * from the added sockets when the mainloop runs.
 *
 * This call is thread-safe, so it may be called from a thread
 * handling an incoming client request.
 *
 * Note that this only stops accepting new connections; it does not
 * close the listening sockets, and you can call
 * g_socket_service_start() again later to begin listening again. To
 * close the listening sockets, call g_socket_listener_close(). (This
 * will happen automatically when the #GSocketService is finalized.)
 *
 * This must be called before calling g_socket_listener_close() as
 * the socket service will start accepting connections immediately
 * when a new socket is added.
 *
 * Since: 2.22
 */
void
g_socket_service_stop (GSocketService *service)
{
  g_return_if_fail (G_IS_SOCKET_SERVICE (service));

  set_active (service, FALSE);
}

static gboolean
g_socket_service_incoming (GSocketService    *service,
                           GSocketConnection *connection,
                           GObject           *source_object)
{
  gboolean result;

  g_signal_emit (service, g_socket_service_incoming_signal,
                 0, connection, source_object, &result);
  return result;
}

static void
g_socket_service_class_init (GSocketServiceClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GSocketListenerClass *listener_class = G_SOCKET_LISTENER_CLASS (class);

  gobject_class->finalize = g_socket_service_finalize;
  gobject_class->set_property = g_socket_service_set_property;
  gobject_class->get_property = g_socket_service_get_property;
  listener_class->changed = g_socket_service_changed;
  class->incoming = g_socket_service_real_incoming;

  /**
   * GSocketService::incoming:
   * @service: the #GSocketService
   * @connection: a new #GSocketConnection object
   * @source_object: (nullable): the source_object passed to
   *     g_socket_listener_add_address()
   *
   * The ::incoming signal is emitted when a new incoming connection
   * to @service needs to be handled. The handler must initiate the
   * handling of @connection, but may not block; in essence,
   * asynchronous operations must be used.
   *
   * @connection will be unreffed once the signal handler returns,
   * so you need to ref it yourself if you are planning to use it.
   *
   * Returns: %TRUE to stop other handlers from being called
   *
   * Since: 2.22
   */
  g_socket_service_incoming_signal =
    g_signal_new (I_("incoming"), G_TYPE_FROM_CLASS (class), G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GSocketServiceClass, incoming),
                  g_signal_accumulator_true_handled, NULL,
                  NULL, G_TYPE_BOOLEAN,
                  2, G_TYPE_SOCKET_CONNECTION, G_TYPE_OBJECT);

  /**
   * GSocketService:active:
   *
   * Whether the service is currently accepting connections.
   *
   * Since: 2.46
   */
  g_object_class_install_property (gobject_class, PROP_ACTIVE,
                                   g_param_spec_boolean ("active",
                                                         P_("Active"),
                                                         P_("Whether the service is currently accepting connections"),
                                                         TRUE,
                                                         G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
g_socket_service_ready (GObject      *object,
                        GAsyncResult *result,
                        gpointer      user_data)
{
  GSocketListener *listener = G_SOCKET_LISTENER (object);
  GSocketService *service = G_SOCKET_SERVICE (object);
  GSocketConnection *connection;
  GObject *source_object;
  GError *error = NULL;

  connection = g_socket_listener_accept_finish (listener, result, &source_object, &error);
  if (error)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
	g_warning ("fail: %s", error->message);
      g_error_free (error);
    }
  else
    {
      g_socket_service_incoming (service, connection, source_object);
      g_object_unref (connection);
    }

  G_LOCK (active);

  g_cancellable_reset (service->priv->cancellable);

  /* requeue */
  service->priv->outstanding_accept = FALSE;
  if (service->priv->active)
    do_accept (service);

  G_UNLOCK (active);
}

/**
 * g_socket_service_new:
 *
 * Creates a new #GSocketService with no sockets to listen for.
 * New listeners can be added with e.g. g_socket_listener_add_address()
 * or g_socket_listener_add_inet_port().
 *
 * New services are created active, there is no need to call
 * g_socket_service_start(), unless g_socket_service_stop() has been
 * called before.
 *
 * Returns: a new #GSocketService.
 *
 * Since: 2.22
 */
GSocketService *
g_socket_service_new (void)
{
  return g_object_new (G_TYPE_SOCKET_SERVICE, NULL);
}
