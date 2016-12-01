(function() {

  'use strict';

  /**
   * @class SessionDetailTagController
   * @classdesc Interacts with add tag area
   *
   * @example
   * '<session-tag sessionid="session.id"></session-tag>'
   */
  class SessionDetailTagController {

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
    addTags() {
      if (this.tags === '') {
        this.error = 'No tag(s) specified.';
        return;
      }

      this.SessionService.addTags(this.sessionid, this.tags, this.include)
        .then((response) => {
          this.tags     = '';
          // notify parent that tags were added (namely session.detail.component)
          this.$scope.$emit('update:tags', { id:this.sessionid });
        })
        .catch((error) => {
          this.error = error;
        });
    }

    removeTags() {
      if (this.tags === '') {
        this.error = 'No tag(s) specified.';
        return;
      }

      this.SessionService.removeTags(this.sessionid, this.tags, this.include)
        .then((response) => {
          this.tags     = '';
          // notify parent that tags were added (namely session.detail.component)
          this.$scope.$emit('update:tags', { id:this.sessionid });
        })
        .catch((error) => {
          this.error = error;
        });
    }

    cancel() {
      // close the form container (in session.detail.component)
      this.$scope.$emit('close:form:container');
    }

  }

  SessionDetailTagController.$inject = ['$scope', 'SessionService'];

  /**
   * Add Tag Directive
   * Displays add tag area
   */
  angular.module('moloch')
    .component('sessionTag', {
      template  : require('html!../templates/session.tag.html'),
      controller: SessionDetailTagController,
      bindings  : { sessionid : '<', add: '<' }
    });

})();
