(function() {

  'use strict';

  /**
   * @class ColheaderController
   * @classdesc Interacts with column headers
   */
  class ColheaderController {

    /**
     * Initialize global variables for this controller
     * @param $scope Angular application model object
     *
     * @ngInject
     */
    constructor($scope) {
      this.$scope = $scope;
    }


    /* exposed functions --------------------------------------------------- */
    /**
     * Sorts the sessions by the clicked column
     */
    sort() {
      if (this.element === this.colId) {
        // the table is already sorted by this element
        // so, toggle the sort order
        this.order = ColheaderController.toggleSortOrder(this.order);
      } else { // sort by a new column
        this.element  = this.colId;
        this.order    = 'asc'; // default
      }

      // notify parent of changed sort order or element
      this.$scope.$emit('change:sort',
        { sortOrder: this.order, sortElement: this.element });

    }


    /* internal functions -------------------------------------------------- */
    /**
     * Toggles the sort order ('asc' -> 'desc' & 'desc' -> 'asc')
     * @param {string} order      The current order of the sortOrder parameter
     * @return {string} newOrder  The new order of the sortOrder parameter
     */
    static toggleSortOrder(order) {
      if (order === 'asc') {
        return 'desc';
      } else return 'asc';
    }

  }

  ColheaderController.$inject = ['$scope'];

  /**
   * Colheader Directive
   * Displays a column table header with sorting
   * @example
   * '<col-header></col-header>'
   */
  angular.module('moloch')
    .component('colHeader', {
      templateUrl : '/modules/session/colheader.html',
      controller  : ColheaderController,
      transclude  : true,
      bindings    : {
        colName   : '<',
        colId     : '<',
        element   : '=',
        order     : '='
      }
    });

})();
