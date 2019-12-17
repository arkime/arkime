/* GDBus - GLib D-Bus Library
 *
 * Copyright (C) 2008-2010 Red Hat, Inc.
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
 * Author: David Zeuthen <davidz@redhat.com>
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include "gioerror.h"
#include "gdbusutils.h"
#include "gdbusaddress.h"
#include "gdbuserror.h"
#include "gioenumtypes.h"
#include "gnetworkaddress.h"
#include "gsocketclient.h"
#include "giostream.h"
#include "gasyncresult.h"
#include "gtask.h"
#include "glib-private.h"
#include "gdbusprivate.h"
#include "giomodule-priv.h"
#include "gdbusdaemon.h"
#include "gstdio.h"

#ifdef G_OS_UNIX
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <gio/gunixsocketaddress.h>
#endif

#ifdef G_OS_WIN32
#include <windows.h>
#include <io.h>
#include <conio.h>
#endif

#include "glibintl.h"

/**
 * SECTION:gdbusaddress
 * @title: D-Bus Addresses
 * @short_description: D-Bus connection endpoints
 * @include: gio/gio.h
 *
 * Routines for working with D-Bus addresses. A D-Bus address is a string
 * like `unix:tmpdir=/tmp/my-app-name`. The exact format of addresses
 * is explained in detail in the
 * [D-Bus specification](http://dbus.freedesktop.org/doc/dbus-specification.html#addresses).
 *
 * TCP D-Bus connections are supported, but accessing them via a proxy is
 * currently not supported.
 */

static gchar *get_session_address_platform_specific (GError **error);
static gchar *get_session_address_dbus_launch       (GError **error);

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_dbus_is_address:
 * @string: A string.
 *
 * Checks if @string is a
 * [D-Bus address](https://dbus.freedesktop.org/doc/dbus-specification.html#addresses).
 *
 * This doesn't check if @string is actually supported by #GDBusServer
 * or #GDBusConnection - use g_dbus_is_supported_address() to do more
 * checks.
 *
 * Returns: %TRUE if @string is a valid D-Bus address, %FALSE otherwise.
 *
 * Since: 2.26
 */
gboolean
g_dbus_is_address (const gchar *string)
{
  guint n;
  gchar **a;
  gboolean ret;

  ret = FALSE;

  g_return_val_if_fail (string != NULL, FALSE);

  a = g_strsplit (string, ";", 0);
  if (a[0] == NULL)
    goto out;

  for (n = 0; a[n] != NULL; n++)
    {
      if (!_g_dbus_address_parse_entry (a[n],
                                        NULL,
                                        NULL,
                                        NULL))
        goto out;
    }

  ret = TRUE;

 out:
  g_strfreev (a);
  return ret;
}

static gboolean
is_valid_unix (const gchar  *address_entry,
               GHashTable   *key_value_pairs,
               GError      **error)
{
  gboolean ret;
  GList *keys;
  GList *l;
  const gchar *path;
  const gchar *tmpdir;
  const gchar *abstract;

  ret = FALSE;
  keys = NULL;
  path = NULL;
  tmpdir = NULL;
  abstract = NULL;

  keys = g_hash_table_get_keys (key_value_pairs);
  for (l = keys; l != NULL; l = l->next)
    {
      const gchar *key = l->data;
      if (g_strcmp0 (key, "path") == 0)
        path = g_hash_table_lookup (key_value_pairs, key);
      else if (g_strcmp0 (key, "tmpdir") == 0)
        tmpdir = g_hash_table_lookup (key_value_pairs, key);
      else if (g_strcmp0 (key, "abstract") == 0)
        abstract = g_hash_table_lookup (key_value_pairs, key);
      else
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_INVALID_ARGUMENT,
                       _("Unsupported key “%s” in address entry “%s”"),
                       key,
                       address_entry);
          goto out;
        }
    }

  if (path != NULL)
    {
      if (tmpdir != NULL || abstract != NULL)
        goto meaningless;
    }
  else if (tmpdir != NULL)
    {
      if (path != NULL || abstract != NULL)
        goto meaningless;
    }
  else if (abstract != NULL)
    {
      if (path != NULL || tmpdir != NULL)
        goto meaningless;
    }
  else
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_ARGUMENT,
                   _("Address “%s” is invalid (need exactly one of path, tmpdir or abstract keys)"),
                   address_entry);
      goto out;
    }


  ret= TRUE;
  goto out;

 meaningless:
  g_set_error (error,
               G_IO_ERROR,
               G_IO_ERROR_INVALID_ARGUMENT,
               _("Meaningless key/value pair combination in address entry “%s”"),
               address_entry);

 out:
  g_list_free (keys);

  return ret;
}

static gboolean
is_valid_nonce_tcp (const gchar  *address_entry,
                    GHashTable   *key_value_pairs,
                    GError      **error)
{
  gboolean ret;
  GList *keys;
  GList *l;
  const gchar *host;
  const gchar *port;
  const gchar *family;
  const gchar *nonce_file;
  gint port_num;
  gchar *endp;

  ret = FALSE;
  keys = NULL;
  host = NULL;
  port = NULL;
  family = NULL;
  nonce_file = NULL;

  keys = g_hash_table_get_keys (key_value_pairs);
  for (l = keys; l != NULL; l = l->next)
    {
      const gchar *key = l->data;
      if (g_strcmp0 (key, "host") == 0)
        host = g_hash_table_lookup (key_value_pairs, key);
      else if (g_strcmp0 (key, "port") == 0)
        port = g_hash_table_lookup (key_value_pairs, key);
      else if (g_strcmp0 (key, "family") == 0)
        family = g_hash_table_lookup (key_value_pairs, key);
      else if (g_strcmp0 (key, "noncefile") == 0)
        nonce_file = g_hash_table_lookup (key_value_pairs, key);
      else
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_INVALID_ARGUMENT,
                       _("Unsupported key “%s” in address entry “%s”"),
                       key,
                       address_entry);
          goto out;
        }
    }

  if (port != NULL)
    {
      port_num = strtol (port, &endp, 10);
      if ((*port == '\0' || *endp != '\0') || port_num < 0 || port_num >= 65536)
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_INVALID_ARGUMENT,
                       _("Error in address “%s” — the port attribute is malformed"),
                       address_entry);
          goto out;
        }
    }

  if (family != NULL && !(g_strcmp0 (family, "ipv4") == 0 || g_strcmp0 (family, "ipv6") == 0))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_ARGUMENT,
                   _("Error in address “%s” — the family attribute is malformed"),
                   address_entry);
      goto out;
    }

  if (host != NULL)
    {
      /* TODO: validate host */
    }

  nonce_file = nonce_file; /* To avoid -Wunused-but-set-variable */

  ret= TRUE;

 out:
  g_list_free (keys);

  return ret;
}

