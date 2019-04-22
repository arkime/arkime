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
      Vue.axios.put('api/settings', { settings: settings })
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
      Vue.axios.put('api/settings/restoreDefaults', { type: type })
        .then((response) => {
          resolve(response.data);
        })
        .catch((error) => {
          reject(error.response.data);
        });
    });
  },

  createNotifier: function (notifier) {
    return new Promise((resolve, reject) => {
      Vue.axios.post('api/notifiers', { notifier: notifier })
        .then((response) => {
          resolve(response.data);
        })
        .catch((error) => {
          reject(error.response.data);
        });
    });
  },

  removeNotifier: function (notifierName) {
    return new Promise((resolve, reject) => {
      Vue.axios.delete(`api/notifiers/${notifierName}`)
        .then((response) => {
          resolve(response.data);
        })
        .catch((error) => {
          reject(error.response.data);
        });
    });
  },

  updateNotifier: function (notifierKey, oldNotifierName, notifier) {
    return new Promise((resolve, reject) => {
      Vue.axios.put(`api/notifiers/${oldNotifierName}`, {
        key: notifierKey,
        notifier: notifier
      })
        .then((response) => {
          resolve(response.data);
        })
        .catch((error) => {
          reject(error.response.data);
        });
    });
  },

  testNotifier: function (notifierName) {
    return new Promise((resolve, reject) => {
      Vue.axios.post('api/testAlert', { notifier: notifierName })
        .then((response) => {
          resolve(response.data);
        })
        .catch((error) => {
          reject(error.response.data);
        });
    });
  },

  getNotifierTypes: function () {
    return new Promise((resolve, reject) => {
      Vue.axios.get('api/notifierTypes')
        .then((response) => {
          resolve(response.data);
        })
        .catch((error) => {
          reject(error.response.data);
        });
    });
  }
};
