(function() {

  'use strict';

  let reqPromise; // promise returned from $interval for recurring requests

  /**
   * @class StatsTasksController
   * @classdesc Interacts with moloch es stats section
   * @example
   * '<moloch-tasks-stats update-interval="$ctrl.dataInterval"></moloch-tasks-stats>'
   */
  class StatsTasksController {

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
      this.sortField    = 'index';
      this.sortReverse  = false;

      this.UserService.getSettings()
        .then((response) => { this.settings = response; })
        .catch((error)   => { this.settings = { timezone:'local' }; });

      this.columns = [
        { name: 'Action', sort: 'action', doStats: false },
        { name: 'Description', sort: 'description', doStats: true },
        { name: 'Start Time', sort: 'start_time_in_millis', doStats: true },
        { name: 'Running Time', sort: 'running_time_in_nanos', doStats: true },
        { name: 'Children', sort: 'childrenCount', doStats: false },
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

    /* loads the tasks stats data and computes the total and average values */
    loadData() {
      this.loading = true;

      this.StatsService.getElasticsearchTasks({filter: this.searchStats, sortField: this.sortField, desc: this.sortReverse})
        .then((response) => {
          this.loading  = false;
          this.stats    = response;

          this.averageValues = {};
          this.totalValues = {};
          var stats = this.stats;

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

    cancelTask(taskId) {
      this.StatsService.cancelTask(taskId);
    }
  }


  StatsTasksController.$inject = ['$scope','$interval','StatsService','UserService'];

  /**
   * Moloch StatsTasks Directive
   * Displays Tasks stats
   */
  angular.module('moloch')
     .component('molochEsTasks', {
       template  : require('./stats.tasks.html'),
       controller: StatsTasksController,
       bindings  : { updateInterval: '<' }
     });

})();