static gboolean
is_valid_tcp (const gchar  *address_entry,
              GHashTable   *key_value_pairs,
              GError      **error)
{
  gboolean ret;
  GList *keys;
  GList *l;
  const gchar *host;
  const gchar *port;
  const gchar *family;
  gint port_num;
  gchar *endp;

  ret = FALSE;
  keys = NULL;
  host = NULL;
  port = NULL;
  family = NULL;

  keys = g_hash_table_get_keys (key_value_pairs);
  for (l = keys; l != NULL; l = l->next)
    {
      const gchar *key = l->data;
      if (g_strcmp0 (key, "host") == 0)
        host = g_hash_table_lookup (key_value_pairs, key);
      else if (g_strcmp0 (key, "port") == 0)
        port = g_hash_table_lookup (key_value_pairs, key);
      else if (g_strcmp0 (key, "family") == 0)
        family = g_hash_table_lookup (key_value_pairs, key);
      else
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_INVALID_ARGUMENT,
                       _("Unsupported key “%s” in address entry “%s”"),
                       key,
                       address_entry);
          goto out;
        }
    }

  if (port != NULL)
    {
      port_num = strtol (port, &endp, 10);
      if ((*port == '\0' || *endp != '\0') || port_num < 0 || port_num >= 65536)
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_INVALID_ARGUMENT,
                       _("Error in address “%s” — the port attribute is malformed"),
                       address_entry);
          goto out;
        }
    }

  if (family != NULL && !(g_strcmp0 (family, "ipv4") == 0 || g_strcmp0 (family, "ipv6") == 0))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_ARGUMENT,
                   _("Error in address “%s” — the family attribute is malformed"),
                   address_entry);
      goto out;
    }

  if (host != NULL)
    {
      /* TODO: validate host */
    }

  ret= TRUE;

 out:
  g_list_free (keys);

  return ret;
}

/**
 * g_dbus_is_supported_address:
 * @string: A string.
 * @error: Return location for error or %NULL.
 *
 * Like g_dbus_is_address() but also checks if the library supports the
 * transports in @string and that key/value pairs for each transport
 * are valid. See the specification of the
 * [D-Bus address format](https://dbus.freedesktop.org/doc/dbus-specification.html#addresses).
 *
 * Returns: %TRUE if @string is a valid D-Bus address that is
 * supported by this library, %FALSE if @error is set.
 *
 * Since: 2.26
 */
gboolean
g_dbus_is_supported_address (const gchar  *string,
                             GError      **error)
{
  guint n;
  gchar **a;
  gboolean ret;

  ret = FALSE;

  g_return_val_if_fail (string != NULL, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  a = g_strsplit (string, ";", 0);
  for (n = 0; a[n] != NULL; n++)
    {
      gchar *transport_name;
      GHashTable *key_value_pairs;
      gboolean supported;

      if (!_g_dbus_address_parse_entry (a[n],
                                        &transport_name,
                                        &key_value_pairs,
                                        error))
        goto out;

      supported = FALSE;
      if (g_strcmp0 (transport_name, "unix") == 0)
        supported = is_valid_unix (a[n], key_value_pairs, error);
      else if (g_strcmp0 (transport_name, "tcp") == 0)
        supported = is_valid_tcp (a[n], key_value_pairs, error);
      else if (g_strcmp0 (transport_name, "nonce-tcp") == 0)
        supported = is_valid_nonce_tcp (a[n], key_value_pairs, error);
      else if (g_strcmp0 (a[n], "autolaunch:") == 0)
        supported = TRUE;
      else
        g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                     _("Unknown or unsupported transport “%s” for address “%s”"),
                     transport_name, a[n]);

      g_free (transport_name);
      g_hash_table_unref (key_value_pairs);

      if (!supported)
        goto out;
    }

  ret = TRUE;

 out:
  g_strfreev (a);

  g_assert (ret || (!ret && (error == NULL || *error != NULL)));

  return ret;
}

gboolean
_g_dbus_address_parse_entry (const gchar  *address_entry,
                             gchar       **out_transport_name,
                             GHashTable  **out_key_value_pairs,
                             GError      **error)
{
  gboolean ret;
  GHashTable *key_value_pairs;
  gchar *transport_name;
  gchar **kv_pairs;
  const gchar *s;
  guint n;

  ret = FALSE;
  kv_pairs = NULL;
  transport_name = NULL;
  key_value_pairs = NULL;

  s = strchr (address_entry, ':');
  if (s == NULL)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_ARGUMENT,
                   _("Address element “%s” does not contain a colon (:)"),
                   address_entry);
      goto out;
    }

  transport_name = g_strndup (address_entry, s - address_entry);
  key_value_pairs = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

  kv_pairs = g_strsplit (s + 1, ",", 0);
  for (n = 0; kv_pairs != NULL && kv_pairs[n] != NULL; n++)
    {
      const gchar *kv_pair = kv_pairs[n];
      gchar *key;
      gchar *value;

      s = strchr (kv_pair, '=');
      if (s == NULL)
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_INVALID_ARGUMENT,
                       _("Key/Value pair %d, “%s”, in address element “%s” does not contain an equal sign"),
                       n,
                       kv_pair,
                       address_entry);
          goto out;
        }

      key = g_uri_unescape_segment (kv_pair, s, NULL);
      value = g_uri_unescape_segment (s + 1, kv_pair + strlen (kv_pair), NULL);
      if (key == NULL || value == NULL)
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_INVALID_ARGUMENT,
                       _("Error unescaping key or value in Key/Value pair %d, “%s”, in address element “%s”"),
                       n,
                       kv_pair,
                       address_entry);
          g_free (key);
          g_free (value);
          goto out;
        }
      g_hash_table_insert (key_value_pairs, key, value);
    }

  ret = TRUE;

