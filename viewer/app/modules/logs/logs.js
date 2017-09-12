(function() {

  'use strict';

  /**
   * @class LogsController
   * @classdesc Interacts with moloch logs page
   * @example
   * '<moloch-logs></moloch-logs>'
   */
  class LogsController {

    /**
     * Initialize global variables for this controller
     *
     * @ngInject
     */
    constructor(LogService) {
      this.LogService = LogService;
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
        filter    : this.searchLogs,
        sortField : this.sortField,
        desc      : this.sortReverse,
        start     : this.query.start,
        length    : this.query.length
      };

      this.LogService.get(params)
         .then((response) => {
           this.loading = false;
           this.logs    = response;
           console.log(response);
         })
         .catch((error) => {
           this.loading  = false;
           this.error    = error.text;
         });
    }

  }

  LogsController.$inject = ['LogService'];

  /**
   * Logs Directive
   * Displays the moloch logs page
   */
  angular.module('moloch')
     .component('molochLogs', {
       template  : require('html!./logs.html'),
       controller: LogsController
     });

})();
