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
#include <errno.h>
#include "gioerror.h"

#ifdef G_OS_WIN32
#include <winsock2.h>
#endif

/**
 * SECTION:gioerror
 * @short_description: Error helper functions
 * @include: gio/gio.h
 *
 * Contains helper functions for reporting errors to the user.
 **/

/**
 * g_io_error_quark:
 *
 * Gets the GIO Error Quark.
 *
 * Returns: a #GQuark.
 **/
G_DEFINE_QUARK (g-io-error-quark, g_io_error)

/**
 * g_io_error_from_errno:
 * @err_no: Error number as defined in errno.h.
 *
 * Converts errno.h error codes into GIO error codes. The fallback
 * value %G_IO_ERROR_FAILED is returned for error codes not currently
 * handled (but note that future GLib releases may return a more
 * specific value instead).
 *
 * As %errno is global and may be modified by intermediate function
 * calls, you should save its value as soon as the call which sets it
 * returns:
 * |[
 *   int saved_errno;
 *
 *   ret = read (blah);
 *   saved_errno = errno;
 *
 *   g_io_error_from_errno (saved_errno);
 * ]|
 *
 * Returns: #GIOErrorEnum value for the given errno.h error number.
 **/
GIOErrorEnum
g_io_error_from_errno (gint err_no)
{
  switch (err_no)
    {
#ifdef EEXIST
    case EEXIST:
      return G_IO_ERROR_EXISTS;
      break;
#endif

#ifdef EISDIR
    case EISDIR:
      return G_IO_ERROR_IS_DIRECTORY;
      break;
#endif

#ifdef EACCES
    case EACCES:
      return G_IO_ERROR_PERMISSION_DENIED;
      break;
#endif

#ifdef ENAMETOOLONG
    case ENAMETOOLONG:
      return G_IO_ERROR_FILENAME_TOO_LONG;
      break;
#endif

#ifdef ENOENT
    case ENOENT:
      return G_IO_ERROR_NOT_FOUND;
      break;
#endif

#ifdef ENOTDIR
    case ENOTDIR:
      return G_IO_ERROR_NOT_DIRECTORY;
      break;
#endif

#ifdef EROFS
    case EROFS:
      return G_IO_ERROR_READ_ONLY;
      break;
#endif

#ifdef ELOOP
    case ELOOP:
      return G_IO_ERROR_TOO_MANY_LINKS;
      break;
#endif

#ifdef ENOSPC
    case ENOSPC:
      return G_IO_ERROR_NO_SPACE;
      break;
#endif

#ifdef ENOMEM
    case ENOMEM:
      return G_IO_ERROR_NO_SPACE;
      break;
#endif
      
#ifdef EINVAL
    case EINVAL:
      return G_IO_ERROR_INVALID_ARGUMENT;
      break;
#endif

#ifdef EPERM
    case EPERM:
      return G_IO_ERROR_PERMISSION_DENIED;
      break;
#endif

#ifdef ECANCELED
    case ECANCELED:
      return G_IO_ERROR_CANCELLED;
      break;
#endif

    /* ENOTEMPTY == EEXIST on AIX for backward compatibility reasons */
#if defined (ENOTEMPTY) && (!defined (EEXIST) || (ENOTEMPTY != EEXIST))
    case ENOTEMPTY:
      return G_IO_ERROR_NOT_EMPTY;
      break;
#endif

#ifdef ENOTSUP
    case ENOTSUP:
      return G_IO_ERROR_NOT_SUPPORTED;
      break;
#endif

    /* EOPNOTSUPP == ENOTSUP on Linux, but POSIX considers them distinct */
#if defined (EOPNOTSUPP) && (!defined (ENOTSUP) || (EOPNOTSUPP != ENOTSUP))
    case EOPNOTSUPP:
      return G_IO_ERROR_NOT_SUPPORTED;
      break;
#endif

#ifdef EPROTONOSUPPORT
    case EPROTONOSUPPORT:
      return G_IO_ERROR_NOT_SUPPORTED;
      break;
#endif

#ifdef ESOCKTNOSUPPORT
    case ESOCKTNOSUPPORT:
      return G_IO_ERROR_NOT_SUPPORTED;
      break;
#endif

#ifdef EPFNOSUPPORT
    case EPFNOSUPPORT:
      return G_IO_ERROR_NOT_SUPPORTED;
      break;
#endif

#ifdef EAFNOSUPPORT
    case EAFNOSUPPORT:
      return G_IO_ERROR_NOT_SUPPORTED;
      break;
#endif

#ifdef ETIMEDOUT
    case ETIMEDOUT:
      return G_IO_ERROR_TIMED_OUT;
      break;
#endif

#ifdef EBUSY
    case EBUSY:
      return G_IO_ERROR_BUSY;
      break;
#endif

#ifdef EWOULDBLOCK
    case EWOULDBLOCK:
      return G_IO_ERROR_WOULD_BLOCK;
      break;
#endif

    /* EWOULDBLOCK == EAGAIN on most systems, but POSIX considers them distinct */
#if defined (EAGAIN) && (!defined (EWOULDBLOCK) || (EWOULDBLOCK != EAGAIN))
    case EAGAIN:
      return G_IO_ERROR_WOULD_BLOCK;
      break;
#endif

#ifdef EMFILE
    case EMFILE:
      return G_IO_ERROR_TOO_MANY_OPEN_FILES;
      break;
#endif

#ifdef EADDRINUSE
    case EADDRINUSE:
      return G_IO_ERROR_ADDRESS_IN_USE;
      break;
#endif

#ifdef EHOSTUNREACH
    case EHOSTUNREACH:
      return G_IO_ERROR_HOST_UNREACHABLE;
      break;
#endif

#ifdef ENETUNREACH
    case ENETUNREACH:
      return G_IO_ERROR_NETWORK_UNREACHABLE;
      break;
#endif

#ifdef ECONNREFUSED
    case ECONNREFUSED:
      return G_IO_ERROR_CONNECTION_REFUSED;
      break;
#endif

#ifdef EPIPE
    case EPIPE:
      return G_IO_ERROR_BROKEN_PIPE;
      break;
#endif

#ifdef ECONNRESET
    case ECONNRESET:
      return G_IO_ERROR_CONNECTION_CLOSED;
      break;
#endif

#ifdef ENOTCONN
    case ENOTCONN:
      return G_IO_ERROR_NOT_CONNECTED;
      break;
#endif

#ifdef EMSGSIZE
    case EMSGSIZE:
      return G_IO_ERROR_MESSAGE_TOO_LARGE;
      break;
#endif

    default:
      return G_IO_ERROR_FAILED;
      break;
    }
}

