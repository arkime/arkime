(function() {

  'use strict';

  /**
   * @class SessionService
   * @classdesc Transacts sessions with the server
   */
  class SessionService {

    /**
     * Initialize global variables for this SessionService
     * @param $q        Service to run functions asynchronously
     * @param $http     Angular service that facilitates communication
     *                  with the remote HTTP servers
     * @param $window   Angular reference to the browser's window object
     * @param $location Exposes browser address bar URL
     *                  (based on the window.location)
     *
     * @ngInject
     */
    constructor($q, $http, $window, $location) {
      this.$q         = $q;
      this.$http      = $http;
      this.$window    = $window;
      this.$location  = $location;
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
          if (query.date) {
            params.date = query.date;
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

    /**
     * Gets details about the session
     * @param {string} id         The unique id of the session
     * @param {string} node       The node that the session belongs to
     * @param {Object} params     The params to send with the request
     * @returns {Promise} Promise A promise object that signals the completion
     *                            or rejection of the request.
     */
    getDetail(id, node, params) {
      return this.$q((resolve, reject) => {

        var options = {
          url   : node + '/' + id + '/' + 'sessionDetail',
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

    /**
     * Add tags to sessions
     * @param {string} id         The unique id of the session to add tags to
     * @param {string} tags       The comma separated string of tags to add
     * @param {string} segments   'no', 'all', or 'time'
     * @returns {Promise} Promise A promise object that signals the completion
     *                            or rejection of the request.
     */
    addTags(id, tags, segments) {
      return this.$q((resolve, reject) => {
        var data = { ids:id, tags:tags };

        if (segments !== 'no') { data.segments = segments; }

        var url = SessionService.urlWithDateParams('addTags', this.$location.search());

        var options = { url:url, method:'POST', data:data };

        this.$http(options)
          .then((response) => {
            resolve(response);
          }, (error) => {
            reject(error);
          });

      });
    }

    /**
     * Remove tags from sessions
     * @param {string} id         The unique id of the session to remove tags from
     * @param {string} tags       The comma separated string of tags to remove
     * @param {string} segments   'no', 'all', or 'time'
     * @returns {Promise} Promise A promise object that signals the completion
     *                            or rejection of the request.
     */
    removeTags(id, tags, segments) {
      return this.$q((resolve, reject) => {
        var data = { ids:id, tags:tags };

        if (segments !== 'no') { data.segments = segments; }

        var url = SessionService.urlWithDateParams('removeTags', this.$location.search());

        var options = { url:url, method:'POST', data:data };

        this.$http(options)
          .then((response) => {
            resolve(response);
          }, (error) => {
            reject(error);
          });

      });
    }

    /**
     * Exports a pcap for the user by setting window.location
     * @param {string} id         The unique id of the session to remove tags from
     * @param {string} tags       The comma separated string of tags to remove
     * @param {string} segments   'no', 'all', or 'time'
     */
    exportPCAP(id, filename, segments) {
      var baseUrl = `sessions.pcap/${filename}`;

      var url = SessionService.urlWithDateParams(baseUrl, this.$location.search());

      url += `&ids=${id}`;

      if (segments !== 'no') { url += `&segments=${segments}`; }

      this.$window.location = url;
    }

    /* internal functions -------------------------------------------------- */
    /**
     * Adds date parameters to a given base url without parameters
     * @param {string} baseUrl The url (without params) to append the params to
     * @param {Object} params  The params object where date params may exist
     */
    static urlWithDateParams(baseUrl, params) {
      if (params.date) {
        baseUrl += '?date=' + params.date;
      } else if (params.startTime &&
        params.stopTime) {
        baseUrl += '?startTime='  + params.startTime;
        baseUrl += '?stopTime='   + params.stopTime;
      }

      return baseUrl;
    }

  }

  SessionService.$inject = ['$q', '$http', '$window', '$location'];


  angular.module('moloch')
    .service('SessionService', SessionService);

})();
