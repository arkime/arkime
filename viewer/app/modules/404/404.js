(function() {

  'use strict';

  /**
   * Not Found Directive
   * Displays the 404 page
   */
  angular.module('moloch')
     .component('moloch404', {
       template: require('./404.html')
     });

})();
