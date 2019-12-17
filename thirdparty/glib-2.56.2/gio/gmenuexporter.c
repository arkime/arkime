/*
 * Copyright © 2011 Canonical Ltd.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Ryan Lortie <desrt@desrt.ca>
 */

#include "config.h"

#include "gmenuexporter.h"

#include "gdbusmethodinvocation.h"
#include "gdbusintrospection.h"
#include "gdbusnamewatching.h"
#include "gdbuserror.h"

/**
 * SECTION:gmenuexporter
 * @title: GMenuModel exporter
 * @short_description: Export GMenuModels on D-Bus
 * @include: gio/gio.h
 * @see_also: #GMenuModel, #GDBusMenuModel
 *
 * These functions support exporting a #GMenuModel on D-Bus.
 * The D-Bus interface that is used is a private implementation
 * detail.
 *
 * To access an exported #GMenuModel remotely, use
 * g_dbus_menu_model_get() to obtain a #GDBusMenuModel.
 */

/* {{{1 D-Bus Interface description */

/* For documentation of this interface, see
 * https://wiki.gnome.org/Projects/GLib/GApplication/DBusAPI
 */

static GDBusInterfaceInfo *
org_gtk_Menus_get_interface (void)
{
  static GDBusInterfaceInfo *interface_info;

  if (interface_info == NULL)
    {
      GError *error = NULL;
      GDBusNodeInfo *info;

      info = g_dbus_node_info_new_for_xml ("<node>"
                                           "  <interface name='org.gtk.Menus'>"
                                           "    <method name='Start'>"
                                           "      <arg type='au' name='groups' direction='in'/>"
                                           "      <arg type='a(uuaa{sv})' name='content' direction='out'/>"
                                           "    </method>"
                                           "    <method name='End'>"
                                           "      <arg type='au' name='groups' direction='in'/>"
                                           "    </method>"
                                           "    <signal name='Changed'>"
                                           "      arg type='a(uuuuaa{sv})' name='changes'/>"
                                           "    </signal>"
                                           "  </interface>"
                                           "</node>", &error);
      if (info == NULL)
        g_error ("%s\n", error->message);
      interface_info = g_dbus_node_info_lookup_interface (info, "org.gtk.Menus");
      g_assert (interface_info != NULL);
      g_dbus_interface_info_ref (interface_info);
      g_dbus_node_info_unref (info);
    }

  return interface_info;
}

/* {{{1 Forward declarations */
typedef struct _GMenuExporterMenu                           GMenuExporterMenu;
typedef struct _GMenuExporterLink                           GMenuExporterLink;
typedef struct _GMenuExporterGroup                          GMenuExporterGroup;
typedef struct _GMenuExporterRemote                         GMenuExporterRemote;
typedef struct _GMenuExporterWatch                          GMenuExporterWatch;
typedef struct _GMenuExporter                               GMenuExporter;

static gboolean                 g_menu_exporter_group_is_subscribed    (GMenuExporterGroup *group);
static guint                    g_menu_exporter_group_get_id           (GMenuExporterGroup *group);
static GMenuExporter *          g_menu_exporter_group_get_exporter     (GMenuExporterGroup *group);
static GMenuExporterMenu *      g_menu_exporter_group_add_menu         (GMenuExporterGroup *group,
                                                                        GMenuModel         *model);
static void                     g_menu_exporter_group_remove_menu      (GMenuExporterGroup *group,
                                                                        guint               id);

static GMenuExporterGroup *     g_menu_exporter_create_group           (GMenuExporter      *exporter);
static GMenuExporterGroup *     g_menu_exporter_lookup_group           (GMenuExporter      *exporter,
                                                                        guint               group_id);
static void                     g_menu_exporter_report                 (GMenuExporter      *exporter,
                                                                        GVariant           *report);
static void                     g_menu_exporter_remove_group           (GMenuExporter      *exporter,
                                                                        guint               id);

/* {{{1 GMenuExporterLink, GMenuExporterMenu */

struct _GMenuExporterMenu
{
  GMenuExporterGroup *group;
  guint               id;

  GMenuModel *model;
  gulong      handler_id;
  GSequence  *item_links;
};

struct _GMenuExporterLink
{
  gchar             *name;
  GMenuExporterMenu *menu;
  GMenuExporterLink *next;
};

