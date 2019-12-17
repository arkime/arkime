/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright 2015 Collabora Ltd.
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
 * Authors: Philip Withnall <philip.withnall@collabora.co.uk>
 */

#include "config.h"

#include "gdatagrambased.h"

#include "gcancellable.h"
#include "gioenumtypes.h"
#include "gioerror.h"
#include "gnetworkingprivate.h"
#include "gsocketaddress.h"
#include "glibintl.h"

/**
 * SECTION:gdatagrambased
 * @short_description: Low-level datagram communications interface
 * @include: gio/gio.h
 * @see_also: #GSocket, [<gnetworking.h>][gio-gnetworking.h]
 *
 * A #GDatagramBased is a networking interface for representing datagram-based
 * communications. It is a more or less direct mapping of the core parts of the
 * BSD socket API in a portable GObject interface. It is implemented by
 * #GSocket, which wraps the UNIX socket API on UNIX and winsock2 on Windows.
 *
 * #GDatagramBased is entirely platform independent, and is intended to be used
 * alongside higher-level networking APIs such as #GIOStream.
 *
 * It uses vectored scatter/gather I/O by default, allowing for many messages
 * to be sent or received in a single call. Where possible, implementations of
 * the interface should take advantage of vectored I/O to minimise processing
 * or system calls. For example, #GSocket uses recvmmsg() and sendmmsg() where
 * possible. Callers should take advantage of scatter/gather I/O (the use of
 * multiple buffers per message) to avoid unnecessary copying of data to
 * assemble or disassemble a message.
 *
 * Each #GDatagramBased operation has a timeout parameter which may be negative
 * for blocking behaviour, zero for non-blocking behaviour, or positive for
 * timeout behaviour. A blocking operation blocks until finished or there is an
 * error. A non-blocking operation will return immediately with a
 * %G_IO_ERROR_WOULD_BLOCK error if it cannot make progress. A timeout operation
 * will block until the operation is complete or the timeout expires; if the
 * timeout expires it will return what progress it made, or
 * %G_IO_ERROR_TIMED_OUT if no progress was made. To know when a call would
 * successfully run you can call g_datagram_based_condition_check() or
 * g_datagram_based_condition_wait(). You can also use
 * g_datagram_based_create_source() and attach it to a #GMainContext to get
 * callbacks when I/O is possible.
 *
 * When running a non-blocking operation applications should always be able to
 * handle getting a %G_IO_ERROR_WOULD_BLOCK error even when some other function
 * said that I/O was possible. This can easily happen in case of a race
 * condition in the application, but it can also happen for other reasons. For
 * instance, on Windows a socket is always seen as writable until a write
 * returns %G_IO_ERROR_WOULD_BLOCK.
 *
 * As with #GSocket, #GDatagramBaseds can be either connection oriented (for
 * example, SCTP) or connectionless (for example, UDP). #GDatagramBaseds must be
 * datagram-based, not stream-based. The interface does not cover connection
 * establishment — use methods on the underlying type to establish a connection
 * before sending and receiving data through the #GDatagramBased API. For
 * connectionless socket types the target/source address is specified or
 * received in each I/O operation.
 *
 * Like most other APIs in GLib, #GDatagramBased is not inherently thread safe.
 * To use a #GDatagramBased concurrently from multiple threads, you must
 * implement your own locking.
 *
 * Since: 2.48
 */

G_DEFINE_INTERFACE (GDatagramBased, g_datagram_based, G_TYPE_OBJECT)

static void
g_datagram_based_default_init (GDatagramBasedInterface *iface)
{
  /* Nothing here. */
}

