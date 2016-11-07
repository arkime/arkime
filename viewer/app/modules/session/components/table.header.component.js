(function() {

  'use strict';

  var defaultTableState = {
    order         : [['fp', 'asc']],
    visibleHeaders: ['', 'fp', 'lp', 'a1', 'p1', 'a2', 'p2', 'pa', 'by', 'no', 'info']
  };

  angular.module('moloch')

    /**
     * Table Header Directive
     *
     * @example
     * '<thead table-header></thead>'
     */
    .directive('tableHeader', ['SessionService',
    function(SessionService) {
      return {
        scope   : {},
        restrict: 'A',
        template: require('html!../templates/table.header.html'),
        link    : function(scope, element, attrs) {

          // TODO: display error saving table state
          /* setup --------------------------------------------------------- */
          SessionService.getTableState()
            .then((response) => {
              scope.tableState = response.data;
              if (Object.keys(scope.tableState).length === 0) {
                scope.tableState = defaultTableState;
              }
              // notify session component of sort order (updates query)
              scope.$emit('change:sort', {
                sorts   : scope.tableState.order,
                refresh : false,
                persist : false
              });
            })
            .catch((error) => {
              scope.tableStateError = error;
            });

          /* LISTEN! */
          scope.$on('change:sort', function(event, args) {
            scope.tableState.order = args.sorts;
            if (args.persist) {
              SessionService.saveTableState(scope.tableState)
                .catch((error) => {
                  scope.tableStateError = error;
                });
            }
          });

        }
      };
    }]);

})();
