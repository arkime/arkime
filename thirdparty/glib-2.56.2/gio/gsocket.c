/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright (C) 2008 Christian Kellner, Samuel Cormier-Iijima
 * Copyright © 2009 Codethink Limited
 * Copyright © 2009 Red Hat, Inc
 * Copyright © 2015 Collabora, Ltd.
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
 *          Philip Withnall <philip.withnall@collabora.co.uk>
 */

#include "config.h"

#include "gsocket.h"

#ifdef G_OS_UNIX
#include "glib-unix.h"
#endif

#include <errno.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>

#ifndef G_OS_WIN32
# include <fcntl.h>
# include <unistd.h>
# include <sys/ioctl.h>
#endif

#ifdef HAVE_SIOCGIFADDR
#include <net/if.h>
#endif

#ifdef HAVE_SYS_FILIO_H
# include <sys/filio.h>
#endif

#ifdef G_OS_UNIX
#include <sys/uio.h>
#endif

#define GOBJECT_COMPILATION
#include "gobject/gtype-private.h" /* For _PRELUDE type define */
#undef GOBJECT_COMPILATION
#include "gcancellable.h"
#include "gdatagrambased.h"
#include "gioenumtypes.h"
#include "ginetaddress.h"
#include "ginetsocketaddress.h"
#include "ginitable.h"
#include "gioerror.h"
#include "gioenums.h"
#include "gioerror.h"
#include "gnetworkingprivate.h"
#include "gsocketaddress.h"
#include "gsocketcontrolmessage.h"
#include "gcredentials.h"
#include "gcredentialsprivate.h"
#include "glibintl.h"

#ifdef G_OS_WIN32
/* For Windows XP runtime compatibility, but use the system's if_nametoindex() if available */
#include "gwin32networking.h"
#endif

/**
 * SECTION:gsocket
 * @short_description: Low-level socket object
 * @include: gio/gio.h
 * @see_also: #GInitable, [<gnetworking.h>][gio-gnetworking.h]
 *
 * A #GSocket is a low-level networking primitive. It is a more or less
 * direct mapping of the BSD socket API in a portable GObject based API.
 * It supports both the UNIX socket implementations and winsock2 on Windows.
 *
 * #GSocket is the platform independent base upon which the higher level
 * network primitives are based. Applications are not typically meant to
 * use it directly, but rather through classes like #GSocketClient,
 * #GSocketService and #GSocketConnection. However there may be cases where
 * direct use of #GSocket is useful.
 *
 * #GSocket implements the #GInitable interface, so if it is manually constructed
 * by e.g. g_object_new() you must call g_initable_init() and check the
 * results before using the object. This is done automatically in
 * g_socket_new() and g_socket_new_from_fd(), so these functions can return
 * %NULL.
 *
 * Sockets operate in two general modes, blocking or non-blocking. When
 * in blocking mode all operations (which don’t take an explicit blocking
 * parameter) block until the requested operation
 * is finished or there is an error. In non-blocking mode all calls that
 * would block return immediately with a %G_IO_ERROR_WOULD_BLOCK error.
 * To know when a call would successfully run you can call g_socket_condition_check(),
 * or g_socket_condition_wait(). You can also use g_socket_create_source() and
 * attach it to a #GMainContext to get callbacks when I/O is possible.
 * Note that all sockets are always set to non blocking mode in the system, and
 * blocking mode is emulated in GSocket.
 *
 * When working in non-blocking mode applications should always be able to
 * handle getting a %G_IO_ERROR_WOULD_BLOCK error even when some other
 * function said that I/O was possible. This can easily happen in case
 * of a race condition in the application, but it can also happen for other
 * reasons. For instance, on Windows a socket is always seen as writable
 * until a write returns %G_IO_ERROR_WOULD_BLOCK.
 *
 * #GSockets can be either connection oriented or datagram based.
 * For connection oriented types you must first establish a connection by
 * either connecting to an address or accepting a connection from another
 * address. For connectionless socket types the target/source address is
 * specified or received in each I/O operation.
 *
 * All socket file descriptors are set to be close-on-exec.
 *
 * Note that creating a #GSocket causes the signal %SIGPIPE to be
 * ignored for the remainder of the program. If you are writing a
 * command-line utility that uses #GSocket, you may need to take into
 * account the fact that your program will not automatically be killed
 * if it tries to write to %stdout after it has been closed.
 *
 * Like most other APIs in GLib, #GSocket is not inherently thread safe. To use
 * a #GSocket concurrently from multiple threads, you must implement your own
 * locking.
 *
 * Since: 2.22
 */

static void     g_socket_initable_iface_init (GInitableIface  *iface);
static gboolean g_socket_initable_init       (GInitable       *initable,
					      GCancellable    *cancellable,
					      GError         **error);

static void     g_socket_datagram_based_iface_init       (GDatagramBasedInterface *iface);
static gint     g_socket_datagram_based_receive_messages (GDatagramBased  *self,
                                                          GInputMessage   *messages,
                                                          guint            num_messages,
                                                          gint             flags,
                                                          gint64           timeout,
                                                          GCancellable    *cancellable,
                                                          GError         **error);
static gint     g_socket_datagram_based_send_messages    (GDatagramBased  *self,
                                                          GOutputMessage  *messages,
                                                          guint            num_messages,
                                                          gint             flags,
                                                          gint64           timeout,
                                                          GCancellable    *cancellable,
                                                          GError         **error);
static GSource *g_socket_datagram_based_create_source    (GDatagramBased           *self,
                                                          GIOCondition              condition,
                                                          GCancellable             *cancellable);
static GIOCondition g_socket_datagram_based_condition_check      (GDatagramBased   *datagram_based,
                                                                  GIOCondition      condition);
static gboolean     g_socket_datagram_based_condition_wait       (GDatagramBased   *datagram_based,
                                                                  GIOCondition      condition,
                                                                  gint64            timeout,
                                                                  GCancellable     *cancellable,
                                                                  GError          **error);

static GSocketAddress *
cache_recv_address (GSocket *socket, struct sockaddr *native, int native_len);

static gssize
g_socket_receive_message_with_timeout  (GSocket                 *socket,
                                        GSocketAddress         **address,
                                        GInputVector            *vectors,
                                        gint                     num_vectors,
                                        GSocketControlMessage ***messages,
                                        gint                    *num_messages,
                                        gint                    *flags,
                                        gint64                   timeout,
                                        GCancellable            *cancellable,
                                        GError                 **error);
static gint
g_socket_receive_messages_with_timeout (GSocket        *socket,
                                        GInputMessage  *messages,
                                        guint           num_messages,
                                        gint            flags,
                                        gint64          timeout,
                                        GCancellable   *cancellable,
                                        GError        **error);
static gssize
g_socket_send_message_with_timeout     (GSocket                 *socket,
                                        GSocketAddress          *address,
                                        GOutputVector           *vectors,
                                        gint                     num_vectors,
                                        GSocketControlMessage  **messages,
                                        gint                     num_messages,
                                        gint                     flags,
                                        gint64                   timeout,
                                        GCancellable            *cancellable,
                                        GError                 **error);
static gint
g_socket_send_messages_with_timeout    (GSocket        *socket,
                                        GOutputMessage *messages,
                                        guint           num_messages,
                                        gint            flags,
                                        gint64          timeout,
                                        GCancellable   *cancellable,
                                        GError        **error);

enum
{
  PROP_0,
  PROP_FAMILY,
  PROP_TYPE,
  PROP_PROTOCOL,
  PROP_FD,
  PROP_BLOCKING,
  PROP_LISTEN_BACKLOG,
  PROP_KEEPALIVE,
  PROP_LOCAL_ADDRESS,
  PROP_REMOTE_ADDRESS,
  PROP_TIMEOUT,
  PROP_TTL,
  PROP_BROADCAST,
  PROP_MULTICAST_LOOPBACK,
  PROP_MULTICAST_TTL
};

/* Size of the receiver cache for g_socket_receive_from() */
#define RECV_ADDR_CACHE_SIZE 8

struct _GSocketPrivate
{
  GSocketFamily   family;
  GSocketType     type;
  GSocketProtocol protocol;
  gint            fd;
  gint            listen_backlog;
  guint           timeout;
  GError         *construct_error;
  GSocketAddress *remote_address;
  guint           inited : 1;
  guint           blocking : 1;
  guint           keepalive : 1;
  guint           closed : 1;
  guint           connected_read : 1;
  guint           connected_write : 1;
  guint           listening : 1;
  guint           timed_out : 1;
  guint           connect_pending : 1;
#ifdef G_OS_WIN32
  WSAEVENT        event;
  gboolean        waiting;
  DWORD           waiting_result;
  int             current_events;
  int             current_errors;
  int             selected_events;
  GList          *requested_conditions; /* list of requested GIOCondition * */
  GMutex          win32_source_lock;
  GCond           win32_source_cond;
#endif

  struct {
    GSocketAddress *addr;
    struct sockaddr *native;
    gint native_len;
    guint64 last_used;
  } recv_addr_cache[RECV_ADDR_CACHE_SIZE];
};

_G_DEFINE_TYPE_EXTENDED_WITH_PRELUDE (GSocket, g_socket, G_TYPE_OBJECT, 0,
                                      /* Need a prelude for https://bugzilla.gnome.org/show_bug.cgi?id=674885 */
                                      g_type_ensure (G_TYPE_SOCKET_FAMILY);
                                      g_type_ensure (G_TYPE_SOCKET_TYPE);
                                      g_type_ensure (G_TYPE_SOCKET_PROTOCOL);
                                      g_type_ensure (G_TYPE_SOCKET_ADDRESS);
                                      /* And networking init is appropriate for the prelude */
                                      g_networking_init ();
                                      , /* And now the regular type init code */
                                      G_ADD_PRIVATE (GSocket)
                                      G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                             g_socket_initable_iface_init);
                                      G_IMPLEMENT_INTERFACE (G_TYPE_DATAGRAM_BASED,
                                                             g_socket_datagram_based_iface_init));

static int
get_socket_errno (void)
{
#ifndef G_OS_WIN32
  return errno;
#else
  return WSAGetLastError ();
#endif
}

static GIOErrorEnum
socket_io_error_from_errno (int err)
{
#ifdef G_OS_WIN32
  return g_io_error_from_win32_error (err);
#else
  return g_io_error_from_errno (err);
#endif
}

static const char *
socket_strerror (int err)
{
#ifndef G_OS_WIN32
  return g_strerror (err);
#else
  const char *msg_ret;
  char *msg;

  msg = g_win32_error_message (err);

  msg_ret = g_intern_string (msg);
  g_free (msg);

  return msg_ret;
#endif
}

/* Wrapper around g_set_error() to avoid doing excess work */
#define socket_set_error_lazy(err, errsv, fmt)                          \
  G_STMT_START {                                                        \
    GError **__err = (err);                                             \
    int __errsv = (errsv);                                              \
                                                                        \
    if (__err)                                                          \
      {                                                                 \
        int __code = socket_io_error_from_errno (__errsv);              \
        const char *__strerr = socket_strerror (__errsv);               \
                                                                        \
        if (__code == G_IO_ERROR_WOULD_BLOCK)                           \
          g_set_error_literal (__err, G_IO_ERROR, __code, __strerr);    \
        else                                                            \
          g_set_error (__err, G_IO_ERROR, __code, fmt, __strerr);       \
      }                                                                 \
  } G_STMT_END

#ifdef G_OS_WIN32
#define win32_unset_event_mask(_socket, _mask) _win32_unset_event_mask (_socket, _mask)
static void
_win32_unset_event_mask (GSocket *socket, int mask)
{
  g_mutex_lock (&socket->priv->win32_source_lock);
  socket->priv->current_events &= ~mask;
  socket->priv->current_errors &= ~mask;
  g_mutex_unlock (&socket->priv->win32_source_lock);
}
#else
#define win32_unset_event_mask(_socket, _mask)
#endif

/* Windows has broken prototypes... */
#ifdef G_OS_WIN32
#define getsockopt(sockfd, level, optname, optval, optlen) \
  getsockopt (sockfd, level, optname, (gpointer) optval, (int*) optlen)
#define setsockopt(sockfd, level, optname, optval, optlen) \
  setsockopt (sockfd, level, optname, (gpointer) optval, optlen)
#define getsockname(sockfd, addr, addrlen) \
  getsockname (sockfd, addr, (int *)addrlen)
#define getpeername(sockfd, addr, addrlen) \
  getpeername (sockfd, addr, (int *)addrlen)
#define recv(sockfd, buf, len, flags) \
  recv (sockfd, (gpointer)buf, len, flags)
#endif

static gboolean
check_socket (GSocket *socket,
	      GError **error)
{
  if (!socket->priv->inited)
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_INITIALIZED,
                           _("Invalid socket, not initialized"));
      return FALSE;
    }

  if (socket->priv->construct_error)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_INITIALIZED,
		   _("Invalid socket, initialization failed due to: %s"),
		   socket->priv->construct_error->message);
      return FALSE;
    }

  if (socket->priv->closed)
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_CLOSED,
			   _("Socket is already closed"));
      return FALSE;
    }

  return TRUE;
}

static gboolean
check_timeout (GSocket *socket,
	       GError **error)
{
  if (socket->priv->timed_out)
    {
      socket->priv->timed_out = FALSE;
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_TIMED_OUT,
			   _("Socket I/O timed out"));
      return FALSE;
    }

  return TRUE;
}

static void
g_socket_details_from_fd (GSocket *socket)
{
  union {
    struct sockaddr_storage storage;
    struct sockaddr sa;
  } address;
  gint fd;
  guint addrlen;
  int value, family;
  int errsv;

  fd = socket->priv->fd;
  if (!g_socket_get_option (socket, SOL_SOCKET, SO_TYPE, &value, NULL))
    {
      errsv = get_socket_errno ();
      goto err;
    }

  switch (value)
    {
     case SOCK_STREAM:
      socket->priv->type = G_SOCKET_TYPE_STREAM;
      break;

     case SOCK_DGRAM:
      socket->priv->type = G_SOCKET_TYPE_DATAGRAM;
      break;

     case SOCK_SEQPACKET:
      socket->priv->type = G_SOCKET_TYPE_SEQPACKET;
      break;

     default:
      socket->priv->type = G_SOCKET_TYPE_INVALID;
      break;
    }

  addrlen = sizeof address;
  if (getsockname (fd, &address.sa, &addrlen) != 0)
    {
      errsv = get_socket_errno ();
      goto err;
    }

  if (addrlen > 0)
    {
      g_assert (G_STRUCT_OFFSET (struct sockaddr, sa_family) +
		sizeof address.storage.ss_family <= addrlen);
      family = address.storage.ss_family;
    }
  else
    {
      /* On Solaris, this happens if the socket is not yet connected.
       * But we can use SO_DOMAIN as a workaround there.
       */
#ifdef SO_DOMAIN
      if (!g_socket_get_option (socket, SOL_SOCKET, SO_DOMAIN, &family, NULL))
	{
	  errsv = get_socket_errno ();
	  goto err;
	}
#else
      /* This will translate to G_IO_ERROR_FAILED on either unix or windows */
      errsv = -1;
      goto err;
#endif
    }

  switch (family)
    {
     case G_SOCKET_FAMILY_IPV4:
     case G_SOCKET_FAMILY_IPV6:
       socket->priv->family = address.storage.ss_family;
       switch (socket->priv->type)
	 {
	 case G_SOCKET_TYPE_STREAM:
	   socket->priv->protocol = G_SOCKET_PROTOCOL_TCP;
	   break;

	 case G_SOCKET_TYPE_DATAGRAM:
	   socket->priv->protocol = G_SOCKET_PROTOCOL_UDP;
	   break;

	 case G_SOCKET_TYPE_SEQPACKET:
	   socket->priv->protocol = G_SOCKET_PROTOCOL_SCTP;
	   break;

	 default:
	   break;
	 }
       break;

     case G_SOCKET_FAMILY_UNIX:
       socket->priv->family = G_SOCKET_FAMILY_UNIX;
       socket->priv->protocol = G_SOCKET_PROTOCOL_DEFAULT;
       break;

     default:
       socket->priv->family = G_SOCKET_FAMILY_INVALID;
       break;
    }

  if (socket->priv->family != G_SOCKET_FAMILY_INVALID)
    {
      addrlen = sizeof address;
      if (getpeername (fd, &address.sa, &addrlen) >= 0)
        {
          socket->priv->connected_read = TRUE;
          socket->priv->connected_write = TRUE;
        }
    }

  if (g_socket_get_option (socket, SOL_SOCKET, SO_KEEPALIVE, &value, NULL))
    {
      socket->priv->keepalive = !!value;
    }
  else
    {
      /* Can't read, maybe not supported, assume FALSE */
      socket->priv->keepalive = FALSE;
    }

  return;

 err:
  g_set_error (&socket->priv->construct_error, G_IO_ERROR,
	       socket_io_error_from_errno (errsv),
	       _("creating GSocket from fd: %s"),
	       socket_strerror (errsv));
}

/* Wrapper around socket() that is shared with gnetworkmonitornetlink.c */
gint
g_socket (gint     domain,
          gint     type,
          gint     protocol,
          GError **error)
{
  int fd, errsv;

#ifdef SOCK_CLOEXEC
  fd = socket (domain, type | SOCK_CLOEXEC, protocol);
  errsv = errno;
  if (fd != -1)
    return fd;

  /* It's possible that libc has SOCK_CLOEXEC but the kernel does not */
  if (fd < 0 && (errsv == EINVAL || errsv == EPROTOTYPE))
#endif
    fd = socket (domain, type, protocol);

  if (fd < 0)
    {
      int errsv = get_socket_errno ();

      g_set_error (error, G_IO_ERROR, socket_io_error_from_errno (errsv),
		   _("Unable to create socket: %s"), socket_strerror (errsv));
      errno = errsv;
      return -1;
    }

#ifndef G_OS_WIN32
  {
    int flags;

    /* We always want to set close-on-exec to protect users. If you
       need to so some weird inheritance to exec you can re-enable this
       using lower level hacks with g_socket_get_fd(). */
    flags = fcntl (fd, F_GETFD, 0);
    if (flags != -1 &&
	(flags & FD_CLOEXEC) == 0)
      {
	flags |= FD_CLOEXEC;
	fcntl (fd, F_SETFD, flags);
      }
  }
#endif

  return fd;
}

static gint
g_socket_create_socket (GSocketFamily   family,
			GSocketType     type,
			int             protocol,
			GError        **error)
{
  gint native_type;

  switch (type)
    {
     case G_SOCKET_TYPE_STREAM:
      native_type = SOCK_STREAM;
      break;

     case G_SOCKET_TYPE_DATAGRAM:
      native_type = SOCK_DGRAM;
      break;

     case G_SOCKET_TYPE_SEQPACKET:
      native_type = SOCK_SEQPACKET;
      break;

     default:
      g_assert_not_reached ();
    }

  if (family <= 0)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                   _("Unable to create socket: %s"), _("Unknown family was specified"));
      return -1;
    }

  if (protocol == -1)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
		   _("Unable to create socket: %s"), _("Unknown protocol was specified"));
      return -1;
    }

  return g_socket (family, native_type, protocol, error);
}

static void
g_socket_constructed (GObject *object)
{
  GSocket *socket = G_SOCKET (object);

  if (socket->priv->fd >= 0)
    /* create socket->priv info from the fd */
    g_socket_details_from_fd (socket);

  else
    /* create the fd from socket->priv info */
    socket->priv->fd = g_socket_create_socket (socket->priv->family,
					       socket->priv->type,
					       socket->priv->protocol,
					       &socket->priv->construct_error);

  if (socket->priv->fd != -1)
    {
#ifndef G_OS_WIN32
      GError *error = NULL;
#else
      gulong arg;
#endif

      /* Always use native nonblocking sockets, as Windows sets sockets to
       * nonblocking automatically in certain operations. This way we make
       * things work the same on all platforms.
       */
#ifndef G_OS_WIN32
      if (!g_unix_set_fd_nonblocking (socket->priv->fd, TRUE, &error))
        {
          g_warning ("Error setting socket nonblocking: %s", error->message);
          g_clear_error (&error);
        }
#else
      arg = TRUE;

      if (ioctlsocket (socket->priv->fd, FIONBIO, &arg) == SOCKET_ERROR)
        {
          int errsv = get_socket_errno ();
          g_warning ("Error setting socket status flags: %s", socket_strerror (errsv));
        }
#endif

#ifdef SO_NOSIGPIPE
      /* See note about SIGPIPE below. */
      g_socket_set_option (socket, SOL_SOCKET, SO_NOSIGPIPE, TRUE, NULL);
#endif
    }
}

