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
      template  : require('./error.html'),
      bindings  : {
        message : '<'
      }
    });

})();
