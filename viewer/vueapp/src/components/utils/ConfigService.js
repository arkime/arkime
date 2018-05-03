import Vue from 'vue';

let _molochClustersCache;
let getMolochClustersQIP;
let _molochClickablesCache;
let getMolochClickablesQIP;

export default {

  /**
   * Gets the available moloch clusters and caches the results
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  getMolochClusters: function () {
    if (getMolochClustersQIP) { return getMolochClustersQIP; }

    getMolochClustersQIP = new Promise((resolve, reject) => {
      if (_molochClustersCache) { resolve(_molochClustersCache); }

      Vue.axios.get('molochclusters')
        .then((response) => {
          getMolochClustersQIP = undefined;
          _molochClustersCache = response.data;
          resolve(response.data);
        }, (error) => {
          getMolochClustersQIP = undefined;
          reject(error);
        });
    });

    return getMolochClustersQIP;
  },

  /**
   * Gets the available clickable fields and caches the result
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  getMolochClickables: function () {
    if (getMolochClickablesQIP) { return getMolochClickablesQIP; }

    getMolochClickablesQIP = new Promise((resolve, reject) => {
      if (_molochClickablesCache) { resolve(_molochClickablesCache); }

      Vue.axios.get('molochRightClick')
        .then((response) => {
          getMolochClickablesQIP = undefined;

          for (var key in response.data) {
            var item = response.data[key];
            if (item.func !== undefined) {
              // TODO
              /* eslint-disable no-new-func */
              item.func = new Function('key', 'value', item.func);
            }
          }

          _molochClickablesCache = response.data;
          resolve(response.data);
        }, (error) => {
          getMolochClickablesQIP = undefined;
          reject(error);
        });
    });

    return getMolochClickablesQIP;
  }

};
