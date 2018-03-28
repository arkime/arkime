import Vue from 'vue';

let _userCache;
let queryInProgress;

export default {

  /**
   * Gets current user from the server and caches it
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
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

  /**
   * Determines whether a user has permission to perform a specific task
   * @param {string} priv       The privilege in question. Values include:
   *                            'createEnabled', 'emailSearch', 'enabled',
   *                            'headerAuthEnabled', 'removeEnabled', 'webEnabled'
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
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

  /**
   * Gets a user's views
   * @param {string} userId     The unique identifier for a user
   *                            (only required if not the current user)
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
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

  /**
   * Creates a specified view for a user
   * @param {Object} params     The params to pass as data to the server
   *                            { viewName: 'specialview', expression: 'something == somethingelse'}
   * @param {string} userId     The unique identifier for a user
   *                            (only required if not the current user)
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  createView (params, userId) {
    return new Promise((resolve, reject) => {
      let url = 'user/views/create';
      if (userId) { url += `?userId=${userId}`; }

      Vue.axios.post(url, params)
        .then((response) => {
          resolve(response.data);
        }, (error) => {
          reject(error.data);
        });
    });
  },

  /**
   * Deletes a user's specified view
   * @param {string} view       The name of the view to be removed
   * @param {string} userId     The unique identifier for a user
   *                            (only required if not the current user)
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
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

  /* internal methods ---------------------------------------------------- */
  /**
   * Adds the name as a property on a view (instead of just key)
   * @param {object} views    The object containing view objects
   * @returns {object} views  The object containing views, now with name
   *                          properties on each view object
   */
  parseViews (views) {
    for (var name in views) {
      if (views.hasOwnProperty(name)) {
        views[name].name = name;
      }
    }
    return views;
  }

};
