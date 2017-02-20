(function() {

  'use strict';

  /**
   * @class ConnectionsLinkPopupController
   * @classdesc Shows the link popup
   */
  class ConnectionsLinkPopupController {

    /**
     * Initialize global variables for this controller
     *
     * @ngInject
     */
    constructor($rootScope) {
      this.$rootScope = $rootScope;
    }

    $onInit() {
    }


    addExpression(op) {
      let fullExpression = `(${this.link.srcExp} == ${this.link.source.id} && ${this.link.dstExp} == ${this.link.target.id})`;

      this.$rootScope.$broadcast('add:to:typeahead', { expression: fullExpression, op: op });
    }

    hideLink() {
      var self = this;
      $('#networkLabel').hide();
      self.svg.selectAll(".link")
         .filter(function(d, i) {
           return d.source.id === self.link.source.id && d.target.id === self.link.target.id;
         })
         .remove();
    }

  }

  ConnectionsLinkPopupController.$inject = ["$rootScope"];

  angular.module('moloch')
    .component('connectionsLinkPopup', {
      template  : require('html!./connections.linkpopup.html'),
      controller: ConnectionsLinkPopupController,
      bindings  : {
        link: '<',
        svg: '<'
      }
    });

})();
