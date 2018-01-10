(function() {

  'use strict';

  /**
   * @class StatsService
   * @classdesc Transacts stats data with the server
   */
  class StatsService {

    /**
     * Initialize global variables for the StatsService
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
     * Gets moloch stats from the server
     * @returns {Promise} Promise A promise object that signals the completion
     *                            or rejection of the request.
     */
    getMolochStats(options) {
      return this.$q((resolve, reject) => {

        let config = { method:'GET',
                          url:'stats.json',
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

    /**
     * Gets moloch stats from the server
     * @returns {Promise} Promise A promise object that signals the completion
     *                            or rejection of the request.
     */
    getElasticsearchStats(options) {
      return this.$q((resolve, reject) => {

        let config = { method:'GET',
                          url:'esstats.json',
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

    /**
     * Gets moloch stats from the server
     * @returns {Promise} Promise A promise object that signals the completion
     *                            or rejection of the request.
     */
    getElasticsearchIndices(options) {
      return this.$q((resolve, reject) => {

        let config = { method:'GET',
                          url:'esindices/list',
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

    /**
     * Gets moloch tasks from the server
     * @returns {Promise} Promise A promise object that signals the completion
     *                            or rejection of the request.
     */
    getElasticsearchTasks(options) {
      return this.$q((resolve, reject) => {

        let config = { method:'GET',
                          url:'estask/list',
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

    /**
     * Gets moloch shards from the server
     * @returns {Promise} Promise A promise object that signals the completion
     *                            or rejection of the request.
     */
    getElasticsearchShards(options) {
      return this.$q((resolve, reject) => {

        let config = { method:'GET',
                          url:'esshard/list',
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

    /**
     * Gets moloch dstats from the server
     */
    getDetailStats(options) {
      return this.$q((resolve, reject) => {

        let config = { method:'GET',
                          url:'dstats.json',
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

    /**
     * Cancel task
     */
    cancelTask(taskId) {
      return this.$q((resolve, reject) => {

        let options = { url:'estask/cancel', method:'POST', data: {taskId: taskId}};

        this.$http(options)
          .then((response) => {
            resolve(response.data);
          }, (error) => {
            reject(error.data);
          });

      });
    }

    /**
     * exclude node/ip
     */
    exclude(type, value) {
      return this.$q((resolve, reject) => {

        let options = { url:`esshard/exclude/${type}/${value}`, method:'POST', data: {}};

        this.$http(options)
          .then((response) => {
            resolve(response.data);
          }, (error) => {
            reject(error.data);
          });
      });
    }

    /**
     * include node/ip
     */
    include(type, value) {
      return this.$q((resolve, reject) => {

        let options = { url:`esshard/include/${type}/${value}`, method:'POST', data: {}};

        this.$http(options)
          .then((response) => {
            resolve(response.data);
          }, (error) => {
            reject(error.data);
          });
      });
    }

  } /* class */


  StatsService.$inject = ['$q', '$http'];


  angular.module('moloch')
    .service('StatsService', StatsService);

})();
