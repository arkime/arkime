import Vue from 'vue';

export default {
  getSettings: function () {
    return new Promise((resolve, reject) => {
      Vue.axios.get('api/settings')
        .then((response) => {
          resolve(response.data);
        })
        .catch((error) => {
          reject(error.response.data);
        });
    });
  },

  saveSettings: function (settings) {
    return new Promise((resolve, reject) => {
      Vue.axios.put('api/settings', { settings })
        .then((response) => {
          resolve(response.data);
        })
        .catch((error) => {
          reject(error.response.data);
        });
    });
  },

  restoreDefaults: function (type) {
    return new Promise((resolve, reject) => {
      Vue.axios.put('api/settings/restoreDefaults', { type })
        .then((response) => {
          resolve(response.data);
        })
        .catch((error) => {
          reject(error.response.data);
        });
    });
  }
};
