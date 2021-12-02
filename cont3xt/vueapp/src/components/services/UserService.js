import store from '@/store';

export default {
  /**
   * Fetches the list of user roles.
   * @returns {Promise} - The promise that either resovles the or rejects in error
   */
  getUser () {
    return new Promise((resolve, reject) => {
      fetch('api/user').then((response) => {
        if (!response.ok) { // test for bad response code
          throw new Error(response.statusText);
        }
        return response.json();
      }).then((response) => {
        store.commit('SET_USER', response);
        return resolve(response);
      }).catch((err) => { // this catches an issue within the ^ .then
        return reject(err);
      });
    });
  },

  /**
   * Fetches the list of user roles.
   * @returns {Promise} - The promise that either resovles the or rejects in error
   */
  getRoles () {
    return new Promise((resolve, reject) => {
      fetch('api/roles').then((response) => {
        if (!response.ok) { // test for bad response code
          throw new Error(response.statusText);
        }
        return response.json();
      }).then((response) => {
        store.commit('SET_ROLES', response.roles);
        return resolve(response);
      }).catch((err) => { // this catches an issue within the ^ .then
        return reject(err);
      });
    });
  }
};
