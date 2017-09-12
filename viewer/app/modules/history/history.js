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
    constructor(HistoryService) {
      this.HistoryService = HistoryService;
    }

    /* Callback when component is mounted and ready */
    $onInit() {
      this.sortField    = 'date';
      this.sortReverse  = false;
      this.currentPage  = 1;
      this.query        = { length: 50, start: 0 };

      this.columns = [
        { name: 'Time', sort: 'date', nowrap:true, help: 'The date and time of the logged action' },
        { name: 'User ID', sort: 'userId', nowrap: true, help: 'The id used for login' },
        { name: 'API', sort: 'api', nowrap: true, help: 'The API endpoint' },
        { name: 'Query', sort: 'query', nowrap: true, help: 'The query issued' },
        { name: 'View', sort: 'view', nowrap: true, help: 'The view expression applied to the query' }
      ];

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

  HistoryController.$inject = ['HistoryService'];

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
