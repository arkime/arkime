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
 * SECTION:gsocketcontrolmessage
 * @title: GSocketControlMessage
 * @short_description: A GSocket control message
 * @include: gio/gio.h
 * @see_also: #GSocket.
 *
 * A #GSocketControlMessage is a special-purpose utility message that
 * can be sent to or received from a #GSocket. These types of
 * messages are often called "ancillary data".
 *
 * The message can represent some sort of special instruction to or
 * information from the socket or can represent a special kind of
 * transfer to the peer (for example, sending a file descriptor over
 * a UNIX socket).
 *
 * These messages are sent with g_socket_send_message() and received
 * with g_socket_receive_message().
 *
 * To extend the set of control message that can be sent, subclass this
 * class and override the get_size, get_level, get_type and serialize
 * methods.
 *
 * To extend the set of control messages that can be received, subclass
 * this class and implement the deserialize method. Also, make sure your
 * class is registered with the GType typesystem before calling
 * g_socket_receive_message() to read such a message.
 *
 * Since: 2.22
 */

#include "config.h"
#include "gsocketcontrolmessage.h"
#include "gnetworkingprivate.h"
#include "glibintl.h"

#ifndef G_OS_WIN32
#include "gunixcredentialsmessage.h"
#include "gunixfdmessage.h"
#endif


G_DEFINE_ABSTRACT_TYPE (GSocketControlMessage, g_socket_control_message, G_TYPE_OBJECT)

/**
 * g_socket_control_message_get_size:
 * @message: a #GSocketControlMessage
 *
 * Returns the space required for the control message, not including
 * headers or alignment.
 *
 * Returns: The number of bytes required.
 *
 * Since: 2.22
 */
gsize
g_socket_control_message_get_size (GSocketControlMessage *message)
{
  g_return_val_if_fail (G_IS_SOCKET_CONTROL_MESSAGE (message), 0);

  return G_SOCKET_CONTROL_MESSAGE_GET_CLASS (message)->get_size (message);
}

/**
 * g_socket_control_message_get_level:
 * @message: a #GSocketControlMessage
 *
 * Returns the "level" (i.e. the originating protocol) of the control message.
 * This is often SOL_SOCKET.
 *
 * Returns: an integer describing the level
 *
 * Since: 2.22
 */
int
g_socket_control_message_get_level (GSocketControlMessage *message)
{
  g_return_val_if_fail (G_IS_SOCKET_CONTROL_MESSAGE (message), 0);

  return G_SOCKET_CONTROL_MESSAGE_GET_CLASS (message)->get_level (message);
}

/**
 * g_socket_control_message_get_msg_type:
 * @message: a #GSocketControlMessage
 *
 * Returns the protocol specific type of the control message.
 * For instance, for UNIX fd passing this would be SCM_RIGHTS.
 *
 * Returns: an integer describing the type of control message
 *
 * Since: 2.22
 */
int
g_socket_control_message_get_msg_type (GSocketControlMessage *message)
{
  g_return_val_if_fail (G_IS_SOCKET_CONTROL_MESSAGE (message), 0);

  return G_SOCKET_CONTROL_MESSAGE_GET_CLASS (message)->get_type (message);
}

/**
 * g_socket_control_message_serialize:
 * @message: a #GSocketControlMessage
 * @data: (not nullable): A buffer to write data to
 *
 * Converts the data in the message to bytes placed in the
 * message.
 *
 * @data is guaranteed to have enough space to fit the size
 * returned by g_socket_control_message_get_size() on this
 * object.
 *
 * Since: 2.22
 */
void
g_socket_control_message_serialize (GSocketControlMessage *message,
				    gpointer               data)
{
  g_return_if_fail (G_IS_SOCKET_CONTROL_MESSAGE (message));

  G_SOCKET_CONTROL_MESSAGE_GET_CLASS (message)->serialize (message, data);
}


static void
g_socket_control_message_init (GSocketControlMessage *message)
{
}

static void
g_socket_control_message_class_init (GSocketControlMessageClass *class)
{
}

/**
 * g_socket_control_message_deserialize:
 * @level: a socket level
 * @type: a socket control message type for the given @level
 * @size: the size of the data in bytes
 * @data: (array length=size) (element-type guint8): pointer to the message data
 *
 * Tries to deserialize a socket control message of a given
 * @level and @type. This will ask all known (to GType) subclasses
 * of #GSocketControlMessage if they can understand this kind
 * of message and if so deserialize it into a #GSocketControlMessage.
 *
 * If there is no implementation for this kind of control message, %NULL
 * will be returned.
 *
 * Returns: (transfer full): the deserialized message or %NULL
 *
 * Since: 2.22
 */
GSocketControlMessage *
g_socket_control_message_deserialize (int      level,
				      int      type,
				      gsize    size,
				      gpointer data)
{
  GSocketControlMessage *message;
  GType *message_types;
  guint n_message_types;
  int i;

  /* Ensure we know about the built in types */
#ifndef G_OS_WIN32
  g_type_ensure (G_TYPE_UNIX_CREDENTIALS_MESSAGE);
  g_type_ensure (G_TYPE_UNIX_FD_MESSAGE);
#endif

  message_types = g_type_children (G_TYPE_SOCKET_CONTROL_MESSAGE, &n_message_types);

  message = NULL;
  for (i = 0; i < n_message_types; i++)
    {
      GSocketControlMessageClass *class;

      class = g_type_class_ref (message_types[i]);
      message = class->deserialize (level, type, size, data);
      g_type_class_unref (class);

      if (message != NULL)
        break;
    }

  g_free (message_types);

  /* It's not a bug if we can't deserialize the control message - for
   * example, the control message may be be discarded if it is deemed
   * empty, see e.g.
   *
   *  http://git.gnome.org/browse/glib/commit/?id=ec91ed00f14c70cca9749347b8ebc19d72d9885b
   *
   * Therefore, it's not appropriate to print a warning about not
   * being able to deserialize the message.
   */

  return message;
}
