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
     * @param Constants Moloch UI global constants
     *
     * @ngInject
     */
    constructor($location, Constants) {
      this.$location      = $location;
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
        this.menu.settings  = { title: 'Settings', link: 'settings' };
        this.menu.users     = { title: 'Users', link: 'users', permission: 'createEnabled' };
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
     * Redirects to the desired link preserving query parameters
     * @param {string} link The link to redirect to
     */
    navTabClick(link) {
      if (link === 'help') {
        // going to help page, so set section of help to navigate to
        this.$location.hash(this.$location.path().split('/')[1]);
      }

      this.$location.path(link);
    }

  }

  NavbarController.$inject = ['$location','Constants'];

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
