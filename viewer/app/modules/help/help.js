(function() {

  'use strict';

  /**
   * @class HelpController
   * @classdesc Interacts with moloch help page
   * @example
   * '<moloch-help></moloch-help>'
   */
  class HelpController {

    /**
     * Initialize global variables for this controller
     * @param $anchorScroll Scrolls to the element related to given hash
     * @param FieldService  Transacts session fields with the server
     *
     * @ngInject
     */
    constructor($anchorScroll, FieldService) {
      this.$anchorScroll  = $anchorScroll;
      this.FieldService   = FieldService;

      // offset anchor scroll position to account for navbars
      this.$anchorScroll.yOffset = 90;
    }

    /* Callback when component is mounted and ready */
    $onInit() {
      this.FieldService.get(true)
        .then((response)  => { this.fields = response; })
        .catch((error)    => { this.error = error; });

      this.info = {
        ip          : { operator: '==, !=', type: 'ip' },
        lotextfield : { operator: '==, !=', type: 'lower case tokenized string' },
        textfield   : { operator: '==, !=', type: 'tokenized string' },
        lotermfield : { operator: '==, !=', type: 'lower case non tokenized string' },
        termfield   : { operator: '==, !=', type: 'non tokenized string' },
        uptermfield : { operator: '==, !=', type: 'upper case non tokenized string' },
        integer     : { operator: '<, <=, ==, >=, >, !=', type: 'integer' },
        seconds     : { operator: '<, <=, ==, >=, >, !=', type: 'date time' }
      };
    }

  }

  HelpController.$inject = ['$anchorScroll','FieldService'];

  /**
   * ES Health Directive
   * Displays elasticsearch health status
   */
  angular.module('moloch')
     .component('molochHelp', {
       template  : require('html!./help.html'),
       controller: HelpController
     });

})();