/**
 * g_datagram_based_receive_messages:
 * @datagram_based: a #GDatagramBased
 * @messages: (array length=num_messages): an array of #GInputMessage structs
 * @num_messages: the number of elements in @messages
 * @flags: an int containing #GSocketMsgFlags flags for the overall operation
 * @timeout: the maximum time (in microseconds) to wait, 0 to not block, or -1
 *   to block indefinitely
 * @cancellable: (nullable): a %GCancellable
 * @error: return location for a #GError
 *
 * Receive one or more data messages from @datagram_based in one go.
 *
 * @messages must point to an array of #GInputMessage structs and
 * @num_messages must be the length of this array. Each #GInputMessage
 * contains a pointer to an array of #GInputVector structs describing the
 * buffers that the data received in each message will be written to.
 *
 * @flags modify how all messages are received. The commonly available
 * arguments for this are available in the #GSocketMsgFlags enum, but the
 * values there are the same as the system values, and the flags
 * are passed in as-is, so you can pass in system-specific flags too. These
 * flags affect the overall receive operation. Flags affecting individual
 * messages are returned in #GInputMessage.flags.
 *
 * The other members of #GInputMessage are treated as described in its
 * documentation.
 *
 * If @timeout is negative the call will block until @num_messages have been
 * received, the connection is closed remotely (EOS), @cancellable is cancelled,
 * or an error occurs.
 *
 * If @timeout is 0 the call will return up to @num_messages without blocking,
 * or %G_IO_ERROR_WOULD_BLOCK if no messages are queued in the operating system
 * to be received.
 *
 * If @timeout is positive the call will block on the same conditions as if
 * @timeout were negative. If the timeout is reached
 * before any messages are received, %G_IO_ERROR_TIMED_OUT is returned,
 * otherwise it will return the number of messages received before timing out.
 * (Note: This is effectively the behaviour of `MSG_WAITFORONE` with
 * recvmmsg().)
 *
 * To be notified when messages are available, wait for the %G_IO_IN condition.
 * Note though that you may still receive %G_IO_ERROR_WOULD_BLOCK from
 * g_datagram_based_receive_messages() even if you were previously notified of a
 * %G_IO_IN condition.
 *
 * If the remote peer closes the connection, any messages queued in the
 * underlying receive buffer will be returned, and subsequent calls to
 * g_datagram_based_receive_messages() will return 0 (with no error set).
 *
 * If the connection is shut down or closed (by calling g_socket_close() or
 * g_socket_shutdown() with @shutdown_read set, if it’s a #GSocket, for
 * example), all calls to this function will return %G_IO_ERROR_CLOSED.
 *
 * On error -1 is returned and @error is set accordingly. An error will only
 * be returned if zero messages could be received; otherwise the number of
 * messages successfully received before the error will be returned. If
 * @cancellable is cancelled, %G_IO_ERROR_CANCELLED is returned as with any
 * other error.
 *
 * Returns: number of messages received, or -1 on error. Note that the number
 *     of messages received may be smaller than @num_messages if @timeout is
 *     zero or positive, if the peer closed the connection, or if @num_messages
 *     was larger than `UIO_MAXIOV` (1024), in which case the caller may re-try
 *     to receive the remaining messages.
 *
 * Since: 2.48
 */
gint
g_datagram_based_receive_messages (GDatagramBased  *datagram_based,
                                   GInputMessage   *messages,
                                   guint            num_messages,
                                   gint             flags,
                                   gint64           timeout,
                                   GCancellable    *cancellable,
                                   GError         **error)
{
  GDatagramBasedInterface *iface;
  gint retval;
  GError *child_error = NULL;

  g_return_val_if_fail (G_IS_DATAGRAM_BASED (datagram_based), -1);
  g_return_val_if_fail (num_messages == 0 || messages != NULL, -1);
  g_return_val_if_fail (cancellable == NULL ||
                        G_IS_CANCELLABLE (cancellable), -1);
  g_return_val_if_fail (error == NULL || *error == NULL, -1);

  iface = G_DATAGRAM_BASED_GET_IFACE (datagram_based);
  g_assert (iface->receive_messages != NULL);

  retval = iface->receive_messages (datagram_based, messages, num_messages,
                                    flags, timeout, cancellable, &child_error);

  /* Postconditions. */
  g_return_val_if_fail ((retval < 0) == (child_error != NULL), -1);
  g_return_val_if_fail (timeout == 0 ||
                        !g_error_matches (child_error, G_IO_ERROR,
                                          G_IO_ERROR_WOULD_BLOCK), -1);
  g_return_val_if_fail (timeout > 0 ||
                        !g_error_matches (child_error, G_IO_ERROR,
                                          G_IO_ERROR_TIMED_OUT), -1);
  g_return_val_if_fail (retval < 0 || (guint) retval <= num_messages, -1);

  if (child_error != NULL)
      g_propagate_error (error, child_error);

  return retval;
}

