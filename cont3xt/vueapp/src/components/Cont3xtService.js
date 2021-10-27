import Vue from 'vue';

export default {
  /**
   * Gets stuff
   * TODO document
   */
  search (searchTerm) {
    return new Promise((resolve, reject) => {
      searchTerm = searchTerm.trim();

      if (!searchTerm) { return resolve({}); } // TODO resolve something else?

      Vue.axios.get(`api/integration/search/${searchTerm}`).then((response) => {
        return resolve(response.data);
      }).catch((error) => {
        return reject(error);
      });
    });
  }
};