static void
g_menu_exporter_menu_free (GMenuExporterMenu *menu)
{
  g_menu_exporter_group_remove_menu (menu->group, menu->id);

  if (menu->handler_id != 0)
    g_signal_handler_disconnect (menu->model, menu->handler_id);

  if (menu->item_links != NULL)
    g_sequence_free (menu->item_links);

  g_object_unref (menu->model);

  g_slice_free (GMenuExporterMenu, menu);
}

static void
g_menu_exporter_link_free (gpointer data)
{
  GMenuExporterLink *link = data;

  while (link != NULL)
    {
      GMenuExporterLink *tmp = link;
      link = tmp->next;

      g_menu_exporter_menu_free (tmp->menu);
      g_free (tmp->name);

      g_slice_free (GMenuExporterLink, tmp);
    }
}

static GMenuExporterLink *
g_menu_exporter_menu_create_links (GMenuExporterMenu *menu,
                                   gint               position)
{
  GMenuExporterLink *list = NULL;
  GMenuLinkIter *iter;
  const char *name;
  GMenuModel *model;

  iter = g_menu_model_iterate_item_links (menu->model, position);

  while (g_menu_link_iter_get_next (iter, &name, &model))
    {
      GMenuExporterGroup *group;
      GMenuExporterLink *tmp;

      /* keep sections in the same group, but create new groups
       * otherwise
       */
      if (!g_str_equal (name, "section"))
        group = g_menu_exporter_create_group (g_menu_exporter_group_get_exporter (menu->group));
      else
        group = menu->group;

      tmp = g_slice_new (GMenuExporterLink);
      tmp->name = g_strconcat (":", name, NULL);
      tmp->menu = g_menu_exporter_group_add_menu (group, model);
      tmp->next = list;
      list = tmp;

      g_object_unref (model);
    }

  g_object_unref (iter);

  return list;
}

static GVariant *
g_menu_exporter_menu_describe_item (GMenuExporterMenu *menu,
                                    gint               position)
{
  GMenuAttributeIter *attr_iter;
  GVariantBuilder builder;
  GSequenceIter *iter;
  GMenuExporterLink *link;
  const char *name;
  GVariant *value;

  g_variant_builder_init (&builder, G_VARIANT_TYPE_VARDICT);

  attr_iter = g_menu_model_iterate_item_attributes (menu->model, position);
  while (g_menu_attribute_iter_get_next (attr_iter, &name, &value))
    {
      g_variant_builder_add (&builder, "{sv}", name, value);
      g_variant_unref (value);
    }
  g_object_unref (attr_iter);

  iter = g_sequence_get_iter_at_pos (menu->item_links, position);
  for (link = g_sequence_get (iter); link; link = link->next)
    g_variant_builder_add (&builder, "{sv}", link->name,
                           g_variant_new ("(uu)", g_menu_exporter_group_get_id (link->menu->group), link->menu->id));

  return g_variant_builder_end (&builder);
}

static GVariant *
g_menu_exporter_menu_list (GMenuExporterMenu *menu)
{
  GVariantBuilder builder;
  gint i, n;

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("aa{sv}"));

  n = g_sequence_get_length (menu->item_links);
  for (i = 0; i < n; i++)
    g_variant_builder_add_value (&builder, g_menu_exporter_menu_describe_item (menu, i));

  return g_variant_builder_end (&builder);
}

static void
g_menu_exporter_menu_items_changed (GMenuModel *model,
                                    gint        position,
                                    gint        removed,
                                    gint        added,
                                    gpointer    user_data)
{
  GMenuExporterMenu *menu = user_data;
  GSequenceIter *point;
  gint i;

  g_assert (menu->model == model);
  g_assert (menu->item_links != NULL);
  g_assert (position + removed <= g_sequence_get_length (menu->item_links));

  point = g_sequence_get_iter_at_pos (menu->item_links, position + removed);
  g_sequence_remove_range (g_sequence_get_iter_at_pos (menu->item_links, position), point);

  for (i = position; i < position + added; i++)
    g_sequence_insert_before (point, g_menu_exporter_menu_create_links (menu, i));

  if (g_menu_exporter_group_is_subscribed (menu->group))
    {
      GVariantBuilder builder;

      g_variant_builder_init (&builder, G_VARIANT_TYPE ("(uuuuaa{sv})"));
      g_variant_builder_add (&builder, "u", g_menu_exporter_group_get_id (menu->group));
      g_variant_builder_add (&builder, "u", menu->id);
      g_variant_builder_add (&builder, "u", position);
      g_variant_builder_add (&builder, "u", removed);

      g_variant_builder_open (&builder, G_VARIANT_TYPE ("aa{sv}"));
      for (i = position; i < position + added; i++)
        g_variant_builder_add_value (&builder, g_menu_exporter_menu_describe_item (menu, i));
      g_variant_builder_close (&builder);

      g_menu_exporter_report (g_menu_exporter_group_get_exporter (menu->group), g_variant_builder_end (&builder));
    }
}

