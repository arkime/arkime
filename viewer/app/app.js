(function() {

  'use strict';


  require('./app.scss');
  require('datatables');
  require('angular-resource');
  require('angular-route');
  require('angular-animate');
  require('angular-ui-bootstrap');
  require('angular-datatables/dist/angular-datatables');
  require('angular-datatables/dist/plugins/bootstrap/angular-datatables.bootstrap');
  require('angular-datatables/dist/plugins/colreorder/angular-datatables.colreorder');
  require('datatables.net-colreorder/js/dataTables.colReorder');
  require('../public/jquery-jvectormap-1.2.2.min.js');
  require('../public/jquery-jvectormap-world-en.js');


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
    'directives.pagination', 'directives.search',

    // utilities
    'moloch.util'
  ])

  .constant('molochVersion', require('../version'))

  .config(['$routeProvider', '$locationProvider',
      function($routeProvider, $locationProvider) {

        $routeProvider
          .when('/session', {
            title   : 'Sessions',
            template: '<session></session>',
            // don't automatically reload when route parameters change
            reloadOnSearch: false
          })
          // default route is the sessions page
          .otherwise({ redirectTo: '/session' });

      }
    ]
  )

  .run(['$rootScope', function($rootScope) {
    $rootScope.$on('$routeChangeSuccess', function (event, current, previous) {
        $rootScope.title = current.$$route.title || 'Moloch';
    });
  }]);


  require('./modules');
  require('../components');

})();
