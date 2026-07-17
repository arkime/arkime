import store from '../../store';
import countries from './countries.json';
import { fetchWrapper, cancelFetchWrapper } from '@common/fetchWrapper.js';
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

const allIpField = {
  category: 'ip',
  group: 'general',
  exp: 'ip',
  dbField: 'ip',
  friendlyName: 'All IP Fields',
  help: 'All source and destination IP addresses combined'
};

export default {

  ipDstPortField,
  allIpField,

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
   * Adds the special summary fields (All IP Fields, Dst IP:Dst Port) to the field list
   * @param {Array|Object} fields The fields array or map to add to
   * @returns {Array|Object} A new array/object with the special fields added
   */
  addSummarySpecialFields (fields) {
    let result;

    if (Array.isArray(fields)) {
      result = [...fields]; // shallow copy so we don't mutate data
      // skip a special field the list already defines — the built-in "All IP fields"
      // (exp 'ip') collides with allIpField, dupe exps break the editor's multi-select
      const exps = new Set(result.map(f => f.exp));
      if (!exps.has(ipDstPortField.exp)) { result.unshift(ipDstPortField); } // add at beginning for visibility
      if (!exps.has(allIpField.exp)) { result.unshift(allIpField); }
    } else {
      result = { ...fields }; // shallow copy so we don't mutate data
      if (!(allIpField.exp in result)) { result[allIpField.exp] = allIpField; }
      if (!(ipDstPortField.exp in result)) { result[ipDstPortField.exp] = ipDstPortField; }
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
    return cancelFetchWrapper({ url: 'api/unique', params });
  },

  /**
   * Gets the list of shortcuts to use in the expression autocomplete typeahead
   * @param {string} url The URL to fetch the variables from
   * @returns {Promise<Object>} The response data parsed as JSON.
   */
  async getShortcuts (url) {
    return fetchWrapper({ url });
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
