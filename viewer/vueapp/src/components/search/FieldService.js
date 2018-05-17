import Vue from 'vue';

import countries from './countries.json';

let _fieldsMapCache;
let _fieldsArrayCache;
let queryInProgress = {
  array: false,
  promise: undefined
};

export default {

  /**
   * Gets a field map from the server
   * @param {bool} array        Whether to request an array or map
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  get (array) {
    if (queryInProgress && queryInProgress.array === array) {
      return queryInProgress.promise;
    }

    queryInProgress.promise = new Promise((resolve, reject) => {
      if (array && _fieldsArrayCache) {
        resolve(_fieldsArrayCache);
      } else if (!array && _fieldsMapCache) {
        resolve(_fieldsMapCache);
      }

      let url = 'fields';
      if (array) { url += '?array=true'; }

      Vue.axios.get(url)
        .then((response) => {
          queryInProgress.promise = undefined;
          if (array) {
            _fieldsArrayCache = response.data;
          } else {
            _fieldsMapCache = response.data;
          }
          resolve(response.data);
        }, (error) => {
          queryInProgress.promise = undefined;
          reject(error);
        });
    });

    return queryInProgress.promise;
  },

  /**
   * Gets field values from the server
   * @param {Object} params The parameters to send with the query
   * @returns {Object} { promise, source } An object including a promise object
   * that signals the completion or rejection of the request and a source object
   * to allow the request to be cancelled
   */
  getValues (params) {
    let source = Vue.axios.CancelToken.source();

    let promise = new Promise((resolve, reject) => {
      let options = { params: params, cancelToken: source.token };

      Vue.axios.get('unique.txt', options)
        .then((response) => {
          resolve(response.data);
        })
        .catch((error) => {
          if (!Vue.axios.isCancel(error)) {
            reject(error);
          }
        });
    });

    return { promise, source };
  },

  /**
   * Gets the cached country code list
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  getCountryCodes () {
    return new Promise((resolve, reject) => {
      if (countries) {
        resolve(countries);
      } else {
        reject(new Error('Error retrieving country codes'));
      }
    });
  }

};
