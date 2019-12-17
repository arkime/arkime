/*
 * Copyright © 2011 Canonical Ltd.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Ryan Lortie <desrt@desrt.ca>
 */

#include "config.h"

#include "gmenumodel.h"

#include "glibintl.h"

/**
 * SECTION:gmenumodel
 * @title: GMenuModel
 * @short_description: An abstract class representing the contents of a menu
 * @include: gio/gio.h
 * @see_also: #GActionGroup
 *
 * #GMenuModel represents the contents of a menu -- an ordered list of
 * menu items. The items are associated with actions, which can be
 * activated through them. Items can be grouped in sections, and may
 * have submenus associated with them. Both items and sections usually
 * have some representation data, such as labels or icons. The type of
 * the associated action (ie whether it is stateful, and what kind of
 * state it has) can influence the representation of the item.
 *
 * The conceptual model of menus in #GMenuModel is hierarchical:
 * sections and submenus are again represented by #GMenuModels.
 * Menus themselves do not define their own roles. Rather, the role
 * of a particular #GMenuModel is defined by the item that references
 * it (or, in the case of the 'root' menu, is defined by the context
 * in which it is used).
 *
 * As an example, consider the visible portions of this menu:
 *
 * ## An example menu # {#menu-example}
 *
 * ![](menu-example.png)
 *
 * There are 8 "menus" visible in the screenshot: one menubar, two
 * submenus and 5 sections:
 *
 * - the toplevel menubar (containing 4 items)
 * - the View submenu (containing 3 sections)
 * - the first section of the View submenu (containing 2 items)
 * - the second section of the View submenu (containing 1 item)
 * - the final section of the View submenu (containing 1 item)
 * - the Highlight Mode submenu (containing 2 sections)
 * - the Sources section (containing 2 items)
 * - the Markup section (containing 2 items)
 *
 * The [example][menu-model] illustrates the conceptual connection between
 * these 8 menus. Each large block in the figure represents a menu and the
 * smaller blocks within the large block represent items in that menu. Some
 * items contain references to other menus.
 *
 * ## A menu example # {#menu-model}
 *
 * ![](menu-model.png)
 *
 * Notice that the separators visible in the [example][menu-example]
 * appear nowhere in the [menu model][menu-model]. This is because
 * separators are not explicitly represented in the menu model. Instead,
 * a separator is inserted between any two non-empty sections of a menu.
 * Section items can have labels just like any other item. In that case,
 * a display system may show a section header instead of a separator.
 *
 * The motivation for this abstract model of application controls is
 * that modern user interfaces tend to make these controls available
 * outside the application. Examples include global menus, jumplists,
 * dash boards, etc. To support such uses, it is necessary to 'export'
 * information about actions and their representation in menus, which
 * is exactly what the [GActionGroup exporter][gio-GActionGroup-exporter]
 * and the [GMenuModel exporter][gio-GMenuModel-exporter] do for
 * #GActionGroup and #GMenuModel. The client-side counterparts to
 * make use of the exported information are #GDBusActionGroup and
 * #GDBusMenuModel.
 *
 * The API of #GMenuModel is very generic, with iterators for the
 * attributes and links of an item, see g_menu_model_iterate_item_attributes()
 * and g_menu_model_iterate_item_links(). The 'standard' attributes and
 * link types have predefined names: %G_MENU_ATTRIBUTE_LABEL,
 * %G_MENU_ATTRIBUTE_ACTION, %G_MENU_ATTRIBUTE_TARGET, %G_MENU_LINK_SECTION
 * and %G_MENU_LINK_SUBMENU.
 *
 * Items in a #GMenuModel represent active controls if they refer to
 * an action that can get activated when the user interacts with the
 * menu item. The reference to the action is encoded by the string id
 * in the %G_MENU_ATTRIBUTE_ACTION attribute. An action id uniquely
 * identifies an action in an action group. Which action group(s) provide
 * actions depends on the context in which the menu model is used.
 * E.g. when the model is exported as the application menu of a
 * #GtkApplication, actions can be application-wide or window-specific
 * (and thus come from two different action groups). By convention, the
 * application-wide actions have names that start with "app.", while the
 * names of window-specific actions start with "win.".
 *
 * While a wide variety of stateful actions is possible, the following
 * is the minimum that is expected to be supported by all users of exported
 * menu information:
 * - an action with no parameter type and no state
 * - an action with no parameter type and boolean state
 * - an action with string parameter type and string state
 *
 * ## Stateless 
 *
 * A stateless action typically corresponds to an ordinary menu item.
 *
 * Selecting such a menu item will activate the action (with no parameter).
 *
 * ## Boolean State
 *
 * An action with a boolean state will most typically be used with a "toggle"
 * or "switch" menu item. The state can be set directly, but activating the
 * action (with no parameter) results in the state being toggled.
 *
 * Selecting a toggle menu item will activate the action. The menu item should
 * be rendered as "checked" when the state is true.
 *
 * ## String Parameter and State
 *
 * Actions with string parameters and state will most typically be used to
 * represent an enumerated choice over the items available for a group of
 * radio menu items. Activating the action with a string parameter is
 * equivalent to setting that parameter as the state.
 *
 * Radio menu items, in addition to being associated with the action, will
 * have a target value. Selecting that menu item will result in activation
 * of the action with the target value as the parameter. The menu item should
 * be rendered as "selected" when the state of the action is equal to the
 * target value of the menu item.
 */

