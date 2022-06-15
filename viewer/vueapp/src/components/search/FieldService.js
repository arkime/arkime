import Vue from 'vue';

import store from '../../store';
import countries from './countries.json';
import customCols from '../sessions/customCols.json';

const ipDstPortField = {
  category: 'ip',
  group: 'general',
  dbField2: 'dstIp:port',
  exp: 'ip.dst:port',
  dbField: 'destination.ip:port',
  fieldECS: 'destination.ip:port',
  friendlyName: 'Dst IP:Dst Port',
  help: 'Destination IP:Destination Port'
};

export default {

  ipDstPortField,

  addIpDstPortField (fields) {
    let result;

    if (Array.isArray(fields)) {
      result = [...fields]; // shallow copy so we don't mutate data
      result.push(ipDstPortField);
    } else {
      result = { ...fields }; // shallow copy so we don't mutate data
      result[ipDstPortField.exp] = ipDstPortField;
    }

    return result;
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
      const options = { params, cancelToken: source.token };

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
   * Matches dbField, dbField2, fieldECS, exp, or aliases (unless excluded)
   * @param {string} search - The value to search for
   * @param {boolean} ignoreAliases - Whether to ignore the alias list
   * @returns {object} The field or undefined
   */
  getField (search, ignoreAliases) {
    if (!search) { return undefined; }

    // search the fields map first
    if (store.state.fieldsMap[search]) return store.state.fieldsMap[search];
    // then search the custom fields
    if (customCols[search]) return customCols[search];
    // then search aliases if not excluded from search
    if (!ignoreAliases && store.state.fieldsAliasMap[search]) {
      return store.state.fieldsAliasMap[search];
    }
    // lastly, check the super special ip.dst:port field
    if (search === ipDstPortField.exp ||
      search === ipDstPortField.dbField ||
      search === ipDstPortField.dbField2 ||
      search === ipDstPortField.fieldECS) {
      return ipDstPortField;
    }

    return undefined;
  },

  /**
   * Retrieves a field's property
   * Matches dbField, dbField2, fieldECS, or rawField
   * @param {string} search - The value to search for
   * @param {string} prop - The field property value to return
   * @param {boolean} ignoreAliases - Whether to ignore the alias list
   * @returns {string} The field property value or undefined
   */
  getFieldProperty (search, prop, ignoreAliases) {
    const field = this.getField(search, ignoreAliases);

    if (field && field[prop]) { return field[prop]; }

    return undefined;
  }

};
