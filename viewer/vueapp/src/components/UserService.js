import Vue from 'vue';

let _userCache;
let queryInProgress;

export default {

  getCurrent () {
    if (queryInProgress) { return queryInProgress; }

    queryInProgress = new Promise((resolve, reject) => {
      if (_userCache) { resolve(_userCache); }

      Vue.axios.get('user/current')
        .then((response) => {
          queryInProgress = undefined;
          _userCache = response.data;
          resolve(response.data);
        }, (error) => {
          queryInProgress = undefined;
          reject(error);
        });
    });

    return queryInProgress;
  },

  hasPermission (priv) {
    return new Promise((resolve, reject) => {
      this.getCurrent()
        .then((user) => {
          let privs = priv.split(',');
          for (let priv of privs) {
            if (!user[priv]) {
              return resolve(false);
            }
          }
          resolve(true);
        }, (error) => {
          reject(error);
        });
    });
  }

};