/**
 * GMenuModel:
 *
 * #GMenuModel is an opaque structure type.  You must access it using the
 * functions below.
 *
 * Since: 2.32
 */

/**
 * GMenuAttributeIter:
 *
 * #GMenuAttributeIter is an opaque structure type.  You must access it
 * using the functions below.
 *
 * Since: 2.32
 */

/**
 * GMenuLinkIter:
 *
 * #GMenuLinkIter is an opaque structure type.  You must access it using
 * the functions below.
 *
 * Since: 2.32
 */

typedef struct
{
  GMenuLinkIter parent_instance;
  GHashTableIter iter;
  GHashTable *table;
} GMenuLinkHashIter;

typedef GMenuLinkIterClass GMenuLinkHashIterClass;

static GType g_menu_link_hash_iter_get_type (void);

G_DEFINE_TYPE (GMenuLinkHashIter, g_menu_link_hash_iter, G_TYPE_MENU_LINK_ITER)

static gboolean
g_menu_link_hash_iter_get_next (GMenuLinkIter  *link_iter,
                                const gchar   **out_name,
                                GMenuModel    **value)
{
  GMenuLinkHashIter *iter = (GMenuLinkHashIter *) link_iter;
  gpointer keyptr, valueptr;

  if (!g_hash_table_iter_next (&iter->iter, &keyptr, &valueptr))
    return FALSE;

  *out_name = keyptr;
  *value = g_object_ref (valueptr);

  return TRUE;
}

static void
g_menu_link_hash_iter_finalize (GObject *object)
{
  GMenuLinkHashIter *iter = (GMenuLinkHashIter *) object;

  g_hash_table_unref (iter->table);

  G_OBJECT_CLASS (g_menu_link_hash_iter_parent_class)
    ->finalize (object);
}

static void
g_menu_link_hash_iter_init (GMenuLinkHashIter *iter)
{
}

static void
g_menu_link_hash_iter_class_init (GMenuLinkHashIterClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = g_menu_link_hash_iter_finalize;
  class->get_next = g_menu_link_hash_iter_get_next;
}


typedef struct
{
  GMenuAttributeIter parent_instance;
  GHashTableIter iter;
  GHashTable *table;
} GMenuAttributeHashIter;

typedef GMenuAttributeIterClass GMenuAttributeHashIterClass;

static GType g_menu_attribute_hash_iter_get_type (void);

G_DEFINE_TYPE (GMenuAttributeHashIter, g_menu_attribute_hash_iter, G_TYPE_MENU_ATTRIBUTE_ITER)

static gboolean
g_menu_attribute_hash_iter_get_next (GMenuAttributeIter  *attr_iter,
                                     const gchar        **name,
                                     GVariant           **value)
{
  GMenuAttributeHashIter *iter = (GMenuAttributeHashIter *) attr_iter;
  gpointer keyptr, valueptr;

  if (!g_hash_table_iter_next (&iter->iter, &keyptr, &valueptr))
    return FALSE;

  *name = keyptr;

  *value = g_variant_ref (valueptr);

  return TRUE;
}