out:
  g_strfreev (kv_pairs);
  if (ret)
    {
      if (out_transport_name != NULL)
        *out_transport_name = transport_name;
      else
        g_free (transport_name);
      if (out_key_value_pairs != NULL)
        *out_key_value_pairs = key_value_pairs;
      else if (key_value_pairs != NULL)
        g_hash_table_unref (key_value_pairs);
    }
  else
    {
      g_free (transport_name);
      if (key_value_pairs != NULL)
        g_hash_table_unref (key_value_pairs);
    }
  return ret;
}

/* ---------------------------------------------------------------------------------------------------- */

static GIOStream *
g_dbus_address_try_connect_one (const gchar   *address_entry,
                                gchar        **out_guid,
                                GCancellable  *cancellable,
                                GError       **error);

/* TODO: Declare an extension point called GDBusTransport (or similar)
 * and move code below to extensions implementing said extension
 * point. That way we can implement a D-Bus transport over X11 without
 * making libgio link to libX11...
 */
static GIOStream *
g_dbus_address_connect (const gchar   *address_entry,
                        const gchar   *transport_name,
                        GHashTable    *key_value_pairs,
                        GCancellable  *cancellable,
                        GError       **error)
{
  GIOStream *ret;
  GSocketConnectable *connectable;
  const gchar *nonce_file;

  connectable = NULL;
  ret = NULL;
  nonce_file = NULL;

  if (FALSE)
    {
    }
#ifdef G_OS_UNIX
  else if (g_strcmp0 (transport_name, "unix") == 0)
    {
      const gchar *path;
      const gchar *abstract;
      path = g_hash_table_lookup (key_value_pairs, "path");
      abstract = g_hash_table_lookup (key_value_pairs, "abstract");
      if ((path == NULL && abstract == NULL) || (path != NULL && abstract != NULL))
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_INVALID_ARGUMENT,
                       _("Error in address “%s” — the unix transport requires exactly one of the "
                         "keys “path” or “abstract” to be set"),
                       address_entry);
        }
      else if (path != NULL)
        {
          connectable = G_SOCKET_CONNECTABLE (g_unix_socket_address_new (path));
        }
      else if (abstract != NULL)
        {
          connectable = G_SOCKET_CONNECTABLE (g_unix_socket_address_new_with_type (abstract,
                                                                                   -1,
                                                                                   G_UNIX_SOCKET_ADDRESS_ABSTRACT));
        }
      else
        {
          g_assert_not_reached ();
        }
    }
#endif
  else if (g_strcmp0 (transport_name, "tcp") == 0 || g_strcmp0 (transport_name, "nonce-tcp") == 0)
    {
      const gchar *s;
      const gchar *host;
      glong port;
      gchar *endp;
      gboolean is_nonce;

      is_nonce = (g_strcmp0 (transport_name, "nonce-tcp") == 0);

      host = g_hash_table_lookup (key_value_pairs, "host");
      if (host == NULL)
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_INVALID_ARGUMENT,
                       _("Error in address “%s” — the host attribute is missing or malformed"),
                       address_entry);
          goto out;
        }

      s = g_hash_table_lookup (key_value_pairs, "port");
      if (s == NULL)
        s = "0";
      port = strtol (s, &endp, 10);
      if ((*s == '\0' || *endp != '\0') || port < 0 || port >= 65536)
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_INVALID_ARGUMENT,
                       _("Error in address “%s” — the port attribute is missing or malformed"),
                       address_entry);
          goto out;
        }


      if (is_nonce)
        {
          nonce_file = g_hash_table_lookup (key_value_pairs, "noncefile");
          if (nonce_file == NULL)
            {
              g_set_error (error,
                           G_IO_ERROR,
                           G_IO_ERROR_INVALID_ARGUMENT,
                           _("Error in address “%s” — the noncefile attribute is missing or malformed"),
                           address_entry);
              goto out;
            }
        }

      /* TODO: deal with family key/value-pair */
      connectable = g_network_address_new (host, port);
    }
  else if (g_strcmp0 (address_entry, "autolaunch:") == 0)
    {
      gchar *autolaunch_address;
      autolaunch_address = get_session_address_dbus_launch (error);
      if (autolaunch_address != NULL)
        {
          ret = g_dbus_address_try_connect_one (autolaunch_address, NULL, cancellable, error);
          g_free (autolaunch_address);
          goto out;
        }
      else
        {
          g_prefix_error (error, _("Error auto-launching: "));
        }
    }
  else
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_ARGUMENT,
                   _("Unknown or unsupported transport “%s” for address “%s”"),
                   transport_name,
                   address_entry);
    }

  if (connectable != NULL)
    {
      GSocketClient *client;
      GSocketConnection *connection;

      g_assert (ret == NULL);
      client = g_socket_client_new ();

      /* Disable proxy support to prevent a deadlock on startup, since loading a
       * proxy resolver causes the GIO modules to be loaded, and there will
       * almost certainly be one of them which then tries to use GDBus.
       * See: https://bugzilla.gnome.org/show_bug.cgi?id=792499 */
      g_socket_client_set_enable_proxy (client, FALSE);

      connection = g_socket_client_connect (client,
                                            connectable,
                                            cancellable,
                                            error);
      g_object_unref (connectable);
      g_object_unref (client);
      if (connection == NULL)
        goto out;

      ret = G_IO_STREAM (connection);

      if (nonce_file != NULL)
        {
          gchar nonce_contents[16 + 1];
          size_t num_bytes_read;
          FILE *f;
          int errsv;

          /* be careful to read only 16 bytes - we also check that the file is only 16 bytes long */
          f = fopen (nonce_file, "rb");
          errsv = errno;
          if (f == NULL)
            {
              g_set_error (error,
                           G_IO_ERROR,
                           G_IO_ERROR_INVALID_ARGUMENT,
                           _("Error opening nonce file “%s”: %s"),
                           nonce_file,
                           g_strerror (errsv));
              g_object_unref (ret);
              ret = NULL;
              goto out;
            }
          num_bytes_read = fread (nonce_contents,
                                  sizeof (gchar),
                                  16 + 1,
                                  f);
          errsv = errno;
          if (num_bytes_read != 16)
            {
              if (num_bytes_read == 0)
                {
                  g_set_error (error,
                               G_IO_ERROR,
                               G_IO_ERROR_INVALID_ARGUMENT,
                               _("Error reading from nonce file “%s”: %s"),
                               nonce_file,
                               g_strerror (errsv));
                }
              else
                {
                  g_set_error (error,
                               G_IO_ERROR,
                               G_IO_ERROR_INVALID_ARGUMENT,
                               _("Error reading from nonce file “%s”, expected 16 bytes, got %d"),
                               nonce_file,
                               (gint) num_bytes_read);
                }
              g_object_unref (ret);
              ret = NULL;
              fclose (f);
              goto out;
            }
          fclose (f);

          if (!g_output_stream_write_all (g_io_stream_get_output_stream (ret),
                                          nonce_contents,
                                          16,
                                          NULL,
                                          cancellable,
                                          error))
            {
              g_prefix_error (error, _("Error writing contents of nonce file “%s” to stream:"), nonce_file);
              g_object_unref (ret);
              ret = NULL;
              goto out;
            }
        }
    }

 out:

  return ret;
}

