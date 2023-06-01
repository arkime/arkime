import Vue from 'vue';
import store from '../../store';

// default settings also in internals.js
const defaultSettings = {
  connDstField: 'ip.dst:port',
  connSrcField: 'source.ip',
  detailFormat: 'last',
  numPackets: 'last',
  showTimestamps: 'last',
  sortColumn: 'firstPacket',
  sortDirection: 'desc',
  spiGraph: 'node',
  theme: 'arkime-light-theme',
  timezone: 'local',
  manualQuery: false,
  timelineDataFilters: ['totPackets', 'totBytes', 'totDataBytes'] // dbField2 values from fields
};

export default {

  /* returns the default user settings */
  getDefaultSettings () {
    return defaultSettings;
  },

  /**
   * Gets current user from the server and caches it
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  getCurrent () {
    return new Promise((resolve, reject) => {
      Vue.axios.get('api/user')
        .then((response) => {
          store.commit('setUser', response.data);
          resolve(response.data);
        }, (error) => {
          reject(error);
        });
    });
  },

  /**
   * Determines whether a user has permission to perform a specific task
   * @param {string} priv The privilege in question. Values include:
   *                      'emailSearch', 'enabled', 'packetSearch',
   *                      'headerAuthEnabled', 'removeEnabled', 'webEnabled',
   *                      '!hideStats', '!hideFiles', '!hidePcap', '!disablePcapDownload'
   * @returns {boolean}   true if all permissions are included.
   */
  hasPermission (priv) {
    const user = store.state.user;
    if (!user) { return false; }
    const privs = priv.split(',');
    for (let p of privs) {
      let reverse = false;
      if (p.startsWith('!')) {
        reverse = true;
        p = p.substr(1);
      }
      if ((!reverse && !user[p]) ||
        (reverse && user[p])) {
        return false;
      }
    }
    return true;
  },

  /**
   * Gets a user's settings
   * @param {string} userId     The unique identifier for a user
   *                            (only required if not the current user)
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  getSettings (userId) {
    return new Promise((resolve, reject) => {
      const options = {
        url: 'api/user/settings',
        method: 'GET',
        params: { userId }
      };

      Vue.axios(options)
        .then((response) => {
          let settings = response.data;
          // if the settings are empty, set smart default
          if (Object.keys(settings).length === 0) {
            settings = defaultSettings;
          }
          resolve(settings);
        }, (error) => {
          reject(error);
        });
    });
  },

  /**
   * Updates a user's settings
   * @param {object} settings   The object containing user settings
   * @param {string} userId     The unique identifier for a user
   *                            (only required if not the current user)
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  saveSettings (settings, userId) {
    return new Promise((resolve, reject) => {
      const options = {
        url: 'api/user/settings',
        method: 'POST',
        data: settings,
        params: { userId }
      };

      // update user settings
      if (!userId || store.state.user.userId === userId) {
        store.commit('setUserSettings', settings);
      }

      Vue.axios(options)
        .then((response) => {
          resolve(response.data);
        }, (error) => {
          reject(error);
        });
    });
  },

  /**
   * Resets a user's settings
   * @param {string} userId     The unique identifier for a user
   *                            (only required if not the current user)
   * @param {string} theme      Current theme identifier.
   *                            Avoid resetting the current theme if it exists
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  resetSettings (userId, theme) {
    const settings = JSON.parse(JSON.stringify(defaultSettings));

    if (theme) {
      settings.theme = theme;
    }

    return this.saveSettings(settings, userId);
  },

  /**
   * Gets a user's custom column configurations
   * @param {string} userId     The unique identifier for a user
   *                            (only required if not the current user)
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  getColumnConfigs (userId) {
    return new Promise((resolve, reject) => {
      const options = {
        url: 'api/user/columns',
        method: 'GET',
        params: { userId }
      };

      Vue.axios(options)
        .then((response) => {
          resolve(response.data);
        }, (error) => {
          reject(error.data);
        });
    });
  },

  /**
   * Creates a specified custom column configuration for a user
   * @param {Object} data       The data to pass to the server
   *                            { name: 'namey', columns: ['field1', 'field2', ... , 'fieldN']}
   * @param {string} userId     The unique identifier for a user
   *                            (only required if not the current user)
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  createColumnConfig (data, userId) {
    return new Promise((resolve, reject) => {
      const options = {
        url: 'api/user/column',
        method: 'POST',
        data,
        params: { userId }
      };

      Vue.axios(options)
        .then((response) => {
          resolve(response.data);
        }, (error) => {
          reject(error.data);
        });
    });
  },

  /**
   * Deletes a user's specified custom column configuration
   * @param {string} colName    The name of the column configuration to be removed
   * @param {string} userId     The unique identifier for a user
   *                            (only required if not the current user)
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  deleteColumnConfig (colName, userId) {
    return new Promise((resolve, reject) => {
      const options = {
        url: `api/user/column/${colName}`,
        method: 'DELETE',
        params: { userId }
      };

      Vue.axios(options)
        .then((response) => {
          resolve(response.data);
        }, (error) => {
          reject(error.data);
        });
    });
  },

  /**
   * Updates a user's specified custom column configuration
   * @param {string} data       The column configuration object to be updated
   * @param {string} userId     The unique identifier for a user
   *                            (only required if not the current user)
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  updateColumnConfig (data, userId) {
    return new Promise((resolve, reject) => {
      const options = {
        url: `api/user/column/${data.name}`,
        method: 'PUT',
        data,
        params: { userId }
      };

      Vue.axios(options)
        .then((response) => {
          resolve(response.data);
        }, (error) => {
          reject(error.data);
        });
    });
  },

  /**
   * Gets a user's custom spiview fields configurations
   * @param {string} userId     The unique identifier for a user
   *                            (only required if not the current user)
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  getSpiviewFields (userId) {
    return new Promise((resolve, reject) => {
      const options = {
        url: 'api/user/spiview',
        method: 'GET',
        params: { userId }
      };

      Vue.axios(options)
        .then((response) => {
          resolve(response.data);
        }, (error) => {
          reject(error.data);
        });
    });
  },

  /**
   * Creates a specified custom spiview fields configuration for a user
   * @param {Object} data       The data to pass to the server
   *                            { name: 'namey', fields: ['field1', 'field2', ... , 'fieldN']}
   * @param {string} userId     The unique identifier for a user
   *                            (only required if not the current user)
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  createSpiviewFieldConfig (data, userId) {
    return new Promise((resolve, reject) => {
      const options = {
        url: 'api/user/spiview',
        method: 'POST',
        data,
        params: { userId }
      };

      Vue.axios(options)
        .then((response) => {
          resolve(response.data);
        }, (error) => {
          reject(error.data);
        });
    });
  },

  /**
   * Deletes a user's specified custom spiview fields configuration
   * @param {string} spiName   The name of the spiview fields configuration to be removed
   * @param {string} userId     The unique identifier for a user
   *                            (only required if not the current user)
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  deleteSpiviewFieldConfig (spiName, userId) {
    return new Promise((resolve, reject) => {
      const options = {
        url: `api/user/spiview/${spiName}`,
        method: 'POST',
        data: { name: spiName },
        params: { userId }
      };

      Vue.axios(options)
        .then((response) => {
          resolve(response.data);
        }, (error) => {
          reject(error.data);
        });
    });
  },

  /**
   * Updates a user's specified custom spiview fields configuration
   * @param {string} data       The name of the spiview fields configuration to be updated
   * @param {string} userId     The unique identifier for a user
   *                            (only required if not the current user)
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  updateSpiviewFieldConfig (data, userId) {
    return new Promise((resolve, reject) => {
      const options = {
        url: `api/user/spiview/${data.name}`,
        method: 'PUT',
        data,
        params: { userId }
      };

      Vue.axios(options)
        .then((response) => {
          resolve(response.data);
        }, (error) => {
          reject(error.data);
        });
    });
  },

  /**
   * Gets a state
   * @param {string} stateName  The name of the state to get
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  getState (stateName) {
    return new Promise((resolve, reject) => {
      Vue.axios.get(`api/user/state/${stateName}`)
        .then((response) => {
          resolve(response);
        }, (error) => {
          reject(error);
        });
    });
  },

  /**
   * Saves a state
   * @param {object} state      The object to save as the state
   * @param {string} stateName  The name of the state to save
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  saveState (state, stateName) {
    return new Promise((resolve, reject) => {
      const options = {
        url: `api/user/state/${stateName}`,
        method: 'POST',
        data: state
      };

      Vue.axios(options)
        .then((response) => {
          resolve(response);
        }, (error) => {
          reject(error);
        });
    });
  },

  /**
  * Acknowledge the welcome message for a user
  * @param {number} msgNum     The message number
  *                            { userId, currentPassword, newPassword }
  * @param {string} userId     The unique identifier for a user
  *                            (only required if not the current user)
  * @returns {Promise} Promise A promise object that signals the completion
  *                            or rejection of the request.
   */
  acknowledgeMsg (msgNum, userId) {
    return new Promise((resolve, reject) => {
      const options = {
        url: `api/user/${userId}/acknowledge`,
        method: 'PUT',
        data: { msgNum }
      };

      Vue.axios(options)
        .then((response) => {
          resolve(response);
        }, (error) => {
          reject(error);
        });
    });
  },

  /**
   * Gets the page configruation variables
   * @param {string} page The page to request the configuration for
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  getPageConfig (page) {
    return new Promise((resolve, reject) => {
      Vue.axios.get(`api/user/config/${page}`).then((response) => {
        resolve(response.data);
      }).catch((error) => {
        reject(error);
      });
    });
  },

  /**
   * Gets roles available to users
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  getRoles () {
    return new Promise((resolve, reject) => {
      Vue.axios.get('api/user/roles').then((response) => {
        const roles = Vue.filter('parseRoles')(response.data.roles);
        return resolve(roles);
      }).catch((err) => {
        return reject(err);
      });
    });
  }

};
