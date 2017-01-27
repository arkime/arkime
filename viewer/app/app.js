(function() {

  'use strict';


  require('./app.scss');
  require('angular-route');
  require('angular-filter');
  require('angular-animate');
  require('angular-resource');
  require('angular-sanitize');
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
    'angular.filter', 'ngDraggable', 'ngSanitize',

    // custom directives
    'directives.navbar', 'directives.footer',
    'directives.loading', 'directives.error',
    'directives.pagination', 'directives.search',
    'directives.toast',

    // utilities
    'moloch.util', 'moloch.config'
  ])

  .constant('molochVersion', require('../version'))

  // watch for rejection status -1 to let the user know the server is down
  .factory('myHttpInterceptor', ['$q', function($q) {
    return {
      'responseError': function(rejection) {
        if (rejection.status === -1) {
          rejection = 'Cannot connect to server: request timed out or canceled.';
        }
        return $q.reject(rejection);
      }
    };
  }])

  .config(['$routeProvider', '$locationProvider', '$httpProvider',
    function($routeProvider, $locationProvider, $httpProvider) {
      $routeProvider
        .when('/app', {
          title         : 'Sessions',
          template      : '<session></session>',
          reloadOnSearch: false
        })
        .when('/help', {
          title    : 'Help',
          template : '<moloch-help></moloch-help>'
        })
        .when('/settings', {
          title    : 'Settings',
          template : '<moloch-settings></moloch-settings>'
        })
        // default route is the sessions page
        .otherwise({ redirectTo: '/app' });

      $locationProvider.html5Mode(true); // activate HTML5 Mode

      $httpProvider.interceptors.push('myHttpInterceptor');

      $httpProvider.defaults.withCredentials  = true;
      $httpProvider.defaults.xsrfCookieName   = 'MOLOCH-COOKIE';
      $httpProvider.defaults.xsrfHeaderName   = 'X-MOLOCH-COOKIE';
    }]
  )

  .run(['$rootScope', 'ConfigService',
    function($rootScope, ConfigService) {

    $rootScope.$on('issue:search', (event, args) => {
      // update title with expression
      ConfigService.setTitle('', args.expression, args.view);
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
