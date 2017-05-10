(function() {

  'use strict';

  /**
   * Not Found Directive
   * Displays the 404 page
   */
  angular.module('moloch')
     .component('moloch404', {
       template: require('html!./404.html')
     });

})();
