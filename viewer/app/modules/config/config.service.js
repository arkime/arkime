(function() {

  'use strict';

  var _page;

  /**
   * @class ConfigService
   * @classdesc Retrieves app configurations from the server and caches them
   */
  class ConfigService {

    /**
     * Initialize global variables for the ConfigService
     * @param $rootScope  Parent of all Angular scopes
     * @param $q          Service to run functions asynchronously
     * @param $http       Angular service that facilitates communication
     *                    with the remote HTTP servers
     *
     * @ngInject
     */
    constructor($rootScope, $q, $http) {
      this.$rootScope     = $rootScope;
      this.$q             = $q;
      this.$http          = $http;
    }

    /* service methods ----------------------------------------------------- */
    /**
     * Gets the title config
     * @returns {Promise} Promise A promise object that signals the completion
     *                            or rejection of the request.
     */
    getTitleConfig() {
      return this.$q((resolve, reject) => {

        this.$http({ url:'titleconfig', method:'GET', cache:true })
          .then((response) => {
            resolve(response.data);
          }, (error) => {
            reject(error);
          });

      });
    }

    /**
     * Sets the browser page title (using $rootScope.title bound in app.jade)
     * @param {string} page       The title of the current app page
     * @param {string} expression The search expression
     * @param {string} view       The view being applied
     * @returns {Promise} Promise A promise object that signals the completion
     *                            or rejection of the request.
     */
    setTitle(page, expression, view) {
      return this.$q((resolve, reject) => {
        if (!page)  { page = _page; }
        else        { _page = page; } // cache page

        this.getTitleConfig()
           .then((title) => {
             if (!expression) { expression = ''; }
             else { expression = ' - ' + expression; }

             if (!view) { view = ''; }
             else { view = ' - ' + view; }

             this.$rootScope.title = title.replace(/_page_/g, page)
                .replace(/( *_-expression|_expression)_/g, expression)
                .replace(/( *_-view|_view)_/g, view);

             resolve(this.$rootScope.title);
           })
           .catch((error) => {
             this.$rootScope.title = 'Moloch - ' + page;
             resolve(this.$rootScope.title);
           });
      });
    }

    /**
     * Gets the available clickable fields
     * @returns {Promise} Promise A promise object that signals the completion
     *                            or rejection of the request.
     */
    getMolochClickables() {
      return this.$q((resolve, reject) => {

        this.$http({ url:'molochRightClick', method:'GET', cache:true })
          .then((response) => {

            for (var key in response.data) {
              var item = response.data[key];
              if (item.func !== undefined) {
                /*jslint evil: true */
                item.func = new Function("key", "value", item.func);
              }
            }
            resolve(response.data);
          }, (error) => {
            reject(error);
          });

      });
    }

    /**
     * Gets the available moloch clusters
     * @returns {Promise} Promise A promise object that signals the completion
     *                            or rejection of the request.
     */
    getMolochClusters() {
      return this.$q((resolve, reject) => {

        this.$http({ url:'molochclusters', method:'GET', cache:true })
          .then((response) => {
            resolve(response.data);
          }, (error) => {
            reject(error);
          });

      });
    }

  }

  ConfigService.$inject = ['$rootScope', '$q', '$http'];


  angular.module('moloch.config', [])
    .service('ConfigService', ConfigService);

})();