static void
g_menu_exporter_menu_prepare (GMenuExporterMenu *menu)
{
  gint n_items;

  g_assert (menu->item_links == NULL);

  if (g_menu_model_is_mutable (menu->model))
    menu->handler_id = g_signal_connect (menu->model, "items-changed",
                                         G_CALLBACK (g_menu_exporter_menu_items_changed), menu);

  menu->item_links = g_sequence_new (g_menu_exporter_link_free);

  n_items = g_menu_model_get_n_items (menu->model);
  if (n_items)
    g_menu_exporter_menu_items_changed (menu->model, 0, 0, n_items, menu);
}

static GMenuExporterMenu *
g_menu_exporter_menu_new (GMenuExporterGroup *group,
                          guint               id,
                          GMenuModel         *model)
{
  GMenuExporterMenu *menu;

  menu = g_slice_new0 (GMenuExporterMenu);
  menu->group = group;
  menu->id = id;
  menu->model = g_object_ref (model);

  return menu;
}

/* {{{1 GMenuExporterGroup */

struct _GMenuExporterGroup
{
  GMenuExporter *exporter;
  guint          id;

  GHashTable *menus;
  guint       next_menu_id;
  gboolean    prepared;

  gint subscribed;
};

static void
g_menu_exporter_group_check_if_useless (GMenuExporterGroup *group)
{
  if (g_hash_table_size (group->menus) == 0 && group->subscribed == 0)
    {
      g_menu_exporter_remove_group (group->exporter, group->id);

      g_hash_table_unref (group->menus);

      g_slice_free (GMenuExporterGroup, group);
    }
}

static void
g_menu_exporter_group_subscribe (GMenuExporterGroup *group,
                                 GVariantBuilder    *builder)
{
  GHashTableIter iter;
  gpointer key, val;

  if (!group->prepared)
    {
      GMenuExporterMenu *menu;

      /* set this first, so that any menus created during the
       * preparation of the first menu also end up in the prepared
       * state.
       * */
      group->prepared = TRUE;

      menu = g_hash_table_lookup (group->menus, 0);

      /* If the group was created by a subscription and does not yet
       * exist, it won't have a root menu...
       *
       * That menu will be prepared if it is ever added (due to
       * group->prepared == TRUE).
       */
      if (menu)
        g_menu_exporter_menu_prepare (menu);
    }

  group->subscribed++;

  g_hash_table_iter_init (&iter, group->menus);
  while (g_hash_table_iter_next (&iter, &key, &val))
    {
      guint id = GPOINTER_TO_INT (key);
      GMenuExporterMenu *menu = val;

      if (!g_sequence_is_empty (menu->item_links))
        {
          g_variant_builder_open (builder, G_VARIANT_TYPE ("(uuaa{sv})"));
          g_variant_builder_add (builder, "u", group->id);
          g_variant_builder_add (builder, "u", id);
          g_variant_builder_add_value (builder, g_menu_exporter_menu_list (menu));
          g_variant_builder_close (builder);
        }
    }
}

static void
g_menu_exporter_group_unsubscribe (GMenuExporterGroup *group,
                                   gint                count)
{
  g_assert (group->subscribed >= count);

  group->subscribed -= count;

  g_menu_exporter_group_check_if_useless (group);
}

static GMenuExporter *
g_menu_exporter_group_get_exporter (GMenuExporterGroup *group)
{
  return group->exporter;
}