static GIOStream *
g_dbus_address_try_connect_one (const gchar   *address_entry,
                                gchar        **out_guid,
                                GCancellable  *cancellable,
                                GError       **error)
{
  GIOStream *ret;
  GHashTable *key_value_pairs;
  gchar *transport_name;
  const gchar *guid;

  ret = NULL;
  transport_name = NULL;
  key_value_pairs = NULL;

  if (!_g_dbus_address_parse_entry (address_entry,
                                    &transport_name,
                                    &key_value_pairs,
                                    error))
    goto out;

  ret = g_dbus_address_connect (address_entry,
                                transport_name,
                                key_value_pairs,
                                cancellable,
                                error);
  if (ret == NULL)
    goto out;

  guid = g_hash_table_lookup (key_value_pairs, "guid");
  if (guid != NULL && out_guid != NULL)
    *out_guid = g_strdup (guid);

out:
  g_free (transport_name);
  if (key_value_pairs != NULL)
    g_hash_table_unref (key_value_pairs);
  return ret;
}


/* ---------------------------------------------------------------------------------------------------- */

typedef struct {
  gchar *address;
  gchar *guid;
} GetStreamData;

static void
get_stream_data_free (GetStreamData *data)
{
  g_free (data->address);
  g_free (data->guid);
  g_free (data);
}

static void
get_stream_thread_func (GTask         *task,
                        gpointer       source_object,
                        gpointer       task_data,
                        GCancellable  *cancellable)
{
  GetStreamData *data = task_data;
  GIOStream *stream;
  GError *error = NULL;

  stream = g_dbus_address_get_stream_sync (data->address,
                                           &data->guid,
                                           cancellable,
                                           &error);
  if (stream)
    g_task_return_pointer (task, stream, g_object_unref);
  else
    g_task_return_error (task, error);
}

/**
 * g_dbus_address_get_stream:
 * @address: A valid D-Bus address.
 * @cancellable: (nullable): A #GCancellable or %NULL.
 * @callback: A #GAsyncReadyCallback to call when the request is satisfied.
 * @user_data: Data to pass to @callback.
 *
 * Asynchronously connects to an endpoint specified by @address and
 * sets up the connection so it is in a state to run the client-side
 * of the D-Bus authentication conversation. @address must be in the
 * [D-Bus address format](https://dbus.freedesktop.org/doc/dbus-specification.html#addresses).
 *
 * When the operation is finished, @callback will be invoked. You can
 * then call g_dbus_address_get_stream_finish() to get the result of
 * the operation.
 *
 * This is an asynchronous failable function. See
 * g_dbus_address_get_stream_sync() for the synchronous version.
 *
 * Since: 2.26
 */
void
g_dbus_address_get_stream (const gchar         *address,
                           GCancellable        *cancellable,
                           GAsyncReadyCallback  callback,
                           gpointer             user_data)
{
  GTask *task;
  GetStreamData *data;

  g_return_if_fail (address != NULL);

  data = g_new0 (GetStreamData, 1);
  data->address = g_strdup (address);

  task = g_task_new (NULL, cancellable, callback, user_data);
  g_task_set_source_tag (task, g_dbus_address_get_stream);
  g_task_set_task_data (task, data, (GDestroyNotify) get_stream_data_free);
  g_task_run_in_thread (task, get_stream_thread_func);
  g_object_unref (task);
}

/**
 * g_dbus_address_get_stream_finish:
 * @res: A #GAsyncResult obtained from the GAsyncReadyCallback passed to g_dbus_address_get_stream().
 * @out_guid: (optional) (out): %NULL or return location to store the GUID extracted from @address, if any.
 * @error: Return location for error or %NULL.
 *
 * Finishes an operation started with g_dbus_address_get_stream().
 *
 * Returns: (transfer full): A #GIOStream or %NULL if @error is set.
 *
 * Since: 2.26
 */
