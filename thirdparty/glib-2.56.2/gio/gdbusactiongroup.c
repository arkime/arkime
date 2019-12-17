/*
 * Copyright © 2010 Codethink Limited
 * Copyright © 2011 Canonical Limited
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
 * Authors: Ryan Lortie <desrt@desrt.ca>
 */

#include "config.h"

#include "gdbusactiongroup-private.h"

#include "gremoteactiongroup.h"
#include "gdbusconnection.h"
#include "gactiongroup.h"

/**
 * SECTION:gdbusactiongroup
 * @title: GDBusActionGroup
 * @short_description: A D-Bus GActionGroup implementation
 * @include: gio/gio.h
 * @see_also: [GActionGroup exporter][gio-GActionGroup-exporter]
 *
 * #GDBusActionGroup is an implementation of the #GActionGroup
 * interface that can be used as a proxy for an action group
 * that is exported over D-Bus with g_dbus_connection_export_action_group().
 */

/**
 * GDBusActionGroup:
 *
 * #GDBusActionGroup is an opaque data structure and can only be accessed
 * using the following functions.
 */

struct _GDBusActionGroup
{
  GObject parent_instance;

  GDBusConnection *connection;
  gchar           *bus_name;
  gchar           *object_path;
  guint            subscription_id;
  GHashTable      *actions;

  /* The 'strict' flag indicates that the non-existence of at least one
   * action has potentially been observed through the API.  This means
   * that we should always emit 'action-added' signals for all new
   * actions.
   *
   * The user can observe the non-existence of an action by listing the
   * actions or by performing a query (such as parameter type) on a
   * non-existent action.
   *
   * If the user has no way of knowing that a given action didn't
   * already exist then we can skip emitting 'action-added' signals
   * since they have no way of knowing that it wasn't there from the
   * start.
   */
  gboolean         strict;
};

typedef GObjectClass GDBusActionGroupClass;

typedef struct
{
  gchar        *name;
  GVariantType *parameter_type;
  gboolean      enabled;
  GVariant     *state;
} ActionInfo;

static void
action_info_free (gpointer user_data)
{
  ActionInfo *info = user_data;

  g_free (info->name);

  if (info->state)
    g_variant_unref (info->state);

  if (info->parameter_type)
    g_variant_type_free (info->parameter_type);

  g_slice_free (ActionInfo, info);
}

static ActionInfo *
action_info_new_from_iter (GVariantIter *iter)
{
  const gchar *param_str;
  ActionInfo *info;
  gboolean enabled;
  GVariant *state;
  gchar *name;

  if (!g_variant_iter_next (iter, "{s(b&g@av)}", &name,
                            &enabled, &param_str, &state))
    return NULL;

  info = g_slice_new (ActionInfo);
  info->name = name;
  info->enabled = enabled;

  if (g_variant_n_children (state))
    g_variant_get_child (state, 0, "v", &info->state);
  else
    info->state = NULL;
  g_variant_unref (state);

  if (param_str[0])
    info->parameter_type = g_variant_type_copy ((GVariantType *) param_str);
  else
    info->parameter_type = NULL;

  return info;
}

static void g_dbus_action_group_remote_iface_init (GRemoteActionGroupInterface *iface);
static void g_dbus_action_group_iface_init        (GActionGroupInterface       *iface);
G_DEFINE_TYPE_WITH_CODE (GDBusActionGroup, g_dbus_action_group, G_TYPE_OBJECT,
  G_IMPLEMENT_INTERFACE (G_TYPE_ACTION_GROUP, g_dbus_action_group_iface_init)
  G_IMPLEMENT_INTERFACE (G_TYPE_REMOTE_ACTION_GROUP, g_dbus_action_group_remote_iface_init))

