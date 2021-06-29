import Vue from 'vue';

import countries from './countries.json';

let _fieldsMapCache;
let _fieldsArrayCache;
const queryInProgress = {
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

      let url = 'api/fields';
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
    const source = Vue.axios.CancelToken.source();

    const promise = new Promise((resolve, reject) => {
      const options = { params: params, cancelToken: source.token };

      Vue.axios.get('api/unique', options)
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
  },

  /**
   * Retrieves a field object
   * Matches dbField, dbField2, fieldECS, or rawField
   * @param {object} fields - A fields map
   * @param {string} search - The value to search for
   * @returns {object} The field or undefined
   */
  getField (fields, search) {
    for (const k in fields) {
      if (search === fields[k].dbField ||
          search === fields[k].dbField2 ||
          search === fields[k].fieldECS ||
          search === fields[k].rawField) {
        return fields[k];
      }
    }

    return undefined;
  },

  /**
   * Retrieves a field's property
   * Matches dbField, dbField2, fieldECS, or rawField
   * @param {object} fields - A map of fields
   * @param {string} search - The value to search for
   * @param {string} prop - The field property value to return
   * @returns {string} The field property value or undefined
   */
  getFieldProperty (fields, search, prop) {
    const field = this.getField(fields, search);

    if (field && field[prop]) { return field[prop]; }

    return undefined;
  }

};
