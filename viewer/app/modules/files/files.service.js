(function() {

  'use strict';

  /**
   * @class FilesService
   * @classdesc Retrieves files from the server and caches them
   */
  class FilesService {

    /**
     * Initialize global variables for this FilesService
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
     * Gets files from the server
     * @returns {Promise} Promise A promise object that signals the completion
     *                            or rejection of the request.
     */
    get(options) {
      console.log("ALW GET", options);
      return this.$q((resolve, reject) => {

        let config = { method:'GET', 
                          url:'filelist', 
                        cache:false,
                       params:options};

        this.$http(config)
          .then((response) => {
            resolve(response.data);
          }, (error) => {
            reject(error);
          });

      });
    }
  }

  FilesService.$inject = ['$q', '$http'];


  angular.module('directives.search')
    .service('FilesService', FilesService);

})();
