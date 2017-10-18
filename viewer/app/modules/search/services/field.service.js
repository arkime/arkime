(function() {

  'use strict';

  const _country_codes_cache = require('./countries.json');

  let _recent_expressions = [];

  /**
   * @class FieldService
   * @classdesc Retrieves fields from the server and caches them
   */
  class FieldService {

    /**
     * Initialize global variables for this FieldService
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
     * Gets a field map from the server
     * @param {bool} array        Whether to request an array or map
     * @returns {Promise} Promise A promise object that signals the completion
     *                            or rejection of the request.
     */
    get(array) {
      return this.$q((resolve, reject) => {

        let config = { method:'GET', url:'fields', cache:true };

        if (array) { config.url += '?array=true'; }

        this.$http(config)
          .then((response) => {
            resolve(response.data);
          }, (error) => {
            reject(error);
          });

      });
    }

    /**
     * Gets field values from the server
     * @param {Object} params     The parameters to send with the query
     * @returns {Promise} Promise A promise object that signals the completion
     *                            or rejection of the request.
     */
    getValues(params) {
      let deferred = this.$q.defer();

      let request = this.$http({
        url     : 'unique.txt',
        method  : 'GET',
        params  : params,
        timeout : deferred.promise
      });

      let promise = request
        .then((response) => {
          return(response.data);
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
     * Gets hasheader field values from the server
     * @param {Object} params     The parameters to send with the query
     * @returns {Promise} Promise A promise object that signals the completion
     *                            or rejection of the request.
     */
    getHasheaderValues(params) {
      let deferred = this.$q.defer();

      let request = this.$http({
        url     : 'uniqueValue.json',
        method  : 'GET',
        params  : params,
        timeout : deferred.promise
      });

      let promise = request
        .then((response) => {
          return(response.data);
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
     * Gets the cached country code list
     * @returns {Promise} Promise A promise object that signals the completion
     *                            or rejection of the request.
     */
    getCountryCodes() {
      return this.$q((resolve, reject) => {

        if (_country_codes_cache) { resolve(_country_codes_cache); }
        else { reject('Error retrieving country codes'); }

      });
    }

    /**
     * Saves an expression to a list of recently used expressions
     * @param {Object} expression The expression to be saved
     * @returns {Promise} Promise A promise object that signals the completion
     *                            or rejection of the request.
     */
    save(expression) {
      return this.$q((resolve, reject) => {

        // only save the last 5 used expressions
        if (_recent_expressions.length >= 5) {
          _recent_expressions.shift();
        }

        _recent_expressions.push(expression);

        resolve();

      });
    }

  }

  FieldService.$inject = ['$q', '$http'];


  angular.module('directives.search')
    .service('FieldService', FieldService);

})();
