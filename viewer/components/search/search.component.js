(function() {

  'use strict';

  /**
   * @class SearchController
   * @classdesc Interacts with the search controls
   */
  class SearchController {

    /**
     * Initialize global variables for this controller
     * @param $scope Angular application model object
     *
     * @ngInject
     */
    constructor($scope) {
      this.$scope = $scope;
    }

    /* Callback when component is mounted and ready */
    $onInit() {
      this.timeRange      = 1;
      this.startTimePopup = { opened: false };
      this.stopTimePopup  = { opened: false };
      this.dateTimeFormat = 'yyyy/MM/dd HH:mm:ss';

      if (!this.startTime && !this.stopTime) {
        // default to one hour
        var ms = new Date().getTime();
        this.stopTime   = ms - 1000;
        this.startTime  = ms - 1000 - 3600000;

        this.change();
      }
    }


    /* exposed functions --------------------------------------------------- */
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

  SearchController.$inject = ['$scope'];

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
