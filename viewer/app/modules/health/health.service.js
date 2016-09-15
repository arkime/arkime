(function() {

  'use strict';

  /**
   * @class HealthService
   * @classdesc Transacts es health with the server
   */
  class HealthService {

    /**
     * Initialize global variables for the HealthService
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
     * Gets a list of sessions from the server
     * @returns {Promise} Promise A promise object that signals the completion
     *                            or rejection of the request.
     */
    esHealth() {
      return this.$q((resolve, reject) => {

        this.$http.get('eshealth.json')
          .then((response) => {
            resolve(response.data);
          }, (error) => {
            reject(error);
          });

      });
    }

  }

  HealthService.$inject = ['$q', '$http'];


  angular.module('moloch')
    .service('HealthService', HealthService);

})();