static void
g_dbus_action_group_changed (GDBusConnection *connection,
                             const gchar     *sender,
                             const gchar     *object_path,
                             const gchar     *interface_name,
                             const gchar     *signal_name,
                             GVariant        *parameters,
                             gpointer         user_data)
{
  GDBusActionGroup *group = user_data;
  GActionGroup *g_group = user_data;

  /* make sure that we've been fully initialised */
  if (group->actions == NULL)
    return;

  if (g_str_equal (signal_name, "Changed") &&
      g_variant_is_of_type (parameters, G_VARIANT_TYPE ("(asa{sb}a{sv}a{s(bgav)})")))
    {
      /* Removes */
      {
        GVariantIter *iter;
        const gchar *name;

        g_variant_get_child (parameters, 0, "as", &iter);
        while (g_variant_iter_next (iter, "&s", &name))
          {
            if (g_hash_table_lookup (group->actions, name))
              {
                g_hash_table_remove (group->actions, name);
                g_action_group_action_removed (g_group, name);
              }
          }
        g_variant_iter_free (iter);
      }

      /* Enable changes */
      {
        GVariantIter *iter;
        const gchar *name;
        gboolean enabled;

        g_variant_get_child (parameters, 1, "a{sb}", &iter);
        while (g_variant_iter_next (iter, "{&sb}", &name, &enabled))
          {
            ActionInfo *info;

            info = g_hash_table_lookup (group->actions, name);

            if (info && info->enabled != enabled)
              {
                info->enabled = enabled;
                g_action_group_action_enabled_changed (g_group, name, enabled);
              }
          }
        g_variant_iter_free (iter);
      }

      /* State changes */
      {
        GVariantIter *iter;
        const gchar *name;
        GVariant *state;

        g_variant_get_child (parameters, 2, "a{sv}", &iter);
        while (g_variant_iter_next (iter, "{&sv}", &name, &state))
          {
            ActionInfo *info;

            info = g_hash_table_lookup (group->actions, name);

            if (info && info->state && !g_variant_equal (state, info->state) &&
                g_variant_is_of_type (state, g_variant_get_type (info->state)))
              {
                g_variant_unref (info->state);
                info->state = g_variant_ref (state);

                g_action_group_action_state_changed (g_group, name, state);
              }

            g_variant_unref (state);
          }
        g_variant_iter_free (iter);
      }

      /* Additions */
      {
        GVariantIter *iter;
        ActionInfo *info;

        g_variant_get_child (parameters, 3, "a{s(bgav)}", &iter);
        while ((info = action_info_new_from_iter (iter)))
          {
            if (!g_hash_table_lookup (group->actions, info->name))
              {
                g_hash_table_insert (group->actions, info->name, info);

                if (group->strict)
                  g_action_group_action_added (g_group, info->name);
              }
            else
              action_info_free (info);
          }
        g_variant_iter_free (iter);
      }
    }
}


static void
g_dbus_action_group_describe_all_done (GObject      *source,
                                       GAsyncResult *result,
                                       gpointer      user_data)
{
  GDBusActionGroup *group= user_data;
  GVariant *reply;

  g_assert (group->actions == NULL);
  group->actions = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, action_info_free);

  g_assert (group->connection == (gpointer) source);
  reply = g_dbus_connection_call_finish (group->connection, result, NULL);

  if (reply != NULL)
    {
      GVariantIter *iter;
      ActionInfo *action;

      g_variant_get (reply, "(a{s(bgav)})", &iter);
      while ((action = action_info_new_from_iter (iter)))
        {
          g_hash_table_insert (group->actions, action->name, action);

          if (group->strict)
            g_action_group_action_added (G_ACTION_GROUP (group), action->name);
        }
      g_variant_iter_free (iter);
      g_variant_unref (reply);
    }

  g_object_unref (group);
}


static void
g_dbus_action_group_async_init (GDBusActionGroup *group)
{
  if (group->subscription_id != 0)
    return;

  group->subscription_id =
    g_dbus_connection_signal_subscribe (group->connection, group->bus_name, "org.gtk.Actions", "Changed", group->object_path,
                                        NULL, G_DBUS_SIGNAL_FLAGS_NONE, g_dbus_action_group_changed, group, NULL);

  g_dbus_connection_call (group->connection, group->bus_name, group->object_path, "org.gtk.Actions", "DescribeAll", NULL,
                          G_VARIANT_TYPE ("(a{s(bgav)})"), G_DBUS_CALL_FLAGS_NONE, -1, NULL,
                          g_dbus_action_group_describe_all_done, g_object_ref (group));
}

