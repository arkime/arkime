import store from '@/store';
import setReqHeaders from '../../../../../common/vueapp/setReqHeaders';

export default {
  /**
   * Fetches the list of user roles.
   * @returns {Promise} - The promise that either resovles the request or rejects in error
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
   * @returns {Promise} - The promise that either resovles the request or rejects in error
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
   * @returns {Promise} - The promise that either resovles the request or rejects in error
   */
  setUserSettings (settings) {
    return new Promise((resolve, reject) => {
      fetch('api/settings', {
        method: 'PUT',
        headers: setReqHeaders({ 'Content-Type': 'application/json' }),
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
   * @returns {Promise} - The promise that either resovles the request or rejects in error
   */
  getRoles () {
    return new Promise((resolve, reject) => {
      fetch('api/roles', {
        headers: setReqHeaders()
      }).then((response) => {
        if (!response.ok) { // test for bad response code
          throw new Error(response.statusText);
        }
        return response.json();
      }).then((response) => {
        const roles = [];
        for (let role of response.roles) {
          let userDefined = false;
          const roleId = role;
          if (role.startsWith('role:')) {
            role = role.slice(5);
            userDefined = true;
          }
          role = { text: role, value: roleId, userDefined };
          roles.push(role);
        }
        store.commit('SET_ROLES', roles);
        return resolve(roles);
      }).catch((err) => { // this catches an issue within the ^ .then
        return reject(err);
      });
    });
  },

  /**
   * Fetches the integration settings for this user
   * @returns {Promise} - The promise that either resovles the request or rejects in error
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
   * @returns {Promise} - The promise that either resovles the request or rejects in error
   */
  setIntegrationSettings (settings) {
    return new Promise((resolve, reject) => {
      fetch('api/integration/settings', {
        method: 'PUT',
        headers: setReqHeaders({ 'Content-Type': 'application/json' }),
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
   * Fetches a list of integration views
   * @returns {Promise} - The promise that either resovles the request or rejects in error
   */
  getIntegrationViews () {
    return new Promise((resolve, reject) => {
      fetch('api/views', {
        method: 'GET',
        headers: { 'Content-Type': 'application/json' }
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
   * Saves a list of integrations as a view
   * @param {Object} view - The view to save { name: String, integrations: [], canView: [roles], canEdit: [roles] }
   * @returns {Promise} - The promise that either resovles the request or rejects in error
   */
  saveIntegrationsView (view) {
    return new Promise((resolve, reject) => {
      fetch('api/view', {
        method: 'POST',
        headers: setReqHeaders({ 'Content-Type': 'application/json' }),
        body: JSON.stringify(view)
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
   * Updates a view
   * @param {Object} view - The view to save { name: String, integrations: [], canView: [roles], canEdit: [roles] }
   * @returns {Promise} - The promise that either resovles the request or rejects in error
   */
  updateIntegrationsView (view) {
    return new Promise((resolve, reject) => {
      fetch(`api/view/${view._id}`, {
        method: 'PUT',
        headers: setReqHeaders({ 'Content-Type': 'application/json' }),
        body: JSON.stringify(view)
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
   * Deletes a view
   * @param {String} id - The id of the view to delete
   * @returns {Promise} - The promise that either resovles the request or rejects in error
   */
  deleteIntegrationsView (id) {
    return new Promise((resolve, reject) => {
      fetch(`api/view/${id}`, {
        method: 'DELETE',
        headers: setReqHeaders()
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