static void
g_menu_attribute_hash_iter_finalize (GObject *object)
{
  GMenuAttributeHashIter *iter = (GMenuAttributeHashIter *) object;

  g_hash_table_unref (iter->table);

  G_OBJECT_CLASS (g_menu_attribute_hash_iter_parent_class)
    ->finalize (object);
}

static void
g_menu_attribute_hash_iter_init (GMenuAttributeHashIter *iter)
{
}

static void
g_menu_attribute_hash_iter_class_init (GMenuAttributeHashIterClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = g_menu_attribute_hash_iter_finalize;
  class->get_next = g_menu_attribute_hash_iter_get_next;
}

G_DEFINE_ABSTRACT_TYPE (GMenuModel, g_menu_model, G_TYPE_OBJECT)


static guint g_menu_model_items_changed_signal;

static GMenuAttributeIter *
g_menu_model_real_iterate_item_attributes (GMenuModel *model,
                                           gint        item_index)
{
  GHashTable *table = NULL;
  GMenuAttributeIter *result;

  G_MENU_MODEL_GET_CLASS (model)->get_item_attributes (model, item_index, &table);

  if (table)
    {
      GMenuAttributeHashIter *iter = g_object_new (g_menu_attribute_hash_iter_get_type (), NULL);
      g_hash_table_iter_init (&iter->iter, table);
      iter->table = g_hash_table_ref (table);
      result = G_MENU_ATTRIBUTE_ITER (iter);
    }
  else
    {
      g_critical ("GMenuModel implementation '%s' doesn't override iterate_item_attributes() "
                  "and fails to return sane values from get_item_attributes()",
                  G_OBJECT_TYPE_NAME (model));
      result = NULL;
    }

  if (table != NULL)
    g_hash_table_unref (table);

  return result;
}

static GVariant *
g_menu_model_real_get_item_attribute_value (GMenuModel         *model,
                                            gint                item_index,
                                            const gchar        *attribute,
                                            const GVariantType *expected_type)
{
  GHashTable *table = NULL;
  GVariant *value = NULL;

  G_MENU_MODEL_GET_CLASS (model)
    ->get_item_attributes (model, item_index, &table);

  if (table != NULL)
    {
      value = g_hash_table_lookup (table, attribute);

      if (value != NULL)
        {
          if (expected_type == NULL || g_variant_is_of_type (value, expected_type))
            value = g_variant_ref (value);
          else
            value = NULL;
        }
    }
  else
    g_assert_not_reached ();

  if (table != NULL)
    g_hash_table_unref (table);

  return value;
}

static GMenuLinkIter *
g_menu_model_real_iterate_item_links (GMenuModel *model,
                                      gint        item_index)
{
  GHashTable *table = NULL;
  GMenuLinkIter *result;

  G_MENU_MODEL_GET_CLASS (model)
    ->get_item_links (model, item_index, &table);

  if (table)
    {
      GMenuLinkHashIter *iter = g_object_new (g_menu_link_hash_iter_get_type (), NULL);
      g_hash_table_iter_init (&iter->iter, table);
      iter->table = g_hash_table_ref (table);
      result = G_MENU_LINK_ITER (iter);
    }
  else
    {
      g_critical ("GMenuModel implementation '%s' doesn't override iterate_item_links() "
                  "and fails to return sane values from get_item_links()",
                  G_OBJECT_TYPE_NAME (model));
      result = NULL;
    }

  if (table != NULL)
    g_hash_table_unref (table);

  return result;
}

static GMenuModel *
g_menu_model_real_get_item_link (GMenuModel  *model,
                                 gint         item_index,
                                 const gchar *link)
{
  GHashTable *table = NULL;
  GMenuModel *value = NULL;

  G_MENU_MODEL_GET_CLASS (model)
    ->get_item_links (model, item_index, &table);

  if (table != NULL)
    value = g_hash_table_lookup (table, link);
  else
    g_assert_not_reached ();

  if (value != NULL)
    g_object_ref (value);

  if (table != NULL)
    g_hash_table_unref (table);

  return value;
}

static void
g_menu_model_init (GMenuModel *model)
{
}

