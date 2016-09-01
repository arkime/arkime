(function() {

  'use strict';

  /**
   * Footer Directive
   * Displays a footer
   *
   * @example
   * '<footer class="footer"></footer>'
   */
  angular.module('directives.footer', [])
    .component('footer', {
      template : require('html!./footer.html')
    });

})();
