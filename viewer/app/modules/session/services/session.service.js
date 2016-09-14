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

        if (query) {
          if (query.length)     { params.length = query.length; }
          if (query.start)      { params.start  = query.start; }
          if (query.facets)     { params.facets = query.facets; }
          if (query.expression) { params.expression = query.expression; }

          if (query.startTime) {
            params.startTime = (query.startTime / 1000) | 0;
          }
          if (query.stopTime) {
            params.stopTime = (query.stopTime / 1000) | 0;
          }

          // server takes one param (order)
          if (query.sorts && query.sorts.length) {
            params.order = '';
            var len = query.sorts.length;
            for (var i = 0; i < len; ++i) {
              var item = query.sorts[i];
              params.order += item.element + ':' + item.order;
              if (i < len - 1) { params.order += ','; }
            }
          }
        }

        var options = {
          url   : 'sessions.json',
          method: 'GET',
          params: params
        };

        this.$http(options)
          .then((response) => {
            if (response.data.bsqErr) { reject(response.data.bsqErr); }
            resolve(response);
          }, (error) => {
            reject(error);
          });

      });
    }

    /**
     * Gets info about the session table columns and sorting order
     * @returns {Promise} Promise A promise object that signals the completion
     *                            or rejection of the request.
     */
    getColumnInfo() {
      return this.$q((resolve, reject) => {

        var options = {
          url   : 'tableState/sessions',
          method: 'GET'
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
