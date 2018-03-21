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
  },

  getViews (userId) {
    return new Promise((resolve, reject) => {
      let url = 'user/views';
      if (userId) { url += `?userId=${userId}`; }

      Vue.axios.get(url)
        .then((response) => {
          response.data = this.parseViews(response.data);
          resolve(response.data);
        }, (error) => {
          reject(error.data);
        });
    });
  },

  deleteView (view, userId) {
    return new Promise((resolve, reject) => {
      let url = 'user/views/delete';
      if (userId) { url += `?userId=${userId}`; }

      Vue.axios.post(url, { view: view })
        .then((response) => {
          resolve(response.data);
        }, (error) => {
          reject(error.data);
        });
    });
  },

  parseViews (views) {
    for (var name in views) {
      if (views.hasOwnProperty(name)) {
        views[name].name = name;
      }
    }
    return views;
  }

};
