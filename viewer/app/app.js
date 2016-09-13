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

  .config(['$routeProvider', '$locationProvider',
      function($routeProvider, $locationProvider) {

        $routeProvider
          .when('/session', {
            template: '<session></session>',
            // don't automatically reload when route parameters change
            reloadOnSearch: false
          })
          // default route is the sessions page
          .otherwise({ redirectTo: '/session' });

      }
    ]
  );


  require('./modules');
  require('../components');

})();
