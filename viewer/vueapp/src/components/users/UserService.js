import Vue from 'vue';
import store from '../../store';

const defaultSettings = {
  connDstField: 'ip.dst:port',
  connSrcField: 'srcIp',
  detailFormat: 'last',
  numPackets: 'last',
  showTimestamps: 'last',
  sortColumn: 'firstPacket',
  sortDirection: 'desc',
  spiGraph: 'node',
  theme: 'default-theme',
  timezone: 'local'
};

export default {

  /**
   * Gets current user from the server and caches it
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  getCurrent: function () {
    return new Promise((resolve, reject) => {
      Vue.axios.get('user/current')
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
   *                      'createEnabled', 'emailSearch', 'enabled', 'packetSearch',
   *                      'headerAuthEnabled', 'removeEnabled', 'webEnabled',
   *                      '!hideStats', '!hideFiles', '!hidePcap', '!disablePcapDownload'
   * @returns {boolean}   A promise object that signals the completion
   *                            or rejection of the request.
   */
  hasPermission: function (priv) {
    let user = store.state.user;
    if (!user) { return false; }
    let privs = priv.split(',');
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
  getSettings: function (userId) {
    return new Promise((resolve, reject) => {
      let url = 'user/settings';
      if (userId) { url += `?userId=${userId}`; }

      Vue.axios.get(url)
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
  saveSettings: function (settings, userId) {
    return new Promise((resolve, reject) => {
      let options = {
        url: 'user/settings/update',
        method: 'POST',
        data: settings
      };

      if (userId) { options.url += `?userId=${userId}`; }

      // update user settings
      if (!userId || store.state.user.userId === userId) {
        store.commit('setUserSettings', settings);
      }

      Vue.axios(options)
        .then((response) => {
          resolve(response.data);
        }, (error) => {
          reject(error.data);
        });
    });
  },

  /**
   * Gets a user's views
   * @param {string} userId     The unique identifier for a user
   *                            (only required if not the current user)
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  getViews: function (userId) {
    return new Promise((resolve, reject) => {
      let url = 'user/views';
      if (userId) { url += `?userId=${userId}`; }

      Vue.axios.get(url)
        .then((response) => {
          response.data = this.parseViews(response.data);
          resolve(response.data);
        }, (error) => {
          reject(error.data);
        });
    });
  },

  /**
   * Creates a specified view for a user
   * @param {Object} params     The params to pass as data to the server
   *                            { name: 'specialview', expression: 'something == somethingelse'}
   * @param {string} userId     The unique identifier for a user
   *                            (only required if not the current user)
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  createView: function (params, userId) {
    return new Promise((resolve, reject) => {
      let url = 'user/views/create';
      if (userId) { url += `?userId=${userId}`; }

      Vue.axios.post(url, params)
        .then((response) => {
          resolve(response.data);
        }, (error) => {
          reject(error);
        });
    });
  },

  /**
   * Deletes a user's specified view
   * @param {Object} view       The view object to be deleted
   * @param {string} userId     The unique identifier for a user
   *                            (only required if not the current user)
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  deleteView: function (view, userId) {
    return new Promise((resolve, reject) => {
      let url = 'user/views/delete';
      if (userId) { url += `?userId=${userId}`; }

      Vue.axios.post(url, view)
        .then((response) => {
          resolve(response.data);
        }, (error) => {
          reject(error);
        });
    });
  },

  /**
   * Updates a specified view for a user
   * @param {Object} data       The view data to pass to the server
   * @param {string} userId     The unique identifier for a user
   *                            (only required if not the current user)
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  updateView: function (data, userId) {
    return new Promise((resolve, reject) => {
      let options = {
        url: 'user/views/update',
        method: 'POST',
        data: data
      };

      if (userId) { options.url += `?userId=${userId}`; }

      Vue.axios(options)
        .then((response) => {
          response.data.views = this.parseViews(response.data.views);
          resolve(response.data);
        }, (error) => {
          reject(error);
        });
    });
  },

  /**
   * Shares/unshares a specified view for a user
   * @param {Object} data       The view data to pass to the server
   * @param {string} userId     The unique identifier for a user
   *                            (only required if not the current user)
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  toggleShareView: function (data, userId) {
    return new Promise((resolve, reject) => {
      let options = {
        url: 'user/views/toggleShare',
        method: 'POST',
        data: data
      };

      if (userId) { options.url += `?userId=${userId}`; }

      Vue.axios(options)
        .then((response) => {
          resolve(response.data);
        }, (error) => {
          reject(error);
        });
    });
  },

  /**
   * Gets a user's cron queries
   * @param {string} userId     The unique identifier for a user
   *                            (only required if not the current user)
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  getCronQueries: function (userId) {
    return new Promise((resolve, reject) => {
      let options = { url: 'user/cron', method: 'GET' };

      if (userId) { options.url += `?userId=${userId}`; }

      Vue.axios(options)
        .then((response) => {
          resolve(response.data);
        }, (error) => {
          reject(error.data);
        });
    });
  },

  /**
   * Creates a specified cron query for a user
   * @param {Object} data       The cron query data to pass to the server
   * @param {string} userId     The unique identifier for a user
   *                            (only required if not the current user)
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  createCronQuery: function (data, userId) {
    return new Promise((resolve, reject) => {
      let options = {
        url: 'user/cron/create',
        method: 'POST',
        data: data
      };

      if (userId) { options.url += `?userId=${userId}`; }

      Vue.axios(options)
        .then((response) => {
          resolve(response.data);
        }, (error) => {
          reject(error.data);
        });
    });
  },

  /**
   * Deletes a user's specified cron query
   * @param {string} key        The key of the cron query to be removed
   * @param {string} userId     The unique identifier for a user
   *                            (only required if not the current user)
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  deleteCronQuery: function (key, userId) {
    return new Promise((resolve, reject) => {
      let options = {
        url: 'user/cron/delete',
        method: 'POST',
        data: { key: key }
      };

      if (userId) { options.url += `?userId=${userId}`; }

      Vue.axios(options)
        .then((response) => {
          resolve(response.data);
        }, (error) => {
          reject(error.data);
        });
    });
  },

  /**
   * Updates a specified cron query for a user
   * @param {Object} data       The cron query data to pass to the server
   * @param {string} userId     The unique identifier for a user
   *                            (only required if not the current user)
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  updateCronQuery: function (data, userId) {
    return new Promise((resolve, reject) => {
      let options = {
        url: 'user/cron/update',
        method: 'POST',
        data: data
      };

      if (userId) { options.url += `?userId=${userId}`; }

      Vue.axios(options)
        .then((response) => {
          resolve(response.data);
        }, (error) => {
          reject(error.data);
        });
    });
  },

  /**
   * Gets a user's custom column configurations
   * @param {string} userId     The unique identifier for a user
   *                            (only required if not the current user)
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  getColumnConfigs: function (userId) {
    return new Promise((resolve, reject) => {
      let url = 'user/columns';
      if (userId) { url += `?userId=${userId}`; }

      Vue.axios.get(url)
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
  createColumnConfig: function (data, userId) {
    return new Promise((resolve, reject) => {
      let options = {
        url: 'user/columns/create',
        method: 'POST',
        data: data
      };

      if (userId) { options.url += `?userId=${userId}`; }

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
   * @param {string} name       The name of the column configuration to be removed
   * @param {string} userId     The unique identifier for a user
   *                            (only required if not the current user)
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  deleteColumnConfig: function (name, userId) {
    return new Promise((resolve, reject) => {
      let options = {
        url: 'user/columns/delete',
        method: 'POST',
        data: { name: name }
      };

      if (userId) { options.url += `?userId=${userId}`; }

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
  updateColumnConfig: function (data, userId) {
    return new Promise((resolve, reject) => {
      let options = {
        url: `user/columns/${data.name}`,
        method: 'PUT',
        data: data
      };

      if (userId) { options.url += `?userId=${userId}`; }

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
  getSpiviewFields: function (userId) {
    return new Promise((resolve, reject) => {
      let options = { url: 'user/spiview/fields', method: 'GET' };

      if (userId) { options.url += `?userId=${userId}`; }

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
  createSpiviewFieldConfig: function (data, userId) {
    return new Promise((resolve, reject) => {
      let options = {
        url: 'user/spiview/fields/create',
        method: 'POST',
        data: data
      };

      if (userId) { options.url += `?userId=${userId}`; }

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
   * @param {string} name       The name of the spiview fields configuration to be removed
   * @param {string} userId     The unique identifier for a user
   *                            (only required if not the current user)
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  deleteSpiviewFieldConfig: function (name, userId) {
    return new Promise((resolve, reject) => {
      let options = {
        url: 'user/spiview/fields/delete',
        method: 'POST',
        data: { name: name }
      };

      if (userId) { options.url += `?userId=${userId}`; }

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
  updateSpiviewFieldConfig: function (data, userId) {
    return new Promise((resolve, reject) => {
      let options = {
        url: `user/spiview/fields/${data.name}`,
        method: 'PUT',
        data: data
      };

      if (userId) { options.url += `?userId=${userId}`; }

      Vue.axios(options)
        .then((response) => {
          resolve(response.data);
        }, (error) => {
          reject(error.data);
        });
    });
  },

  /**
   * Changes current user's password
   * @param {object} data       The data to send to the server
   *                            { userId, currentPassword, newPassword }
   * @param {string} userId     The unique identifier for a user
   *                            (only required if not the current user)
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  changePassword: function (data, userId) {
    return new Promise((resolve, reject) => {
      let options = {
        url: 'user/password/change',
        method: 'POST'
      };

      if (userId) {
        options.url += `?userId=${userId}`;
        options.data = { newPassword: data.newPassword };
      } else { options.data = data; }

      Vue.axios(options)
        .then((response) => {
          resolve(response.data);
        }, (error) => {
          reject(error);
        });
    });
  },

  /**
   * Gets a state
   * @param {string} name       The name of the state to get
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  getState: function (name) {
    return new Promise((resolve, reject) => {
      Vue.axios.get(`state/${name}`)
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
   * @param {string} name       The name of the state to save
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  saveState: function (state, name) {
    return new Promise((resolve, reject) => {
      let options = {
        url: `state/${name}`,
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
  acknowledgeMsg: function (msgNum, userId) {
    return new Promise((resolve, reject) => {
      let options = {
        url: `user/${userId}/acknowledgeMsg`,
        method: 'PUT',
        data: { msgNum: msgNum }
      };

      Vue.axios(options)
        .then((response) => {
          resolve(response);
        }, (error) => {
          reject(error);
        });
    });
  },

  /* internal methods ---------------------------------------------------- */
  /**
   * Adds the name as a property on a view (instead of just key)
   * @param {object} views    The object containing view objects
   * @returns {object} views  The object containing views, now with name
   *                          properties on each view object
   */
  parseViews: function (views) {
    for (var name in views) {
      if (views.hasOwnProperty(name)) {
        views[name].name = name;
      }
    }
    return views;
  }

};
