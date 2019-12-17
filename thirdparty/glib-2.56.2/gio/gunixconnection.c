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

#include "config.h"

#include "gunixconnection.h"
#include "gnetworking.h"
#include "gsocket.h"
#include "gsocketcontrolmessage.h"
#include "gunixcredentialsmessage.h"
#include "gunixfdmessage.h"
#include "glibintl.h"

#include <errno.h>
#include <string.h>
#include <unistd.h>

/**
 * SECTION:gunixconnection
 * @title: GUnixConnection
 * @short_description: A UNIX domain GSocketConnection
 * @include: gio/gunixconnection.h
 * @see_also: #GSocketConnection.
 *
 * This is the subclass of #GSocketConnection that is created
 * for UNIX domain sockets.
 *
 * It contains functions to do some of the UNIX socket specific
 * functionality like passing file descriptors.
 *
 * Note that `<gio/gunixconnection.h>` belongs to the UNIX-specific
 * GIO interfaces, thus you have to use the `gio-unix-2.0.pc`
 * pkg-config file when using it.
 *
 * Since: 2.22
 */

/**
 * GUnixConnection:
 *
 * #GUnixConnection is an opaque data structure and can only be accessed
 * using the following functions.
 **/

G_DEFINE_TYPE_WITH_CODE (GUnixConnection, g_unix_connection,
			 G_TYPE_SOCKET_CONNECTION,
  g_socket_connection_factory_register_type (g_define_type_id,
					     G_SOCKET_FAMILY_UNIX,
					     G_SOCKET_TYPE_STREAM,
					     G_SOCKET_PROTOCOL_DEFAULT);
			 );

/**
 * g_unix_connection_send_fd:
 * @connection: a #GUnixConnection
 * @fd: a file descriptor
 * @cancellable: (nullable): optional #GCancellable object, %NULL to ignore.
 * @error: (nullable): #GError for error reporting, or %NULL to ignore.
 *
 * Passes a file descriptor to the receiving side of the
 * connection. The receiving end has to call g_unix_connection_receive_fd()
 * to accept the file descriptor.
 *
 * As well as sending the fd this also writes a single byte to the
 * stream, as this is required for fd passing to work on some
 * implementations.
 *
 * Returns: a %TRUE on success, %NULL on error.
 *
 * Since: 2.22
 */
gboolean
g_unix_connection_send_fd (GUnixConnection  *connection,
                           gint              fd,
                           GCancellable     *cancellable,
                           GError          **error)
{
  GSocketControlMessage *scm;
  GSocket *socket;

  g_return_val_if_fail (G_IS_UNIX_CONNECTION (connection), FALSE);
  g_return_val_if_fail (fd >= 0, FALSE);

  scm = g_unix_fd_message_new ();

  if (!g_unix_fd_message_append_fd (G_UNIX_FD_MESSAGE (scm), fd, error))
    {
      g_object_unref (scm);
      return FALSE;
    }

  g_object_get (connection, "socket", &socket, NULL);
  if (g_socket_send_message (socket, NULL, NULL, 0, &scm, 1, 0, cancellable, error) != 1)
    /* XXX could it 'fail' with zero? */
    {
      g_object_unref (socket);
      g_object_unref (scm);

      return FALSE;
    }

  g_object_unref (socket);
  g_object_unref (scm);

  return TRUE;
}

/**
 * g_unix_connection_receive_fd:
 * @connection: a #GUnixConnection
 * @cancellable: (nullable): optional #GCancellable object, %NULL to ignore
 * @error: (nullable): #GError for error reporting, or %NULL to ignore
 *
 * Receives a file descriptor from the sending end of the connection.
 * The sending end has to call g_unix_connection_send_fd() for this
 * to work.
 *
 * As well as reading the fd this also reads a single byte from the
 * stream, as this is required for fd passing to work on some
 * implementations.
 *
 * Returns: a file descriptor on success, -1 on error.
 *
 * Since: 2.22
 **/
