/*
 * Copyright © 2010 Codethink Limited
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

#include "gsimpleactiongroup.h"

#include "gsimpleaction.h"
#include "gactionmap.h"
#include "gaction.h"

/**
 * SECTION:gsimpleactiongroup
 * @title: GSimpleActionGroup
 * @short_description: A simple GActionGroup implementation
 * @include: gio/gio.h
 *
 * #GSimpleActionGroup is a hash table filled with #GAction objects,
 * implementing the #GActionGroup and #GActionMap interfaces.
 **/

struct _GSimpleActionGroupPrivate
{
  GHashTable *table;  /* string -> GAction */
};

static void g_simple_action_group_iface_init (GActionGroupInterface *);
static void g_simple_action_group_map_iface_init (GActionMapInterface *);
G_DEFINE_TYPE_WITH_CODE (GSimpleActionGroup,
  g_simple_action_group, G_TYPE_OBJECT,
  G_ADD_PRIVATE (GSimpleActionGroup)
  G_IMPLEMENT_INTERFACE (G_TYPE_ACTION_GROUP,
                         g_simple_action_group_iface_init);
  G_IMPLEMENT_INTERFACE (G_TYPE_ACTION_MAP,
                         g_simple_action_group_map_iface_init))

static gchar **
g_simple_action_group_list_actions (GActionGroup *group)
{
  GSimpleActionGroup *simple = G_SIMPLE_ACTION_GROUP (group);
  GHashTableIter iter;
  gint n, i = 0;
  gchar **keys;
  gpointer key;

  n = g_hash_table_size (simple->priv->table);
  keys = g_new (gchar *, n + 1);

  g_hash_table_iter_init (&iter, simple->priv->table);
  while (g_hash_table_iter_next (&iter, &key, NULL))
    keys[i++] = g_strdup (key);
  g_assert_cmpint (i, ==, n);
  keys[n] = NULL;

  return keys;
}

static gboolean
g_simple_action_group_query_action (GActionGroup        *group,
                                    const gchar         *action_name,
                                    gboolean            *enabled,
                                    const GVariantType **parameter_type,
                                    const GVariantType **state_type,
                                    GVariant           **state_hint,
                                    GVariant           **state)
{
  GSimpleActionGroup *simple = G_SIMPLE_ACTION_GROUP (group);
  GAction *action;

  action = g_hash_table_lookup (simple->priv->table, action_name);

  if (action == NULL)
    return FALSE;

  if (enabled)
    *enabled = g_action_get_enabled (action);

  if (parameter_type)
    *parameter_type = g_action_get_parameter_type (action);

  if (state_type)
    *state_type = g_action_get_state_type (action);

  if (state_hint)
    *state_hint = g_action_get_state_hint (action);

  if (state)
    *state = g_action_get_state (action);

  return TRUE;
}

static void
g_simple_action_group_change_state (GActionGroup *group,
                                    const gchar  *action_name,
                                    GVariant     *value)
{
  GSimpleActionGroup *simple = G_SIMPLE_ACTION_GROUP (group);
  GAction *action;

  action = g_hash_table_lookup (simple->priv->table, action_name);

  if (action == NULL)
    return;

  g_action_change_state (action, value);
}

static void
g_simple_action_group_activate (GActionGroup *group,
                                const gchar  *action_name,
                                GVariant     *parameter)
{
  GSimpleActionGroup *simple = G_SIMPLE_ACTION_GROUP (group);
  GAction *action;

  action = g_hash_table_lookup (simple->priv->table, action_name);

  if (action == NULL)
    return;

  g_action_activate (action, parameter);
}

static void
action_enabled_notify (GAction     *action,
                       GParamSpec  *pspec,
                       gpointer     user_data)
{
  g_action_group_action_enabled_changed (user_data,
                                         g_action_get_name (action),
                                         g_action_get_enabled (action));
}

static void
action_state_notify (GAction    *action,
                     GParamSpec *pspec,
                     gpointer    user_data)
{
  GVariant *value;

  value = g_action_get_state (action);
  g_action_group_action_state_changed (user_data,
                                       g_action_get_name (action),
                                       value);
  g_variant_unref (value);
}

static void
g_simple_action_group_disconnect (gpointer key,
                                  gpointer value,
                                  gpointer user_data)
{
  g_signal_handlers_disconnect_by_func (value, action_enabled_notify,
                                        user_data);
  g_signal_handlers_disconnect_by_func (value, action_state_notify,
                                        user_data);
}

static GAction *
g_simple_action_group_lookup_action (GActionMap *action_map,
                                     const gchar        *action_name)
{
  GSimpleActionGroup *simple = G_SIMPLE_ACTION_GROUP (action_map);

  return g_hash_table_lookup (simple->priv->table, action_name);
}

static void
g_simple_action_group_add_action (GActionMap *action_map,
                                  GAction    *action)
{
  GSimpleActionGroup *simple = G_SIMPLE_ACTION_GROUP (action_map);
  const gchar *action_name;
  GAction *old_action;

  action_name = g_action_get_name (action);
  if (action_name == NULL)
    {
      g_critical ("The supplied action has no name. You must set the "
                  "GAction:name property when creating an action.");
      return;
    }

  old_action = g_hash_table_lookup (simple->priv->table, action_name);

  if (old_action != action)
    {
      if (old_action != NULL)
        {
          g_action_group_action_removed (G_ACTION_GROUP (simple),
                                         action_name);
          g_simple_action_group_disconnect (NULL, old_action, simple);
        }

      g_signal_connect (action, "notify::enabled",
                        G_CALLBACK (action_enabled_notify), simple);

      if (g_action_get_state_type (action) != NULL)
        g_signal_connect (action, "notify::state",
                          G_CALLBACK (action_state_notify), simple);

      g_hash_table_insert (simple->priv->table,
                           g_strdup (action_name),
                           g_object_ref (action));

      g_action_group_action_added (G_ACTION_GROUP (simple), action_name);
    }
}