static void
g_socket_get_property (GObject    *object,
		       guint       prop_id,
		       GValue     *value,
		       GParamSpec *pspec)
{
  GSocket *socket = G_SOCKET (object);
  GSocketAddress *address;

  switch (prop_id)
    {
      case PROP_FAMILY:
	g_value_set_enum (value, socket->priv->family);
	break;

      case PROP_TYPE:
	g_value_set_enum (value, socket->priv->type);
	break;

      case PROP_PROTOCOL:
	g_value_set_enum (value, socket->priv->protocol);
	break;

      case PROP_FD:
	g_value_set_int (value, socket->priv->fd);
	break;

      case PROP_BLOCKING:
	g_value_set_boolean (value, socket->priv->blocking);
	break;

      case PROP_LISTEN_BACKLOG:
	g_value_set_int (value, socket->priv->listen_backlog);
	break;

      case PROP_KEEPALIVE:
	g_value_set_boolean (value, socket->priv->keepalive);
	break;

      case PROP_LOCAL_ADDRESS:
	address = g_socket_get_local_address (socket, NULL);
	g_value_take_object (value, address);
	break;

      case PROP_REMOTE_ADDRESS:
	address = g_socket_get_remote_address (socket, NULL);
	g_value_take_object (value, address);
	break;

      case PROP_TIMEOUT:
	g_value_set_uint (value, socket->priv->timeout);
	break;

      case PROP_TTL:
	g_value_set_uint (value, g_socket_get_ttl (socket));
	break;

      case PROP_BROADCAST:
	g_value_set_boolean (value, g_socket_get_broadcast (socket));
	break;

      case PROP_MULTICAST_LOOPBACK:
	g_value_set_boolean (value, g_socket_get_multicast_loopback (socket));
	break;

      case PROP_MULTICAST_TTL:
	g_value_set_uint (value, g_socket_get_multicast_ttl (socket));
	break;

      default:
	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
g_socket_set_property (GObject      *object,
		       guint         prop_id,
		       const GValue *value,
		       GParamSpec   *pspec)
{
  GSocket *socket = G_SOCKET (object);

  switch (prop_id)
    {
      case PROP_FAMILY:
	socket->priv->family = g_value_get_enum (value);
	break;

      case PROP_TYPE:
	socket->priv->type = g_value_get_enum (value);
	break;

      case PROP_PROTOCOL:
	socket->priv->protocol = g_value_get_enum (value);
	break;

      case PROP_FD:
	socket->priv->fd = g_value_get_int (value);
	break;

      case PROP_BLOCKING:
	g_socket_set_blocking (socket, g_value_get_boolean (value));
	break;

      case PROP_LISTEN_BACKLOG:
	g_socket_set_listen_backlog (socket, g_value_get_int (value));
	break;

      case PROP_KEEPALIVE:
	g_socket_set_keepalive (socket, g_value_get_boolean (value));
	break;

      case PROP_TIMEOUT:
	g_socket_set_timeout (socket, g_value_get_uint (value));
	break;

      case PROP_TTL:
	g_socket_set_ttl (socket, g_value_get_uint (value));
	break;

      case PROP_BROADCAST:
	g_socket_set_broadcast (socket, g_value_get_boolean (value));
	break;

      case PROP_MULTICAST_LOOPBACK:
	g_socket_set_multicast_loopback (socket, g_value_get_boolean (value));
	break;

      case PROP_MULTICAST_TTL:
	g_socket_set_multicast_ttl (socket, g_value_get_uint (value));
	break;

      default:
	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
g_socket_finalize (GObject *object)
{
  GSocket *socket = G_SOCKET (object);
  gint i;

  g_clear_error (&socket->priv->construct_error);

  if (socket->priv->fd != -1 &&
      !socket->priv->closed)
    g_socket_close (socket, NULL);

  if (socket->priv->remote_address)
    g_object_unref (socket->priv->remote_address);

#ifdef G_OS_WIN32
  if (socket->priv->event != WSA_INVALID_EVENT)
    {
      WSACloseEvent (socket->priv->event);
      socket->priv->event = WSA_INVALID_EVENT;
    }

  g_assert (socket->priv->requested_conditions == NULL);
  g_mutex_clear (&socket->priv->win32_source_lock);
  g_cond_clear (&socket->priv->win32_source_cond);
#endif

  for (i = 0; i < RECV_ADDR_CACHE_SIZE; i++)
    {
      if (socket->priv->recv_addr_cache[i].addr)
        {
          g_object_unref (socket->priv->recv_addr_cache[i].addr);
          g_free (socket->priv->recv_addr_cache[i].native);
        }
    }

  if (G_OBJECT_CLASS (g_socket_parent_class)->finalize)
    (*G_OBJECT_CLASS (g_socket_parent_class)->finalize) (object);
}

static void
g_socket_class_init (GSocketClass *klass)
{
  GObjectClass *gobject_class G_GNUC_UNUSED = G_OBJECT_CLASS (klass);

#ifdef SIGPIPE
  /* There is no portable, thread-safe way to avoid having the process
   * be killed by SIGPIPE when calling send() or sendmsg(), so we are
   * forced to simply ignore the signal process-wide.
   *
   * Even if we ignore it though, gdb will still stop if the app
   * receives a SIGPIPE, which can be confusing and annoying. So when
   * possible, we also use MSG_NOSIGNAL / SO_NOSIGPIPE elsewhere to
   * prevent the signal from occurring at all.
   */
  signal (SIGPIPE, SIG_IGN);
#endif

  gobject_class->finalize = g_socket_finalize;
  gobject_class->constructed = g_socket_constructed;
  gobject_class->set_property = g_socket_set_property;
  gobject_class->get_property = g_socket_get_property;

  g_object_class_install_property (gobject_class, PROP_FAMILY,
				   g_param_spec_enum ("family",
						      P_("Socket family"),
						      P_("The sockets address family"),
						      G_TYPE_SOCKET_FAMILY,
						      G_SOCKET_FAMILY_INVALID,
						      G_PARAM_CONSTRUCT_ONLY |
                                                      G_PARAM_READWRITE |
                                                      G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_TYPE,
				   g_param_spec_enum ("type",
						      P_("Socket type"),
						      P_("The sockets type"),
						      G_TYPE_SOCKET_TYPE,
						      G_SOCKET_TYPE_STREAM,
						      G_PARAM_CONSTRUCT_ONLY |
                                                      G_PARAM_READWRITE |
                                                      G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_PROTOCOL,
				   g_param_spec_enum ("protocol",
						      P_("Socket protocol"),
						      P_("The id of the protocol to use, or -1 for unknown"),
						      G_TYPE_SOCKET_PROTOCOL,
						      G_SOCKET_PROTOCOL_UNKNOWN,
						      G_PARAM_CONSTRUCT_ONLY |
                                                      G_PARAM_READWRITE |
                                                      G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_FD,
				   g_param_spec_int ("fd",
						     P_("File descriptor"),
						     P_("The sockets file descriptor"),
						     G_MININT,
						     G_MAXINT,
						     -1,
						     G_PARAM_CONSTRUCT_ONLY |
                                                     G_PARAM_READWRITE |
                                                     G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_BLOCKING,
				   g_param_spec_boolean ("blocking",
							 P_("blocking"),
							 P_("Whether or not I/O on this socket is blocking"),
							 TRUE,
							 G_PARAM_READWRITE |
                                                         G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_LISTEN_BACKLOG,
				   g_param_spec_int ("listen-backlog",
						     P_("Listen backlog"),
						     P_("Outstanding connections in the listen queue"),
						     0,
						     SOMAXCONN,
						     10,
						     G_PARAM_READWRITE |
                                                     G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_KEEPALIVE,
				   g_param_spec_boolean ("keepalive",
							 P_("Keep connection alive"),
							 P_("Keep connection alive by sending periodic pings"),
							 FALSE,
							 G_PARAM_READWRITE |
                                                         G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_LOCAL_ADDRESS,
				   g_param_spec_object ("local-address",
							P_("Local address"),
							P_("The local address the socket is bound to"),
							G_TYPE_SOCKET_ADDRESS,
							G_PARAM_READABLE |
                                                        G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_REMOTE_ADDRESS,
				   g_param_spec_object ("remote-address",
							P_("Remote address"),
							P_("The remote address the socket is connected to"),
							G_TYPE_SOCKET_ADDRESS,
							G_PARAM_READABLE |
                                                        G_PARAM_STATIC_STRINGS));

  /**
   * GSocket:timeout:
   *
   * The timeout in seconds on socket I/O
   *
   * Since: 2.26
   */
  g_object_class_install_property (gobject_class, PROP_TIMEOUT,
				   g_param_spec_uint ("timeout",
						      P_("Timeout"),
						      P_("The timeout in seconds on socket I/O"),
						      0,
						      G_MAXUINT,
						      0,
						      G_PARAM_READWRITE |
						      G_PARAM_STATIC_STRINGS));

  /**
   * GSocket:broadcast:
   *
   * Whether the socket should allow sending to broadcast addresses.
   *
   * Since: 2.32
   */
  g_object_class_install_property (gobject_class, PROP_BROADCAST,
				   g_param_spec_boolean ("broadcast",
							 P_("Broadcast"),
							 P_("Whether to allow sending to broadcast addresses"),
							 FALSE,
							 G_PARAM_READWRITE |
                                                         G_PARAM_STATIC_STRINGS));

  /**
   * GSocket:ttl:
   *
   * Time-to-live for outgoing unicast packets
   *
   * Since: 2.32
   */
  g_object_class_install_property (gobject_class, PROP_TTL,
				   g_param_spec_uint ("ttl",
						      P_("TTL"),
						      P_("Time-to-live of outgoing unicast packets"),
						      0, G_MAXUINT, 0,
						      G_PARAM_READWRITE |
						      G_PARAM_STATIC_STRINGS));

  /**
   * GSocket:multicast-loopback:
   *
   * Whether outgoing multicast packets loop back to the local host.
   *
   * Since: 2.32
   */
  g_object_class_install_property (gobject_class, PROP_MULTICAST_LOOPBACK,
				   g_param_spec_boolean ("multicast-loopback",
							 P_("Multicast loopback"),
							 P_("Whether outgoing multicast packets loop back to the local host"),
							 TRUE,
							 G_PARAM_READWRITE |
                                                         G_PARAM_STATIC_STRINGS));

  /**
   * GSocket:multicast-ttl:
   *
   * Time-to-live out outgoing multicast packets
   *
   * Since: 2.32
   */
  g_object_class_install_property (gobject_class, PROP_MULTICAST_TTL,
				   g_param_spec_uint ("multicast-ttl",
						      P_("Multicast TTL"),
						      P_("Time-to-live of outgoing multicast packets"),
						      0, G_MAXUINT, 1,
						      G_PARAM_READWRITE |
						      G_PARAM_STATIC_STRINGS));
}

static void
g_socket_initable_iface_init (GInitableIface *iface)
{
  iface->init = g_socket_initable_init;
}

static void
g_socket_datagram_based_iface_init (GDatagramBasedInterface *iface)
{
  iface->receive_messages = g_socket_datagram_based_receive_messages;
  iface->send_messages = g_socket_datagram_based_send_messages;
  iface->create_source = g_socket_datagram_based_create_source;
  iface->condition_check = g_socket_datagram_based_condition_check;
  iface->condition_wait = g_socket_datagram_based_condition_wait;
}

static void
g_socket_init (GSocket *socket)
{
  socket->priv = g_socket_get_instance_private (socket);

  socket->priv->fd = -1;
  socket->priv->blocking = TRUE;
  socket->priv->listen_backlog = 10;
  socket->priv->construct_error = NULL;
#ifdef G_OS_WIN32
  socket->priv->event = WSA_INVALID_EVENT;
  g_mutex_init (&socket->priv->win32_source_lock);
  g_cond_init (&socket->priv->win32_source_cond);
#endif
}

static gboolean
g_socket_initable_init (GInitable *initable,
			GCancellable *cancellable,
			GError  **error)
{
  GSocket  *socket;

  g_return_val_if_fail (G_IS_SOCKET (initable), FALSE);

  socket = G_SOCKET (initable);

  if (cancellable != NULL)
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                           _("Cancellable initialization not supported"));
      return FALSE;
    }

  socket->priv->inited = TRUE;

  if (socket->priv->construct_error)
    {
      if (error)
	*error = g_error_copy (socket->priv->construct_error);
      return FALSE;
    }


  return TRUE;
}

static gboolean
check_datagram_based (GDatagramBased  *self,
                      GError         **error)
{
  switch (g_socket_get_socket_type (G_SOCKET (self)))
    {
    case G_SOCKET_TYPE_INVALID:
    case G_SOCKET_TYPE_STREAM:
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                   _("Cannot use datagram operations on a non-datagram "
                     "socket."));
      return FALSE;
    case G_SOCKET_TYPE_DATAGRAM:
    case G_SOCKET_TYPE_SEQPACKET:
      /* Fall through. */
      break;
    }

  /* Due to us sharing #GSocketSource with the #GSocket implementation, it is
   * pretty tricky to split out #GSocket:timeout so that it does not affect
   * #GDatagramBased operations (but still affects #GSocket operations). It is
   * not worth that effort — just disallow it and require the user to specify
   * timeouts on a per-operation basis. */
  if (g_socket_get_timeout (G_SOCKET (self)) != 0)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                   _("Cannot use datagram operations on a socket with a "
                     "timeout set."));
      return FALSE;
    }

  return TRUE;
}

static gint
g_socket_datagram_based_receive_messages (GDatagramBased  *self,
                                          GInputMessage   *messages,
                                          guint            num_messages,
                                          gint             flags,
                                          gint64           timeout,
                                          GCancellable    *cancellable,
                                          GError         **error)
{
  if (!check_datagram_based (self, error))
    return FALSE;

  return g_socket_receive_messages_with_timeout (G_SOCKET (self), messages,
                                                 num_messages, flags, timeout,
                                                 cancellable, error);
}

static gint
g_socket_datagram_based_send_messages (GDatagramBased  *self,
                                       GOutputMessage  *messages,
                                       guint            num_messages,
                                       gint             flags,
                                       gint64           timeout,
                                       GCancellable    *cancellable,
                                       GError         **error)
{
  if (!check_datagram_based (self, error))
    return FALSE;

  return g_socket_send_messages_with_timeout (G_SOCKET (self), messages,
                                              num_messages, flags, timeout,
                                              cancellable, error);
}

static GSource *
g_socket_datagram_based_create_source (GDatagramBased  *self,
                                       GIOCondition     condition,
                                       GCancellable    *cancellable)
{
  if (!check_datagram_based (self, NULL))
    return NULL;

  return g_socket_create_source (G_SOCKET (self), condition, cancellable);
}

static GIOCondition
g_socket_datagram_based_condition_check (GDatagramBased  *datagram_based,
                                         GIOCondition     condition)
{
  if (!check_datagram_based (datagram_based, NULL))
    return G_IO_ERR;

  return g_socket_condition_check (G_SOCKET (datagram_based), condition);
}

static gboolean
g_socket_datagram_based_condition_wait (GDatagramBased  *datagram_based,
                                        GIOCondition     condition,
                                        gint64           timeout,
                                        GCancellable    *cancellable,
                                        GError         **error)
{
  if (!check_datagram_based (datagram_based, error))
    return FALSE;

  return g_socket_condition_timed_wait (G_SOCKET (datagram_based), condition,
                                        timeout, cancellable, error);
}

/**
 * g_socket_new:
 * @family: the socket family to use, e.g. %G_SOCKET_FAMILY_IPV4.
 * @type: the socket type to use.
 * @protocol: the id of the protocol to use, or 0 for default.
 * @error: #GError for error reporting, or %NULL to ignore.
 *
 * Creates a new #GSocket with the defined family, type and protocol.
 * If @protocol is 0 (%G_SOCKET_PROTOCOL_DEFAULT) the default protocol type
 * for the family and type is used.
 *
 * The @protocol is a family and type specific int that specifies what
 * kind of protocol to use. #GSocketProtocol lists several common ones.
 * Many families only support one protocol, and use 0 for this, others
 * support several and using 0 means to use the default protocol for
 * the family and type.
 *
 * The protocol id is passed directly to the operating
 * system, so you can use protocols not listed in #GSocketProtocol if you
 * know the protocol number used for it.
 *
 * Returns: a #GSocket or %NULL on error.
 *     Free the returned object with g_object_unref().
 *
 * Since: 2.22
 */
GSocket *
g_socket_new (GSocketFamily     family,
	      GSocketType       type,
	      GSocketProtocol   protocol,
	      GError          **error)
{
  return G_SOCKET (g_initable_new (G_TYPE_SOCKET,
				   NULL, error,
				   "family", family,
				   "type", type,
				   "protocol", protocol,
				   NULL));
}

/**
 * g_socket_new_from_fd:
 * @fd: a native socket file descriptor.
 * @error: #GError for error reporting, or %NULL to ignore.
 *
 * Creates a new #GSocket from a native file descriptor
 * or winsock SOCKET handle.
 *
 * This reads all the settings from the file descriptor so that
 * all properties should work. Note that the file descriptor
 * will be set to non-blocking mode, independent on the blocking
 * mode of the #GSocket.
 *
 * On success, the returned #GSocket takes ownership of @fd. On failure, the
 * caller must close @fd themselves.
 *
 * Since GLib 2.46, it is no longer a fatal error to call this on a non-socket
 * descriptor.  Instead, a GError will be set with code %G_IO_ERROR_FAILED
 *
 * Returns: a #GSocket or %NULL on error.
 *     Free the returned object with g_object_unref().
 *
 * Since: 2.22
 */
GSocket *
g_socket_new_from_fd (gint     fd,
		      GError **error)
{
  return G_SOCKET (g_initable_new (G_TYPE_SOCKET,
				   NULL, error,
				   "fd", fd,
				   NULL));
}

/**
 * g_socket_set_blocking:
 * @socket: a #GSocket.
 * @blocking: Whether to use blocking I/O or not.
 *
 * Sets the blocking mode of the socket. In blocking mode
 * all operations (which don’t take an explicit blocking parameter) block until
 * they succeed or there is an error. In
 * non-blocking mode all functions return results immediately or
 * with a %G_IO_ERROR_WOULD_BLOCK error.
 *
 * All sockets are created in blocking mode. However, note that the
 * platform level socket is always non-blocking, and blocking mode
 * is a GSocket level feature.
 *
 * Since: 2.22
 */
void
g_socket_set_blocking (GSocket  *socket,
		       gboolean  blocking)
{
  g_return_if_fail (G_IS_SOCKET (socket));

  blocking = !!blocking;

  if (socket->priv->blocking == blocking)
    return;

  socket->priv->blocking = blocking;
  g_object_notify (G_OBJECT (socket), "blocking");
}

/**
 * g_socket_get_blocking:
 * @socket: a #GSocket.
 *
 * Gets the blocking mode of the socket. For details on blocking I/O,
 * see g_socket_set_blocking().
 *
 * Returns: %TRUE if blocking I/O is used, %FALSE otherwise.
 *
 * Since: 2.22
 */
gboolean
g_socket_get_blocking (GSocket *socket)
{
  g_return_val_if_fail (G_IS_SOCKET (socket), FALSE);

  return socket->priv->blocking;
}

/**
 * g_socket_set_keepalive:
 * @socket: a #GSocket.
 * @keepalive: Value for the keepalive flag
 *
 * Sets or unsets the %SO_KEEPALIVE flag on the underlying socket. When
 * this flag is set on a socket, the system will attempt to verify that the
 * remote socket endpoint is still present if a sufficiently long period of
 * time passes with no data being exchanged. If the system is unable to
 * verify the presence of the remote endpoint, it will automatically close
 * the connection.
 *
 * This option is only functional on certain kinds of sockets. (Notably,
 * %G_SOCKET_PROTOCOL_TCP sockets.)
 *
 * The exact time between pings is system- and protocol-dependent, but will
 * normally be at least two hours. Most commonly, you would set this flag
 * on a server socket if you want to allow clients to remain idle for long
 * periods of time, but also want to ensure that connections are eventually
 * garbage-collected if clients crash or become unreachable.
 *
 * Since: 2.22
 */
void
g_socket_set_keepalive (GSocket  *socket,
			gboolean  keepalive)
{
  GError *error = NULL;

  g_return_if_fail (G_IS_SOCKET (socket));

  keepalive = !!keepalive;
  if (socket->priv->keepalive == keepalive)
    return;

  if (!g_socket_set_option (socket, SOL_SOCKET, SO_KEEPALIVE,
			    keepalive, &error))
    {
      g_warning ("error setting keepalive: %s", error->message);
      g_error_free (error);
      return;
    }

  socket->priv->keepalive = keepalive;
  g_object_notify (G_OBJECT (socket), "keepalive");
}

/**
 * g_socket_get_keepalive:
 * @socket: a #GSocket.
 *
 * Gets the keepalive mode of the socket. For details on this,
 * see g_socket_set_keepalive().
 *
 * Returns: %TRUE if keepalive is active, %FALSE otherwise.
 *
 * Since: 2.22
 */
gboolean
g_socket_get_keepalive (GSocket *socket)
{
  g_return_val_if_fail (G_IS_SOCKET (socket), FALSE);

  return socket->priv->keepalive;
}

/**
 * g_socket_get_listen_backlog:
 * @socket: a #GSocket.
 *
 * Gets the listen backlog setting of the socket. For details on this,
 * see g_socket_set_listen_backlog().
 *
 * Returns: the maximum number of pending connections.
 *
 * Since: 2.22
 */
gint
g_socket_get_listen_backlog  (GSocket *socket)
{
  g_return_val_if_fail (G_IS_SOCKET (socket), 0);

  return socket->priv->listen_backlog;
}

/**
 * g_socket_set_listen_backlog:
 * @socket: a #GSocket.
 * @backlog: the maximum number of pending connections.
 *
 * Sets the maximum number of outstanding connections allowed
 * when listening on this socket. If more clients than this are
 * connecting to the socket and the application is not handling them
 * on time then the new connections will be refused.
 *
 * Note that this must be called before g_socket_listen() and has no
 * effect if called after that.
 *
 * Since: 2.22
 */
void
g_socket_set_listen_backlog (GSocket *socket,
			     gint     backlog)
{
  g_return_if_fail (G_IS_SOCKET (socket));
  g_return_if_fail (!socket->priv->listening);

  if (backlog != socket->priv->listen_backlog)
    {
      socket->priv->listen_backlog = backlog;
      g_object_notify (G_OBJECT (socket), "listen-backlog");
    }
}

/**
 * g_socket_get_timeout:
 * @socket: a #GSocket.
 *
 * Gets the timeout setting of the socket. For details on this, see
 * g_socket_set_timeout().
 *
 * Returns: the timeout in seconds
 *
 * Since: 2.26
 */
guint
g_socket_get_timeout (GSocket *socket)
{
  g_return_val_if_fail (G_IS_SOCKET (socket), 0);

  return socket->priv->timeout;
}

/**
 * g_socket_set_timeout:
 * @socket: a #GSocket.
 * @timeout: the timeout for @socket, in seconds, or 0 for none
 *
 * Sets the time in seconds after which I/O operations on @socket will
 * time out if they have not yet completed.
 *
 * On a blocking socket, this means that any blocking #GSocket
 * operation will time out after @timeout seconds of inactivity,
 * returning %G_IO_ERROR_TIMED_OUT.
 *
 * On a non-blocking socket, calls to g_socket_condition_wait() will
 * also fail with %G_IO_ERROR_TIMED_OUT after the given time. Sources
 * created with g_socket_create_source() will trigger after
 * @timeout seconds of inactivity, with the requested condition
 * set, at which point calling g_socket_receive(), g_socket_send(),
 * g_socket_check_connect_result(), etc, will fail with
 * %G_IO_ERROR_TIMED_OUT.
 *
 * If @timeout is 0 (the default), operations will never time out
 * on their own.
 *
 * Note that if an I/O operation is interrupted by a signal, this may
 * cause the timeout to be reset.
 *
 * Since: 2.26
 */
void
g_socket_set_timeout (GSocket *socket,
		      guint    timeout)
{
  g_return_if_fail (G_IS_SOCKET (socket));

  if (timeout != socket->priv->timeout)
    {
      socket->priv->timeout = timeout;
      g_object_notify (G_OBJECT (socket), "timeout");
    }
}

/**
 * g_socket_get_ttl:
 * @socket: a #GSocket.
 *
 * Gets the unicast time-to-live setting on @socket; see
 * g_socket_set_ttl() for more details.
 *
 * Returns: the time-to-live setting on @socket
 *
 * Since: 2.32
 */
guint
g_socket_get_ttl (GSocket *socket)
{
  GError *error = NULL;
  gint value;

  g_return_val_if_fail (G_IS_SOCKET (socket), 0);

  if (socket->priv->family == G_SOCKET_FAMILY_IPV4)
    {
      g_socket_get_option (socket, IPPROTO_IP, IP_TTL,
			   &value, &error);
    }
  else if (socket->priv->family == G_SOCKET_FAMILY_IPV6)
    {
      g_socket_get_option (socket, IPPROTO_IPV6, IPV6_UNICAST_HOPS,
			   &value, &error);
    }
  else
    g_return_val_if_reached (0);

  if (error)
    {
      g_warning ("error getting unicast ttl: %s", error->message);
      g_error_free (error);
      return 0;
    }

  return value;
}

/**
 * g_socket_set_ttl:
 * @socket: a #GSocket.
 * @ttl: the time-to-live value for all unicast packets on @socket
 *
 * Sets the time-to-live for outgoing unicast packets on @socket.
 * By default the platform-specific default value is used.
 *
 * Since: 2.32
 */
void
g_socket_set_ttl (GSocket  *socket,
                  guint     ttl)
{
  GError *error = NULL;

  g_return_if_fail (G_IS_SOCKET (socket));

  if (socket->priv->family == G_SOCKET_FAMILY_IPV4)
    {
      g_socket_set_option (socket, IPPROTO_IP, IP_TTL,
			   ttl, &error);
    }
  else if (socket->priv->family == G_SOCKET_FAMILY_IPV6)
    {
      g_socket_set_option (socket, IPPROTO_IP, IP_TTL,
			   ttl, NULL);
      g_socket_set_option (socket, IPPROTO_IPV6, IPV6_UNICAST_HOPS,
			   ttl, &error);
    }
  else
    g_return_if_reached ();

  if (error)
    {
      g_warning ("error setting unicast ttl: %s", error->message);
      g_error_free (error);
      return;
    }

  g_object_notify (G_OBJECT (socket), "ttl");
}

/**
 * g_socket_get_broadcast:
 * @socket: a #GSocket.
 *
 * Gets the broadcast setting on @socket; if %TRUE,
 * it is possible to send packets to broadcast
 * addresses.
 *
 * Returns: the broadcast setting on @socket
 *
 * Since: 2.32
 */
gboolean
g_socket_get_broadcast (GSocket *socket)
{
  GError *error = NULL;
  gint value;

  g_return_val_if_fail (G_IS_SOCKET (socket), FALSE);

  if (!g_socket_get_option (socket, SOL_SOCKET, SO_BROADCAST,
			    &value, &error))
    {
      g_warning ("error getting broadcast: %s", error->message);
      g_error_free (error);
      return FALSE;
    }

  return !!value;
}

/**
 * g_socket_set_broadcast:
 * @socket: a #GSocket.
 * @broadcast: whether @socket should allow sending to broadcast
 *     addresses
 *
 * Sets whether @socket should allow sending to broadcast addresses.
 * This is %FALSE by default.
 *
 * Since: 2.32
 */
void
g_socket_set_broadcast (GSocket    *socket,
       	                gboolean    broadcast)
{
  GError *error = NULL;

  g_return_if_fail (G_IS_SOCKET (socket));

  broadcast = !!broadcast;

  if (!g_socket_set_option (socket, SOL_SOCKET, SO_BROADCAST,
			    broadcast, &error))
    {
      g_warning ("error setting broadcast: %s", error->message);
      g_error_free (error);
      return;
    }

  g_object_notify (G_OBJECT (socket), "broadcast");
}

/**
 * g_socket_get_multicast_loopback:
 * @socket: a #GSocket.
 *
 * Gets the multicast loopback setting on @socket; if %TRUE (the
 * default), outgoing multicast packets will be looped back to
 * multicast listeners on the same host.
 *
 * Returns: the multicast loopback setting on @socket
 *
 * Since: 2.32
 */
gboolean
g_socket_get_multicast_loopback (GSocket *socket)
{
  GError *error = NULL;
  gint value;

  g_return_val_if_fail (G_IS_SOCKET (socket), FALSE);

  if (socket->priv->family == G_SOCKET_FAMILY_IPV4)
    {
      g_socket_get_option (socket, IPPROTO_IP, IP_MULTICAST_LOOP,
			   &value, &error);
    }
  else if (socket->priv->family == G_SOCKET_FAMILY_IPV6)
    {
      g_socket_get_option (socket, IPPROTO_IPV6, IPV6_MULTICAST_LOOP,
			   &value, &error);
    }
  else
    g_return_val_if_reached (FALSE);

  if (error)
    {
      g_warning ("error getting multicast loopback: %s", error->message);
      g_error_free (error);
      return FALSE;
    }

  return !!value;
}

/**
 * g_socket_set_multicast_loopback:
 * @socket: a #GSocket.
 * @loopback: whether @socket should receive messages sent to its
 *   multicast groups from the local host
 *
 * Sets whether outgoing multicast packets will be received by sockets
 * listening on that multicast address on the same host. This is %TRUE
 * by default.
 *
 * Since: 2.32
 */
void
g_socket_set_multicast_loopback (GSocket    *socket,
				 gboolean    loopback)
{
  GError *error = NULL;

  g_return_if_fail (G_IS_SOCKET (socket));

  loopback = !!loopback;

  if (socket->priv->family == G_SOCKET_FAMILY_IPV4)
    {
      g_socket_set_option (socket, IPPROTO_IP, IP_MULTICAST_LOOP,
			   loopback, &error);
    }
  else if (socket->priv->family == G_SOCKET_FAMILY_IPV6)
    {
      g_socket_set_option (socket, IPPROTO_IP, IP_MULTICAST_LOOP,
			   loopback, NULL);
      g_socket_set_option (socket, IPPROTO_IPV6, IPV6_MULTICAST_LOOP,
			   loopback, &error);
    }
  else
    g_return_if_reached ();

  if (error)
    {
      g_warning ("error setting multicast loopback: %s", error->message);
      g_error_free (error);
      return;
    }

  g_object_notify (G_OBJECT (socket), "multicast-loopback");
}

/**
 * g_socket_get_multicast_ttl:
 * @socket: a #GSocket.
 *
 * Gets the multicast time-to-live setting on @socket; see
 * g_socket_set_multicast_ttl() for more details.
 *
 * Returns: the multicast time-to-live setting on @socket
 *
 * Since: 2.32
 */
guint
g_socket_get_multicast_ttl (GSocket *socket)
{
  GError *error = NULL;
  gint value;

  g_return_val_if_fail (G_IS_SOCKET (socket), 0);

  if (socket->priv->family == G_SOCKET_FAMILY_IPV4)
    {
      g_socket_get_option (socket, IPPROTO_IP, IP_MULTICAST_TTL,
			   &value, &error);
    }
  else if (socket->priv->family == G_SOCKET_FAMILY_IPV6)
    {
      g_socket_get_option (socket, IPPROTO_IPV6, IPV6_MULTICAST_HOPS,
			   &value, &error);
    }
  else
    g_return_val_if_reached (FALSE);

  if (error)
    {
      g_warning ("error getting multicast ttl: %s", error->message);
      g_error_free (error);
      return FALSE;
    }

  return value;
}

/**
 * g_socket_set_multicast_ttl:
 * @socket: a #GSocket.
 * @ttl: the time-to-live value for all multicast datagrams on @socket
 *
 * Sets the time-to-live for outgoing multicast datagrams on @socket.
 * By default, this is 1, meaning that multicast packets will not leave
 * the local network.
 *
 * Since: 2.32
 */
void
g_socket_set_multicast_ttl (GSocket  *socket,
                            guint     ttl)
{
  GError *error = NULL;

  g_return_if_fail (G_IS_SOCKET (socket));

  if (socket->priv->family == G_SOCKET_FAMILY_IPV4)
    {
      g_socket_set_option (socket, IPPROTO_IP, IP_MULTICAST_TTL,
			   ttl, &error);
    }
  else if (socket->priv->family == G_SOCKET_FAMILY_IPV6)
    {
      g_socket_set_option (socket, IPPROTO_IP, IP_MULTICAST_TTL,
			   ttl, NULL);
      g_socket_set_option (socket, IPPROTO_IPV6, IPV6_MULTICAST_HOPS,
			   ttl, &error);
    }
  else
    g_return_if_reached ();

  if (error)
    {
      g_warning ("error setting multicast ttl: %s", error->message);
      g_error_free (error);
      return;
    }

  g_object_notify (G_OBJECT (socket), "multicast-ttl");
}

/**
 * g_socket_get_family:
 * @socket: a #GSocket.
 *
 * Gets the socket family of the socket.
 *
 * Returns: a #GSocketFamily
 *
 * Since: 2.22
 */
GSocketFamily
g_socket_get_family (GSocket *socket)
{
  g_return_val_if_fail (G_IS_SOCKET (socket), G_SOCKET_FAMILY_INVALID);

  return socket->priv->family;
}

/**
 * g_socket_get_socket_type:
 * @socket: a #GSocket.
 *
 * Gets the socket type of the socket.
 *
 * Returns: a #GSocketType
 *
 * Since: 2.22
 */
GSocketType
g_socket_get_socket_type (GSocket *socket)
{
  g_return_val_if_fail (G_IS_SOCKET (socket), G_SOCKET_TYPE_INVALID);

  return socket->priv->type;
}

/**
 * g_socket_get_protocol:
 * @socket: a #GSocket.
 *
 * Gets the socket protocol id the socket was created with.
 * In case the protocol is unknown, -1 is returned.
 *
 * Returns: a protocol id, or -1 if unknown
 *
 * Since: 2.22
 */
GSocketProtocol
g_socket_get_protocol (GSocket *socket)
{
  g_return_val_if_fail (G_IS_SOCKET (socket), -1);

  return socket->priv->protocol;
}

/**
 * g_socket_get_fd:
 * @socket: a #GSocket.
 *
 * Returns the underlying OS socket object. On unix this
 * is a socket file descriptor, and on Windows this is
 * a Winsock2 SOCKET handle. This may be useful for
 * doing platform specific or otherwise unusual operations
 * on the socket.
 *
 * Returns: the file descriptor of the socket.
 *
 * Since: 2.22
 */
int
g_socket_get_fd (GSocket *socket)
{
  g_return_val_if_fail (G_IS_SOCKET (socket), -1);

  return socket->priv->fd;
}

/**
 * g_socket_get_local_address:
 * @socket: a #GSocket.
 * @error: #GError for error reporting, or %NULL to ignore.
 *
 * Try to get the local address of a bound socket. This is only
 * useful if the socket has been bound to a local address,
 * either explicitly or implicitly when connecting.
 *
 * Returns: (transfer full): a #GSocketAddress or %NULL on error.
 *     Free the returned object with g_object_unref().
 *
 * Since: 2.22
 */
GSocketAddress *
g_socket_get_local_address (GSocket  *socket,
			    GError  **error)
{
  union {
    struct sockaddr_storage storage;
    struct sockaddr sa;
  } buffer;
  guint len = sizeof (buffer);

  g_return_val_if_fail (G_IS_SOCKET (socket), NULL);

  if (getsockname (socket->priv->fd, &buffer.sa, &len) < 0)
    {
      int errsv = get_socket_errno ();
      g_set_error (error, G_IO_ERROR, socket_io_error_from_errno (errsv),
		   _("could not get local address: %s"), socket_strerror (errsv));
      return NULL;
    }

  return g_socket_address_new_from_native (&buffer.storage, len);
}

/**
 * g_socket_get_remote_address:
 * @socket: a #GSocket.
 * @error: #GError for error reporting, or %NULL to ignore.
 *
 * Try to get the remote address of a connected socket. This is only
 * useful for connection oriented sockets that have been connected.
 *
 * Returns: (transfer full): a #GSocketAddress or %NULL on error.
 *     Free the returned object with g_object_unref().
 *
 * Since: 2.22
 */
GSocketAddress *
g_socket_get_remote_address (GSocket  *socket,
			     GError  **error)
{
  union {
    struct sockaddr_storage storage;
    struct sockaddr sa;
  } buffer;
  guint len = sizeof (buffer);

  g_return_val_if_fail (G_IS_SOCKET (socket), NULL);

  if (socket->priv->connect_pending)
    {
      if (!g_socket_check_connect_result (socket, error))
        return NULL;
      else
        socket->priv->connect_pending = FALSE;
    }

  if (!socket->priv->remote_address)
    {
      if (getpeername (socket->priv->fd, &buffer.sa, &len) < 0)
	{
	  int errsv = get_socket_errno ();
	  g_set_error (error, G_IO_ERROR, socket_io_error_from_errno (errsv),
		       _("could not get remote address: %s"), socket_strerror (errsv));
	  return NULL;
	}

      socket->priv->remote_address = g_socket_address_new_from_native (&buffer.storage, len);
    }

  return g_object_ref (socket->priv->remote_address);
}

/**
 * g_socket_is_connected:
 * @socket: a #GSocket.
 *
 * Check whether the socket is connected. This is only useful for
 * connection-oriented sockets.
 *
 * If using g_socket_shutdown(), this function will return %TRUE until the
 * socket has been shut down for reading and writing. If you do a non-blocking
 * connect, this function will not return %TRUE until after you call
 * g_socket_check_connect_result().
 *
 * Returns: %TRUE if socket is connected, %FALSE otherwise.
 *
 * Since: 2.22
 */
gboolean
g_socket_is_connected (GSocket *socket)
{
  g_return_val_if_fail (G_IS_SOCKET (socket), FALSE);

  return (socket->priv->connected_read || socket->priv->connected_write);
}

/**
 * g_socket_listen:
 * @socket: a #GSocket.
 * @error: #GError for error reporting, or %NULL to ignore.
 *
 * Marks the socket as a server socket, i.e. a socket that is used
 * to accept incoming requests using g_socket_accept().
 *
 * Before calling this the socket must be bound to a local address using
 * g_socket_bind().
 *
 * To set the maximum amount of outstanding clients, use
 * g_socket_set_listen_backlog().
 *
 * Returns: %TRUE on success, %FALSE on error.
 *
 * Since: 2.22
 */
gboolean
g_socket_listen (GSocket  *socket,
		 GError  **error)
{
  g_return_val_if_fail (G_IS_SOCKET (socket), FALSE);

  if (!check_socket (socket, error))
    return FALSE;

  if (listen (socket->priv->fd, socket->priv->listen_backlog) < 0)
    {
      int errsv = get_socket_errno ();

      g_set_error (error, G_IO_ERROR, socket_io_error_from_errno (errsv),
		   _("could not listen: %s"), socket_strerror (errsv));
      return FALSE;
    }

  socket->priv->listening = TRUE;

  return TRUE;
}

/**
 * g_socket_bind:
 * @socket: a #GSocket.
 * @address: a #GSocketAddress specifying the local address.
 * @allow_reuse: whether to allow reusing this address
 * @error: #GError for error reporting, or %NULL to ignore.
 *
 * When a socket is created it is attached to an address family, but it
 * doesn't have an address in this family. g_socket_bind() assigns the
 * address (sometimes called name) of the socket.
 *
 * It is generally required to bind to a local address before you can
 * receive connections. (See g_socket_listen() and g_socket_accept() ).
 * In certain situations, you may also want to bind a socket that will be
 * used to initiate connections, though this is not normally required.
 *
 * If @socket is a TCP socket, then @allow_reuse controls the setting
 * of the `SO_REUSEADDR` socket option; normally it should be %TRUE for
 * server sockets (sockets that you will eventually call
 * g_socket_accept() on), and %FALSE for client sockets. (Failing to
 * set this flag on a server socket may cause g_socket_bind() to return
 * %G_IO_ERROR_ADDRESS_IN_USE if the server program is stopped and then
 * immediately restarted.)
 *
 * If @socket is a UDP socket, then @allow_reuse determines whether or
 * not other UDP sockets can be bound to the same address at the same
 * time. In particular, you can have several UDP sockets bound to the
 * same address, and they will all receive all of the multicast and
 * broadcast packets sent to that address. (The behavior of unicast
 * UDP packets to an address with multiple listeners is not defined.)
 *
 * Returns: %TRUE on success, %FALSE on error.
 *
 * Since: 2.22
 */
gboolean
g_socket_bind (GSocket         *socket,
	       GSocketAddress  *address,
	       gboolean         reuse_address,
	       GError         **error)
{
  union {
    struct sockaddr_storage storage;
    struct sockaddr sa;
  } addr;
  gboolean so_reuseaddr;
#ifdef SO_REUSEPORT
  gboolean so_reuseport;
#endif

  g_return_val_if_fail (G_IS_SOCKET (socket) && G_IS_SOCKET_ADDRESS (address), FALSE);

  if (!check_socket (socket, error))
    return FALSE;

  if (!g_socket_address_to_native (address, &addr.storage, sizeof addr, error))
    return FALSE;

  /* On Windows, SO_REUSEADDR has the semantics we want for UDP
   * sockets, but has nasty side effects we don't want for TCP
   * sockets.
   *
   * On other platforms, we set SO_REUSEPORT, if it exists, for
   * UDP sockets, and SO_REUSEADDR for all sockets, hoping that
   * if SO_REUSEPORT doesn't exist, then SO_REUSEADDR will have
   * the desired semantics on UDP (as it does on Linux, although
   * Linux has SO_REUSEPORT too as of 3.9).
   */

#ifdef G_OS_WIN32
  so_reuseaddr = reuse_address && (socket->priv->type == G_SOCKET_TYPE_DATAGRAM);
#else
  so_reuseaddr = !!reuse_address;
#endif

#ifdef SO_REUSEPORT
  so_reuseport = reuse_address && (socket->priv->type == G_SOCKET_TYPE_DATAGRAM);
#endif

  /* Ignore errors here, the only likely error is "not supported", and
   * this is a "best effort" thing mainly.
   */
  g_socket_set_option (socket, SOL_SOCKET, SO_REUSEADDR, so_reuseaddr, NULL);
#ifdef SO_REUSEPORT
  g_socket_set_option (socket, SOL_SOCKET, SO_REUSEPORT, so_reuseport, NULL);
#endif

  if (bind (socket->priv->fd, &addr.sa,
	    g_socket_address_get_native_size (address)) < 0)
    {
      int errsv = get_socket_errno ();
      g_set_error (error,
		   G_IO_ERROR, socket_io_error_from_errno (errsv),
		   _("Error binding to address: %s"), socket_strerror (errsv));
      return FALSE;
    }

  return TRUE;
}

static gboolean
g_socket_multicast_group_operation (GSocket       *socket,
				    GInetAddress  *group,
                                    gboolean       source_specific,
                                    const gchar   *iface,
				    gboolean       join_group,
				    GError       **error)
{
  const guint8 *native_addr;
  gint optname, result;

  g_return_val_if_fail (G_IS_SOCKET (socket), FALSE);
  g_return_val_if_fail (socket->priv->type == G_SOCKET_TYPE_DATAGRAM, FALSE);
  g_return_val_if_fail (G_IS_INET_ADDRESS (group), FALSE);

  if (!check_socket (socket, error))
    return FALSE;

  native_addr = g_inet_address_to_bytes (group);
  if (g_inet_address_get_family (group) == G_SOCKET_FAMILY_IPV4)
    {
#ifdef HAVE_IP_MREQN
      struct ip_mreqn mc_req;
#else
      struct ip_mreq mc_req;
#endif

      memset (&mc_req, 0, sizeof (mc_req));
      memcpy (&mc_req.imr_multiaddr, native_addr, sizeof (struct in_addr));

#ifdef HAVE_IP_MREQN
      if (iface)
        mc_req.imr_ifindex = if_nametoindex (iface);
      else
        mc_req.imr_ifindex = 0;  /* Pick any.  */
#elif defined(G_OS_WIN32)
      if (iface)
        mc_req.imr_interface.s_addr = g_htonl (if_nametoindex (iface));
      else
        mc_req.imr_interface.s_addr = g_htonl (INADDR_ANY);
#else
      mc_req.imr_interface.s_addr = g_htonl (INADDR_ANY);
#endif

      if (source_specific)
	{
#ifdef IP_ADD_SOURCE_MEMBERSHIP
	  optname = join_group ? IP_ADD_SOURCE_MEMBERSHIP : IP_DROP_SOURCE_MEMBERSHIP;
#else
	  g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
		       join_group ?
		       _("Error joining multicast group: %s") :
		       _("Error leaving multicast group: %s"),
		       _("No support for source-specific multicast"));
	  return FALSE;
#endif
	}
      else
        optname = join_group ? IP_ADD_MEMBERSHIP : IP_DROP_MEMBERSHIP;
      result = setsockopt (socket->priv->fd, IPPROTO_IP, optname,
			   &mc_req, sizeof (mc_req));
    }
  else if (g_inet_address_get_family (group) == G_SOCKET_FAMILY_IPV6)
    {
      struct ipv6_mreq mc_req_ipv6;

      memset (&mc_req_ipv6, 0, sizeof (mc_req_ipv6));
      memcpy (&mc_req_ipv6.ipv6mr_multiaddr, native_addr, sizeof (struct in6_addr));
#ifdef HAVE_IF_NAMETOINDEX
      if (iface)
        mc_req_ipv6.ipv6mr_interface = if_nametoindex (iface);
      else
#endif
        mc_req_ipv6.ipv6mr_interface = 0;

      optname = join_group ? IPV6_JOIN_GROUP : IPV6_LEAVE_GROUP;
      result = setsockopt (socket->priv->fd, IPPROTO_IPV6, optname,
			   &mc_req_ipv6, sizeof (mc_req_ipv6));
    }
  else
    g_return_val_if_reached (FALSE);

  if (result < 0)
    {
      int errsv = get_socket_errno ();

      g_set_error (error, G_IO_ERROR, socket_io_error_from_errno (errsv),
		   join_group ?
		   _("Error joining multicast group: %s") :
		   _("Error leaving multicast group: %s"),
		   socket_strerror (errsv));
      return FALSE;
    }

  return TRUE;
}

/**
 * g_socket_join_multicast_group:
 * @socket: a #GSocket.
 * @group: a #GInetAddress specifying the group address to join.
 * @iface: (nullable): Name of the interface to use, or %NULL
 * @source_specific: %TRUE if source-specific multicast should be used
 * @error: #GError for error reporting, or %NULL to ignore.
 *
 * Registers @socket to receive multicast messages sent to @group.
 * @socket must be a %G_SOCKET_TYPE_DATAGRAM socket, and must have
 * been bound to an appropriate interface and port with
 * g_socket_bind().
 *
 * If @iface is %NULL, the system will automatically pick an interface
 * to bind to based on @group.
 *
 * If @source_specific is %TRUE, source-specific multicast as defined
 * in RFC 4604 is used. Note that on older platforms this may fail
 * with a %G_IO_ERROR_NOT_SUPPORTED error.
 *
 * To bind to a given source-specific multicast address, use
 * g_socket_join_multicast_group_ssm() instead.
 *
 * Returns: %TRUE on success, %FALSE on error.
 *
 * Since: 2.32
 */
gboolean
g_socket_join_multicast_group (GSocket       *socket,
			       GInetAddress  *group,
                               gboolean       source_specific,
                               const gchar   *iface,
			       GError       **error)
{
  return g_socket_multicast_group_operation (socket, group, source_specific, iface, TRUE, error);
}

/**
 * g_socket_leave_multicast_group:
 * @socket: a #GSocket.
 * @group: a #GInetAddress specifying the group address to leave.
 * @iface: (nullable): Interface used
 * @source_specific: %TRUE if source-specific multicast was used
 * @error: #GError for error reporting, or %NULL to ignore.
 *
 * Removes @socket from the multicast group defined by @group, @iface,
 * and @source_specific (which must all have the same values they had
 * when you joined the group).
 *
 * @socket remains bound to its address and port, and can still receive
 * unicast messages after calling this.
 *
 * To unbind to a given source-specific multicast address, use
 * g_socket_leave_multicast_group_ssm() instead.
 *
 * Returns: %TRUE on success, %FALSE on error.
 *
 * Since: 2.32
 */
gboolean
g_socket_leave_multicast_group (GSocket       *socket,
				GInetAddress  *group,
                                gboolean       source_specific,
                                const gchar   *iface,
				GError       **error)
{
  return g_socket_multicast_group_operation (socket, group, source_specific, iface, FALSE, error);
}

static gboolean
g_socket_multicast_group_operation_ssm (GSocket       *socket,
                                        GInetAddress  *group,
                                        GInetAddress  *source_specific,
                                        const gchar   *iface,
                                        gboolean       join_group,
                                        GError       **error)
{
  gint result;

  g_return_val_if_fail (G_IS_SOCKET (socket), FALSE);
  g_return_val_if_fail (socket->priv->type == G_SOCKET_TYPE_DATAGRAM, FALSE);
  g_return_val_if_fail (G_IS_INET_ADDRESS (group), FALSE);
  g_return_val_if_fail (iface == NULL || *iface != '\0', FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  if (!source_specific)
    {
      return g_socket_multicast_group_operation (socket, group, FALSE, iface,
                                                 join_group, error);
    }

  if (!check_socket (socket, error))
    return FALSE;

  switch (g_inet_address_get_family (group))
    {
    case G_SOCKET_FAMILY_INVALID:
    case G_SOCKET_FAMILY_UNIX:
      {
        g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
            join_group ?
            _("Error joining multicast group: %s") :
            _("Error leaving multicast group: %s"),
            _("Unsupported socket family"));
        return FALSE;
      }
      break;

    case G_SOCKET_FAMILY_IPV4:
      {
#ifdef IP_ADD_SOURCE_MEMBERSHIP

#ifdef BROKEN_IP_MREQ_SOURCE_STRUCT
#define S_ADDR_FIELD(src) src.imr_interface
#else
#define S_ADDR_FIELD(src) src.imr_interface.s_addr
#endif

        gint optname;
        struct ip_mreq_source mc_req_src;

        if (g_inet_address_get_family (source_specific) !=
            G_SOCKET_FAMILY_IPV4)
          {
            g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                join_group ?
                _("Error joining multicast group: %s") :
                _("Error leaving multicast group: %s"),
                _("source-specific not an IPv4 address"));
            return FALSE;
          }

        memset (&mc_req_src, 0, sizeof (mc_req_src));

        /* By default use the default IPv4 multicast interface. */
        S_ADDR_FIELD(mc_req_src) = g_htonl (INADDR_ANY);

        if (iface)
          {
#if defined(G_OS_WIN32) && defined (HAVE_IF_NAMETOINDEX)
            guint iface_index = if_nametoindex (iface);
            if (iface_index == 0)
              {
                int errsv = errno;

                g_set_error (error, G_IO_ERROR,  g_io_error_from_errno (errsv),
                             _("Interface not found: %s"), g_strerror (errsv));
                return FALSE;
              }
            /* (0.0.0.iface_index) only works on Windows. */
            S_ADDR_FIELD(mc_req_src) = g_htonl (iface_index);
#elif defined (HAVE_SIOCGIFADDR)
            int ret;
            struct ifreq ifr;
            struct sockaddr_in *iface_addr;
            size_t if_name_len = strlen (iface);

            memset (&ifr, 0, sizeof (ifr));

            if (if_name_len >= sizeof (ifr.ifr_name))
              {
                g_set_error (error, G_IO_ERROR,  G_IO_ERROR_FILENAME_TOO_LONG,
                             _("Interface name too long"));
                return FALSE;
              }

            memcpy (ifr.ifr_name, iface, if_name_len);

            /* Get the IPv4 address of the given network interface name. */
            ret = ioctl (socket->priv->fd, SIOCGIFADDR, &ifr);
            if (ret < 0)
              {
                int errsv = errno;

                g_set_error (error, G_IO_ERROR,  g_io_error_from_errno (errsv),
                             _("Interface not found: %s"), g_strerror (errsv));
                return FALSE;
              }

            iface_addr = (struct sockaddr_in *) &ifr.ifr_addr;
            S_ADDR_FIELD(mc_req_src) = iface_addr->sin_addr.s_addr;
#endif  /* defined(G_OS_WIN32) && defined (HAVE_IF_NAMETOINDEX) */
          }
        memcpy (&mc_req_src.imr_multiaddr, g_inet_address_to_bytes (group),
                g_inet_address_get_native_size (group));
        memcpy (&mc_req_src.imr_sourceaddr,
                g_inet_address_to_bytes (source_specific),
                g_inet_address_get_native_size (source_specific));

        optname =
            join_group ? IP_ADD_SOURCE_MEMBERSHIP : IP_DROP_SOURCE_MEMBERSHIP;
        result = setsockopt (socket->priv->fd, IPPROTO_IP, optname,
                             &mc_req_src, sizeof (mc_req_src));

#undef S_ADDR_FIELD

#else
        g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
            join_group ?
            _("Error joining multicast group: %s") :
            _("Error leaving multicast group: %s"),
            _("No support for IPv4 source-specific multicast"));
        return FALSE;
#endif  /* IP_ADD_SOURCE_MEMBERSHIP */
      }
      break;

    case G_SOCKET_FAMILY_IPV6:
      {
#ifdef MCAST_JOIN_SOURCE_GROUP
        gboolean res;
        gint optname;
        struct group_source_req mc_req_src;
        GSocketAddress *saddr_group, *saddr_source_specific;
        guint iface_index = 0;

#if defined (HAVE_IF_NAMETOINDEX)
        if (iface)
          {
            iface_index = if_nametoindex (iface);
            if (iface_index == 0)
              {
                int errsv = errno;

                g_set_error (error, G_IO_ERROR,  g_io_error_from_errno (errsv),
                             _("Interface not found: %s"), g_strerror (errsv));
                return FALSE;
              }
          }
#endif  /* defined (HAVE_IF_NAMETOINDEX) */
        mc_req_src.gsr_interface = iface_index;

        saddr_group = g_inet_socket_address_new (group, 0);
        res = g_socket_address_to_native (saddr_group, &mc_req_src.gsr_group,
                                          sizeof (mc_req_src.gsr_group),
                                          error);
        g_object_unref (saddr_group);
        if (!res)
          return FALSE;

        saddr_source_specific = g_inet_socket_address_new (source_specific, 0);
        res = g_socket_address_to_native (saddr_source_specific,
                                          &mc_req_src.gsr_source,
                                          sizeof (mc_req_src.gsr_source),
                                          error);
        g_object_unref (saddr_source_specific);

        if (!res)
          return FALSE;

        optname =
            join_group ? MCAST_JOIN_SOURCE_GROUP : MCAST_LEAVE_SOURCE_GROUP;
        result = setsockopt (socket->priv->fd, IPPROTO_IPV6, optname,
                             &mc_req_src, sizeof (mc_req_src));
#else
        g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
            join_group ?
            _("Error joining multicast group: %s") :
            _("Error leaving multicast group: %s"),
            _("No support for IPv6 source-specific multicast"));
        return FALSE;
#endif  /* MCAST_JOIN_SOURCE_GROUP */
      }
      break;

    default:
      g_return_val_if_reached (FALSE);
    }

  if (result < 0)
    {
      int errsv = get_socket_errno ();

      g_set_error (error, G_IO_ERROR, socket_io_error_from_errno (errsv),
          join_group ?
          _("Error joining multicast group: %s") :
          _("Error leaving multicast group: %s"),
           socket_strerror (errsv));
      return FALSE;
    }

  return TRUE;
}

/**
 * g_socket_join_multicast_group_ssm:
 * @socket: a #GSocket.
 * @group: a #GInetAddress specifying the group address to join.
 * @source_specific: (nullable): a #GInetAddress specifying the
 * source-specific multicast address or %NULL to ignore.
 * @iface: (nullable): Name of the interface to use, or %NULL
 * @error: #GError for error reporting, or %NULL to ignore.
 *
 * Registers @socket to receive multicast messages sent to @group.
 * @socket must be a %G_SOCKET_TYPE_DATAGRAM socket, and must have
 * been bound to an appropriate interface and port with
 * g_socket_bind().
 *
 * If @iface is %NULL, the system will automatically pick an interface
 * to bind to based on @group.
 *
 * If @source_specific is not %NULL, use source-specific multicast as
 * defined in RFC 4604. Note that on older platforms this may fail
 * with a %G_IO_ERROR_NOT_SUPPORTED error.
 *
 * Note that this function can be called multiple times for the same
 * @group with different @source_specific in order to receive multicast
 * packets from more than one source.
 *
 * Returns: %TRUE on success, %FALSE on error.
 *
 * Since: 2.56
 */
gboolean
g_socket_join_multicast_group_ssm (GSocket       *socket,
                                   GInetAddress  *group,
                                   GInetAddress  *source_specific,
                                   const gchar   *iface,
                                   GError       **error)
{
  return g_socket_multicast_group_operation_ssm (socket, group,
      source_specific, iface, TRUE, error);
}

/**
 * g_socket_leave_multicast_group_ssm:
 * @socket: a #GSocket.
 * @group: a #GInetAddress specifying the group address to leave.
 * @source_specific: (nullable): a #GInetAddress specifying the
 * source-specific multicast address or %NULL to ignore.
 * @iface: (nullable): Name of the interface to use, or %NULL
 * @error: #GError for error reporting, or %NULL to ignore.
 *
 * Removes @socket from the multicast group defined by @group, @iface,
 * and @source_specific (which must all have the same values they had
 * when you joined the group).
 *
 * @socket remains bound to its address and port, and can still receive
 * unicast messages after calling this.
 *
 * Returns: %TRUE on success, %FALSE on error.
 *
 * Since: 2.56
 */
gboolean
g_socket_leave_multicast_group_ssm (GSocket       *socket,
                                    GInetAddress  *group,
                                    GInetAddress  *source_specific,
                                    const gchar   *iface,
                                    GError       **error)
{
  return g_socket_multicast_group_operation_ssm (socket, group,
      source_specific, iface, FALSE, error);
}

/**
 * g_socket_speaks_ipv4:
 * @socket: a #GSocket
 *
 * Checks if a socket is capable of speaking IPv4.
 *
 * IPv4 sockets are capable of speaking IPv4.  On some operating systems
 * and under some combinations of circumstances IPv6 sockets are also
 * capable of speaking IPv4.  See RFC 3493 section 3.7 for more
 * information.
 *
 * No other types of sockets are currently considered as being capable
 * of speaking IPv4.
 *
 * Returns: %TRUE if this socket can be used with IPv4.
 *
 * Since: 2.22
 **/
gboolean
g_socket_speaks_ipv4 (GSocket *socket)
{
  switch (socket->priv->family)
    {
    case G_SOCKET_FAMILY_IPV4:
      return TRUE;

    case G_SOCKET_FAMILY_IPV6:
#if defined (IPPROTO_IPV6) && defined (IPV6_V6ONLY)
      {
        gint v6_only;

        if (!g_socket_get_option (socket,
				  IPPROTO_IPV6, IPV6_V6ONLY,
				  &v6_only, NULL))
          return FALSE;

        return !v6_only;
      }
#else
      return FALSE;
#endif

    default:
      return FALSE;
    }
}

/**
 * g_socket_accept:
 * @socket: a #GSocket.
 * @cancellable: (nullable): a %GCancellable or %NULL
 * @error: #GError for error reporting, or %NULL to ignore.
 *
 * Accept incoming connections on a connection-based socket. This removes
 * the first outstanding connection request from the listening socket and
 * creates a #GSocket object for it.
 *
 * The @socket must be bound to a local address with g_socket_bind() and
 * must be listening for incoming connections (g_socket_listen()).
 *
 * If there are no outstanding connections then the operation will block
 * or return %G_IO_ERROR_WOULD_BLOCK if non-blocking I/O is enabled.
 * To be notified of an incoming connection, wait for the %G_IO_IN condition.
 *
 * Returns: (transfer full): a new #GSocket, or %NULL on error.
 *     Free the returned object with g_object_unref().
 *
 * Since: 2.22
 */
GSocket *
g_socket_accept (GSocket       *socket,
		 GCancellable  *cancellable,
		 GError       **error)
{
  GSocket *new_socket;
  gint ret;

  g_return_val_if_fail (G_IS_SOCKET (socket), NULL);

  if (!check_socket (socket, error))
    return NULL;

  if (!check_timeout (socket, error))
    return NULL;

  while (TRUE)
    {
      win32_unset_event_mask (socket, FD_ACCEPT);

      if ((ret = accept (socket->priv->fd, NULL, 0)) < 0)
	{
	  int errsv = get_socket_errno ();

	  if (errsv == EINTR)
	    continue;

#ifdef WSAEWOULDBLOCK
          if (errsv == WSAEWOULDBLOCK)
#else
          if (errsv == EWOULDBLOCK ||
              errsv == EAGAIN)
#endif
            {
              if (socket->priv->blocking)
                {
                  if (!g_socket_condition_wait (socket,
                                                G_IO_IN, cancellable, error))
                    return NULL;

                  continue;
                }
            }

	  socket_set_error_lazy (error, errsv, _("Error accepting connection: %s"));
	  return NULL;
	}
      break;
    }

#ifdef G_OS_WIN32
  {
    /* The socket inherits the accepting sockets event mask and even object,
       we need to remove that */
    WSAEventSelect (ret, NULL, 0);
  }
#else
  {
    int flags;

    /* We always want to set close-on-exec to protect users. If you
       need to so some weird inheritance to exec you can re-enable this
       using lower level hacks with g_socket_get_fd(). */
    flags = fcntl (ret, F_GETFD, 0);
    if (flags != -1 &&
	(flags & FD_CLOEXEC) == 0)
      {
	flags |= FD_CLOEXEC;
	fcntl (ret, F_SETFD, flags);
      }
  }
#endif

  new_socket = g_socket_new_from_fd (ret, error);
  if (new_socket == NULL)
    {
#ifdef G_OS_WIN32
      closesocket (ret);
#else
      close (ret);
#endif
    }
  else
    new_socket->priv->protocol = socket->priv->protocol;

  return new_socket;
}

/**
 * g_socket_connect:
 * @socket: a #GSocket.
 * @address: a #GSocketAddress specifying the remote address.
 * @cancellable: (nullable): a %GCancellable or %NULL
 * @error: #GError for error reporting, or %NULL to ignore.
 *
 * Connect the socket to the specified remote address.
 *
 * For connection oriented socket this generally means we attempt to make
 * a connection to the @address. For a connection-less socket it sets
 * the default address for g_socket_send() and discards all incoming datagrams
 * from other sources.
 *
 * Generally connection oriented sockets can only connect once, but
 * connection-less sockets can connect multiple times to change the
 * default address.
 *
 * If the connect call needs to do network I/O it will block, unless
 * non-blocking I/O is enabled. Then %G_IO_ERROR_PENDING is returned
 * and the user can be notified of the connection finishing by waiting
 * for the G_IO_OUT condition. The result of the connection must then be
 * checked with g_socket_check_connect_result().
 *
 * Returns: %TRUE if connected, %FALSE on error.
 *
 * Since: 2.22
 */
gboolean
g_socket_connect (GSocket         *socket,
		  GSocketAddress  *address,
		  GCancellable    *cancellable,
		  GError         **error)
{
  union {
    struct sockaddr_storage storage;
    struct sockaddr sa;
  } buffer;

  g_return_val_if_fail (G_IS_SOCKET (socket) && G_IS_SOCKET_ADDRESS (address), FALSE);

  if (!check_socket (socket, error))
    return FALSE;

  if (!g_socket_address_to_native (address, &buffer.storage, sizeof buffer, error))
    return FALSE;

  if (socket->priv->remote_address)
    g_object_unref (socket->priv->remote_address);
  socket->priv->remote_address = g_object_ref (address);

  while (1)
    {
      win32_unset_event_mask (socket, FD_CONNECT);

      if (connect (socket->priv->fd, &buffer.sa,
		   g_socket_address_get_native_size (address)) < 0)
	{
	  int errsv = get_socket_errno ();

	  if (errsv == EINTR)
	    continue;

#ifndef G_OS_WIN32
	  if (errsv == EINPROGRESS)
#else
	  if (errsv == WSAEWOULDBLOCK)
#endif
	    {
	      if (socket->priv->blocking)
		{
		  if (g_socket_condition_wait (socket, G_IO_OUT, cancellable, error))
		    {
		      if (g_socket_check_connect_result (socket, error))
			break;
		    }
		}
	      else
                {
                  g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_PENDING,
                                       _("Connection in progress"));
                  socket->priv->connect_pending = TRUE;
                }
	    }
	  else
	    g_set_error_literal (error, G_IO_ERROR,
				 socket_io_error_from_errno (errsv),
				 socket_strerror (errsv));

	  return FALSE;
	}
      break;
    }

  socket->priv->connected_read = TRUE;
  socket->priv->connected_write = TRUE;

  return TRUE;
}

/**
 * g_socket_check_connect_result:
 * @socket: a #GSocket
 * @error: #GError for error reporting, or %NULL to ignore.
 *
 * Checks and resets the pending connect error for the socket.
 * This is used to check for errors when g_socket_connect() is
 * used in non-blocking mode.
 *
 * Returns: %TRUE if no error, %FALSE otherwise, setting @error to the error
 *
 * Since: 2.22
 */
gboolean
g_socket_check_connect_result (GSocket  *socket,
			       GError  **error)
{
  int value;

  g_return_val_if_fail (G_IS_SOCKET (socket), FALSE);

  if (!check_socket (socket, error))
    return FALSE;

  if (!check_timeout (socket, error))
    return FALSE;

  if (!g_socket_get_option (socket, SOL_SOCKET, SO_ERROR, &value, error))
    {
      g_prefix_error (error, _("Unable to get pending error: "));
      return FALSE;
    }

  if (value != 0)
    {
      g_set_error_literal (error, G_IO_ERROR, socket_io_error_from_errno (value),
                           socket_strerror (value));
      if (socket->priv->remote_address)
        {
          g_object_unref (socket->priv->remote_address);
          socket->priv->remote_address = NULL;
        }
      return FALSE;
    }

  socket->priv->connected_read = TRUE;
  socket->priv->connected_write = TRUE;

  return TRUE;
}

/**
 * g_socket_get_available_bytes:
 * @socket: a #GSocket
 *
 * Get the amount of data pending in the OS input buffer, without blocking.
 *
 * If @socket is a UDP or SCTP socket, this will return the size of
 * just the next packet, even if additional packets are buffered after
 * that one.
 *
 * Note that on Windows, this function is rather inefficient in the
 * UDP case, and so if you know any plausible upper bound on the size
 * of the incoming packet, it is better to just do a
 * g_socket_receive() with a buffer of that size, rather than calling
 * g_socket_get_available_bytes() first and then doing a receive of
 * exactly the right size.
 *
 * Returns: the number of bytes that can be read from the socket
 * without blocking or truncating, or -1 on error.
 *
 * Since: 2.32
 */
gssize
g_socket_get_available_bytes (GSocket *socket)
{
#ifdef G_OS_WIN32
  const gint bufsize = 64 * 1024;
  static guchar *buf = NULL;
  u_long avail;
#else
  gint avail;
#endif

  g_return_val_if_fail (G_IS_SOCKET (socket), -1);

#if defined (SO_NREAD)
  if (!g_socket_get_option (socket, SOL_SOCKET, SO_NREAD, &avail, NULL))
      return -1;
#elif !defined (G_OS_WIN32)
  if (ioctl (socket->priv->fd, FIONREAD, &avail) < 0)
    avail = -1;
#else
  if (socket->priv->type == G_SOCKET_TYPE_DATAGRAM)
    {
      if (G_UNLIKELY (g_once_init_enter (&buf)))
        g_once_init_leave (&buf, g_malloc (bufsize));

      avail = recv (socket->priv->fd, buf, bufsize, MSG_PEEK);
      if (avail == -1 && get_socket_errno () == WSAEWOULDBLOCK)
        avail = 0;
    }
  else
    {
      if (ioctlsocket (socket->priv->fd, FIONREAD, &avail) < 0)
        avail = -1;
    }
#endif

  return avail;
}

/* Block on a timed wait for @condition until (@start_time + @timeout).
 * Return %G_IO_ERROR_TIMED_OUT if the timeout is reached; otherwise %TRUE.
 */
static gboolean
block_on_timeout (GSocket       *socket,
                  GIOCondition   condition,
                  gint64         timeout,
                  gint64         start_time,
                  GCancellable  *cancellable,
                  GError       **error)
{
  gint64 wait_timeout = -1;

  g_return_val_if_fail (timeout != 0, TRUE);

  /* check if we've timed out or how much time to wait at most */
  if (timeout >= 0)
    {
      gint64 elapsed = g_get_monotonic_time () - start_time;

      if (elapsed >= timeout)
        {
          g_set_error_literal (error,
                               G_IO_ERROR, G_IO_ERROR_TIMED_OUT,
                               _("Socket I/O timed out"));
          return FALSE;
        }

      wait_timeout = timeout - elapsed;
    }

  return g_socket_condition_timed_wait (socket, condition, wait_timeout,
                                        cancellable, error);
}

static gssize
g_socket_receive_with_timeout (GSocket       *socket,
                               guint8        *buffer,
                               gsize          size,
                               gint64         timeout,
                               GCancellable  *cancellable,
                               GError       **error)
{
  gssize ret;
  gint64 start_time;

  g_return_val_if_fail (G_IS_SOCKET (socket) && buffer != NULL, -1);

  start_time = g_get_monotonic_time ();

  if (!check_socket (socket, error))
    return -1;

  if (!check_timeout (socket, error))
    return -1;

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return -1;

  while (1)
    {
      win32_unset_event_mask (socket, FD_READ);

      if ((ret = recv (socket->priv->fd, buffer, size, 0)) < 0)
	{
	  int errsv = get_socket_errno ();

	  if (errsv == EINTR)
	    continue;

#ifdef WSAEWOULDBLOCK
          if (errsv == WSAEWOULDBLOCK)
#else
          if (errsv == EWOULDBLOCK ||
              errsv == EAGAIN)
#endif
            {
              if (timeout != 0)
                {
                  if (!block_on_timeout (socket, G_IO_IN, timeout, start_time,
                                         cancellable, error))
                    return -1;

                  continue;
                }
            }

	  socket_set_error_lazy (error, errsv, _("Error receiving data: %s"));
	  return -1;
	}

      break;
    }

  return ret;
}

/**
 * g_socket_receive:
 * @socket: a #GSocket
 * @buffer: (array length=size) (element-type guint8): a buffer to
 *     read data into (which should be at least @size bytes long).
 * @size: the number of bytes you want to read from the socket
 * @cancellable: (nullable): a %GCancellable or %NULL
 * @error: #GError for error reporting, or %NULL to ignore.
 *
 * Receive data (up to @size bytes) from a socket. This is mainly used by
 * connection-oriented sockets; it is identical to g_socket_receive_from()
 * with @address set to %NULL.
 *
 * For %G_SOCKET_TYPE_DATAGRAM and %G_SOCKET_TYPE_SEQPACKET sockets,
 * g_socket_receive() will always read either 0 or 1 complete messages from
 * the socket. If the received message is too large to fit in @buffer, then
 * the data beyond @size bytes will be discarded, without any explicit
 * indication that this has occurred.
 *
 * For %G_SOCKET_TYPE_STREAM sockets, g_socket_receive() can return any
 * number of bytes, up to @size. If more than @size bytes have been
 * received, the additional data will be returned in future calls to
 * g_socket_receive().
 *
 * If the socket is in blocking mode the call will block until there
 * is some data to receive, the connection is closed, or there is an
 * error. If there is no data available and the socket is in
 * non-blocking mode, a %G_IO_ERROR_WOULD_BLOCK error will be
 * returned. To be notified when data is available, wait for the
 * %G_IO_IN condition.
 *
 * On error -1 is returned and @error is set accordingly.
 *
 * Returns: Number of bytes read, or 0 if the connection was closed by
 * the peer, or -1 on error
 *
 * Since: 2.22
 */
gssize
g_socket_receive (GSocket       *socket,
		  gchar         *buffer,
		  gsize          size,
		  GCancellable  *cancellable,
		  GError       **error)
{
  return g_socket_receive_with_timeout (socket, (guint8 *) buffer, size,
                                        socket->priv->blocking ? -1 : 0,
                                        cancellable, error);
}

/**
 * g_socket_receive_with_blocking:
 * @socket: a #GSocket
 * @buffer: (array length=size) (element-type guint8): a buffer to
 *     read data into (which should be at least @size bytes long).
 * @size: the number of bytes you want to read from the socket
 * @blocking: whether to do blocking or non-blocking I/O
 * @cancellable: (nullable): a %GCancellable or %NULL
 * @error: #GError for error reporting, or %NULL to ignore.
 *
 * This behaves exactly the same as g_socket_receive(), except that
 * the choice of blocking or non-blocking behavior is determined by
 * the @blocking argument rather than by @socket's properties.
 *
 * Returns: Number of bytes read, or 0 if the connection was closed by
 * the peer, or -1 on error
 *
 * Since: 2.26
 */
gssize
g_socket_receive_with_blocking (GSocket       *socket,
				gchar         *buffer,
				gsize          size,
				gboolean       blocking,
				GCancellable  *cancellable,
				GError       **error)
{
  return g_socket_receive_with_timeout (socket, (guint8 *) buffer, size,
                                        blocking ? -1 : 0, cancellable, error);
}

/**
 * g_socket_receive_from:
 * @socket: a #GSocket
 * @address: (out) (optional): a pointer to a #GSocketAddress
 *     pointer, or %NULL
 * @buffer: (array length=size) (element-type guint8): a buffer to
 *     read data into (which should be at least @size bytes long).
 * @size: the number of bytes you want to read from the socket
 * @cancellable: (nullable): a %GCancellable or %NULL
 * @error: #GError for error reporting, or %NULL to ignore.
 *
 * Receive data (up to @size bytes) from a socket.
 *
 * If @address is non-%NULL then @address will be set equal to the
 * source address of the received packet.
 * @address is owned by the caller.
 *
 * See g_socket_receive() for additional information.
 *
 * Returns: Number of bytes read, or 0 if the connection was closed by
 * the peer, or -1 on error
 *
 * Since: 2.22
 */
gssize
g_socket_receive_from (GSocket         *socket,
		       GSocketAddress **address,
		       gchar           *buffer,
		       gsize            size,
		       GCancellable    *cancellable,
		       GError         **error)
{
  GInputVector v;

  v.buffer = buffer;
  v.size = size;

  return g_socket_receive_message (socket,
				   address,
				   &v, 1,
				   NULL, 0, NULL,
				   cancellable,
				   error);
}

/* See the comment about SIGPIPE above. */
#ifdef MSG_NOSIGNAL
#define G_SOCKET_DEFAULT_SEND_FLAGS MSG_NOSIGNAL
#else
#define G_SOCKET_DEFAULT_SEND_FLAGS 0
#endif

static gssize
g_socket_send_with_timeout (GSocket       *socket,
                            const guint8  *buffer,
                            gsize          size,
                            gint64         timeout,
                            GCancellable  *cancellable,
                            GError       **error)
{
  gssize ret;
  gint64 start_time;

  g_return_val_if_fail (G_IS_SOCKET (socket) && buffer != NULL, -1);

  start_time = g_get_monotonic_time ();

  if (!check_socket (socket, error))
    return -1;

  if (!check_timeout (socket, error))
    return -1;

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return -1;

  while (1)
    {
      win32_unset_event_mask (socket, FD_WRITE);

      if ((ret = send (socket->priv->fd, buffer, size, G_SOCKET_DEFAULT_SEND_FLAGS)) < 0)
	{
	  int errsv = get_socket_errno ();

	  if (errsv == EINTR)
	    continue;

#ifdef WSAEWOULDBLOCK
          if (errsv == WSAEWOULDBLOCK)
#else
          if (errsv == EWOULDBLOCK ||
              errsv == EAGAIN)
#endif
            {
              if (timeout != 0)
                {
                  if (!block_on_timeout (socket, G_IO_OUT, timeout, start_time,
                                         cancellable, error))
                    return -1;

                  continue;
                }
            }

	  socket_set_error_lazy (error, errsv, _("Error sending data: %s"));
	  return -1;
	}
      break;
    }

  return ret;
}

/**
 * g_socket_send:
 * @socket: a #GSocket
 * @buffer: (array length=size) (element-type guint8): the buffer
 *     containing the data to send.
 * @size: the number of bytes to send
 * @cancellable: (nullable): a %GCancellable or %NULL
 * @error: #GError for error reporting, or %NULL to ignore.
 *
 * Tries to send @size bytes from @buffer on the socket. This is
 * mainly used by connection-oriented sockets; it is identical to
 * g_socket_send_to() with @address set to %NULL.
 *
 * If the socket is in blocking mode the call will block until there is
 * space for the data in the socket queue. If there is no space available
 * and the socket is in non-blocking mode a %G_IO_ERROR_WOULD_BLOCK error
 * will be returned. To be notified when space is available, wait for the
 * %G_IO_OUT condition. Note though that you may still receive
 * %G_IO_ERROR_WOULD_BLOCK from g_socket_send() even if you were previously
 * notified of a %G_IO_OUT condition. (On Windows in particular, this is
 * very common due to the way the underlying APIs work.)
 *
 * On error -1 is returned and @error is set accordingly.
 *
 * Returns: Number of bytes written (which may be less than @size), or -1
 * on error
 *
 * Since: 2.22
 */
gssize
g_socket_send (GSocket       *socket,
	       const gchar   *buffer,
	       gsize          size,
	       GCancellable  *cancellable,
	       GError       **error)
{
  return g_socket_send_with_blocking (socket, buffer, size,
				      socket->priv->blocking,
				      cancellable, error);
}

/**
 * g_socket_send_with_blocking:
 * @socket: a #GSocket
 * @buffer: (array length=size) (element-type guint8): the buffer
 *     containing the data to send.
 * @size: the number of bytes to send
 * @blocking: whether to do blocking or non-blocking I/O
 * @cancellable: (nullable): a %GCancellable or %NULL
 * @error: #GError for error reporting, or %NULL to ignore.
 *
 * This behaves exactly the same as g_socket_send(), except that
 * the choice of blocking or non-blocking behavior is determined by
 * the @blocking argument rather than by @socket's properties.
 *
 * Returns: Number of bytes written (which may be less than @size), or -1
 * on error
 *
 * Since: 2.26
 */
gssize
g_socket_send_with_blocking (GSocket       *socket,
			     const gchar   *buffer,
			     gsize          size,
			     gboolean       blocking,
			     GCancellable  *cancellable,
			     GError       **error)
{
  return g_socket_send_with_timeout (socket, (const guint8 *) buffer, size,
                                     blocking ? -1 : 0, cancellable, error);
}

/**
 * g_socket_send_to:
 * @socket: a #GSocket
 * @address: (nullable): a #GSocketAddress, or %NULL
 * @buffer: (array length=size) (element-type guint8): the buffer
 *     containing the data to send.
 * @size: the number of bytes to send
 * @cancellable: (nullable): a %GCancellable or %NULL
 * @error: #GError for error reporting, or %NULL to ignore.
 *
 * Tries to send @size bytes from @buffer to @address. If @address is
 * %NULL then the message is sent to the default receiver (set by
 * g_socket_connect()).
 *
 * See g_socket_send() for additional information.
 *
 * Returns: Number of bytes written (which may be less than @size), or -1
 * on error
 *
 * Since: 2.22
 */
gssize
g_socket_send_to (GSocket         *socket,
		  GSocketAddress  *address,
		  const gchar     *buffer,
		  gsize            size,
		  GCancellable    *cancellable,
		  GError         **error)
{
  GOutputVector v;

  v.buffer = buffer;
  v.size = size;

  return g_socket_send_message (socket,
				address,
				&v, 1,
				NULL, 0,
				0,
				cancellable,
				error);
}

/**
 * g_socket_shutdown:
 * @socket: a #GSocket
 * @shutdown_read: whether to shut down the read side
 * @shutdown_write: whether to shut down the write side
 * @error: #GError for error reporting, or %NULL to ignore.
 *
 * Shut down part or all of a full-duplex connection.
 *
 * If @shutdown_read is %TRUE then the receiving side of the connection
 * is shut down, and further reading is disallowed.
 *
 * If @shutdown_write is %TRUE then the sending side of the connection
 * is shut down, and further writing is disallowed.
 *
 * It is allowed for both @shutdown_read and @shutdown_write to be %TRUE.
 *
 * One example where it is useful to shut down only one side of a connection is
 * graceful disconnect for TCP connections where you close the sending side,
 * then wait for the other side to close the connection, thus ensuring that the
 * other side saw all sent data.
 *
 * Returns: %TRUE on success, %FALSE on error
 *
 * Since: 2.22
 */
gboolean
g_socket_shutdown (GSocket   *socket,
		   gboolean   shutdown_read,
		   gboolean   shutdown_write,
		   GError   **error)
{
  int how;

  g_return_val_if_fail (G_IS_SOCKET (socket), TRUE);

  if (!check_socket (socket, error))
    return FALSE;

  /* Do nothing? */
  if (!shutdown_read && !shutdown_write)
    return TRUE;

#ifndef G_OS_WIN32
  if (shutdown_read && shutdown_write)
    how = SHUT_RDWR;
  else if (shutdown_read)
    how = SHUT_RD;
  else
    how = SHUT_WR;
#else
  if (shutdown_read && shutdown_write)
    how = SD_BOTH;
  else if (shutdown_read)
    how = SD_RECEIVE;
  else
    how = SD_SEND;
#endif

  if (shutdown (socket->priv->fd, how) != 0)
    {
      int errsv = get_socket_errno ();
      g_set_error (error, G_IO_ERROR, socket_io_error_from_errno (errsv),
		   _("Unable to shutdown socket: %s"), socket_strerror (errsv));
      return FALSE;
    }

  if (shutdown_read)
    socket->priv->connected_read = FALSE;
  if (shutdown_write)
    socket->priv->connected_write = FALSE;

  return TRUE;
}

/**
 * g_socket_close:
 * @socket: a #GSocket
 * @error: #GError for error reporting, or %NULL to ignore.
 *
 * Closes the socket, shutting down any active connection.
 *
 * Closing a socket does not wait for all outstanding I/O operations
 * to finish, so the caller should not rely on them to be guaranteed
 * to complete even if the close returns with no error.
 *
 * Once the socket is closed, all other operations will return
 * %G_IO_ERROR_CLOSED. Closing a socket multiple times will not
 * return an error.
 *
 * Sockets will be automatically closed when the last reference
 * is dropped, but you might want to call this function to make sure
 * resources are released as early as possible.
 *
 * Beware that due to the way that TCP works, it is possible for
 * recently-sent data to be lost if either you close a socket while the
 * %G_IO_IN condition is set, or else if the remote connection tries to
 * send something to you after you close the socket but before it has
 * finished reading all of the data you sent. There is no easy generic
 * way to avoid this problem; the easiest fix is to design the network
 * protocol such that the client will never send data "out of turn".
 * Another solution is for the server to half-close the connection by
 * calling g_socket_shutdown() with only the @shutdown_write flag set,
 * and then wait for the client to notice this and close its side of the
 * connection, after which the server can safely call g_socket_close().
 * (This is what #GTcpConnection does if you call
 * g_tcp_connection_set_graceful_disconnect(). But of course, this
 * only works if the client will close its connection after the server
 * does.)
 *
 * Returns: %TRUE on success, %FALSE on error
 *
 * Since: 2.22
 */
gboolean
g_socket_close (GSocket  *socket,
		GError  **error)
{
  int res;

  g_return_val_if_fail (G_IS_SOCKET (socket), TRUE);

  if (socket->priv->closed)
    return TRUE; /* Multiple close not an error */

  if (!check_socket (socket, error))
    return FALSE;

  while (1)
    {
#ifdef G_OS_WIN32
      res = closesocket (socket->priv->fd);
#else
      res = close (socket->priv->fd);
#endif
      if (res == -1)
	{
	  int errsv = get_socket_errno ();

	  if (errsv == EINTR)
	    continue;

	  g_set_error (error, G_IO_ERROR,
		       socket_io_error_from_errno (errsv),
		       _("Error closing socket: %s"),
		       socket_strerror (errsv));
	  return FALSE;
	}
      break;
    }

  socket->priv->fd = -1;
  socket->priv->connected_read = FALSE;
  socket->priv->connected_write = FALSE;
  socket->priv->closed = TRUE;
  if (socket->priv->remote_address)
    {
      g_object_unref (socket->priv->remote_address);
      socket->priv->remote_address = NULL;
    }

  return TRUE;
}

/**
 * g_socket_is_closed:
 * @socket: a #GSocket
 *
 * Checks whether a socket is closed.
 *
 * Returns: %TRUE if socket is closed, %FALSE otherwise
 *
 * Since: 2.22
 */
gboolean
g_socket_is_closed (GSocket *socket)
{
  return socket->priv->closed;
}

#ifdef G_OS_WIN32
/* Broken source, used on errors */
static gboolean
broken_dispatch (GSource     *source,
		 GSourceFunc  callback,
		 gpointer     user_data)
{
  return TRUE;
}

static GSourceFuncs broken_funcs =
{
  NULL,
  NULL,
  broken_dispatch,
  NULL
};

static gint
network_events_for_condition (GIOCondition condition)
{
  int event_mask = 0;

  if (condition & G_IO_IN)
    event_mask |= (FD_READ | FD_ACCEPT);
  if (condition & G_IO_OUT)
    event_mask |= (FD_WRITE | FD_CONNECT);
  event_mask |= FD_CLOSE;

  return event_mask;
}

static void
ensure_event (GSocket *socket)
{
  if (socket->priv->event == WSA_INVALID_EVENT)
    socket->priv->event = WSACreateEvent();
}

static void
update_select_events (GSocket *socket)
{
  int event_mask;
  GIOCondition *ptr;
  GList *l;
  WSAEVENT event;

  ensure_event (socket);

  event_mask = 0;
  for (l = socket->priv->requested_conditions; l != NULL; l = l->next)
    {
      ptr = l->data;
      event_mask |= network_events_for_condition (*ptr);
    }

  if (event_mask != socket->priv->selected_events)
    {
      /* If no events selected, disable event so we can unset
	 nonblocking mode */

      if (event_mask == 0)
	event = NULL;
      else
	event = socket->priv->event;

      if (WSAEventSelect (socket->priv->fd, event, event_mask) == 0)
	socket->priv->selected_events = event_mask;
    }
}

static void
add_condition_watch (GSocket      *socket,
		     GIOCondition *condition)
{
  g_mutex_lock (&socket->priv->win32_source_lock);
  g_assert (g_list_find (socket->priv->requested_conditions, condition) == NULL);

  socket->priv->requested_conditions =
    g_list_prepend (socket->priv->requested_conditions, condition);

  update_select_events (socket);
  g_mutex_unlock (&socket->priv->win32_source_lock);
}

static void
remove_condition_watch (GSocket      *socket,
			GIOCondition *condition)
{
  g_mutex_lock (&socket->priv->win32_source_lock);
  g_assert (g_list_find (socket->priv->requested_conditions, condition) != NULL);

  socket->priv->requested_conditions =
    g_list_remove (socket->priv->requested_conditions, condition);

  update_select_events (socket);
  g_mutex_unlock (&socket->priv->win32_source_lock);
}

static GIOCondition
update_condition_unlocked (GSocket *socket)
{
  WSANETWORKEVENTS events;
  GIOCondition condition;

  if (WSAEnumNetworkEvents (socket->priv->fd,
			    socket->priv->event,
			    &events) == 0)
    {
      socket->priv->current_events |= events.lNetworkEvents;
      if (events.lNetworkEvents & FD_WRITE &&
	  events.iErrorCode[FD_WRITE_BIT] != 0)
	socket->priv->current_errors |= FD_WRITE;
      if (events.lNetworkEvents & FD_CONNECT &&
	  events.iErrorCode[FD_CONNECT_BIT] != 0)
	socket->priv->current_errors |= FD_CONNECT;
    }

  condition = 0;
  if (socket->priv->current_events & (FD_READ | FD_ACCEPT))
    condition |= G_IO_IN;

  if (socket->priv->current_events & FD_CLOSE)
    {
      int r, errsv, buffer;

      r = recv (socket->priv->fd, &buffer, sizeof (buffer), MSG_PEEK);
      if (r < 0)
          errsv = get_socket_errno ();

      if (r > 0 ||
          (r < 0 && errsv == WSAENOTCONN))
        condition |= G_IO_IN;
      else if (r == 0 ||
               (r < 0 && (errsv == WSAESHUTDOWN || errsv == WSAECONNRESET ||
                          errsv == WSAECONNABORTED || errsv == WSAENETRESET)))
        condition |= G_IO_HUP;
      else
        condition |= G_IO_ERR;
    }

  if (socket->priv->closed)
    condition |= G_IO_HUP;

  /* Never report both G_IO_OUT and HUP, these are
     mutually exclusive (can't write to a closed socket) */
  if ((condition & G_IO_HUP) == 0 &&
      socket->priv->current_events & FD_WRITE)
    {
      if (socket->priv->current_errors & FD_WRITE)
	condition |= G_IO_ERR;
      else
	condition |= G_IO_OUT;
    }
  else
    {
      if (socket->priv->current_events & FD_CONNECT)
	{
	  if (socket->priv->current_errors & FD_CONNECT)
	    condition |= (G_IO_HUP | G_IO_ERR);
	  else
	    condition |= G_IO_OUT;
	}
    }

  return condition;
}

static GIOCondition
update_condition (GSocket *socket)
{
  GIOCondition res;
  g_mutex_lock (&socket->priv->win32_source_lock);
  res = update_condition_unlocked (socket);
  g_mutex_unlock (&socket->priv->win32_source_lock);
  return res;
}
#endif

typedef struct {
  GSource       source;
#ifdef G_OS_WIN32
  GPollFD       pollfd;
#else
  gpointer      fd_tag;
#endif
  GSocket      *socket;
  GIOCondition  condition;
} GSocketSource;

static gboolean
socket_source_prepare (GSource *source,
                       gint    *timeout)
{
  GSocketSource *socket_source = (GSocketSource *)source;

  *timeout = -1;

#ifdef G_OS_WIN32
  if ((socket_source->pollfd.revents & G_IO_NVAL) != 0)
    return TRUE;

  if (g_socket_is_closed (socket_source->socket))
    {
      g_source_remove_poll (source, &socket_source->pollfd);
      socket_source->pollfd.revents = G_IO_NVAL;
      return TRUE;
    }

  return (update_condition (socket_source->socket) & socket_source->condition) != 0;
#else
  return g_socket_is_closed (socket_source->socket) && socket_source->fd_tag != NULL;
#endif
}

#ifdef G_OS_WIN32
static gboolean
socket_source_check_win32 (GSource *source)
{
  int timeout;

  return socket_source_prepare (source, &timeout);
}
#endif

static gboolean
socket_source_dispatch (GSource     *source,
			GSourceFunc  callback,
			gpointer     user_data)
{
  GSocketSourceFunc func = (GSocketSourceFunc)callback;
  GSocketSource *socket_source = (GSocketSource *)source;
  GSocket *socket = socket_source->socket;
  gint64 timeout;
  guint events;
  gboolean ret;

#ifdef G_OS_WIN32
  events = update_condition (socket_source->socket);
#else
  if (g_socket_is_closed (socket_source->socket))
    {
      if (socket_source->fd_tag)
        g_source_remove_unix_fd (source, socket_source->fd_tag);
      socket_source->fd_tag = NULL;
      events = G_IO_NVAL;
    }
  else
    {
      events = g_source_query_unix_fd (source, socket_source->fd_tag);
    }
#endif

  timeout = g_source_get_ready_time (source);
  if (timeout >= 0 && timeout < g_source_get_time (source) &&
      !g_socket_is_closed (socket_source->socket))
    {
      socket->priv->timed_out = TRUE;
      events |= (G_IO_IN | G_IO_OUT);
    }

  ret = (*func) (socket, events & socket_source->condition, user_data);

  if (socket->priv->timeout && !g_socket_is_closed (socket_source->socket))
    g_source_set_ready_time (source, g_get_monotonic_time () + socket->priv->timeout * 1000000);
  else
    g_source_set_ready_time (source, -1);

  return ret;
}

static void
socket_source_finalize (GSource *source)
{
  GSocketSource *socket_source = (GSocketSource *)source;
  GSocket *socket;

  socket = socket_source->socket;

#ifdef G_OS_WIN32
  remove_condition_watch (socket, &socket_source->condition);
#endif

  g_object_unref (socket);
}

static gboolean
socket_source_closure_callback (GSocket      *socket,
				GIOCondition  condition,
				gpointer      data)
{
  GClosure *closure = data;

  GValue params[2] = { G_VALUE_INIT, G_VALUE_INIT };
  GValue result_value = G_VALUE_INIT;
  gboolean result;

  g_value_init (&result_value, G_TYPE_BOOLEAN);

  g_value_init (&params[0], G_TYPE_SOCKET);
  g_value_set_object (&params[0], socket);
  g_value_init (&params[1], G_TYPE_IO_CONDITION);
  g_value_set_flags (&params[1], condition);

  g_closure_invoke (closure, &result_value, 2, params, NULL);

  result = g_value_get_boolean (&result_value);
  g_value_unset (&result_value);
  g_value_unset (&params[0]);
  g_value_unset (&params[1]);

  return result;
}

static GSourceFuncs socket_source_funcs =
{
  socket_source_prepare,
#ifdef G_OS_WIN32
  socket_source_check_win32,
#else
  NULL,
#endif
  socket_source_dispatch,
  socket_source_finalize,
  (GSourceFunc)socket_source_closure_callback,
};

static GSource *
socket_source_new (GSocket      *socket,
		   GIOCondition  condition,
		   GCancellable *cancellable)
{
  GSource *source;
  GSocketSource *socket_source;

#ifdef G_OS_WIN32
  ensure_event (socket);

  if (socket->priv->event == WSA_INVALID_EVENT)
    {
      g_warning ("Failed to create WSAEvent");
      return g_source_new (&broken_funcs, sizeof (GSource));
    }
#endif

  condition |= G_IO_HUP | G_IO_ERR | G_IO_NVAL;

  source = g_source_new (&socket_source_funcs, sizeof (GSocketSource));
  g_source_set_name (source, "GSocket");
  socket_source = (GSocketSource *)source;

  socket_source->socket = g_object_ref (socket);
  socket_source->condition = condition;

  if (cancellable)
    {
      GSource *cancellable_source;

      cancellable_source = g_cancellable_source_new (cancellable);
      g_source_add_child_source (source, cancellable_source);
      g_source_set_dummy_callback (cancellable_source);
      g_source_unref (cancellable_source);
    }

#ifdef G_OS_WIN32
  add_condition_watch (socket, &socket_source->condition);
  socket_source->pollfd.fd = (gintptr) socket->priv->event;
  socket_source->pollfd.events = condition;
  socket_source->pollfd.revents = 0;
  g_source_add_poll (source, &socket_source->pollfd);
#else
  socket_source->fd_tag = g_source_add_unix_fd (source, socket->priv->fd, condition);
#endif

  if (socket->priv->timeout)
    g_source_set_ready_time (source, g_get_monotonic_time () + socket->priv->timeout * 1000000);
  else
    g_source_set_ready_time (source, -1);

  return source;
}

/**
 * g_socket_create_source: (skip)
 * @socket: a #GSocket
 * @condition: a #GIOCondition mask to monitor
 * @cancellable: (nullable): a %GCancellable or %NULL
 *
 * Creates a #GSource that can be attached to a %GMainContext to monitor
 * for the availability of the specified @condition on the socket. The #GSource
 * keeps a reference to the @socket.
 *
 * The callback on the source is of the #GSocketSourceFunc type.
 *
 * It is meaningless to specify %G_IO_ERR or %G_IO_HUP in @condition;
 * these conditions will always be reported output if they are true.
 *
 * @cancellable if not %NULL can be used to cancel the source, which will
 * cause the source to trigger, reporting the current condition (which
 * is likely 0 unless cancellation happened at the same time as a
 * condition change). You can check for this in the callback using
 * g_cancellable_is_cancelled().
 *
 * If @socket has a timeout set, and it is reached before @condition
 * occurs, the source will then trigger anyway, reporting %G_IO_IN or
 * %G_IO_OUT depending on @condition. However, @socket will have been
 * marked as having had a timeout, and so the next #GSocket I/O method
 * you call will then fail with a %G_IO_ERROR_TIMED_OUT.
 *
 * Returns: (transfer full): a newly allocated %GSource, free with g_source_unref().
 *
 * Since: 2.22
 */
GSource *
g_socket_create_source (GSocket      *socket,
			GIOCondition  condition,
			GCancellable *cancellable)
{
  g_return_val_if_fail (G_IS_SOCKET (socket) && (cancellable == NULL || G_IS_CANCELLABLE (cancellable)), NULL);

  return socket_source_new (socket, condition, cancellable);
}

/**
 * g_socket_condition_check:
 * @socket: a #GSocket
 * @condition: a #GIOCondition mask to check
 *
 * Checks on the readiness of @socket to perform operations.
 * The operations specified in @condition are checked for and masked
 * against the currently-satisfied conditions on @socket. The result
 * is returned.
 *
 * Note that on Windows, it is possible for an operation to return
 * %G_IO_ERROR_WOULD_BLOCK even immediately after
 * g_socket_condition_check() has claimed that the socket is ready for
 * writing. Rather than calling g_socket_condition_check() and then
 * writing to the socket if it succeeds, it is generally better to
 * simply try writing to the socket right away, and try again later if
 * the initial attempt returns %G_IO_ERROR_WOULD_BLOCK.
 *
 * It is meaningless to specify %G_IO_ERR or %G_IO_HUP in condition;
 * these conditions will always be set in the output if they are true.
 *
 * This call never blocks.
 *
 * Returns: the @GIOCondition mask of the current state
 *
 * Since: 2.22
 */
GIOCondition
g_socket_condition_check (GSocket      *socket,
			  GIOCondition  condition)
{
  g_return_val_if_fail (G_IS_SOCKET (socket), 0);

  if (!check_socket (socket, NULL))
    return 0;

#ifdef G_OS_WIN32
  {
    GIOCondition current_condition;

    condition |= G_IO_ERR | G_IO_HUP;

    add_condition_watch (socket, &condition);
    current_condition = update_condition (socket);
    remove_condition_watch (socket, &condition);
    return condition & current_condition;
  }
#else
  {
    GPollFD poll_fd;
    gint result;
    poll_fd.fd = socket->priv->fd;
    poll_fd.events = condition;
    poll_fd.revents = 0;

    do
      result = g_poll (&poll_fd, 1, 0);
    while (result == -1 && get_socket_errno () == EINTR);

    return poll_fd.revents;
  }
#endif
}

/**
 * g_socket_condition_wait:
 * @socket: a #GSocket
 * @condition: a #GIOCondition mask to wait for
 * @cancellable: (nullable): a #GCancellable, or %NULL
 * @error: a #GError pointer, or %NULL
 *
 * Waits for @condition to become true on @socket. When the condition
 * is met, %TRUE is returned.
 *
 * If @cancellable is cancelled before the condition is met, or if the
 * socket has a timeout set and it is reached before the condition is
 * met, then %FALSE is returned and @error, if non-%NULL, is set to
 * the appropriate value (%G_IO_ERROR_CANCELLED or
 * %G_IO_ERROR_TIMED_OUT).
 *
 * See also g_socket_condition_timed_wait().
 *
 * Returns: %TRUE if the condition was met, %FALSE otherwise
 *
 * Since: 2.22
 */
gboolean
g_socket_condition_wait (GSocket       *socket,
			 GIOCondition   condition,
			 GCancellable  *cancellable,
			 GError       **error)
{
  g_return_val_if_fail (G_IS_SOCKET (socket), FALSE);

  return g_socket_condition_timed_wait (socket, condition, -1,
					cancellable, error);
}

/**
 * g_socket_condition_timed_wait:
 * @socket: a #GSocket
 * @condition: a #GIOCondition mask to wait for
 * @timeout: the maximum time (in microseconds) to wait, or -1
 * @cancellable: (nullable): a #GCancellable, or %NULL
 * @error: a #GError pointer, or %NULL
 *
 * Waits for up to @timeout microseconds for @condition to become true
 * on @socket. If the condition is met, %TRUE is returned.
 *
 * If @cancellable is cancelled before the condition is met, or if
 * @timeout (or the socket's #GSocket:timeout) is reached before the
 * condition is met, then %FALSE is returned and @error, if non-%NULL,
 * is set to the appropriate value (%G_IO_ERROR_CANCELLED or
 * %G_IO_ERROR_TIMED_OUT).
 *
 * If you don't want a timeout, use g_socket_condition_wait().
 * (Alternatively, you can pass -1 for @timeout.)
 *
 * Note that although @timeout is in microseconds for consistency with
 * other GLib APIs, this function actually only has millisecond
 * resolution, and the behavior is undefined if @timeout is not an
 * exact number of milliseconds.
 *
 * Returns: %TRUE if the condition was met, %FALSE otherwise
 *
 * Since: 2.32
 */
gboolean
g_socket_condition_timed_wait (GSocket       *socket,
			       GIOCondition   condition,
			       gint64         timeout,
			       GCancellable  *cancellable,
			       GError       **error)
{
  gint64 start_time;

  g_return_val_if_fail (G_IS_SOCKET (socket), FALSE);

  if (!check_socket (socket, error))
    return FALSE;

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return FALSE;

  if (socket->priv->timeout &&
      (timeout < 0 || socket->priv->timeout < timeout / G_USEC_PER_SEC))
    timeout = (gint64) socket->priv->timeout * 1000;
  else if (timeout != -1)
    timeout = timeout / 1000;

  start_time = g_get_monotonic_time ();

#ifdef G_OS_WIN32
  {
    GIOCondition current_condition;
    WSAEVENT events[2];
    DWORD res;
    GPollFD cancel_fd;
    int num_events;

    /* Always check these */
    condition |=  G_IO_ERR | G_IO_HUP;

    add_condition_watch (socket, &condition);

    num_events = 0;
    events[num_events++] = socket->priv->event;

    if (g_cancellable_make_pollfd (cancellable, &cancel_fd))
      events[num_events++] = (WSAEVENT)cancel_fd.fd;

    if (timeout == -1)
      timeout = WSA_INFINITE;

    g_mutex_lock (&socket->priv->win32_source_lock);
    current_condition = update_condition_unlocked (socket);
    while ((condition & current_condition) == 0)
      {
        if (!socket->priv->waiting)
          {
            socket->priv->waiting = TRUE;
            socket->priv->waiting_result = 0;
            g_mutex_unlock (&socket->priv->win32_source_lock);

            res = WSAWaitForMultipleEvents (num_events, events, FALSE, timeout, FALSE);

            g_mutex_lock (&socket->priv->win32_source_lock);
            socket->priv->waiting = FALSE;
            socket->priv->waiting_result = res;
            g_cond_broadcast (&socket->priv->win32_source_cond);
          }
        else
          {
            if (timeout != WSA_INFINITE)
              {
                if (!g_cond_wait_until (&socket->priv->win32_source_cond, &socket->priv->win32_source_lock, timeout))
                  {
                    res = WSA_WAIT_TIMEOUT;
                    break;
                  }
                else
                  {
                    res = socket->priv->waiting_result;
                  }
              }
            else
              {
                g_cond_wait (&socket->priv->win32_source_cond, &socket->priv->win32_source_lock);
                res = socket->priv->waiting_result;
              }
          }

	if (res == WSA_WAIT_FAILED)
	  {
	    int errsv = get_socket_errno ();

	    g_set_error (error, G_IO_ERROR,
			 socket_io_error_from_errno (errsv),
			 _("Waiting for socket condition: %s"),
			 socket_strerror (errsv));
	    break;
	  }
	else if (res == WSA_WAIT_TIMEOUT)
	  {
	    g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_TIMED_OUT,
				 _("Socket I/O timed out"));
	    break;
	  }

	if (g_cancellable_set_error_if_cancelled (cancellable, error))
	  break;

        current_condition = update_condition_unlocked (socket);

	if (timeout != WSA_INFINITE)
	  {
	    timeout -= (g_get_monotonic_time () - start_time) * 1000;
	    if (timeout < 0)
	      timeout = 0;
	  }
      }
    g_mutex_unlock (&socket->priv->win32_source_lock);
    remove_condition_watch (socket, &condition);
    if (num_events > 1)
      g_cancellable_release_fd (cancellable);

    return (condition & current_condition) != 0;
  }
#else
  {
    GPollFD poll_fd[2];
    gint result;
    gint num;

    poll_fd[0].fd = socket->priv->fd;
    poll_fd[0].events = condition;
    num = 1;

    if (g_cancellable_make_pollfd (cancellable, &poll_fd[1]))
      num++;

    while (TRUE)
      {
	int errsv;
	result = g_poll (poll_fd, num, timeout);
	errsv = errno;
	if (result != -1 || errsv != EINTR)
	  break;

	if (timeout != -1)
	  {
	    timeout -= (g_get_monotonic_time () - start_time) / 1000;
	    if (timeout < 0)
	      timeout = 0;
	  }
      }
    
    if (num > 1)
      g_cancellable_release_fd (cancellable);

    if (result == 0)
      {
	g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_TIMED_OUT,
			     _("Socket I/O timed out"));
	return FALSE;
      }

    return !g_cancellable_set_error_if_cancelled (cancellable, error);
  }
  #endif
}

