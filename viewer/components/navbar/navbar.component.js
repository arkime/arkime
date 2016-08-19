'use strict';

class NavbarComponent {

  constructor($location) {
    'ngInject';

    this.$location = $location;
  }

  $onInit() {
    this.menu = {
      session: { title: 'Sessions', link: '/app#/session' }
    }
  }

  isActive(route) {
    return route === '/app#' + this.$location.path();
  }

}


angular.module('directives.navbar', [])
  .component('navbar', {
    templateUrl : '/components/navbar/navbar.html',
    controller  : NavbarComponent
  });
