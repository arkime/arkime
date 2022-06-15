import Vue from 'vue';

export default {
  /**
   * Gets a list of histories
   * @param {Object} query Parameters to query the server for specific histories
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  get (query) {
    return new Promise((resolve, reject) => {
      const options = {
        url: 'api/histories',
        method: 'GET',
        params: query
      };

      Vue.axios(options)
        .then((response) => {
          resolve(response);
        }, (error) => {
          reject(error);
        });
    });
  },

  /**
   * Deletes a history item
   * @param {Object} id The id of the history item to delete
   * @param {Object} index The es index of the history item to delete
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  delete (id, index) {
    return new Promise((resolve, reject) => {
      const options = {
        url: `api/history/${id}`,
        method: 'DELETE',
        params: { index }
      };

      Vue.axios(options)
        .then((response) => {
          resolve(response);
        }, (error) => {
          reject(error);
        });
    });
  }
};