#ifndef G_OS_WIN32

/* Unfortunately these have to be macros rather than inline functions due to
 * using alloca(). */
#define output_message_to_msghdr(message, prev_message, msg, prev_msg, error) \
G_STMT_START { \
  const GOutputMessage  *_message = (message); \
  const GOutputMessage *_prev_message = (prev_message); \
  struct msghdr *_msg = (msg); \
  const struct msghdr *_prev_msg = (prev_msg); \
  GError **_error = (error); \
 \
  _msg->msg_flags = 0; \
 \
  /* name */ \
  if (_prev_message != NULL && _prev_message->address == _message->address) \
    { \
      _msg->msg_name = _prev_msg->msg_name; \
      _msg->msg_namelen = _prev_msg->msg_namelen; \
    } \
  else if (_message->address != NULL) \
    { \
      _msg->msg_namelen = g_socket_address_get_native_size (_message->address); \
      _msg->msg_name = g_alloca (_msg->msg_namelen); \
      if (!g_socket_address_to_native (_message->address, _msg->msg_name, \
                                       _msg->msg_namelen, _error)) \
        break; \
    } \
  else \
    { \
      _msg->msg_name = NULL; \
      _msg->msg_namelen = 0; \
    } \
 \
  /* iov */ \
  { \
    /* this entire expression will be evaluated at compile time */ \
    if (sizeof *_msg->msg_iov == sizeof *_message->vectors && \
        sizeof _msg->msg_iov->iov_base == sizeof _message->vectors->buffer && \
        G_STRUCT_OFFSET (struct iovec, iov_base) == \
        G_STRUCT_OFFSET (GOutputVector, buffer) && \
        sizeof _msg->msg_iov->iov_len == sizeof _message->vectors->size && \
        G_STRUCT_OFFSET (struct iovec, iov_len) == \
        G_STRUCT_OFFSET (GOutputVector, size)) \
      /* ABI is compatible */ \
      { \
        _msg->msg_iov = (struct iovec *) _message->vectors; \
        _msg->msg_iovlen = _message->num_vectors; \
      } \
    else \
      /* ABI is incompatible */ \
      { \
        gint i; \
 \
        _msg->msg_iov = g_newa (struct iovec, _message->num_vectors); \
        for (i = 0; i < _message->num_vectors; i++) \
          { \
            _msg->msg_iov[i].iov_base = (void *) _message->vectors[i].buffer; \
            _msg->msg_iov[i].iov_len = _message->vectors[i].size; \
          } \
        _msg->msg_iovlen = _message->num_vectors; \
      } \
  } \
 \
  /* control */ \
  { \
    struct cmsghdr *cmsg; \
    gint i; \
 \
    _msg->msg_controllen = 0; \
    for (i = 0; i < _message->num_control_messages; i++) \
      _msg->msg_controllen += CMSG_SPACE (g_socket_control_message_get_size (_message->control_messages[i])); \
 \
    if (_msg->msg_controllen == 0) \
      _msg->msg_control = NULL; \
    else \
      { \
        _msg->msg_control = g_alloca (_msg->msg_controllen); \
        memset (_msg->msg_control, '\0', _msg->msg_controllen); \
      } \
 \
    cmsg = CMSG_FIRSTHDR (_msg); \
    for (i = 0; i < _message->num_control_messages; i++) \
      { \
        cmsg->cmsg_level = g_socket_control_message_get_level (_message->control_messages[i]); \
        cmsg->cmsg_type = g_socket_control_message_get_msg_type (_message->control_messages[i]); \
        cmsg->cmsg_len = CMSG_LEN (g_socket_control_message_get_size (_message->control_messages[i])); \
        g_socket_control_message_serialize (_message->control_messages[i], \
                                            CMSG_DATA (cmsg)); \
        cmsg = CMSG_NXTHDR (_msg, cmsg); \
      } \
    g_assert (cmsg == NULL); \
  } \
} G_STMT_END

