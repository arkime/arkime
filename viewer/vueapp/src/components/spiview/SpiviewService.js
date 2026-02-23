import Utils from '../utils/utils';
import { cancelFetchWrapper } from '@common/fetchWrapper.js';

export default {

  /* service methods ------------------------------------------------------- */
  /**
   * Gets spiview data from the server
   * @param {object} params        Parameters to query the server
   * @returns {Promise} Promise   A promise object that signals the completion
   *                              or rejection of the request.
   */
  async get (params) {
    Utils.setFacetsQuery(params, 'spiview');

    const options = {
      url: 'api/spiview',
      method: 'POST',
      params
    };

    return cancelFetchWrapper(options);
  }

};
