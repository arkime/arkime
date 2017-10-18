(function() {

  'use strict';

  /**
   * @class SessionSendController
   * @classdesc Interacts with send session form
   *
   * @example
   * '<session-send sessions="[session1...sessionN]"
   *    apply-to="'open' || 'visible' || 'matching'"
   *    num-visible="numVisibleSessions" start="startSessionIndex"
   *    num-matching="numQueryMatchingSessions"
   *    cluster="destinationCluster"></session-send>'
   */
  class SessionSendController {

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
      this.tags     = '';
      this.loading  = false;
    }

    /* exposed functions --------------------------------------------------- */
    send() {
      this.loading = true;

      let data = {
        tags        : this.tags,
        start       : this.start,
        cluster     : this.cluster,
        applyTo     : this.applyTo,
        segments    : this.segments,
        sessions    : this.sessions,
        numVisible  : this.numVisible,
        numMatching : this.numMatching
      };

      this.SessionService.send(data)
        .then((response) => {
          this.loading = false;

          let args = {};

          if (response.data.text) {
            args.message = response.data.text;
            args.success = response.data.success;
          }

          // notify parent to close form
          this.$scope.$emit('close:form:container', args);
        })
        .catch((error) => {
          this.error    = error;
          this.loading  = false;
        });
    }

    cancel() { // close the form
      this.$scope.$emit('close:form:container');
    }

  }

  SessionSendController.$inject = ['$scope', 'SessionService'];

  /**
   * Send Session Directive
   * Displays send session form
   */
  angular.module('moloch')
    .component('sessionSend', {
      template  : require('../templates/session.send.html'),
      controller: SessionSendController,
      bindings  : {
        start       : '<', // where to start the action
        cluster     : '<', // the cluster to send the session(s) to
        applyTo     : '<', // what to apply the action to [open,visible,matching]
        sessions    : '<', // sessions to apply the action to
        numVisible  : '<', // number of visible sessions to apply action to
        numMatching : '<'  // number of matching sessions to apply action to
      }
    });

})();
