import store from '../../store';
import { fetchWrapper } from '@common/fetchWrapper.js';

export default {
  // NOTIFIERS ------------------------------------------------------------- //
  /**
   * Gets a list of notifier types (notifiers that a user can create)
   * @returns {Promise} Promise - A promise object that signals the completion
   *                              or rejection of the request.
   */
  async getNotifierTypes () {
    return await fetchWrapper({ url: 'api/notifierTypes' });
  },

  /**
   * Gets a list of user configured notifiers
   * NOTE: updates the store with the list of notifiers returned
   * @returns {Promise} Promise - A promise object that signals the completion
   *                              or rejection of the request.
   */
  async getNotifiers () {
    const response = await fetchWrapper({ url: 'api/notifiers' });
    store.commit('setNotifiers', response.data);
    return response.data;
  },

  /**
   * Creates a new notifier
   * @param {Object} notifier - The new notifier to create
   * @returns {Promise} Promise - A promise object that signals the completion
   *                              or rejection of the request.
   */
  async createNotifier (notifier) {
    const response = await fetchWrapper({ url: 'api/notifier', method: 'POST', data: notifier });
    return response.data;
  },

  /**
   * Updates a notifier
   * @param {String} id - The unique id of the notifier to update
   * @param {Object} notifier - The notifier to update
   * @returns {Promise} Promise - A promise object that signals the completion
   *                              or rejection of the request.
   */
  async updateNotifier (id, notifier) {
    const response = await fetchWrapper({ url: `api/notifier/${id}`, method: 'PUT', data: notifier });
    return response.data;
  },

  /**
   * Deletes a notifier
   * @param {String} id - The id of the notifier to delete
   * @returns {Promise} Promise - A promise object that signals the completion
   *                              or rejection of the request.
   */
  async deleteNotifier (id) {
    const response = await fetchWrapper({ url: `api/notifier/${id}`, method: 'DELETE' });
    return response.data;
  },

  /**
   * Tests a notifier
   * @param {String} id - The id of the notifier to test
   * @returns {Promise} Promise - A promise object that signals the completion
   *                              or rejection of the request.
   */
  async testNotifier (id) {
    const response = await fetchWrapper({ url: `api/notifier/${id}/test`, method: 'POST' });
    return response.data;
  },

  // SHORTCUTS ------------------------------------------------------------- //
  /**
   * Gets a list of user configured shortcuts
   * @param {Object} params - Query params object to search for shortcuts
   * @returns {Promise} Promise - A promise object that signals the completion
   *                              or rejection of the request.
   */
  async getShortcuts (params) {
    return await fetchWrapper({ url: 'api/shortcuts', params });
  },

  /**
   * Creates a new shortcut
   * @param {Object} shortcut - The new shortcut to create
   * @returns {Promise} Promise - A promise object that signals the completion
   *                              or rejection of the request.
   */
  async createShortcut (shortcut) {
    return await fetchWrapper({ url: 'api/shortcut', method: 'POST', data: shortcut });
  },

  /**
   * Updates a shortcut
   * @param {String} id - The unique id of the shortcut to update
   * @param {Object} shortcut - The shortcut to update
   * @returns {Promise} Promise - A promise object that signals the completion
   *                              or rejection of the request.
   */
  async updateShortcut (id, shortcut) {
    return await fetchWrapper({ url: `api/shortcut/${id}`, method: 'PUT', data: shortcut });
  },

  /**
   * Deletes a shortcut
   * @param {String} id - The unique id of the shortcut to delete
   * @returns {Promise} Promise - A promise object that signals the completion
   *                              or rejection of the request.
   */
  async deleteShortcut (id) {
    return await fetchWrapper({ url: `api/shortcut/${id}`, method: 'DELETE' });
  },

  // VIEWS ----------------------------------------------------------------- //
  /**
   * Gets views
   * @param {Object} params     Query params object to search for views
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  async getViews (params) {
    const response = await fetchWrapper({ url: 'api/views', params });
    store.commit('setViews', response.data);
    return response;
  },

  /**
   * Creates a view
   * @param {Object} view       The new view data to the server
   *                            { name: 'specialview', expression: 'something == somethingelse'}
   * @param {string} userId     The unique identifier for a user
   *                            (only required if not the current user)
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  async createView (view, userId) {
    return await fetchWrapper({ url: 'api/view', method: 'POST', data: view, params: { userId } });
  },

  /**
   * Deletes a view
   * @param {string} viewId     The name of the view to delete
   * @param {string} userId     The unique identifier for a user
   *                            (only required if not the current user)
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  async deleteView (viewId, userId) {
    return await fetchWrapper({ url: `api/view/${viewId}`, method: 'DELETE', params: { userId } });
  },

  /**
   * Updates a view
   * @param {Object} view       The view data to pass to the server
   * @param {string} userId     The unique identifier for a user
   *                            (only required if not the current user)
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  async updateView (view, userId) {
    const id = view.id;
    // remove client only fields
    delete view.id;
    delete view.changed;

    return await fetchWrapper({ url: `api/view/${id}`, method: 'PUT', data: view, params: { userId } });
  },

  // PERIODIC QUERIES ------------------------------------------------------ //
  /**
   * Gets a user's cron queries
  * @param {Object} params      Query params object to search for queries
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  async getCronQueries (params) {
    return await fetchWrapper({ url: 'api/crons', params });
  },

  /**
   * Creates a specified cron query for a user
   * @param {Object} data       The cron query data to pass to the server
   * @param {string} userId     The unique identifier for a user
   *                            (only required if not the current user)
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  async createCronQuery (data, userId) {
    return await fetchWrapper({ url: 'api/cron', method: 'POST', data, params: { userId } });
  },

  /**
   * Deletes a user's specified cron query
   * @param {string} key        The key of the cron query to be removed
   * @param {string} userId     The unique identifier for a user
   *                            (only required if not the current user)
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  async deleteCronQuery (key, userId) {
    return await fetchWrapper({ url: `api/cron/${key}`, method: 'DELETE', params: { userId } });
  },

  /**
   * Updates a specified cron query for a user
   * @param {Object} data       The cron query data to pass to the server
   * @param {string} userId     The unique identifier for a user
   *                            (only required if not the current user)
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  async updateCronQuery (data, userId) {
    delete data.changed; // remove client only fields
    return await fetchWrapper({ url: `api/cron/${data.key}`, method: 'POST', data, params: { userId } });
  }
};