static void
g_menu_model_class_init (GMenuModelClass *class)
{
  class->iterate_item_attributes = g_menu_model_real_iterate_item_attributes;
  class->get_item_attribute_value = g_menu_model_real_get_item_attribute_value;
  class->iterate_item_links = g_menu_model_real_iterate_item_links;
  class->get_item_link = g_menu_model_real_get_item_link;

  /**
   * GMenuModel::items-changed:
   * @model: the #GMenuModel that is changing
   * @position: the position of the change
   * @removed: the number of items removed
   * @added: the number of items added
   *
   * Emitted when a change has occured to the menu.
   *
   * The only changes that can occur to a menu is that items are removed
   * or added.  Items may not change (except by being removed and added
   * back in the same location).  This signal is capable of describing
   * both of those changes (at the same time).
   *
   * The signal means that starting at the index @position, @removed
   * items were removed and @added items were added in their place.  If
   * @removed is zero then only items were added.  If @added is zero
   * then only items were removed.
   *
   * As an example, if the menu contains items a, b, c, d (in that
   * order) and the signal (2, 1, 3) occurs then the new composition of
   * the menu will be a, b, _, _, _, d (with each _ representing some
   * new item).
   *
   * Signal handlers may query the model (particularly the added items)
   * and expect to see the results of the modification that is being
   * reported.  The signal is emitted after the modification.
   **/
  g_menu_model_items_changed_signal =
    g_signal_new (I_("items-changed"), G_TYPE_MENU_MODEL,
                  G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                  g_cclosure_marshal_generic, G_TYPE_NONE,
                  3, G_TYPE_INT, G_TYPE_INT, G_TYPE_INT);
}

/**
 * g_menu_model_is_mutable:
 * @model: a #GMenuModel
 *
 * Queries if @model is mutable.
 *
 * An immutable #GMenuModel will never emit the #GMenuModel::items-changed
 * signal. Consumers of the model may make optimisations accordingly.
 *
 * Returns: %TRUE if the model is mutable (ie: "items-changed" may be
 *     emitted).
 *
 * Since: 2.32
 */
gboolean
g_menu_model_is_mutable (GMenuModel *model)
{
  return G_MENU_MODEL_GET_CLASS (model)
    ->is_mutable (model);
}

/**
 * g_menu_model_get_n_items:
 * @model: a #GMenuModel
 *
 * Query the number of items in @model.
 *
 * Returns: the number of items
 *
 * Since: 2.32
 */
gint
g_menu_model_get_n_items (GMenuModel *model)
{
  return G_MENU_MODEL_GET_CLASS (model)
    ->get_n_items (model);
}

/**
 * g_menu_model_iterate_item_attributes:
 * @model: a #GMenuModel
 * @item_index: the index of the item
 *
 * Creates a #GMenuAttributeIter to iterate over the attributes of
 * the item at position @item_index in @model.
 *
 * You must free the iterator with g_object_unref() when you are done.
 *
 * Returns: (transfer full): a new #GMenuAttributeIter
 *
 * Since: 2.32
 */
GMenuAttributeIter *
g_menu_model_iterate_item_attributes (GMenuModel *model,
                                      gint        item_index)
{
  return G_MENU_MODEL_GET_CLASS (model)
    ->iterate_item_attributes (model, item_index);
}

/**
 * g_menu_model_get_item_attribute_value:
 * @model: a #GMenuModel
 * @item_index: the index of the item
 * @attribute: the attribute to query
 * @expected_type: (nullable): the expected type of the attribute, or
 *     %NULL
 *
 * Queries the item at position @item_index in @model for the attribute
 * specified by @attribute.
 *
 * If @expected_type is non-%NULL then it specifies the expected type of
 * the attribute.  If it is %NULL then any type will be accepted.
 *
 * If the attribute exists and matches @expected_type (or if the
 * expected type is unspecified) then the value is returned.
 *
 * If the attribute does not exist, or does not match the expected type
 * then %NULL is returned.
 *
 * Returns: (transfer full): the value of the attribute
 *
 * Since: 2.32
 */
GVariant *
g_menu_model_get_item_attribute_value (GMenuModel         *model,
                                       gint                item_index,
                                       const gchar        *attribute,
                                       const GVariantType *expected_type)
{
  return G_MENU_MODEL_GET_CLASS (model)
    ->get_item_attribute_value (model, item_index, attribute, expected_type);
}

