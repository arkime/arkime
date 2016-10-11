(function() {

  'use strict';

  /**
   * @class SessionActionsController
   * @classdesc Interacts with add tag area
   *
   * @example
   * '<session-actions></session-actions>'
   */
  class SessionActionsController {

    /**
     * Initialize global variables for this controller
     * @param $scope          Angular application model object
     * @param ConfigService   Transacts app configurations with the server
     *
     * @ngInject
     */
    constructor($scope, ConfigService) {
      this.$scope         = $scope;
      this.ConfigService  = ConfigService;
    }

    $onInit() {
      this.isopen = false; // actions are closed until user opens them

      this.ConfigService.getMolochClusters()
        .then((clusters) => {
          this.molochclusters = clusters;
        });
    }

    /* exposed functions --------------------------------------------------- */
    removeTags() {
      // open the form container for removing tags (in session.detail.component)
      this.$scope.$emit('removing:tags');
    }

    exportPCAP() {
      // open the form container for exporting pcaps (in session.detail.component)
      this.$scope.$emit('export:pcap');
    }

  }

  SessionActionsController.$inject = ['$scope', 'ConfigService'];

  /**
   * Session Actions Directive
   * Displays available session actions
   */
  angular.module('moloch')
    .component('sessionActions', {
      template    : require('html!../templates/session.actions.html'),
      controller  : SessionActionsController
    });

})();