#define input_message_to_msghdr(message, msg) \
G_STMT_START { \
  const GInputMessage  *_message = (message); \
  struct msghdr *_msg = (msg); \
 \
  /* name */ \
  if (_message->address) \
    { \
      _msg->msg_namelen = sizeof (struct sockaddr_storage); \
      _msg->msg_name = g_alloca (_msg->msg_namelen); \
    } \
  else \
    { \
      _msg->msg_name = NULL; \
      _msg->msg_namelen = 0; \
    } \
 \
  /* iov */ \
  /* this entire expression will be evaluated at compile time */ \
  if (sizeof *_msg->msg_iov == sizeof *_message->vectors && \
      sizeof _msg->msg_iov->iov_base == sizeof _message->vectors->buffer && \
      G_STRUCT_OFFSET (struct iovec, iov_base) == \
      G_STRUCT_OFFSET (GInputVector, buffer) && \
      sizeof _msg->msg_iov->iov_len == sizeof _message->vectors->size && \
      G_STRUCT_OFFSET (struct iovec, iov_len) == \
      G_STRUCT_OFFSET (GInputVector, size)) \
    /* ABI is compatible */ \
    { \
      _msg->msg_iov = (struct iovec *) _message->vectors; \
      _msg->msg_iovlen = _message->num_vectors; \
    } \
  else \
    /* ABI is incompatible */ \
    { \
      guint i; \
 \
      _msg->msg_iov = g_newa (struct iovec, _message->num_vectors); \
      for (i = 0; i < _message->num_vectors; i++) \
        { \
          _msg->msg_iov[i].iov_base = _message->vectors[i].buffer; \
          _msg->msg_iov[i].iov_len = _message->vectors[i].size; \
        } \
      _msg->msg_iovlen = _message->num_vectors; \
    } \
 \
  /* control */ \
  if (_message->control_messages == NULL) \
    { \
	  _msg->msg_controllen = 0; \
	  _msg->msg_control = NULL; \
    } \
  else \
    { \
      _msg->msg_controllen = 2048; \
      _msg->msg_control = g_alloca (_msg->msg_controllen); \
    } \
 \
  /* flags */ \
  _msg->msg_flags = _message->flags; \
} G_STMT_END

