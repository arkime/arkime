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
   * @returns {Promise} Promise resolving to { data: [], recordsTotal, recordsFiltered }
   */
  async list () {
    return await fetchWrapper({
      url: 'api/shareables',
      params: { type }
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
   * Updates an existing shareable item
   * @param {string} id - The shareable ID to update
   * @param {object} config - The updated configuration
   * @returns {Promise} Promise resolving to { success, shareable }
   */
  async update (id, config) {
    return await fetchWrapper({
      url: `api/shareable/${id}`,
      method: 'PUT',
      data: {
        name: config.name,
        description: config.description,
        data: config.data,
        viewUsers: config.viewUsers,
        viewRoles: config.viewRoles,
        editUsers: config.editUsers,
        editRoles: config.editRoles
      }
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
