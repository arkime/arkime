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
     * @param $filter       Filters format the value of an expression
     * @param $window       Angular's reference to the browser's window object
     * @param $location     Exposes browser address bar URL
     *                      (based on the window.location)
     * @param $rootScope    Angular application main scope
     * @param $routeParams  Retrieves the current set of route parameters
     * @param Constants     Moloch UI global constants
     *
     * @ngInject
     */
    constructor($filter, $window, $location, $rootScope, $routeParams,
                Constants) {
      this.$filter        = $filter;
      this.$window        = $window;
      this.$location      = $location;
      this.$rootScope     = $rootScope;
      this.$routeParams   = $routeParams;
      this.molochVersion  = Constants.version;
      this.demoMode       = Constants.demoMode;
    }

    /* Callback when component is mounted and ready */
    $onInit() {
      this.menu = {
        sessions    : { title: 'Sessions',    link: 'sessions' },
        spiview     : { title: 'SPI View',    link: 'spiview' },
        spigraph    : { title: 'SPI Graph',   link: 'spigraph' },
        connections : { title: 'Connections', link: 'connections' },
        files       : { title: 'Files',       link: 'files' },
        stats       : { title: 'Stats',       link: 'stats' },
        upload      : { title: 'Upload',      link: 'upload', permission: 'canUpload' }
      };

      if (!this.demoMode) {
        this.menu.history   = { title: 'History',   link: 'history' };
        this.menu.settings  = { title: 'Settings',  link: 'settings' };
        this.menu.users     = { title: 'Users',     link: 'users', permission: 'createEnabled' };
      }
    }


    /* exposed functions --------------------------------------------------- */
    /**
     * Determines whether a tab is active based on it's link
     * @param {string} link The link of the nav item
     */
    isActive(link) {
      return link === this.$location.path().split('/')[1];
    }

    /**
     * Creates an href for the tab buttons
     * @param {string} link The link of the nav item
     * @returns {string} href
     */
    createHref(link) {
      return this.$filter('buildUrl')(link);
    }

    /**
     * Redirects to the desired link preserving query parameters
     * @param {string} link   The link to redirect to
     * @param {object} event  The click event that triggered this function
     */
    navTabClick(link, $event) {
      $event.preventDefault();
      $event.stopPropagation();

      if (link === 'help') { // help link is special!
        // must set the section of the help page to navigate to
        this.$location.hash(this.$location.path().split('/')[1]);
        this.$location.path(link);
      } else {
        if (this.$rootScope.expression !== this.$routeParams.expression) {
          // if the expression input doesn't match the expression url parameter,
          // the url needs to be constructed so that there is only one entry
          // in the browser history
          let newUrl = this.$filter('buildUrl')(link);
          this.$window.location.href = newUrl;
        } else { // the expression hasn't changed, so just go to the link
          this.$location.path(link);
        }
      }
    }

  }

  NavbarController.$inject = ['$filter','$window','$location','$rootScope',
    '$routeParams','Constants'];

  /**
   * Navbar Directive
   * Displays the navbar
   */
  angular.module('directives.navbar', [])
    .component('navbar', {
      template  : require('./navbar.html'),
      controller: NavbarController
    });

})();
