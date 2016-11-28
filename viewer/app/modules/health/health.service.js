(function() {

  'use strict';


  var timeToLive = 5000; // 5 seconds
  var lastCalledTime;

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
    constructor($q, $http, $cacheFactory) {
      this.$q     = $q;
      this.$http  = $http;
      this.$cacheFactory = $cacheFactory;
    }

    /* service methods ----------------------------------------------------- */
    /**
     * Gets a list of sessions from the server
     * @returns {Promise} Promise A promise object that signals the completion
     *                            or rejection of the request.
     */
    esHealth() {
      return this.$q((resolve, reject) => {

        var currentTime = new Date().getTime();
        var makeCall    = true;

        if (!lastCalledTime || (currentTime - lastCalledTime < timeToLive)) {
          makeCall = false; // don't make the call, use the cached value
        }

        if (makeCall) { // make a new call by removing the cached value
          this.$cacheFactory.get('$http').remove('eshealth.json');
        }

        lastCalledTime = currentTime;

        this.$http({ method:'GET', url:'eshealth.json', cache:true })
          .then((response) => {
            resolve(response.data);
          }, (error) => {
            reject(error);
          });

      });
    }

  }

  HealthService.$inject = ['$q', '$http', '$cacheFactory'];


  angular.module('moloch')
    .service('HealthService', HealthService);

})();