static gboolean
g_menu_exporter_group_is_subscribed (GMenuExporterGroup *group)
{
  return group->subscribed > 0;
}

static guint
g_menu_exporter_group_get_id (GMenuExporterGroup *group)
{
  return group->id;
}

static void
g_menu_exporter_group_remove_menu (GMenuExporterGroup *group,
                                   guint               id)
{
  g_hash_table_remove (group->menus, GINT_TO_POINTER (id));

  g_menu_exporter_group_check_if_useless (group);
}

static GMenuExporterMenu *
g_menu_exporter_group_add_menu (GMenuExporterGroup *group,
                                GMenuModel         *model)
{
  GMenuExporterMenu *menu;
  guint id;

  id = group->next_menu_id++;
  menu = g_menu_exporter_menu_new (group, id, model);
  g_hash_table_insert (group->menus, GINT_TO_POINTER (id), menu);

  if (group->prepared)
    g_menu_exporter_menu_prepare (menu);

  return menu;
}

static GMenuExporterGroup *
g_menu_exporter_group_new (GMenuExporter *exporter,
                           guint          id)
{
  GMenuExporterGroup *group;

  group = g_slice_new0 (GMenuExporterGroup);
  group->menus = g_hash_table_new (NULL, NULL);
  group->exporter = exporter;
  group->id = id;

  return group;
}

/* {{{1 GMenuExporterRemote */

struct _GMenuExporterRemote
{
  GMenuExporter *exporter;
  GHashTable    *watches;
  guint          watch_id;
};

static void
g_menu_exporter_remote_subscribe (GMenuExporterRemote *remote,
                                  guint                group_id,
                                  GVariantBuilder     *builder)
{
  GMenuExporterGroup *group;
  guint count;

  count = (gsize) g_hash_table_lookup (remote->watches, GINT_TO_POINTER (group_id));
  g_hash_table_insert (remote->watches, GINT_TO_POINTER (group_id), GINT_TO_POINTER (count + 1));

  /* Group will be created (as empty/unsubscribed if it does not exist) */
  group = g_menu_exporter_lookup_group (remote->exporter, group_id);
  g_menu_exporter_group_subscribe (group, builder);
}

static void
g_menu_exporter_remote_unsubscribe (GMenuExporterRemote *remote,
                                    guint                group_id)
{
  GMenuExporterGroup *group;
  guint count;

  count = (gsize) g_hash_table_lookup (remote->watches, GINT_TO_POINTER (group_id));

  if (count == 0)
    return;

  if (count != 1)
    g_hash_table_insert (remote->watches, GINT_TO_POINTER (group_id), GINT_TO_POINTER (count - 1));
  else
    g_hash_table_remove (remote->watches, GINT_TO_POINTER (group_id));

  group = g_menu_exporter_lookup_group (remote->exporter, group_id);
  g_menu_exporter_group_unsubscribe (group, 1);
}

static gboolean
g_menu_exporter_remote_has_subscriptions (GMenuExporterRemote *remote)
{
  return g_hash_table_size (remote->watches) != 0;
}

static void
g_menu_exporter_remote_free (gpointer data)
{
  GMenuExporterRemote *remote = data;
  GHashTableIter iter;
  gpointer key, val;

  g_hash_table_iter_init (&iter, remote->watches);
  while (g_hash_table_iter_next (&iter, &key, &val))
    {
      GMenuExporterGroup *group;

      group = g_menu_exporter_lookup_group (remote->exporter, GPOINTER_TO_INT (key));
      g_menu_exporter_group_unsubscribe (group, GPOINTER_TO_INT (val));
    }

  if (remote->watch_id > 0)
    g_bus_unwatch_name (remote->watch_id);

  g_hash_table_unref (remote->watches);

  g_slice_free (GMenuExporterRemote, remote);
}

static GMenuExporterRemote *
g_menu_exporter_remote_new (GMenuExporter *exporter,
                            guint          watch_id)
{
  GMenuExporterRemote *remote;

  remote = g_slice_new0 (GMenuExporterRemote);
  remote->exporter = exporter;
  remote->watches = g_hash_table_new (NULL, NULL);
  remote->watch_id = watch_id;

  return remote;
}

/* {{{1 GMenuExporter */