static gchar **
g_dbus_action_group_list_actions (GActionGroup *g_group)
{
  GDBusActionGroup *group = G_DBUS_ACTION_GROUP (g_group);
  gchar **keys;

  if (group->actions != NULL)
    {
      GHashTableIter iter;
      gint n, i = 0;
      gpointer key;

      n = g_hash_table_size (group->actions);
      keys = g_new (gchar *, n + 1);

      g_hash_table_iter_init (&iter, group->actions);
      while (g_hash_table_iter_next (&iter, &key, NULL))
        keys[i++] = g_strdup (key);
      g_assert_cmpint (i, ==, n);
      keys[n] = NULL;
    }
  else
    {
      g_dbus_action_group_async_init (group);
      keys = g_new0 (gchar *, 1);
    }

  group->strict = TRUE;

  return keys;
}

static gboolean
g_dbus_action_group_query_action (GActionGroup        *g_group,
                                  const gchar         *action_name,
                                  gboolean            *enabled,
                                  const GVariantType **parameter_type,
                                  const GVariantType **state_type,
                                  GVariant           **state_hint,
                                  GVariant           **state)
{
  GDBusActionGroup *group = G_DBUS_ACTION_GROUP (g_group);
  ActionInfo *info;

  if (group->actions != NULL)
    {
      info = g_hash_table_lookup (group->actions, action_name);

      if (info == NULL)
        {
          group->strict = TRUE;
          return FALSE;
        }

      if (enabled)
        *enabled = info->enabled;

      if (parameter_type)
        *parameter_type = info->parameter_type;

      if (state_type)
        *state_type = info->state ? g_variant_get_type (info->state) : NULL;

      if (state_hint)
        *state_hint = NULL;

      if (state)
        *state = info->state ? g_variant_ref (info->state) : NULL;

      return TRUE;
    }
  else
    {
      g_dbus_action_group_async_init (group);
      group->strict = TRUE;

      return FALSE;
    }
}

static void
g_dbus_action_group_activate_action_full (GRemoteActionGroup *remote,
                                          const gchar        *action_name,
                                          GVariant           *parameter,
                                          GVariant           *platform_data)
{
  GDBusActionGroup *group = G_DBUS_ACTION_GROUP (remote);
  GVariantBuilder builder;

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("av"));

  if (parameter)
    g_variant_builder_add (&builder, "v", parameter);

  g_dbus_connection_call (group->connection, group->bus_name, group->object_path, "org.gtk.Actions", "Activate",
                          g_variant_new ("(sav@a{sv})", action_name, &builder, platform_data),
                          NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);
}

static void
g_dbus_action_group_change_action_state_full (GRemoteActionGroup *remote,
                                              const gchar        *action_name,
                                              GVariant           *value,
                                              GVariant           *platform_data)
{
  GDBusActionGroup *group = G_DBUS_ACTION_GROUP (remote);

  g_dbus_connection_call (group->connection, group->bus_name, group->object_path, "org.gtk.Actions", "SetState",
                          g_variant_new ("(sv@a{sv})", action_name, value, platform_data),
                          NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);
}

static void
g_dbus_action_group_change_state (GActionGroup *group,
                                  const gchar  *action_name,
                                  GVariant     *value)
{
  g_dbus_action_group_change_action_state_full (G_REMOTE_ACTION_GROUP (group),
                                                action_name, value, g_variant_new ("a{sv}", NULL));
}

static void
g_dbus_action_group_activate (GActionGroup *group,
                              const gchar  *action_name,
                              GVariant     *parameter)
{
  g_dbus_action_group_activate_action_full (G_REMOTE_ACTION_GROUP (group),
                                            action_name, parameter, g_variant_new ("a{sv}", NULL));
}

