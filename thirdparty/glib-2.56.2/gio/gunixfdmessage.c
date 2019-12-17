/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright © 2009 Codethink Limited
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * See the included COPYING file for more information.
 *
 * Authors: Ryan Lortie <desrt@desrt.ca>
 */

/**
 * SECTION:gunixfdmessage
 * @title: GUnixFDMessage
 * @short_description: A GSocketControlMessage containing a GUnixFDList
 * @include: gio/gunixfdmessage.h
 * @see_also: #GUnixConnection, #GUnixFDList, #GSocketControlMessage
 *
 * This #GSocketControlMessage contains a #GUnixFDList.
 * It may be sent using g_socket_send_message() and received using
 * g_socket_receive_message() over UNIX sockets (ie: sockets in the
 * %G_SOCKET_ADDRESS_UNIX family). The file descriptors are copied
 * between processes by the kernel.
 *
 * For an easier way to send and receive file descriptors over
 * stream-oriented UNIX sockets, see g_unix_connection_send_fd() and
 * g_unix_connection_receive_fd().
 *
 * Note that `<gio/gunixfdmessage.h>` belongs to the UNIX-specific GIO
 * interfaces, thus you have to use the `gio-unix-2.0.pc` pkg-config
 * file when using it.
 */

/**
 * GUnixFDMessage:
 *
 * #GUnixFDMessage is an opaque data structure and can only be accessed
 * using the following functions.
 **/

#include "config.h"

#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#include "gunixfdmessage.h"
#include "gunixfdlist.h"
#include "gnetworking.h"
#include "gioerror.h"

struct _GUnixFDMessagePrivate
{
  GUnixFDList *list;
};

G_DEFINE_TYPE_WITH_PRIVATE (GUnixFDMessage, g_unix_fd_message, G_TYPE_SOCKET_CONTROL_MESSAGE)

static gsize
g_unix_fd_message_get_size (GSocketControlMessage *message)
{
  GUnixFDMessage *fd_message = G_UNIX_FD_MESSAGE (message);

  return g_unix_fd_list_get_length (fd_message->priv->list) * sizeof (gint);
}

static int
g_unix_fd_message_get_level (GSocketControlMessage *message)
{
  return SOL_SOCKET;
}

static int
g_unix_fd_message_get_msg_type (GSocketControlMessage *message)
{
  return SCM_RIGHTS;
}

static GSocketControlMessage *
g_unix_fd_message_deserialize (int      level,
			       int      type,
			       gsize    size,
			       gpointer data)
{
  GSocketControlMessage *message;
  GUnixFDList *list;
  gint n, s, i;
  gint *fds;

  if (level != SOL_SOCKET ||
      type != SCM_RIGHTS)
    return NULL;
  
  if (size % 4 > 0)
    {
      g_warning ("Kernel returned non-integral number of fds");
      return NULL;
    }

  fds = data;
  n = size / sizeof (gint);

  /* Note we probably handled this in gsocket.c already if we're on
   * Linux and have MSG_CMSG_CLOEXEC, but this code remains as a fallback
   * in case the kernel is too old for MSG_CMSG_CLOEXEC.
   */
  for (i = 0; i < n; i++)
    {
      int errsv;

      do
        {
          s = fcntl (fds[i], F_SETFD, FD_CLOEXEC);
          errsv = errno;
        }
      while (s < 0 && errsv == EINTR);

      if (s < 0)
        {
          g_warning ("Error setting close-on-exec flag on incoming fd: %s",
                     g_strerror (errsv));
          return NULL;
        }
    }

  list = g_unix_fd_list_new_from_array (fds, n);
  message = g_unix_fd_message_new_with_fd_list (list);
  g_object_unref (list);

  return message;
}

static void
g_unix_fd_message_serialize (GSocketControlMessage *message,
			     gpointer               data)
{
  GUnixFDMessage *fd_message = G_UNIX_FD_MESSAGE (message);
  const gint *fds;
  gint n_fds;

  fds = g_unix_fd_list_peek_fds (fd_message->priv->list, &n_fds);
  memcpy (data, fds, sizeof (gint) * n_fds);
}

static void
g_unix_fd_message_set_property (GObject *object, guint prop_id,
                                const GValue *value, GParamSpec *pspec)
{
  GUnixFDMessage *message = G_UNIX_FD_MESSAGE (object);

  g_assert (message->priv->list == NULL);
  g_assert_cmpint (prop_id, ==, 1);

  message->priv->list = g_value_dup_object (value);

  if (message->priv->list == NULL)
    message->priv->list = g_unix_fd_list_new ();
}

/**
 * g_unix_fd_message_get_fd_list:
 * @message: a #GUnixFDMessage
 *
 * Gets the #GUnixFDList contained in @message.  This function does not
 * return a reference to the caller, but the returned list is valid for
 * the lifetime of @message.
 *
 * Returns: (transfer none): the #GUnixFDList from @message
 *
 * Since: 2.24
 **/