/**
 * g_menu_model_get_item_attribute:
 * @model: a #GMenuModel
 * @item_index: the index of the item
 * @attribute: the attribute to query
 * @format_string: a #GVariant format string
 * @...: positional parameters, as per @format_string
 *
 * Queries item at position @item_index in @model for the attribute
 * specified by @attribute.
 *
 * If the attribute exists and matches the #GVariantType corresponding
 * to @format_string then @format_string is used to deconstruct the
 * value into the positional parameters and %TRUE is returned.
 *
 * If the attribute does not exist, or it does exist but has the wrong
 * type, then the positional parameters are ignored and %FALSE is
 * returned.
 *
 * This function is a mix of g_menu_model_get_item_attribute_value() and
 * g_variant_get(), followed by a g_variant_unref().  As such,
 * @format_string must make a complete copy of the data (since the
 * #GVariant may go away after the call to g_variant_unref()).  In
 * particular, no '&' characters are allowed in @format_string.
 *
 * Returns: %TRUE if the named attribute was found with the expected
 *     type
 *
 * Since: 2.32
 */
gboolean
g_menu_model_get_item_attribute (GMenuModel  *model,
                                 gint         item_index,
                                 const gchar *attribute,
                                 const gchar *format_string,
                                 ...)
{
  GVariant *value;
  va_list ap;

  value = g_menu_model_get_item_attribute_value (model, item_index, attribute, NULL);

  if (value == NULL)
    return FALSE;

  if (!g_variant_check_format_string (value, format_string, TRUE))
    {
      g_variant_unref (value);
      return FALSE;
    }

  va_start (ap, format_string);
  g_variant_get_va (value, format_string, NULL, &ap);
  g_variant_unref (value);
  va_end (ap);

  return TRUE;
}

/**
 * g_menu_model_iterate_item_links:
 * @model: a #GMenuModel
 * @item_index: the index of the item
 *
 * Creates a #GMenuLinkIter to iterate over the links of the item at
 * position @item_index in @model.
 *
 * You must free the iterator with g_object_unref() when you are done.
 *
 * Returns: (transfer full): a new #GMenuLinkIter
 *
 * Since: 2.32
 */
GMenuLinkIter *
g_menu_model_iterate_item_links (GMenuModel *model,
                                 gint        item_index)
{
  return G_MENU_MODEL_GET_CLASS (model)
    ->iterate_item_links (model, item_index);
}

/**
 * g_menu_model_get_item_link:
 * @model: a #GMenuModel
 * @item_index: the index of the item
 * @link: the link to query
 *
 * Queries the item at position @item_index in @model for the link
 * specified by @link.
 *
 * If the link exists, the linked #GMenuModel is returned.  If the link
 * does not exist, %NULL is returned.
 *
 * Returns: (transfer full): the linked #GMenuModel, or %NULL
 *
 * Since: 2.32
 */
GMenuModel *
g_menu_model_get_item_link (GMenuModel *model,
                            gint        item_index,
                            const gchar *link)
{
  return G_MENU_MODEL_GET_CLASS (model)
    ->get_item_link (model, item_index, link);
}

/**
 * g_menu_model_items_changed:
 * @model: a #GMenuModel
 * @position: the position of the change
 * @removed: the number of items removed
 * @added: the number of items added
 *
 * Requests emission of the #GMenuModel::items-changed signal on @model.
 *
 * This function should never be called except by #GMenuModel
 * subclasses.  Any other calls to this function will very likely lead
 * to a violation of the interface of the model.
 *
 * The implementation should update its internal representation of the
 * menu before emitting the signal.  The implementation should further
 * expect to receive queries about the new state of the menu (and
 * particularly added menu items) while signal handlers are running.
 *
 * The implementation must dispatch this call directly from a mainloop
 * entry and not in response to calls -- particularly those from the
 * #GMenuModel API.  Said another way: the menu must not change while
 * user code is running without returning to the mainloop.
 *
 * Since: 2.32
 */
void
g_menu_model_items_changed (GMenuModel *model,
                            gint        position,
                            gint        removed,
                            gint        added)
{
  g_signal_emit (model, g_menu_model_items_changed_signal, 0, position, removed, added);
}

struct _GMenuAttributeIterPrivate
{
  GQuark name;
  GVariant *value;
  gboolean valid;
};

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (GMenuAttributeIter, g_menu_attribute_iter, G_TYPE_OBJECT)

