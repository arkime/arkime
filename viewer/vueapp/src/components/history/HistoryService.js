import setReqHeaders from '@real_common/setReqHeaders';

const historyReqOptions = {
  headers: setReqHeaders({ 'Content-Type': 'application/json' })
}

export default {
  /**
   * Gets a list of histories
   * @param {Object} query Parameters to query the server for specific histories
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  async get (query) {
    const options = {
      ...historyReqOptions,
      params: query
    };
    const params = new URLSearchParams(query)

    try {
      let response = await fetch(`api/histories${params}`, historyReqOptions);
      response = await response.json();
      return response;
    } catch (err) {
      throw err; // TODO VUE3 test
    }
  },

  /**
   * Deletes a history item
   * @param {Object} id The id of the history item to delete
   * @param {Object} index The es index of the history item to delete
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  async delete (id, index) {
    const options = {
      ...historyReqOptions,
      method: 'DELETE'
    };
    const params = new URLSearchParams({ index })

    try {
      let response = await fetch(`api/history/${id}${params}`, options);
      response = await response.json();
      return response;
    } catch (err) {
      throw err; // TODO VUE3 test
    }
  }
};
