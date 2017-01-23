(function() {

  'use strict';

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
     * Gets current user's settings
     * @returns {Promise} Promise A promise object that signals the completion
     *                            or rejection of the request.
     */
    getSettings() {
      return this.$q((resolve, reject) => {

        let options = { url:'user/settings', method:'GET' };

        this.$http(options)
          .then((response) => {
            resolve(response.data);
          }, (error) => {
            reject(error.data);
          });

      });
    }

    /**
     * Updates current user's settings
     * @param {object} settings   The object containing user settings
     * @returns {Promise} Promise A promise object that signals the completion
     *                            or rejection of the request.
     */
    saveSettings(settings) {
      return this.$q((resolve, reject) => {

        let options = { url:'user/settings/update', method:'POST', data: settings };

        this.$http(options)
          .then((response) => {
            resolve(response.data);
          }, (error) => {
            reject(error.data);
          });

      });
    }

    /**
     * Gets current user's views
     * @returns {Promise} Promise A promise object that signals the completion
     *                            or rejection of the request.
     */
    getViews() {
      return this.$q((resolve, reject) => {

        let options = { url:'user/views', method:'GET' };

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
     * Deletes current user's specified view
     * @param {string} view       The name of the view to be removed
     * @returns {Promise} Promise A promise object that signals the completion
     *                            or rejection of the request.
     */
    deleteView(view) {
      return this.$q((resolve, reject) => {

        let options = { url:'user/views/delete', method:'POST', data:{ view:view } };

        this.$http(options)
          .then((response) => {
            resolve(response.data);
          }, (error) => {
            reject(error.data);
          });

      });
    }

    /**
     * Creates a specified view for the current user
     * @param {Object} params     The params to pass as data to the server
     *                            { viewName: 'specialview', expression: 'something == somethingelse'}
     * @returns {Promise} Promise A promise object that signals the completion
     *                            or rejection of the request.
     */
    createView(params) {
      return this.$q((resolve, reject) => {
        let options = { url:'user/views/create', method:'POST', data:params };

        this.$http(options)
          .then((response) => {
            resolve(response.data);
          }, (error) => {
            reject(error.data);
          });

      });
    }

    /**
     * Updates a specified view for the current user
     * @param {Object} data       The view data to pass to the server
     * @returns {Promise} Promise A promise object that signals the completion
     *                            or rejection of the request.
     */
    updateView(data) {
      return this.$q((resolve, reject) => {
        let options = { url:'user/views/update', method:'POST', data:data };

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
     * Gets current user's cron queries
     * @returns {Promise} Promise A promise object that signals the completion
     *                            or rejection of the request.
     */
    getCronQueries() {
      return this.$q((resolve, reject) => {

        let options = { url:'user/cron', method:'GET' };

        this.$http(options)
          .then((response) => {
            resolve(response.data);
          }, (error) => {
            reject(error.data);
          });

      });
    }

    /**
     * Creates a specified cron query for the current user
     * @param {Object} data       The cron query data to pass to the server
     * @returns {Promise} Promise A promise object that signals the completion
     *                            or rejection of the request.
     */
    createCronQuery(data) {
      return this.$q((resolve, reject) => {
        let options = { url:'user/cron/create', method:'POST', data:data };

        this.$http(options)
          .then((response) => {
            resolve(response.data);
          }, (error) => {
            reject(error.data);
          });

      });
    }

    /**
     * Updates a specified cron query for the current user
     * @param {Object} data       The cron query data to pass to the server
     * @returns {Promise} Promise A promise object that signals the completion
     *                            or rejection of the request.
     */
    updateCronQuery(data) {
      return this.$q((resolve, reject) => {
        let options = { url:'user/cron/update', method:'POST', data:data };

        this.$http(options)
          .then((response) => {
            resolve(response.data);
          }, (error) => {
            reject(error.data);
          });

      });
    }

    /**
     * Deletes current user's specified cron query
     * @param {string} key        The key of the cron query to be removed
     * @returns {Promise} Promise A promise object that signals the completion
     *                            or rejection of the request.
     */
    deleteCronQuery(key) {
      return this.$q((resolve, reject) => {

        let options = { url:'user/cron/delete', method:'POST', data:{ key:key } };

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
     * @param {string} currentPassword  The user's current password
     * @param {string} newPassword      The user's new password
     * @returns {Promise} Promise       A promise object that signals the completion
     *                                  or rejection of the request.
     */
    changePassword(currentPassword, newPassword) {
      return this.$q((resolve, reject) => {

        let options = {
          url   :'user/password/change',
          method:'POST',
          data  :{ currentPassword:currentPassword, newPassword:newPassword }
        };

        this.$http(options)
          .then((response) => {
            resolve(response.data);
          }, (error) => {
            reject(error.data);
          });

      });
    }

    /* internal methods ---------------------------------------------------- */
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
