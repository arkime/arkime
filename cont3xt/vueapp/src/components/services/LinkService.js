import store from '@/store';

export default {
  /**
   * Fetches the list of link groups that a user can view.
   * @returns {Promise} - The promise that either resovles the or rejects in error
   */
  getLinkGroups () {
    store.commit('SET_LINK_GROUPS_ERROR', '');

    return new Promise((resolve, reject) => {
      fetch('api/linkGroup').then((response) => {
        if (!response.ok) { // test for bad response code
          throw new Error(response.statusText);
        }
        return response.json();
      }).then((response) => {
        store.commit('SET_LINK_GROUPS', response.linkGroups);
        return resolve(response.linkGroups);
      }).catch((err) => { // this catches an issue within the ^ .then
        store.commit('SET_LINK_GROUPS_ERROR', err);
        return reject(err);
      });
    });
  },

  /**
   * Creates a link group.
   * @param {Object} linkGroup - The link group data
   * @returns {Promise} - The promise that either resovles the or rejects in error
   */
  createLinkGroup (linkGroup) {
    return new Promise((resolve, reject) => {
      fetch('api/linkGroup', {
        method: 'PUT',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(linkGroup)
      }).then((response) => {
        return response.json();
      }).then((response) => {
        if (response.success) {
          this.getLinkGroups();
          return resolve(response);
        } else {
          return reject(response.text);
        }
      });
    });
  },

  /**
   * Deletes a link group.
   * @param {String} id - The id of the link group to delete
   * @param {Number} index - The index of the link group within the array to delete
   * @returns {Promise} - The promise that either resovles the or rejects in error
   */
  deleteLinkGroup (id, index) {
    store.commit('SET_LINK_GROUPS_ERROR', '');

    return new Promise((resolve, reject) => {
      fetch(`api/linkGroup/${id}`, {
        method: 'DELETE'
      }).then((response) => {
        return response.json();
      }).then((response) => {
        if (response.success) {
          store.commit('REMOVE_LINK_GROUP', index);
          return resolve(response);
        } else {
          store.commit('SET_LINK_GROUPS_ERROR', response.text);
          return reject(response.text);
        }
      });
    });
  },

  /**
   * Updates a link group.
   * @param {Object} linkGroup - The new link group data
   * @returns {Promise} - The promise that either resovles the or rejects in error
   */
  updateLinkGroup (linkGroup) {
    store.commit('SET_LINK_GROUPS_ERROR', '');

    return new Promise((resolve, reject) => {
      fetch(`api/linkGroup/${linkGroup._id}`, {
        method: 'PUT',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(linkGroup)
      }).then((response) => {
        return response.json();
      }).then((response) => {
        if (response.success) {
          store.commit('UPDATE_LINK_GROUP', linkGroup);
          return resolve(response);
        } else {
          store.commit('SET_LINK_GROUPS_ERROR', response.text);
          return reject(response.text);
        }
      });
    });
  }
};
