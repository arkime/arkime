(function() {

  'use strict';

  angular.module('moloch')

    /**
     * Column Header Directive
     *
     * @example
     */
    .directive('colheader', [
    function() {
      return {
        restrict: 'A',
        scope   : { order: '=', colName: '=', colId: '=' },
        template: require('html!../templates/colheader.html'),
        link    : function(scope, element, attrs) {

          /* setup --------------------------------------------------------- */
          /**
           * Sorts the sessions by the clicked column
           * @param {Object} event The click event that triggered the sort
           */
          scope.sortBy = function(event) {
            if (!scope.order) { return; } // it's an unsortable column

            if (scope.isSorted() >= 0) {
              // the table is already sorted by this element
              if (!event.shiftKey) {
                var item    = scope.toggleSortOrder();
                scope.order = [item];
              } else {
                // if it's a shift click - toggle the order between 3 states:
                // 'asc' -> 'desc' -> removed from sorts
                if (scope.getSortOrder() === 'desc') {
                  for (var i = 0, len = scope.order.length; i < len; ++i) {
                    if (scope.order[i][0] === scope.colId) {
                      scope.order.splice(i, 1);
                      break;
                    }
                  }
                } else {
                  scope.toggleSortOrder();
                }
              }
            } else { // sort by a new column
              if (!event.shiftKey) {
                // if it's a regular click - remove other sorts and add this one
                scope.order = [[ scope.colId, 'asc' ]];
              } else {
                // if it's a shift click - add it to the list
                scope.order.push([ scope.colId, 'asc' ]);
              }
            }

            // notify parent of changed sort order or element
            scope.$emit('change:sort', { sorts: scope.order });
          };

          /**
           * Determines if the table is being sorted by this column
           * @return {integer} i  The index of the array in which the column
           *                      exists or -1 if it is not sorted
           */
          scope.isSorted = function() {
            for (var i = 0, len = scope.order.length; i < len; ++i) {
              if (scope.order[i][0] === scope.colId) { return i; }
            }

            return -1;
          };

          /**
           * Determines the sort order of a column
           * @return {string} order The sort order of the column
           */
          scope.getSortOrder = function() {
            for (var i = 0, len = scope.order.length; i < len; ++i) {
              if (scope.order[i][0] === scope.colId) {
                return scope.order[i][1];
              }
            }
          };

          /**
           * Finds sort element and toggles the sort order
           * ('asc' -> 'desc' & 'desc' -> 'asc')
           * @return {Array} item The sort item with updated order
           */
          scope.toggleSortOrder = function() {
            for (var i = 0, len = scope.order.length; i < len; ++i) {
              var item = scope.order[i];
              if (item[0] === scope.colId) {
                if (item[1] === 'asc') { item[1] = 'desc'; }
                else { item[1] = 'asc'; }
                return item;
              }
            }
          };

        }
      };
    }]);

})();
