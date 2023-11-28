import Vue from 'vue';
import store from '../../store';

let _arkimeClickablesCache;
let getArkimeClickablesQIP;
let getFieldActionsQIP;

export default {
  /**
   * Gets the information that every page in the application needs to run
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  getAppInfo: function () {
    return new Promise((resolve, reject) => {
      Vue.axios.get('api/appinfo').then((response) => {
        store.commit('setAppInfo', response.data);
        resolve(response.data);
      }).catch((error) => {
        console.log('ERROR - fetching app info. Arkime app will not function!', error);
        reject(error);
      });
    });
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

      Vue.axios.get('api/valueactions')
        .then((response) => {
          getArkimeClickablesQIP = undefined;

          for (const key in response.data) {
            const item = response.data[key];
            if (item.func !== undefined) {
              /* eslint-disable no-new-func */
              item.func = new Function('key', 'value', item.func);
            }

            if (item.category !== undefined && !Array.isArray(item.category)) {
              item.category = item.category.split(',');
            }
          }

          _arkimeClickablesCache = response.data;
          return resolve(response.data);
        }, (error) => {
          getArkimeClickablesQIP = undefined;
          return reject(error);
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

      Vue.axios.get('api/fieldactions').then((response) => {
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
  cancelEsTask: function (cancelId) {
    return new Promise((resolve, reject) => {
      Vue.axios.post(`api/estasks/${cancelId}/cancelwith`)
        .then((response) => {
          resolve(response);
        }, (error) => {
          reject(error);
        });
    });
  },

  /**
   * Retrieves a list of active and inactive Arkime clusters.
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  getClusters: function () {
    return new Promise((resolve, reject) => {
      Vue.axios.get('api/clusters')
        .then((response) => {
          resolve(response.data);
        }, (error) => {
          reject(error);
        });
    });
  }
};
