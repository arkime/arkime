(function() {

  'use strict';

  require('./app.css');
  require('angular-route');
  require('angular-filter');
  require('angular-animate');
  require('angular-resource');
  require('angular-sanitize');
  require('angular-file-upload');
  require('angular-ui-bootstrap');
  require('angular-bind-notifier');
  require('angular-bootstrap-colorpicker');
  require('ngdraggable');
  require('bootstrap/js/tooltip');
  require('bootstrap/js/dropdown');
  require('../public/flot-0.7/jquery.flot.js');
  require('../public/flot-0.7/jquery.flot.selection.js');
  require('../public/flot-0.7/jquery.flot.navigate.js');
  require('../public/flot-0.7/jquery.flot.resize.js');
  require('../public/flot-0.7/jquery.flot.stack.js');
  require('../public/jquery-jvectormap-1.2.2.min.js');
  require('../public/jquery-jvectormap-world-en.js');
  require('../public/d3.min.js');

  /**
   * Moloch Angular Application Definition
   */
  angular.module('moloch', [
    // angular dependencies
    'ngResource', 'ngRoute', 'ui.bootstrap', 'ngAnimate', 'colorpicker.module',
    'angular.filter', 'ngDraggable', 'ngSanitize', 'angular.bind.notifier',
    'angularFileUpload',

    // custom directives
    'directives.navbar', 'directives.footer',
    'directives.loading', 'directives.error',
    'directives.pagination', 'directives.search',
    'directives.toast',

    // utilities
    'moloch.util', 'moloch.config',

    // moloch ui constants
    'moloch.Constants'
  ])

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

  .config(['$routeProvider','$locationProvider','$httpProvider','$compileProvider','Constants',
    function($routeProvider, $locationProvider, $httpProvider, $compileProvider, Constants) {
      $routeProvider
        .when('/sessions', {
          title         : 'Sessions',
          template      : '<session></session>',
          reloadOnSearch: false
        })
        .when('/help', {
          title         : 'Help',
          template      : '<moloch-help></moloch-help>',
          reloadOnSearch: false
        })
        .when('/settings', {
          title         : 'Settings',
          template      : '<moloch-settings></moloch-settings>',
          reloadOnSearch: false
        })
        .when('/files', {
          title         : 'Files',
          template      : '<moloch-files></moloch-files>',
          reloadOnSearch: false
        })
        .when('/users', {
          title         : 'Users',
          template      : '<moloch-users></moloch-users>',
          reloadOnSearch: false
        })
        .when('/stats', {
          title         : 'Stats',
          template      : '<moloch-stats></moloch-stats>',
          reloadOnSearch: false
        })
        .when('/spiview', {
          title         : 'SPI View',
          template      : '<moloch-spiview></moloch-spiview>',
          reloadOnSearch: false
        })
        .when('/connections', {
          title         : 'Connections',
          template      : '<moloch-connections></moloch-connections>',
          reloadOnSearch: false
        })
        .when('/spigraph', {
          title         : 'SPIGraph',
          template      : '<moloch-spigraph></moloch-spigraph>',
          reloadOnSearch: false
        })
        .when('/upload', {
          title    : 'Upload',
          template : '<moloch-upload></moloch-upload>',
          reloadOnSearch: false
        })
        .otherwise({
          title    : 'Not Found',
          template : '<moloch-404></moloch-404>'
        });

      $locationProvider.html5Mode(true); // activate HTML5 Mode

      if (Constants) {
        if (!Constants.devMode) { $compileProvider.debugInfoEnabled(false); }
        else { console.log('%c Moloch Develoment Mode (>^_^)> ','background:#cc0863;color:white'); }
      }

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
