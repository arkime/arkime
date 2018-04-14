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
      this.$anchorScroll.yOffset = 60;
    }

    /* Callback when component is mounted and ready */
    $onInit() {
      this.FieldService.get(true)
        .then((response)  => { this.fields = HelpController.fixFields(response); })
        .catch((error)    => { this.error = error; });

      this.info = {
        ip          : { operator: '==, !=', type: 'ip' },
        lotermfield : { operator: '==, !=', type: 'lower case string' },
        termfield   : { operator: '==, !=', type: 'mixed case string' },
        uptermfield : { operator: '==, !=', type: 'upper case string' },
        integer     : { operator: '<, <=, ==, >=, >, !=', type: 'integer' },
        seconds     : { operator: '<, <=, ==, >=, >, !=', type: 'date time' }
      };
    }

    static fixFields(fields) {
      fields.forEach(function(item) {
        if (item.regex) {item.dbField = '';}
        else if (item.rawField) {item.dbField = item.dbField + ', ' + item.rawField;}
        else if (item.dbField === 'ipall') {item.dbField = '';}
      });
      return fields;
    }

  }

  HelpController.$inject = ['$anchorScroll','FieldService'];

  /**
   * Help Directive
   * Displays the moloch help page
   */
  angular.module('moloch')
     .component('molochHelp', {
       template  : require('./help.html'),
       controller: HelpController
     });

})();
