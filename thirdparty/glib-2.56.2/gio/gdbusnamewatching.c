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

#include "gdbusutils.h"
#include "gdbusnamewatching.h"
#include "gdbuserror.h"
#include "gdbusprivate.h"
#include "gdbusconnection.h"

#include "glibintl.h"

/**
 * SECTION:gdbusnamewatching
 * @title: Watching Bus Names
 * @short_description: Simple API for watching bus names
 * @include: gio/gio.h
 *
 * Convenience API for watching bus names.
 *
 * A simple example for watching a name can be found in
 * [gdbus-example-watch-name.c](https://git.gnome.org/browse/glib/tree/gio/tests/gdbus-example-watch-name.c)
 */

G_LOCK_DEFINE_STATIC (lock);

/* ---------------------------------------------------------------------------------------------------- */

typedef enum
{
  PREVIOUS_CALL_NONE = 0,
  PREVIOUS_CALL_APPEARED,
  PREVIOUS_CALL_VANISHED,
} PreviousCall;

typedef struct
{
  volatile gint             ref_count;
  guint                     id;
  gchar                    *name;
  GBusNameWatcherFlags      flags;
  gchar                    *name_owner;
  GBusNameAppearedCallback  name_appeared_handler;
  GBusNameVanishedCallback  name_vanished_handler;
  gpointer                  user_data;
  GDestroyNotify            user_data_free_func;
  GMainContext             *main_context;

  GDBusConnection          *connection;
  gulong                    disconnected_signal_handler_id;
  guint                     name_owner_changed_subscription_id;

  PreviousCall              previous_call;

  gboolean                  cancelled;
  gboolean                  initialized;
} Client;

/* Must be accessed atomically. */
static volatile guint next_global_id = 1;

/* Must be accessed with @lock held. */
static GHashTable *map_id_to_client = NULL;

static Client *
client_ref (Client *client)
{
  g_atomic_int_inc (&client->ref_count);
  return client;
}

static void
client_unref (Client *client)
{
  if (g_atomic_int_dec_and_test (&client->ref_count))
    {
      if (client->connection != NULL)
        {
          if (client->name_owner_changed_subscription_id > 0)
            g_dbus_connection_signal_unsubscribe (client->connection, client->name_owner_changed_subscription_id);
          if (client->disconnected_signal_handler_id > 0)
            g_signal_handler_disconnect (client->connection, client->disconnected_signal_handler_id);
          g_object_unref (client->connection);
        }
      g_free (client->name);
      g_free (client->name_owner);
      g_main_context_unref (client->main_context);
      if (client->user_data_free_func != NULL)
        client->user_data_free_func (client->user_data);
      g_free (client);
    }
}

/* ---------------------------------------------------------------------------------------------------- */

typedef enum
{
  CALL_TYPE_NAME_APPEARED,
  CALL_TYPE_NAME_VANISHED
} CallType;

typedef struct
{
  Client *client;

  /* keep this separate because client->connection may
   * be set to NULL after scheduling the call
   */
  GDBusConnection *connection;

  /* ditto */
  gchar *name_owner;

  CallType call_type;
} CallHandlerData;

static void
call_handler_data_free (CallHandlerData *data)
{
  if (data->connection != NULL)
    g_object_unref (data->connection);
  g_free (data->name_owner);
  client_unref (data->client);
  g_free (data);
}

static void
actually_do_call (Client *client, GDBusConnection *connection, const gchar *name_owner, CallType call_type)
{
  switch (call_type)
    {
    case CALL_TYPE_NAME_APPEARED:
      if (client->name_appeared_handler != NULL)
        {
          client->name_appeared_handler (connection,
                                         client->name,
                                         name_owner,
                                         client->user_data);
        }
      break;

    case CALL_TYPE_NAME_VANISHED:
      if (client->name_vanished_handler != NULL)
        {
          client->name_vanished_handler (connection,
                                         client->name,
                                         client->user_data);
        }
      break;

    default:
      g_assert_not_reached ();
      break;
    }
}

static gboolean
call_in_idle_cb (gpointer _data)
{
  CallHandlerData *data = _data;
  actually_do_call (data->client, data->connection, data->name_owner, data->call_type);
  return FALSE;
}

