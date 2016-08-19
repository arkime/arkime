'use strict';


angular.module('moloch', [
  'ngResource', 'ngRoute',
  'directives.navbar', 'directives.footer'
])

.config(['$routeProvider', '$locationProvider',
    function($routeProvider, $locationProvider) {

      $routeProvider
        .when('/session', {
          template: '<session></session>'
        })
        // default route is session page
        .otherwise({ redirectTo: '/session' });

    }
  ]
);