struct _GMenuExporter
{
  GDBusConnection *connection;
  gchar *object_path;
  guint registration_id;
  GHashTable *groups;
  guint next_group_id;

  GMenuExporterMenu *root;
  GMenuExporterRemote *peer_remote;
  GHashTable *remotes;
};

static void
g_menu_exporter_name_vanished (GDBusConnection *connection,
                               const gchar     *name,
                               gpointer         user_data)
{
  GMenuExporter *exporter = user_data;

  /* connection == NULL when we get called because the connection closed */
  g_assert (exporter->connection == connection || connection == NULL);

  g_hash_table_remove (exporter->remotes, name);
}

static GVariant *
g_menu_exporter_subscribe (GMenuExporter *exporter,
                           const gchar   *sender,
                           GVariant      *group_ids)
{
  GMenuExporterRemote *remote;
  GVariantBuilder builder;
  GVariantIter iter;
  guint32 id;

  if (sender != NULL)
    remote = g_hash_table_lookup (exporter->remotes, sender);
  else
    remote = exporter->peer_remote;

  if (remote == NULL)
    {
      if (sender != NULL)
        {
          guint watch_id;

          watch_id = g_bus_watch_name_on_connection (exporter->connection, sender, G_BUS_NAME_WATCHER_FLAGS_NONE,
                                                     NULL, g_menu_exporter_name_vanished, exporter, NULL);
          remote = g_menu_exporter_remote_new (exporter, watch_id);
          g_hash_table_insert (exporter->remotes, g_strdup (sender), remote);
        }
      else
        remote = exporter->peer_remote =
          g_menu_exporter_remote_new (exporter, 0);
    }

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("(a(uuaa{sv}))"));

  g_variant_builder_open (&builder, G_VARIANT_TYPE ("a(uuaa{sv})"));

  g_variant_iter_init (&iter, group_ids);
  while (g_variant_iter_next (&iter, "u", &id))
    g_menu_exporter_remote_subscribe (remote, id, &builder);

  g_variant_builder_close (&builder);

  return g_variant_builder_end (&builder);
}

static void
g_menu_exporter_unsubscribe (GMenuExporter *exporter,
                             const gchar   *sender,
                             GVariant      *group_ids)
{
  GMenuExporterRemote *remote;
  GVariantIter iter;
  guint32 id;

  if (sender != NULL)
    remote = g_hash_table_lookup (exporter->remotes, sender);
  else
    remote = exporter->peer_remote;

  if (remote == NULL)
    return;

  g_variant_iter_init (&iter, group_ids);
  while (g_variant_iter_next (&iter, "u", &id))
    g_menu_exporter_remote_unsubscribe (remote, id);

  if (!g_menu_exporter_remote_has_subscriptions (remote))
    {
      if (sender != NULL)
        g_hash_table_remove (exporter->remotes, sender);
      else
        g_clear_pointer (&exporter->peer_remote, g_menu_exporter_remote_free);
    }
}

static void
g_menu_exporter_report (GMenuExporter *exporter,
                        GVariant      *report)
{
  GVariantBuilder builder;

  g_variant_builder_init (&builder, G_VARIANT_TYPE_TUPLE);
  g_variant_builder_open (&builder, G_VARIANT_TYPE_ARRAY);
  g_variant_builder_add_value (&builder, report);
  g_variant_builder_close (&builder);

  g_dbus_connection_emit_signal (exporter->connection,
                                 NULL,
                                 exporter->object_path,
                                 "org.gtk.Menus", "Changed",
                                 g_variant_builder_end (&builder),
                                 NULL);
}

static void
g_menu_exporter_remove_group (GMenuExporter *exporter,
                              guint          id)
{
  g_hash_table_remove (exporter->groups, GINT_TO_POINTER (id));
}

static GMenuExporterGroup *
g_menu_exporter_lookup_group (GMenuExporter *exporter,
                              guint          group_id)
{
  GMenuExporterGroup *group;

  group = g_hash_table_lookup (exporter->groups, GINT_TO_POINTER (group_id));

  if (group == NULL)
    {
      group = g_menu_exporter_group_new (exporter, group_id);
      g_hash_table_insert (exporter->groups, GINT_TO_POINTER (group_id), group);
    }

  return group;
}