/**
 * g_menu_attribute_iter_get_next:
 * @iter: a #GMenuAttributeIter
 * @out_name: (out) (optional) (transfer none): the type of the attribute
 * @value: (out) (optional) (transfer full): the attribute value
 *
 * This function combines g_menu_attribute_iter_next() with
 * g_menu_attribute_iter_get_name() and g_menu_attribute_iter_get_value().
 *
 * First the iterator is advanced to the next (possibly first) attribute.
 * If that fails, then %FALSE is returned and there are no other
 * effects.
 *
 * If successful, @name and @value are set to the name and value of the
 * attribute that has just been advanced to.  At this point,
 * g_menu_attribute_iter_get_name() and g_menu_attribute_iter_get_value() will
 * return the same values again.
 *
 * The value returned in @name remains valid for as long as the iterator
 * remains at the current position.  The value returned in @value must
 * be unreffed using g_variant_unref() when it is no longer in use.
 *
 * Returns: %TRUE on success, or %FALSE if there is no additional
 *     attribute
 *
 * Since: 2.32
 */
gboolean
g_menu_attribute_iter_get_next (GMenuAttributeIter  *iter,
                                const gchar        **out_name,
                                GVariant           **value)
{
  const gchar *name;

  if (iter->priv->value)
    {
      g_variant_unref (iter->priv->value);
      iter->priv->value = NULL;
    }

  iter->priv->valid = G_MENU_ATTRIBUTE_ITER_GET_CLASS (iter)
    ->get_next (iter, &name, &iter->priv->value);

  if (iter->priv->valid)
    {
      iter->priv->name = g_quark_from_string (name);
      if (out_name)
        *out_name = g_quark_to_string (iter->priv->name);

      if (value)
        *value = g_variant_ref (iter->priv->value);
    }

  return iter->priv->valid;
}

/**
 * g_menu_attribute_iter_next:
 * @iter: a #GMenuAttributeIter
 *
 * Attempts to advance the iterator to the next (possibly first)
 * attribute.
 *
 * %TRUE is returned on success, or %FALSE if there are no more
 * attributes.
 *
 * You must call this function when you first acquire the iterator
 * to advance it to the first attribute (and determine if the first
 * attribute exists at all).
 *
 * Returns: %TRUE on success, or %FALSE when there are no more attributes
 *
 * Since: 2.32
 */
gboolean
g_menu_attribute_iter_next (GMenuAttributeIter *iter)
{
  return g_menu_attribute_iter_get_next (iter, NULL, NULL);
}

/**
 * g_menu_attribute_iter_get_name:
 * @iter: a #GMenuAttributeIter
 *
 * Gets the name of the attribute at the current iterator position, as
 * a string.
 *
 * The iterator is not advanced.
 *
 * Returns: the name of the attribute
 *
 * Since: 2.32
 */
const gchar *
g_menu_attribute_iter_get_name (GMenuAttributeIter *iter)
{
  g_return_val_if_fail (iter->priv->valid, 0);

  return g_quark_to_string (iter->priv->name);
}

/**
 * g_menu_attribute_iter_get_value:
 * @iter: a #GMenuAttributeIter
 *
 * Gets the value of the attribute at the current iterator position.
 *
 * The iterator is not advanced.
 *
 * Returns: (transfer full): the value of the current attribute
 *
 * Since: 2.32
 */
GVariant *
g_menu_attribute_iter_get_value (GMenuAttributeIter *iter)
{
  g_return_val_if_fail (iter->priv->valid, NULL);

  return g_variant_ref (iter->priv->value);
}

static void
g_menu_attribute_iter_finalize (GObject *object)
{
  GMenuAttributeIter *iter = G_MENU_ATTRIBUTE_ITER (object);

  if (iter->priv->value)
    g_variant_unref (iter->priv->value);

  G_OBJECT_CLASS (g_menu_attribute_iter_parent_class)
    ->finalize (object);
}

static void
g_menu_attribute_iter_init (GMenuAttributeIter *iter)
{
  iter->priv = g_menu_attribute_iter_get_instance_private (iter);
}

static void
g_menu_attribute_iter_class_init (GMenuAttributeIterClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = g_menu_attribute_iter_finalize;
}

