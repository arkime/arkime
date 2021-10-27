import Vue from 'vue';

export default {
  /**
   * Gets stuff
   */
  search (searchTerm) {
    return new Promise((resolve, reject) => {
      if (!searchTerm) { return resolve({}); }
      Vue.axios.get(`api/integration/search/${searchTerm.trim()}`).then((response) => {
        return resolve(response.data);
      }).catch((error) => {
        return reject(error);
      });
    });
  }
};