/**
 * g_datagram_based_send_messages:
 * @datagram_based: a #GDatagramBased
 * @messages: (array length=num_messages): an array of #GOutputMessage structs
 * @num_messages: the number of elements in @messages
 * @flags: an int containing #GSocketMsgFlags flags
 * @timeout: the maximum time (in microseconds) to wait, 0 to not block, or -1
 *   to block indefinitely
 * @cancellable: (nullable): a %GCancellable
 * @error: return location for a #GError
 *
 * Send one or more data messages from @datagram_based in one go.
 *
 * @messages must point to an array of #GOutputMessage structs and
 * @num_messages must be the length of this array. Each #GOutputMessage
 * contains an address to send the data to, and a pointer to an array of
 * #GOutputVector structs to describe the buffers that the data to be sent
 * for each message will be gathered from.
 *
 * @flags modify how the message is sent. The commonly available arguments
 * for this are available in the #GSocketMsgFlags enum, but the
 * values there are the same as the system values, and the flags
 * are passed in as-is, so you can pass in system-specific flags too.
 *
 * The other members of #GOutputMessage are treated as described in its
 * documentation.
 *
 * If @timeout is negative the call will block until @num_messages have been
 * sent, @cancellable is cancelled, or an error occurs.
 *
 * If @timeout is 0 the call will send up to @num_messages without blocking,
 * or will return %G_IO_ERROR_WOULD_BLOCK if there is no space to send messages.
 *
 * If @timeout is positive the call will block on the same conditions as if
 * @timeout were negative. If the timeout is reached before any messages are
 * sent, %G_IO_ERROR_TIMED_OUT is returned, otherwise it will return the number
 * of messages sent before timing out.
 *
 * To be notified when messages can be sent, wait for the %G_IO_OUT condition.
 * Note though that you may still receive %G_IO_ERROR_WOULD_BLOCK from
 * g_datagram_based_send_messages() even if you were previously notified of a
 * %G_IO_OUT condition. (On Windows in particular, this is very common due to
 * the way the underlying APIs work.)
 *
 * If the connection is shut down or closed (by calling g_socket_close() or
 * g_socket_shutdown() with @shutdown_write set, if it’s a #GSocket, for
 * example), all calls to this function will return %G_IO_ERROR_CLOSED.
 *
 * On error -1 is returned and @error is set accordingly. An error will only
 * be returned if zero messages could be sent; otherwise the number of messages
 * successfully sent before the error will be returned. If @cancellable is
 * cancelled, %G_IO_ERROR_CANCELLED is returned as with any other error.
 *
 * Returns: number of messages sent, or -1 on error. Note that the number of
 *     messages sent may be smaller than @num_messages if @timeout is zero
 *     or positive, or if @num_messages was larger than `UIO_MAXIOV` (1024), in
 *     which case the caller may re-try to send the remaining messages.
 *
 * Since: 2.48
 */
