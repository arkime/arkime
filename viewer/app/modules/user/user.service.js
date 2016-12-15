(function() {

  'use strict';

  /**
   * @class UserService
   * @classdesc Transacts users with the server
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

        this.$http({ url:'users/current', method:'GET', cache:true })
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
     * Gets current user's views
     * @returns {Promise} Promise A promise object that signals the completion
     *                            or rejection of the request.
     */
    getViews() {
      return this.$q((resolve, reject) => {

        let options = { url:'views', method:'GET' };

        this.$http(options)
           .then((response) => {
             resolve(response.data);
           }, (error) => {
             reject(error);
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

        let options = { url:'views/delete', method:'POST', data:{ view:view } };

        this.$http(options)
           .then((response) => {
             resolve(response.data);
           }, (error) => {
             reject(error);
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
        let options = { url:'views/create', method:'POST', data:params };

        this.$http(options)
           .then((response) => {
             resolve(response);
           }, (error) => {
             reject(error);
           });

      });
    }

  }

  UserService.$inject = ['$q', '$http'];


  angular.module('moloch')
    .service('UserService', UserService);

})();
