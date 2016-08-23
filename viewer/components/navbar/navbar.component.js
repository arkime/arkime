(function() {

  'use strict';

  /**
   * @class NavbarController
   * @classdesc Interacts with the navbar
   */
  class NavbarController {

    /**
     * Initialize global variables for this controller
     * @param $location Exposes browser address bar URL
     *                  (based on the window.location)
     *
     * @ngInject
     */
    constructor($location) {
      this.$location = $location;
    }

    /* Callback when component is mounted and ready */
    $onInit() {
      this.menu = {
        session: { title: 'Sessions', link: '/app#/session' }
      }
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

  NavbarController.$inject = ['$location'];

  /**
   * Navbar Directive
   * Displays a navbar
   * @example
   * '<navbar></navbar>'
   */
  angular.module('directives.navbar', [])
    .component('navbar', {
      templateUrl : '/components/navbar/navbar.html',
      controller  : NavbarController
    });

})();
