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
    addTags() {
      // open the form container to remove tags (in session.detail.component)
      this.$scope.$emit('open:form:container', { form:'add:tags' });
    }

    removeTags() {
      // open the form container to remove tags (in session.detail.component)
      this.$scope.$emit('open:form:container', { form:'remove:tags' });
    }

    exportPCAP() {
      // open the form container to export pcaps (in session.detail.component)
      this.$scope.$emit('open:form:container', { form:'export:pcap' });
    }

    scrubPCAP() {
      // open the form container to scrub pcap (in session.detail.component)
      this.$scope.$emit('open:form:container', { form:'scrub:pcap' });
    }

    deleteSession() {
      // open the form container to delete session (in session.detail.component)
      this.$scope.$emit('open:form:container', { form:'delete:session' });
    }

    sendSession(cluster) {
      // open the form container to delete session (in session.detail.component)
      this.$scope.$emit('open:form:container', {
        form:'send:session', cluster:cluster
      });
    }

  }

  SessionActionsController.$inject = ['$scope', 'ConfigService'];

  /**
   * Session Actions Directive
   * Displays available session actions
   */
  angular.module('moloch')
    .component('sessionActions', {
      template    : require('../templates/session.actions.html'),
      controller  : SessionActionsController
    });

})();
