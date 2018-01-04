(function() {

  'use strict';

  let reqPromise; // promise returned from $interval for recurring requests

  /**
   * @class StatsShardsController
   * @classdesc Interacts with moloch es stats section
   * @example
   * '<moloch-shards-stats update-interval="$ctrl.dataInterval"></moloch-shards-stats>'
   */
  class StatsShardsController {

    /**
     * Initialize global variables for this controller
     * @param $scope        Angular application model object
     * @param $interval     Angular's wrapper for window.setInterval
     * @param StatsService  Transacts stats with the server
     *
     * @ngInject
     */
    constructor($scope, $interval, StatsService, UserService) {
      this.$scope         = $scope;
      this.$interval      = $interval;
      this.StatsService   = StatsService;
      this.UserService    = UserService;
    }

    /* Callback when component is mounted and ready */
    $onInit() {
      this.loading      = true;
      this.UserService.getSettings()
        .then((response) => { this.settings = response; })
        .catch((error)   => { this.settings = { timezone:'local' }; });

      this.columns = [
        { name: 'Index', sort: 'action', doStats: false }
      ];

      this.loadData();
      if (this.updateInterval !== '0') {
        reqPromise = this.$interval(this.loadData.bind(this), parseInt(this.updateInterval));
      }
    }

    /**
     * fired whenever one-way bindings are updated
     * @param {obj} changesObj Hash whose keys are the names of the bound
     *                         properties that have changed, and the values
     *                         are an object of the form
     */
    $onChanges(changesObj) {
      if (changesObj.updateInterval && reqPromise) {
        this.$interval.cancel(reqPromise);

        if (this.updateInterval === '0') { return; }

        reqPromise = this.$interval(this.loadData.bind(this), parseInt(this.updateInterval));
      }
    }

    /* fired when controller's containing scope is destroyed */
    $onDestroy() {
      if (reqPromise) {
        this.$interval.cancel(reqPromise);
        reqPromise = null;
      }
    }

    /**
     * Loads data with sort parameter
     * Fired when a column is clicked
     * @param {string} name The name of the column
     */
    columnClick(name) {
      this.sortField = name;
      this.sortReverse = !this.sortReverse;
      this.loadData();
    }

    /* loads the shards stats data and computes the total and average values */
    loadData() {
      this.loading = true;

      this.StatsService.getElasticsearchShards()
        .then((response) => {
          this.loading  = false;
          this.stats    = response;
          this.columns.splice(1);
          for (var node of response.nodes) {
            this.columns.push({name: node, sort: undefined, doStats: false})
          }
          this.$scope.$broadcast('$$rebind::refreshShards');
        })
        .catch((error) => {
          this.error    = error;
          this.loading  = false;
        });
    }
  }


  StatsShardsController.$inject = ['$scope','$interval','StatsService','UserService'];

  /**
   * Moloch StatsShards Directive
   * Displays Shards stats
   */
  angular.module('moloch')
     .component('molochEsShards', {
       template  : require('./stats.shards.html'),
       controller: StatsShardsController,
       bindings  : { updateInterval: '<' }
     });

})();
