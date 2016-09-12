(function() {

  'use strict';

  var _fields_cache, _country_codes_cache;
  var _recent_expressions = [];

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
     * @returns {Promise} Promise A promise object that signals the completion
     *                            or rejection of the request.
     */
    get() {
      return this.$q((resolve, reject) => {

        if (_fields_cache && Object.keys(_fields_cache).length > 0) {
          resolve(_fields_cache);
          return;
        }

        this.$http.get('fields')
          .then((response) => {
            _fields_cache = response.data;
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
    getValue(params) {
      return this.$q((resolve, reject) => {

        var config = {
          method: 'GET',
          url   : 'unique.txt',
          params: params
        };

        this.$http(config)
          .then((response) => {
            resolve(response.data);
          }, (error) => {
            reject(error);
          });

      });
    }

    /**
     * Gets hasheader field values from the server
     * @param {Object} params     The parameters to send with the query
     * @returns {Promise} Promise A promise object that signals the completion
     *                            or rejection of the request.
     */
    getHasheader(params) {
      return this.$q((resolve, reject) => {

        var config = {
          method: 'GET',
          url   : 'uniqueValue.json',
          params: params
        };

        this.$http(config)
          .then((response) => {
            resolve(response.data);
          }, (error) => {
            reject(error);
          });

      });
    }

    /**
     * Caches a country code list for later use
     * @param {Object} codes The jQuery vectorMap country code object
     */
    saveCountryCodes(codes) {
      var result = [];

      for (var key in codes) {
        if (codes.hasOwnProperty(key)) {
          var code = codes[key];
          result.push({ exp: key, friendlyName: code.config.name });
        }
      }

      _country_codes_cache = result;
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