static void
input_message_from_msghdr (const struct msghdr  *msg,
                           GInputMessage        *message,
                           GSocket              *socket)
{
  /* decode address */
  if (message->address != NULL)
    {
      *message->address = cache_recv_address (socket, msg->msg_name,
                                              msg->msg_namelen);
    }

  /* decode control messages */
  {
    GPtrArray *my_messages = NULL;
    struct cmsghdr *cmsg;

    if (msg->msg_controllen >= sizeof (struct cmsghdr))
      {
        g_assert (message->control_messages != NULL);
        for (cmsg = CMSG_FIRSTHDR (msg);
             cmsg != NULL;
             cmsg = CMSG_NXTHDR ((struct msghdr *) msg, cmsg))
          {
            GSocketControlMessage *control_message;

            control_message = g_socket_control_message_deserialize (cmsg->cmsg_level,
                                                                    cmsg->cmsg_type,
                                                                    cmsg->cmsg_len - ((char *)CMSG_DATA (cmsg) - (char *)cmsg),
                                                                    CMSG_DATA (cmsg));
            if (control_message == NULL)
              /* We've already spewed about the problem in the
                 deserialization code, so just continue */
              continue;

            if (my_messages == NULL)
              my_messages = g_ptr_array_new ();
            g_ptr_array_add (my_messages, control_message);
           }
      }

    if (message->num_control_messages)
      *message->num_control_messages = my_messages != NULL ? my_messages->len : 0;

    if (message->control_messages)
      {
        if (my_messages == NULL)
          {
            *message->control_messages = NULL;
          }
        else
          {
            g_ptr_array_add (my_messages, NULL);
            *message->control_messages = (GSocketControlMessage **) g_ptr_array_free (my_messages, FALSE);
          }
      }
    else
      {
        g_assert (my_messages == NULL);
      }
  }

  /* capture the flags */
  message->flags = msg->msg_flags;
}
#endif

