import Vue from 'vue';

export default {
  getSources: function () {
    return new Promise((resolve, reject) => {
      Vue.axios.get('/sources')
        .then((response) => {
          resolve(response.data);
        })
        .catch((error) => {
          console.log('service error', error);
          reject(error);
        });
    });
  },
  getTypes: function (source) {
    return new Promise((resolve, reject) => {
      let url = source ? '/types/' + source : '/types';
      Vue.axios.get(url)
        .then((response) => {
          resolve(response.data);
        })
        .catch((error) => {
          console.log('service error', error);
          reject(error);
        });
    });
  },
  search: function (source, type, value) {
    return new Promise((resolve, reject) => {
      let url = '/' + ((source ? source + '/' : '') + type + '/' + value);
      Vue.axios.get(url)
        .then((response) => {
          resolve(response.data);
        })
        .catch((error) => {
          console.log('service error', error);
          reject(error);
        });
    });
  }
};
