(function() {

  'use strict';

  /**
   * @class FilesController
   * @classdesc Interacts with moloch files page
   * @example
   * '<moloch-fields></moloch-fields>'
   */
  class FilesController {

    /**
     * Initialize global variables for this controller
     * @param FilesService  Transacts files with the server
     *
     * @ngInject
     */
    constructor(FilesService) {
      this.FilesService   = FilesService;
    }

    /* Callback when component is mounted and ready */
    $onInit() {
      this.sortField    = 'num';
      this.sortReverse  = false;

      this.loadData();

      this.columns = [
        { name: 'File Number', sort: 'num' },
        { name: 'Node', sort: 'node' },
        { name: 'Name', sort: 'name' },
        { name: 'Locked', sort: 'locked' },
        { name: 'First Date', sort: 'first' },
        { name: 'File Size', sort: 'filesize' },
      ];
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

  FilesController.$inject = ['FilesService'];

  /**
   * Moloch Files Directive
   * Displays pcap files
   */
  angular.module('moloch')
     .component('molochFiles', {
       template  : require('html!./files.html'),
       controller: FilesController
     });

})();