/**
 * g_socket_send_message:
 * @socket: a #GSocket
 * @address: (nullable): a #GSocketAddress, or %NULL
 * @vectors: (array length=num_vectors): an array of #GOutputVector structs
 * @num_vectors: the number of elements in @vectors, or -1
 * @messages: (array length=num_messages) (nullable): a pointer to an
 *   array of #GSocketControlMessages, or %NULL.
 * @num_messages: number of elements in @messages, or -1.
 * @flags: an int containing #GSocketMsgFlags flags
 * @cancellable: (nullable): a %GCancellable or %NULL
 * @error: #GError for error reporting, or %NULL to ignore.
 *
 * Send data to @address on @socket.  For sending multiple messages see
 * g_socket_send_messages(); for easier use, see
 * g_socket_send() and g_socket_send_to().
 *
 * If @address is %NULL then the message is sent to the default receiver
 * (set by g_socket_connect()).
 *
 * @vectors must point to an array of #GOutputVector structs and
 * @num_vectors must be the length of this array. (If @num_vectors is -1,
 * then @vectors is assumed to be terminated by a #GOutputVector with a
 * %NULL buffer pointer.) The #GOutputVector structs describe the buffers
 * that the sent data will be gathered from. Using multiple
 * #GOutputVectors is more memory-efficient than manually copying
 * data from multiple sources into a single buffer, and more
 * network-efficient than making multiple calls to g_socket_send().
 *
 * @messages, if non-%NULL, is taken to point to an array of @num_messages
 * #GSocketControlMessage instances. These correspond to the control
 * messages to be sent on the socket.
 * If @num_messages is -1 then @messages is treated as a %NULL-terminated
 * array.
 *
 * @flags modify how the message is sent. The commonly available arguments
 * for this are available in the #GSocketMsgFlags enum, but the
 * values there are the same as the system values, and the flags
 * are passed in as-is, so you can pass in system-specific flags too.
 *
 * If the socket is in blocking mode the call will block until there is
 * space for the data in the socket queue. If there is no space available
 * and the socket is in non-blocking mode a %G_IO_ERROR_WOULD_BLOCK error
 * will be returned. To be notified when space is available, wait for the
 * %G_IO_OUT condition. Note though that you may still receive
 * %G_IO_ERROR_WOULD_BLOCK from g_socket_send() even if you were previously
 * notified of a %G_IO_OUT condition. (On Windows in particular, this is
 * very common due to the way the underlying APIs work.)
 *
 * On error -1 is returned and @error is set accordingly.
 *
 * Returns: Number of bytes written (which may be less than @size), or -1
 * on error
 *
 * Since: 2.22
 */
