(function() {

  'use strict';

  /**
   * @class SessionExportPCAPController
   * @classdesc Interacts with export pcap form
   *
   * @example
   * '<export-pcap sessionid="session.id"></export-pcap>'
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
      this.include  = 'no';
      this.filename = 'sessions.pcap';
    }

    /* exposed functions --------------------------------------------------- */
    exportPCAP() {
      if (this.filename === '') {
        this.error = 'No filename specified.';
        return;
      }

      this.SessionService.exportPCAP(this.sessionid, this.filename, this.include);
      this.$scope.$emit('close:form:container');
    }

    cancel() { // close the form container (in session.detail.component)
      this.$scope.$emit('close:form:container');
    }

  }

  SessionExportPCAPController.$inject = ['$scope', 'SessionService'];

  /**
   * Add Tag Directive
   * Displays add tag area
   */
  angular.module('moloch')
    .component('exportPcap', {
      template  : require('html!../templates/session.export.pcap.html'),
      controller: SessionExportPCAPController,
      bindings  : { sessionid : '<' }
    });

})();
