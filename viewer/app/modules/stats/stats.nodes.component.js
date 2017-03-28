(function() {

  'use strict';

  /**
   * @class StatsNodesController
   * @classdesc Interacts with moloch stats page
   * @example
   * '<moloch-fields></moloch-fields>'
   */
  class StatsNodesController {

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
      this.expanded     = {};
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
        { name: '', doStats: false},
        { name: 'Node', sort: 'nodeName', doStats: false },
        { name: 'Time', sort: 'currentTime', doStats: true },
        { name: 'Sessions', sort: 'monitoring', doStats: true },
        { name: 'Free Space', sort: 'freeSpaceM', doStats: true },
        { name: 'CPU', sort: 'cpu', doStats: true },
        { name: 'Memory', sort: 'memory', doStats: true },
        { name: 'Packet Q', sort: 'packetQueue', doStats: true },
        { name: 'Packet/s', sort: 'deltaPackets', field: 'deltaPacketsPerSec', doStats: true },
        { name: 'Bytes/s', sort: 'deltaBytes', field: 'deltaBytesPerSec', doStats: true },
        { name: 'Sessions/s', sort: 'deltaSessions', field: 'deltaSessionsPerSec', doStats: true },
        { name: 'Packet Drops/s', sort: 'deltaDropped', field: 'deltaDroppedPerSec', doStats: true },
        { name: 'Overload Drops/s', sort: 'deltaOverloadDropped', field: 'deltaOverloadDroppedPerSec', doStats: true },
        { name: 'ES Drops/s', sort: 'deltaESDropped', field: 'deltaESDroppedPerSec', doStats: true }
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
      this.StatsService.getMolochStats({filter: this.searchStats, sortField: this.sortField, desc: this.sortReverse, start: this.query.start, length:this.query.length})
        .then((response)  => {
          this.stats = response;

          this.averageValues = {};
          this.totalValues = {};
          var stats = this.stats.data;

          var columnNames = this.columns.map(function(item) {return item.field || item.sort;});
          columnNames.push("memoryP");
          columnNames.push("freeSpaceP");

          for (var i = 3; i < columnNames.length; i++) {
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
    toggleStatDetail(stat) {
      var self = this;
      this.expanded[stat.id] = !this.expanded[stat.id];

      $(document.getElementById("statsGraph-" + stat.id)).empty();

      if (!this.expanded[stat.id]) {return;}

      var dcontext = cubism.context()
          .serverDelay(0)
          .clientDelay(0)
          .step(60e3)
          .size(1440);

      function dmetric(name, mname) {
        return dcontext.metric(function(startV, stopV, stepV, callback) {
          self.StatsService.getDetailStats({nodeName: stat.id,
                                            start: startV/1000,
                                            stop: stopV/1000,
                                            step: stepV/1000,
                                            interval: 60,
                                            name: mname})
            .then((response)  => {
              callback(null, response);
            })
            .catch((error)    => { return callback(new Error('Unable to load data')); });
        }, name);
      }
      var headerNames = this.columns.map(function(item) {return item.name;});
      var dataSrcs = this.columns.map(function(item) {return item.sort;});
      var metrics = [];
      for (var i = 3; i < headerNames.length; i++) {
        if (headerNames[i].match("/s")) {
          metrics.push(dmetric(headerNames[i].replace("/s", "/m"), dataSrcs[i].replace("PerSec", "")));
        } else {
          metrics.push(dmetric(headerNames[i], dataSrcs[i]));
        }
      }

      d3.select("#statsGraph-" + stat.id).call(function(div) {
        div.append("div")
            .attr("class", "axis")
            .call(dcontext.axis().orient("top"));

        div.selectAll(".horizon")
          .data(metrics)
          .enter().append("div")
            .attr("class", "horizon")
            .call(dcontext.horizon().colors(self.colors()));

        div.append("div")
            .attr("class", "rule")
            .call(dcontext.rule());

      });

      dcontext.on("focus", function(i) {
          d3.selectAll(".value").style("right", i === null ? null : dcontext.size() - i + "px");
      });
    }
  }

  StatsNodesController.$inject = ['$scope', 'StatsService', 'UserService'];

  /**
   * Moloch StatsNodes Directive
   * Displays pcap stats
   */
  angular.module('moloch')
     .component('molochNodesStats', {
       template  : require('html!./stats.nodes.html'),
       controller: StatsNodesController,
       bindings   : { colors: '&' }
     });

})();