gint
g_unix_connection_receive_fd (GUnixConnection  *connection,
                              GCancellable     *cancellable,
                              GError          **error)
{
  GSocketControlMessage **scms;
  gint *fds, nfd, fd, nscm;
  GUnixFDMessage *fdmsg;
  GSocket *socket;

  g_return_val_if_fail (G_IS_UNIX_CONNECTION (connection), -1);

  g_object_get (connection, "socket", &socket, NULL);
  if (g_socket_receive_message (socket, NULL, NULL, 0,
                                &scms, &nscm, NULL, cancellable, error) != 1)
    /* XXX it _could_ 'fail' with zero. */
    {
      g_object_unref (socket);

      return -1;
    }

  g_object_unref (socket);

  if (nscm != 1)
    {
      gint i;

      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
        ngettext("Expecting 1 control message, got %d",
                 "Expecting 1 control message, got %d",
                 nscm),
        nscm);

      for (i = 0; i < nscm; i++)
        g_object_unref (scms[i]);

      g_free (scms);

      return -1;
    }

  if (!G_IS_UNIX_FD_MESSAGE (scms[0]))
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
			   _("Unexpected type of ancillary data"));
      g_object_unref (scms[0]);
      g_free (scms);

      return -1;
    }

  fdmsg = G_UNIX_FD_MESSAGE (scms[0]);
  g_free (scms);

  fds = g_unix_fd_message_steal_fds (fdmsg, &nfd);
  g_object_unref (fdmsg);

  if (nfd != 1)
    {
      gint i;

      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   ngettext("Expecting one fd, but got %d\n",
                            "Expecting one fd, but got %d\n",
                            nfd),
                   nfd);

      for (i = 0; i < nfd; i++)
        close (fds[i]);

      g_free (fds);

      return -1;
    }

  fd = *fds;
  g_free (fds);

  if (fd < 0)
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                           _("Received invalid fd"));
      fd = -1;
    }

  return fd;
}

static void
g_unix_connection_init (GUnixConnection *connection)
{
}

static void
g_unix_connection_class_init (GUnixConnectionClass *class)
{
}

/* TODO: Other stuff we might want to add are:
void                    g_unix_connection_send_fd_async                 (GUnixConnection      *connection,
                                                                         gint                  fd,
                                                                         gboolean              close,
                                                                         gint                  io_priority,
                                                                         GAsyncReadyCallback   callback,
                                                                         gpointer              user_data);
gboolean                g_unix_connection_send_fd_finish                (GUnixConnection      *connection,
                                                                         GError              **error);

gboolean                g_unix_connection_send_fds                      (GUnixConnection      *connection,
                                                                         gint                 *fds,
                                                                         gint                  nfds,
                                                                         GError              **error);
void                    g_unix_connection_send_fds_async                (GUnixConnection      *connection,
                                                                         gint                 *fds,
                                                                         gint                  nfds,
                                                                         gint                  io_priority,
                                                                         GAsyncReadyCallback   callback,
                                                                         gpointer              user_data);
gboolean                g_unix_connection_send_fds_finish               (GUnixConnection      *connection,
                                                                         GError              **error);

void                    g_unix_connection_receive_fd_async              (GUnixConnection      *connection,
                                                                         gint                  io_priority,
                                                                         GAsyncReadyCallback   callback,
                                                                         gpointer              user_data);
gint                    g_unix_connection_receive_fd_finish             (GUnixConnection      *connection,
                                                                         GError              **error);


gboolean                g_unix_connection_send_fake_credentials         (GUnixConnection      *connection,
                                                                         guint64               pid,
                                                                         guint64               uid,
                                                                         guint64               gid,
                                                                         GError              **error);
void                    g_unix_connection_send_fake_credentials_async   (GUnixConnection      *connection,
                                                                         guint64               pid,
                                                                         guint64               uid,
                                                                         guint64               gid,
                                                                         gint                  io_priority,
                                                                         GAsyncReadyCallback   callback,
                                                                         gpointer              user_data);
gboolean                g_unix_connection_send_fake_credentials_finish  (GUnixConnection      *connection,
                                                                         GError              **error);

gboolean                g_unix_connection_create_pair                   (GUnixConnection     **one,
                                                                         GUnixConnection     **two,
                                                                         GError              **error);
*/


