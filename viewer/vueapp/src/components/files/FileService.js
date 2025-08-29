import { fetchWrapper } from '@common/fetchWrapper.js';

export default {
  /**
   * Gets a list of files
   * @param {Object} query Parameters to query the server for specific files
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  async get (query) {
    return await fetchWrapper({ url: 'api/files', params: query });
  }
};