GIOStream *
g_dbus_address_get_stream_finish (GAsyncResult        *res,
                                  gchar              **out_guid,
                                  GError             **error)
{
  GTask *task;
  GetStreamData *data;
  GIOStream *ret;

  g_return_val_if_fail (g_task_is_valid (res, NULL), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  task = G_TASK (res);
  ret = g_task_propagate_pointer (task, error);

  if (ret != NULL && out_guid != NULL)
    {
      data = g_task_get_task_data (task);
      *out_guid = data->guid;
      data->guid = NULL;
    }

  return ret;
}

/**
 * g_dbus_address_get_stream_sync:
 * @address: A valid D-Bus address.
 * @out_guid: (optional) (out): %NULL or return location to store the GUID extracted from @address, if any.
 * @cancellable: (nullable): A #GCancellable or %NULL.
 * @error: Return location for error or %NULL.
 *
 * Synchronously connects to an endpoint specified by @address and
 * sets up the connection so it is in a state to run the client-side
 * of the D-Bus authentication conversation. @address must be in the
 * [D-Bus address format](https://dbus.freedesktop.org/doc/dbus-specification.html#addresses).
 *
 * This is a synchronous failable function. See
 * g_dbus_address_get_stream() for the asynchronous version.
 *
 * Returns: (transfer full): A #GIOStream or %NULL if @error is set.
 *
 * Since: 2.26
 */
GIOStream *
g_dbus_address_get_stream_sync (const gchar   *address,
                                gchar        **out_guid,
                                GCancellable  *cancellable,
                                GError       **error)
{
  GIOStream *ret;
  gchar **addr_array;
  guint n;
  GError *last_error;

  g_return_val_if_fail (address != NULL, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  ret = NULL;
  last_error = NULL;

  addr_array = g_strsplit (address, ";", 0);
  if (addr_array != NULL && addr_array[0] == NULL)
    {
      last_error = g_error_new_literal (G_IO_ERROR,
                                        G_IO_ERROR_INVALID_ARGUMENT,
                                        _("The given address is empty"));
      goto out;
    }

  for (n = 0; addr_array != NULL && addr_array[n] != NULL; n++)
    {
      const gchar *addr = addr_array[n];
      GError *this_error;

      this_error = NULL;
      ret = g_dbus_address_try_connect_one (addr,
                                            out_guid,
                                            cancellable,
                                            &this_error);
      if (ret != NULL)
        {
          goto out;
        }
      else
        {
          g_assert (this_error != NULL);
          if (last_error != NULL)
            g_error_free (last_error);
          last_error = this_error;
        }
    }

 out:
  if (ret != NULL)
    {
      if (last_error != NULL)
        g_error_free (last_error);
    }
  else
    {
      g_assert (last_error != NULL);
      g_propagate_error (error, last_error);
    }

  g_strfreev (addr_array);
  return ret;
}

/* ---------------------------------------------------------------------------------------------------- */

/*
 * Return the address of XDG_RUNTIME_DIR/bus if it exists, belongs to
 * us, and is a socket, and we are on Unix.
 */
static gchar *
get_session_address_xdg (void)
{
#ifdef G_OS_UNIX
  gchar *ret = NULL;
  gchar *bus;
  gchar *tmp;
  GStatBuf buf;

  bus = g_build_filename (g_get_user_runtime_dir (), "bus", NULL);

  /* if ENOENT, EPERM, etc., quietly don't use it */
  if (g_stat (bus, &buf) < 0)
    goto out;

  /* if it isn't ours, we have incorrectly inherited someone else's
   * XDG_RUNTIME_DIR; silently don't use it
   */
  if (buf.st_uid != geteuid ())
    goto out;

  /* if it isn't a socket, silently don't use it */
  if ((buf.st_mode & S_IFMT) != S_IFSOCK)
    goto out;

  tmp = g_dbus_address_escape_value (bus);
  ret = g_strconcat ("unix:path=", tmp, NULL);
  g_free (tmp);

out:
  g_free (bus);
  return ret;
#else
  return NULL;
#endif
}

/* ---------------------------------------------------------------------------------------------------- */

#ifdef G_OS_UNIX
static gchar *
get_session_address_dbus_launch (GError **error)
{
  gchar *ret;
  gchar *machine_id;
  gchar *command_line;
  gchar *launch_stdout;
  gchar *launch_stderr;
  gint exit_status;
  gchar *old_dbus_verbose;
  gboolean restore_dbus_verbose;

  ret = NULL;
  machine_id = NULL;
  command_line = NULL;
  launch_stdout = NULL;
  launch_stderr = NULL;
  restore_dbus_verbose = FALSE;
  old_dbus_verbose = NULL;

  /* Don't run binaries as root if we're setuid. */
  if (GLIB_PRIVATE_CALL (g_check_setuid) ())
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
		   _("Cannot spawn a message bus when setuid"));
      goto out;
    }

  machine_id = _g_dbus_get_machine_id (error);
  if (machine_id == NULL)
    {
      g_prefix_error (error, _("Cannot spawn a message bus without a machine-id: "));
      goto out;
    }

  if (g_getenv ("DISPLAY") == NULL)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   _("Cannot autolaunch D-Bus without X11 $DISPLAY"));
      goto out;
    }

  /* We're using private libdbus facilities here. When everything
   * (X11, Mac OS X, Windows) is spec'ed out correctly (not even the
   * X11 property is correctly documented right now) we should
   * consider using the spec instead of dbus-launch.
   *
   *   --autolaunch=MACHINEID
   *          This option implies that dbus-launch should scan  for  a  previ‐
   *          ously-started  session  and  reuse the values found there. If no
   *          session is found, it will start a new session. The  --exit-with-
   *          session option is implied if --autolaunch is given.  This option
   *          is for the exclusive use of libdbus, you do not want to  use  it
   *          manually. It may change in the future.
   */

  /* TODO: maybe provide a variable for where to look for the dbus-launch binary? */
  command_line = g_strdup_printf ("dbus-launch --autolaunch=%s --binary-syntax --close-stderr", machine_id);

  if (G_UNLIKELY (_g_dbus_debug_address ()))
    {
      _g_dbus_debug_print_lock ();
      g_print ("GDBus-debug:Address: Running '%s' to get bus address (possibly autolaunching)\n", command_line);
      old_dbus_verbose = g_strdup (g_getenv ("DBUS_VERBOSE"));
      restore_dbus_verbose = TRUE;
      g_setenv ("DBUS_VERBOSE", "1", TRUE);
      _g_dbus_debug_print_unlock ();
    }

  if (!g_spawn_command_line_sync (command_line,
                                  &launch_stdout,
                                  &launch_stderr,
                                  &exit_status,
                                  error))
    {
      goto out;
    }

  if (!g_spawn_check_exit_status (exit_status, error))
    {
      g_prefix_error (error, _("Error spawning command line “%s”: "), command_line);
      goto out;
    }

  /* From the dbus-launch(1) man page:
   *
   *   --binary-syntax Write to stdout a nul-terminated bus address,
   *   then the bus PID as a binary integer of size sizeof(pid_t),
   *   then the bus X window ID as a binary integer of size
   *   sizeof(long).  Integers are in the machine's byte order, not
   *   network byte order or any other canonical byte order.
   */
  ret = g_strdup (launch_stdout);

 out:
  if (G_UNLIKELY (_g_dbus_debug_address ()))
    {
      gchar *s;
      _g_dbus_debug_print_lock ();
      g_print ("GDBus-debug:Address: dbus-launch output:");
      if (launch_stdout != NULL)
        {
          s = _g_dbus_hexdump (launch_stdout, strlen (launch_stdout) + 1 + sizeof (pid_t) + sizeof (long), 2);
          g_print ("\n%s", s);
          g_free (s);
        }
      else
        {
          g_print (" (none)\n");
        }
      g_print ("GDBus-debug:Address: dbus-launch stderr output:");
      if (launch_stderr != NULL)
        g_print ("\n%s", launch_stderr);
      else
        g_print (" (none)\n");
      _g_dbus_debug_print_unlock ();
    }

  g_free (machine_id);
  g_free (command_line);
  g_free (launch_stdout);
  g_free (launch_stderr);
  if (G_UNLIKELY (restore_dbus_verbose))
    {
      if (old_dbus_verbose != NULL)
        g_setenv ("DBUS_VERBOSE", old_dbus_verbose, TRUE);
      else
        g_unsetenv ("DBUS_VERBOSE");
    }
  g_free (old_dbus_verbose);
  return ret;
}

