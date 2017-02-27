(function() {

  'use strict';

  /**
   * @class ConnectionsNodePopupController
   * @classdesc Connections node popup
   */
  class ConnectionsNodePopupController {

    /**
     * Initialize global variables for this controller
     * @param $rootScope Angular application main scope
     *
     * @ngInject
     */
    constructor($rootScope) {
      this.$rootScope = $rootScope;
    }

    closePopup() {
      $('#networkLabel').hide();
    }

    addExpression(op) {
      let fullExpression = `${this.node.exp} == ${this.node.id}`;

      this.$rootScope.$broadcast('add:to:typeahead', { expression: fullExpression, op: op });
    }

    hideNode() {
      let self = this;
      $('#networkLabel').hide();
      self.svg.select('#id'+self.node.id.replace(/[:\.]/g,'_')).remove();
      self.svg.selectAll('.link')
         .filter(function(d, i) {
           return d.source.id === self.node.id || d.target.id === self.node.id;
         })
         .remove();
    }

  }

  ConnectionsNodePopupController.$inject = ['$rootScope'];

  angular.module('moloch')
    .component('connectionsNodePopup', {
      template  : require('html!./connections.nodepopup.html'),
      controller: ConnectionsNodePopupController,
      bindings  : {
        node    : '<',
        svg     : '<'
      }
    });

})();
