(function() {

  'use strict';

  angular.module('moloch')
    .directive('lazyMenu', ['$compile', function($compile) {
      function compile() {
        let menu = require('../templates/session.field.menu.html');
        let transcludeMenu = $compile(menu);

        // bind the content to the scope
        return function($scope, element) {
          let addMenu = function() {
            let item = $(this).closest('span');

            // add menu to the DOM tree
            transcludeMenu($scope, function(clone) {
              item.append(clone);
            });

            // remove event listener; it should only fire once
            element.off('mousedown', 'a.value', addMenu);
          };

          // when clickable field element is clicked, inject the menu
          element.on('mousedown', 'a.value', addMenu);

          $scope.$on('$destroy', () => {
            element.off('mousedown', 'a.value', addMenu);
          });
        };
      }

      // return the directive configuration
      return({ compile: compile });
    }]);

})();
