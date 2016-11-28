(function() {

  'use strict';

  /**
   * @class SessionScrubPCAPController
   * @classdesc Interacts with scrub pcap form
   *
   * @example
   * '<scrub-pcap sessionid="session.id"></scrub-pcap>'
   */
  class SessionScrubPCAPController {

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
      this.include = 'no';
    }

    /* exposed functions --------------------------------------------------- */
    scrubPCAP() {
      if (this.filename === '') {
        this.error = 'No filename specified.';
        return;
      }

      this.SessionService.scrubPCAP(this.sessionid, this.include)
        .then((response) => {
          this.$scope.$emit('close:form:container', { reloadData: true });
        })
        .catch((error) => {
          this.error = error;
        });
    }

    cancel() { // close the form container (in session.detail.component)
      this.$scope.$emit('close:form:container');
    }

  }

  SessionScrubPCAPController.$inject = ['$scope', 'SessionService'];

  /**
   * Scrub PCAP Directive
   * Displays scrub PCAP
   */
  angular.module('moloch')
    .component('scrubPcap', {
      template  : require('html!../templates/session.scrub.pcap.html'),
      controller: SessionScrubPCAPController,
      bindings  : { sessionid : '<' }
    });

})();
