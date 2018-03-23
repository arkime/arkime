import Vue from 'vue';

import countries from './countries.json';

let _fieldsCache;
let queryInProgress;
const noop = () => {};

export default {

  /**
   * Gets a field map from the server
   * @param {bool} array        Whether to request an array or map
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  get (array) {
    if (queryInProgress) { return queryInProgress; }

    queryInProgress = new Promise((resolve, reject) => {
      if (_fieldsCache) { resolve(_fieldsCache); }

      let url = 'fields';
      if (array) { url += '?array=true'; }

      Vue.axios.get(url)
        .then((response) => {
          queryInProgress = undefined;
          _fieldsCache = response.data;
          resolve(response.data);
        }, (error) => {
          queryInProgress = undefined;
          reject(error);
        });
    });

    return queryInProgress;
  },

  /**
   * Gets field values from the server
   * @param {Object} params     The parameters to send with the query
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  getValues (params) {
    let deferred = Promise.defer();

    let request = Vue.axios.get('unique.text', {
      params: params,
      timeout: deferred.promise
    });

    let promise = request
      .then((response) => {
        return response.data;
      }, (error) => {
        return Promise.reject(error);
      }).catch(noop); // handle abort

    promise.abort = () => {
      deferred.resolve({ error: 'Request canceled.' });
    };

    // cleanup
    promise.finally(() => {
      promise.abort = noop;
      deferred = request = promise = null;
    });

    return promise;
  },

  /**
   * Gets the cached country code list
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  getCountryCodes () {
    return this.$q((resolve, reject) => {
      if (countries) {
        resolve(countries);
      } else {
        reject('Error retrieving country codes');
      }
    });
  }

};
