/*
 * Copyright 2015 Lars Uebernickel
 * Copyright 2015 Ryan Lortie
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
 * Authors:
 *     Lars Uebernickel <lars@uebernic.de>
 *     Ryan Lortie <desrt@desrt.ca>
 */

#include "config.h"

#include "glistmodel.h"
#include "glibintl.h"

G_DEFINE_INTERFACE (GListModel, g_list_model, G_TYPE_OBJECT)

/**
 * SECTION:glistmodel
 * @title: GListModel
 * @short_description: An interface describing a dynamic list of objects
 * @include: gio/gio.h
 * @see_also: #GListStore
 *
 * #GListModel is an interface that represents a mutable list of
 * #GObjects. Its main intention is as a model for various widgets in
 * user interfaces, such as list views, but it can also be used as a
 * convenient method of returning lists of data, with support for
 * updates.
 *
 * Each object in the list may also report changes in itself via some
 * mechanism (normally the #GObject::notify signal).  Taken together
 * with the #GListModel::items-changed signal, this provides for a list
 * that can change its membership, and in which the members can change
 * their individual properties.
 *
 * A good example would be the list of visible wireless network access
 * points, where each access point can report dynamic properties such as
 * signal strength.
 *
 * It is important to note that the #GListModel itself does not report
 * changes to the individual items.  It only reports changes to the list
 * membership.  If you want to observe changes to the objects themselves
 * then you need to connect signals to the objects that you are
 * interested in.
 *
 * All items in a #GListModel are of (or derived from) the same type.
 * g_list_model_get_item_type() returns that type.  The type may be an
 * interface, in which case all objects in the list must implement it.
 *
 * The semantics are close to that of an array:
 * g_list_model_get_n_items() returns the number of items in the list and
 * g_list_model_get_item() returns an item at a (0-based) position. In
 * order to allow implementations to calculate the list length lazily,
 * you can also iterate over items: starting from 0, repeatedly call
 * g_list_model_get_item() until it returns %NULL.
 *
 * An implementation may create objects lazily, but must take care to
 * return the same object for a given position until all references to
 * it are gone.
 *
 * On the other side, a consumer is expected only to hold references on
 * objects that are currently "user visible", in order to faciliate the
 * maximum level of laziness in the implementation of the list and to
 * reduce the required number of signal connections at a given time.
 *
 * This interface is intended only to be used from a single thread.  The
 * thread in which it is appropriate to use it depends on the particular
 * implementation, but typically it will be from the thread that owns
 * the [thread-default main context][g-main-context-push-thread-default]
 * in effect at the time that the model was created.
 */

/**
 * GListModelInterface:
 * @g_iface: parent #GTypeInterface
 * @get_item_type: the virtual function pointer for g_list_model_get_item_type()
 * @get_n_items: the virtual function pointer for g_list_model_get_n_items()
 * @get_item: the virtual function pointer for g_list_model_get_item()
 *
 * The virtual function table for #GListModel.
 *
 * Since: 2.44
 */

/**
 * GListModelInterface::get_item:
 * @list: a #GListModel
 * @position: the position of the item to fetch
 *
 * Get the item at @position. If @position is greater than the number of
 * items in @list, %NULL is returned.
 *
 * %NULL is never returned for an index that is smaller than the length
 * of the list.  See g_list_model_get_n_items().
 *
 * Returns: (type GObject) (transfer full) (nullable): the object at @position.
 *
 * Since: 2.44
 */

/**
 * GListModel:
 *
 * #GListModel is an opaque data structure and can only be accessed
 * using the following functions.
 **/

static guint g_list_model_changed_signal;

static void
g_list_model_default_init (GListModelInterface *iface)
{
  /**
   * GListModel::items-changed:
   * @list: the #GListModel that changed
   * @position: the position at which @list changed
   * @removed: the number of items removed
   * @added: the number of items added
   *
   * This signal is emitted whenever items were added or removed to
   * @list. At @position, @removed items were removed and @added items
   * were added in their place.
   *
   * Since: 2.44
   */
  g_list_model_changed_signal = g_signal_new (I_("items-changed"),
                                              G_TYPE_LIST_MODEL,
                                              G_SIGNAL_RUN_LAST,
                                              0,
                                              NULL, NULL,
                                              g_cclosure_marshal_generic,
                                              G_TYPE_NONE,
                                              3, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_UINT);
}

