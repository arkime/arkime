(function() {

  'use strict';

  /**
   * @class SpiviewService
   * @classdesc Transacts spiview data with the server
   */
  class SpiviewService {

    /**
     * Initialize global variables for the SpiviewService
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
     * Gets spiview data from the server
     * @param {object} query      The query to be passed as url parameters
     * @returns {Promise} Promise A promise object that signals the completion
     *                            or rejection of the request.
     */
    get(query) {
      let deferred = this.$q.defer();

      let request = this.$http({
        url     : 'spiview.json',
        method  : 'GET',
        params  : query,
        timeout : deferred.promise
      });

      let promise = request
        .then((response) => {
          return(response.data);
        }, (error) => {
          return(this.$q.reject(error.data));
        }).catch(angular.noop); // handle abort

      promise.abort = () => {
        deferred.resolve({error:'Request canceled.'});
      };

      // cleanup
      promise.finally(() => {
        promise.abort = angular.noop;
        deferred = request = promise = null;
      });

      return(promise);
    }

  }

  SpiviewService.$inject = ['$q', '$http'];


  angular.module('moloch')
    .service('SpiviewService', SpiviewService);

})();
