import Vue from 'vue';

export default {
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
