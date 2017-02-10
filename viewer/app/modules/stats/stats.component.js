(function() {

  'use strict';

  /**
   * @class StatsController
   * @classdesc Interacts with moloch stats page
   * @example
   * '<moloch-fields></moloch-fields>'
   */
  class StatsController {

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
      var self = this;
      this.graphSelect      = 'deltaPacketsPerSec';
      this.graphTimeSelect  = '5';


      this.UserService.getSettings()
        .then((response) => {this.settings = response; })
        .catch((error)   => {this.settings = {timezone: "local"}; });

      this.loadData();

      // watch for the user to leave or return to the page
      // Don't load graph data if the user is not focused on the page!
      // if data is loaded in an inactive (background) tab,
      // the user will experience gaps in their cubism graph data
      // cubism uses setTimeout to delay requests
      // inactive tabs' timeouts are clamped and can fire late;
      // cubism requires little error in the timing of requests
      // for more info, view the "reasons for delays longer than specified" section of:
      // https://developer.mozilla.org/en-US/docs/Web/API/WindowTimers/setTimeout#Inactive_tabs
      if(document.addEventListener) {
        document.addEventListener('visibilitychange', function () {
          if (!self.context) {return;}
          if (document.hidden) {self.context.stop();}
          else {self.context.start();}
        });
      }
    }


    loadData() {
      this.StatsService.getMolochStats({})
        .then((response)  => { 
          this.stats = response.data;
          this.makeStatsGraph(this.graphSelect, parseInt(this.graphTimeSelect, 10));
        })
        .catch((error)    => { this.error = error; });
    }

  /**
   * Creates a cubism graph of time series data for a specific metric
   * https://github.com/square/cubism/wiki/Metric
   * @param {string} metricName the name of the metric to visualize data for
   */
    makeStatsGraph(metricName, interval) {
      var context = this.context;
      var nodes = this.stats.map(function(item) {return item.nodeName;});
      function metric(name) {
        return context.metric(function(startV, stopV, stepV, callback) {
          $.ajax( {
            "dataType": 'json',
            "type": "GET",
            "url": "dstats.json?nodeName=" + name +
                          "&start=" + startV/1000 +
                          "&stop=" + stopV/1000 +
                          "&step=" + stepV/1000 +
                          "&interval=" + interval +
                          "&name=" + metricName,
            "success": function(data) {
              if (!data) {return callback(new Error('Unable to load data'));}
              callback(null, data);
            }
          });
        }, name);
      }

      if (context) {context.stop();} // Stop old context

      context = cubism.context()
          .step(interval * 1000)
          .size(1440);

      context.on("focus", function(i) {
            d3.selectAll(".value").style("right", i === null ? null : context.size() - i + "px");
          });

      $("#statsGraph").empty();
      d3.select("#statsGraph").call(function(div) {
        var metrics = [];
        for (var i = 0, ilen = nodes.length; i < ilen; i++) {
          metrics.push(metric(nodes[i]));
        }

        div.append("div")
            .attr("class", "axis")
            .call(context.axis().orient("top"));

        div.selectAll(".horizon")
            .data(metrics)
          .enter().append("div")
            .attr("class", "horizon")
            .call(context.horizon());

        div.append("div")
            .attr("class", "rule")
            .call(context.rule());

      });
    }
  }


StatsController.$inject = ['$scope', 'StatsService', 'UserService'];

  /**
   * Moloch Stats Directive
   * Displays pcap stats
   */
  angular.module('moloch')
     .component('molochStats', {
       template  : require('html!./stats.html'),
       controller: StatsController
     });

})();
