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
    apply(addTags) {
      if (!this.tags || this.tags === '') {
        this.error = 'No tag(s) specified.';
        return;
      }

      let data = {
        tags    : this.tags,
        segments: this.include
      };

      if (!this.applyTo || this.applyTo === 'open') {
        data.ids    = SessionDetailTagController.getSessionIds(this.sessions);
      } else if (this.applyTo === 'visible') {
        data.start  = this.start || 0;
        data['0']   = this.numVisible;
      } else if (this.applyTo === 'matching') {
        data.start  = 0;
        data['0']   = this.numVisible;
        data.length = this.numMatching;
      }

      if (addTags) {
        this.addTags(data)
      } else {
        this.removeTags(data)
      }
    }

    addTags(data) {
      this.SessionService.addTags(data)
        .then((response) => {
          this.tags = '';
          SessionDetailTagController.closeForm(response, data, this.$scope);
        })
        .catch((error) => {
          this.error = error;
        });
    }

    removeTags(data) {
      this.SessionService.removeTags(data)
        .then((response) => {
          this.tags = '';
          SessionDetailTagController.closeForm(response, data, this.$scope);
        })
        .catch((error) => {
          this.error = error;
        });
    }

    cancel() {
      // close the form container
      this.$scope.$emit('close:form:container');
    }


    /* internal functions -------------------------------------------------- */
    static closeForm(response, data, scope) {
      // notify parent that tags were added
      // but only reload data if tags were added to only one
      let args = { ids: data.ids };
      if (response.data.text) { args.message = response.data.text; }
      if (data.ids && data.ids.length === 1) { args.reloadData = true; }

      scope.$emit('close:form:container', args);
    }

    static getSessionIds(sessions) {
      let ids = [];

      if (sessions) {
        for (let i = 0, len = sessions.length; i < len; ++i) {
          ids.push(sessions[i].id);
        }
      }

      return ids;
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
      bindings  : {
        sessions    : '<',
        add         : '<',
        applyTo     : '<',
        numMatching : '<',
        numVisible  : '<',
        start       : '<'
      }
    });

})();
