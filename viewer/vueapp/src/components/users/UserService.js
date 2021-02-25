import Vue from 'vue';
import store from '../../store';

// default settings also in internals.js
const defaultSettings = {
  connDstField: 'ip.dst:port',
  connSrcField: 'srcIp',
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
   * Gets a list of users
   * @param {Object} query      Parameters to query the server for specific users
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  getUsers (query) {
    return new Promise((resolve, reject) => {
      const options = {
        url: 'api/users',
        method: 'POST',
        data: query
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
   * Creates a new user
   * @param {Object} newuser    The new user object
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  createUser (newuser) {
    return new Promise((resolve, reject) => {
      Vue.axios.post('api/user', newuser)
        .then((response) => {
          resolve(response);
        }, (error) => {
          reject(error);
        });
    });
  },

  /**
   * Deletes a user
   * @param {Object} user       The user to delete
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  deleteUser (user) {
    return new Promise((resolve, reject) => {
      Vue.axios.delete(`api/user/${user.id}`, user)
        .then((response) => {
          resolve(response);
        }, (error) => {
          reject(error);
        });
    });
  },

  /**
   * Updates a user
   * @param {Object} user       The user to update
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  updateUser (user) {
    return new Promise((resolve, reject) => {
      Vue.axios.post(`api/user/${user.id}`, user)
        .then((response) => {
          resolve(response);
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
        params: { userId: userId }
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
        params: { userId: userId }
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
   * Gets a user's views
   * @param {string} userId     The unique identifier for a user
   *                            (only required if not the current user)
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  getViews (userId) {
    return new Promise((resolve, reject) => {
      const options = {
        url: 'api/user/views',
        method: 'GET',
        params: { userId: userId }
      };

      Vue.axios(options)
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
   * @param {Object} data       The new view data to the server
   *                            { name: 'specialview', expression: 'something == somethingelse'}
   * @param {string} userId     The unique identifier for a user
   *                            (only required if not the current user)
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  createView (data, userId) {
    return new Promise((resolve, reject) => {
      const options = {
        url: 'api/user/view',
        method: 'POST',
        data: data,
        params: { userId: userId }
      };

      Vue.axios(options)
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
   * @param {string} name       The name of the view to delete
   * @param {string} userId     The unique identifier for a user
   *                            (only required if not the current user)
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  deleteView (view, userId) {
    return new Promise((resolve, reject) => {
      const options = {
        url: `api/user/view/${view.name}`,
        method: 'DELETE',
        data: view,
        params: { userId: userId }
      };

      Vue.axios(options)
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
  updateView (data, userId) {
    return new Promise((resolve, reject) => {
      const options = {
        url: `api/user/view/${data.key}`,
        method: 'PUT',
        data: data,
        params: { userId: userId }
      };

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
  toggleShareView (data, userId) {
    return new Promise((resolve, reject) => {
      const options = {
        url: `api/user/view/${data.name}/toggleshare`,
        method: 'POST',
        data: data,
        params: { userId: userId }
      };

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
  getCronQueries (userId) {
    return new Promise((resolve, reject) => {
      const options = {
        url: 'api/user/crons',
        method: 'GET',
        params: { userId: userId }
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
   * Creates a specified cron query for a user
   * @param {Object} data       The cron query data to pass to the server
   * @param {string} userId     The unique identifier for a user
   *                            (only required if not the current user)
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  createCronQuery (data, userId) {
    return new Promise((resolve, reject) => {
      const options = {
        url: 'api/user/cron',
        method: 'POST',
        data: data,
        params: { userId: userId }
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
   * Deletes a user's specified cron query
   * @param {string} key        The key of the cron query to be removed
   * @param {string} userId     The unique identifier for a user
   *                            (only required if not the current user)
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  deleteCronQuery (key, userId) {
    return new Promise((resolve, reject) => {
      const options = {
        url: `api/user/cron/${key}`,
        method: 'DELETE',
        params: { userId: userId }
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
   * Updates a specified cron query for a user
   * @param {Object} data       The cron query data to pass to the server
   * @param {string} userId     The unique identifier for a user
   *                            (only required if not the current user)
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  updateCronQuery (data, userId) {
    return new Promise((resolve, reject) => {
      const options = {
        url: `api/user/cron/${data.key}`,
        method: 'POST',
        data: data,
        params: { userId: userId }
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
        params: { userId: userId }
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
        data: data,
        params: { userId: userId }
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
   * @param {string} name       The name of the column configuration to be removed
   * @param {string} userId     The unique identifier for a user
   *                            (only required if not the current user)
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  deleteColumnConfig (name, userId) {
    return new Promise((resolve, reject) => {
      const options = {
        url: `api/user/column/${name}`,
        method: 'DELETE',
        params: { userId: userId }
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
        data: data,
        params: { userId: userId }
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
        params: { userId: userId }
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
        data: data,
        params: { userId: userId }
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
   * @param {string} name       The name of the spiview fields configuration to be removed
   * @param {string} userId     The unique identifier for a user
   *                            (only required if not the current user)
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  deleteSpiviewFieldConfig (name, userId) {
    return new Promise((resolve, reject) => {
      const options = {
        url: `api/user/spiview/${name}`,
        method: 'POST',
        data: { name: name },
        params: { userId: userId }
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
        data: data,
        params: { userId: userId }
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
   * Changes current user's password
   * @param {object} data       The data to send to the server
   *                            { userId, currentPassword, newPassword }
   * @param {string} userId     The unique identifier for a user
   *                            (only required if not the current user)
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  changePassword (data, userId) {
    return new Promise((resolve, reject) => {
      const options = {
        url: 'api/user/password',
        method: 'POST',
        data: data,
        params: { userId: userId }
      };

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
  getState (name) {
    return new Promise((resolve, reject) => {
      Vue.axios.get(`api/user/state/${name}`)
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
  saveState (state, name) {
    return new Promise((resolve, reject) => {
      const options = {
        url: `api/user/state/${name}`,
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
  parseViews (views) {
    for (const name in views) {
      views[name].name = name;
    }
    return views;
  }

};
