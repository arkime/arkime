import store from '@/store';

export default {
  /**
   * Fetches the list of user roles.
   * @returns {Promise} - The promise that either resovles the or rejects in error
   */
  getUser () {
    return new Promise((resolve, reject) => {
      fetch('api/user').then((response) => {
        if (!response.ok) { // test for bad response code
          throw new Error(response.statusText);
        }
        return response.json();
      }).then((response) => {
        store.commit('SET_USER', response);
        return resolve(response);
      }).catch((err) => { // this catches an issue within the ^ .then
        return reject(err);
      });
    });
  },

  /**
   * Fetches users settings.
   * @returns {Promise} - The promise that either resovles the or rejects in error
   */
  getUserSettings () {
    return new Promise((resolve, reject) => {
      fetch('api/settings').then((response) => {
        if (!response.ok) { // test for bad response code
          throw new Error(response.statusText);
        }
        return response.json();
      }).then((response) => {
        return resolve(response);
      }).catch((err) => { // this catches an issue within the ^ .then
        return reject(err);
      });
    });
  },

  /**
   * Set the users settings
   * @param {Object} settings - The settings to update
   * @returns {Promise} - The promise that either resovles the or rejects in error
   */
  setUserSettings (settings) {
    return new Promise((resolve, reject) => {
      fetch('api/settings', {
        method: 'PUT',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(settings)
      }).then((response) => {
        return response.json();
      }).then((response) => {
        if (response.success) {
          return resolve(response);
        } else {
          return reject(response.text);
        }
      });
    });
  },

  /**
   * Fetches the list of user roles.
   * @returns {Promise} - The promise that either resovles the or rejects in error
   */
  getRoles () {
    return new Promise((resolve, reject) => {
      fetch('api/roles').then((response) => {
        if (!response.ok) { // test for bad response code
          throw new Error(response.statusText);
        }
        return response.json();
      }).then((response) => {
        store.commit('SET_ROLES', response.roles);
        return resolve(response);
      }).catch((err) => { // this catches an issue within the ^ .then
        return reject(err);
      });
    });
  },

  /**
   * Fetches the integration settings for this user
   * @returns {Promise} - The promise that either resovles the or rejects in error
   */
  getIntegrationSettings () {
    return new Promise((resolve, reject) => {
      fetch('api/integration/settings').then((response) => {
        if (!response.ok) { // test for bad response code
          throw new Error(response.statusText);
        }
        return response.json();
      }).then((response) => {
        return resolve(response.settings);
      }).catch((err) => { // this catches an issue within the ^ .then
        return reject(err);
      });
    });
  },

  /**
   * Set the settings for integrations
   * @param {Object} settings - The settings to update
   * @returns {Promise} - The promise that either resovles the or rejects in error
   */
  setIntegrationSettings (settings) {
    return new Promise((resolve, reject) => {
      fetch('api/integration/settings', {
        method: 'PUT',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(settings)
      }).then((response) => {
        return response.json();
      }).then((response) => {
        if (response.success) {
          return resolve(response);
        } else {
          return reject(response.text);
        }
      });
    });
  }
};
