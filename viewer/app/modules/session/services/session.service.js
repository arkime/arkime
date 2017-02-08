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

        let params = { flatten: 1 };

        if (query) {
          if (query.length)     { params.length = query.length; }
          if (query.start)      { params.start  = query.start; }
          if (query.facets)     { params.facets = query.facets; }
          if (query.expression) { params.expression = query.expression; }
          if (query.view)       { params.view = query.view; }
          if (query.bounding)   { params.bounding = query.bounding; }

          if (query.startTime) {
            params.startTime = (query.startTime) | 0;
          }
          if (query.stopTime) {
            params.stopTime = (query.stopTime) | 0;
          }
          if (query.date) {
            params.date = query.date;
          }

          let i, len, item;
          // server takes one param (order)
          if (query.sorts && query.sorts.length) {
            params.order = '';
            for (i = 0, len = query.sorts.length; i < len; ++i) {
              item = query.sorts[i];
              params.order += item[0] + ':' + item[1];
              if (i < len - 1) { params.order += ','; }
            }
          }

          // server takes one param (fields)
          if (query.fields && query.fields.length) {
            params.fields = '';
            for (i = 0, len = query.fields.length; i < len; ++i) {
              item = query.fields[i];
              params.fields += item;
              if (i < len - 1) { params.fields += ','; }
            }
          }
        }

        let options = {
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
    getTableState() {
      return this.$q((resolve, reject) => {

        let options = {
          url   : 'tableState/sessionsNew',
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
     * Saves info about the session table columns and sorting order
     * @returns {Promise} Promise A promise object that signals the completion
     *                            or rejection of the request.
     */
    saveTableState(tableState) {
      return this.$q((resolve, reject) => {

        let options = {
          url   : 'tableState/sessionsNew',
          method: 'POST',
          data  : tableState
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
     * @returns {Promise} Promise A promise object that signals the completion
     *                            or rejection of the request.
     */
    getDetail(id, node) {
      return this.$q((resolve, reject) => {

        let options = {
          url   : node + '/session/' + id + '/detail',
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
     * Gets session packets
     * @param {string} id         The unique id of the session
     * @param {string} node       The node that the session belongs to
     * @param {Object} params     The params to send with the request
     * @returns {Promise} Promise A promise object that signals the completion
     *                            or rejection of the request.
     */
    getPackets(id, node, params) {
      let deferred = this.$q.defer();

      let request = this.$http({
        url     : node + '/session/' + id + '/packets',
        method  : 'GET',
        params  : params,
        timeout : deferred.promise
      });

      let promise = request
         .then((response) => {
           return(response);
         }, (error) => {
           return(this.$q.reject(error));
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

    /**
     * Add tags to sessions
     * @param {object} params     The parameters to be passed to server
     * @returns {Promise} Promise A promise object that signals the completion
     *                            or rejection of the request.
     */
    addTags(params) {
      return this.$q((resolve, reject) => {

        let options = SessionService
           .getReqOptions('addTags', 'POST', params, this.$location.search());

        // add tag data
        options.data.tags = params.tags;

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
     * @param {object} params     The parameters to be passed to server
     * @returns {Promise} Promise A promise object that signals the completion
     *                            or rejection of the request.
     */
    removeTags(params) {
      return this.$q((resolve, reject) => {

        let options = SessionService
           .getReqOptions('removeTags', 'POST', params, this.$location.search());

        // add tag data
        options.data.tags = params.tags;

        this.$http(options)
          .then((response) => {
            resolve(response);
          }, (error) => {
            reject(error);
          });

      });
    }

    /**
     * Exports a pcap by setting window.location
     * @param {object} params The parameters to be passed to server
     */
    exportPCAP(params) {
      let baseUrl = `sessions.pcap/${params.filename}`;

      let options = SessionService
         .getReqOptions(baseUrl, '', params, this.$location.search());

      let url = options.url;

      if (options.data.ids) {  url += `&ids=${options.data.ids}`; }

      url += `&segments=${params.segments}`;

      this.$window.location = url;
    }

    /**
     * Exports a csv by setting window.location
     * @param {object} params The parameters to be passed to server
     */
    exportCSV(params) {
      let baseUrl = `sessions.csv/${params.filename}`;

      let options = SessionService
         .getReqOptions(baseUrl, '', params, this.$location.search());

      let url = options.url;

      if (options.data.ids) {  url += `&ids=${options.data.ids}`; }

      url += `&segments=${params.segments}`;

      this.$window.location = url;
    }

    /**
     * Scrubs pcap data in session
     * @param {object} params     The parameters to be passed to server
     * @returns {Promise} Promise A promise object that signals the completion
     *                            or rejection of the request.
     */
    scrubPCAP(params) {
      return this.$q((resolve, reject) => {

        let options = SessionService
           .getReqOptions('scrub', 'POST', params, this.$location.search());

        this.$http(options)
          .then((response) => {
            resolve(response);
          }, (error) => {
            reject(error);
          });

      });
    }

    /**
     * Deletes a session
     * @param {object} params     The parameters to be passed to server
     * @returns {Promise} Promise A promise object that signals the completion
     *                            or rejection of the request.
     */
    remove(params) {
      return this.$q((resolve, reject) => {

        let options = SessionService
           .getReqOptions('delete', 'POST', params, this.$location.search());

        this.$http(options)
          .then((response) => {
            resolve(response);
          }, (error) => {
            reject(error);
          });

      });
    }

    /**
     * Sends a session to a cluster
     * @param {object} params     The parameters to be passed to server
     * @returns {Promise} Promise A promise object that signals the completion
     *                            or rejection of the request.
     */
    send(params) {
      return this.$q((resolve, reject) => {

        let options = SessionService
           .getReqOptions('sendSessions', 'POST', params, this.$location.search());

        // add tag and cluster data
        options.data.tags     = params.tags;
        options.data.cluster  = params.cluster;

        this.$http(options)
          .then((response) => {
            resolve(response);
          }, (error) => {
            reject(error);
          });

      });
    }

    /**
     * Open a new page to view unique values for different fields
     * @param {string} dbField  The field to get unique values for
     * @param {number} counts   1 or 0 whether to include counts of the values
     */
    exportUniqueValues(dbField, counts) {
      let url = 'unique.txt';

      url = SessionService.urlWithParams(url, this.$location.search());

      url += `&field=${dbField}&counts=${counts}`;

      this.$window.open(url, '_blank');
    }


    /* internal functions -------------------------------------------------- */
    /**
     * Adds date and expression parameters to a given base url without parameters
     * @param {string} baseUrl The url (without params) to append the params to
     * @param {Object} params  The params object where date params may exist
     */
    static urlWithParams(baseUrl, params) {
      if (params.date) {
        baseUrl += '?date=' + params.date;
      } else if (params.startTime && params.stopTime) {
        baseUrl += '?startTime='  + params.startTime;
        baseUrl += '&stopTime='   + params.stopTime;
      }

      baseUrl += '&expression=' + encodeURIComponent(params.expression);

      return baseUrl;
    }

    /**
     * Get Request Options
     * @param {string} baseUrl        The base url to append params to
     * @param {string} method         The HTTP method (POST, GET, PUT, DELETE, etc)
     * @param {object} params         The parameters to be applied to url or data
     * @param {object} existingParams The parameters existing in the url bar
     * @returns { url: {string}, method: {string}, data: {object} }
     */
    static getReqOptions(baseUrl, method, params, existingParams) {
      let data  = { segments: params.segments };
      let url   = SessionService.urlWithParams(baseUrl, existingParams);

      if (!params.applyTo || params.applyTo === 'open') {
        // specific sessions
        data.ids = [];
        for (let i = 0, len = params.sessions.length; i < len; ++i) {
          data.ids.push(params.sessions[i].id);
        }
        data.ids = data.ids.join(',');
      } else if (params.applyTo === 'visible') {
        // sessions on the open page
        url += '&start='  + params.start;
        url += '&length=' + params.numVisible;
      } else if (params.applyTo === 'matching') {
        // all sessions in query results
        url += '&start=0&length=' + params.numMatching;
      }

      return { url:url, method:method, data:data };
    }

  }


  SessionService.$inject = ['$q', '$http', '$window', '$location'];


  angular.module('moloch')
    .service('SessionService', SessionService);

})();
