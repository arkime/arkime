import store from '@/store';
import setReqHeaders from '../../../../../common/vueapp/setReqHeaders';
import { paramStr } from '@/utils/paramStr';

export default {
  /**
   * Fetches the list of overviews that a user can view.
   * @returns {Promise} - The promise that either resolves the request or rejects in error
   */
  getOverviews () {
    store.commit('SET_OVERVIEWS_ERROR', '');

    const query = {};
    if (store.state.seeAllOverviews) { query.all = true; }

    return new Promise((resolve, reject) => {
      fetch(`api/overview/${paramStr(query)}`).then((response) => {
        if (!response.ok) { // test for bad response code
          throw new Error(response.statusText);
        }
        return response.json();
      }).then((response) => {
        store.commit('SET_OVERVIEWS', response.overviews);
        return resolve(response.overviews);
      }).catch((err) => { // this catches an issue within the ^ .then
        store.commit('SET_OVERVIEWS_ERROR', err);
        return reject(err);
      });
    });
  },

  /**
   * Creates an overview.
   * @param {Object} overview - The overview data
   * @param {Function} onReFetchCb - Optional callback after successful creation and getOverviews
   * @returns {Promise} - The promise that either resolves the request or rejects in error
   */
  createOverview (overview, onReFetchCb) {
    return new Promise((resolve, reject) => {
      fetch('api/overview', {
        method: 'PUT',
        headers: setReqHeaders({ 'Content-Type': 'application/json' }),
        body: JSON.stringify(overview)
      }).then((response) => {
        return response.json();
      }).then((response) => {
        if (response.success) {
          this.getOverviews().then(() => onReFetchCb?.());
          return resolve(response);
        } else {
          return reject(response.text);
        }
      });
    });
  },

  /**
   * Deletes an overview.
   * @param {String} id - The id of the overview to delete
   * @returns {Promise} - The promise that either resolves the request or rejects in error
   */
  deleteOverview (id) {
    store.commit('SET_OVERVIEWS_ERROR', '');

    return new Promise((resolve, reject) => {
      fetch(`api/overview/${id}`, {
        method: 'DELETE',
        headers: setReqHeaders()
      }).then((response) => {
        return response.json();
      }).then((response) => {
        if (response.success) {
          store.commit('REMOVE_OVERVIEW', id);
          return resolve(response);
        } else {
          store.commit('SET_OVERVIEWS_ERROR', response.text);
          return reject(response.text);
        }
      });
    });
  },

  /**
   * Updates an overview.
   * @param {Object} overview - The new overview data
   * @returns {Promise} - The promise that either resolves the request or rejects in error
   */
  updateOverview (overview) {
    store.commit('SET_OVERVIEWS_ERROR', '');

    return new Promise((resolve, reject) => {
      fetch(`api/overview/${overview._id}`, {
        method: 'PUT',
        headers: setReqHeaders({ 'Content-Type': 'application/json' }),
        body: JSON.stringify(overview)
      }).then((response) => {
        return response.json();
      }).then((response) => {
        if (response.success) {
          store.commit('UPDATE_OVERVIEW', overview);
          return resolve(response);
        } else {
          store.commit('SET_OVERVIEWS_ERROR', response.text);
          return reject(response.text);
        }
      });
    });
  }
};
