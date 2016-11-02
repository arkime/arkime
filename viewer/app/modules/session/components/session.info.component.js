(function() {

  'use strict';

  /**
   * @class SessionInfoController
   * @classdesc Interacts with session information
   * @example
   * '<session-info session="$ctrl.session"></session-info>'
   */
  class SessionInfoController {

    /**
     * Initialize global variables for this controller
     * @param $scope Angular application model object
     *
     * @ngInject
     */
    constructor($scope) {
      this.$scope = $scope;
    }

    $onInit() {
      this.limit = 3;
      this.initialLimit = this.limit;
      this.showAll = false;
    }

    /* exposed functions --------------------------------------------------- */
    toggleShowAll() {
      this.showAll = !this.showAll;

      if (this.showAll) { this.limit = 9999; }
      else { this.limit = 3; }
    }
  }

  SessionInfoController.$inject = ['$scope'];

  /**
   * Colheader Directive
   * Displays a column table header with sorting
   */
  angular.module('moloch')
    .component('sessionInfo', {
      template  : require('html!../templates/session.info.html'),
      controller: SessionInfoController,
      bindings  : { session : '<' }
    });

})();
