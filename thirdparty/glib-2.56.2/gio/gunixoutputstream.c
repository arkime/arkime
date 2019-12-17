/* GIO - GLib Input, Output and Streaming Library
 * 
 * Copyright (C) 2006-2007 Red Hat, Inc.
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
 * Author: Alexander Larsson <alexl@redhat.com>
 */

#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>

#include <glib.h>
#include <glib/gstdio.h>
#include <glib/glib-unix.h>
#include "gioerror.h"
#include "gunixoutputstream.h"
#include "gcancellable.h"
#include "gasynchelper.h"
#include "gfiledescriptorbased.h"
#include "glibintl.h"


/**
 * SECTION:gunixoutputstream
 * @short_description: Streaming output operations for UNIX file descriptors
 * @include: gio/gunixoutputstream.h
 * @see_also: #GOutputStream
 *
 * #GUnixOutputStream implements #GOutputStream for writing to a UNIX
 * file descriptor, including asynchronous operations. (If the file
 * descriptor refers to a socket or pipe, this will use poll() to do
 * asynchronous I/O. If it refers to a regular file, it will fall back
 * to doing asynchronous I/O in another thread.)
 *
 * Note that `<gio/gunixoutputstream.h>` belongs to the UNIX-specific GIO
 * interfaces, thus you have to use the `gio-unix-2.0.pc` pkg-config file
 * when using it.
 */

enum {
  PROP_0,
  PROP_FD,
  PROP_CLOSE_FD
};

struct _GUnixOutputStreamPrivate {
  int fd;
  guint close_fd : 1;
  guint is_pipe_or_socket : 1;
};

static void g_unix_output_stream_pollable_iface_init (GPollableOutputStreamInterface *iface);
static void g_unix_output_stream_file_descriptor_based_iface_init (GFileDescriptorBasedIface *iface);

G_DEFINE_TYPE_WITH_CODE (GUnixOutputStream, g_unix_output_stream, G_TYPE_OUTPUT_STREAM,
                         G_ADD_PRIVATE (GUnixOutputStream)
			 G_IMPLEMENT_INTERFACE (G_TYPE_POLLABLE_OUTPUT_STREAM,
						g_unix_output_stream_pollable_iface_init)
			 G_IMPLEMENT_INTERFACE (G_TYPE_FILE_DESCRIPTOR_BASED,
						g_unix_output_stream_file_descriptor_based_iface_init)
			 )

static void     g_unix_output_stream_set_property (GObject              *object,
						   guint                 prop_id,
						   const GValue         *value,
						   GParamSpec           *pspec);
static void     g_unix_output_stream_get_property (GObject              *object,
						   guint                 prop_id,
						   GValue               *value,
						   GParamSpec           *pspec);
static gssize   g_unix_output_stream_write        (GOutputStream        *stream,
						   const void           *buffer,
						   gsize                 count,
						   GCancellable         *cancellable,
						   GError              **error);
static gboolean g_unix_output_stream_close        (GOutputStream        *stream,
						   GCancellable         *cancellable,
						   GError              **error);
static void     g_unix_output_stream_close_async  (GOutputStream        *stream,
						   int                   io_priority,
						   GCancellable         *cancellable,
						   GAsyncReadyCallback   callback,
						   gpointer              data);
static gboolean g_unix_output_stream_close_finish (GOutputStream        *stream,
						   GAsyncResult         *result,
						   GError              **error);

static gboolean g_unix_output_stream_pollable_can_poll      (GPollableOutputStream *stream);
static gboolean g_unix_output_stream_pollable_is_writable   (GPollableOutputStream *stream);
static GSource *g_unix_output_stream_pollable_create_source (GPollableOutputStream *stream,
							     GCancellable         *cancellable);

