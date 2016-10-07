(function() {

  'use strict';

  /**
   * @class SessionDetailTagController
   * @classdesc Interacts with add tag area
   *
   * @example
   * '<add-tag sessionid="session.id"></add-tag>'
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
      this.$scope = $scope;
      this.SessionService = SessionService;
    }

    $onInit() {
      this.addingTag  = false;
      this.include    = 'no';
      this.tags       = '';
    }

    /* exposed functions --------------------------------------------------- */
    addTags() {
      if (this.tags === '') {
        this.error = 'No tag(s) specified.';
        return;
      }

      this.SessionService.addTags(this.sessionid, this.tags, this.include)
        .then((response) => {
          this.addingTag  = false;
          this.tags       = '';
          // notify parent that tags were added (namely session.detail.component)
          this.$scope.$emit('added:tags', { id:this.sessionid });
        })
        .catch((error) => {
          this.error = error;
        });
    }

  }

  SessionDetailTagController.$inject = ['$scope', 'SessionService'];

  /**
   * Add Tag Directive
   * Displays add tag area
   */
  angular.module('moloch')
    .component('addTag', {
      template  : require('html!../templates/session.detail.tag.html'),
      controller: SessionDetailTagController,
      bindings  : { sessionid : '<' }
    });

})();
