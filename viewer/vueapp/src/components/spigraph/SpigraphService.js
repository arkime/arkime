import Vue from 'vue';

export default {

  /* service methods ------------------------------------------------------- */
  /**
   * Gets spigraph data from the server
   * @param {object} query        Parameters to query the server
   * @param {object} cancelToken  Token to cancel the request
   * @returns {Promise} Promise   A promise object that signals the completion
   *                              or rejection of the request.
   */
  get: function (query, cancelToken) {
    return new Promise((resolve, reject) => {
      const options = {
        url: 'api/spigraph',
        method: 'POST',
        data: query,
        cancelToken: cancelToken
      };

      Vue.axios(options)
        .then((response) => {
          if (response.data.error) { reject(response.data.error); }
          resolve(response);
        }, (error) => {
          if (!Vue.axios.isCancel(error)) {
            reject(error);
          }
        });
    });
  },

  /**
   * Gets spigraph hierarchy data from the server
   * @param {object} query        Parameters to query the server
   * @param {object} cancelToken  Token to cancel the request
   * @returns {Promise} Promise   A promise object that signals the completion
   *                              or rejection of the request.
   */
  getHierarchy: function (query, cancelToken) {
    return new Promise((resolve, reject) => {
      const options = {
        url: 'api/spigraphhierarchy',
        method: 'POST',
        data: query,
        cancelToken: cancelToken
      };

      Vue.axios(options)
        .then((response) => {
          if (response.data.error) { reject(response.data.error); }
          resolve(response);
        }, (error) => {
          if (!Vue.axios.isCancel(error)) {
            reject(error);
          }
        });
    });
  }

};
