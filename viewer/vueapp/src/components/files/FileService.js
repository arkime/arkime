import Vue from 'vue';
import setReqHeaders from '@real_common/setReqHeaders';

export default {
  /**
   * Gets a list of files
   * @param {Object} query Parameters to query the server for specific files
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  async get (query) {
    const options = {
      headers: setReqHeaders({ 'Content-Type': 'application/json' })
    };

    const params = new URLSearchParams(query)

    try {
      let response = await fetch(`api/files${params}`, options);
      response = await response.json();
      return response;
    } catch (err) {
      throw err;
    }
  }
};
