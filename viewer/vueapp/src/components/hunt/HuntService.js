import Vue from 'vue';

export default {
  /**
   * Gets a list of hunts
   * @param {Object} query Parameters to query the server for specific hunts
   * @param {Boolean} huntHistory Whether to retrieve the hunt history (completed hunts)
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  get (query, huntHistory) {
    return new Promise((resolve, reject) => {
      if (history) { query = { ...query, history: huntHistory }; }

      const options = {
        url: 'api/hunts',
        method: 'GET',
        params: query
      };

      Vue.axios(options).then((response) => {
        resolve(response);
      }).catch((error) => {
        reject(error);
      });
    });
  },

  /**
   * Creates a new hunt
   * @param {Object} hunt The hunt object
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  create (hunt) {
    return new Promise((resolve, reject) => {
      const options = {
        url: 'api/hunt',
        method: 'POST',
        data: hunt
      };

      Vue.axios(options).then((response) => {
        resolve(response);
      }).catch((error) => {
        reject(error);
      });
    });
  },

  /**
   * Deletes a hunt
   * @param {string} id The id of the hunt item to delete
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  delete (id) {
    return new Promise((resolve, reject) => {
      const options = {
        url: `api/hunt/${id}`,
        method: 'DELETE'
      };

      Vue.axios(options).then((response) => {
        resolve(response);
      }).catch((error) => {
        reject(error);
      });
    });
  },

  /**
   * Cancels a hunt
   * @param {string} id The id of the hunt item to cancel
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  cancel (id) {
    return new Promise((resolve, reject) => {
      const options = {
        url: `api/hunt/${id}/cancel`,
        method: 'PUT'
      };

      Vue.axios(options).then((response) => {
        resolve(response);
      }).catch((error) => {
        reject(error);
      });
    });
  },

  /**
   * Pauses a hunt
   * @param {string} id The id of the hunt item to pause
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  pause (id) {
    return new Promise((resolve, reject) => {
      const options = {
        url: `api/hunt/${id}/pause`,
        method: 'PUT'
      };

      Vue.axios(options).then((response) => {
        resolve(response);
      }).catch((error) => {
        reject(error);
      });
    });
  },

  /**
   * Plays a hunt
   * @param {string} id The id of the hunt item to play
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  play (id) {
    return new Promise((resolve, reject) => {
      const options = {
        url: `api/hunt/${id}/play`,
        method: 'PUT'
      };

      Vue.axios(options).then((response) => {
        resolve(response);
      }).catch((error) => {
        reject(error);
      });
    });
  },

  /**
   * Removes a user from a hunt
   * @param {string} id The id of the hunt
   * @param {string} userid The id of the user to remove
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  removeUser (id, userid) {
    return new Promise((resolve, reject) => {
      const options = {
        url: `api/hunt/${id}/user/${userid}`,
        method: 'DELETE'
      };

      Vue.axios(options).then((response) => {
        resolve(response);
      }).catch((error) => {
        reject(error);
      });
    });
  },

  /**
   * Adds users to a hunt
   * @param {string} id The id of the hunt
   * @param {string} users Comma separated list of users
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  addUsers (id, users) {
    return new Promise((resolve, reject) => {
      const options = {
        url: `api/hunt/${id}/users`,
        method: 'POST',
        data: { users: users }
      };

      Vue.axios(options).then((response) => {
        resolve(response);
      }).catch((error) => {
        reject(error);
      });
    });
  },

  /**
   * Removes a hunt name and id from its matched sessions
   * @param {string} id The id of the hunt
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  cleanup (id, users) {
    return new Promise((resolve, reject) => {
      const options = {
        url: `api/hunt/${id}/removefromsessions`,
        method: 'PUT'
      };

      Vue.axios(options).then((response) => {
        resolve(response);
      }).catch((error) => {
        reject(error);
      });
    });
  }
};
