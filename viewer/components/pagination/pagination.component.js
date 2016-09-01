(function() {

  'use strict';

  /**
   * @class PaginationController
   * @classdesc Interacts with the pagination controls
   *
   * Emits a 'change:pagination' event, exposing the new start element,
   * page length, and current page of results
   *
   * @example
   * '<moloch-pagination length="$ctrl.query.length"
   *    records-total="$ctrl.sessions.recordsTotal"
   *    records-filtered="$ctrl.sessions.recordsFiltered"
   *    current-page="$ctrl.currentPage"
   *    start="$ctrl.query.start"></moloch-pagination>'
   *
   * Note: you can leave out start, length, and current-page
   * as they default to 0, 50, and 1 respectively
   */
  class PaginationController {

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
      if (!this.start) {
        this.start = 0;   // default to first item
      }

      if (!this.length) {
        this.length = 50; // default with page size of 50
      }

      if (!this.currentPage) {
        this.currentPage = 1; // default to the first page
      }
    }


    /* exposed functions --------------------------------------------------- */
    /**
     * Fired when a pagination control value is changed
     */
    change() {
      // calculate new starting item
      this.start = (this.currentPage - 1) * this.length;

      // let parent know about pagination change
      this.$scope.$emit('change:pagination', {
        start       : this.start,
        length      : this.length,
        currentPage : this.currentPage
      });
    }

  }

  PaginationController.$inject = ['$scope'];

  /**
   * Pagination Directive
   * Displays pagination controls
   */
  angular.module('directives.pagination', [])
    .component('molochPagination', {
      template  : require('html!./pagination.html'),
      controller: PaginationController,
      bindings  : {
        start           : '<',  // the item to start at
        length          : '<',  // the page length of the results
        currentPage     : '<',  // the current page of results
        recordsTotal    : '=',  // the total number of records in the db
        recordsFiltered : '='   // the total number of results
      }
    });

})();
