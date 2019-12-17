/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright (C) 2010 Collabora, Ltd.
 * Copyright (C) 2014 Red Hat, Inc.
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
 * Author:  Nicolas Dufresne <nicolas.dufresne@collabora.co.uk>
 *          Marc-André Lureau <marcandre.lureau@redhat.com>
 */

#include "config.h"

#include "ghttpproxy.h"

#include <string.h>
#include <stdlib.h>

#include "giomodule.h"
#include "giomodule-priv.h"
#include "giostream.h"
#include "ginputstream.h"
#include "glibintl.h"
#include "goutputstream.h"
#include "gproxy.h"
#include "gproxyaddress.h"
#include "gsocketconnectable.h"
#include "gtask.h"
#include "gtlsclientconnection.h"
#include "gtlsconnection.h"


struct _GHttpProxy
{
  GObject parent;
};

struct _GHttpProxyClass
{
  GObjectClass parent_class;
};

static void g_http_proxy_iface_init (GProxyInterface *proxy_iface);

#define g_http_proxy_get_type _g_http_proxy_get_type
G_DEFINE_TYPE_WITH_CODE (GHttpProxy, g_http_proxy, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_PROXY,
                                                g_http_proxy_iface_init)
                         _g_io_modules_ensure_extension_points_registered ();
                         g_io_extension_point_implement (G_PROXY_EXTENSION_POINT_NAME,
                                                         g_define_type_id,
                                                         "http",
                                                         0))

static void
g_http_proxy_init (GHttpProxy *proxy)
{
}

static gchar *
create_request (GProxyAddress  *proxy_address,
                gboolean       *has_cred,
                GError        **error)
{
  const gchar *hostname;
  gint port;
  const gchar *username;
  const gchar *password;
  GString *request;
  gchar *ascii_hostname;

  if (has_cred)
    *has_cred = FALSE;

  hostname = g_proxy_address_get_destination_hostname (proxy_address);
  ascii_hostname = g_hostname_to_ascii (hostname);
  if (!ascii_hostname)
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                           _("Invalid hostname"));
      return NULL;
    }
  port = g_proxy_address_get_destination_port (proxy_address);
  username = g_proxy_address_get_username (proxy_address);
  password = g_proxy_address_get_password (proxy_address);

  request = g_string_new (NULL);

  g_string_append_printf (request,
                          "CONNECT %s:%i HTTP/1.0\r\n"
                          "Host: %s:%i\r\n"
                          "Proxy-Connection: keep-alive\r\n"
                          "User-Agent: GLib/%i.%i\r\n",
                          ascii_hostname, port,
                          ascii_hostname, port,
                          GLIB_MAJOR_VERSION, GLIB_MINOR_VERSION);
  g_free (ascii_hostname);

  if (username != NULL && password != NULL)
    {
      gchar *cred;
      gchar *base64_cred;

      if (has_cred)
        *has_cred = TRUE;

      cred = g_strdup_printf ("%s:%s", username, password);
      base64_cred = g_base64_encode ((guchar *) cred, strlen (cred));
      g_free (cred);
      g_string_append_printf (request,
                              "Proxy-Authorization: Basic %s\r\n",
                              base64_cred);
      g_free (base64_cred);
    }

  g_string_append (request, "\r\n");

  return g_string_free (request, FALSE);
}

static gboolean
check_reply (const gchar  *buffer,
             gboolean      has_cred,
             GError      **error)
{
  gint err_code;
  const gchar *ptr = buffer + 7;

  if (strncmp (buffer, "HTTP/1.", 7) != 0 || (*ptr != '0' && *ptr != '1'))
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_PROXY_FAILED,
                           _("Bad HTTP proxy reply"));
      return FALSE;
    }

  ptr++;
  while (*ptr == ' ')
    ptr++;

  err_code = atoi (ptr);

  if (err_code < 200 || err_code >= 300)
    {
      switch (err_code)
        {
          case 403:
            g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_PROXY_NOT_ALLOWED,
                                 _("HTTP proxy connection not allowed"));
            break;
          case 407:
            if (has_cred)
              g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_PROXY_AUTH_FAILED,
                                   _("HTTP proxy authentication failed"));
            else
              g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_PROXY_NEED_AUTH,
                                   _("HTTP proxy authentication required"));
            break;
          default:
            g_set_error (error, G_IO_ERROR, G_IO_ERROR_PROXY_FAILED,
                         _("HTTP proxy connection failed: %i"), err_code);
        }

      return FALSE;
    }

  return TRUE;
}

#define HTTP_END_MARKER "\r\n\r\n"

