/* GLib testing framework examples and tests
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

#include <gio/gio.h>
#include <unistd.h>

#include "gdbus-tests.h"

/* ---------------------------------------------------------------------------------------------------- */

typedef struct
{
  GMainLoop *loop;
  gboolean   timed_out;
} PropertyNotifyData;

static void
on_property_notify (GObject    *object,
                    GParamSpec *pspec,
                    gpointer    user_data)
{
  PropertyNotifyData *data = user_data;
  g_main_loop_quit (data->loop);
}

static gboolean
on_property_notify_timeout (gpointer user_data)
{
  PropertyNotifyData *data = user_data;
  data->timed_out = TRUE;
  g_main_loop_quit (data->loop);
  return TRUE;
}

gboolean
_g_assert_property_notify_run (gpointer     object,
                               const gchar *property_name)
{
  gchar *s;
  gulong handler_id;
  guint timeout_id;
  PropertyNotifyData data;

  data.loop = g_main_loop_new (g_main_context_get_thread_default (), FALSE);
  data.timed_out = FALSE;
  s = g_strdup_printf ("notify::%s", property_name);
  handler_id = g_signal_connect (object,
                                 s,
                                 G_CALLBACK (on_property_notify),
                                 &data);
  g_free (s);
  timeout_id = g_timeout_add_seconds (30,
                                      on_property_notify_timeout,
                                      &data);
  g_main_loop_run (data.loop);
  g_signal_handler_disconnect (object, handler_id);
  g_source_remove (timeout_id);
  g_main_loop_unref (data.loop);

  return data.timed_out;
}

static gboolean
_give_up (gpointer data)
{
  g_error ("%s", (const gchar *) data);
  g_return_val_if_reached (TRUE);
}

void
ensure_gdbus_testserver_up (void)
{
  guint id;
  gchar *name_owner;
  GDBusConnection *connection;
  GDBusProxy *proxy;
  GError *error = NULL;

  connection = g_bus_get_sync (G_BUS_TYPE_SESSION,
                               NULL,
                               &error);

  g_assert_no_error (error);
  error = NULL;

  proxy = g_dbus_proxy_new_sync (connection,
                                 G_DBUS_PROXY_FLAGS_NONE,
                                 NULL,                      /* GDBusInterfaceInfo */
                                 "com.example.TestService", /* name */
                                 "/com/example/TestObject", /* object path */
                                 "com.example.Frob",        /* interface */
                                 NULL, /* GCancellable */
                                 &error);
  g_assert_no_error (error);

  id = g_timeout_add_seconds (60, _give_up,
      "waited more than ~ 60s for gdbus-testserver to take its bus name");

  while (TRUE)
    {
      name_owner = g_dbus_proxy_get_name_owner (proxy);

      if (name_owner != NULL)
        break;

      g_main_context_iteration (NULL, TRUE);
    }

  g_source_remove (id);
  g_free (name_owner);
  g_object_unref (proxy);
  g_object_unref (connection);
}

/* ---------------------------------------------------------------------------------------------------- */

typedef struct
{
  GMainLoop *loop;
  gboolean   timed_out;
} SignalReceivedData;

static void
on_signal_received (gpointer user_data)
{
  SignalReceivedData *data = user_data;
  g_main_loop_quit (data->loop);
}

static gboolean
on_signal_received_timeout (gpointer user_data)
{
  SignalReceivedData *data = user_data;
  data->timed_out = TRUE;
  g_main_loop_quit (data->loop);
  return TRUE;
}

gboolean
_g_assert_signal_received_run (gpointer     object,
                               const gchar *signal_name)
{
  gulong handler_id;
  guint timeout_id;
  SignalReceivedData data;

  data.loop = g_main_loop_new (g_main_context_get_thread_default (), FALSE);
  data.timed_out = FALSE;
  handler_id = g_signal_connect_swapped (object,
                                         signal_name,
                                         G_CALLBACK (on_signal_received),
                                         &data);
  timeout_id = g_timeout_add_seconds (30,
                                      on_signal_received_timeout,
                                      &data);
  g_main_loop_run (data.loop);
  g_signal_handler_disconnect (object, handler_id);
  g_source_remove (timeout_id);
  g_main_loop_unref (data.loop);

  return data.timed_out;
}

/* ---------------------------------------------------------------------------------------------------- */

GDBusConnection *
_g_bus_get_priv (GBusType            bus_type,
                 GCancellable       *cancellable,
                 GError            **error)
{
  gchar *address;
  GDBusConnection *ret;

  ret = NULL;

  address = g_dbus_address_get_for_bus_sync (bus_type, cancellable, error);
  if (address == NULL)
    goto out;

  ret = g_dbus_connection_new_for_address_sync (address,
                                                G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT |
                                                G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION,
                                                NULL, /* GDBusAuthObserver */
                                                cancellable,
                                                error);
  g_free (address);

 out:
  return ret;
}

/* ---------------------------------------------------------------------------------------------------- */
