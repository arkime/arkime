import store from '../../store';
import setReqHeaders from '@real_common/setReqHeaders';
import { parseRoles } from '@real_common/vueFilters.js'

const userReqOptions = {
  headers: setReqHeaders({ 'Content-Type': 'application/json' })
}

export default {

  /* returns the default user settings */
  getDefaultSettings () {
    return store.state.userSettingDefaults;
  },

  /**
   * Gets current user from the server and caches it
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  getCurrent () {
    return new Promise((resolve, reject) => {
      fetch('api/user', userReqOptions).then((response) => {
        return response.json();
      }).then((response) => {
        store.commit('setUser', response.data);
        return resolve(response.data);
      }).catch((err) => {
        return reject(err);
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
      const params = new URLSearchParams({ userId });

      fetch(`api/user/settings${params}`, userReqOptions).then((response) => {
        return response.json();
      }).then((response) => {
        let settings = response.data;
        if (Object.keys(settings).length === 0) {
          settings = store.state.userSettingDefaults;
        }
        return resolve(settings);
      }).catch((err) => {
        return reject(err);
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
        ...userReqOptions,
        method: 'POST',
        data: JSON.stringify(settings)
      };

      const params = new URLSearchParams({ userId });

      // update user settings
      if (!userId || store.state.user.userId === userId) {
        store.commit('setUserSettings', settings);
      }

      fetch(`api/user/settings${params}`, options).then((response) => {
        return response.json();
      }).then((response) => {
        return resolve(response.data);
      }).catch((err) => {
        return reject(err);
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
    const settings = JSON.parse(JSON.stringify(store.state.userSettingDefaults));

    if (theme) {
      settings.theme = theme;
    }

    return this.saveSettings(settings, userId);
  },

  /**
   * Gets a user's custom layouts
   * @param {string} type       The type of the layout to get
   * @param {string} userId     The unique identifier for a user
   *                            (only required if not the current user)
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  getLayout (type, userId) {
    return new Promise((resolve, reject) => {
      const params = new URLSearchParams({ userId });

      fetch(`api/user/layouts/${type}${params}`, userReqOptions).then((response) => {
        return response.json();
      }).then((response) => {
        return resolve(response.data);
      }).catch((err) => {
        return reject(err);
      });
    });
  },

  /**
   * Creates a specified custom configuration for a user
   * @param {string} key        The key of the configuration to be created
   * @param {Object} data       The data to pass to the server
   * @param {string} userId     The unique identifier for a user
   *                            (only required if not the current user)
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  createLayout (key, data, userId) {
    return new Promise((resolve, reject) => {
      const options = {
        ...userReqOptions,
        method: 'POST',
        data: JSON.stringify(data)
      }

      const params = new URLSearchParams({ userId })

      fetch(`api/user/layouts/${key}${params}`, options).then((response) => {
        return response.json();
      }).then((response) => {
        return resolve(response.data);
      }).catch((err) => {
        return reject(err);
      });
    });
  },

  /**
   * Deletes a user's specified layout
   * @param {string} key        The key of the layout to be removed
   * @param {string} layoutName The name of the layout to be removed
   * @param {string} userId     The unique identifier for a user
   *                            (only required if not the current user)
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  deleteLayout (layoutType, layoutName, userId) {
    return new Promise((resolve, reject) => {
      const options = {
        ...userReqOptions,
        method: 'DELETE'
      };

      const params = new URLSearchParams({ userId })

      fetch(`api/user/layouts/${layoutType}/${layoutName}${params}`, options).then((response) => {
        return response.json();
      }).then((response) => {
        return resolve(response.data);
      }).catch((err) => {
        return reject(err);
      });
    });
  },

  /**
   * Updates a user's specified custom column configuration
   * @param {string} layoutName The name of the configuration to be updated
   * @param {string} data       The configuration object to be updated
   * @param {string} userId     The unique identifier for a user
   *                            (only required if not the current user)
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  updateLayout (layoutName, data, userId) {
    return new Promise((resolve, reject) => {
      const options = {
        ...userReqOptions,
        method: 'PUT',
        data: JSON.stringify(data)
      };

      const params = new URLSearchParams({ userId })

      fetch(`api/user/layouts/${layoutName}${params}`, options).then((response) => {
        return response.json();
      }).then((response) => {
        return resolve(response.data);
      }).catch((err) => {
        return reject(err);
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
      fetch(`api/user/state/${stateName}`, userReqOptions).then((response) => {
        return response.json();
      }).then((response) => {
        return resolve(response.data);
      }).catch((err) => {
        return reject(err);
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
        ...userReqOptions,
        method: 'POST',
        data: JSON.stringify(state)
      };

      const params = new URLSearchParams({ userId })

      fetch(`api/user/state/${stateName}${params}`, options).then((response) => {
        return response.json();
      }).then((response) => {
        return resolve(response);
      }).catch((err) => {
        return reject(err);
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
        ...userReqOptions,
        method: 'PUT',
        data: JSON.stringify({ msgNum })
      };

      const params = new URLSearchParams({ userId })

      fetch(`api/user/${userId}/acknowledge${params}`, options).then((response) => {
        return response.json();
      }).then((response) => {
        return resolve(response);
      }).catch((err) => {
        return reject(err);
      });
    });
  },

  /**
   * Gets the page configuration variables
   * @param {string} page The page to request the configuration for
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  getPageConfig (page) {
    return new Promise((resolve, reject) => {
      fetch(`api/user/config/${page}`, userReqOptions).then((response) => {
        return response.json();
      }).then((response) => {
        return resolve(response.data);
      }).catch((err) => {
        return reject(err);
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
      fetch('api/user/roles', userReqOptions).then((response) => {
        return response.json();
      }).then((response) => {
        const roles = parseRoles(response.roles);
        return resolve(roles);
      }).catch((err) => {
        return reject(err);
      });
    });
  }

};
