(function() {

  'use strict';

  /**
   * @class NavbarController
   * @classdesc Interacts with the navbar
   * @example
   * '<navbar></navbar>'
   */
  class NavbarController {

    /**
     * Initialize global variables for this controller
     * @param $location Exposes browser address bar URL
     *                  (based on the window.location)
     * @param molochVersion The installed version of moloch
     *
     * @ngInject
     */
    constructor($location, molochVersion) {
      this.$location      = $location;
      this.molochVersion  = molochVersion.version;
    }

    /* Callback when component is mounted and ready */
    $onInit() {
      this.menu = {
        session : { title: 'Sessions', link: '/app#/session' }
      };
    }


    /* exposed functions --------------------------------------------------- */
    /**
     * Determines the active nav item based on the page route
     * @param {string} route The route of the nav item
     */
    isActive(route) {
      return route === '/app#' + this.$location.path();
    }

  }

  NavbarController.$inject = ['$location','molochVersion'];

  /**
   * Navbar Directive
   * Displays the navbar
   */
  angular.module('directives.navbar', [])
    .component('navbar', {
      template  : require('html!./navbar.html'),
      controller: NavbarController
    });

})();