gint
g_datagram_based_send_messages (GDatagramBased   *datagram_based,
                                GOutputMessage   *messages,
                                guint             num_messages,
                                gint              flags,
                                gint64            timeout,
                                GCancellable     *cancellable,
                                GError          **error)
{
  GDatagramBasedInterface *iface;
  gint retval;
  GError *child_error = NULL;

  g_return_val_if_fail (G_IS_DATAGRAM_BASED (datagram_based), -1);
  g_return_val_if_fail (num_messages == 0 || messages != NULL, -1);
  g_return_val_if_fail (cancellable == NULL ||
                        G_IS_CANCELLABLE (cancellable), -1);
  g_return_val_if_fail (error == NULL || *error == NULL, -1);

  iface = G_DATAGRAM_BASED_GET_IFACE (datagram_based);
  g_assert (iface->send_messages != NULL);

  retval = iface->send_messages (datagram_based, messages, num_messages, flags,
                                 timeout, cancellable, &child_error);

  /* Postconditions. */
  g_return_val_if_fail ((retval < 0) == (child_error != NULL), -1);
  g_return_val_if_fail (timeout == 0 ||
                        !g_error_matches (child_error, G_IO_ERROR,
                                          G_IO_ERROR_WOULD_BLOCK), -1);
  g_return_val_if_fail (timeout > 0 ||
                        !g_error_matches (child_error, G_IO_ERROR,
                                          G_IO_ERROR_TIMED_OUT), -1);
  g_return_val_if_fail (retval < 0 || (guint) retval <= num_messages, -1);
  g_return_val_if_fail (!(timeout < 0 && num_messages > 0) || retval != 0, -1);

  if (child_error != NULL)
      g_propagate_error (error, child_error);

  return retval;
}

/**
 * g_datagram_based_create_source:
 * @datagram_based: a #GDatagramBased
 * @condition: a #GIOCondition mask to monitor
 * @cancellable: (nullable): a #GCancellable
 *
 * Creates a #GSource that can be attached to a #GMainContext to monitor for
 * the availability of the specified @condition on the #GDatagramBased. The
 * #GSource keeps a reference to the @datagram_based.
 *
 * The callback on the source is of the #GDatagramBasedSourceFunc type.
 *
 * It is meaningless to specify %G_IO_ERR or %G_IO_HUP in @condition; these
 * conditions will always be reported in the callback if they are true.
 *
 * If non-%NULL, @cancellable can be used to cancel the source, which will
 * cause the source to trigger, reporting the current condition (which is
 * likely 0 unless cancellation happened at the same time as a condition
 * change). You can check for this in the callback using
 * g_cancellable_is_cancelled().
 *
 * Returns: (transfer full): a newly allocated #GSource
 *
 * Since: 2.48
 */
GSource *
g_datagram_based_create_source (GDatagramBased *datagram_based,
                                GIOCondition    condition,
                                GCancellable   *cancellable)
{
  GDatagramBasedInterface *iface;

  g_return_val_if_fail (G_IS_DATAGRAM_BASED (datagram_based), NULL);
  g_return_val_if_fail (cancellable == NULL ||
                        G_IS_CANCELLABLE (cancellable), NULL);

  iface = G_DATAGRAM_BASED_GET_IFACE (datagram_based);
  g_assert (iface->create_source != NULL);

  return iface->create_source (datagram_based, condition, cancellable);
}

/**
 * g_datagram_based_condition_check:
 * @datagram_based: a #GDatagramBased
 * @condition: a #GIOCondition mask to check
 *
 * Checks on the readiness of @datagram_based to perform operations. The
 * operations specified in @condition are checked for and masked against the
 * currently-satisfied conditions on @datagram_based. The result is returned.
 *
 * %G_IO_IN will be set in the return value if data is available to read with
 * g_datagram_based_receive_messages(), or if the connection is closed remotely
 * (EOS); and if the datagram_based has not been closed locally using some
 * implementation-specific method (such as g_socket_close() or
 * g_socket_shutdown() with @shutdown_read set, if it’s a #GSocket).
 *
 * If the connection is shut down or closed (by calling g_socket_close() or
 * g_socket_shutdown() with @shutdown_read set, if it’s a #GSocket, for
 * example), all calls to this function will return %G_IO_ERROR_CLOSED.
 *
 * %G_IO_OUT will be set if it is expected that at least one byte can be sent
 * using g_datagram_based_send_messages() without blocking. It will not be set
 * if the datagram_based has been closed locally.
 *
 * %G_IO_HUP will be set if the connection has been closed locally.
 *
 * %G_IO_ERR will be set if there was an asynchronous error in transmitting data
 * previously enqueued using g_datagram_based_send_messages().
 *
 * Note that on Windows, it is possible for an operation to return
 * %G_IO_ERROR_WOULD_BLOCK even immediately after
 * g_datagram_based_condition_check() has claimed that the #GDatagramBased is
 * ready for writing. Rather than calling g_datagram_based_condition_check() and
 * then writing to the #GDatagramBased if it succeeds, it is generally better to
 * simply try writing right away, and try again later if the initial attempt
 * returns %G_IO_ERROR_WOULD_BLOCK.
 *
 * It is meaningless to specify %G_IO_ERR or %G_IO_HUP in @condition; these
 * conditions will always be set in the output if they are true. Apart from
 * these flags, the output is guaranteed to be masked by @condition.
 *
 * This call never blocks.
 *
 * Returns: the #GIOCondition mask of the current state
 *
 * Since: 2.48
 */
