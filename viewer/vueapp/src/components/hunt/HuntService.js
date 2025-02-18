import setReqHeaders from '@real_common/setReqHeaders';

const huntReqOptions = {
  headers: setReqHeaders({ 'Content-Type': 'application/json' })
}

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

      const params = new URLSearchParams(query);

      fetch(`api/hunts${params}`, huntReqOptions).then((response) => {
        return response.json();
      }).then((response) => {
        return resolve(response);
      }).catch((error) => {
        return reject(error);
      });
    });
  },

  /**
   * Creates a new hunt
   * @param {Object} hunt The hunt object
   * @param {string} cluster The cluster name string of the cluster to create the hunt on
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  create (hunt, cluster) {
    return new Promise((resolve, reject) => {
      const options = {
        ...huntReqOptions,
        method: 'POST',
        data: JSON.stringify(hunt),
      };

      const params = new URLSearchParams({ cluster });

      fetch(`api/hunt${params}`, options).then((response) => {
        return response.json();
      }).then((response) => {
        return resolve(response);
      }).catch((error) => {
        return reject(error);
      });
    });
  },

  /**
   * Deletes a hunt
   * @param {string} id The id of the hunt item to delete
   * @param {string} cluster The cluster name string of the cluster to delete the hunt from
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  delete (id, cluster) {
    return new Promise((resolve, reject) => {
      const options = {
        ...huntReqOptions,
        method: 'DELETE',
        params: { cluster }
      };

      const params = new URLSearchParams({ cluster });

      fetch(`api/hunt/${id}${params}`, options).then((response) => {
        return response.json();
      }).then((response) => {
        return resolve(response);
      }).catch((error) => {
        return reject(error);
      });
    });
  },

  /**
   * Cancels a hunt
   * @param {string} id The id of the hunt item to cancel
   * @param {string} cluster The cluster name string of the cluster to cancel the hunt from
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  cancel (id, cluster) {
    return new Promise((resolve, reject) => {
      const options = {
        ...huntReqOptions,
        method: 'PUT'
      };

      const params = new URLSearchParams({ cluster });

      fetch(`api/hunt/${id}/cancel${params}`, options).then((response) => {
        return response.json();
      }).then((response) => {
        return resolve(response);
      }).catch((error) => {
        return reject(error);
      });
    });
  },

  /**
   * Pauses a hunt
   * @param {string} id The id of the hunt item to pause
   * @param {string} cluster The cluster name string of the cluster to pause the hunt from
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  pause (id, cluster) {
    return new Promise((resolve, reject) => {
      const options = {
        ...huntReqOptions,
        method: 'PUT'
      };

      const params = new URLSearchParams({ cluster });

      fetch(`api/hunt/${id}/pause${params}`, options).then((response) => {
        return response.json();
      }).then((response) => {
        return resolve(response);
      }).catch((error) => {
        return reject(error);
      });
    });
  },

  /**
   * Plays a hunt
   * @param {string} id The id of the hunt item to play
   * @param {string} cluster The cluster name string of the cluster to plays the hunt from
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  play (id, cluster) {
    return new Promise((resolve, reject) => {
      const options = {
        ...huntReqOptions,
        method: 'PUT'
      };

      const params = new URLSearchParams({ cluster });

      fetch(`api/hunt/${id}/play${params}`, options).then((response) => {
        return response.json();
      }).then((response) => {
        return resolve(response);
      }).catch((error) => {
        return reject(error);
      });
    });
  },

  /**
   * Updates a hunt - can only update description & roles
   * @param {string} id The id of the hunt
   * @param {object} data The updated description & roles:
                          { description: 'text', roles: ['one', 'two'] }
   * @param {string} cluster The cluster name string of the cluster to updates the hunt from
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  updateHunt (id, data, cluster) {
    return new Promise((resolve, reject) => {
      const options = {
        ...huntReqOptions,
        method: 'PUT',
        data: JSON.stringify(data)
      };

      const params = new URLSearchParams({ cluster });

      fetch(`api/hunt/${id}${params}`, options).then((response) => {
        return response.json();
      }).then((response) => {
        return resolve(response);
      }).catch((error) => {
        return reject(error);
      });
    });
  },

  /**
   * Removes a user from a hunt
   * @param {string} id The id of the hunt
   * @param {string} userid The id of the user to remove
   * @param {string} cluster The cluster name string of the cluster to removes the hunt from
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  removeUser (id, userid, cluster) {
    return new Promise((resolve, reject) => {
      const options = {
        ...huntReqOptions,
        method: 'DELETE'
      };

      const params = new URLSearchParams({ cluster });

      fetch(`api/hunt/${id}/user/${userid}${params}`, options).then((response) => {
        return response.json();
      }).then((response) => {
        return resolve(response);
      }).catch((error) => {
        return reject(error);
      });
    });
  },

  /**
   * Adds users to a hunt
   * @param {string} id The id of the hunt
   * @param {string} users Comma separated list of users
   * @param {string} cluster The cluster name string of the cluster to add users to the hunt from
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  addUsers (id, users, cluster) {
    return new Promise((resolve, reject) => {
      const options = {
        ...huntReqOptions,
        method: 'POST',
        data: JSON.stringify({ users })
      };

      const params = new URLSearchParams({ cluster });

      fetch(`api/hunt/${id}/users${params}`, options).then((response) => {
        return response.json();
      }).then((response) => {
        return resolve(response);
      }).catch((error) => {
        return reject(error);
      });
    });
  },

  /**
   * Removes a hunt name and id from its matched sessions
   * @param {string} id The id of the hunt
   * @param {string} cluster The cluster name string of the cluster to cleanup the hunt on
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  cleanup (id, users, cluster) {
    return new Promise((resolve, reject) => {
      const options = {
        ...huntReqOptions,
        method: 'PUT'
      };

      const params = new URLSearchParams({ cluster });

      fetch(`api/hunt/${id}/removefromsessions${params}`, options).then((response) => {
        return response.json();
      }).then((response) => {
        return resolve(response);
      }).catch((error) => {
        return reject(error);
      });
    });
  },

  /**
   * Determines whether a user has a role assigned to a hunt
   * @param {object} user The user trying to access the hunt
   * @param {object} hunt The hunt being accessed
   * @returns {boolean} - True if the user has a role assigned to the hunt, false otherwise
   */
  userHasHuntRole (user, hunt) {
    if (hunt.roles.length) {
      for (const role of hunt.roles) {
        if (user.roles.indexOf(role) > -1) {
          return true;
        }
      }
    }

    return false;
  },

  /**
   * Determines whether a user can edit a hunt
   * @param {object} user The user trying to access the hunt
   * @param {object} hunt The hunt being accessed
   * @returns {boolean} - True if the user is the creator of the hunt or an Arkime admin
   */
  canEditHunt (user, hunt) {
    const userRoles = user.roles || [];
    return user.userId === hunt.userId || userRoles.includes('arkimeAdmin');
  },

  /**
   * Determines whether a user can view a hunt
   * @param {object} user The user trying to access the hunt
   * @param {object} hunt The hunt being accessed
   * @returns {boolean} - True if the user is the creator of the hunt, or an Arkime admin, or the user has
   *                      been assigned to the hunt, or the user has a role that is assigned to the hunt
   */
  canViewHunt (user, hunt) {
    const huntUsers = hunt.users || [];
    const userRoles = user.roles || [];

    return user.userId === hunt.userId ||
      userRoles.includes('arkimeAdmin') ||
      huntUsers.includes(user.userId) ||
      this.userHasHuntRole(user, hunt);
  },

  /**
   * Determines whether hunt is shared with a user
   * @param {object} user The user trying to access the hunt
   * @param {object} hunt The hunt being accessed
   * @returns {boolean} - True if the user has been assigned to the hunt, or the user has a role
   *                      that is assigned to the hunt
   */
  isShared (user, hunt) {
    const huntUsers = hunt.users || [];
    return this.userHasHuntRole(user, hunt) || huntUsers.includes(user.userId);
  }
};
