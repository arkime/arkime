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
     * @param $scope  Angular application model object
     * @param $window
     *
     * @ngInject
     */
    constructor($scope, $window) {
      this.$scope   = $scope;
      this.$window  = $window;
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

      // qs.push({name: "ids", value: $("#sessionActionsMenu").attr("sessionid")});
      //
      // this.$window.location.href = 'sessions.pcap/' + filename + "?" + $.param(qs);
      this.$window.location.href = `sessions.pcap/${this.filename}?name=ids&value=${this.sessionid}`;

      // this.SessionService.exportPCAP(this.sessionid, this.filename, this.include)
      //   .then((response) => {
      //     this.filename = 'sessions.pcap';
      //     // close the form container (in session.detail.component)
      //     this.$scope.$emit('close:form:container');
      //   })
      //   .catch((error) => {
      //     this.error = error;
      //   });
    }

    cancel() { // close the form container (in session.detail.component)
      this.$scope.$emit('close:form:container');
    }

  }

  SessionExportPCAPController.$inject = ['$scope', '$window'];

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
