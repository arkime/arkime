/*
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
*/
import store from '@/store';
import setReqHeaders from '@common/setReqHeaders';
import { parseRoles } from '@common/vueFilters';

// App.vue's mount-time fetch, the /users and /roles route guards, and the
// navbar's admin-menu fallback all independently call getUser() around the
// same time on page load. Share one in-flight request instead of firing an
// identical fetch for each caller.
let pendingGetUser;

export default {
  /**
   * Fetches the current user.
   * @returns {Promise} - The promise that either resolves the request or rejects in error
   */
  getUser () {
    if (pendingGetUser) { return pendingGetUser; }

    pendingGetUser = new Promise((resolve, reject) => {
      fetch('api/user').then((response) => {
        if (!response.ok) { // test for bad response code
          throw new Error(response.statusText);
        }
        return response.json();
      }).then((response) => {
        store.commit('setUser', response);
        return resolve(response);
      }).catch((err) => { // this catches an issue within the ^ .then
        return reject(err);
      });
    }).finally(() => {
      pendingGetUser = undefined;
    });

    return pendingGetUser;
  },

  /**
  * Fetches the list of user roles.
  * @returns {Promise} - The promise that either resolves the request or rejects in error
  */
  getRoles () {
    return new Promise((resolve, reject) => {
      fetch('api/user/roles', {
        headers: setReqHeaders()
      }).then((response) => {
        if (!response.ok) { // test for bad response code
          throw new Error(response.statusText);
        }
        return response.json();
      }).then((response) => {
        const roles = parseRoles(response.roles);
        store.commit('setRoles', roles);
        return resolve(roles);
      }).catch((err) => { // this catches an issue within the ^ .then
        return reject(err);
      });
    });
  }
};
