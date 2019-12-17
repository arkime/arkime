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

#include "gsimpleaction.h"
#include "gactionmap.h"
#include "gaction.h"

/**
 * SECTION:gactionmap
 * @title: GActionMap
 * @include: gio/gio.h
 * @short_description: Interface for action containers
 *
 * The GActionMap interface is implemented by #GActionGroup
 * implementations that operate by containing a number of
 * named #GAction instances, such as #GSimpleActionGroup.
 *
 * One useful application of this interface is to map the
 * names of actions from various action groups to unique,
 * prefixed names (e.g. by prepending "app." or "win.").
 * This is the motivation for the 'Map' part of the interface
 * name.
 *
 * Since: 2.32
 **/

/**
 * GActionMap:
 *
 * #GActionMap is an opaque data structure and can only be accessed
 * using the following functions.
 **/

/**
 * GActionMapInterface:
 * @lookup_action: the virtual function pointer for g_action_map_lookup_action()
 * @add_action: the virtual function pointer for g_action_map_add_action()
 * @remove_action: the virtual function pointer for g_action_map_remove_action()
 *
 * The virtual function table for #GActionMap.
 *
 * Since: 2.32
 **/

G_DEFINE_INTERFACE (GActionMap, g_action_map, G_TYPE_OBJECT)

static void
g_action_map_default_init (GActionMapInterface *iface)
{
}

/**
 * g_action_map_lookup_action:
 * @action_map: a #GActionMap
 * @action_name: the name of an action
 *
 * Looks up the action with the name @action_name in @action_map.
 *
 * If no such action exists, returns %NULL.
 *
 * Returns: (transfer none): a #GAction, or %NULL
 *
 * Since: 2.32
 */
GAction *
g_action_map_lookup_action (GActionMap  *action_map,
                            const gchar *action_name)
{
  return G_ACTION_MAP_GET_IFACE (action_map)
    ->lookup_action (action_map, action_name);
}

/**
 * g_action_map_add_action:
 * @action_map: a #GActionMap
 * @action: a #GAction
 *
 * Adds an action to the @action_map.
 *
 * If the action map already contains an action with the same name
 * as @action then the old action is dropped from the action map.
 *
 * The action map takes its own reference on @action.
 *
 * Since: 2.32
 */
void
g_action_map_add_action (GActionMap *action_map,
                         GAction    *action)
{
  G_ACTION_MAP_GET_IFACE (action_map)->add_action (action_map, action);
}

/**
 * g_action_map_remove_action:
 * @action_map: a #GActionMap
 * @action_name: the name of the action
 *
 * Removes the named action from the action map.
 *
 * If no action of this name is in the map then nothing happens.
 *
 * Since: 2.32
 */
void
g_action_map_remove_action (GActionMap  *action_map,
                            const gchar *action_name)
{
  G_ACTION_MAP_GET_IFACE (action_map)->remove_action (action_map, action_name);
}

/**
 * GActionEntry:
 * @name: the name of the action
 * @activate: the callback to connect to the "activate" signal of the
 *            action.  Since GLib 2.40, this can be %NULL for stateful
 *            actions, in which case the default handler is used.  For
 *            boolean-stated actions with no parameter, this is a
 *            toggle.  For other state types (and parameter type equal
 *            to the state type) this will be a function that
 *            just calls @change_state (which you should provide).
 * @parameter_type: the type of the parameter that must be passed to the
 *                  activate function for this action, given as a single
 *                  GVariant type string (or %NULL for no parameter)
 * @state: the initial state for this action, given in
 *         [GVariant text format][gvariant-text].  The state is parsed
 *         with no extra type information, so type tags must be added to
 *         the string if they are necessary.  Stateless actions should
 *         give %NULL here.
 * @change_state: the callback to connect to the "change-state" signal
 *                of the action.  All stateful actions should provide a
 *                handler here; stateless actions should not.
 *
 * This struct defines a single action.  It is for use with
 * g_action_map_add_action_entries().
 *
 * The order of the items in the structure are intended to reflect
 * frequency of use.  It is permissible to use an incomplete initialiser
 * in order to leave some of the later values as %NULL.  All values
 * after @name are optional.  Additional optional fields may be added in
 * the future.
 *
 * See g_action_map_add_action_entries() for an example.
 **/

