(function() {

  'use strict';

  /**
   * @class SessionExportCSVController
   * @classdesc Interacts with export csv form
   *
   * @example
   * '<export-csv sessions="[session1...sessionN]"
   *    apply-to="'open' || 'visible' || 'matching'"
   *    num-visible="numVisibleSessions" start="startSessionIndex"
   *    num-matching="numQueryMatchingSessions"></export-csv>'
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
      this.segments = 'no';
      this.filename = 'sessions.csv';
    }

    /* exposed functions --------------------------------------------------- */
    exportCSV() {
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
        numMatching : this.numMatching,
        fields      : []
      };

      if (this.fields) {
        for (let i = 0; i < this.fields.length; ++i) {
          let field = this.fields[i];
          if (field.children) {
            for (let j = 0; j < field.children.length; ++j) {
              let child = field.children[j];
              if (child) { data.fields.push(child.dbField); }
            }
          } else {
            data.fields.push(field.dbField);
          }
        }
      }

      this.SessionService.exportCSV(data);
      this.$scope.$emit('close:form:container');
    }

    cancel() { // close the form
      this.$scope.$emit('close:form:container');
    }

  }

  SessionExportCSVController.$inject = ['$scope', 'SessionService'];

  /**
   * Export CSV Directive
   * Displays export CSV form
   */
  angular.module('moloch')
    .component('exportCsv', {
      template  : require('html!../templates/session.export.csv.html'),
      controller: SessionExportCSVController,
      bindings  : {
        start       : '<', // where to start the action
        applyTo     : '<', // what to apply the action to [open,visible,matching]
        sessions    : '<', // sessions to apply the action to
        numVisible  : '<', // number of visible sessions to apply action to
        numMatching : '<', // number of matching sessions to apply action to
        fields      : '<'  // the fields to display in the csv (same as column headers)
      }
    });

})();