static void
schedule_call_in_idle (Client *client, CallType call_type)
{
  CallHandlerData *data;
  GSource *idle_source;

  data = g_new0 (CallHandlerData, 1);
  data->client = client_ref (client);
  data->connection = client->connection != NULL ? g_object_ref (client->connection) : NULL;
  data->name_owner = g_strdup (client->name_owner);
  data->call_type = call_type;

  idle_source = g_idle_source_new ();
  g_source_set_priority (idle_source, G_PRIORITY_HIGH);
  g_source_set_callback (idle_source,
                         call_in_idle_cb,
                         data,
                         (GDestroyNotify) call_handler_data_free);
  g_source_set_name (idle_source, "[gio, gdbusnamewatching.c] call_in_idle_cb");
  g_source_attach (idle_source, client->main_context);
  g_source_unref (idle_source);
}

static void
do_call (Client *client, CallType call_type)
{
  GMainContext *current_context;

  /* only schedule in idle if we're not in the right thread */
  current_context = g_main_context_ref_thread_default ();
  if (current_context != client->main_context)
    schedule_call_in_idle (client, call_type);
  else
    actually_do_call (client, client->connection, client->name_owner, call_type);
  g_main_context_unref (current_context);
}

static void
call_appeared_handler (Client *client)
{
  if (client->previous_call != PREVIOUS_CALL_APPEARED)
    {
      client->previous_call = PREVIOUS_CALL_APPEARED;
      if (!client->cancelled && client->name_appeared_handler != NULL)
        {
          do_call (client, CALL_TYPE_NAME_APPEARED);
        }
    }
}

static void
call_vanished_handler (Client  *client,
                       gboolean ignore_cancelled)
{
  if (client->previous_call != PREVIOUS_CALL_VANISHED)
    {
      client->previous_call = PREVIOUS_CALL_VANISHED;
      if (((!client->cancelled) || ignore_cancelled) && client->name_vanished_handler != NULL)
        {
          do_call (client, CALL_TYPE_NAME_VANISHED);
        }
    }
}

/* ---------------------------------------------------------------------------------------------------- */

/* Return a reference to the #Client for @watcher_id, or %NULL if it’s been
 * unwatched. This is safe to call from any thread. */
static Client *
dup_client (guint watcher_id)
{
  Client *client;

  G_LOCK (lock);

  g_assert (watcher_id != 0);
  g_assert (map_id_to_client != NULL);

  client = g_hash_table_lookup (map_id_to_client, GUINT_TO_POINTER (watcher_id));

  if (client != NULL)
    client_ref (client);

  G_UNLOCK (lock);

  return client;
}

/* Could be called from any thread, so it could be called after client_unref()
 * has started finalising the #Client. Avoid that by looking up the #Client
 * atomically. */
static void
on_connection_disconnected (GDBusConnection *connection,
                            gboolean         remote_peer_vanished,
                            GError          *error,
                            gpointer         user_data)
{
  guint watcher_id = GPOINTER_TO_UINT (user_data);
  Client *client = NULL;

  client = dup_client (watcher_id);
  if (client == NULL)
    return;

  if (client->name_owner_changed_subscription_id > 0)
    g_dbus_connection_signal_unsubscribe (client->connection, client->name_owner_changed_subscription_id);
  if (client->disconnected_signal_handler_id > 0)
    g_signal_handler_disconnect (client->connection, client->disconnected_signal_handler_id);
  g_object_unref (client->connection);
  client->disconnected_signal_handler_id = 0;
  client->name_owner_changed_subscription_id = 0;
  client->connection = NULL;

  call_vanished_handler (client, FALSE);

  client_unref (client);
}

/* ---------------------------------------------------------------------------------------------------- */

/* Will always be called from the thread which acquired client->main_context. */
static void
on_name_owner_changed (GDBusConnection *connection,
                       const gchar      *sender_name,
                       const gchar      *object_path,
                       const gchar      *interface_name,
                       const gchar      *signal_name,
                       GVariant         *parameters,
                       gpointer          user_data)
{
  guint watcher_id = GPOINTER_TO_UINT (user_data);
  Client *client = NULL;
  const gchar *name;
  const gchar *old_owner;
  const gchar *new_owner;

  client = dup_client (watcher_id);
  if (client == NULL)
    return;

  if (!client->initialized)
    goto out;

  if (g_strcmp0 (object_path, "/org/freedesktop/DBus") != 0 ||
      g_strcmp0 (interface_name, "org.freedesktop.DBus") != 0 ||
      g_strcmp0 (sender_name, "org.freedesktop.DBus") != 0)
    goto out;

  g_variant_get (parameters,
                 "(&s&s&s)",
                 &name,
                 &old_owner,
                 &new_owner);

  /* we only care about a specific name */
  if (g_strcmp0 (name, client->name) != 0)
    goto out;

  if ((old_owner != NULL && strlen (old_owner) > 0) && client->name_owner != NULL)
    {
      g_free (client->name_owner);
      client->name_owner = NULL;
      call_vanished_handler (client, FALSE);
    }

  if (new_owner != NULL && strlen (new_owner) > 0)
    {
      g_warn_if_fail (client->name_owner == NULL);
      g_free (client->name_owner);
      client->name_owner = g_strdup (new_owner);
      call_appeared_handler (client);
    }

 out:
  client_unref (client);
}