/**
 * g_action_map_add_action_entries:
 * @action_map: a #GActionMap
 * @entries: (array length=n_entries) (element-type GActionEntry): a pointer to
 *           the first item in an array of #GActionEntry structs
 * @n_entries: the length of @entries, or -1 if @entries is %NULL-terminated
 * @user_data: the user data for signal connections
 *
 * A convenience function for creating multiple #GSimpleAction instances
 * and adding them to a #GActionMap.
 *
 * Each action is constructed as per one #GActionEntry.
 *
 * |[<!-- language="C" -->
 * static void
 * activate_quit (GSimpleAction *simple,
 *                GVariant      *parameter,
 *                gpointer       user_data)
 * {
 *   exit (0);
 * }
 *
 * static void
 * activate_print_string (GSimpleAction *simple,
 *                        GVariant      *parameter,
 *                        gpointer       user_data)
 * {
 *   g_print ("%s\n", g_variant_get_string (parameter, NULL));
 * }
 *
 * static GActionGroup *
 * create_action_group (void)
 * {
 *   const GActionEntry entries[] = {
 *     { "quit",         activate_quit              },
 *     { "print-string", activate_print_string, "s" }
 *   };
 *   GSimpleActionGroup *group;
 *
 *   group = g_simple_action_group_new ();
 *   g_action_map_add_action_entries (G_ACTION_MAP (group), entries, G_N_ELEMENTS (entries), NULL);
 *
 *   return G_ACTION_GROUP (group);
 * }
 * ]|
 *
 * Since: 2.32
 */
void
g_action_map_add_action_entries (GActionMap         *action_map,
                                 const GActionEntry *entries,
                                 gint                n_entries,
                                 gpointer            user_data)
{
  gint i;

  g_return_if_fail (G_IS_ACTION_MAP (action_map));
  g_return_if_fail (entries != NULL || n_entries == 0);

  for (i = 0; n_entries == -1 ? entries[i].name != NULL : i < n_entries; i++)
    {
      const GActionEntry *entry = &entries[i];
      const GVariantType *parameter_type;
      GSimpleAction *action;

      if (entry->parameter_type)
        {
          if (!g_variant_type_string_is_valid (entry->parameter_type))
            {
              g_critical ("g_action_map_add_entries: the type "
                          "string '%s' given as the parameter type for "
                          "action '%s' is not a valid GVariant type "
                          "string.  This action will not be added.",
                          entry->parameter_type, entry->name);
              return;
            }

          parameter_type = G_VARIANT_TYPE (entry->parameter_type);
        }
      else
        parameter_type = NULL;

      if (entry->state)
        {
          GError *error = NULL;
          GVariant *state;

          state = g_variant_parse (NULL, entry->state, NULL, NULL, &error);
          if (state == NULL)
            {
              g_critical ("g_action_map_add_entries: GVariant could "
                          "not parse the state value given for action '%s' "
                          "('%s'): %s.  This action will not be added.",
                          entry->name, entry->state, error->message);
              g_error_free (error);
              continue;
            }

          action = g_simple_action_new_stateful (entry->name,
                                                 parameter_type,
                                                 state);

          g_variant_unref (state);
        }
      else
        {
          action = g_simple_action_new (entry->name,
                                        parameter_type);
        }

      if (entry->activate != NULL)
        g_signal_connect (action, "activate",
                          G_CALLBACK (entry->activate), user_data);

      if (entry->change_state != NULL)
        g_signal_connect (action, "change-state",
                          G_CALLBACK (entry->change_state), user_data);

      g_action_map_add_action (action_map, G_ACTION (action));
      g_object_unref (action);
    }
}
