(function() {

  'use strict';

  let reqPromise; // promise returned from $interval for recurring requests

  /**
   * @class StatsESController
   * @classdesc Interacts with moloch es stats section
   * @example
   * '<moloch-es-stats update-interval="$ctrl.dataInterval"></moloch-es-stats>'
   */
  class StatsESController {

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
      this.sortField    = 'nodeName';
      this.sortReverse  = false;

      this.UserService.getSettings()
        .then((response) => { this.settings = response; })
        .catch((error)   => { this.settings = { timezone:'local' }; });

      this.columns = [
        { name: 'Name', sort: 'name', doStats: false },
        { name: 'Documents', sort: 'docs', doStats: true },
        { name: 'Disk Storage', sort: 'storeSize', doStats: true },
        { name: 'Heap Size', sort: 'heapSize', doStats: true },
        { name: 'OS Load', sort: 'load', doStats: true },
        { name: 'CPU', sort: 'cpu', doStats: true },
        { name: 'Read/s', sort: 'read', doStats: true },
        { name: 'Write/s', sort: 'write', doStats: true },
        { name: 'Searches/s', sort: 'searches', doStats: true },
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

    exclude(type, value) {
      this.StatsService.exclude(type, value)
        .then((response) => {
          this.loadData();
        })
        .catch((error) => {
          this.error    = error;
          this.loading  = false;
        });
    }

    include(type, value) {
      this.StatsService.include(type, value)
        .then((response) => {
          this.loadData();
        })
        .catch((error) => {
          this.error    = error;
          this.loading  = false;
        });
    }

    /* loads the es stats data and computes the total and average values */
    loadData() {
      this.loading = true;

      this.StatsService.getElasticsearchStats({filter: this.searchStats, sortField: this.sortField, desc: this.sortReverse})
        .then((response) => {
          this.loading  = false;
          this.stats    = response;

          this.averageValues = {};
          this.totalValues = {};
          var stats = this.stats.data;

          var columnNames = this.columns.map(function(item) {return item.field || item.sort;});

          for (var i = 1; i < columnNames.length; i++) {
            var columnName = columnNames[i];

            this.totalValues[columnName] = 0;
            for (var s = 0; s < stats.length; s++) {
              this.totalValues[columnName] += stats[s][columnName];
            }
            this.averageValues[columnName] = this.totalValues[columnName]/stats.length;
          }
        })
        .catch((error) => {
          this.error    = error;
          this.loading  = false;
        });
    }
  }


  StatsESController.$inject = ['$scope','$interval','StatsService','UserService'];

  /**
   * Moloch StatsES Directive
   * Displays ES stats
   */
  angular.module('moloch')
     .component('molochEsStats', {
       template  : require('./stats.es.html'),
       controller: StatsESController,
       bindings  : { updateInterval: '<' }
     });

})();
