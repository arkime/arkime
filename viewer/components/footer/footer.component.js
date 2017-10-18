(function() {

  'use strict';

  /**
   * @class FooterController
   * @classdesc Interacts with the footer
   * @example
   * '<footer class="footer"></footer>'
   */
  class FooterController {

    /**
     * Initialize global variables for this controller
     * @param Constants Moloch UI global constants
     *
     * @ngInject
     */
    constructor(Constants) {
      this.molochVersion = Constants.version;
    }

  }

  FooterController.$inject = ['Constants'];

  /**
   * Footer Directive
   * Displays the footer
   */
  angular.module('directives.footer', [])
    .component('footer', {
      template  : require('./footer.html'),
      controller: FooterController
    });

})();