struct _GMenuLinkIterPrivate
{
  GQuark name;
  GMenuModel *value;
  gboolean valid;
};

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (GMenuLinkIter, g_menu_link_iter, G_TYPE_OBJECT)

/**
 * g_menu_link_iter_get_next:
 * @iter: a #GMenuLinkIter
 * @out_link: (out) (optional) (transfer none): the name of the link
 * @value: (out) (optional) (transfer full): the linked #GMenuModel
 *
 * This function combines g_menu_link_iter_next() with
 * g_menu_link_iter_get_name() and g_menu_link_iter_get_value().
 *
 * First the iterator is advanced to the next (possibly first) link.
 * If that fails, then %FALSE is returned and there are no other effects.
 *
 * If successful, @out_link and @value are set to the name and #GMenuModel
 * of the link that has just been advanced to.  At this point,
 * g_menu_link_iter_get_name() and g_menu_link_iter_get_value() will return the
 * same values again.
 *
 * The value returned in @out_link remains valid for as long as the iterator
 * remains at the current position.  The value returned in @value must
 * be unreffed using g_object_unref() when it is no longer in use.
 *
 * Returns: %TRUE on success, or %FALSE if there is no additional link
 *
 * Since: 2.32
 */
gboolean
g_menu_link_iter_get_next (GMenuLinkIter  *iter,
                           const gchar   **out_link,
                           GMenuModel    **value)
{
  const gchar *name;

  if (iter->priv->value)
    {
      g_object_unref (iter->priv->value);
      iter->priv->value = NULL;
    }

  iter->priv->valid = G_MENU_LINK_ITER_GET_CLASS (iter)
    ->get_next (iter, &name, &iter->priv->value);

  if (iter->priv->valid)
    {
      g_assert (name != NULL);

      iter->priv->name = g_quark_from_string (name);
      if (out_link)
        *out_link = g_quark_to_string (iter->priv->name);

      if (value)
        *value = g_object_ref (iter->priv->value);
    }

  return iter->priv->valid;
}

/**
 * g_menu_link_iter_next:
 * @iter: a #GMenuLinkIter
 *
 * Attempts to advance the iterator to the next (possibly first)
 * link.
 *
 * %TRUE is returned on success, or %FALSE if there are no more links.
 *
 * You must call this function when you first acquire the iterator to
 * advance it to the first link (and determine if the first link exists
 * at all).
 *
 * Returns: %TRUE on success, or %FALSE when there are no more links
 *
 * Since: 2.32
 */
gboolean
g_menu_link_iter_next (GMenuLinkIter *iter)
{
  return g_menu_link_iter_get_next (iter, NULL, NULL);
}

/**
 * g_menu_link_iter_get_name:
 * @iter: a #GMenuLinkIter
 *
 * Gets the name of the link at the current iterator position.
 *
 * The iterator is not advanced.
 *
 * Returns: the type of the link
 *
 * Since: 2.32
 */
const gchar *
g_menu_link_iter_get_name (GMenuLinkIter *iter)
{
  g_return_val_if_fail (iter->priv->valid, 0);

  return g_quark_to_string (iter->priv->name);
}

/**
 * g_menu_link_iter_get_value:
 * @iter: a #GMenuLinkIter
 *
 * Gets the linked #GMenuModel at the current iterator position.
 *
 * The iterator is not advanced.
 *
 * Returns: (transfer full): the #GMenuModel that is linked to
 *
 * Since: 2.32
 */
GMenuModel *
g_menu_link_iter_get_value (GMenuLinkIter *iter)
{
  g_return_val_if_fail (iter->priv->valid, NULL);

  return g_object_ref (iter->priv->value);
}

static void
g_menu_link_iter_finalize (GObject *object)
{
  GMenuLinkIter *iter = G_MENU_LINK_ITER (object);

  if (iter->priv->value)
    g_object_unref (iter->priv->value);

  G_OBJECT_CLASS (g_menu_link_iter_parent_class)
    ->finalize (object);
}

static void
g_menu_link_iter_init (GMenuLinkIter *iter)
{
  iter->priv = g_menu_link_iter_get_instance_private (iter);
}

static void
g_menu_link_iter_class_init (GMenuLinkIterClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = g_menu_link_iter_finalize;
}
