import Vue from 'vue';

export default {
  /**
   * Gets a list of files
   * @param {Object} query Parameters to query the server for specific files
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  get (query) {
    return new Promise((resolve, reject) => {
      const options = {
        url: 'api/files',
        method: 'GET',
        params: query
      };

      Vue.axios(options).then((response) => {
        resolve(response);
      }).catch((error) => {
        reject(error);
      });
    });
  }
};
