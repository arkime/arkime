/*
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
*/
import Vue from 'vue';

import store from '@/store';
import setReqHeaders from '../../../../common/vueapp/setReqHeaders';

export default {
  /**
   * Fetches the list of user roles.
   * @returns {Promise} - The promise that either resolves the request or rejects in error
   */
  getUser () {
    return new Promise((resolve, reject) => {
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
    });
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
        const roles = Vue.filter('parseRoles')(response.roles);
        store.commit('setRoles', roles);
        return resolve(roles);
      }).catch((err) => { // this catches an issue within the ^ .then
        return reject(err);
      });
    });
  }
};
