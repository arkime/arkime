(function() {

  'use strict';

  /**
   * Error Directive
   * Displays an error message
   *
   * @example
   * '<error ng-if="$ctrl.error" message="$ctrl.error"></error>'
   */
  angular.module('directives.error', [])
    .component('error', {
      templateUrl : '/components/error/error.html',
      bindings    : {
        message   : '<'
      }
    });

})();
