import Vue from 'vue';

export default {

  /* service methods ------------------------------------------------------- */
  /**
   * Gets connections data from the server
   * @param {object} query        Parameters to query the server
   * @param {object} cancelToken  Token to cancel the request
   * @returns {Promise} Promise   A promise object that signals the completion
   *                              or rejection of the request.
   */
  get: function (query, cancelToken) {
    return new Promise((resolve, reject) => {
      let options = {
        url: 'connections.json',
        method: 'GET',
        params: query,
        cancelToken: cancelToken
      };

      Vue.axios(options)
        .then((response) => {
          if (response.data.bsqErr) { reject(response.data.bsqErr); }
          resolve(response);
        }, (error) => {
          if (!Vue.axios.isCancel(error)) {
            reject(error);
          }
        });
    });
  }

};
