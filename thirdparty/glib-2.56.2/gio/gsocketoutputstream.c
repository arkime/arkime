/*  GIO - GLib Input, Output and Streaming Library
 *
 * Copyright © 2008 Christian Kellner, Samuel Cormier-Iijima
 *           © 2009 codethink
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
 */

#include "config.h"
#include "goutputstream.h"
#include "gsocketoutputstream.h"
#include "gsocket.h"
#include "glibintl.h"

#include "gcancellable.h"
#include "gpollableinputstream.h"
#include "gpollableoutputstream.h"
#include "gioerror.h"
#include "glibintl.h"
#include "gfiledescriptorbased.h"

struct _GSocketOutputStreamPrivate
{
  GSocket *socket;

  /* pending operation metadata */
  gconstpointer buffer;
  gsize count;
};

static void g_socket_output_stream_pollable_iface_init (GPollableOutputStreamInterface *iface);
#ifdef G_OS_UNIX
static void g_socket_output_stream_file_descriptor_based_iface_init (GFileDescriptorBasedIface *iface);
#endif

#define g_socket_output_stream_get_type _g_socket_output_stream_get_type

#ifdef G_OS_UNIX
G_DEFINE_TYPE_WITH_CODE (GSocketOutputStream, g_socket_output_stream, G_TYPE_OUTPUT_STREAM,
                         G_ADD_PRIVATE (GSocketOutputStream)
			 G_IMPLEMENT_INTERFACE (G_TYPE_POLLABLE_OUTPUT_STREAM, g_socket_output_stream_pollable_iface_init)
			 G_IMPLEMENT_INTERFACE (G_TYPE_FILE_DESCRIPTOR_BASED, g_socket_output_stream_file_descriptor_based_iface_init)
			 )
#else
G_DEFINE_TYPE_WITH_CODE (GSocketOutputStream, g_socket_output_stream, G_TYPE_OUTPUT_STREAM,
                         G_ADD_PRIVATE (GSocketOutputStream)
			 G_IMPLEMENT_INTERFACE (G_TYPE_POLLABLE_OUTPUT_STREAM, g_socket_output_stream_pollable_iface_init)
			 )
#endif

enum
{
  PROP_0,
  PROP_SOCKET
};

static void
g_socket_output_stream_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  GSocketOutputStream *stream = G_SOCKET_OUTPUT_STREAM (object);

  switch (prop_id)
    {
      case PROP_SOCKET:
        g_value_set_object (value, stream->priv->socket);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
g_socket_output_stream_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  GSocketOutputStream *stream = G_SOCKET_OUTPUT_STREAM (object);

  switch (prop_id)
    {
      case PROP_SOCKET:
        stream->priv->socket = g_value_dup_object (value);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
g_socket_output_stream_finalize (GObject *object)
{
  GSocketOutputStream *stream = G_SOCKET_OUTPUT_STREAM (object);

  if (stream->priv->socket)
    g_object_unref (stream->priv->socket);

  G_OBJECT_CLASS (g_socket_output_stream_parent_class)->finalize (object);
}

static gssize
g_socket_output_stream_write (GOutputStream  *stream,
                              const void     *buffer,
                              gsize           count,
                              GCancellable   *cancellable,
                              GError        **error)
{
  GSocketOutputStream *onput_stream = G_SOCKET_OUTPUT_STREAM (stream);

  return g_socket_send_with_blocking (onput_stream->priv->socket,
				      buffer, count, TRUE,
				      cancellable, error);
}

static gboolean
g_socket_output_stream_pollable_is_writable (GPollableOutputStream *pollable)
{
  GSocketOutputStream *output_stream = G_SOCKET_OUTPUT_STREAM (pollable);

  return g_socket_condition_check (output_stream->priv->socket, G_IO_OUT);
}

static gssize
g_socket_output_stream_pollable_write_nonblocking (GPollableOutputStream  *pollable,
						   const void             *buffer,
						   gsize                   size,
						   GError                **error)
{
  GSocketOutputStream *output_stream = G_SOCKET_OUTPUT_STREAM (pollable);

  return g_socket_send_with_blocking (output_stream->priv->socket,
				      buffer, size, FALSE,
				      NULL, error);
}

static GSource *
g_socket_output_stream_pollable_create_source (GPollableOutputStream *pollable,
					       GCancellable          *cancellable)
{
  GSocketOutputStream *output_stream = G_SOCKET_OUTPUT_STREAM (pollable);
  GSource *socket_source, *pollable_source;

  pollable_source = g_pollable_source_new (G_OBJECT (output_stream));
  socket_source = g_socket_create_source (output_stream->priv->socket,
					  G_IO_OUT, cancellable);
  g_source_set_dummy_callback (socket_source);
  g_source_add_child_source (pollable_source, socket_source);
  g_source_unref (socket_source);

  return pollable_source;
}

#ifdef G_OS_UNIX
static int
g_socket_output_stream_get_fd (GFileDescriptorBased *fd_based)
{
  GSocketOutputStream *output_stream = G_SOCKET_OUTPUT_STREAM (fd_based);

  return g_socket_get_fd (output_stream->priv->socket);
}
#endif

static void
g_socket_output_stream_class_init (GSocketOutputStreamClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GOutputStreamClass *goutputstream_class = G_OUTPUT_STREAM_CLASS (klass);

  gobject_class->finalize = g_socket_output_stream_finalize;
  gobject_class->get_property = g_socket_output_stream_get_property;
  gobject_class->set_property = g_socket_output_stream_set_property;

  goutputstream_class->write_fn = g_socket_output_stream_write;

  g_object_class_install_property (gobject_class, PROP_SOCKET,
				   g_param_spec_object ("socket",
							P_("socket"),
							P_("The socket that this stream wraps"),
							G_TYPE_SOCKET, G_PARAM_CONSTRUCT_ONLY |
							G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

#ifdef G_OS_UNIX
static void
g_socket_output_stream_file_descriptor_based_iface_init (GFileDescriptorBasedIface *iface)
{
  iface->get_fd = g_socket_output_stream_get_fd;
}
#endif

static void
g_socket_output_stream_pollable_iface_init (GPollableOutputStreamInterface *iface)
{
  iface->is_writable = g_socket_output_stream_pollable_is_writable;
  iface->create_source = g_socket_output_stream_pollable_create_source;
  iface->write_nonblocking = g_socket_output_stream_pollable_write_nonblocking;
}

static void
g_socket_output_stream_init (GSocketOutputStream *stream)
{
  stream->priv = g_socket_output_stream_get_instance_private (stream);
}

GSocketOutputStream *
_g_socket_output_stream_new (GSocket *socket)
{
  return g_object_new (G_TYPE_SOCKET_OUTPUT_STREAM, "socket", socket, NULL);
}
