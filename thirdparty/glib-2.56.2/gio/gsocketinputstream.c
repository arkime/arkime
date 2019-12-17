/* GIO - GLib Input, Output and Streaming Library
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
#include "gsocketinputstream.h"
#include "glibintl.h"

#include "gcancellable.h"
#include "gpollableinputstream.h"
#include "gioerror.h"
#include "gfiledescriptorbased.h"

struct _GSocketInputStreamPrivate
{
  GSocket *socket;

  /* pending operation metadata */
  gpointer buffer;
  gsize count;
};

static void g_socket_input_stream_pollable_iface_init (GPollableInputStreamInterface *iface);
#ifdef G_OS_UNIX
static void g_socket_input_stream_file_descriptor_based_iface_init (GFileDescriptorBasedIface *iface);
#endif

#define g_socket_input_stream_get_type _g_socket_input_stream_get_type

#ifdef G_OS_UNIX
G_DEFINE_TYPE_WITH_CODE (GSocketInputStream, g_socket_input_stream, G_TYPE_INPUT_STREAM,
                         G_ADD_PRIVATE (GSocketInputStream)
			 G_IMPLEMENT_INTERFACE (G_TYPE_POLLABLE_INPUT_STREAM, g_socket_input_stream_pollable_iface_init)
			 G_IMPLEMENT_INTERFACE (G_TYPE_FILE_DESCRIPTOR_BASED, g_socket_input_stream_file_descriptor_based_iface_init)
			 )
#else
G_DEFINE_TYPE_WITH_CODE (GSocketInputStream, g_socket_input_stream, G_TYPE_INPUT_STREAM,
                         G_ADD_PRIVATE (GSocketInputStream)
			 G_IMPLEMENT_INTERFACE (G_TYPE_POLLABLE_INPUT_STREAM, g_socket_input_stream_pollable_iface_init)
			 )
#endif

enum
{
  PROP_0,
  PROP_SOCKET
};

static void
g_socket_input_stream_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  GSocketInputStream *stream = G_SOCKET_INPUT_STREAM (object);

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
g_socket_input_stream_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  GSocketInputStream *stream = G_SOCKET_INPUT_STREAM (object);

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
g_socket_input_stream_finalize (GObject *object)
{
  GSocketInputStream *stream = G_SOCKET_INPUT_STREAM (object);

  if (stream->priv->socket)
    g_object_unref (stream->priv->socket);

  G_OBJECT_CLASS (g_socket_input_stream_parent_class)->finalize (object);
}

static gssize
g_socket_input_stream_read (GInputStream  *stream,
                            void          *buffer,
                            gsize          count,
                            GCancellable  *cancellable,
                            GError       **error)
{
  GSocketInputStream *input_stream = G_SOCKET_INPUT_STREAM (stream);

  return g_socket_receive_with_blocking (input_stream->priv->socket,
					 buffer, count, TRUE,
					 cancellable, error);
}

static gboolean
g_socket_input_stream_pollable_is_readable (GPollableInputStream *pollable)
{
  GSocketInputStream *input_stream = G_SOCKET_INPUT_STREAM (pollable);

  return g_socket_condition_check (input_stream->priv->socket, G_IO_IN);
}

static GSource *
g_socket_input_stream_pollable_create_source (GPollableInputStream *pollable,
					      GCancellable         *cancellable)
{
  GSocketInputStream *input_stream = G_SOCKET_INPUT_STREAM (pollable);
  GSource *socket_source, *pollable_source;

  pollable_source = g_pollable_source_new (G_OBJECT (input_stream));
  socket_source = g_socket_create_source (input_stream->priv->socket,
					  G_IO_IN, cancellable);
  g_source_set_dummy_callback (socket_source);
  g_source_add_child_source (pollable_source, socket_source);
  g_source_unref (socket_source);

  return pollable_source;
}

static gssize
g_socket_input_stream_pollable_read_nonblocking (GPollableInputStream  *pollable,
						 void                  *buffer,
						 gsize                  size,
						 GError               **error)
{
  GSocketInputStream *input_stream = G_SOCKET_INPUT_STREAM (pollable);

  return g_socket_receive_with_blocking (input_stream->priv->socket,
					 buffer, size, FALSE,
					 NULL, error);
}

#ifdef G_OS_UNIX
static int
g_socket_input_stream_get_fd (GFileDescriptorBased *fd_based)
{
  GSocketInputStream *input_stream = G_SOCKET_INPUT_STREAM (fd_based);

  return g_socket_get_fd (input_stream->priv->socket);
}
#endif

static void
g_socket_input_stream_class_init (GSocketInputStreamClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GInputStreamClass *ginputstream_class = G_INPUT_STREAM_CLASS (klass);

  gobject_class->finalize = g_socket_input_stream_finalize;
  gobject_class->get_property = g_socket_input_stream_get_property;
  gobject_class->set_property = g_socket_input_stream_set_property;

  ginputstream_class->read_fn = g_socket_input_stream_read;

  g_object_class_install_property (gobject_class, PROP_SOCKET,
				   g_param_spec_object ("socket",
							P_("socket"),
							P_("The socket that this stream wraps"),
							G_TYPE_SOCKET, G_PARAM_CONSTRUCT_ONLY |
							G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

#ifdef G_OS_UNIX
static void
g_socket_input_stream_file_descriptor_based_iface_init (GFileDescriptorBasedIface *iface)
{
  iface->get_fd = g_socket_input_stream_get_fd;
}
#endif

static void
g_socket_input_stream_pollable_iface_init (GPollableInputStreamInterface *iface)
{
  iface->is_readable = g_socket_input_stream_pollable_is_readable;
  iface->create_source = g_socket_input_stream_pollable_create_source;
  iface->read_nonblocking = g_socket_input_stream_pollable_read_nonblocking;
}

static void
g_socket_input_stream_init (GSocketInputStream *stream)
{
  stream->priv = g_socket_input_stream_get_instance_private (stream);
}

GSocketInputStream *
_g_socket_input_stream_new (GSocket *socket)
{
  return g_object_new (G_TYPE_SOCKET_INPUT_STREAM, "socket", socket, NULL);
}
