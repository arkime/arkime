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
     * @param molochVersion The installed version of moloch
     *
     * @ngInject
     */
    constructor(molochVersion) {
      this.molochVersion = molochVersion.version;
    }

  }

  FooterController.$inject = ['molochVersion'];

  /**
   * Footer Directive
   * Displays the footer
   */
  angular.module('directives.footer', [])
    .component('footer', {
      template  : require('html!./footer.html'),
      controller: FooterController
    });

})();