static void
g_dbus_action_group_finalize (GObject *object)
{
  GDBusActionGroup *group = G_DBUS_ACTION_GROUP (object);

  if (group->subscription_id)
    g_dbus_connection_signal_unsubscribe (group->connection, group->subscription_id);

  if (group->actions)
    g_hash_table_unref (group->actions);

  g_object_unref (group->connection);
  g_free (group->object_path);
  g_free (group->bus_name);

  G_OBJECT_CLASS (g_dbus_action_group_parent_class)
    ->finalize (object);
}

static void
g_dbus_action_group_init (GDBusActionGroup *group)
{
}

static void
g_dbus_action_group_class_init (GDBusActionGroupClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = g_dbus_action_group_finalize;
}

static void
g_dbus_action_group_remote_iface_init (GRemoteActionGroupInterface *iface)
{
  iface->activate_action_full = g_dbus_action_group_activate_action_full;
  iface->change_action_state_full = g_dbus_action_group_change_action_state_full;
}

static void
g_dbus_action_group_iface_init (GActionGroupInterface *iface)
{
  iface->list_actions = g_dbus_action_group_list_actions;
  iface->query_action = g_dbus_action_group_query_action;
  iface->change_action_state = g_dbus_action_group_change_state;
  iface->activate_action = g_dbus_action_group_activate;
}

/**
 * g_dbus_action_group_get:
 * @connection: A #GDBusConnection
 * @bus_name: (nullable): the bus name which exports the action
 *     group or %NULL if @connection is not a message bus connection
 * @object_path: the object path at which the action group is exported
 *
 * Obtains a #GDBusActionGroup for the action group which is exported at
 * the given @bus_name and @object_path.
 *
 * The thread default main context is taken at the time of this call.
 * All signals on the menu model (and any linked models) are reported
 * with respect to this context.  All calls on the returned menu model
 * (and linked models) must also originate from this same context, with
 * the thread default main context unchanged.
 *
 * This call is non-blocking.  The returned action group may or may not
 * already be filled in.  The correct thing to do is connect the signals
 * for the action group to monitor for changes and then to call
 * g_action_group_list_actions() to get the initial list.
 *
 * Returns: (transfer full): a #GDBusActionGroup
 *
 * Since: 2.32
 */
GDBusActionGroup *
g_dbus_action_group_get (GDBusConnection *connection,
                         const gchar     *bus_name,
                         const gchar     *object_path)
{
  GDBusActionGroup *group;

  g_return_val_if_fail (bus_name != NULL || g_dbus_connection_get_unique_name (connection) == NULL, NULL);

  group = g_object_new (G_TYPE_DBUS_ACTION_GROUP, NULL);
  group->connection = g_object_ref (connection);
  group->bus_name = g_strdup (bus_name);
  group->object_path = g_strdup (object_path);

  return group;
}

gboolean
g_dbus_action_group_sync (GDBusActionGroup  *group,
                          GCancellable      *cancellable,
                          GError           **error)
{
  GVariant *reply;

  g_assert (group->subscription_id == 0);

  group->subscription_id =
    g_dbus_connection_signal_subscribe (group->connection, group->bus_name, "org.gtk.Actions", "Changed", group->object_path,
                                        NULL, G_DBUS_SIGNAL_FLAGS_NONE, g_dbus_action_group_changed, group, NULL);

  reply = g_dbus_connection_call_sync (group->connection, group->bus_name, group->object_path, "org.gtk.Actions",
                                       "DescribeAll", NULL, G_VARIANT_TYPE ("(a{s(bgav)})"),
                                       G_DBUS_CALL_FLAGS_NONE, -1, cancellable, error);

  if (reply != NULL)
    {
      GVariantIter *iter;
      ActionInfo *action;

      g_assert (group->actions == NULL);
      group->actions = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, action_info_free);

      g_variant_get (reply, "(a{s(bgav)})", &iter);
      while ((action = action_info_new_from_iter (iter)))
        g_hash_table_insert (group->actions, action->name, action);
      g_variant_iter_free (iter);
      g_variant_unref (reply);
    }

  return reply != NULL;
}