/* end of G_OS_UNIX case */
#elif defined(G_OS_WIN32)

#define DBUS_DAEMON_ADDRESS_INFO "DBusDaemonAddressInfo"
#define DBUS_DAEMON_MUTEX "DBusDaemonMutex"
#define UNIQUE_DBUS_INIT_MUTEX "UniqueDBusInitMutex"
#define DBUS_AUTOLAUNCH_MUTEX "DBusAutolaunchMutex"

static void
release_mutex (HANDLE mutex)
{
  ReleaseMutex (mutex);
  CloseHandle (mutex);
}

static HANDLE
acquire_mutex (const char *mutexname)
{
  HANDLE mutex;
  DWORD res;

  mutex = CreateMutexA (NULL, FALSE, mutexname);
  if (!mutex)
    return 0;

  res = WaitForSingleObject (mutex, INFINITE);
  switch (res)
    {
    case WAIT_ABANDONED:
      release_mutex (mutex);
      return 0;
    case WAIT_FAILED:
    case WAIT_TIMEOUT:
      return 0;
    }

  return mutex;
}

static gboolean
is_mutex_owned (const char *mutexname)
{
  HANDLE mutex;
  gboolean res = FALSE;

  mutex = CreateMutexA (NULL, FALSE, mutexname);
  if (WaitForSingleObject (mutex, 10) == WAIT_TIMEOUT)
    res = TRUE;
  else
    ReleaseMutex (mutex);
  CloseHandle (mutex);

  return res;
}

static char *
read_shm (const char *shm_name)
{
  HANDLE shared_mem;
  char *shared_data;
  char *res;
  int i;

  res = NULL;

  for (i = 0; i < 20; i++)
    {
      shared_mem = OpenFileMappingA (FILE_MAP_READ, FALSE, shm_name);
      if (shared_mem != 0)
	break;
      Sleep (100);
    }

  if (shared_mem != 0)
    {
      shared_data = MapViewOfFile (shared_mem, FILE_MAP_READ, 0, 0, 0);
      if (shared_data != NULL)
	{
	  res = g_strdup (shared_data);
	  UnmapViewOfFile (shared_data);
	}
      CloseHandle (shared_mem);
    }

  return res;
}

static HANDLE
set_shm (const char *shm_name, const char *value)
{
  HANDLE shared_mem;
  char *shared_data;

  shared_mem = CreateFileMappingA (INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
				   0, strlen (value) + 1, shm_name);
  if (shared_mem == 0)
    return 0;

  shared_data = MapViewOfFile (shared_mem, FILE_MAP_WRITE, 0, 0, 0 );
  if (shared_data == NULL)
    return 0;

  strcpy (shared_data, value);

  UnmapViewOfFile (shared_data);

  return shared_mem;
}

/* These keep state between publish_session_bus and unpublish_session_bus */
static HANDLE published_daemon_mutex;
static HANDLE published_shared_mem;

static gboolean
publish_session_bus (const char *address)
{
  HANDLE init_mutex;

  init_mutex = acquire_mutex (UNIQUE_DBUS_INIT_MUTEX);

  published_daemon_mutex = CreateMutexA (NULL, FALSE, DBUS_DAEMON_MUTEX);
  if (WaitForSingleObject (published_daemon_mutex, 10 ) != WAIT_OBJECT_0)
    {
      release_mutex (init_mutex);
      CloseHandle (published_daemon_mutex);
      published_daemon_mutex = NULL;
      return FALSE;
    }

  published_shared_mem = set_shm (DBUS_DAEMON_ADDRESS_INFO, address);
  if (!published_shared_mem)
    {
      release_mutex (init_mutex);
      CloseHandle (published_daemon_mutex);
      published_daemon_mutex = NULL;
      return FALSE;
    }

  release_mutex (init_mutex);
  return TRUE;
}

static void
unpublish_session_bus (void)
{
  HANDLE init_mutex;

  init_mutex = acquire_mutex (UNIQUE_DBUS_INIT_MUTEX);

  CloseHandle (published_shared_mem);
  published_shared_mem = NULL;

  release_mutex (published_daemon_mutex);
  published_daemon_mutex = NULL;

  release_mutex (init_mutex);
}

static void
wait_console_window (void)
{
  FILE *console = fopen ("CONOUT$", "w");

  SetConsoleTitleW (L"gdbus-daemon output. Type any character to close this window.");
  fprintf (console, _("(Type any character to close this window)\n"));
  fflush (console);
  _getch ();
}

static void
open_console_window (void)
{
  if (((HANDLE) _get_osfhandle (fileno (stdout)) == INVALID_HANDLE_VALUE ||
       (HANDLE) _get_osfhandle (fileno (stderr)) == INVALID_HANDLE_VALUE) && AllocConsole ())
    {
      if ((HANDLE) _get_osfhandle (fileno (stdout)) == INVALID_HANDLE_VALUE)
        freopen ("CONOUT$", "w", stdout);

      if ((HANDLE) _get_osfhandle (fileno (stderr)) == INVALID_HANDLE_VALUE)
        freopen ("CONOUT$", "w", stderr);

      SetConsoleTitleW (L"gdbus-daemon debug output.");

      atexit (wait_console_window);
    }
}

