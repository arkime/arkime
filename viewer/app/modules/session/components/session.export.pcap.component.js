(function() {

  'use strict';

  /**
   * @class SessionExportPCAPController
   * @classdesc Interacts with export pcap form
   *
   * @example
   * '<export-pcap sessions="[session1...sessionN]"
   *    apply-to="'open' || 'visible' || 'matching'"
   *    num-visible="numVisibleSessions" start="startSessionIndex"
   *    num-matching="numQueryMatchingSessions"></export-pcap>'
   */
  class SessionExportPCAPController {

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
      this.filename = 'sessions.pcap';
    }

    /* exposed functions --------------------------------------------------- */
    exportPCAP() {
      if (this.filename === '') {
        this.error = 'No filename specified.';
        return;
      }

      let data = {
        start       : this.start,
        applyTo     : this.applyTo,
        filename    : this.filename,
        segments    : this.segments,
        sessions    : this.sessions,
        numVisible  : this.numVisible,
        numMatching : this.numMatching
      };

      this.SessionService.exportPCAP(data);
      this.$scope.$emit('close:form:container');
    }

    cancel() { // close the form
      this.$scope.$emit('close:form:container');
    }

  }

  SessionExportPCAPController.$inject = ['$scope', 'SessionService'];

  /**
   * Export PCAP Directive
   * Displays export PCAP form
   */
  angular.module('moloch')
    .component('exportPcap', {
      template  : require('../templates/session.export.pcap.html'),
      controller: SessionExportPCAPController,
      bindings  : {
        start       : '<', // where to start the action
        applyTo     : '<', // what to apply the action to [open,visible,matching]
        sessions    : '<', // sessions to apply the action to
        numVisible  : '<', // number of visible sessions to apply action to
        numMatching : '<'  // number of matching sessions to apply action to
      }
    });

})();
