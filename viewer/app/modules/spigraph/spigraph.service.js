(function() {

  'use strict';

  /**
   * @class SpigraphService
   * @classdesc Transacts stats data with the server
   */
  class SpigraphService {

    /**
     * Initialize global variables for the SpigraphService
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
     * Gets spigraph from the server
     * @param {object} params     The query parameters for the request
     * @returns {Promise} Promise A promise object that signals the completion
     *                            or rejection of the request.
     */
    get(params) {
      return this.$q((resolve, reject) => {

        let config = { method:'GET',  url:'spigraph.json', params:params};

        this.$http(config)
          .then((response) => {
            resolve(response.data);
          }, (error) => {
            reject(error.data);
          });

      });
    }
  }

  SpigraphService.$inject = ['$q', '$http'];


  angular.module('moloch')
    .service('SpigraphService', SpigraphService);

})();
