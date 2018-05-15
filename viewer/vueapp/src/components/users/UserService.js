import Vue from 'vue';

let _userCache;
let queryInProgress;

const defaultSettings = { timezone: 'local' };

export default {

  /**
   * Gets current user from the server and caches it
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  getCurrent: function () {
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
  hasPermission: function (priv) {
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
   * Gets a user's settings
   * @param {string} userId     The unique identifier for a user
   *                            (only required if not the current user)
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  getSettings: function (userId) {
    return new Promise((resolve, reject) => {
      let url = 'user/settings';
      if (userId) { url += `?userId=${userId}`; }

      Vue.axios.get(url)
        .then((response) => {
          let settings = response.data;
          // if the settings are empty, set smart default
          if (Object.keys(settings).length === 0) {
            settings = defaultSettings;
          }
          resolve(settings);
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
  getViews: function (userId) {
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
  createView: function (params, userId) {
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
  deleteView: function (view, userId) {
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

  /**
   * Gets a user's custom column configurations
   * @param {string} userId     The unique identifier for a user
   *                            (only required if not the current user)
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  getColumnConfigs: function (userId) {
    return new Promise((resolve, reject) => {
      let url = 'user/columns';
      if (userId) { url += `?userId=${userId}`; }

      Vue.axios.get(url)
        .then((response) => {
          resolve(response.data);
        }, (error) => {
          reject(error.data);
        });
    });
  },

  /**
   * Creates a specified custom column configuration for a user
   * @param {Object} data       The data to pass to the server
   *                            { name: 'namey', columns: ['field1', 'field2', ... , 'fieldN']}
   * @param {string} userId     The unique identifier for a user
   *                            (only required if not the current user)
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  createColumnConfig: function (data, userId) {
    return new Promise((resolve, reject) => {
      let options = {
        url: 'user/columns/create',
        method: 'POST',
        data: data
      };

      if (userId) { options.url += `?userId=${userId}`; }

      Vue.axios(options)
        .then((response) => {
          resolve(response.data);
        }, (error) => {
          reject(error.data);
        });
    });
  },

  /**
   * Deletes a user's specified custom column configuration
   * @param {string} name       The name of the column configuration to be removed
   * @param {string} userId     The unique identifier for a user
   *                            (only required if not the current user)
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  deleteColumnConfig: function (name, userId) {
    return new Promise((resolve, reject) => {
      let options = {
        url: 'user/columns/delete',
        method: 'POST',
        data: { name: name }
      };

      if (userId) { options.url += `?userId=${userId}`; }

      Vue.axios(options)
        .then((response) => {
          resolve(response.data);
        }, (error) => {
          reject(error.data);
        });
    });
  },

  /**
   * Gets a user's custom spiview fields configurations
   * @param {string} userId     The unique identifier for a user
   *                            (only required if not the current user)
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  getSpiviewFields: function (userId) {
    return new Promise((resolve, reject) => {
      let options = { url: 'user/spiview/fields', method: 'GET' };

      if (userId) { options.url += `?userId=${userId}`; }

      Vue.axios(options)
        .then((response) => {
          resolve(response.data);
        }, (error) => {
          reject(error.data);
        });
    });
  },

  /**
   * Creates a specified custom spiview fields configuration for a user
   * @param {Object} data       The data to pass to the server
   *                            { name: 'namey', fields: ['field1', 'field2', ... , 'fieldN']}
   * @param {string} userId     The unique identifier for a user
   *                            (only required if not the current user)
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  createSpiviewFieldConfig: function (data, userId) {
    return new Promise((resolve, reject) => {
      let options = {
        url: 'user/spiview/fields/create',
        method: 'POST',
        data: data
      };

      if (userId) { options.url += `?userId=${userId}`; }

      Vue.axios(options)
        .then((response) => {
          resolve(response.data);
        }, (error) => {
          reject(error.data);
        });
    });
  },

  /**
   * Deletes a user's specified custom spiview fields configuration
   * @param {string} name       The name of the spiview fields configuration to be removed
   * @param {string} userId     The unique identifier for a user
   *                            (only required if not the current user)
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  deleteSpiviewFieldConfig: function (name, userId) {
    return new Promise((resolve, reject) => {
      let options = {
        url: 'user/spiview/fields/delete',
        method: 'POST',
        data: { name: name }
      };

      if (userId) { options.url += `?userId=${userId}`; }

      Vue.axios(options)
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
  parseViews: function (views) {
    for (var name in views) {
      if (views.hasOwnProperty(name)) {
        views[name].name = name;
      }
    }
    return views;
  }

};