gssize
g_socket_send_message (GSocket                *socket,
		       GSocketAddress         *address,
		       GOutputVector          *vectors,
		       gint                    num_vectors,
		       GSocketControlMessage **messages,
		       gint                    num_messages,
		       gint                    flags,
		       GCancellable           *cancellable,
		       GError                **error)
{
  return g_socket_send_message_with_timeout (socket, address,
                                             vectors, num_vectors,
                                             messages, num_messages, flags,
                                             socket->priv->blocking ? -1 : 0,
                                             cancellable, error);
}

static gssize
g_socket_send_message_with_timeout (GSocket                *socket,
                                    GSocketAddress         *address,
                                    GOutputVector          *vectors,
                                    gint                    num_vectors,
                                    GSocketControlMessage **messages,
                                    gint                    num_messages,
                                    gint                    flags,
                                    gint64                  timeout,
                                    GCancellable           *cancellable,
                                    GError                **error)
{
  GOutputVector one_vector;
  char zero;
  gint64 start_time;

  g_return_val_if_fail (G_IS_SOCKET (socket), -1);
  g_return_val_if_fail (address == NULL || G_IS_SOCKET_ADDRESS (address), -1);
  g_return_val_if_fail (num_vectors == 0 || vectors != NULL, -1);
  g_return_val_if_fail (num_messages == 0 || messages != NULL, -1);
  g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), -1);
  g_return_val_if_fail (error == NULL || *error == NULL, -1);

  start_time = g_get_monotonic_time ();

  if (!check_socket (socket, error))
    return -1;

  if (!check_timeout (socket, error))
    return -1;

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return -1;

  if (num_vectors == -1)
    {
      for (num_vectors = 0;
	   vectors[num_vectors].buffer != NULL;
	   num_vectors++)
	;
    }

  if (num_messages == -1)
    {
      for (num_messages = 0;
	   messages != NULL && messages[num_messages] != NULL;
	   num_messages++)
	;
    }

  if (num_vectors == 0)
    {
      zero = '\0';

      one_vector.buffer = &zero;
      one_vector.size = 1;
      num_vectors = 1;
      vectors = &one_vector;
    }

#ifndef G_OS_WIN32
  {
    GOutputMessage output_message;
    struct msghdr msg;
    gssize result;
    GError *child_error = NULL;

    output_message.address = address;
    output_message.vectors = vectors;
    output_message.num_vectors = num_vectors;
    output_message.bytes_sent = 0;
    output_message.control_messages = messages;
    output_message.num_control_messages = num_messages;

    output_message_to_msghdr (&output_message, NULL, &msg, NULL, &child_error);

    if (child_error != NULL)
      {
        g_propagate_error (error, child_error);
        return -1;
      }

    while (1)
      {
	result = sendmsg (socket->priv->fd, &msg, flags | G_SOCKET_DEFAULT_SEND_FLAGS);
	if (result < 0)
	  {
	    int errsv = get_socket_errno ();

	    if (errsv == EINTR)
	      continue;

	    if (timeout != 0 &&
		(errsv == EWOULDBLOCK ||
		 errsv == EAGAIN))
              {
                if (!block_on_timeout (socket, G_IO_OUT, timeout, start_time,
                                       cancellable, error))
                  return -1;

                continue;
              }

	    socket_set_error_lazy (error, errsv, _("Error sending message: %s"));
	    return -1;
	  }
	break;
      }

    return result;
  }
#else
  {
    struct sockaddr_storage addr;
    guint addrlen;
    DWORD bytes_sent;
    int result;
    WSABUF *bufs;
    gint i;

    /* Win32 doesn't support control messages.
       Actually this is possible for raw and datagram sockets
       via WSASendMessage on Vista or later, but that doesn't
       seem very useful */
    if (num_messages != 0)
      {
        g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                             _("GSocketControlMessage not supported on Windows"));
	return -1;
      }

    /* iov */
    bufs = g_newa (WSABUF, num_vectors);
    for (i = 0; i < num_vectors; i++)
      {
	bufs[i].buf = (char *)vectors[i].buffer;
	bufs[i].len = (gulong)vectors[i].size;
      }

    /* name */
    addrlen = 0; /* Avoid warning */
    if (address)
      {
	addrlen = g_socket_address_get_native_size (address);
	if (!g_socket_address_to_native (address, &addr, sizeof addr, error))
	  return -1;
      }

    while (1)
      {
        win32_unset_event_mask (socket, FD_WRITE);

	if (address)
	  result = WSASendTo (socket->priv->fd,
			      bufs, num_vectors,
			      &bytes_sent, flags,
			      (const struct sockaddr *)&addr, addrlen,
			      NULL, NULL);
	else
	  result = WSASend (socket->priv->fd,
			    bufs, num_vectors,
			    &bytes_sent, flags,
			    NULL, NULL);

	if (result != 0)
	  {
	    int errsv = get_socket_errno ();

	    if (errsv == WSAEINTR)
	      continue;

	    if (errsv == WSAEWOULDBLOCK)
              {
                if (timeout != 0)
                  {
                    if (!block_on_timeout (socket, G_IO_OUT, timeout,
                                           start_time, cancellable, error))
                      return -1;

                    continue;
                  }
              }

	    socket_set_error_lazy (error, errsv, _("Error sending message: %s"));
	    return -1;
	  }
	break;
      }

    return bytes_sent;
  }
#endif
}

/**
 * g_socket_send_messages:
 * @socket: a #GSocket
 * @messages: (array length=num_messages): an array of #GOutputMessage structs
 * @num_messages: the number of elements in @messages
 * @flags: an int containing #GSocketMsgFlags flags
 * @cancellable: (nullable): a %GCancellable or %NULL
 * @error: #GError for error reporting, or %NULL to ignore.
 *
 * Send multiple data messages from @socket in one go.  This is the most
 * complicated and fully-featured version of this call. For easier use, see
 * g_socket_send(), g_socket_send_to(), and g_socket_send_message().
 *
 * @messages must point to an array of #GOutputMessage structs and
 * @num_messages must be the length of this array. Each #GOutputMessage
 * contains an address to send the data to, and a pointer to an array of
 * #GOutputVector structs to describe the buffers that the data to be sent
 * for each message will be gathered from. Using multiple #GOutputVectors is
 * more memory-efficient than manually copying data from multiple sources
 * into a single buffer, and more network-efficient than making multiple
 * calls to g_socket_send(). Sending multiple messages in one go avoids the
 * overhead of making a lot of syscalls in scenarios where a lot of data
 * packets need to be sent (e.g. high-bandwidth video streaming over RTP/UDP),
 * or where the same data needs to be sent to multiple recipients.
 *
 * @flags modify how the message is sent. The commonly available arguments
 * for this are available in the #GSocketMsgFlags enum, but the
 * values there are the same as the system values, and the flags
 * are passed in as-is, so you can pass in system-specific flags too.
 *
 * If the socket is in blocking mode the call will block until there is
 * space for all the data in the socket queue. If there is no space available
 * and the socket is in non-blocking mode a %G_IO_ERROR_WOULD_BLOCK error
 * will be returned if no data was written at all, otherwise the number of
 * messages sent will be returned. To be notified when space is available,
 * wait for the %G_IO_OUT condition. Note though that you may still receive
 * %G_IO_ERROR_WOULD_BLOCK from g_socket_send() even if you were previously
 * notified of a %G_IO_OUT condition. (On Windows in particular, this is
 * very common due to the way the underlying APIs work.)
 *
 * On error -1 is returned and @error is set accordingly. An error will only
 * be returned if zero messages could be sent; otherwise the number of messages
 * successfully sent before the error will be returned.
 *
 * Returns: number of messages sent, or -1 on error. Note that the number of
 *     messages sent may be smaller than @num_messages if the socket is
 *     non-blocking or if @num_messages was larger than UIO_MAXIOV (1024),
 *     in which case the caller may re-try to send the remaining messages.
 *
 * Since: 2.44
 */
gint
g_socket_send_messages (GSocket        *socket,
		        GOutputMessage *messages,
		        guint           num_messages,
		        gint            flags,
		        GCancellable   *cancellable,
		        GError        **error)
{
  return g_socket_send_messages_with_timeout (socket, messages, num_messages,
                                              flags,
                                              socket->priv->blocking ? -1 : 0,
                                              cancellable, error);
}

static gint
g_socket_send_messages_with_timeout (GSocket        *socket,
                                     GOutputMessage *messages,
                                     guint           num_messages,
                                     gint            flags,
                                     gint64          timeout,
                                     GCancellable   *cancellable,
                                     GError        **error)
{
  gint64 start_time;

  g_return_val_if_fail (G_IS_SOCKET (socket), -1);
  g_return_val_if_fail (num_messages == 0 || messages != NULL, -1);
  g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), -1);
  g_return_val_if_fail (error == NULL || *error == NULL, -1);

  start_time = g_get_monotonic_time ();

  if (!check_socket (socket, error))
    return -1;

  if (!check_timeout (socket, error))
    return -1;

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return -1;

  if (num_messages == 0)
    return 0;

#if !defined (G_OS_WIN32) && defined (HAVE_SENDMMSG)
  {
    struct mmsghdr *msgvec;
    gint i, num_sent;

#ifdef UIO_MAXIOV
#define MAX_NUM_MESSAGES UIO_MAXIOV
#else
#define MAX_NUM_MESSAGES 1024
#endif

    if (num_messages > MAX_NUM_MESSAGES)
      num_messages = MAX_NUM_MESSAGES;

    msgvec = g_newa (struct mmsghdr, num_messages);

    for (i = 0; i < num_messages; ++i)
      {
        GOutputMessage *msg = &messages[i];
        struct msghdr *msg_hdr = &msgvec[i].msg_hdr;
        GError *child_error = NULL;

        msgvec[i].msg_len = 0;

        output_message_to_msghdr (msg, (i > 0) ? &messages[i - 1] : NULL,
                                  msg_hdr, (i > 0) ? &msgvec[i - 1].msg_hdr : NULL,
                                  &child_error);

        if (child_error != NULL)
          {
            g_propagate_error (error, child_error);
            return -1;
          }
      }

    for (num_sent = 0; num_sent < num_messages;)
      {
        gint ret;

        ret = sendmmsg (socket->priv->fd, msgvec + num_sent, num_messages - num_sent,
                        flags | G_SOCKET_DEFAULT_SEND_FLAGS);

        if (ret < 0)
          {
            int errsv = get_socket_errno ();

            if (errsv == EINTR)
              continue;

            if (timeout != 0 &&
                (errsv == EWOULDBLOCK ||
                 errsv == EAGAIN))
              {
                if (!block_on_timeout (socket, G_IO_OUT, timeout, start_time,
                                       cancellable, error))
                  {
                    if (num_sent > 0)
                      {
                        g_clear_error (error);
                        break;
                      }

                    return -1;
                  }

                continue;
              }

            /* If any messages were successfully sent, do not error. */
            if (num_sent > 0)
              break;

            socket_set_error_lazy (error, errsv, _("Error sending message: %s"));

            return -1;
          }

        num_sent += ret;
      }

    for (i = 0; i < num_sent; ++i)
      messages[i].bytes_sent = msgvec[i].msg_len;

    return num_sent;
  }
#else
  {
    gssize result;
    gint i;
    gint64 wait_timeout;

    wait_timeout = timeout;

    for (i = 0; i < num_messages; ++i)
      {
        GOutputMessage *msg = &messages[i];
        GError *msg_error = NULL;

        result = g_socket_send_message_with_timeout (socket, msg->address,
                                                     msg->vectors,
                                                     msg->num_vectors,
                                                     msg->control_messages,
                                                     msg->num_control_messages,
                                                     flags, wait_timeout,
                                                     cancellable, &msg_error);

        /* check if we've timed out or how much time to wait at most */
        if (timeout > 0)
          {
            gint64 elapsed = g_get_monotonic_time () - start_time;
            wait_timeout = MAX (timeout - elapsed, 1);
          }

        if (result < 0)
          {
            /* if we couldn't send all messages, just return how many we did
             * manage to send, provided we managed to send at least one */
            if (i > 0)
              {
                g_error_free (msg_error);
                return i;
              }
            else
              {
                g_propagate_error (error, msg_error);
                return -1;
              }
          }

        msg->bytes_sent = result;
      }

    return i;
  }
#endif
}

static GSocketAddress *
cache_recv_address (GSocket *socket, struct sockaddr *native, int native_len)
{
  GSocketAddress *saddr;
  gint i;
  guint64 oldest_time = G_MAXUINT64;
  gint oldest_index = 0;

  if (native_len <= 0)
    return NULL;

  saddr = NULL;
  for (i = 0; i < RECV_ADDR_CACHE_SIZE; i++)
    {
      GSocketAddress *tmp = socket->priv->recv_addr_cache[i].addr;
      gpointer tmp_native = socket->priv->recv_addr_cache[i].native;
      gint tmp_native_len = socket->priv->recv_addr_cache[i].native_len;

      if (!tmp)
        continue;

      if (tmp_native_len != native_len)
        continue;

      if (memcmp (tmp_native, native, native_len) == 0)
        {
          saddr = g_object_ref (tmp);
          socket->priv->recv_addr_cache[i].last_used = g_get_monotonic_time ();
          return saddr;
        }

      if (socket->priv->recv_addr_cache[i].last_used < oldest_time)
        {
          oldest_time = socket->priv->recv_addr_cache[i].last_used;
          oldest_index = i;
        }
    }

  saddr = g_socket_address_new_from_native (native, native_len);

  if (socket->priv->recv_addr_cache[oldest_index].addr)
    {
      g_object_unref (socket->priv->recv_addr_cache[oldest_index].addr);
      g_free (socket->priv->recv_addr_cache[oldest_index].native);
    }

  socket->priv->recv_addr_cache[oldest_index].native = g_memdup (native, native_len);
  socket->priv->recv_addr_cache[oldest_index].native_len = native_len;
  socket->priv->recv_addr_cache[oldest_index].addr = g_object_ref (saddr);
  socket->priv->recv_addr_cache[oldest_index].last_used = g_get_monotonic_time ();

  return saddr;
}

static gssize
g_socket_receive_message_with_timeout (GSocket                 *socket,
                                       GSocketAddress         **address,
                                       GInputVector            *vectors,
                                       gint                     num_vectors,
                                       GSocketControlMessage ***messages,
                                       gint                    *num_messages,
                                       gint                    *flags,
                                       gint64                   timeout,
                                       GCancellable            *cancellable,
                                       GError                 **error)
{
  GInputVector one_vector;
  char one_byte;
  gint64 start_time;

  g_return_val_if_fail (G_IS_SOCKET (socket), -1);

  start_time = g_get_monotonic_time ();

  if (!check_socket (socket, error))
    return -1;

  if (!check_timeout (socket, error))
    return -1;

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return -1;

  if (num_vectors == -1)
    {
      for (num_vectors = 0;
	   vectors[num_vectors].buffer != NULL;
	   num_vectors++)
	;
    }

  if (num_vectors == 0)
    {
      one_vector.buffer = &one_byte;
      one_vector.size = 1;
      num_vectors = 1;
      vectors = &one_vector;
    }

#ifndef G_OS_WIN32
  {
    GInputMessage input_message;
    struct msghdr msg;
    gssize result;

    input_message.address = address;
    input_message.vectors = vectors;
    input_message.num_vectors = num_vectors;
    input_message.bytes_received = 0;
    input_message.flags = (flags != NULL) ? *flags : 0;
    input_message.control_messages = messages;
    input_message.num_control_messages = (guint *) num_messages;

    /* We always set the close-on-exec flag so we don't leak file
     * descriptors into child processes.  Note that gunixfdmessage.c
     * will later call fcntl (fd, FD_CLOEXEC), but that isn't atomic.
     */
#ifdef MSG_CMSG_CLOEXEC
    input_message.flags |= MSG_CMSG_CLOEXEC;
#endif

    input_message_to_msghdr (&input_message, &msg);

    /* do it */
    while (1)
      {
	result = recvmsg (socket->priv->fd, &msg, msg.msg_flags);
#ifdef MSG_CMSG_CLOEXEC	
	if (result < 0 && get_socket_errno () == EINVAL)
	  {
	    /* We must be running on an old kernel.  Call without the flag. */
	    msg.msg_flags &= ~(MSG_CMSG_CLOEXEC);
	    result = recvmsg (socket->priv->fd, &msg, msg.msg_flags);
	  }
#endif

	if (result < 0)
	  {
	    int errsv = get_socket_errno ();

	    if (errsv == EINTR)
	      continue;

	    if (timeout != 0 &&
		(errsv == EWOULDBLOCK ||
		 errsv == EAGAIN))
	      {
                if (!block_on_timeout (socket, G_IO_IN, timeout, start_time,
                                       cancellable, error))
                  return -1;

                continue;
	      }

	    socket_set_error_lazy (error, errsv, _("Error receiving message: %s"));
	    return -1;
	  }
	break;
      }

    input_message_from_msghdr (&msg, &input_message, socket);

    if (flags != NULL)
      *flags = input_message.flags;

    return result;
  }
#else
  {
    struct sockaddr_storage addr;
    int addrlen;
    DWORD bytes_received;
    DWORD win_flags;
    int result;
    WSABUF *bufs;
    gint i;

    /* iov */
    bufs = g_newa (WSABUF, num_vectors);
    for (i = 0; i < num_vectors; i++)
      {
	bufs[i].buf = (char *)vectors[i].buffer;
	bufs[i].len = (gulong)vectors[i].size;
      }

    /* flags */
    if (flags != NULL)
      win_flags = *flags;
    else
      win_flags = 0;

    /* do it */
    while (1)
      {
        win32_unset_event_mask (socket, FD_READ);

	addrlen = sizeof addr;
	if (address)
	  result = WSARecvFrom (socket->priv->fd,
				bufs, num_vectors,
				&bytes_received, &win_flags,
				(struct sockaddr *)&addr, &addrlen,
				NULL, NULL);
	else
	  result = WSARecv (socket->priv->fd,
			    bufs, num_vectors,
			    &bytes_received, &win_flags,
			    NULL, NULL);
	if (result != 0)
	  {
	    int errsv = get_socket_errno ();

	    if (errsv == WSAEINTR)
	      continue;

            if (errsv == WSAEWOULDBLOCK)
              {
                if (timeout != 0)
                  {
                    if (!block_on_timeout (socket, G_IO_IN, timeout,
                                           start_time, cancellable, error))
                      return -1;

                    continue;
                  }
              }

	    socket_set_error_lazy (error, errsv, _("Error receiving message: %s"));
	    return -1;
	  }
	break;
      }

    /* decode address */
    if (address != NULL)
      {
        *address = cache_recv_address (socket, (struct sockaddr *)&addr, addrlen);
      }

    /* capture the flags */
    if (flags != NULL)
      *flags = win_flags;

    if (messages != NULL)
      *messages = NULL;
    if (num_messages != NULL)
      *num_messages = 0;

    return bytes_received;
  }
#endif
}

