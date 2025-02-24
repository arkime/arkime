import { cancelFetchWrapper } from '@/fetchWrapper.js';

export default {

  /* service methods ------------------------------------------------------- */
  /**
   * Gets connections data from the server
   * @param {object} query        Parameters to query the server
   * @returns {AbortController} The AbortController used to cancel the request.
   * @returns {Promise<Object>} The response data parsed as JSON.
   */
  get (query) {
    return cancelFetchWrapper({ url: 'api/connections', method: 'POST', params: query });
  }

};
