import Vue from 'vue';
import store from '../../store';

export default {
  // NOTIFIERS ------------------------------------------------------------- //
  /**
   * Gets a list of notifier types (notifiers that a user can create)
   * @returns {Promise} Promise - A promise object that signals the completion
   *                              or rejection of the request.
   */
  getNotifierTypes: function () {
    return new Promise((resolve, reject) => {
      Vue.axios.get('api/notifierTypes').then((response) => {
        resolve(response.data);
      }).catch((err) => {
        reject(err);
      });
    });
  },

  /**
   * Gets a list of user configured notifiers
   * NOTE: updates the store with the list of notifiers returned
   * @returns {Promise} Promise - A promise object that signals the completion
   *                              or rejection of the request.
   */
  getNotifiers: function () {
    return new Promise((resolve, reject) => {
      Vue.axios.get('api/notifiers').then((response) => {
        store.commit('setNotifiers', response.data);
        resolve(response.data);
      }).catch((err) => {
        reject(err);
      });
    });
  },

  /**
   * Creates a new notifier
   * @param {Object} notifier - The new notifier to create
   * @returns {Promise} Promise - A promise object that signals the completion
   *                              or rejection of the request.
   */
  createNotifier: function (notifier) {
    return new Promise((resolve, reject) => {
      const options = {
        url: 'api/notifier',
        method: 'POST',
        data: notifier
      };

      Vue.axios(options).then((response) => {
        resolve(response.data);
      }).catch((error) => {
        reject(error);
      });
    });
  },

  /**
   * Updates a notifier
   * @param {String} id - The unique id of the notifier to update
   * @param {Object} notifier - The notifier to update
   * @returns {Promise} Promise - A promise object that signals the completion
   *                              or rejection of the request.
   */
  updateNotifier (id, notifier) {
    return new Promise((resolve, reject) => {
      Vue.axios.put(`api/notifier/${id}`, notifier).then((response) => {
        resolve(response.data);
      }).catch((error) => {
        reject(error);
      });
    });
  },

  /**
   * Deletes a notifier
   * @param {String} id - The id of the notifier to delete
   * @returns {Promise} Promise - A promise object that signals the completion
   *                              or rejection of the request.
   */
  deleteNotifier (id) {
    return new Promise((resolve, reject) => {
      Vue.axios.delete(`api/notifier/${id}`).then((response) => {
        resolve(response.data);
      }).catch((error) => {
        reject(error);
      });
    });
  },

  /**
   * Tests a notifier
   * @param {String} id - The id of the notifier to test
   * @returns {Promise} Promise - A promise object that signals the completion
   *                              or rejection of the request.
   */
  testNotifier: function (id) {
    return new Promise((resolve, reject) => {
      const options = {
        url: `api/notifier/${id}/test`,
        method: 'POST',
        data: {}
      };

      Vue.axios(options).then((response) => {
        resolve(response.data);
      }).catch((error) => {
        reject(error);
      });
    });
  },

  // SHORTCUTS ------------------------------------------------------------- //
  /**
   * Gets a list of user configured shortcuts
   * @param {Object} params - Query params object to search for shortcuts
   * @returns {Promise} Promise - A promise object that signals the completion
   *                              or rejection of the request.
   */
  getShortcuts: function (params) {
    return new Promise((resolve, reject) => {
      Vue.axios.get('api/shortcuts', { params }).then((response) => {
        resolve(response.data);
      }).catch((err) => {
        reject(err);
      });
    });
  },

  /**
   * Creates a new shortcut
   * @param {Object} shortcut - The new shortcut to create
   * @returns {Promise} Promise - A promise object that signals the completion
   *                              or rejection of the request.
   */
  createShortcut: function (shortcut) {
    return new Promise((resolve, reject) => {
      const options = {
        url: 'api/shortcut',
        method: 'POST',
        data: shortcut
      };

      Vue.axios(options).then((response) => {
        resolve(response.data);
      }).catch((error) => {
        reject(error);
      });
    });
  },

  /**
   * Updates a shortcut
   * @param {String} id - The unique id of the shortcut to update
   * @param {Object} shortcut - The shortcut to update
   * @returns {Promise} Promise - A promise object that signals the completion
   *                              or rejection of the request.
   */
  updateShortcut (id, shortcut) {
    return new Promise((resolve, reject) => {
      Vue.axios.put(`api/shortcut/${id}`, shortcut).then((response) => {
        resolve(response.data);
      }).catch((error) => {
        reject(error);
      });
    });
  },

  /**
   * Deletes a shortcut
   * @param {String} id - The unique id of the shortcut to delete
   * @returns {Promise} Promise - A promise object that signals the completion
   *                              or rejection of the request.
   */
  deleteShortcut (id) {
    return new Promise((resolve, reject) => {
      Vue.axios.delete(`api/shortcut/${id}`).then((response) => {
        resolve(response.data);
      }).catch((error) => {
        reject(error);
      });
    });
  },

  // TODO update store
  // TODO don't need to pass in userid
  // VIEWS ----------------------------------------------------------------- //
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
        url: 'api/views',
        method: 'GET',
        params: { userId: userId }
      };

      Vue.axios(options).then((response) => {
        resolve(response.data);
      }).catch((error) => {
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

      Vue.axios(options).then((response) => {
        resolve(response.data);
      }).catch((error) => {
        reject(error.data);
      });
    });
  },

  /**
   * Deletes a user's specified view
   * @param {string} viewId     The name of the view to delete
   * @param {string} userId     The unique identifier for a user
   *                            (only required if not the current user)
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  deleteView (viewId, userId) {
    return new Promise((resolve, reject) => {
      const options = {
        url: `api/view/${viewId}`,
        method: 'DELETE',
        params: { userId: userId }
      };

      Vue.axios(options).then((response) => {
        resolve(response.data);
      }).catch((error) => {
        reject(error.data);
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

      Vue.axios(options).then((response) => {
        resolve(response.data);
      }).catch((error) => {
        reject(error.data);
      });
    });
  }
};
