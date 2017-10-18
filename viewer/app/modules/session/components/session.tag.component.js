(function() {

  'use strict';

  /**
   * @class SessionTagController
   * @classdesc Interacts with add/remove tag area
   *
   * @example
   * '<session-tag sessions="[session1...sessionN]"
   *    apply-to="'open' || 'visible' || 'matching'"
   *    num-visible="numVisibleSessions" start="startSessionIndex"
   *    num-matching="numQueryMatchingSessions"></session-tag>'
   */
  class SessionTagController {

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
      this.tags     = '';
      this.loading  = false;
    }


    /* exposed functions --------------------------------------------------- */
    /**
     * Triggered by add/remove tag button in form
     * @param {bool} addTags Whether to add tags or remove tags
     */
    apply(addTags) {
      if (!this.tags || this.tags === '') {
        this.error = 'No tag(s) specified.';
        return;
      }

      this.loading = true;

      let data = {
        tags        : this.tags,
        start       : this.start,
        applyTo     : this.applyTo,
        segments    : this.segments,
        sessions    : this.sessions,
        numVisible  : this.numVisible,
        numMatching : this.numMatching
      };

      if (addTags) { this.addTags(data); }
      else { this.removeTags(data); }
    }

    addTags(data) {
      this.SessionService.addTags(data)
        .then((response) => {
          this.tags     = '';
          this.loading  = false;
          SessionTagController.closeForm(response, data, this.$scope);
        })
        .catch((error) => {
          this.error    = error;
          this.loading  = false;
        });
    }

    removeTags(data) {
      this.SessionService.removeTags(data)
        .then((response) => {
          this.tags = '';
          this.tags     = '';
          this.loading  = false;
          SessionTagController.closeForm(response, data, this.$scope);
        })
        .catch((error) => {
          this.error    = error;
          this.loading  = false;
        });
    }

    cancel() { // close the form
      this.$scope.$emit('close:form:container');
    }


    /* internal functions -------------------------------------------------- */
    /**
     * Closes the add/remove tag form and notifies parent
     * @param {object} response Response data from the server
     * @param {object} data     Data sent to the server
     * @param {object} scope    Session Tag Controller's scope
     */
    static closeForm(response, data, scope) {
      let args = {};

      if (response.data.text) {
        args.message = response.data.text;
        args.success = response.data.success;
      }

      //  only reload data if tags were added to only one
      if (data.sessions && data.sessions.length === 1) {
        args.reloadData = true;
      }

      // notify parent to close form
      scope.$emit('close:form:container', args);
    }

  }

  SessionTagController.$inject = ['$scope', 'SessionService'];

  /**
   * Session Tag Directive
   * Displays add/remove tags form
   */
  angular.module('moloch')
    .component('sessionTag', {
      template  : require('../templates/session.tag.html'),
      controller: SessionTagController,
      bindings  : {
        add         : '<', // whether to add or remove tags
        start       : '<', // where to start the action
        applyTo     : '<', // what to apply the action to [open,visible,matching]
        sessions    : '<', // sessions to apply the action to
        numVisible  : '<', // number of visible sessions to apply action to
        numMatching : '<'  // number of matching sessions to apply action to
      }
    });

})();