static GMenuExporterGroup *
g_menu_exporter_create_group (GMenuExporter *exporter)
{
  GMenuExporterGroup *group;
  guint id;

  id = exporter->next_group_id++;
  group = g_menu_exporter_group_new (exporter, id);
  g_hash_table_insert (exporter->groups, GINT_TO_POINTER (id), group);

  return group;
}

static void
g_menu_exporter_free (gpointer user_data)
{
  GMenuExporter *exporter = user_data;

  g_menu_exporter_menu_free (exporter->root);
  g_clear_pointer (&exporter->peer_remote, g_menu_exporter_remote_free);
  g_hash_table_unref (exporter->remotes);
  g_hash_table_unref (exporter->groups);
  g_object_unref (exporter->connection);
  g_free (exporter->object_path);

  g_slice_free (GMenuExporter, exporter);
}

static void
g_menu_exporter_method_call (GDBusConnection       *connection,
                             const gchar           *sender,
                             const gchar           *object_path,
                             const gchar           *interface_name,
                             const gchar           *method_name,
                             GVariant              *parameters,
                             GDBusMethodInvocation *invocation,
                             gpointer               user_data)
{
  GMenuExporter *exporter = user_data;
  GVariant *group_ids;

  group_ids = g_variant_get_child_value (parameters, 0);

  if (g_str_equal (method_name, "Start"))
    g_dbus_method_invocation_return_value (invocation, g_menu_exporter_subscribe (exporter, sender, group_ids));

  else if (g_str_equal (method_name, "End"))
    {
      g_menu_exporter_unsubscribe (exporter, sender, group_ids);
      g_dbus_method_invocation_return_value (invocation, NULL);
    }

  else
    g_assert_not_reached ();

  g_variant_unref (group_ids);
}

/* {{{1 Public API */

/**
 * g_dbus_connection_export_menu_model:
 * @connection: a #GDBusConnection
 * @object_path: a D-Bus object path
 * @menu: a #GMenuModel
 * @error: return location for an error, or %NULL
 *
 * Exports @menu on @connection at @object_path.
 *
 * The implemented D-Bus API should be considered private.
 * It is subject to change in the future.
 *
 * An object path can only have one menu model exported on it. If this
 * constraint is violated, the export will fail and 0 will be
 * returned (with @error set accordingly).
 *
 * You can unexport the menu model using
 * g_dbus_connection_unexport_menu_model() with the return value of
 * this function.
 *
 * Returns: the ID of the export (never zero), or 0 in case of failure
 *
 * Since: 2.32
 */
guint
g_dbus_connection_export_menu_model (GDBusConnection  *connection,
                                     const gchar      *object_path,
                                     GMenuModel       *menu,
                                     GError          **error)
{
  const GDBusInterfaceVTable vtable = {
    g_menu_exporter_method_call,
  };
  GMenuExporter *exporter;
  guint id;

  exporter = g_slice_new0 (GMenuExporter);

  id = g_dbus_connection_register_object (connection, object_path, org_gtk_Menus_get_interface (),
                                          &vtable, exporter, g_menu_exporter_free, error);

  if (id == 0)
    {
      g_slice_free (GMenuExporter, exporter);
      return 0;
    }

  exporter->connection = g_object_ref (connection);
  exporter->object_path = g_strdup (object_path);
  exporter->groups = g_hash_table_new (NULL, NULL);
  exporter->remotes = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_menu_exporter_remote_free);
  exporter->root = g_menu_exporter_group_add_menu (g_menu_exporter_create_group (exporter), menu);

  return id;
}

/**
 * g_dbus_connection_unexport_menu_model:
 * @connection: a #GDBusConnection
 * @export_id: the ID from g_dbus_connection_export_menu_model()
 *
 * Reverses the effect of a previous call to
 * g_dbus_connection_export_menu_model().
 *
 * It is an error to call this function with an ID that wasn't returned
 * from g_dbus_connection_export_menu_model() or to call it with the
 * same ID more than once.
 *
 * Since: 2.32
 */
void
g_dbus_connection_unexport_menu_model (GDBusConnection *connection,
                                       guint            export_id)
{
  g_dbus_connection_unregister_object (connection, export_id);
}

/* {{{1 Epilogue */
/* vim:set foldmethod=marker: */