/**
 * g_list_model_get_item_type:
 * @list: a #GListModel
 *
 * Gets the type of the items in @list. All items returned from
 * g_list_model_get_type() are of that type or a subtype, or are an
 * implementation of that interface.
 *
 * The item type of a #GListModel can not change during the life of the
 * model.
 *
 * Returns: the #GType of the items contained in @list.
 *
 * Since: 2.44
 */
GType
g_list_model_get_item_type (GListModel *list)
{
  g_return_val_if_fail (G_IS_LIST_MODEL (list), G_TYPE_NONE);

  return G_LIST_MODEL_GET_IFACE (list)->get_item_type (list);
}

/**
 * g_list_model_get_n_items:
 * @list: a #GListModel
 *
 * Gets the number of items in @list.
 *
 * Depending on the model implementation, calling this function may be
 * less efficient than iterating the list with increasing values for
 * @position until g_list_model_get_item() returns %NULL.
 *
 * Returns: the number of items in @list.
 *
 * Since: 2.44
 */
guint
g_list_model_get_n_items (GListModel *list)
{
  g_return_val_if_fail (G_IS_LIST_MODEL (list), 0);

  return G_LIST_MODEL_GET_IFACE (list)->get_n_items (list);
}

/**
 * g_list_model_get_item: (skip)
 * @list: a #GListModel
 * @position: the position of the item to fetch
 *
 * Get the item at @position. If @position is greater than the number of
 * items in @list, %NULL is returned.
 *
 * %NULL is never returned for an index that is smaller than the length
 * of the list.  See g_list_model_get_n_items().
 *
 * Returns: (transfer full) (nullable): the item at @position.
 *
 * Since: 2.44
 */
gpointer
g_list_model_get_item (GListModel *list,
                       guint       position)
{
  g_return_val_if_fail (G_IS_LIST_MODEL (list), NULL);

  return G_LIST_MODEL_GET_IFACE (list)->get_item (list, position);
}

/**
 * g_list_model_get_object: (rename-to g_list_model_get_item)
 * @list: a #GListModel
 * @position: the position of the item to fetch
 *
 * Get the item at @position. If @position is greater than the number of
 * items in @list, %NULL is returned.
 *
 * %NULL is never returned for an index that is smaller than the length
 * of the list.  See g_list_model_get_n_items().
 *
 * Returns: (transfer full) (nullable): the object at @position.
 *
 * Since: 2.44
 */
GObject *
g_list_model_get_object (GListModel *list,
                         guint       position)
{
  gpointer item;

  g_return_val_if_fail (G_IS_LIST_MODEL (list), NULL);

  item = g_list_model_get_item (list, position);

  return G_OBJECT (item);
}

/**
 * g_list_model_items_changed:
 * @list: a #GListModel
 * @position: the position at which @list changed
 * @removed: the number of items removed
 * @added: the number of items added
 *
 * Emits the #GListModel::items-changed signal on @list.
 *
 * This function should only be called by classes implementing
 * #GListModel. It has to be called after the internal representation
 * of @list has been updated, because handlers connected to this signal
 * might query the new state of the list.
 *
 * Implementations must only make changes to the model (as visible to
 * its consumer) in places that will not cause problems for that
 * consumer.  For models that are driven directly by a write API (such
 * as #GListStore), changes can be reported in response to uses of that
 * API.  For models that represent remote data, changes should only be
 * made from a fresh mainloop dispatch.  It is particularly not
 * permitted to make changes in response to a call to the #GListModel
 * consumer API.
 *
 * Stated another way: in general, it is assumed that code making a
 * series of accesses to the model via the API, without returning to the
 * mainloop, and without calling other code, will continue to view the
 * same contents of the model.
 *
 * Since: 2.44
 */
void
g_list_model_items_changed (GListModel *list,
                            guint       position,
                            guint       removed,
                            guint       added)
{
  g_return_if_fail (G_IS_LIST_MODEL (list));

  g_signal_emit (list, g_list_model_changed_signal, 0, position, removed, added);
}
