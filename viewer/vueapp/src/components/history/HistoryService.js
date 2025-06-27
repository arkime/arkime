import { fetchWrapper } from '@/fetchWrapper.js';

export default {
  /**
   * Gets a list of histories
   * @param {Object} query Parameters to query the server for specific histories
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  async get (query) {
    return await fetchWrapper({ url: 'api/histories', params: query });
  },

  /**
   * Deletes a history item
   * @param {Object} id The id of the history item to delete
   * @param {Object} index The es index of the history item to delete
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  async delete (id, index) {
    return await fetchWrapper({ url: `api/history/${id}`, method: 'DELETE', params: { index } });
  }
};
