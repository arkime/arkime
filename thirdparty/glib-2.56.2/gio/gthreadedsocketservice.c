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
 * SECTION:gthreadedsocketservice
 * @title: GThreadedSocketService
 * @short_description: A threaded GSocketService
 * @include: gio/gio.h
 * @see_also: #GSocketService.
 *
 * A #GThreadedSocketService is a simple subclass of #GSocketService
 * that handles incoming connections by creating a worker thread and
 * dispatching the connection to it by emitting the
 * #GThreadedSocketService::run signal in the new thread.
 *
 * The signal handler may perform blocking IO and need not return
 * until the connection is closed.
 *
 * The service is implemented using a thread pool, so there is a
 * limited amount of threads available to serve incoming requests.
 * The service automatically stops the #GSocketService from accepting
 * new connections when all threads are busy.
 *
 * As with #GSocketService, you may connect to #GThreadedSocketService::run,
 * or subclass and override the default handler.
 */

#include "config.h"
#include "gsocketconnection.h"
#include "gthreadedsocketservice.h"
#include "glibintl.h"

struct _GThreadedSocketServicePrivate
{
  GThreadPool *thread_pool;
  int max_threads;
  gint job_count;
};

static guint g_threaded_socket_service_run_signal;

G_DEFINE_TYPE_WITH_PRIVATE (GThreadedSocketService,
                            g_threaded_socket_service,
                            G_TYPE_SOCKET_SERVICE)

enum
{
  PROP_0,
  PROP_MAX_THREADS
};

G_LOCK_DEFINE_STATIC(job_count);

typedef struct
{
  GSocketConnection *connection;
  GObject *source_object;
} GThreadedSocketServiceData;

static void
g_threaded_socket_service_func (gpointer _data,
				gpointer user_data)
{
  GThreadedSocketService *threaded = user_data;
  GThreadedSocketServiceData *data = _data;
  gboolean result;

  g_signal_emit (threaded, g_threaded_socket_service_run_signal,
                 0, data->connection, data->source_object, &result);

  g_object_unref (data->connection);
  if (data->source_object)
    g_object_unref (data->source_object);
  g_slice_free (GThreadedSocketServiceData, data);

  G_LOCK (job_count);
  if (threaded->priv->job_count-- == threaded->priv->max_threads)
    g_socket_service_start (G_SOCKET_SERVICE (threaded));
  G_UNLOCK (job_count);

  g_object_unref (threaded);
}

static gboolean
g_threaded_socket_service_incoming (GSocketService    *service,
                                    GSocketConnection *connection,
                                    GObject           *source_object)
{
  GThreadedSocketService *threaded;
  GThreadedSocketServiceData *data;

  threaded = G_THREADED_SOCKET_SERVICE (service);

  data = g_slice_new (GThreadedSocketServiceData);

  /* Ref the socket service for the thread */
  g_object_ref (service);

  data->connection = g_object_ref (connection);
  if (source_object)
    data->source_object = g_object_ref (source_object);
  else
    data->source_object = NULL;

  G_LOCK (job_count);
  if (++threaded->priv->job_count == threaded->priv->max_threads)
    g_socket_service_stop (service);
  G_UNLOCK (job_count);

  g_thread_pool_push (threaded->priv->thread_pool, data, NULL);



  return FALSE;
}

static void
g_threaded_socket_service_init (GThreadedSocketService *service)
{
  service->priv = g_threaded_socket_service_get_instance_private (service);
  service->priv->max_threads = 10;
}

static void
g_threaded_socket_service_constructed (GObject *object)
{
  GThreadedSocketService *service = G_THREADED_SOCKET_SERVICE (object);

  service->priv->thread_pool =
    g_thread_pool_new  (g_threaded_socket_service_func,
			service,
			service->priv->max_threads,
			FALSE,
			NULL);
}


static void
g_threaded_socket_service_finalize (GObject *object)
{
  GThreadedSocketService *service = G_THREADED_SOCKET_SERVICE (object);

  g_thread_pool_free (service->priv->thread_pool, FALSE, FALSE);

  G_OBJECT_CLASS (g_threaded_socket_service_parent_class)
    ->finalize (object);
}

static void
g_threaded_socket_service_get_property (GObject    *object,
					guint       prop_id,
					GValue     *value,
					GParamSpec *pspec)
{
  GThreadedSocketService *service = G_THREADED_SOCKET_SERVICE (object);

  switch (prop_id)
    {
      case PROP_MAX_THREADS:
	g_value_set_int (value, service->priv->max_threads);
	break;

      default:
	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
g_threaded_socket_service_set_property (GObject      *object,
					guint         prop_id,
					const GValue *value,
					GParamSpec   *pspec)
{
  GThreadedSocketService *service = G_THREADED_SOCKET_SERVICE (object);

  switch (prop_id)
    {
      case PROP_MAX_THREADS:
	service->priv->max_threads = g_value_get_int (value);
	break;

      default:
	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}


static void
g_threaded_socket_service_class_init (GThreadedSocketServiceClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GSocketServiceClass *ss_class = &class->parent_class;

  gobject_class->constructed = g_threaded_socket_service_constructed;
  gobject_class->finalize = g_threaded_socket_service_finalize;
  gobject_class->set_property = g_threaded_socket_service_set_property;
  gobject_class->get_property = g_threaded_socket_service_get_property;

  ss_class->incoming = g_threaded_socket_service_incoming;

  /**
   * GThreadedSocketService::run:
   * @service: the #GThreadedSocketService.
   * @connection: a new #GSocketConnection object.
   * @source_object: the source_object passed to g_socket_listener_add_address().
   *
   * The ::run signal is emitted in a worker thread in response to an
   * incoming connection. This thread is dedicated to handling
   * @connection and may perform blocking IO. The signal handler need
   * not return until the connection is closed.
   *
   * Returns: %TRUE to stop further signal handlers from being called
   */
  g_threaded_socket_service_run_signal =
    g_signal_new (I_("run"), G_TYPE_FROM_CLASS (class), G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GThreadedSocketServiceClass, run),
		  g_signal_accumulator_true_handled, NULL,
		  NULL, G_TYPE_BOOLEAN,
		  2, G_TYPE_SOCKET_CONNECTION, G_TYPE_OBJECT);

  g_object_class_install_property (gobject_class, PROP_MAX_THREADS,
				   g_param_spec_int ("max-threads",
						     P_("Max threads"),
						     P_("The max number of threads handling clients for this service"),
						     -1,
						     G_MAXINT,
						     10,
						     G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

/**
 * g_threaded_socket_service_new:
 * @max_threads: the maximal number of threads to execute concurrently
 *   handling incoming clients, -1 means no limit
 *
 * Creates a new #GThreadedSocketService with no listeners. Listeners
 * must be added with one of the #GSocketListener "add" methods.
 *
 * Returns: a new #GSocketService.
 *
 * Since: 2.22
 */
GSocketService *
g_threaded_socket_service_new (int max_threads)
{
  return g_object_new (G_TYPE_THREADED_SOCKET_SERVICE,
		       "max-threads", max_threads,
		       NULL);
}
