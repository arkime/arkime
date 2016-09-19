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
     * @param $q    Service to run functions asynchronously
     * @param $http Angular service that facilitates communication
     *              with the remote HTTP servers
     *
     * @ngInject
     */
    constructor($rootScope, $q, $http, HealthService) {
      this.$rootScope     = $rootScope;
      this.$q             = $q;
      this.$http          = $http;
      this.HealthService  = HealthService;
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
     * Sets the browser page title
     */
    setTitle(page, expression, view) {

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
        })
        .catch((error) => {
          this.$rootScope.title = 'Moloch - ' + page;
        });
    }


  }

  ConfigService.$inject = ['$rootScope', '$q', '$http', 'HealthService'];


  angular.module('moloch.config', [])
    .service('ConfigService', ConfigService);

})();
