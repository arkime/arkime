(function() {

  'use strict';

  /**
   * @class HistoryController
   * @classdesc Interacts with moloch history page
   * @example
   * '<moloch-history></moloch-history>'
   */
  class HistoryController {

    /**
     * Initialize global variables for this controller
     *
     * @ngInject
     */
    constructor(UserService, HistoryService) {
      this.UserService    = UserService;
      this.HistoryService = HistoryService;
    }

    /* Callback when component is mounted and ready */
    $onInit() {
      this.sortField    = 'timestamp';
      this.sortReverse  = true;
      this.currentPage  = 1;
      this.query        = { length: 50, start: 0 };

      this.columns = [
        { name: 'Time', sort: 'timestamp', nowrap:true, help: 'The date and time of the request' },
        { name: 'User ID', sort: 'userId', nowrap: true, help: 'The id of the user that initiated the request' },
        { name: 'API', sort: 'api', nowrap: true, help: 'The API endpoint of the request' },
        { name: 'Query', sort: 'query', nowrap: true, help: 'The query issued with the request' },
        { name: 'View', sort: 'view', nowrap: true, help: 'The view expression applied to the query' }
      ];

      this.UserService.getSettings()
         .then((response) => { this.settings = response; })
         .catch((error)   => { this.settings = {timezone: "local"}; });

      this.loadData();
    }

    columnClick(name) {
      this.sortField    = name;
      this.sortReverse  = !this.sortReverse;
      this.loadData();
    }

    loadData() {
      this.loading = true;

      let params = {
        filter    : this.searchHistory,
        sortField : this.sortField,
        desc      : this.sortReverse,
        start     : this.query.start,
        length    : this.query.length
      };

      this.HistoryService.get(params)
         .then((response) => {
           this.loading = false;
           this.history = response;
         })
         .catch((error) => {
           this.loading  = false;
           this.error    = error.text;
         });
    }

  }

  HistoryController.$inject = ['UserService','HistoryService'];

  /**
   * History Directive
   * Displays the moloch history page
   */
  angular.module('moloch')
     .component('molochHistory', {
       template  : require('html!./history.html'),
       controller: HistoryController
     });

})();
