(function() {

  'use strict';

  /**
   * SessionField Directive
   * Displays session fields
   *
   * @example
   * '<session-field column="col" session="session"></session-field>'
   */
  angular.module('moloch')
    .component('sessionField', {
      template  : require('html!../templates/session.field.html'),
      bindings  : { session : '<', column : '<' }
    });

})();
