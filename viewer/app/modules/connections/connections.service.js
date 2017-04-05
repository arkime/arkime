(function() {

  'use strict';

  /**
   * @class ConnectionsService
   * @classdesc Transacts stats data with the server
   */
  class ConnectionsService {

    /**
     * Initialize global variables for the ConnectionsService
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
     * Gets connections from the server
     * @param {object} options    The query parameters for the request
     * @returns {Promise} Promise A promise object that signals the completion
     *                            or rejection of the request.
     */
    get(options) {
      return this.$q((resolve, reject) => {

        let config = { method:'GET', url:'connections.json', params:options };

        this.$http(config)
          .then((response) => {
            resolve(response.data);
          }, (error) => {
            reject(error.data);
          });

      });
    }
  }

  ConnectionsService.$inject = ['$q', '$http'];


  angular.module('moloch')
    .service('ConnectionsService', ConnectionsService);

})();