static void
g_simple_action_group_remove_action (GActionMap  *action_map,
                                     const gchar *action_name)
{
  GSimpleActionGroup *simple = G_SIMPLE_ACTION_GROUP (action_map);
  GAction *action;

  action = g_hash_table_lookup (simple->priv->table, action_name);

  if (action != NULL)
    {
      g_action_group_action_removed (G_ACTION_GROUP (simple), action_name);
      g_simple_action_group_disconnect (NULL, action, simple);
      g_hash_table_remove (simple->priv->table, action_name);
    }
}

static void
g_simple_action_group_finalize (GObject *object)
{
  GSimpleActionGroup *simple = G_SIMPLE_ACTION_GROUP (object);

  g_hash_table_foreach (simple->priv->table,
                        g_simple_action_group_disconnect,
                        simple);
  g_hash_table_unref (simple->priv->table);

  G_OBJECT_CLASS (g_simple_action_group_parent_class)
    ->finalize (object);
}

static void
g_simple_action_group_init (GSimpleActionGroup *simple)
{
  simple->priv = g_simple_action_group_get_instance_private (simple);
  simple->priv->table = g_hash_table_new_full (g_str_hash, g_str_equal,
                                               g_free, g_object_unref);
}

static void
g_simple_action_group_class_init (GSimpleActionGroupClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = g_simple_action_group_finalize;
}

static void
g_simple_action_group_iface_init (GActionGroupInterface *iface)
{
  iface->list_actions = g_simple_action_group_list_actions;
  iface->query_action = g_simple_action_group_query_action;
  iface->change_action_state = g_simple_action_group_change_state;
  iface->activate_action = g_simple_action_group_activate;
}

static void
g_simple_action_group_map_iface_init (GActionMapInterface *iface)
{
  iface->add_action = g_simple_action_group_add_action;
  iface->remove_action = g_simple_action_group_remove_action;
  iface->lookup_action = g_simple_action_group_lookup_action;
}

/**
 * g_simple_action_group_new:
 *
 * Creates a new, empty, #GSimpleActionGroup.
 *
 * Returns: a new #GSimpleActionGroup
 *
 * Since: 2.28
 **/
GSimpleActionGroup *
g_simple_action_group_new (void)
{
  return g_object_new (G_TYPE_SIMPLE_ACTION_GROUP, NULL);
}

/**
 * g_simple_action_group_lookup:
 * @simple: a #GSimpleActionGroup
 * @action_name: the name of an action
 *
 * Looks up the action with the name @action_name in the group.
 *
 * If no such action exists, returns %NULL.
 *
 * Returns: (transfer none): a #GAction, or %NULL
 *
 * Since: 2.28
 *
 * Deprecated: 2.38: Use g_action_map_lookup_action()
 */
GAction *
g_simple_action_group_lookup (GSimpleActionGroup *simple,
                              const gchar        *action_name)
{
  g_return_val_if_fail (G_IS_SIMPLE_ACTION_GROUP (simple), NULL);

  return g_action_map_lookup_action (G_ACTION_MAP (simple), action_name);
}

/**
 * g_simple_action_group_insert:
 * @simple: a #GSimpleActionGroup
 * @action: a #GAction
 *
 * Adds an action to the action group.
 *
 * If the action group already contains an action with the same name as
 * @action then the old action is dropped from the group.
 *
 * The action group takes its own reference on @action.
 *
 * Since: 2.28
 *
 * Deprecated: 2.38: Use g_action_map_add_action()
 **/
void
g_simple_action_group_insert (GSimpleActionGroup *simple,
                              GAction            *action)
{
  g_return_if_fail (G_IS_SIMPLE_ACTION_GROUP (simple));

  g_action_map_add_action (G_ACTION_MAP (simple), action);
}

/**
 * g_simple_action_group_remove:
 * @simple: a #GSimpleActionGroup
 * @action_name: the name of the action
 *
 * Removes the named action from the action group.
 *
 * If no action of this name is in the group then nothing happens.
 *
 * Since: 2.28
 *
 * Deprecated: 2.38: Use g_action_map_remove_action()
 **/
void
g_simple_action_group_remove (GSimpleActionGroup *simple,
                              const gchar        *action_name)
{
  g_return_if_fail (G_IS_SIMPLE_ACTION_GROUP (simple));

  g_action_map_remove_action (G_ACTION_MAP (simple), action_name);
}


/**
 * g_simple_action_group_add_entries:
 * @simple: a #GSimpleActionGroup
 * @entries: (array length=n_entries): a pointer to the first item in
 *           an array of #GActionEntry structs
 * @n_entries: the length of @entries, or -1
 * @user_data: the user data for signal connections
 *
 * A convenience function for creating multiple #GSimpleAction instances
 * and adding them to the action group.
 *
 * Since: 2.30
 *
 * Deprecated: 2.38: Use g_action_map_add_action_entries()
 **/
void
g_simple_action_group_add_entries (GSimpleActionGroup *simple,
                                   const GActionEntry *entries,
                                   gint                n_entries,
                                   gpointer            user_data)
{
  g_action_map_add_action_entries (G_ACTION_MAP (simple), entries, n_entries, user_data);
}