/**
 * g_unix_connection_send_credentials:
 * @connection: A #GUnixConnection.
 * @cancellable: (nullable): A #GCancellable or %NULL.
 * @error: Return location for error or %NULL.
 *
 * Passes the credentials of the current user the receiving side
 * of the connection. The receiving end has to call
 * g_unix_connection_receive_credentials() (or similar) to accept the
 * credentials.
 *
 * As well as sending the credentials this also writes a single NUL
 * byte to the stream, as this is required for credentials passing to
 * work on some implementations.
 *
 * Other ways to exchange credentials with a foreign peer includes the
 * #GUnixCredentialsMessage type and g_socket_get_credentials() function.
 *
 * Returns: %TRUE on success, %FALSE if @error is set.
 *
 * Since: 2.26
 */
gboolean
g_unix_connection_send_credentials (GUnixConnection      *connection,
                                    GCancellable         *cancellable,
                                    GError              **error)
{
  GCredentials *credentials;
  GSocketControlMessage *scm;
  GSocket *socket;
  gboolean ret;
  GOutputVector vector;
  guchar nul_byte[1] = {'\0'};
  gint num_messages;

  g_return_val_if_fail (G_IS_UNIX_CONNECTION (connection), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  ret = FALSE;

  credentials = g_credentials_new ();

  vector.buffer = &nul_byte;
  vector.size = 1;

  if (g_unix_credentials_message_is_supported ())
    {
      scm = g_unix_credentials_message_new_with_credentials (credentials);
      num_messages = 1;
    }
  else
    {
      scm = NULL;
      num_messages = 0;
    }

  g_object_get (connection, "socket", &socket, NULL);
  if (g_socket_send_message (socket,
                             NULL, /* address */
                             &vector,
                             1,
                             &scm,
                             num_messages,
                             G_SOCKET_MSG_NONE,
                             cancellable,
                             error) != 1)
    {
      g_prefix_error (error, _("Error sending credentials: "));
      goto out;
    }

  ret = TRUE;

 out:
  g_object_unref (socket);
  if (scm != NULL)
    g_object_unref (scm);
  g_object_unref (credentials);
  return ret;
}

static void
send_credentials_async_thread (GTask         *task,
			       gpointer       source_object,
			       gpointer       task_data,
			       GCancellable  *cancellable)
{
  GError *error = NULL;

  if (g_unix_connection_send_credentials (G_UNIX_CONNECTION (source_object),
					  cancellable,
					  &error))
    g_task_return_boolean (task, TRUE);
  else
    g_task_return_error (task, error);
  g_object_unref (task);
}

/**
 * g_unix_connection_send_credentials_async:
 * @connection: A #GUnixConnection.
 * @cancellable: (nullable): optional #GCancellable object, %NULL to ignore.
 * @callback: (scope async): a #GAsyncReadyCallback to call when the request is satisfied
 * @user_data: (closure): the data to pass to callback function
 *
 * Asynchronously send credentials.
 *
 * For more details, see g_unix_connection_send_credentials() which is
 * the synchronous version of this call.
 *
 * When the operation is finished, @callback will be called. You can then call
 * g_unix_connection_send_credentials_finish() to get the result of the operation.
 *
 * Since: 2.32
 **/
void
g_unix_connection_send_credentials_async (GUnixConnection      *connection,
                                          GCancellable         *cancellable,
                                          GAsyncReadyCallback   callback,
                                          gpointer              user_data)
{
  GTask *task;

  task = g_task_new (connection, cancellable, callback, user_data);
  g_task_set_source_tag (task, g_unix_connection_send_credentials_async);
  g_task_run_in_thread (task, send_credentials_async_thread);
}

/**
 * g_unix_connection_send_credentials_finish:
 * @connection: A #GUnixConnection.
 * @result: a #GAsyncResult.
 * @error: a #GError, or %NULL
 *
 * Finishes an asynchronous send credentials operation started with
 * g_unix_connection_send_credentials_async().
 *
 * Returns: %TRUE if the operation was successful, otherwise %FALSE.
 *
 * Since: 2.32
 **/
gboolean
g_unix_connection_send_credentials_finish (GUnixConnection *connection,
                                           GAsyncResult    *result,
                                           GError         **error)
{
  g_return_val_if_fail (g_task_is_valid (result, connection), FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

/**
 * g_unix_connection_receive_credentials:
 * @connection: A #GUnixConnection.
 * @cancellable: (nullable): A #GCancellable or %NULL.
 * @error: Return location for error or %NULL.
 *
 * Receives credentials from the sending end of the connection.  The
 * sending end has to call g_unix_connection_send_credentials() (or
 * similar) for this to work.
 *
 * As well as reading the credentials this also reads (and discards) a
 * single byte from the stream, as this is required for credentials
 * passing to work on some implementations.
 *
 * Other ways to exchange credentials with a foreign peer includes the
 * #GUnixCredentialsMessage type and g_socket_get_credentials() function.
 *
 * Returns: (transfer full): Received credentials on success (free with
 * g_object_unref()), %NULL if @error is set.
 *
 * Since: 2.26
 */
GCredentials *
g_unix_connection_receive_credentials (GUnixConnection      *connection,
                                       GCancellable         *cancellable,
                                       GError              **error)
{
  GCredentials *ret;
  GSocketControlMessage **scms;
  gint nscm;
  GSocket *socket;
  gint n;
  gssize num_bytes_read;
#ifdef __linux__
  gboolean turn_off_so_passcreds;
#endif

  g_return_val_if_fail (G_IS_UNIX_CONNECTION (connection), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  ret = NULL;
  scms = NULL;

  g_object_get (connection, "socket", &socket, NULL);

  /* On Linux, we need to turn on SO_PASSCRED if it isn't enabled
   * already. We also need to turn it off when we're done.  See
   * #617483 for more discussion.
   */
#ifdef __linux__
  {
    gint opt_val;

    turn_off_so_passcreds = FALSE;
    opt_val = 0;
    if (!g_socket_get_option (socket,
			      SOL_SOCKET,
			      SO_PASSCRED,
			      &opt_val,
			      NULL))
      {
        int errsv = errno;
        g_set_error (error,
                     G_IO_ERROR,
                     g_io_error_from_errno (errsv),
                     _("Error checking if SO_PASSCRED is enabled for socket: %s"),
                     g_strerror (errsv));
        goto out;
      }
    if (opt_val == 0)
      {
        if (!g_socket_set_option (socket,
				  SOL_SOCKET,
				  SO_PASSCRED,
				  TRUE,
				  NULL))
          {
            int errsv = errno;
            g_set_error (error,
                         G_IO_ERROR,
                         g_io_error_from_errno (errsv),
                         _("Error enabling SO_PASSCRED: %s"),
                         g_strerror (errsv));
            goto out;
          }
        turn_off_so_passcreds = TRUE;
      }
  }
#endif

  g_type_ensure (G_TYPE_UNIX_CREDENTIALS_MESSAGE);
  num_bytes_read = g_socket_receive_message (socket,
                                             NULL, /* GSocketAddress **address */
                                             NULL,
                                             0,
                                             &scms,
                                             &nscm,
                                             NULL,
                                             cancellable,
                                             error);
  if (num_bytes_read != 1)
    {
      /* Handle situation where g_socket_receive_message() returns
       * 0 bytes and not setting @error
       */
      if (num_bytes_read == 0 && error != NULL && *error == NULL)
        {
          g_set_error_literal (error,
                               G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               _("Expecting to read a single byte for receiving credentials but read zero bytes"));
        }
      goto out;
    }

  if (g_unix_credentials_message_is_supported () &&
      /* Fall back on get_credentials if the other side didn't send the credentials */
      nscm > 0)
    {
      if (nscm != 1)
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_FAILED,
                       ngettext("Expecting 1 control message, got %d",
                                "Expecting 1 control message, got %d",
                                nscm),
                       nscm);
          goto out;
        }

      if (!G_IS_UNIX_CREDENTIALS_MESSAGE (scms[0]))
        {
          g_set_error_literal (error,
                               G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               _("Unexpected type of ancillary data"));
          goto out;
        }

      ret = g_unix_credentials_message_get_credentials (G_UNIX_CREDENTIALS_MESSAGE (scms[0]));
      g_object_ref (ret);
    }
  else
    {
      if (nscm != 0)
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_FAILED,
                       _("Not expecting control message, but got %d"),
                       nscm);
          goto out;
        }
      else
        {
          ret = g_socket_get_credentials (socket, error);
        }
    }

 out:

#ifdef __linux__
  if (turn_off_so_passcreds)
    {
      if (!g_socket_set_option (socket,
				SOL_SOCKET,
				SO_PASSCRED,
				FALSE,
				NULL))
        {
          int errsv = errno;
          g_set_error (error,
                       G_IO_ERROR,
                       g_io_error_from_errno (errsv),
                       _("Error while disabling SO_PASSCRED: %s"),
                       g_strerror (errsv));
          goto out;
        }
    }
#endif

  if (scms != NULL)
    {
      for (n = 0; n < nscm; n++)
        g_object_unref (scms[n]);
      g_free (scms);
    }
  g_object_unref (socket);
  return ret;
}