static void
idle_timeout_cb (GDBusDaemon *daemon, gpointer user_data)
{
  GMainLoop *loop = user_data;
  g_main_loop_quit (loop);
}

/* Satisfies STARTF_FORCEONFEEDBACK */
static void
turn_off_the_starting_cursor (void)
{
  MSG msg;
  BOOL bRet;

  PostQuitMessage (0);

  while ((bRet = GetMessage (&msg, 0, 0, 0)) != 0)
    {
      if (bRet == -1)
        continue;

      TranslateMessage (&msg);
      DispatchMessage (&msg);
    }
}

__declspec(dllexport) void CALLBACK g_win32_run_session_bus (HWND hwnd, HINSTANCE hinst, char *cmdline, int nCmdShow);

__declspec(dllexport) void CALLBACK
g_win32_run_session_bus (HWND hwnd, HINSTANCE hinst, char *cmdline, int nCmdShow)
{
  GDBusDaemon *daemon;
  GMainLoop *loop;
  const char *address;
  GError *error = NULL;

  turn_off_the_starting_cursor ();

  if (g_getenv ("GDBUS_DAEMON_DEBUG") != NULL)
    open_console_window ();

  loop = g_main_loop_new (NULL, FALSE);

  address = "nonce-tcp:";
  daemon = _g_dbus_daemon_new (address, NULL, &error);
  if (daemon == NULL)
    {
      g_printerr ("Can't init bus: %s\n", error->message);
      g_error_free (error);
      return;
    }

  g_signal_connect (daemon, "idle-timeout", G_CALLBACK (idle_timeout_cb), loop);

  if (publish_session_bus (_g_dbus_daemon_get_address (daemon)))
    {
      g_main_loop_run (loop);

      unpublish_session_bus ();
    }

  g_main_loop_unref (loop);
  g_object_unref (daemon);
}

static gchar *
get_session_address_dbus_launch (GError **error)
{
  HANDLE autolaunch_mutex, init_mutex;
  char *address = NULL;
  wchar_t gio_path[MAX_PATH+1+200];

  autolaunch_mutex = acquire_mutex (DBUS_AUTOLAUNCH_MUTEX);

  init_mutex = acquire_mutex (UNIQUE_DBUS_INIT_MUTEX);

  if (is_mutex_owned (DBUS_DAEMON_MUTEX))
    address = read_shm (DBUS_DAEMON_ADDRESS_INFO);

  release_mutex (init_mutex);

  if (address == NULL)
    {
      gio_path[MAX_PATH] = 0;
      if (GetModuleFileNameW (_g_io_win32_get_module (), gio_path, MAX_PATH))
	{
	  PROCESS_INFORMATION pi = { 0 };
	  STARTUPINFOW si = { 0 };
	  BOOL res;
	  wchar_t gio_path_short[MAX_PATH];
	  wchar_t rundll_path[MAX_PATH*2];
	  wchar_t args[MAX_PATH*4];

	  GetShortPathNameW (gio_path, gio_path_short, MAX_PATH);

	  GetWindowsDirectoryW (rundll_path, MAX_PATH);
	  wcscat (rundll_path, L"\\rundll32.exe");
	  if (GetFileAttributesW (rundll_path) == INVALID_FILE_ATTRIBUTES)
	    {
	      GetSystemDirectoryW (rundll_path, MAX_PATH);
	      wcscat (rundll_path, L"\\rundll32.exe");
	    }

	  wcscpy (args, L"\"");
	  wcscat (args, rundll_path);
	  wcscat (args, L"\" ");
	  wcscat (args, gio_path_short);
#if defined(_WIN64) || defined(_M_X64) || defined(_M_AMD64)
	  wcscat (args, L",g_win32_run_session_bus");
#elif defined (_MSC_VER)
	  wcscat (args, L",_g_win32_run_session_bus@16");
#else
	  wcscat (args, L",g_win32_run_session_bus@16");
#endif

	  res = CreateProcessW (rundll_path, args,
				0, 0, FALSE,
				NORMAL_PRIORITY_CLASS | CREATE_NO_WINDOW | DETACHED_PROCESS,
				0, NULL /* TODO: Should be root */,
				&si, &pi);
	  if (res)
	    address = read_shm (DBUS_DAEMON_ADDRESS_INFO);
	}
    }

  release_mutex (autolaunch_mutex);

  if (address == NULL)
    g_set_error (error,
		 G_IO_ERROR,
		 G_IO_ERROR_FAILED,
		 _("Session dbus not running, and autolaunch failed"));

  return address;
}
#else /* neither G_OS_UNIX nor G_OS_WIN32 */
static gchar *
get_session_address_dbus_launch (GError **error)
{
  g_set_error (error,
               G_IO_ERROR,
               G_IO_ERROR_FAILED,
               _("Cannot determine session bus address (not implemented for this OS)"));
  return NULL;
}
#endif /* neither G_OS_UNIX nor G_OS_WIN32 */

/* ---------------------------------------------------------------------------------------------------- */

