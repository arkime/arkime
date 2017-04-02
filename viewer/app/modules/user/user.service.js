(function() {

  'use strict';

  const defaultSettings = { timezone: 'local' };

  /**
   * @class UserService
   * @classdesc Transacts users and user data with the server
   */
  class UserService {

    /**
     * Initialize global variables for the UserService
     * @param $q    Service to run functions asynchronously
     * @param $http Angular service that facilitates communication
     *              with the remote HTTP servers
     *
     * @ngInject
     */
    constructor($q, $http) {
      this.$q     = $q;
      this.$http  = $http;
    }

    /* service methods ----------------------------------------------------- */
    /**
     * Gets current user from the server
     * @returns {Promise} Promise A promise object that signals the completion
     *                            or rejection of the request.
     */
    getCurrent() {
      return this.$q((resolve, reject) => {

        this.$http({ url:'user/current', method:'GET', cache:true })
          .then((response) => {
            resolve(response.data);
          }, (error) => {
            reject(error);
          });

      });
    }

    /**
     * Determines whether a user has permission to perform a specific task
     * @param {string} priv       The privilege in question. Values include:
     *                            'createEnabled', 'emailSearch', 'enabled',
     *                            'headerAuthEnabled', 'removeEnabled', 'webEnabled'
     * @returns {Promise} Promise A promise object that signals the completion
     *                            or rejection of the request.
     */
    hasPermission(priv) {
      return this.$q((resolve, reject) => {

        this.getCurrent()
          .then((user) => {
            resolve(user[priv]);
          }, (error) => {
            reject(error);
          });

      });
    }

    /**
     * Gets a user's settings
     * @param {string} userId     The unique identifier for a user
     *                            (only required if not the current user)
     * @returns {Promise} Promise A promise object that signals the completion
     *                            or rejection of the request.
     */
    getSettings(userId) {
      return this.$q((resolve, reject) => {

        let options = { url:'user/settings', method:'GET' };

        if (userId) { options.url += `?userId=${userId}`; }

        this.$http(options)
          .then((response) => {
            let settings = response.data;
            // if the settings are empty, set smart default
            if (Object.keys(settings).length === 0) {
              settings = defaultSettings;
            }
            resolve(settings);
          }, (error) => {
            resolve(defaultSettings);
          });

      });
    }

    /**
     * Updates a user's settings
     * @param {object} settings   The object containing user settings
     * @param {string} userId     The unique identifier for a user
     *                            (only required if not the current user)
     * @returns {Promise} Promise A promise object that signals the completion
     *                            or rejection of the request.
     */
    saveSettings(settings, userId) {
      return this.$q((resolve, reject) => {

        let options = { url:'user/settings/update', method:'POST', data: settings };

        if (userId) { options.url += `?userId=${userId}`; }

        this.$http(options)
          .then((response) => {
            resolve(response.data);
          }, (error) => {
            reject(error.data);
          });

      });
    }

    /**
     * Updates a user's information
     * @param {object} user   The object containing the user
     * @returns {Promise} Promise A promise object that signals the completion
     *                            or rejection of the request.
     */
    updateUser(user) {
      return this.$q((resolve, reject) => {

        let options = { url:'user/update', method:'POST', data: user };

        this.$http(options)
          .then((response) => {
            resolve(response.data);
          }, (error) => {
            reject(error.data);
          });

      });
    }

    /**
     * Create a user
     * @param {object} user   The object containing the user
     * @returns {Promise} Promise A promise object that signals the completion
     *                            or rejection of the request.
     */
    createUser(user) {
      return this.$q((resolve, reject) => {

        let options = { url:'user/create', method:'POST', data: user };

        this.$http(options)
          .then((response) => {
            resolve(response.data);
          }, (error) => {
            reject(error.data);
          });

      });
    }

    /**
     * Delete a user
     * @param {object} user   The user to delete
     * @returns {Promise} Promise A promise object that signals the completion
     *                            or rejection of the request.
     */
    deleteUser(user) {
      return this.$q((resolve, reject) => {

        let options = { url:'user/delete', method:'POST', data: user};

        this.$http(options)
          .then((response) => {
            resolve(response.data);
          }, (error) => {
            reject(error.data);
          });

      });
    }

    /**
     * Gets a user's views
     * @param {string} userId     The unique identifier for a user
     *                            (only required if not the current user)
     * @returns {Promise} Promise A promise object that signals the completion
     *                            or rejection of the request.
     */
    getViews(userId) {
      return this.$q((resolve, reject) => {

        let options = { url:'user/views', method:'GET' };

        if (userId) { options.url += `?userId=${userId}`; }

        this.$http(options)
          .then((response) => {
            response.data = UserService.parseViews(response.data);
            resolve(response.data);
          }, (error) => {
            reject(error.data);
          });

      });
    }

    /**
     * Creates a specified view for a user
     * @param {Object} params     The params to pass as data to the server
     *                            { viewName: 'specialview', expression: 'something == somethingelse'}
     * @param {string} userId     The unique identifier for a user
     *                            (only required if not the current user)
     * @returns {Promise} Promise A promise object that signals the completion
     *                            or rejection of the request.
     */
    createView(params, userId) {
      return this.$q((resolve, reject) => {

        let options = { url:'user/views/create', method:'POST', data:params };

        if (userId) { options.url += `?userId=${userId}`; }

        this.$http(options)
          .then((response) => {
            resolve(response.data);
          }, (error) => {
            reject(error.data);
          });

      });
    }

    /**
     * Deletes a user's specified view
     * @param {string} view       The name of the view to be removed
     * @param {string} userId     The unique identifier for a user
     *                            (only required if not the current user)
     * @returns {Promise} Promise A promise object that signals the completion
     *                            or rejection of the request.
     */
    deleteView(view, userId) {
      return this.$q((resolve, reject) => {

        let options = { url:'user/views/delete', method:'POST', data:{ view:view } };

        if (userId) { options.url += `?userId=${userId}`; }

        this.$http(options)
          .then((response) => {
            resolve(response.data);
          }, (error) => {
            reject(error.data);
          });

      });
    }

    /**
     * Updates a specified view for a user
     * @param {Object} data       The view data to pass to the server
     * @param {string} userId     The unique identifier for a user
     *                            (only required if not the current user)
     * @returns {Promise} Promise A promise object that signals the completion
     *                            or rejection of the request.
     */
    updateView(data, userId) {
      return this.$q((resolve, reject) => {

        let options = { url:'user/views/update', method:'POST', data:data };

        if (userId) { options.url += `?userId=${userId}`; }

        this.$http(options)
          .then((response) => {
            response.data.views = UserService.parseViews(response.data.views);
            resolve(response.data);
          }, (error) => {
            reject(error.data);
          });

      });
    }

    /**
     * Gets a user's cron queries
     * @param {string} userId     The unique identifier for a user
     *                            (only required if not the current user)
     * @returns {Promise} Promise A promise object that signals the completion
     *                            or rejection of the request.
     */
    getCronQueries(userId) {
      return this.$q((resolve, reject) => {

        let options = { url:'user/cron', method:'GET' };

        if (userId) { options.url += `?userId=${userId}`; }

        this.$http(options)
          .then((response) => {
            resolve(response.data);
          }, (error) => {
            reject(error.data);
          });

      });
    }

    /**
     * Creates a specified cron query for a user
     * @param {Object} data       The cron query data to pass to the server
     * @param {string} userId     The unique identifier for a user
     *                            (only required if not the current user)
     * @returns {Promise} Promise A promise object that signals the completion
     *                            or rejection of the request.
     */
    createCronQuery(data, userId) {
      return this.$q((resolve, reject) => {

        let options = { url:'user/cron/create', method:'POST', data:data };

        if (userId) { options.url += `?userId=${userId}`; }

        this.$http(options)
          .then((response) => {
            resolve(response.data);
          }, (error) => {
            reject(error.data);
          });

      });
    }

    /**
     * Deletes a user's specified cron query
     * @param {string} key        The key of the cron query to be removed
     * @param {string} userId     The unique identifier for a user
     *                            (only required if not the current user)
     * @returns {Promise} Promise A promise object that signals the completion
     *                            or rejection of the request.
     */
    deleteCronQuery(key, userId) {
      return this.$q((resolve, reject) => {

        let options = { url:'user/cron/delete', method:'POST', data:{ key:key } };

        if (userId) { options.url += `?userId=${userId}`; }

        this.$http(options)
          .then((response) => {
            resolve(response.data);
          }, (error) => {
            reject(error.data);
          });

      });
    }

    /**
     * Updates a specified cron query for a user
     * @param {Object} data       The cron query data to pass to the server
     * @param {string} userId     The unique identifier for a user
     *                            (only required if not the current user)
     * @returns {Promise} Promise A promise object that signals the completion
     *                            or rejection of the request.
     */
    updateCronQuery(data, userId) {
      return this.$q((resolve, reject) => {

        let options = { url:'user/cron/update', method:'POST', data:data };

        if (userId) { options.url += `?userId=${userId}`; }

        this.$http(options)
          .then((response) => {
            resolve(response.data);
          }, (error) => {
            reject(error.data);
          });

      });
    }

    /**
     * Gets a user's custom column configurations
     * @param {string} userId     The unique identifier for a user
     *                            (only required if not the current user)
     * @returns {Promise} Promise A promise object that signals the completion
     *                            or rejection of the request.
     */
    getColumnConfigs(userId) {
      return this.$q((resolve, reject) => {

        let options = { url:'user/columns', method:'GET' };

        if (userId) { options.url += `?userId=${userId}`; }

        this.$http(options)
          .then((response) => {
            resolve(response.data);
          }, (error) => {
            reject(error.data);
          });

      });
    }

    /**
     * Creates a specified custom column configuration for a user
     * @param {Object} data       The data to pass to the server
     *                            { name: 'namey', columns: ['field1', 'field2', ... , 'fieldN']}
     * @param {string} userId     The unique identifier for a user
     *                            (only required if not the current user)
     * @returns {Promise} Promise A promise object that signals the completion
     *                            or rejection of the request.
     */
    createColumnConfig(data, userId) {
      return this.$q((resolve, reject) => {

        let options = { url:'user/columns/create', method:'POST', data:data };

        if (userId) { options.url += `?userId=${userId}`; }

        this.$http(options)
          .then((response) => {
            resolve(response.data);
          }, (error) => {
            reject(error.data);
          });

      });
    }

    /**
     * Deletes a user's specified custom column configuration
     * @param {string} name       The name of the column configuration to be removed
     * @param {string} userId     The unique identifier for a user
     *                            (only required if not the current user)
     * @returns {Promise} Promise A promise object that signals the completion
     *                            or rejection of the request.
     */
    deleteColumnConfig(name, userId) {
      return this.$q((resolve, reject) => {

        let options = { url:'user/columns/delete', method:'POST', data:{ name:name } };

        if (userId) { options.url += `?userId=${userId}`; }

        this.$http(options)
          .then((response) => {
            resolve(response.data);
          }, (error) => {
            reject(error.data);
          });

      });
    }

    /**
     * Changes current user's password
     * @param {object} data       The data to send to the server
     *                            { userId, currentPassword, newPassword }
     * @param {string} userId     The unique identifier for a user
     *                            (only required if not the current user)
     * @returns {Promise} Promise A promise object that signals the completion
     *                            or rejection of the request.
     */
    changePassword(data, userId) {
      return this.$q((resolve, reject) => {

        let options = { url:'user/password/change', method:'POST' };

        if (userId) {
          options.url += `?userId=${userId}`;
          options.data = { newPassword:data.newPassword };
        } else { options.data = data; }

        this.$http(options)
          .then((response) => {
            resolve(response.data);
          }, (error) => {
            reject(error.data);
          });

      });
    }

    /**
     * Gets users from the server
     * @returns {Promise} Promise A promise object that signals the completion
     *                            or rejection of the request.
     */
    listUsers(options) {
      return this.$q((resolve, reject) => {

        let config = { method:'POST',
                          url:'user/list',
                        cache:false,
                       data:options};

        this.$http(config)
          .then((response) => {
            resolve(response.data);
          }, (error) => {
            reject(error);
          });

      });
    }

    /* internal methods ---------------------------------------------------- */
    /**
     * Adds the name as a property on a view (instead of just key)
     * @param {object} views    The object containing view objects
     * @returns {object} views  The object containing views, now with name
     *                          properties on each view object
     */
    static parseViews(views) {
      for (var name in views) {
        if (views.hasOwnProperty(name)) {
          views[name].name = name;
        }
      }
      return views;
    }

  }

  UserService.$inject = ['$q', '$http'];


  angular.module('moloch')
    .service('UserService', UserService);

})();
