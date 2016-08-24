(function() {

  'use strict';

  /**
   * @class SessionService
   * @classdesc Transacts sessions with the server
   */
  class SessionService {

    /**
     * Initialize global variables for this SessionService
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
     * @param {object} query      Parameters to query the server
     * @returns {Promise} Promise A promise object that signals the completion
     *                            or rejection of the request.
     */
    get(query) {
      return this.$q((resolve, reject) => {

        var params = {};

        if (query.length) { params.length = query.length; }
        if (query.start)  { params.start  = query.start; }
        if (query.date)   { params.date   = query.date; }
        if (query.facets) { params.facets = query.facets; }
        if (query.draw)   { params.draw   = query.draw; }

        // server takes one param (order)
        if (query.sortElement && query.sortOrder) {
          params.order = query.sortElement + ':' + query.sortOrder;
        } else if (query.order) {
          params.order = query.order;
        }

        var options = {
          url   : 'sessions.json',
          method: 'GET',
          params: params
        };

        this.$http(options)
          .then((response) => {
            resolve(response);
          }, (error) => {
            reject(error);
          });

      });
    }

  }

  SessionService.$inject = ['$q', '$http'];


  angular.module('moloch')
    .service('SessionService', SessionService);

})();
