import store from '../../store';
import { fetchWrapper } from '@real_common/fetchWrapper.js';

let _arkimeClickablesCache;
let getArkimeClickablesQIP;
let getFieldActionsQIP;

export default {
  /**
   * Gets the information that every page in the application needs to run
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  getAppInfo: async function () {
    const data = await fetchWrapper({ url: 'api/appinfo' });
    store.commit('setAppInfo', data);
    return data;
  },

  /**
   * Gets the available clickable fields and caches the result
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  getArkimeClickables: function () {
    if (getArkimeClickablesQIP) { return getArkimeClickablesQIP; }

    getArkimeClickablesQIP = new Promise((resolve, reject) => {
      if (_arkimeClickablesCache) { return resolve(_arkimeClickablesCache); }

      fetchWrapper({ url: 'api/valueactions' }).then((response) => {
        getArkimeClickablesQIP = undefined;

        for (const key in response) {
          const item = response[key];
          if (item.func !== undefined) {
             
            item.func = new Function('key', 'value', item.func);
          }

          if (item.category !== undefined && !Array.isArray(item.category)) {
            item.category = item.category.split(',');
          }
        }

        _arkimeClickablesCache = response;
        return resolve(response);
      }).catch((err) => {
        getArkimeClickablesQIP = undefined;
        return reject(err);
      });
    });

    return getArkimeClickablesQIP;
  },

  /**
   * Gets the available field actions to add to field dropdown menus
   * and caches the result (in store)
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  getFieldActions: function () {
    if (getFieldActionsQIP) { return getFieldActionsQIP; }

    getFieldActionsQIP = new Promise((resolve, reject) => {
      const fieldActions = store.getters.getFieldActions;
      if (fieldActions && Object.keys(fieldActions).length > 0) {
        return resolve(fieldActions);
      }

      fetchWrapper({ url: 'api/fieldactions' }).then((response) => {
        getFieldActionsQIP = undefined;
        for (const key in response.data) {
          const item = response.data[key];
          if (item.category !== undefined && !Array.isArray(item.category)) {
            item.category = item.category.split(',');
          }
        }

        store.commit('setFieldActions', response.data);
        return resolve(response.data);
      }).catch((error) => {
        getFieldActionsQIP = undefined;
        return reject(error);
      });
    });
  },

  /**
   * Cancels any es tasks whose X-Opaque-Id equal this cancel id
   * @param {string} cancelId   The X-Opaque-Id set for the es tasks to cancel
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  cancelEsTask: async function (cancelId) {
    return await fetchWrapper({ url: `api/estasks/${cancelId}/cancelwith` });
  },

  /**
   * Retrieves a list of active and inactive Arkime clusters.
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  getClusters: async function () {
    const response = await fetchWrapper({ url: 'api/clusters' });
    return response.data;
  }
};
