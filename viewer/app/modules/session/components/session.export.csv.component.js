(function() {

  'use strict';

  /**
   * @class SessionExportCSVController
   * @classdesc Interacts with export csv form
   *
   * @example
   * '<export-csv sessionid="session.id"></export-csv>'
   */
  class SessionExportCSVController {

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
      this.filename = 'sessions.csv';
    }

    /* exposed functions --------------------------------------------------- */
    exportCSV() {
      if (this.filename === '') {
        this.error = 'No filename specified.';
        return;
      }

      this.SessionService.exportCSV(this.sessionid, this.filename, this.include);
      this.$scope.$emit('close:form:container');
    }

    cancel() { // close the form container
      this.$scope.$emit('close:form:container');
    }

  }

  SessionExportCSVController.$inject = ['$scope', 'SessionService'];

  /**
   * Export CSV Directive
   * Displays export CSV
   */
  angular.module('moloch')
    .component('exportCsv', {
      template  : require('html!../templates/session.export.csv.html'),
      controller: SessionExportCSVController,
      bindings  : { sessionid : '<' }
    });

})();
