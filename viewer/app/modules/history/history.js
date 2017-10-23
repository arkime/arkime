(function() {

  'use strict';

  let initialized;

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
     * @param HistoryService  Transacts history with the server
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
      this.colSpan      = 7;

      if (this.$routeParams.length) {
        this.query.length = parseInt(this.$routeParams.length);
      }

      this.columns = [
        { name:'Time', sort:'timestamp', nowrap:true, width:10, help:'The time of the request' },
        { name:'Time Range', sort:'range', nowrap:true, width:8, help:'The time range of the request' },
        { name:'User ID', sort:'userId', nowrap:true, width:11, filter:true, permission:'createEnabled', help:'The id of the user that initiated the request' },
        { name:'Query Time', sort:'queryTime', nowrap:true, width:8, help:'Execution time in MS' },
        { name:'API', sort:'api', nowrap:true, width:13, filter:true, help:'The API endpoint of the request' },
        { name:'Expression', sort:'expression', nowrap:true, width:27, exists:false, help:'The query expression issued with the request' },
        { name:'View', sort:'view.name', nowrap:true, width:23, exists:false, help:'The view expression applied to the request' }
      ];

      this.UserService.getSettings()
        .then((response) => {this.settings = response; })
        .catch((error)   => {this.settings = {timezone: 'local'}; });

      this.UserService.getCurrent()
        .then((response) => {
          if (response.createEnabled) { this.colSpan = 8; }
          this.filters.userId = this.$routeParams.userId || response.userId;
          initialized = true;
          this.loadData();
        })
        .catch(() => {
          initialized = true;
          this.loadData();
        });

      /* LISTEN! */
      this.$scope.$on('change:pagination', (event, args) => {
        // pagination affects length, currentPage, and start
        this.query.length = args.length;
        this.query.start  = args.start;
        this.currentPage  = args.currentPage;

        if (initialized) { this.loadData(); }
      });

      // update the temporal parameters
      this.$scope.$on('change:time:input', (event, args) => {
        if (args.date) {
          this.timeRange = args.date;
          this.startTime = null;
          this.stopTime  = null;
        } else if (args.startTime && args.stopTime) {
          this.timeRange = null;
          this.startTime = args.startTime;
          this.stopTime  = args.stopTime;
        }

        if (initialized) { this.loadData(); }
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

      if (this.timeRange) { params.date       = this.timeRange; }
      if (this.startTime) { params.startTime  = this.startTime; }
      if (this.stopTime)  { params.stopTime   = this.stopTime;  }

      let exists = [];
      for (let i = 0, len = this.columns.length; i < len; ++i) {
        let col = this.columns[i];
        if (col.exists) { exists.push(col.sort); }
      }
      if (exists.length) {
        params.exists = exists.join();
      }

      if (this.filters && Object.keys(this.filters).length) {
        for (let key in this.filters) {
          if (this.filters.hasOwnProperty(key)) {
            params[key] = this.filters[key];
          }
        }
      }

      this.HistoryService.get(params)
         .then((response) => {
           this.error   = false;
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

    deleteLog(log, index) {
      this.HistoryService.delete(log.id, log.index)
        .then((response) => {
          this.msg = response.text || 'Successfully deleted history item';
          this.msgType = 'success';
          this.history.data.splice(index, 1);
        })
        .catch((error) => {
          this.msg = error.data.text || 'Error deleting history item';
          this.msgType = 'danger';
        });
    }

    /* remove the message when user is done with it or duration ends */
    messageDone() {
      this.msg = null;
      this.msgType = null;
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
       template  : require('./history.html'),
       controller: HistoryController
     });

})();
