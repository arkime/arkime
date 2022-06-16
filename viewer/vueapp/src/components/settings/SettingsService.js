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

  // VIEWS ----------------------------------------------------------------- //
  /**
   * Gets views
   * @param {Object} params     Query params object to search for views
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  getViews (params) {
    return new Promise((resolve, reject) => {
      Vue.axios.get('api/views', { params }).then((response) => {
        store.commit('setViews', response.data.data);
        resolve(response.data);
      }).catch((err) => {
        reject(err);
      });
    });
  },

  /**
   * Creates a view
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
        data,
        method: 'POST',
        url: 'api/view',
        params: { userId }
      };

      Vue.axios(options).then((response) => {
        resolve(response.data);
      }).catch((error) => {
        reject(error.data);
      });
    });
  },

  /**
   * Deletes a view
   * @param {string} viewId     The name of the view to delete
   * @param {string} userId     The unique identifier for a user
   *                            (only required if not the current user)
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  deleteView (viewId, userId) {
    return new Promise((resolve, reject) => {
      const options = {
        method: 'DELETE',
        params: { userId },
        url: `api/view/${viewId}`
      };

      Vue.axios(options).then((response) => {
        resolve(response.data);
      }).catch((error) => {
        reject(error.data);
      });
    });
  },

  /**
   * Updates a view
   * @param {Object} view       The view data to pass to the server
   * @param {string} userId     The unique identifier for a user
   *                            (only required if not the current user)
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  updateView (view, userId) {
    return new Promise((resolve, reject) => {
      const id = view.id;

      // remove client only fields
      delete view.id;
      delete view.changed;

      const options = {
        data: view,
        method: 'PUT',
        params: { userId },
        url: `api/view/${id}`
      };

      Vue.axios(options).then((response) => {
        resolve(response.data);
      }).catch((error) => {
        reject(error.data);
      });
    });
  }
};
