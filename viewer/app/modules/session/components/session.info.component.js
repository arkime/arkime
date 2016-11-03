(function() {

  'use strict';

  /**
   * @class SessionInfoController
   * @classdesc Interacts with session information
   * @example
   * '<session-info session="$ctrl.session"></session-info>'
   */
  class SessionInfoController {

    /* Callback when component is mounted and ready */
    $onInit() {
      this.limit        = 3;
      this.initialLimit = this.limit;

      this.showAll      = false;
    }

    /* exposed functions --------------------------------------------------- */
    /* Show/hides more information */
    toggleShowAll() {
      this.showAll = !this.showAll;

      if (this.showAll) { this.limit = 9999; }
      else { this.limit = 3; }
    }
  }

  /**
   * Colheader Directive
   * Displays a column table header with sorting
   */
  angular.module('moloch')
    .component('sessionInfo', {
      template  : require('html!../templates/session.info.html'),
      controller: SessionInfoController,
      bindings  : { session : '<' }
    });

})();
