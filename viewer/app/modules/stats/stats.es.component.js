(function() {

  'use strict';

  /**
   * @class StatsESController
   * @classdesc Interacts with moloch stats page
   * @example
   * '<moloch-fields></moloch-fields>'
   */
  class StatsESController {

    /**
     * Initialize global variables for this controller
     * @param StatsService  Transacts stats with the server
     *
     * @ngInject
     */
    constructor($scope, StatsService, UserService) {
      this.$scope         = $scope;
      this.StatsService   = StatsService;
      this.UserService    = UserService;
    }

    /* Callback when component is mounted and ready */
    $onInit() {
      this.sortField    = 'nodeName';
      this.sortReverse  = false;
      this.query        = {length: 50, start: 0};
      this.currentPage  = 1;

      this.$scope.$on('change:pagination', (event, args) => {
        // pagination affects length, currentPage, and start
        this.query.length = args.length;
        this.query.start  = args.start;
        this.currentPage  = args.currentPage;

        this.loadData();
      });

      this.UserService.getSettings()
        .then((response) => {this.settings = response; })
        .catch((error)   => {this.settings = {timezone: "local"}; });

      this.columns = [
        { name: 'Name', sort: 'name', doStats: false },
        { name: 'Documents', sort: 'docs', doStats: true },
        { name: 'Disk Storage', sort: 'storeSize', doStats: true },
        { name: 'Heap Size', sort: 'heapSize', doStats: true },
        { name: 'OS Load', sort: 'load', doStats: true },
        { name: 'CPU', sort: 'cpu', doStats: true },
        { name: 'Read/s', sort: 'read', doStats: true },
        { name: 'Write/s Q', sort: 'write', doStats: true },
        { name: 'Searches/s', sort: 'searches', doStats: true },
      ];

      this.loadData();
      setInterval(this.loadData.bind(this), 5000);
    }

    columnClick(name) {
      this.sortField=name; 
      this.sortReverse = !this.sortReverse;
      this.loadData();
    }

    loadData() {
      this.StatsService.getElasticsearchStats({filter: this.searchStats, sortField: this.sortField, desc: this.sortReverse, start: this.query.start, length:this.query.length})
        .then((response)  => { 
          this.stats = response; 

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
        .catch((error)    => { this.error = error; });
    }
  }

  StatsESController.$inject = ['$scope', 'StatsService', 'UserService'];

  /**
   * Moloch StatsES Directive
   * Displays pcap stats
   */
  angular.module('moloch')
     .component('molochEsStats', {
       template  : require('html!./stats.es.html'),
       controller: StatsESController
     });

})();
