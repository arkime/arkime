(function() {

  'use strict';

  /**
   * No Results Directive
   * Displays a message indicating a lack of search results
   *
   * @example
   * '<noresults records-total="{{::$ctrl.recordsTotal}}"
   *    view="{{::$ctrl.view}}"></noresults>'
   */
  angular.module('directives.noresults', [])
    .component('noresults', {
      template  : require('./noresults.html'),
      bindings  : {
        recordsTotal  : '@',
        view          : '@'
      }
    });

})();
