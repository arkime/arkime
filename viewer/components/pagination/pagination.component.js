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
   * as they default to 0, 100, and 1 respectively
   */
  class PaginationController {

    /**
     * Initialize global variables for this controller
     * @param $scope        Angular application model object
     * @param $location     Exposes browser address bar URL (based on the window.location)
     *
     * @ngInject
     */
    constructor($scope, $location) {
      this.$scope       = $scope;
      this.$location    = $location;
    }

    /* Callback when component is mounted and ready */
    $onInit() {
      if (!this.start) {
        this.start = 0;       // default to first item
      }

      if (!this.currentPage) {
        this.currentPage = 1; // default to the first page
      }

      if (!this.length) {
        this.length = 50;     // default to page size of 50
      }

      this.setupLengthOptions();

      // update page length if length parameter exists
      let lenParam = this.$location.search().length;
      if (lenParam) { this.length = parseInt(lenParam); }

      // watch for the url parameters to change and update the page
      this.$scope.$on('$routeUpdate', (event, current) => {
        if (current.params.length) { // paging only uses length from url params
          let len = parseInt(current.params.length);
          if (len !== this.length) {
            this.length       = len;
            this.start        = 0; // reset to beginning
            this.currentPage  = 1; // reset to beginning
            this.notifyOfChange();
          }
        }
      });
    }

    /* Creates page length options. Adds a custom value if the page length
     * specified in the url does not exist in the default options */
    setupLengthOptions() {
      this.options = [
        { value: 10,  label: '10 per page'  },
        { value: 50,  label: '50 per page'  },
        { value: 100, label: '100 per page' },
        { value: 200, label: '200 per page' },
        { value: 500, label: '500 per page' }
      ];

      let exists = false;
      for (let i = 0, len = this.options.length; i < len; ++i) {
        if (parseInt(this.length) === this.options[i].value) {
          exists = true;
          break;
        }
      }

      if (!exists) { // add custom option
        this.options.push({
          value: parseInt(this.length),
          label: `${this.length} per page`
        });
      }
    }

    /* let parent controller know about pagination change */
    notifyOfChange() {
      this.$scope.$emit('change:pagination', {
        start       : this.start,
        length      : this.length,
        currentPage : this.currentPage
      });
    }

    /* exposed functions --------------------------------------------------- */
    /**
     * Fired when a pagination control value is changed
     */
    change() {
      // calculate new starting item index
      this.start = (this.currentPage - 1) * this.length;

      this.$location.search('length', this.length);

      this.notifyOfChange();
    }

  }

  PaginationController.$inject = ['$scope','$location'];

  /**
   * Pagination Directive
   * Displays pagination controls
   */
  angular.module('directives.pagination', [])
    .component('molochPagination', {
      template  : require('./pagination.html'),
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
