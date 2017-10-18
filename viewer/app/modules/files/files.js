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
    constructor($scope, FilesService, UserService) {
      this.$scope         = $scope;
      this.FilesService   = FilesService;
      this.UserService    = UserService;
    }

    /* Callback when component is mounted and ready */
    $onInit() {
      this.sortField    = 'num';
      this.sortReverse  = false;
      this.query        = {length: 50, start: 0};
      this.currentPage  = 1;

      this.$scope.$on('change:pagination', (event, args) => {
        // pagination affects length, currentPage, and start
        this.query.length = args.length;
        this.query.start  = args.start;
        this.currentPage  = args.currentPage;

        this.loadData();
      });

      this.UserService.getSettings()
        .then((response) => {this.settings = response; })
        .catch((error)   => {this.settings = {timezone: "local"}; });

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
      this.FilesService.get({filter: this.searchFiles, sortField: this.sortField, desc: this.sortReverse, start: this.query.start, length:this.query.length})
        .then((response)  => { this.files = response; })
        .catch((error)    => { this.error = error; });
    }

  }

  FilesController.$inject = ['$scope', 'FilesService', 'UserService'];

  /**
   * Moloch Files Directive
   * Displays pcap files
   */
  angular.module('moloch')
     .component('molochFiles', {
       template  : require('./files.html'),
       controller: FilesController
     });

})();
