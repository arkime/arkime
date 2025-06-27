import store from '../../store';
import { fetchWrapper } from '@/fetchWrapper.js';
import { parseRoles } from '@real_common/vueFilters.js';

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
  async getCurrent () {
    const response = await fetchWrapper({ url: 'api/user' });
    store.commit('setUser', response);
    return response;
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
  async getSettings (userId) {
    const response = await fetchWrapper({ url: 'api/user/settings', params: { userId } });
    let settings = response;
    if (Object.keys(settings).length === 0) {
      settings = store.state.userSettingDefaults;
    }
    return settings;
  },

  /**
   * Updates a user's settings
   * @param {object} settings   The object containing user settings
   * @param {string} userId     The unique identifier for a user
   *                            (only required if not the current user)
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  async saveSettings (settings, userId) {
    // update user settings
    if (!userId || store.state.user.userId === userId) {
      store.commit('setUserSettings', settings);
    }

    return await fetchWrapper({ url: 'api/user/settings', method: 'POST', data: settings, params: { userId } });
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
  async getLayout (type, userId) {
    return await fetchWrapper({ url: `api/user/layouts/${type}`, params: { userId } });
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
  async createLayout (key, data, userId) {
    return await fetchWrapper({ url: `api/user/layouts/${key}`, method: 'POST', data, params: { userId } });
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
  async deleteLayout (layoutType, layoutName, userId) {
    return await fetchWrapper({ url: `api/user/layouts/${layoutType}/${layoutName}`, method: 'DELETE', params: { userId } });
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
  async updateLayout (layoutName, data, userId) {
    return await fetchWrapper({ url: `api/user/layouts/${layoutName}`, method: 'PUT', data, params: { userId } });
  },

  /**
   * Gets a state
   * @param {string} stateName  The name of the state to get
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  async getState (stateName) {
    return await fetchWrapper({ url: `api/user/state/${stateName}` });
  },

  /**
   * Saves a state
   * @param {object} state      The object to save as the state
   * @param {string} stateName  The name of the state to save
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  async saveState (state, stateName) {
    return await fetchWrapper({ url: `api/user/state/${stateName}`, method: 'POST', data: state });
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
  async acknowledgeMsg (msgNum, userId) {
    return await fetchWrapper({ url: `api/user/${userId}/acknowledge`, method: 'PUT', data: { msgNum } });
  },

  /**
   * Gets the page configuration variables
   * @param {string} page The page to request the configuration for
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  async getPageConfig (page) {
    return await fetchWrapper({ url: `api/user/config/${page}` });
  },

  /**
   * Gets roles available to users
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  async getRoles () {
    const response = await fetchWrapper({ url: 'api/user/roles' });
    return parseRoles(response.roles);
  }

};