#ifdef G_OS_WIN32

/**
 * g_io_error_from_win32_error:
 * @error_code: Windows error number.
 *
 * Converts some common error codes (as returned from GetLastError()
 * or WSAGetLastError()) into GIO error codes. The fallback value
 * %G_IO_ERROR_FAILED is returned for error codes not currently
 * handled (but note that future GLib releases may return a more
 * specific value instead).
 *
 * You can use g_win32_error_message() to get a localized string
 * corresponding to @error_code. (But note that unlike g_strerror(),
 * g_win32_error_message() returns a string that must be freed.)
 *
 * Returns: #GIOErrorEnum value for the given error number.
 *
 * Since: 2.26
 **/
GIOErrorEnum
g_io_error_from_win32_error (gint error_code)
{
  /* Note: Winsock errors are a subset of Win32 error codes as a
   * whole. (The fact that the Winsock API makes them look like they
   * aren't is just because the API predates Win32.)
   */

  switch (error_code)
    {
    case WSAEADDRINUSE:
      return G_IO_ERROR_ADDRESS_IN_USE;

    case WSAEWOULDBLOCK:
      return G_IO_ERROR_WOULD_BLOCK;

    case WSAEACCES:
      return G_IO_ERROR_PERMISSION_DENIED;

    case WSA_INVALID_HANDLE:
    case WSA_INVALID_PARAMETER:
    case WSAEINVAL:
    case WSAEBADF:
    case WSAENOTSOCK:
      return G_IO_ERROR_INVALID_ARGUMENT;

    case WSAEPROTONOSUPPORT:
      return G_IO_ERROR_NOT_SUPPORTED;

    case WSAECANCELLED:
      return G_IO_ERROR_CANCELLED;

    case WSAESOCKTNOSUPPORT:
    case WSAEOPNOTSUPP:
    case WSAEPFNOSUPPORT:
    case WSAEAFNOSUPPORT:
      return G_IO_ERROR_NOT_SUPPORTED;

    case WSAECONNRESET:
    case WSAESHUTDOWN:
      return G_IO_ERROR_CONNECTION_CLOSED;

    case WSAEHOSTUNREACH:
      return G_IO_ERROR_HOST_UNREACHABLE;

    case WSAENETUNREACH:
      return G_IO_ERROR_NETWORK_UNREACHABLE;

    case WSAECONNREFUSED:
      return G_IO_ERROR_CONNECTION_REFUSED;

    case WSAETIMEDOUT:
      return G_IO_ERROR_TIMED_OUT;

    case WSAENOTCONN:
    case ERROR_PIPE_LISTENING:
      return G_IO_ERROR_NOT_CONNECTED;

    case WSAEMSGSIZE:
      return G_IO_ERROR_MESSAGE_TOO_LARGE;

    default:
      return G_IO_ERROR_FAILED;
    }
}

#endif