/* ---------------------------------------------------------------------------------------------------- */

static void
get_name_owner_cb (GObject      *source_object,
                   GAsyncResult *res,
                   gpointer      user_data)
{
  Client *client = user_data;
  GVariant *result;
  const char *name_owner;

  name_owner = NULL;
  result = NULL;

  result = g_dbus_connection_call_finish (client->connection,
                                          res,
                                          NULL);
  if (result != NULL)
    {
      g_variant_get (result, "(&s)", &name_owner);
    }

  if (name_owner != NULL)
    {
      g_warn_if_fail (client->name_owner == NULL);
      client->name_owner = g_strdup (name_owner);
      call_appeared_handler (client);
    }
  else
    {
      call_vanished_handler (client, FALSE);
    }

  client->initialized = TRUE;

  if (result != NULL)
    g_variant_unref (result);
  client_unref (client);
}

/* ---------------------------------------------------------------------------------------------------- */

static void
invoke_get_name_owner (Client *client)
{
  g_dbus_connection_call (client->connection,
                          "org.freedesktop.DBus",  /* bus name */
                          "/org/freedesktop/DBus", /* object path */
                          "org.freedesktop.DBus",  /* interface name */
                          "GetNameOwner",          /* method name */
                          g_variant_new ("(s)", client->name),
                          G_VARIANT_TYPE ("(s)"),
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          NULL,
                          (GAsyncReadyCallback) get_name_owner_cb,
                          client_ref (client));
}

/* ---------------------------------------------------------------------------------------------------- */

static void
start_service_by_name_cb (GObject      *source_object,
                          GAsyncResult *res,
                          gpointer      user_data)
{
  Client *client = user_data;
  GVariant *result;

  result = NULL;

  result = g_dbus_connection_call_finish (client->connection,
                                          res,
                                          NULL);
  if (result != NULL)
    {
      guint32 start_service_result;
      g_variant_get (result, "(u)", &start_service_result);

      if (start_service_result == 1) /* DBUS_START_REPLY_SUCCESS */
        {
          invoke_get_name_owner (client);
        }
      else if (start_service_result == 2) /* DBUS_START_REPLY_ALREADY_RUNNING */
        {
          invoke_get_name_owner (client);
        }
      else
        {
          g_warning ("Unexpected reply %d from StartServiceByName() method", start_service_result);
          call_vanished_handler (client, FALSE);
          client->initialized = TRUE;
        }
    }
  else
    {
      /* Errors are not unexpected; the bus will reply e.g.
       *
       *   org.freedesktop.DBus.Error.ServiceUnknown: The name org.gnome.Epiphany2
       *   was not provided by any .service files
       *
       * This doesn't mean that the name doesn't have an owner, just
       * that it's not provided by a .service file. So proceed to
       * invoke GetNameOwner().
       */
      invoke_get_name_owner (client);
    }

  if (result != NULL)
    g_variant_unref (result);
  client_unref (client);
}

/* ---------------------------------------------------------------------------------------------------- */

static void
has_connection (Client *client)
{
  /* listen for disconnection */
  client->disconnected_signal_handler_id = g_signal_connect (client->connection,
                                                             "closed",
                                                             G_CALLBACK (on_connection_disconnected),
                                                             GUINT_TO_POINTER (client->id));

  /* start listening to NameOwnerChanged messages immediately */
  client->name_owner_changed_subscription_id = g_dbus_connection_signal_subscribe (client->connection,
                                                                                   "org.freedesktop.DBus",  /* name */
                                                                                   "org.freedesktop.DBus",  /* if */
                                                                                   "NameOwnerChanged",      /* signal */
                                                                                   "/org/freedesktop/DBus", /* path */
                                                                                   client->name,
                                                                                   G_DBUS_SIGNAL_FLAGS_NONE,
                                                                                   on_name_owner_changed,
                                                                                   GUINT_TO_POINTER (client->id),
                                                                                   NULL);

  if (client->flags & G_BUS_NAME_WATCHER_FLAGS_AUTO_START)
    {
      g_dbus_connection_call (client->connection,
                              "org.freedesktop.DBus",  /* bus name */
                              "/org/freedesktop/DBus", /* object path */
                              "org.freedesktop.DBus",  /* interface name */
                              "StartServiceByName",    /* method name */
                              g_variant_new ("(su)", client->name, 0),
                              G_VARIANT_TYPE ("(u)"),
                              G_DBUS_CALL_FLAGS_NONE,
                              -1,
                              NULL,
                              (GAsyncReadyCallback) start_service_by_name_cb,
                              client_ref (client));
    }
  else
    {
      /* check owner */
      invoke_get_name_owner (client);
    }
}


