(function() {

  'use strict';

  /**
   * Has Permission directive
   * Determines whether a user has permission to view an element
   * Hides elements a user has no permission to view
   *
   * @example
   * '<div class="btn" has-permission="createEnabled">Admin Button!</div>'
   *
   * Permission values include:
   * 'createEnabled', 'emailSearch', 'enabled', 'headerAuthEnabled',
   * 'removeEnabled', 'webEnabled'
   */
  angular.module('moloch')
    .directive('hasPermission', ['UserService',
    function(UserService) {
      return {
        link  : function(scope, element, attrs) {
          if (!attrs.hasPermission) { return; }

          element.hide(); // hide element by default

          UserService.hasPermission(attrs.hasPermission)
            .then((response) => {
              if (response)   { element.show(); }
            })
            .catch((error) => { element.hide(); });
        }
      };
    }]);

})();
