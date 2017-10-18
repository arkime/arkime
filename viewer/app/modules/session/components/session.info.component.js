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

      if (!this.field || !this.field.children) { return; }

      this.fieldMap = {};

      // build field map to send field to session-field
      for (let i = 0, len = this.field.children.length; i < len; ++i) {
        let field = this.field.children[i];
        this.fieldMap[field.exp] = field;
      }
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
   * SessionInfo Directive
   * Displays the session info cell
   */
  angular.module('moloch')
    .component('sessionInfo', {
      template  : require('../templates/session.info.html'),
      controller: SessionInfoController,
      bindings  : { session : '<', field : '<' }
    });

})();
