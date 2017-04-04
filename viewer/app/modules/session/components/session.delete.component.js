(function() {

  'use strict';

  /**
   * @class SessionDeleteController
   * @classdesc Interacts with session delete form
   *
   * @example
   * '<session-delete sessions="[session1...sessionN]"
   *    apply-to="'open' || 'visible' || 'matching'"
   *    num-visible="numVisibleSessions" start="startSessionIndex"
   *    num-matching="numQueryMatchingSessions"></session-delete>'
   */
  class SessionDeleteController {

    /**
     * Initialize global variables for this controller
     * @param $scope          Angular application model object
     * @param SessionService  Transacts sessions with the server
     *
     * @ngInject
     */
    constructor($scope, SessionService) {
      this.$scope         = $scope;
      this.SessionService = SessionService;
    }

    $onInit() {
      this.segments = 'no';
      this.loading  = false;
    }

    /* exposed functions --------------------------------------------------- */
    deleteSessions() {
      if (this.filename === '') {
        this.error = 'No filename specified.';
        return;
      }

      this.loading = true;

      let data = {
        start       : this.start,
        applyTo     : this.applyTo,
        segments    : this.segments,
        sessions    : this.sessions,
        numVisible  : this.numVisible,
        numMatching : this.numMatching
      };

      this.SessionService.remove(data)
        .then((response) => {
          this.loading = false;

          let args = {};

          if (response.data.text) {
            args.message = response.data.text;
            args.success = response.data.success;
          }

          //  only reload data if only one was deleted
          if (data.sessions && data.sessions.length === 1) {
            args.reloadData = true;
          }

          // notify parent to close form
          this.$scope.$emit('close:form:container', args);
        })
        .catch((error) => {
          this.error    = error.text;
          this.loading  = false;
        });
    }

    cancel() { // close the form
      this.$scope.$emit('close:form:container');
    }

  }

  SessionDeleteController.$inject = ['$scope', 'SessionService'];

  /**
   * Delete Session Directive
   * Displays session deletion form
   */
  angular.module('moloch')
    .component('sessionDelete', {
      template  : require('html!../templates/session.delete.html'),
      controller: SessionDeleteController,
      bindings  : {
        start       : '<', // where to start the action
        applyTo     : '<', // what to apply the action to [open,visible,matching]
        sessions    : '<', // sessions to apply the action to
        numVisible  : '<', // number of visible sessions to apply action to
        numMatching : '<'  // number of matching sessions to apply action to
      }
    });

})();
