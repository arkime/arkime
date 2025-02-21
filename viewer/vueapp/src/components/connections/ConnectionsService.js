import { fetchWrapper } from '@/fetchWrap';

export default {

  /* service methods ------------------------------------------------------- */
  /**
   * Gets connections data from the server
   * @param {object} query        Parameters to query the server
   * @param {object} cancelToken  Token to cancel the request
   * @returns {Promise} Promise   A promise object that signals the completion
   *                              or rejection of the request.
   */
  async get (query, cancelToken) {
    return await fetchWrapper({ url: 'api/connections', method: 'POST', data: query, cancelToken });
  }

};
