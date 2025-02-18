import store from '../../store';
import setReqHeaders from '@real_common/setReqHeaders';

let _arkimeClickablesCache;
let getArkimeClickablesQIP;
let getFieldActionsQIP;

const configReqOptions = {
  headers: setReqHeaders({ 'Content-Type': 'application/json' })
}

export default {
  /**
   * Gets the information that every page in the application needs to run
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  getAppInfo: async function () {
    try {
      let response = await fetch('api/appinfo', configReqOptions)
      response = await response.json()
      store.commit('setAppInfo', response);
      return response;
    } catch (err) {
      console.log('ERROR - fetching app info. Arkime app will not function!', err);
      throw err;
    }
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

      fetch('api/valueactions', configReqOptions).then((response) => {
        return response.json();
      }).then((response) => {
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

      fetch('api/fieldactions', configReqOptions).then((response) => {
        return response.json();
      }).then((response) => {
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
      fetch(`api/estasks/${cancelId}/cancelwith`, configReqOptions).then((response) => {
        return response.json();
      }).then((response) => {
        return resolve(response);
      }).catch((error) => {
        return reject(error);
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
      fetch('api/clusters', configReqOptions).then((response) => {
        return response.json();
      }).then((response) => {
        return resolve(response.data);
      }).catch((error) => {
        return reject(error);
      });
    });
  }
};
