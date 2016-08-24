(function() {

  'use strict';

  /**
   * Moloch Angular Application Definiton
   */
  angular.module('moloch', [
    // angular dependencies
    'ngResource', 'ngRoute', 'ui.bootstrap', 'ngAnimate',

    // datatables dependencies
    'datatables', 'datatables.bootstrap', 'datatables.colreorder',

    // custom directives
    'directives.navbar', 'directives.footer',
    'directives.loading', 'directives.error',
    'moloch.util'
  ])

  .config(['$routeProvider', '$locationProvider',
      function($routeProvider, $locationProvider) {

        $routeProvider
          .when('/session', {
            template: '<session></session>'
          })
          // default route is the sessions page
          .otherwise({ redirectTo: '/session' });

      }
    ]
  );

})();
