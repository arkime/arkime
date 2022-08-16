import store from '@/store';
import setReqHeaders from '../../../../../common/vueapp/setReqHeaders';
import { paramStr } from '@/utils/paramStr';

export default {
  /**
   * Fetches the list of link groups that a user can view.
   * @param {boolean} all - if true, request all link groups (admin only)
   * @param {boolean} doSet - whether to store these link groups as those to be applied during use
   * @returns {Promise} - The promise that either resovles the or rejects in error
   */
  getLinkGroups (all = false) {
    store.commit('SET_LINK_GROUPS_ERROR', '');

    const query = {};
    if (store.state.seeAllLinkGroups) { query.all = true; }

    return new Promise((resolve, reject) => {
      fetch(`api/linkGroup/${paramStr(query)}`).then((response) => {
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
    for (const link of linkGroup.links) {
      delete link.expanded; // clear link expanded prop, it's just for the UI
    }

    return new Promise((resolve, reject) => {
      fetch('api/linkGroup', {
        method: 'PUT',
        headers: setReqHeaders({ 'Content-Type': 'application/json' }),
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
   * @returns {Promise} - The promise that either resovles the or rejects in error
   */
  deleteLinkGroup (id) {
    store.commit('SET_LINK_GROUPS_ERROR', '');

    return new Promise((resolve, reject) => {
      fetch(`api/linkGroup/${id}`, {
        method: 'DELETE',
        headers: setReqHeaders()
      }).then((response) => {
        return response.json();
      }).then((response) => {
        if (response.success) {
          store.commit('REMOVE_LINK_GROUP', id);
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

    for (const link of linkGroup.links) {
      delete link.expanded; // clear link expanded prop, it's just for the UI
    }

    return new Promise((resolve, reject) => {
      fetch(`api/linkGroup/${linkGroup._id}`, {
        method: 'PUT',
        headers: setReqHeaders({ 'Content-Type': 'application/json' }),
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
