import Vue from 'vue';

export default {
  /**
   * Gets a list of notifiers
   * @returns {Promise} Promise - A promise object that signals the completion
   *                              or rejection of the request.
   */
  getNotifiers: function () {
    return new Promise((resolve, reject) => {
      Vue.axios.get('api/notifierTypes').then((response) => {
        resolve(response);
      }).catch((err) => {
        reject(err);
      });
    });
  }
};
