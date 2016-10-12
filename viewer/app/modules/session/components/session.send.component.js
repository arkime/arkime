(function() {

  'use strict';

  /**
   * @class SessionSendController
   * @classdesc Interacts with send session area
   *
   * @example
   * '<session-send sessionid="session.id"></session-send>'
   */
  class SessionSendController {

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
      this.tags     = '';
    }

    /* exposed functions --------------------------------------------------- */
    send() {
      this.SessionService.send(this.sessionid, this.tags, this.include, this.cluster)
        .then((response) => {
          this.tags = '';
          this.$scope.$emit('close:form:container', {
            message: response.data.text
          });
        })
        .catch((error) => {
          this.error = error;
        });
    }

    cancel() { // close the form container (in session.detail.component)
      this.$scope.$emit('close:form:container');
    }

  }

  SessionSendController.$inject = ['$scope', 'SessionService'];

  /**
   * Send Session Directive
   * Displays send session area
   */
  angular.module('moloch')
    .component('sessionSend', {
      template  : require('html!../templates/session.send.html'),
      controller: SessionSendController,
      bindings  : { sessionid : '<', cluster : '<' }
    });

})();