static void
connection_get_cb (GObject      *source_object,
                   GAsyncResult *res,
                   gpointer      user_data)
{
  Client *client = user_data;

  client->connection = g_bus_get_finish (res, NULL);
  if (client->connection == NULL)
    {
      call_vanished_handler (client, FALSE);
      goto out;
    }

  has_connection (client);

 out:
  client_unref (client);
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_bus_watch_name:
 * @bus_type: The type of bus to watch a name on.
 * @name: The name (well-known or unique) to watch.
 * @flags: Flags from the #GBusNameWatcherFlags enumeration.
 * @name_appeared_handler: (nullable): Handler to invoke when @name is known to exist or %NULL.
 * @name_vanished_handler: (nullable): Handler to invoke when @name is known to not exist or %NULL.
 * @user_data: User data to pass to handlers.
 * @user_data_free_func: (nullable): Function for freeing @user_data or %NULL.
 *
 * Starts watching @name on the bus specified by @bus_type and calls
 * @name_appeared_handler and @name_vanished_handler when the name is
 * known to have a owner respectively known to lose its
 * owner. Callbacks will be invoked in the
 * [thread-default main context][g-main-context-push-thread-default]
 * of the thread you are calling this function from.
 *
 * You are guaranteed that one of the handlers will be invoked after
 * calling this function. When you are done watching the name, just
 * call g_bus_unwatch_name() with the watcher id this function
 * returns.
 *
 * If the name vanishes or appears (for example the application owning
 * the name could restart), the handlers are also invoked. If the
 * #GDBusConnection that is used for watching the name disconnects, then
 * @name_vanished_handler is invoked since it is no longer
 * possible to access the name.
 *
 * Another guarantee is that invocations of @name_appeared_handler
 * and @name_vanished_handler are guaranteed to alternate; that
 * is, if @name_appeared_handler is invoked then you are
 * guaranteed that the next time one of the handlers is invoked, it
 * will be @name_vanished_handler. The reverse is also true.
 *
 * This behavior makes it very simple to write applications that want
 * to take action when a certain [name exists][gdbus-watching-names].
 * Basically, the application should create object proxies in
 * @name_appeared_handler and destroy them again (if any) in
 * @name_vanished_handler.
 *
 * Returns: An identifier (never 0) that an be used with
 * g_bus_unwatch_name() to stop watching the name.
 *
 * Since: 2.26
 */
guint
g_bus_watch_name (GBusType                  bus_type,
                  const gchar              *name,
                  GBusNameWatcherFlags      flags,
                  GBusNameAppearedCallback  name_appeared_handler,
                  GBusNameVanishedCallback  name_vanished_handler,
                  gpointer                  user_data,
                  GDestroyNotify            user_data_free_func)
{
  Client *client;

  g_return_val_if_fail (g_dbus_is_name (name), 0);

  G_LOCK (lock);

  client = g_new0 (Client, 1);
  client->ref_count = 1;
  client->id = g_atomic_int_add (&next_global_id, 1); /* TODO: uh oh, handle overflow */
  client->name = g_strdup (name);
  client->flags = flags;
  client->name_appeared_handler = name_appeared_handler;
  client->name_vanished_handler = name_vanished_handler;
  client->user_data = user_data;
  client->user_data_free_func = user_data_free_func;
  client->main_context = g_main_context_ref_thread_default ();

  if (map_id_to_client == NULL)
    {
      map_id_to_client = g_hash_table_new (g_direct_hash, g_direct_equal);
    }
  g_hash_table_insert (map_id_to_client,
                       GUINT_TO_POINTER (client->id),
                       client);

  g_bus_get (bus_type,
             NULL,
             connection_get_cb,
             client_ref (client));

  G_UNLOCK (lock);

  return client->id;
}

/**
 * g_bus_watch_name_on_connection:
 * @connection: A #GDBusConnection.
 * @name: The name (well-known or unique) to watch.
 * @flags: Flags from the #GBusNameWatcherFlags enumeration.
 * @name_appeared_handler: (nullable): Handler to invoke when @name is known to exist or %NULL.
 * @name_vanished_handler: (nullable): Handler to invoke when @name is known to not exist or %NULL.
 * @user_data: User data to pass to handlers.
 * @user_data_free_func: (nullable): Function for freeing @user_data or %NULL.
 *
 * Like g_bus_watch_name() but takes a #GDBusConnection instead of a
 * #GBusType.
 *
 * Returns: An identifier (never 0) that an be used with
 * g_bus_unwatch_name() to stop watching the name.
 *
 * Since: 2.26
 */
guint g_bus_watch_name_on_connection (GDBusConnection          *connection,
                                      const gchar              *name,
                                      GBusNameWatcherFlags      flags,
                                      GBusNameAppearedCallback  name_appeared_handler,
                                      GBusNameVanishedCallback  name_vanished_handler,
                                      gpointer                  user_data,
                                      GDestroyNotify            user_data_free_func)
{
  Client *client;

  g_return_val_if_fail (G_IS_DBUS_CONNECTION (connection), 0);
  g_return_val_if_fail (g_dbus_is_name (name), 0);

  G_LOCK (lock);

  client = g_new0 (Client, 1);
  client->ref_count = 1;
  client->id = g_atomic_int_add (&next_global_id, 1); /* TODO: uh oh, handle overflow */
  client->name = g_strdup (name);
  client->flags = flags;
  client->name_appeared_handler = name_appeared_handler;
  client->name_vanished_handler = name_vanished_handler;
  client->user_data = user_data;
  client->user_data_free_func = user_data_free_func;
  client->main_context = g_main_context_ref_thread_default ();

  if (map_id_to_client == NULL)
    map_id_to_client = g_hash_table_new (g_direct_hash, g_direct_equal);

  g_hash_table_insert (map_id_to_client,
                       GUINT_TO_POINTER (client->id),
                       client);

  client->connection = g_object_ref (connection);
  G_UNLOCK (lock);

  has_connection (client);

  return client->id;
}

typedef struct {
  GClosure *name_appeared_closure;
  GClosure *name_vanished_closure;
} WatchNameData;

static WatchNameData *
watch_name_data_new (GClosure *name_appeared_closure,
                     GClosure *name_vanished_closure)
{
  WatchNameData *data;

  data = g_new0 (WatchNameData, 1);

  if (name_appeared_closure != NULL)
    {
      data->name_appeared_closure = g_closure_ref (name_appeared_closure);
      g_closure_sink (name_appeared_closure);
      if (G_CLOSURE_NEEDS_MARSHAL (name_appeared_closure))
        g_closure_set_marshal (name_appeared_closure, g_cclosure_marshal_generic);
    }

  if (name_vanished_closure != NULL)
    {
      data->name_vanished_closure = g_closure_ref (name_vanished_closure);
      g_closure_sink (name_vanished_closure);
      if (G_CLOSURE_NEEDS_MARSHAL (name_vanished_closure))
        g_closure_set_marshal (name_vanished_closure, g_cclosure_marshal_generic);
    }

  return data;
}

static void
watch_with_closures_on_name_appeared (GDBusConnection *connection,
                                      const gchar     *name,
                                      const gchar     *name_owner,
                                      gpointer         user_data)
{
  WatchNameData *data = user_data;
  GValue params[3] = { G_VALUE_INIT, G_VALUE_INIT, G_VALUE_INIT };

  g_value_init (&params[0], G_TYPE_DBUS_CONNECTION);
  g_value_set_object (&params[0], connection);

  g_value_init (&params[1], G_TYPE_STRING);
  g_value_set_string (&params[1], name);

  g_value_init (&params[2], G_TYPE_STRING);
  g_value_set_string (&params[2], name_owner);

  g_closure_invoke (data->name_appeared_closure, NULL, 3, params, NULL);

  g_value_unset (params + 0);
  g_value_unset (params + 1);
  g_value_unset (params + 2);
}

static void
watch_with_closures_on_name_vanished (GDBusConnection *connection,
                                      const gchar     *name,
                                      gpointer         user_data)
{
  WatchNameData *data = user_data;
  GValue params[2] = { G_VALUE_INIT, G_VALUE_INIT };

  g_value_init (&params[0], G_TYPE_DBUS_CONNECTION);
  g_value_set_object (&params[0], connection);

  g_value_init (&params[1], G_TYPE_STRING);
  g_value_set_string (&params[1], name);

  g_closure_invoke (data->name_vanished_closure, NULL, 2, params, NULL);

  g_value_unset (params + 0);
  g_value_unset (params + 1);
}

static void
bus_watch_name_free_func (gpointer user_data)
{
  WatchNameData *data = user_data;

  if (data->name_appeared_closure != NULL)
    g_closure_unref (data->name_appeared_closure);

  if (data->name_vanished_closure != NULL)
    g_closure_unref (data->name_vanished_closure);

  g_free (data);
}

/**
 * g_bus_watch_name_with_closures: (rename-to g_bus_watch_name)
 * @bus_type: The type of bus to watch a name on.
 * @name: The name (well-known or unique) to watch.
 * @flags: Flags from the #GBusNameWatcherFlags enumeration.
 * @name_appeared_closure: (nullable): #GClosure to invoke when @name is known
 * to exist or %NULL.
 * @name_vanished_closure: (nullable): #GClosure to invoke when @name is known
 * to not exist or %NULL.
 *
 * Version of g_bus_watch_name() using closures instead of callbacks for
 * easier binding in other languages.
 *
 * Returns: An identifier (never 0) that an be used with
 * g_bus_unwatch_name() to stop watching the name.
 *
 * Since: 2.26
 */
guint
g_bus_watch_name_with_closures (GBusType                 bus_type,
                                const gchar             *name,
                                GBusNameWatcherFlags     flags,
                                GClosure                *name_appeared_closure,
                                GClosure                *name_vanished_closure)
{
  return g_bus_watch_name (bus_type,
          name,
          flags,
          name_appeared_closure != NULL ? watch_with_closures_on_name_appeared : NULL,
          name_vanished_closure != NULL ? watch_with_closures_on_name_vanished : NULL,
          watch_name_data_new (name_appeared_closure, name_vanished_closure),
          bus_watch_name_free_func);
}

/**
 * g_bus_watch_name_on_connection_with_closures: (rename-to g_bus_watch_name_on_connection)
 * @connection: A #GDBusConnection.
 * @name: The name (well-known or unique) to watch.
 * @flags: Flags from the #GBusNameWatcherFlags enumeration.
 * @name_appeared_closure: (nullable): #GClosure to invoke when @name is known
 * to exist or %NULL.
 * @name_vanished_closure: (nullable): #GClosure to invoke when @name is known
 * to not exist or %NULL.
 *
 * Version of g_bus_watch_name_on_connection() using closures instead of callbacks for
 * easier binding in other languages.
 *
 * Returns: An identifier (never 0) that an be used with
 * g_bus_unwatch_name() to stop watching the name.
 *
 * Since: 2.26
 */
guint g_bus_watch_name_on_connection_with_closures (
                                      GDBusConnection          *connection,
                                      const gchar              *name,
                                      GBusNameWatcherFlags      flags,
                                      GClosure                 *name_appeared_closure,
                                      GClosure                 *name_vanished_closure)
{
  return g_bus_watch_name_on_connection (connection,
          name,
          flags,
          name_appeared_closure != NULL ? watch_with_closures_on_name_appeared : NULL,
          name_vanished_closure != NULL ? watch_with_closures_on_name_vanished : NULL,
          watch_name_data_new (name_appeared_closure, name_vanished_closure),
          bus_watch_name_free_func);
}

/**
 * g_bus_unwatch_name:
 * @watcher_id: An identifier obtained from g_bus_watch_name()
 *
 * Stops watching a name.
 *
 * Since: 2.26
 */
void
g_bus_unwatch_name (guint watcher_id)
{
  Client *client;

  g_return_if_fail (watcher_id > 0);

  client = NULL;

  G_LOCK (lock);
  if (watcher_id == 0 ||
      map_id_to_client == NULL ||
      (client = g_hash_table_lookup (map_id_to_client, GUINT_TO_POINTER (watcher_id))) == NULL)
    {
      g_warning ("Invalid id %d passed to g_bus_unwatch_name()", watcher_id);
      goto out;
    }

  client->cancelled = TRUE;
  g_warn_if_fail (g_hash_table_remove (map_id_to_client, GUINT_TO_POINTER (watcher_id)));

 out:
  G_UNLOCK (lock);

  /* do callback without holding lock */
  if (client != NULL)
    {
      client_unref (client);
    }
}