static void
g_unix_output_stream_class_init (GUnixOutputStreamClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GOutputStreamClass *stream_class = G_OUTPUT_STREAM_CLASS (klass);

  gobject_class->get_property = g_unix_output_stream_get_property;
  gobject_class->set_property = g_unix_output_stream_set_property;

  stream_class->write_fn = g_unix_output_stream_write;
  stream_class->close_fn = g_unix_output_stream_close;
  stream_class->close_async = g_unix_output_stream_close_async;
  stream_class->close_finish = g_unix_output_stream_close_finish;

   /**
   * GUnixOutputStream:fd:
   *
   * The file descriptor that the stream writes to.
   *
   * Since: 2.20
   */
  g_object_class_install_property (gobject_class,
				   PROP_FD,
				   g_param_spec_int ("fd",
						     P_("File descriptor"),
						     P_("The file descriptor to write to"),
						     G_MININT, G_MAXINT, -1,
						     G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

  /**
   * GUnixOutputStream:close-fd:
   *
   * Whether to close the file descriptor when the stream is closed.
   *
   * Since: 2.20
   */
  g_object_class_install_property (gobject_class,
				   PROP_CLOSE_FD,
				   g_param_spec_boolean ("close-fd",
							 P_("Close file descriptor"),
							 P_("Whether to close the file descriptor when the stream is closed"),
							 TRUE,
							 G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));
}

static void
g_unix_output_stream_pollable_iface_init (GPollableOutputStreamInterface *iface)
{
  iface->can_poll = g_unix_output_stream_pollable_can_poll;
  iface->is_writable = g_unix_output_stream_pollable_is_writable;
  iface->create_source = g_unix_output_stream_pollable_create_source;
}

static void
g_unix_output_stream_file_descriptor_based_iface_init (GFileDescriptorBasedIface *iface)
{
  iface->get_fd = (int (*) (GFileDescriptorBased *))g_unix_output_stream_get_fd;
}

static void
g_unix_output_stream_set_property (GObject         *object,
				   guint            prop_id,
				   const GValue    *value,
				   GParamSpec      *pspec)
{
  GUnixOutputStream *unix_stream;

  unix_stream = G_UNIX_OUTPUT_STREAM (object);

  switch (prop_id)
    {
    case PROP_FD:
      unix_stream->priv->fd = g_value_get_int (value);
      if (lseek (unix_stream->priv->fd, 0, SEEK_CUR) == -1 && errno == ESPIPE)
	unix_stream->priv->is_pipe_or_socket = TRUE;
      else
	unix_stream->priv->is_pipe_or_socket = FALSE;
      break;
    case PROP_CLOSE_FD:
      unix_stream->priv->close_fd = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
g_unix_output_stream_get_property (GObject    *object,
				   guint       prop_id,
				   GValue     *value,
				   GParamSpec *pspec)
{
  GUnixOutputStream *unix_stream;

  unix_stream = G_UNIX_OUTPUT_STREAM (object);

  switch (prop_id)
    {
    case PROP_FD:
      g_value_set_int (value, unix_stream->priv->fd);
      break;
    case PROP_CLOSE_FD:
      g_value_set_boolean (value, unix_stream->priv->close_fd);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
g_unix_output_stream_init (GUnixOutputStream *unix_stream)
{
  unix_stream->priv = g_unix_output_stream_get_instance_private (unix_stream);
  unix_stream->priv->fd = -1;
  unix_stream->priv->close_fd = TRUE;
}

/**
 * g_unix_output_stream_new:
 * @fd: a UNIX file descriptor
 * @close_fd: %TRUE to close the file descriptor when done
 * 
 * Creates a new #GUnixOutputStream for the given @fd. 
 * 
 * If @close_fd, is %TRUE, the file descriptor will be closed when 
 * the output stream is destroyed.
 * 
 * Returns: a new #GOutputStream
 **/
GOutputStream *
g_unix_output_stream_new (gint     fd,
			  gboolean close_fd)
{
  GUnixOutputStream *stream;

  g_return_val_if_fail (fd != -1, NULL);

  stream = g_object_new (G_TYPE_UNIX_OUTPUT_STREAM,
			 "fd", fd,
			 "close-fd", close_fd,
			 NULL);
  
  return G_OUTPUT_STREAM (stream);
}

/**
 * g_unix_output_stream_set_close_fd:
 * @stream: a #GUnixOutputStream
 * @close_fd: %TRUE to close the file descriptor when done
 *
 * Sets whether the file descriptor of @stream shall be closed
 * when the stream is closed.
 *
 * Since: 2.20
 */
void
g_unix_output_stream_set_close_fd (GUnixOutputStream *stream,
                                   gboolean           close_fd)
{
  g_return_if_fail (G_IS_UNIX_OUTPUT_STREAM (stream));

  close_fd = close_fd != FALSE;
  if (stream->priv->close_fd != close_fd)
    {
      stream->priv->close_fd = close_fd;
      g_object_notify (G_OBJECT (stream), "close-fd");
    }
}

/**
 * g_unix_output_stream_get_close_fd:
 * @stream: a #GUnixOutputStream
 *
 * Returns whether the file descriptor of @stream will be
 * closed when the stream is closed.
 *
 * Returns: %TRUE if the file descriptor is closed when done
 *
 * Since: 2.20
 */
gboolean
g_unix_output_stream_get_close_fd (GUnixOutputStream *stream)
{
  g_return_val_if_fail (G_IS_UNIX_OUTPUT_STREAM (stream), FALSE);

  return stream->priv->close_fd;
}

/**
 * g_unix_output_stream_get_fd:
 * @stream: a #GUnixOutputStream
 *
 * Return the UNIX file descriptor that the stream writes to.
 *
 * Returns: The file descriptor of @stream
 *
 * Since: 2.20
 */
gint
g_unix_output_stream_get_fd (GUnixOutputStream *stream)
{
  g_return_val_if_fail (G_IS_UNIX_OUTPUT_STREAM (stream), -1);

  return stream->priv->fd;
}

static gssize
g_unix_output_stream_write (GOutputStream  *stream,
			    const void     *buffer,
			    gsize           count,
			    GCancellable   *cancellable,
			    GError        **error)
{
  GUnixOutputStream *unix_stream;
  gssize res = -1;
  GPollFD poll_fds[2];
  int nfds;
  int poll_ret;

  unix_stream = G_UNIX_OUTPUT_STREAM (stream);

  poll_fds[0].fd = unix_stream->priv->fd;
  poll_fds[0].events = G_IO_OUT;

  if (unix_stream->priv->is_pipe_or_socket &&
      g_cancellable_make_pollfd (cancellable, &poll_fds[1]))
    nfds = 2;
  else
    nfds = 1;

  while (1)
    {
      int errsv;

      poll_fds[0].revents = poll_fds[1].revents = 0;
      do
        {
          poll_ret = g_poll (poll_fds, nfds, -1);
          errsv = errno;
        }
      while (poll_ret == -1 && errsv == EINTR);

      if (poll_ret == -1)
	{
	  g_set_error (error, G_IO_ERROR,
		       g_io_error_from_errno (errsv),
		       _("Error writing to file descriptor: %s"),
		       g_strerror (errsv));
	  break;
	}

      if (g_cancellable_set_error_if_cancelled (cancellable, error))
	break;

      if (!poll_fds[0].revents)
	continue;

      res = write (unix_stream->priv->fd, buffer, count);
      errsv = errno;
      if (res == -1)
	{
	  if (errsv == EINTR || errsv == EAGAIN)
	    continue;

	  g_set_error (error, G_IO_ERROR,
		       g_io_error_from_errno (errsv),
		       _("Error writing to file descriptor: %s"),
		       g_strerror (errsv));
	}

      break;
    }

  if (nfds == 2)
    g_cancellable_release_fd (cancellable);
  return res;
}

static gboolean
g_unix_output_stream_close (GOutputStream  *stream,
			    GCancellable   *cancellable,
			    GError        **error)
{
  GUnixOutputStream *unix_stream;
  int res;

  unix_stream = G_UNIX_OUTPUT_STREAM (stream);

  if (!unix_stream->priv->close_fd)
    return TRUE;
  
  /* This might block during the close. Doesn't seem to be a way to avoid it though. */
  res = close (unix_stream->priv->fd);
  if (res == -1)
    {
      int errsv = errno;

      g_set_error (error, G_IO_ERROR,
		   g_io_error_from_errno (errsv),
		   _("Error closing file descriptor: %s"),
		   g_strerror (errsv));
    }

  return res != -1;
}

static void
g_unix_output_stream_close_async (GOutputStream       *stream,
				  int                  io_priority,
				  GCancellable        *cancellable,
				  GAsyncReadyCallback  callback,
				  gpointer             user_data)
{
  GTask *task;
  GError *error = NULL;

  task = g_task_new (stream, cancellable, callback, user_data);
  g_task_set_source_tag (task, g_unix_output_stream_close_async);
  g_task_set_priority (task, io_priority);

  if (g_unix_output_stream_close (stream, cancellable, &error))
    g_task_return_boolean (task, TRUE);
  else
    g_task_return_error (task, error);
  g_object_unref (task);
}

static gboolean
g_unix_output_stream_close_finish (GOutputStream  *stream,
				   GAsyncResult   *result,
				   GError        **error)
{
  g_return_val_if_fail (g_task_is_valid (result, stream), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

static gboolean
g_unix_output_stream_pollable_can_poll (GPollableOutputStream *stream)
{
  return G_UNIX_OUTPUT_STREAM (stream)->priv->is_pipe_or_socket;
}

static gboolean
g_unix_output_stream_pollable_is_writable (GPollableOutputStream *stream)
{
  GUnixOutputStream *unix_stream = G_UNIX_OUTPUT_STREAM (stream);
  GPollFD poll_fd;
  gint result;

  poll_fd.fd = unix_stream->priv->fd;
  poll_fd.events = G_IO_OUT;
  poll_fd.revents = 0;

  do
    result = g_poll (&poll_fd, 1, 0);
  while (result == -1 && errno == EINTR);

  return poll_fd.revents != 0;
}

static GSource *
g_unix_output_stream_pollable_create_source (GPollableOutputStream *stream,
					     GCancellable          *cancellable)
{
  GUnixOutputStream *unix_stream = G_UNIX_OUTPUT_STREAM (stream);
  GSource *inner_source, *cancellable_source, *pollable_source;

  pollable_source = g_pollable_source_new (G_OBJECT (stream));

  inner_source = g_unix_fd_source_new (unix_stream->priv->fd, G_IO_OUT);
  g_source_set_dummy_callback (inner_source);
  g_source_add_child_source (pollable_source, inner_source);
  g_source_unref (inner_source);

  if (cancellable)
    {
      cancellable_source = g_cancellable_source_new (cancellable);
      g_source_set_dummy_callback (cancellable_source);
      g_source_add_child_source (pollable_source, cancellable_source);
      g_source_unref (cancellable_source);
    }

  return pollable_source;
}