static gchar *
get_session_address_platform_specific (GError **error)
{
  gchar *ret;

  /* Use XDG_RUNTIME_DIR/bus if it exists and is suitable. This is appropriate
   * for systems using the "a session is a user-session" model described in
   * <http://lists.freedesktop.org/archives/dbus/2015-January/016522.html>,
   * and implemented in dbus >= 1.9.14 and sd-bus.
   *
   * On systems following the more traditional "a session is a login-session"
   * model, this will fail and we'll fall through to X11 autolaunching
   * (dbus-launch) below.
   */
  ret = get_session_address_xdg ();

  if (ret != NULL)
    return ret;

  /* TODO (#694472): try launchd on OS X, like
   * _dbus_lookup_session_address_launchd() does, since
   * 'dbus-launch --autolaunch' probably won't work there
   */

  /* As a last resort, try the "autolaunch:" transport. On Unix this means
   * X11 autolaunching; on Windows this means a different autolaunching
   * mechanism based on shared memory.
   */
  return get_session_address_dbus_launch (error);
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_dbus_address_get_for_bus_sync:
 * @bus_type: a #GBusType
 * @cancellable: (nullable): a #GCancellable or %NULL
 * @error: return location for error or %NULL
 *
 * Synchronously looks up the D-Bus address for the well-known message
 * bus instance specified by @bus_type. This may involve using various
 * platform specific mechanisms.
 *
 * The returned address will be in the
 * [D-Bus address format](https://dbus.freedesktop.org/doc/dbus-specification.html#addresses).
 *
 * Returns: a valid D-Bus address string for @bus_type or %NULL if
 *     @error is set
 *
 * Since: 2.26
 */
gchar *
g_dbus_address_get_for_bus_sync (GBusType       bus_type,
                                 GCancellable  *cancellable,
                                 GError       **error)
{
  gchar *ret, *s = NULL;
  const gchar *starter_bus;
  GError *local_error;

  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  ret = NULL;
  local_error = NULL;

  if (G_UNLIKELY (_g_dbus_debug_address ()))
    {
      guint n;
      _g_dbus_debug_print_lock ();
      s = _g_dbus_enum_to_string (G_TYPE_BUS_TYPE, bus_type);
      g_print ("GDBus-debug:Address: In g_dbus_address_get_for_bus_sync() for bus type '%s'\n",
               s);
      g_free (s);
      for (n = 0; n < 3; n++)
        {
          const gchar *k;
          const gchar *v;
          switch (n)
            {
            case 0: k = "DBUS_SESSION_BUS_ADDRESS"; break;
            case 1: k = "DBUS_SYSTEM_BUS_ADDRESS"; break;
            case 2: k = "DBUS_STARTER_BUS_TYPE"; break;
            default: g_assert_not_reached ();
            }
          v = g_getenv (k);
          g_print ("GDBus-debug:Address: env var %s", k);
          if (v != NULL)
            g_print ("='%s'\n", v);
          else
            g_print (" is not set\n");
        }
      _g_dbus_debug_print_unlock ();
    }

  switch (bus_type)
    {
    case G_BUS_TYPE_SYSTEM:
      ret = g_strdup (g_getenv ("DBUS_SYSTEM_BUS_ADDRESS"));
      if (ret == NULL)
        {
          ret = g_strdup ("unix:path=/var/run/dbus/system_bus_socket");
        }
      break;

    case G_BUS_TYPE_SESSION:
      ret = g_strdup (g_getenv ("DBUS_SESSION_BUS_ADDRESS"));
      if (ret == NULL)
        {
          ret = get_session_address_platform_specific (&local_error);
        }
      break;

    case G_BUS_TYPE_STARTER:
      starter_bus = g_getenv ("DBUS_STARTER_BUS_TYPE");
      if (g_strcmp0 (starter_bus, "session") == 0)
        {
          ret = g_dbus_address_get_for_bus_sync (G_BUS_TYPE_SESSION, cancellable, &local_error);
          goto out;
        }
      else if (g_strcmp0 (starter_bus, "system") == 0)
        {
          ret = g_dbus_address_get_for_bus_sync (G_BUS_TYPE_SYSTEM, cancellable, &local_error);
          goto out;
        }
      else
        {
          if (starter_bus != NULL)
            {
              g_set_error (&local_error,
                           G_IO_ERROR,
                           G_IO_ERROR_FAILED,
                           _("Cannot determine bus address from DBUS_STARTER_BUS_TYPE environment variable"
                             " — unknown value “%s”"),
                           starter_bus);
            }
          else
            {
              g_set_error_literal (&local_error,
                                   G_IO_ERROR,
                                   G_IO_ERROR_FAILED,
                                   _("Cannot determine bus address because the DBUS_STARTER_BUS_TYPE environment "
                                     "variable is not set"));
            }
        }
      break;

    default:
      g_set_error (&local_error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   _("Unknown bus type %d"),
                   bus_type);
      break;
    }

 out:
  if (G_UNLIKELY (_g_dbus_debug_address ()))
    {
      _g_dbus_debug_print_lock ();
      s = _g_dbus_enum_to_string (G_TYPE_BUS_TYPE, bus_type);
      if (ret != NULL)
        {
          g_print ("GDBus-debug:Address: Returning address '%s' for bus type '%s'\n",
                   ret, s);
        }
      else
        {
          g_print ("GDBus-debug:Address: Cannot look-up address bus type '%s': %s\n",
                   s, local_error ? local_error->message : "");
        }
      g_free (s);
      _g_dbus_debug_print_unlock ();
    }

  if (local_error != NULL)
    g_propagate_error (error, local_error);

  return ret;
}

/**
 * g_dbus_address_escape_value:
 * @string: an unescaped string to be included in a D-Bus address
 *     as the value in a key-value pair
 *
 * Escape @string so it can appear in a D-Bus address as the value
 * part of a key-value pair.
 *
 * For instance, if @string is `/run/bus-for-:0`,
 * this function would return `/run/bus-for-%3A0`,
 * which could be used in a D-Bus address like
 * `unix:nonce-tcp:host=127.0.0.1,port=42,noncefile=/run/bus-for-%3A0`.
 *
 * Returns: (transfer full): a copy of @string with all
 *     non-optionally-escaped bytes escaped
 *
 * Since: 2.36
 */
gchar *
g_dbus_address_escape_value (const gchar *string)
{
  GString *s;
  gsize i;

  g_return_val_if_fail (string != NULL, NULL);

  /* There will often not be anything needing escaping at all. */
  s = g_string_sized_new (strlen (string));

  /* D-Bus address escaping is mostly the same as URI escaping... */
  g_string_append_uri_escaped (s, string, "\\/", FALSE);

  /* ... but '~' is an unreserved character in URIs, but a
   * non-optionally-escaped character in D-Bus addresses. */
  for (i = 0; i < s->len; i++)
    {
      if (G_UNLIKELY (s->str[i] == '~'))
        {
          s->str[i] = '%';
          g_string_insert (s, i + 1, "7E");
          i += 2;
        }
    }

  return g_string_free (s, FALSE);
}
