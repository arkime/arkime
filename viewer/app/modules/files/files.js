(function() {

  'use strict';

  /**
   * @class FilesController
   * @classdesc Interacts with moloch help page
   * @example
   * '<moloch-help></moloch-help>'
   */
  class FilesController {

    /**
     * Initialize global variables for this controller
     * @param $anchorScroll Scrolls to the element related to given hash
     * @param FilesService  Transacts files with the server
     *
     * @ngInject
     */
    constructor($anchorScroll, FilesService) {
      this.$anchorScroll  = $anchorScroll;
      this.FilesService   = FilesService;
      this.sortField = 'num';
      this.sortReverse = false;

      // offset anchor scroll position to account for navbars
      this.$anchorScroll.yOffset = 90;
    }

    /* Callback when component is mounted and ready */
    $onInit() {
      this.loadData();
    }

    columnClick(name) {
      this.sortField=name; 
      this.sortReverse = !this.sortReverse;
      this.loadData();
    }

    loadData() {
      this.FilesService.get({filter: this.searchFiles, sortField: this.sortField, desc: this.sortReverse})
        .then((response)  => { this.files = response; })
        .catch((error)    => { this.error = error; });
    }

  }

  FilesController.$inject = ['$anchorScroll','FilesService'];

  /**
   * ES Health Directive
   * Displays elasticsearch health status
   */
  angular.module('moloch')
     .component('molochFiles', {
       template  : require('html!./files.html'),
       controller: FilesController
     });

})();
