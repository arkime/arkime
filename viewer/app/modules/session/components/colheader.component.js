(function() {

  'use strict';

  /**
   * @class ColheaderController
   * @classdesc Interacts with column headers
   *
   * Emits a 'change:sort' event, exposing the new sortOrder and sortElement
   *
   * @example
   * '<colheader col-name="'Start'" col-id="'fp'"
   *    sort="$ctrl.query.sort"></colheader>'
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
    sortBy(event) {
      if (!this.sorts) { return; } // it's an unsortable column

      if (this.isSorted(this.colId) >= 0) {
        // the table is already sorted by this element
        if (!event.shiftKey) {
          var item  = this.toggleSortOrder();
          this.sorts = [item];
        } else {
          // if it's a shift click - toggle the order between 3 states:
          // 'asc' -> 'desc' -> removed from sorts
          if (this.getSortOrder() === 'desc') {
            for (var i = 0; i < this.sorts.length; ++i) {
              if (this.sorts[i].element === this.colId) {
                this.sorts.splice(i, 1);
                break;
              }
            }
          } else {
            this.toggleSortOrder();
          }
        }
      } else { // sort by a new column
        if (!event.shiftKey) {
          // if it's a regular click - remove other sorts and add this one
          this.sorts = [{ element: this.colId, order: 'asc' }];
        } else {
          // if it's a shift click - add it to the list
          this.sorts.push({ element: this.colId, order: 'asc' });
        }
      }

      // notify parent of changed sort order or element
      this.$scope.$emit('change:sort', { sorts: this.sorts });
    }

    /**
     * Determines whether the column is being sorted
     * @return {int} index The index in the array of sorted items
     *                     or -1 if it is not sorted
     */
    isSorted() {
      for (var i = 0; i < this.sorts.length; ++i) {
        if (this.sorts[i].element === this.colId) { return i; }
      }

      return -1;
    }

    /**
     * Determines the sort order of a column
     * @return {string} order The sort order of the column
     */
    getSortOrder() {
      for (var i = 0; i < this.sorts.length; ++i) {
        if (this.sorts[i].element === this.colId) {
          return this.sorts[i].order;
        }
      }
    }

    /**
     * Finds sort element and toggles the sort order
     * ('asc' -> 'desc' & 'desc' -> 'asc')
     * @return {Object} item The sort item with updated order
     */
    toggleSortOrder() {
      for (var i = 0; i < this.sorts.length; ++i) {
        var item = this.sorts[i];
        if (item.element === this.colId) {
          if (item.order === 'asc') { item.order = 'desc'; }
          else { item.order = 'asc'; }
          return item;
        }
      }
    }

  }

  ColheaderController.$inject = ['$scope'];

  /**
   * Colheader Directive
   * Displays a column table header with sorting
   */
  angular.module('moloch')
    .component('colheader', {
      template  : require('html!../templates/colheader.html'),
      controller: ColheaderController,
      bindings  : {
        colName : '<',  // human readable name of the column
        colId   : '<',  // unique id of the data the column displays
        // array of sort items to be used in queries
        // e.g. [{ element: 'lp', order: 'asc' }]
        // (if the column is not sortable, this should be null)
        sorts   : '='
      }
    });

})();
