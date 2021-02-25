import Vue from 'vue';

export default {
  getSources: function () {
    return new Promise((resolve, reject) => {
      Vue.axios.get('sources')
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
      const url = source ? 'types/' + source : 'types';
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
  getResourceStats: function () {
    return new Promise((resolve, reject) => {
      Vue.axios.get('stats')
        .then((response) => {
          resolve(response.data);
        })
        .catch((error) => {
          console.log('service error', error);
          reject(error);
        });
    });
  },
  getConfigDefs: function () {
    return new Promise((resolve, reject) => {
      Vue.axios.get('config/defs')
        .then((response) => {
          resolve(response.data);
        })
        .catch((error) => {
          console.log('service error', error);
          reject(error);
        });
    });
  },
  getCurrConfig: function () {
    return new Promise((resolve, reject) => {
      Vue.axios.get('config/get')
        .then((response) => {
          resolve(response.data);
        })
        .catch((error) => {
          console.log('service error', error);
          reject(error);
        });
    });
  },
  getSourceFile: function (sourceName) {
    return new Promise((resolve, reject) => {
      Vue.axios.get('source/' + sourceName + '/get')
        .then((response) => {
          resolve(response.data);
        })
        .catch((error) => {
          console.log('service error', error);
          reject(error);
        });
    });
  },
  saveSourceFile: function (sourceName, data, configCode) {
    // TODO: new file saving
    return new Promise((resolve, reject) => {
      Vue.axios.put('source/' + sourceName + '/put', { raw: data, configCode: configCode })
        .then((response) => {
          resolve(response.data);
        })
        .catch((error) => {
          reject(error);
        });
    });
  },
  getSourceDisplay: function (sourceName) {
    return new Promise((resolve, reject) => {
      Vue.axios.get('dump/' + sourceName)
        .then((response) => {
          resolve(response.data);
        })
        .catch((error) => {
          console.log('service error', error);
          reject(error);
        });
    });
  },
  saveCurrConfig: function (config, configCode) {
    return new Promise((resolve, reject) => {
      Vue.axios.put('config/save', { config: config, configCode: configCode })
        .then((response) => {
          resolve(response.data);
        })
        .catch((error) => {
          reject(error.response.data);
        });
    });
  },
  search: function (source, type, value) {
    return new Promise((resolve, reject) => {
      const url = ((source ? source.replace(':', '%3A') + '/' : '') + type + '/' + value);
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
