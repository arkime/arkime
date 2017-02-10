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
      return this.$q((resolve, reject) => {

        this.$http({ url:'spiview.json', method:'GET', params:query })
          .then((response) => {
            resolve(response.data);
          }, (error) => {
            reject(error);
          });

      });
    }

    // TODO ECR
    getCategories() {
      return this.$q((resolve, reject) => {

        this.$http({ url:'spiview/categories', method:'GET' })
          .then((response) => {
            resolve(response.data);
          }, (error) => {
            reject(error);
          });

      });
    }

  }

  SpiviewService.$inject = ['$q', '$http'];


  angular.module('moloch')
    .service('SpiviewService', SpiviewService);

})();