GIOCondition
g_datagram_based_condition_check (GDatagramBased *datagram_based,
                                  GIOCondition    condition)
{
  GDatagramBasedInterface *iface;
  GIOCondition out;

  g_return_val_if_fail (G_IS_DATAGRAM_BASED (datagram_based), 0);

  iface = G_DATAGRAM_BASED_GET_IFACE (datagram_based);
  g_assert (iface->condition_check != NULL);

  out = iface->condition_check (datagram_based, condition);

  /* Postconditions. G_IO_OUT and G_IO_HUP are mutually exclusive. G_IO_IN and
   * G_IO_HUP are mutually exclusive. The return value must be a subset of
   * (condition | G_IO_ERR | G_IO_HUP). */
  g_return_val_if_fail ((out & (G_IO_OUT | G_IO_HUP)) != (G_IO_OUT | G_IO_HUP),
                        out & ~G_IO_OUT);
  g_return_val_if_fail ((out & (G_IO_IN | G_IO_HUP)) != (G_IO_IN | G_IO_HUP),
                        out & ~G_IO_IN);
  g_return_val_if_fail ((out & ~(condition | G_IO_ERR | G_IO_HUP)) == 0,
                        out & (condition | G_IO_ERR | G_IO_HUP));

  return out;
}

/**
 * g_datagram_based_condition_wait:
 * @datagram_based: a #GDatagramBased
 * @condition: a #GIOCondition mask to wait for
 * @timeout: the maximum time (in microseconds) to wait, 0 to not block, or -1
 *   to block indefinitely
 * @cancellable: (nullable): a #GCancellable
 * @error: return location for a #GError
 *
 * Waits for up to @timeout microseconds for condition to become true on
 * @datagram_based. If the condition is met, %TRUE is returned.
 *
 * If @cancellable is cancelled before the condition is met, or if @timeout is
 * reached before the condition is met, then %FALSE is returned and @error is
 * set appropriately (%G_IO_ERROR_CANCELLED or %G_IO_ERROR_TIMED_OUT).
 *
 * Returns: %TRUE if the condition was met, %FALSE otherwise
 *
 * Since: 2.48
 */
gboolean
g_datagram_based_condition_wait (GDatagramBased  *datagram_based,
                                 GIOCondition     condition,
                                 gint64           timeout,
                                 GCancellable    *cancellable,
                                 GError         **error)
{
  GDatagramBasedInterface *iface;
  gboolean out;
  GError *child_error = NULL;

  g_return_val_if_fail (G_IS_DATAGRAM_BASED (datagram_based), FALSE);
  g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable),
                        FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  iface = G_DATAGRAM_BASED_GET_IFACE (datagram_based);
  g_assert (iface->condition_wait != NULL);

  out = iface->condition_wait (datagram_based, condition, timeout,
                               cancellable, &child_error);

  /* Postconditions. */
  g_return_val_if_fail (out == (child_error == NULL), FALSE);

  if (child_error != NULL)
    g_propagate_error (error, child_error);

  return out;
}
