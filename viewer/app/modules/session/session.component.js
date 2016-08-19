'use strict';


class SessionComponent {

  /* @ngInject */
  constructor($http) {
    this.$http = $http;
  }

  $onInit() {
    console.log('Session Component Initialized!');
  }

}


angular.module('moloch')
  .component('session', {
    templateUrl : '/modules/session/session.html',
    controller  : SessionComponent
  });
