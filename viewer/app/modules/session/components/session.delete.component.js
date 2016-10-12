(function() {

  'use strict';

  /**
   * @class SessionDeleteController
   * @classdesc Interacts with session delete form
   *
   * @example
   * '<session-delete sessionid="session.id"></session-delete>'
   */
  class SessionDeleteController {

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
    delete() {
      if (this.filename === '') {
        this.error = 'No filename specified.';
        return;
      }

      this.SessionService.delete(this.sessionid, this.include)
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

  SessionDeleteController.$inject = ['$scope', 'SessionService'];

  /**
   * Add Tag Directive
   * Displays add tag area
   */
  angular.module('moloch')
    .component('sessionDelete', {
      template  : require('html!../templates/session.delete.html'),
      controller: SessionDeleteController,
      bindings  : { sessionid : '<' }
    });

})();
