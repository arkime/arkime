(function() {

  'use strict';


  require('./app.scss');
  require('angular-route');
  require('angular-filter');
  require('angular-animate');
  require('angular-resource');
  require('angular-ui-bootstrap');
  require('ngdraggable');
  require('bootstrap/js/tooltip');
  require('bootstrap/js/dropdown');
  require('../public/flot-0.7/jquery.flot.js');
  require('../public/flot-0.7/jquery.flot.selection.js');
  require('../public/flot-0.7/jquery.flot.navigate.js');
  require('../public/flot-0.7/jquery.flot.resize.js');
  require('../public/jquery-jvectormap-1.2.2.min.js');
  require('../public/jquery-jvectormap-world-en.js');


  /**
   * Moloch Angular Application Definition
   */
  angular.module('moloch', [
    // angular dependencies
    'ngResource', 'ngRoute', 'ui.bootstrap', 'ngAnimate',
    'angular.filter', 'ngDraggable',

    // custom directives
    'directives.navbar', 'directives.footer',
    'directives.loading', 'directives.error',
    'directives.pagination', 'directives.search',

    // utilities
    'moloch.util', 'moloch.config'
  ])

  .constant('molochVersion', require('../version'))

  .config(['$routeProvider',
      function($routeProvider) {

        $routeProvider
          .when('/session', {
            title         : 'Sessions',
            template      : '<session></session>',
            reloadOnSearch: false
          })
          // default route is the sessions page
          .otherwise({ redirectTo: '/session' });

      }
    ]
  )

  .run(['$rootScope', 'ConfigService',
    function($rootScope, ConfigService) {

    $rootScope.$on('issue:search', (event, args) => {
      // update title with expression
      ConfigService.setTitle('', args.expression, '');
    });

    $rootScope.$on('$routeChangeSuccess', function (event, current) {
      if (current && current.$$route && current.$$route.title) {
        // update title with page
        ConfigService.setTitle(current.$$route.title, '', '');
      } else { // fallback to 'Moloch'
        $rootScope.title = 'Moloch';
      }
    });

  }]);


  require('./modules');
  require('../components');

})();