static GIOStream *
g_http_proxy_connect (GProxy         *proxy,
                      GIOStream      *io_stream,
                      GProxyAddress  *proxy_address,
                      GCancellable   *cancellable,
                      GError        **error)
{
  GInputStream *in;
  GOutputStream *out;
  gchar *buffer = NULL;
  gsize buffer_length;
  gssize bytes_read;
  gboolean has_cred;
  GIOStream *tlsconn = NULL;

  if (G_IS_HTTPS_PROXY (proxy))
    {
      tlsconn = g_tls_client_connection_new (io_stream,
                                             G_SOCKET_CONNECTABLE (proxy_address),
                                             error);
      if (!tlsconn)
        goto error;

#ifdef DEBUG
      {
        GTlsCertificateFlags tls_validation_flags = G_TLS_CERTIFICATE_VALIDATE_ALL;

        tls_validation_flags &= ~(G_TLS_CERTIFICATE_UNKNOWN_CA | G_TLS_CERTIFICATE_BAD_IDENTITY);
        g_tls_client_connection_set_validation_flags (G_TLS_CLIENT_CONNECTION (tlsconn),
                                                      tls_validation_flags);
      }
#endif

      if (!g_tls_connection_handshake (G_TLS_CONNECTION (tlsconn), cancellable, error))
        goto error;

      io_stream = tlsconn;
    }

  in = g_io_stream_get_input_stream (io_stream);
  out = g_io_stream_get_output_stream (io_stream);

  buffer = create_request (proxy_address, &has_cred, error);
  if (!buffer)
    goto error;
  if (!g_output_stream_write_all (out, buffer, strlen (buffer), NULL,
                                  cancellable, error))
    goto error;

  g_free (buffer);

  bytes_read = 0;
  buffer_length = 1024;
  buffer = g_malloc (buffer_length);

  /* Read byte-by-byte instead of using GDataInputStream
   * since we do not want to read beyond the end marker
   */
  do
    {
      gsize nread;

      nread = g_input_stream_read (in, buffer + bytes_read, 1, cancellable, error);
      if (nread == -1)
        goto error;

      if (nread == 0)
        break;

      ++bytes_read;

      if (bytes_read == buffer_length)
        {
          buffer_length = 2 * buffer_length;
          buffer = g_realloc (buffer, buffer_length);
        }

      *(buffer + bytes_read) = '\0';

      if (g_str_has_suffix (buffer, HTTP_END_MARKER))
        break;
    }
  while (TRUE);

  if (bytes_read == 0)
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_PROXY_FAILED,
                           _("HTTP proxy server closed connection unexpectedly."));
      goto error;
    }

  if (!check_reply (buffer, has_cred, error))
    goto error;

  g_free (buffer);

  g_object_ref (io_stream);
  g_clear_object (&tlsconn);

  return io_stream;

error:
  g_clear_object (&tlsconn);
  g_free (buffer);
  return NULL;
}

typedef struct
{
  GIOStream *io_stream;
  GProxyAddress *proxy_address;
} ConnectAsyncData;

static void
free_connect_data (ConnectAsyncData *data)
{
  g_object_unref (data->io_stream);
  g_object_unref (data->proxy_address);
  g_slice_free (ConnectAsyncData, data);
}

static void
connect_thread (GTask        *task,
                gpointer      source_object,
                gpointer      task_data,
                GCancellable *cancellable)
{
  GProxy *proxy = source_object;
  ConnectAsyncData *data = task_data;
  GIOStream *res;
  GError *error = NULL;

  res = g_http_proxy_connect (proxy, data->io_stream, data->proxy_address,
                              cancellable, &error);

  if (res == NULL)
    g_task_return_error (task, error);
  else
    g_task_return_pointer (task, res, g_object_unref);
}

static void
g_http_proxy_connect_async (GProxy              *proxy,
                            GIOStream           *io_stream,
                            GProxyAddress       *proxy_address,
                            GCancellable        *cancellable,
                            GAsyncReadyCallback  callback,
                            gpointer             user_data)
{
  ConnectAsyncData *data;
  GTask *task;

  data = g_slice_new0 (ConnectAsyncData);
  data->io_stream = g_object_ref (io_stream);
  data->proxy_address = g_object_ref (proxy_address);

  task = g_task_new (proxy, cancellable, callback, user_data);
  g_task_set_source_tag (task, g_http_proxy_connect_async);
  g_task_set_task_data (task, data, (GDestroyNotify) free_connect_data);

  g_task_run_in_thread (task, connect_thread);
  g_object_unref (task);
}

static GIOStream *
g_http_proxy_connect_finish (GProxy        *proxy,
                             GAsyncResult  *result,
                             GError       **error)
{
  return g_task_propagate_pointer (G_TASK (result), error);
}

static gboolean
g_http_proxy_supports_hostname (GProxy *proxy)
{
  return TRUE;
}

static void
g_http_proxy_class_init (GHttpProxyClass *class)
{
}

static void
g_http_proxy_iface_init (GProxyInterface *proxy_iface)
{
  proxy_iface->connect = g_http_proxy_connect;
  proxy_iface->connect_async = g_http_proxy_connect_async;
  proxy_iface->connect_finish = g_http_proxy_connect_finish;
  proxy_iface->supports_hostname = g_http_proxy_supports_hostname;
}

struct _GHttpsProxy
{
  GHttpProxy parent;
};

struct _GHttpsProxyClass
{
  GHttpProxyClass parent_class;
};

#define g_https_proxy_get_type _g_https_proxy_get_type
G_DEFINE_TYPE_WITH_CODE (GHttpsProxy, g_https_proxy, G_TYPE_HTTP_PROXY,
                         G_IMPLEMENT_INTERFACE (G_TYPE_PROXY,
                                                g_http_proxy_iface_init)
                         _g_io_modules_ensure_extension_points_registered ();
                         g_io_extension_point_implement (G_PROXY_EXTENSION_POINT_NAME,
                                                         g_define_type_id,
                                                         "https",
                                                         0))

static void
g_https_proxy_init (GHttpsProxy *proxy)
{
}

static void
g_https_proxy_class_init (GHttpsProxyClass *class)
{
}