static void
receive_credentials_async_thread (GTask         *task,
				  gpointer       source_object,
				  gpointer       task_data,
				  GCancellable  *cancellable)
{
  GCredentials *creds;
  GError *error = NULL;

  creds = g_unix_connection_receive_credentials (G_UNIX_CONNECTION (source_object),
                                                 cancellable,
                                                 &error);
  if (creds)
    g_task_return_pointer (task, creds, g_object_unref);
  else
    g_task_return_error (task, error);
  g_object_unref (task);
}

/**
 * g_unix_connection_receive_credentials_async:
 * @connection: A #GUnixConnection.
 * @cancellable: (nullable): optional #GCancellable object, %NULL to ignore.
 * @callback: (scope async): a #GAsyncReadyCallback to call when the request is satisfied
 * @user_data: (closure): the data to pass to callback function
 *
 * Asynchronously receive credentials.
 *
 * For more details, see g_unix_connection_receive_credentials() which is
 * the synchronous version of this call.
 *
 * When the operation is finished, @callback will be called. You can then call
 * g_unix_connection_receive_credentials_finish() to get the result of the operation.
 *
 * Since: 2.32
 **/
void
g_unix_connection_receive_credentials_async (GUnixConnection      *connection,
                                              GCancellable         *cancellable,
                                              GAsyncReadyCallback   callback,
                                              gpointer              user_data)
{
  GTask *task;

  task = g_task_new (connection, cancellable, callback, user_data);
  g_task_set_source_tag (task, g_unix_connection_receive_credentials_async);
  g_task_run_in_thread (task, receive_credentials_async_thread);
}

/**
 * g_unix_connection_receive_credentials_finish:
 * @connection: A #GUnixConnection.
 * @result: a #GAsyncResult.
 * @error: a #GError, or %NULL
 *
 * Finishes an asynchronous receive credentials operation started with
 * g_unix_connection_receive_credentials_async().
 *
 * Returns: (transfer full): a #GCredentials, or %NULL on error.
 *     Free the returned object with g_object_unref().
 *
 * Since: 2.32
 **/
GCredentials *
g_unix_connection_receive_credentials_finish (GUnixConnection *connection,
                                              GAsyncResult    *result,
                                              GError         **error)
{
  g_return_val_if_fail (g_task_is_valid (result, connection), NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}