GUnixFDList *
g_unix_fd_message_get_fd_list (GUnixFDMessage *message)
{
  return message->priv->list;
}

static void
g_unix_fd_message_get_property (GObject *object, guint prop_id,
                                GValue *value, GParamSpec *pspec)
{
  GUnixFDMessage *message = G_UNIX_FD_MESSAGE (object);

  g_assert_cmpint (prop_id, ==, 1);

  g_value_set_object (value, g_unix_fd_message_get_fd_list (message));
}

static void
g_unix_fd_message_init (GUnixFDMessage *message)
{
  message->priv = g_unix_fd_message_get_instance_private (message);
}

static void
g_unix_fd_message_finalize (GObject *object)
{
  GUnixFDMessage *message = G_UNIX_FD_MESSAGE (object);

  g_object_unref (message->priv->list);

  G_OBJECT_CLASS (g_unix_fd_message_parent_class)
    ->finalize (object);
}

static void
g_unix_fd_message_class_init (GUnixFDMessageClass *class)
{
  GSocketControlMessageClass *scm_class = G_SOCKET_CONTROL_MESSAGE_CLASS (class);
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  scm_class->get_size = g_unix_fd_message_get_size;
  scm_class->get_level = g_unix_fd_message_get_level;
  scm_class->get_type = g_unix_fd_message_get_msg_type;
  scm_class->serialize = g_unix_fd_message_serialize;
  scm_class->deserialize = g_unix_fd_message_deserialize;
  object_class->finalize = g_unix_fd_message_finalize;
  object_class->set_property = g_unix_fd_message_set_property;
  object_class->get_property = g_unix_fd_message_get_property;

  g_object_class_install_property (object_class, 1,
    g_param_spec_object ("fd-list", "file descriptor list",
                         "The GUnixFDList object to send with the message",
                         G_TYPE_UNIX_FD_LIST, G_PARAM_STATIC_STRINGS |
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

/**
 * g_unix_fd_message_new:
 *
 * Creates a new #GUnixFDMessage containing an empty file descriptor
 * list.
 *
 * Returns: a new #GUnixFDMessage
 *
 * Since: 2.22
 **/
GSocketControlMessage *
g_unix_fd_message_new (void)
{
  return g_object_new (G_TYPE_UNIX_FD_MESSAGE, NULL);
}

/**
 * g_unix_fd_message_new_with_fd_list:
 * @fd_list: a #GUnixFDList
 *
 * Creates a new #GUnixFDMessage containing @list.
 *
 * Returns: a new #GUnixFDMessage
 *
 * Since: 2.24
 **/
GSocketControlMessage *
g_unix_fd_message_new_with_fd_list (GUnixFDList *fd_list)
{
  return g_object_new (G_TYPE_UNIX_FD_MESSAGE,
                       "fd-list", fd_list,
                       NULL);
}

/**
 * g_unix_fd_message_steal_fds:
 * @message: a #GUnixFDMessage
 * @length: (out) (optional): pointer to the length of the returned
 *     array, or %NULL
 *
 * Returns the array of file descriptors that is contained in this
 * object.
 *
 * After this call, the descriptors are no longer contained in
 * @message. Further calls will return an empty list (unless more
 * descriptors have been added).
 *
 * The return result of this function must be freed with g_free().
 * The caller is also responsible for closing all of the file
 * descriptors.
 *
 * If @length is non-%NULL then it is set to the number of file
 * descriptors in the returned array. The returned array is also
 * terminated with -1.
 *
 * This function never returns %NULL. In case there are no file
 * descriptors contained in @message, an empty array is returned.
 *
 * Returns: (array length=length) (transfer full): an array of file
 *     descriptors
 *
 * Since: 2.22
 **/
gint *
g_unix_fd_message_steal_fds (GUnixFDMessage *message,
                             gint           *length)
{
  g_return_val_if_fail (G_UNIX_FD_MESSAGE (message), NULL);

  return g_unix_fd_list_steal_fds (message->priv->list, length);
}

/**
 * g_unix_fd_message_append_fd:
 * @message: a #GUnixFDMessage
 * @fd: a valid open file descriptor
 * @error: a #GError pointer
 *
 * Adds a file descriptor to @message.
 *
 * The file descriptor is duplicated using dup(). You keep your copy
 * of the descriptor and the copy contained in @message will be closed
 * when @message is finalized.
 *
 * A possible cause of failure is exceeding the per-process or
 * system-wide file descriptor limit.
 *
 * Returns: %TRUE in case of success, else %FALSE (and @error is set)
 *
 * Since: 2.22
 **/
gboolean
g_unix_fd_message_append_fd (GUnixFDMessage  *message,
                             gint             fd,
                             GError         **error)
{
  g_return_val_if_fail (G_UNIX_FD_MESSAGE (message), FALSE);

  return g_unix_fd_list_append (message->priv->list, fd, error) >= 0;
}
