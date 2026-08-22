/*
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
*/
import { fetchWrapper } from '@common/fetchWrapper.js';

/**
 * Creates a service for managing shareable items of a specific type.
 * This factory function generates CRUD operations for shareables.
 *
 * @param {string} type - The shareable type (e.g., 'summaryConfig', 'dashboard')
 * @returns {Object} Service object with CRUD methods
 *
 * @example
 * // Create a service for summary configurations
 * const SummaryConfigService = createShareableService('summaryConfig');
 *
 * // Use the service
 * const configs = await SummaryConfigService.list();
 * const config = await SummaryConfigService.get(id);
 * await SummaryConfigService.save({ name: 'My Config', data: {...} });
 */
export const createShareableService = (type) => ({
  /**
   * Lists all shareable items of this type that the user has access to
   * @param {object} params - Optional extra params (viewOnly, searchTerm, sort, desc, start, length)
   * @returns {Promise} Promise resolving to { data: [], recordsTotal, recordsFiltered }
   */
  async list (params) {
    return await fetchWrapper({
      url: 'api/shareables',
      params: { type, ...params }
    });
  },

  /**
   * Gets a specific shareable item by ID
   * @param {string} id - The shareable ID
   * @returns {Promise} Promise resolving to { success, shareable }
   */
  async get (id) {
    return await fetchWrapper({
      url: `api/shareable/${id}`
    });
  },

  /**
   * Creates a new shareable item
   * @param {object} config - The configuration to save
   * @param {string} config.name - Required name for the item
   * @param {string} config.description - Optional description
   * @param {object} config.data - The item data
   * @param {string[]} config.viewUsers - Users who can view this item
   * @param {string[]} config.viewRoles - Roles that can view this item
   * @param {string[]} config.editUsers - Users who can edit this item
   * @param {string[]} config.editRoles - Roles that can edit this item
   * @returns {Promise} Promise resolving to { success, shareable, id }
   */
  async save (config) {
    return await fetchWrapper({
      url: 'api/shareable',
      method: 'POST',
      data: {
        type,
        name: config.name,
        description: config.description,
        data: config.data,
        viewUsers: config.viewUsers || [],
        viewRoles: config.viewRoles || [],
        editUsers: config.editUsers || [],
        editRoles: config.editRoles || []
      }
    });
  },

  /**
   * Updates an existing shareable item. Only the keys given are sent, and the
   * API leaves anything omitted as it was, so a partial update cannot wipe the
   * sharing off an item.
   * @param {string} id - The shareable ID to update
   * @param {object} config - The fields to change
   * @returns {Promise} Promise resolving to { success, shareable }
   */
  async update (id, config) {
    return await fetchWrapper({
      url: `api/shareable/${id}`,
      method: 'PUT',
      data: config
    });
  },

  /**
   * Deletes a shareable item
   * @param {string} id - The shareable ID to delete
   * @returns {Promise} Promise resolving to { success, text }
   */
  async delete (id) {
    return await fetchWrapper({
      url: `api/shareable/${id}`,
      method: 'DELETE'
    });
  }
});

/**
 * Flattens a shareable into the shape the layout UIs use. A layout is just its
 * name plus whatever is in data, so `data` round-trips without a per-type map.
 * @param {object} shareable - A shareable from the API
 * @returns {object} { id, name, ...data, canEdit, canDelete, view/edit users and roles }
 */
export const shareableToLayout = (shareable) => ({
  id: shareable.id,
  name: shareable.name,
  ...(shareable.data || {}),
  creator: shareable.creator,
  shared: !!shareable.shared,
  canEdit: shareable.canEdit !== false,
  canDelete: shareable.canDelete !== false,
  viewUsers: shareable.viewUsers || [],
  viewRoles: shareable.viewRoles || [],
  editUsers: shareable.editUsers || [],
  editRoles: shareable.editRoles || []
});

// sharing lives at the top level of a shareable, everything else is its data
const SHARE_KEYS = ['viewUsers', 'viewRoles', 'editUsers', 'editRoles'];

const splitLayout = (layout) => {
  const { name: layoutName, ...rest } = layout;
  const body = {};
  if (layoutName !== undefined) { body.name = layoutName; }
  for (const key of SHARE_KEYS) {
    if (rest[key] !== undefined) {
      body[key] = rest[key];
      delete rest[key];
    }
  }
  return { body, data: rest };
};

/**
 * Creates a service for a layout type stored as shareables (column layouts,
 * info field layouts, spiview layouts). Wraps createShareableService so callers
 * work with flat layout objects instead of shareable envelopes.
 *
 * @param {string} type - The shareable type, eg 'sessionsTableLayout'
 * @returns {Object} Service with list/create/update/delete over flat layouts
 */
export const createLayoutService = (type) => {
  const shareables = createShareableService(type);

  return {
    /* Layouts the user owns or that are shared with them, view or edit.
       Yours come first, then the ones shared with you, each still in the
       name order the API returned. */
    async list () {
      const response = await shareables.list({ viewOnly: false });
      const layouts = response.data.map(shareableToLayout);
      return [...layouts.filter(l => !l.shared), ...layouts.filter(l => l.shared)];
    },

    async create (layout) {
      const { body, data } = splitLayout(layout);
      const response = await shareables.save({ ...body, data });
      return shareableToLayout(response.shareable);
    },

    /* Only the keys given are sent, so an update that leaves sharing out
       cannot clear it */
    async update (id, layout) {
      const { body, data } = splitLayout(layout);
      if (Object.keys(data).length) { body.data = data; }
      const response = await shareables.update(id, body);
      return shareableToLayout(response.shareable);
    },

    async delete (id) {
      return await shareables.delete(id);
    }
  };
};
