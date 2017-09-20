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
     * @param $scope          Angular application model object
     * @param $filter         Formats data displayed to the user
     * @param $routeParams    Retrieve the current set of route parameters
     * @param UserService     Transacts users and user data with the server
     * @param HistoryService  Transacts logs with the server
     *
     * @ngInject
     */
    constructor($scope, $filter, $routeParams, UserService, HistoryService) {
      this.$scope         = $scope;
      this.$filter        = $filter;
      this.$routeParams   = $routeParams;
      this.UserService    = UserService;
      this.HistoryService = HistoryService;
    }

    /* Callback when component is mounted and ready */
    $onInit() {
      this.sortField    = 'timestamp';
      this.sortReverse  = true;
      this.currentPage  = 1;
      this.query        = { length:50, start:0 };
      this.filters      = {}; // filters for column values

      if (this.$routeParams.length) {
        this.query.length = parseInt(this.$routeParams.length);
      }

      this.columns = [
        { name:'Time', sort:'timestamp', nowrap:true, width:12, help:'The time of the request' },
        { name:'Time Range', sort:'range', nowrap:true, width:10, help:'The time range of the request'},
        { name:'User ID', sort:'userId', nowrap:true, width:8, filter:true, permission:'createEnabled', help:'The id of the user that initiated the request' },
        { name:'API', sort:'pathname', nowrap:true, width:15, filter:true, help:'The API endpoint of the request' },
        { name:'Expression', sort:'expression', nowrap:true, width:30, help:'The query expression issued with the request' },
        { name:'View', sort:'view.name', nowrap:true, width:25, help:'The view expression applied to the request' }
      ];

      this.UserService.getSettings()
         .then((response) => { this.settings = response; })
         .catch((error)   => { this.settings = { timezone:'local'}; });

      this.loadData();

      /* LISTEN! */
      this.$scope.$on('change:pagination', (event, args) => {
        // pagination affects length, currentPage, and start
        this.query.length = args.length;
        this.query.start  = args.start;
        this.currentPage  = args.currentPage;

        this.loadData();
      });
    }

    loadData() {
      this.loading = true;

      let params = {
        searchTerm: this.searchHistory,
        sortField : this.sortField,
        desc      : this.sortReverse,
        start     : this.query.start,
        length    : this.query.length
      };

      if (this.filters && Object.keys(this.filters).length) {
        for (let key in this.filters) {
          if (this.filters.hasOwnProperty(key)) {
            params[key] = this.filters[key];
          }
        }
      }

      this.HistoryService.get(params)
         .then((response) => {
           this.loading = false;
           this.history = response;
         })
         .catch((error) => {
           this.loading  = false;
           this.error    = error.data.text || 'History retrieval error';
         });
    }

    /* page functions ------------------------------------------------------ */
    columnClick(name) {
      this.sortField    = name;
      this.sortReverse  = !this.sortReverse;
      this.loadData();
    }

    filterTable(columnId, value) {
      if (!value) { // remove empty filter
        this.filters[columnId] = null;
        delete this.filters[columnId];
      }

      this.loadData();
    }

    toggleLogDetail(log) {
      log.expanded = !log.expanded;

      if (log.query) {
        log.queryObj = this.$filter('parseParamString')(log.query);
      }
    }

  }

  HistoryController.$inject = ['$scope','$filter','$routeParams',
    'UserService','HistoryService'];

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
