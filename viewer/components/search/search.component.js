(function() {

  'use strict';

  var hourMS      = 3600000;
  var currentTime = new Date().getTime();

  /**
   * @class SearchController
   * @classdesc Interacts with the search controls
   */
  class SearchController {

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
      if (this.$routeParams.date) { // time range is available
        this.timeRange = this.$routeParams.date;
      } else if(this.$routeParams.startTime && this.$routeParams.stopTime) {
        // start and stop times available
        this.timeRange  = '0'; // custom time range
        this.stopTime   = parseInt(this.$routeParams.stopTime, 10);
        this.startTime  = parseInt(this.$routeParams.startTime, 10);
      } else if (!this.$routeParams.date &&
          !this.$routeParams.startTime && !this.$routeParams.stopTime) {
        // there are no time query parameters, so set defaults
        this.timeRange = '1'; // default to 1 hour
        this.$location.search('date', this.timeRange);
      }

      if (this.$routeParams.expression) {
        this.expression = { value: this.$routeParams.expression };
      } else { this.expression = { value: null }; }

      this.strictly = false;
      if (this.$routeParams.strictly) { this.strictly = true; }

      this.startTimePopup = { opened: false };
      this.stopTimePopup  = { opened: false };
      this.dateTimeFormat = 'yyyy/MM/dd HH:mm:ss';
      this.altInputFormats = ['yyyy/M!/d! H:mm:ss'];

      this.change();
    }


    /* exposed functions --------------------------------------------------- */
    /**
     * Fired when the time range value changes
     */
    changeTimeRange() {
      this.$location.search('date', this.timeRange);
      this.$location.search('stopTime', null);
      this.$location.search('startTime', null);

      this.change();
    }

    /**
     * Fired when a date value is changed
     */
     changeDate() {
       this.timeRange = '0'; // custom time range

       this.$location.search('date', null);
       this.$location.search('stopTime', this.stopTime);
       this.$location.search('startTime', this.startTime);

       this.change();
     }

     /**
      * Fired when change bounded checkbox is (un)checked
      */
     changeBounded() {
       this.strictly = !this.strictly;

       if (this.strictly) {
         this.$location.search('strictly', 'true');
       } else {
         this.$location.search('strictly', null);
       }

       this.change();
     }

    /**
     * Fired when a search control value is changed
     */
    change() {
      // update the expression
      if (this.expression.value && this.expression.value !== '') {
        this.$location.search('expression', this.expression.value);
      } else {
        this.$location.search('expression', null);
      }

      if (this.timeRange !== '0') {
        // if it's not a custom time range, update the time
        currentTime = new Date().getTime();

        this.stopTime   = currentTime;
        this.startTime  = currentTime - (hourMS * this.timeRange);
      }

      this.deltaTime  = this.stopTime - this.startTime;

      if (this.startTime && this.stopTime) {
        this.$scope.$emit('change:search', {
          expression: this.expression.value,
          startTime : this.startTime,
          stopTime  : this.stopTime,
          strictly  : this.strictly
        });
      }
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
      controller: SearchController
    });

})();
