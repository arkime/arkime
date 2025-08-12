import { fetchWrapper } from '@real_common/fetchWrapper.js';

export default {
  /**
   * Gets a list of hunts
   * @param {Object} query Parameters to query the server for specific hunts
   * @param {Boolean} huntHistory Whether to retrieve the hunt history (completed hunts)
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  async get (query, huntHistory) {
    if (huntHistory) { query = { ...query, history: huntHistory }; }
    return await fetchWrapper({ url: 'api/hunts', params: query });
  },

  /**
   * Creates a new hunt
   * @param {Object} hunt The hunt object
   * @param {string} cluster The cluster name string of the cluster to create the hunt on
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  async create (hunt, cluster) {
    return await fetchWrapper({ url: 'api/hunt', method: 'POST', data: hunt, params: { cluster } });
  },

  /**
   * Deletes a hunt
   * @param {string} id The id of the hunt item to delete
   * @param {string} cluster The cluster name string of the cluster to delete the hunt from
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  async delete (id, cluster) {
    return await fetchWrapper({ url: `api/hunt/${id}`, method: 'DELETE', params: { cluster } });
  },

  /**
   * Cancels a hunt
   * @param {string} id The id of the hunt item to cancel
   * @param {string} cluster The cluster name string of the cluster to cancel the hunt from
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  async cancel (id, cluster) {
    return await fetchWrapper({ url: `api/hunt/${id}/cancel`, method: 'PUT', params: { cluster } });
  },

  /**
   * Pauses a hunt
   * @param {string} id The id of the hunt item to pause
   * @param {string} cluster The cluster name string of the cluster to pause the hunt from
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  async pause (id, cluster) {
    return await fetchWrapper({ url: `api/hunt/${id}/pause`, method: 'PUT', params: { cluster } });
  },

  /**
   * Plays a hunt
   * @param {string} id The id of the hunt item to play
   * @param {string} cluster The cluster name string of the cluster to plays the hunt from
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  async play (id, cluster) {
    return await fetchWrapper({ url: `api/hunt/${id}/play`, method: 'PUT', params: { cluster } });
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
  async updateHunt (id, data, cluster) {
    return await fetchWrapper({ url: `api/hunt/${id}`, method: 'PUT', data, params: { cluster } });
  },

  /**
   * Removes a user from a hunt
   * @param {string} id The id of the hunt
   * @param {string} userid The id of the user to remove
   * @param {string} cluster The cluster name string of the cluster to removes the hunt from
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  async removeUser (id, userid, cluster) {
    return await fetchWrapper({ url: `api/hunt/${id}/user/${userid}`, method: 'DELETE', params: { cluster } });
  },

  /**
   * Adds users to a hunt
   * @param {string} id The id of the hunt
   * @param {string} users Comma separated list of users
   * @param {string} cluster The cluster name string of the cluster to add users to the hunt from
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  async addUsers (id, users, cluster) {
    return await fetchWrapper({ url: `api/hunt/${id}/users`, method: 'POST', data: { users }, params: { cluster } });
  },

  /**
   * Removes a hunt name and id from its matched sessions
   * @param {string} id The id of the hunt
   * @param {string} cluster The cluster name string of the cluster to cleanup the hunt on
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  async cleanup (id, users, cluster) {
    return await fetchWrapper({ url: `api/hunt/${id}/removefromsessions`, method: 'PUT', params: { cluster } });
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