/**
 * g_socket_receive_messages:
 * @socket: a #GSocket
 * @messages: (array length=num_messages): an array of #GInputMessage structs
 * @num_messages: the number of elements in @messages
 * @flags: an int containing #GSocketMsgFlags flags for the overall operation
 * @cancellable: (nullable): a %GCancellable or %NULL
 * @error: #GError for error reporting, or %NULL to ignore
 *
 * Receive multiple data messages from @socket in one go.  This is the most
 * complicated and fully-featured version of this call. For easier use, see
 * g_socket_receive(), g_socket_receive_from(), and g_socket_receive_message().
 *
 * @messages must point to an array of #GInputMessage structs and
 * @num_messages must be the length of this array. Each #GInputMessage
 * contains a pointer to an array of #GInputVector structs describing the
 * buffers that the data received in each message will be written to. Using
 * multiple #GInputVectors is more memory-efficient than manually copying data
 * out of a single buffer to multiple sources, and more system-call-efficient
 * than making multiple calls to g_socket_receive(), such as in scenarios where
 * a lot of data packets need to be received (e.g. high-bandwidth video
 * streaming over RTP/UDP).
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
 * If #GSocket:blocking is %TRUE the call will block until @num_messages have
 * been received, or the end of the stream is reached.
 *
 * If #GSocket:blocking is %FALSE the call will return up to @num_messages
 * without blocking, or %G_IO_ERROR_WOULD_BLOCK if no messages are queued in the
 * operating system to be received.
 *
 * In blocking mode, if #GSocket:timeout is positive and is reached before any
 * messages are received, %G_IO_ERROR_TIMED_OUT is returned, otherwise up to
 * @num_messages are returned. (Note: This is effectively the
 * behaviour of `MSG_WAITFORONE` with recvmmsg().)
 *
 * To be notified when messages are available, wait for the
 * %G_IO_IN condition. Note though that you may still receive
 * %G_IO_ERROR_WOULD_BLOCK from g_socket_receive_messages() even if you were
 * previously notified of a %G_IO_IN condition.
 *
 * If the remote peer closes the connection, any messages queued in the
 * operating system will be returned, and subsequent calls to
 * g_socket_receive_messages() will return 0 (with no error set).
 *
 * On error -1 is returned and @error is set accordingly. An error will only
 * be returned if zero messages could be received; otherwise the number of
 * messages successfully received before the error will be returned.
 *
 * Returns: number of messages received, or -1 on error. Note that the number
 *     of messages received may be smaller than @num_messages if in non-blocking
 *     mode, if the peer closed the connection, or if @num_messages
 *     was larger than `UIO_MAXIOV` (1024), in which case the caller may re-try
 *     to receive the remaining messages.
 *
 * Since: 2.48
 */
gint
g_socket_receive_messages (GSocket        *socket,
                           GInputMessage  *messages,
                           guint           num_messages,
                           gint            flags,
                           GCancellable   *cancellable,
                           GError        **error)
{
  if (!check_socket (socket, error) ||
      !check_timeout (socket, error))
    return -1;

  return g_socket_receive_messages_with_timeout (socket, messages, num_messages,
                                                 flags,
                                                 socket->priv->blocking ? -1 : 0,
                                                 cancellable, error);
}

static gint
g_socket_receive_messages_with_timeout (GSocket        *socket,
                                        GInputMessage  *messages,
                                        guint           num_messages,
                                        gint            flags,
                                        gint64          timeout,
                                        GCancellable   *cancellable,
                                        GError        **error)
{
  gint64 start_time;

  g_return_val_if_fail (G_IS_SOCKET (socket), -1);
  g_return_val_if_fail (num_messages == 0 || messages != NULL, -1);
  g_return_val_if_fail (cancellable == NULL ||
                        G_IS_CANCELLABLE (cancellable), -1);
  g_return_val_if_fail (error == NULL || *error == NULL, -1);

  start_time = g_get_monotonic_time ();

  if (!check_socket (socket, error))
    return -1;

  if (!check_timeout (socket, error))
    return -1;

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return -1;

  if (num_messages == 0)
    return 0;

#if !defined (G_OS_WIN32) && defined (HAVE_RECVMMSG)
  {
    struct mmsghdr *msgvec;
    guint i, num_received;

#ifdef UIO_MAXIOV
#define MAX_NUM_MESSAGES UIO_MAXIOV
#else
#define MAX_NUM_MESSAGES 1024
#endif

    if (num_messages > MAX_NUM_MESSAGES)
      num_messages = MAX_NUM_MESSAGES;

    msgvec = g_newa (struct mmsghdr, num_messages);

    for (i = 0; i < num_messages; ++i)
      {
        GInputMessage *msg = &messages[i];
        struct msghdr *msg_hdr = &msgvec[i].msg_hdr;

        input_message_to_msghdr (msg, msg_hdr);
        msgvec[i].msg_len = 0;
      }

    /* We always set the close-on-exec flag so we don't leak file
     * descriptors into child processes.  Note that gunixfdmessage.c
     * will later call fcntl (fd, FD_CLOEXEC), but that isn't atomic.
     */
#ifdef MSG_CMSG_CLOEXEC
    flags |= MSG_CMSG_CLOEXEC;
#endif

    for (num_received = 0; num_received < num_messages;)
      {
        gint ret;

        /* We operate in non-blocking mode and handle the timeout ourselves. */
        ret = recvmmsg (socket->priv->fd,
                        msgvec + num_received,
                        num_messages - num_received,
                        flags | G_SOCKET_DEFAULT_SEND_FLAGS, NULL);
#ifdef MSG_CMSG_CLOEXEC
        if (ret < 0 && get_socket_errno () == EINVAL)
          {
            /* We must be running on an old kernel. Call without the flag. */
            flags &= ~(MSG_CMSG_CLOEXEC);
            ret = recvmmsg (socket->priv->fd,
                            msgvec + num_received,
                            num_messages - num_received,
                            flags | G_SOCKET_DEFAULT_SEND_FLAGS, NULL);
          }
#endif

        if (ret < 0)
          {
            int errsv = get_socket_errno ();

            if (errsv == EINTR)
              continue;

            if (timeout != 0 &&
                (errsv == EWOULDBLOCK ||
                 errsv == EAGAIN))
              {
                if (!block_on_timeout (socket, G_IO_IN, timeout, start_time,
                                       cancellable, error))
                  {
                    if (num_received > 0)
                      {
                        g_clear_error (error);
                        break;
                      }

                    return -1;
                  }

                continue;
              }

            /* If any messages were successfully received, do not error. */
            if (num_received > 0)
              break;

            socket_set_error_lazy (error, errsv,
                                   _("Error receiving message: %s"));

            return -1;
          }
        else if (ret == 0)
          {
            /* EOS. */
            break;
          }

        num_received += ret;
      }

    for (i = 0; i < num_received; ++i)
      {
        input_message_from_msghdr (&msgvec[i].msg_hdr, &messages[i], socket);
        messages[i].bytes_received = msgvec[i].msg_len;
      }

    return num_received;
  }
#else
  {
    guint i;
    gint64 wait_timeout;

    wait_timeout = timeout;

    for (i = 0; i < num_messages; i++)
      {
        GInputMessage *msg = &messages[i];
        gssize len;
        GError *msg_error = NULL;

        msg->flags = flags;  /* in-out parameter */

        len = g_socket_receive_message_with_timeout (socket,
                                                     msg->address,
                                                     msg->vectors,
                                                     msg->num_vectors,
                                                     msg->control_messages,
                                                     (gint *) msg->num_control_messages,
                                                     &msg->flags,
                                                     wait_timeout,
                                                     cancellable,
                                                     &msg_error);

        /* check if we've timed out or how much time to wait at most */
        if (timeout > 0)
          {
            gint64 elapsed = g_get_monotonic_time () - start_time;
            wait_timeout = MAX (timeout - elapsed, 1);
          }

        if (len >= 0)
          msg->bytes_received = len;

        if (i != 0 &&
            (g_error_matches (msg_error, G_IO_ERROR, G_IO_ERROR_WOULD_BLOCK) ||
             g_error_matches (msg_error, G_IO_ERROR, G_IO_ERROR_TIMED_OUT)))
          {
            g_clear_error (&msg_error);
            break;
          }

        if (msg_error != NULL)
          {
            g_propagate_error (error, msg_error);
            return -1;
          }

        if (len == 0)
          break;
      }

    return i;
  }
#endif
}

/**
 * g_socket_receive_message:
 * @socket: a #GSocket
 * @address: (out) (optional): a pointer to a #GSocketAddress
 *     pointer, or %NULL
 * @vectors: (array length=num_vectors): an array of #GInputVector structs
 * @num_vectors: the number of elements in @vectors, or -1
 * @messages: (array length=num_messages) (out) (optional) (nullable): a pointer
 *    which may be filled with an array of #GSocketControlMessages, or %NULL
 * @num_messages: (out): a pointer which will be filled with the number of
 *    elements in @messages, or %NULL
 * @flags: (inout): a pointer to an int containing #GSocketMsgFlags flags
 * @cancellable: a %GCancellable or %NULL
 * @error: a #GError pointer, or %NULL
 *
 * Receive data from a socket.  For receiving multiple messages, see
 * g_socket_receive_messages(); for easier use, see
 * g_socket_receive() and g_socket_receive_from().
 *
 * If @address is non-%NULL then @address will be set equal to the
 * source address of the received packet.
 * @address is owned by the caller.
 *
 * @vector must point to an array of #GInputVector structs and
 * @num_vectors must be the length of this array.  These structs
 * describe the buffers that received data will be scattered into.
 * If @num_vectors is -1, then @vectors is assumed to be terminated
 * by a #GInputVector with a %NULL buffer pointer.
 *
 * As a special case, if @num_vectors is 0 (in which case, @vectors
 * may of course be %NULL), then a single byte is received and
 * discarded. This is to facilitate the common practice of sending a
 * single '\0' byte for the purposes of transferring ancillary data.
 *
 * @messages, if non-%NULL, will be set to point to a newly-allocated
 * array of #GSocketControlMessage instances or %NULL if no such
 * messages was received. These correspond to the control messages
 * received from the kernel, one #GSocketControlMessage per message
 * from the kernel. This array is %NULL-terminated and must be freed
 * by the caller using g_free() after calling g_object_unref() on each
 * element. If @messages is %NULL, any control messages received will
 * be discarded.
 *
 * @num_messages, if non-%NULL, will be set to the number of control
 * messages received.
 *
 * If both @messages and @num_messages are non-%NULL, then
 * @num_messages gives the number of #GSocketControlMessage instances
 * in @messages (ie: not including the %NULL terminator).
 *
 * @flags is an in/out parameter. The commonly available arguments
 * for this are available in the #GSocketMsgFlags enum, but the
 * values there are the same as the system values, and the flags
 * are passed in as-is, so you can pass in system-specific flags too
 * (and g_socket_receive_message() may pass system-specific flags out).
 * Flags passed in to the parameter affect the receive operation; flags returned
 * out of it are relevant to the specific returned message.
 *
 * As with g_socket_receive(), data may be discarded if @socket is
 * %G_SOCKET_TYPE_DATAGRAM or %G_SOCKET_TYPE_SEQPACKET and you do not
 * provide enough buffer space to read a complete message. You can pass
 * %G_SOCKET_MSG_PEEK in @flags to peek at the current message without
 * removing it from the receive queue, but there is no portable way to find
 * out the length of the message other than by reading it into a
 * sufficiently-large buffer.
 *
 * If the socket is in blocking mode the call will block until there
 * is some data to receive, the connection is closed, or there is an
 * error. If there is no data available and the socket is in
 * non-blocking mode, a %G_IO_ERROR_WOULD_BLOCK error will be
 * returned. To be notified when data is available, wait for the
 * %G_IO_IN condition.
 *
 * On error -1 is returned and @error is set accordingly.
 *
 * Returns: Number of bytes read, or 0 if the connection was closed by
 * the peer, or -1 on error
 *
 * Since: 2.22
 */
gssize
g_socket_receive_message (GSocket                 *socket,
			  GSocketAddress         **address,
			  GInputVector            *vectors,
			  gint                     num_vectors,
			  GSocketControlMessage ***messages,
			  gint                    *num_messages,
			  gint                    *flags,
			  GCancellable            *cancellable,
			  GError                 **error)
{
  return g_socket_receive_message_with_timeout (socket, address, vectors,
                                                 num_vectors, messages,
                                                 num_messages, flags,
                                                 socket->priv->blocking ? -1 : 0,
                                                 cancellable, error);
}

/**
 * g_socket_get_credentials:
 * @socket: a #GSocket.
 * @error: #GError for error reporting, or %NULL to ignore.
 *
 * Returns the credentials of the foreign process connected to this
 * socket, if any (e.g. it is only supported for %G_SOCKET_FAMILY_UNIX
 * sockets).
 *
 * If this operation isn't supported on the OS, the method fails with
 * the %G_IO_ERROR_NOT_SUPPORTED error. On Linux this is implemented
 * by reading the %SO_PEERCRED option on the underlying socket.
 *
 * Other ways to obtain credentials from a foreign peer includes the
 * #GUnixCredentialsMessage type and
 * g_unix_connection_send_credentials() /
 * g_unix_connection_receive_credentials() functions.
 *
 * Returns: (transfer full): %NULL if @error is set, otherwise a #GCredentials object
 * that must be freed with g_object_unref().
 *
 * Since: 2.26
 */
GCredentials *
g_socket_get_credentials (GSocket   *socket,
                          GError   **error)
{
  GCredentials *ret;

  g_return_val_if_fail (G_IS_SOCKET (socket), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  ret = NULL;

#if G_CREDENTIALS_SOCKET_GET_CREDENTIALS_SUPPORTED

#ifdef SO_PEERCRED
  {
    guint8 native_creds_buf[G_CREDENTIALS_NATIVE_SIZE];
    socklen_t optlen = sizeof (native_creds_buf);

    if (getsockopt (socket->priv->fd,
                    SOL_SOCKET,
                    SO_PEERCRED,
                    native_creds_buf,
                    &optlen) == 0)
      {
        ret = g_credentials_new ();
        g_credentials_set_native (ret,
                                  G_CREDENTIALS_NATIVE_TYPE,
                                  native_creds_buf);
      }
  }
#elif G_CREDENTIALS_USE_NETBSD_UNPCBID
  {
    struct unpcbid cred;
    socklen_t optlen = sizeof (cred);

    if (getsockopt (socket->priv->fd,
                    0,
                    LOCAL_PEEREID,
                    &cred,
                    &optlen) == 0)
      {
        ret = g_credentials_new ();
        g_credentials_set_native (ret,
                                  G_CREDENTIALS_NATIVE_TYPE,
                                  &cred);
      }
  }
#elif G_CREDENTIALS_USE_SOLARIS_UCRED
  {
    ucred_t *ucred = NULL;

    if (getpeerucred (socket->priv->fd, &ucred) == 0)
      {
        ret = g_credentials_new ();
        g_credentials_set_native (ret,
                                  G_CREDENTIALS_TYPE_SOLARIS_UCRED,
                                  ucred);
        ucred_free (ucred);
      }
  }
#else
  #error "G_CREDENTIALS_SOCKET_GET_CREDENTIALS_SUPPORTED is set but this is no code for this platform"
#endif

  if (!ret)
    {
      int errsv = get_socket_errno ();

      g_set_error (error,
                   G_IO_ERROR,
                   socket_io_error_from_errno (errsv),
                   _("Unable to read socket credentials: %s"),
                   socket_strerror (errsv));
    }

#else

  g_set_error_literal (error,
                       G_IO_ERROR,
                       G_IO_ERROR_NOT_SUPPORTED,
                       _("g_socket_get_credentials not implemented for this OS"));
#endif

  return ret;
}

/**
 * g_socket_get_option:
 * @socket: a #GSocket
 * @level: the "API level" of the option (eg, `SOL_SOCKET`)
 * @optname: the "name" of the option (eg, `SO_BROADCAST`)
 * @value: (out): return location for the option value
 * @error: #GError for error reporting, or %NULL to ignore.
 *
 * Gets the value of an integer-valued option on @socket, as with
 * getsockopt(). (If you need to fetch a  non-integer-valued option,
 * you will need to call getsockopt() directly.)
 *
 * The [<gio/gnetworking.h>][gio-gnetworking.h]
 * header pulls in system headers that will define most of the
 * standard/portable socket options. For unusual socket protocols or
 * platform-dependent options, you may need to include additional
 * headers.
 *
 * Note that even for socket options that are a single byte in size,
 * @value is still a pointer to a #gint variable, not a #guchar;
 * g_socket_get_option() will handle the conversion internally.
 *
 * Returns: success or failure. On failure, @error will be set, and
 *   the system error value (`errno` or WSAGetLastError()) will still
 *   be set to the result of the getsockopt() call.
 *
 * Since: 2.36
 */
gboolean
g_socket_get_option (GSocket  *socket,
		     gint      level,
		     gint      optname,
		     gint     *value,
		     GError  **error)
{
  guint size;

  g_return_val_if_fail (G_IS_SOCKET (socket), FALSE);

  *value = 0;
  size = sizeof (gint);
  if (getsockopt (socket->priv->fd, level, optname, value, &size) != 0)
    {
      int errsv = get_socket_errno ();

      g_set_error_literal (error,
			   G_IO_ERROR,
			   socket_io_error_from_errno (errsv),
			   socket_strerror (errsv));
#ifndef G_OS_WIN32
      /* Reset errno in case the caller wants to look at it */
      errno = errsv;
#endif
      return FALSE;
    }

#if G_BYTE_ORDER == G_BIG_ENDIAN
  /* If the returned value is smaller than an int then we need to
   * slide it over into the low-order bytes of *value.
   */
  if (size != sizeof (gint))
    *value = *value >> (8 * (sizeof (gint) - size));
#endif

  return TRUE;
}

/**
 * g_socket_set_option:
 * @socket: a #GSocket
 * @level: the "API level" of the option (eg, `SOL_SOCKET`)
 * @optname: the "name" of the option (eg, `SO_BROADCAST`)
 * @value: the value to set the option to
 * @error: #GError for error reporting, or %NULL to ignore.
 *
 * Sets the value of an integer-valued option on @socket, as with
 * setsockopt(). (If you need to set a non-integer-valued option,
 * you will need to call setsockopt() directly.)
 *
 * The [<gio/gnetworking.h>][gio-gnetworking.h]
 * header pulls in system headers that will define most of the
 * standard/portable socket options. For unusual socket protocols or
 * platform-dependent options, you may need to include additional
 * headers.
 *
 * Returns: success or failure. On failure, @error will be set, and
 *   the system error value (`errno` or WSAGetLastError()) will still
 *   be set to the result of the setsockopt() call.
 *
 * Since: 2.36
 */
gboolean
g_socket_set_option (GSocket  *socket,
		     gint      level,
		     gint      optname,
		     gint      value,
		     GError  **error)
{
  gint errsv;

  g_return_val_if_fail (G_IS_SOCKET (socket), FALSE);

  if (setsockopt (socket->priv->fd, level, optname, &value, sizeof (gint)) == 0)
    return TRUE;

#if !defined (__linux__) && !defined (G_OS_WIN32)
  /* Linux and Windows let you set a single-byte value from an int,
   * but most other platforms don't.
   */
  if (errno == EINVAL && value >= SCHAR_MIN && value <= CHAR_MAX)
    {
#if G_BYTE_ORDER == G_BIG_ENDIAN
      value = value << (8 * (sizeof (gint) - 1));
#endif
      if (setsockopt (socket->priv->fd, level, optname, &value, 1) == 0)
        return TRUE;
    }
#endif

  errsv = get_socket_errno ();

  g_set_error_literal (error,
                       G_IO_ERROR,
                       socket_io_error_from_errno (errsv),
                       socket_strerror (errsv));
#ifndef G_OS_WIN32
  errno = errsv;
#endif
  return FALSE;
}

