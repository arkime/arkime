(function() {

  'use strict';

  var hourMS      = 3600000;
  var currentTime = new Date().getTime();

  /**
   * @class SearchController
   * @classdesc Interacts with the search controls
   */
  class SearchController {
    // hourMS      = 3600000;
    // currentTime = new Date().getTime();

    /**
     * Initialize global variables for this controller
     * @param $scope        Angular application model object
     * @param $routeParams  Retrieve the current set of route parameters
     * @param $location     Exposes browser address bar URL (based on the window.location)
     *
     * @ngInject
     */
    constructor($scope, $routeParams, $location) {
      this.$scope       = $scope;
      this.$routeParams = $routeParams;
      this.$location    = $location;
    }

    /* Callback when component is mounted and ready */
    $onInit() {
      if (!this.$routeParams.startTime || !this.$routeParams.stopTime) {
        this.timeRange = '1'; // default to 1 hour
        this.stopTime  = currentTime;
        this.startTime = currentTime - (hourMS * this.timeRange);
        this.$location.search('stopTime', this.stopTime);
        this.$location.search('startTime', this.startTime);
      } else {
        this.timeRange  = '0'; // custom time range
        this.stopTime   = parseInt(this.$routeParams.stopTime, 10);
        this.startTime  = parseInt(this.$routeParams.startTime, 10);
      }

      this.startTimePopup = { opened: false };
      this.stopTimePopup  = { opened: false };
      this.dateTimeFormat = 'yyyy/MM/dd HH:mm:ss';

      this.change();
    }


    /* exposed functions --------------------------------------------------- */
    /**
     * Fired when the time range value changes
     */
    changeTimeRange() {
      this.stopTime   = currentTime;
      this.startTime  = currentTime - (hourMS * this.timeRange);

      this.$location.search('stopTime', this.stopTime);
      this.$location.search('startTime', this.startTime);

      this.change();
    }

    /**
     * Fired when a date value is changed
     */
     changeDate() {
       this.timeRange = '0';

       this.$location.search('stopTime', this.stopTime);
       this.$location.search('startTime', this.startTime);

       this.change();
     }

    /**
     * Fired when a search control value is changed
     */
    change() {
      this.$scope.$emit('change:search', {
        expression: this.expression,
        startTime : this.startTime,
        stopTime  : this.stopTime
      });
    }

  }

  SearchController.$inject = ['$scope','$routeParams','$location'];

  /**
   * Search Directive
   * Displays searching controls
   */
  angular.module('directives.search', [])
    .component('molochSearch', {
      template  : require('html!./search.html'),
      controller: SearchController,
      bindings  : {
        startTime : '<',  // the start of the time range
        stopTime  : '<',  // the end of the time range
        expression: '<'   // the search expression
      }
    });

})();
