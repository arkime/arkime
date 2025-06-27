import Utils from '../utils/utils';
import { cancelFetchWrapper } from '@/fetchWrapper.js';

export default {

  /* service methods ------------------------------------------------------- */
  /**
   * Gets spigraph data from the server
   * @param {object} query        Parameters to query the server
   * @returns {Promise} Promise   A promise object that signals the completion
   *                              or rejection of the request.
   */
  async get (query, cancelToken) {
    Utils.setFacetsQuery(query, 'spigraph');

    const options = {
      url: 'api/spigraph',
      method: 'POST',
      data: query
    };

    return cancelFetchWrapper(options);
  },

  /**
   * Gets spigraph hierarchy data from the server
   * @param {object} query        Parameters to query the server
   * @returns {Promise} Promise   A promise object that signals the completion
   *                              or rejection of the request.
   */
  getHierarchy (query) {
    Utils.setFacetsQuery(query, 'spigraph');

    const options = {
      url: 'api/spigraphhierarchy',
      method: 'POST',
      data: query
    };

    return cancelFetchWrapper(options);
  }

};
