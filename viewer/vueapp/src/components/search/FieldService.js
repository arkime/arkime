import Vue from 'vue';

import countries from './countries.json';

let _fieldsMapCache;
let _fieldsArrayCache;
const queryInProgress = {
  array: false,
  promise: undefined
};

const ipDstPortField = {
  category: 'ip',
  group: 'general',
  dbField2: 'dstIp',
  exp: 'ip.dst:port',
  dbField: 'destination.ip:port',
  fieldECS: 'destination.ip:port',
  friendlyName: 'Dst IP:Dst Port',
  help: 'Destination IP:Destination Port'
};

export default {

  ipDstPortField: ipDstPortField,

  /**
   * Gets a field map from the server
   * @param {bool} array        Whether to request an array or map
   * @param {bool} addIpDstPort Whether to add the ip.dst:port field to the results
                                (don't add it to the cache because some pages don't want it)
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  get (array, addIpDstPort) {
    if (queryInProgress && queryInProgress.array === array) {
      return queryInProgress.promise;
    }

    function addIpDstPortFieldToResults (data) {
      let result;

      if (array) {
        result = [...data]; // shallow copy so we don't mutate data
        result.push(ipDstPortField);
      } else {
        result = { ...data }; // shallow copy so we don't mutate data
        result[ipDstPortField.exp] = ipDstPortField;
      }

      return result;
    }

    queryInProgress.promise = new Promise((resolve, reject) => {
      let cachedResult = false;
      if (array && _fieldsArrayCache) {
        cachedResult = _fieldsArrayCache;
      } else if (!array && _fieldsMapCache) {
        cachedResult = _fieldsMapCache;
      }

      if (cachedResult) { // if we have a cached result, return it
        if (addIpDstPort) { cachedResult = addIpDstPortFieldToResults(cachedResult); }
        return resolve(cachedResult);
      }

      // there is no cached result, so we need to fetch the fields
      Vue.axios.get(`api/fields${array ? '?array=true' : ''}`)
        .then((response) => {
          queryInProgress.promise = undefined;

          if (array) {
            _fieldsArrayCache = response.data;
          } else {
            _fieldsMapCache = response.data;
          }

          let result = response.data;
          if (addIpDstPort) { result = addIpDstPortFieldToResults(response.data); }
          return resolve(result);
        }, (error) => {
          queryInProgress.promise = undefined;
          return reject(error);
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
   * Matches dbField, dbField2, fieldECS, rawField, exp, or aliases
   * @param {string} search - The value to search for
   * @param {object|array} fields - A fields map or array
   * @returns {object} The field or undefined
   */
  getField (search, fields) {
    for (const k in fields) {
      if (search === fields[k].exp ||
          search === fields[k].dbField ||
          search === fields[k].dbField2 ||
          search === fields[k].fieldECS ||
          search === fields[k].rawField) {
        return fields[k];
      }
      if (fields[k].aliases) {
        for (const alias of fields[k].aliases) {
          if (search === alias) {
            return fields[k];
          }
        }
      }
    }

    return undefined;
  },

  /**
   * Retrieves a field's property
   * Matches dbField, dbField2, fieldECS, or rawField
   * @param {string} search - The value to search for
   * @param {string} prop - The field property value to return
   * @param {object|array} fields - A fields map or array
   * @returns {string} The field property value or undefined
   */
  getFieldProperty (search, prop, fields) {
    const field = this.getField(search, fields);

    if (field && field[prop]) { return field[prop]; }

    return undefined;
  }

};
