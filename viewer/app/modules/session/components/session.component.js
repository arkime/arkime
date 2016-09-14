(function() {

  'use strict';

  /**
   * @class SessionController
   * @classdesc Interacts with session list
   */
  class SessionController {

    /* setup --------------------------------------------------------------- */
    /**
     * Initialize global variables for this controller
     * @param $scope              Angular application model object
     * @param $routeParams        Retrieve the current set of route parameters
     * @param SessionService      Transacts sessions with the server
     * @param DTOptionsBuilder    DataTables options builder
     * @param DTColumnDefBuilder  DataTables column builder
     *
     * @ngInject
     */
    constructor($scope, $routeParams, SessionService,
      DTOptionsBuilder, DTColumnDefBuilder) {
      this.$scope             = $scope;
      this.$routeParams       = $routeParams;
      this.SessionService     = SessionService;
      this.DTOptionsBuilder   = DTOptionsBuilder;
      this.DTColumnDefBuilder = DTColumnDefBuilder;
    }

    /* Callback when component is mounted and ready */
    $onInit() {
      // initialize scope variables
      this.loading      = true;
      this.currentPage  = 1; // start on the first page

      this.query = {        // query defaults:
        length: 50,   // page length
        start : 0,    // first item index
        // array of sort objects
        sorts : [],   // [ { element: 'lp', order: 'asc' } ]
        facets: 1,    // facets
      };

      // configure datatable
      this.dtOptions = this.DTOptionsBuilder.newOptions()
        .withDOM('t')
        .withBootstrap()
        .withColReorder()
        .withColReorderOption('iFixedColumnsLeft', 1)
        .withDisplayLength(this.query.length)
        .withPaginationType('full_numbers')
        .withOption('responsive', true);

      // configure datatable columns
      this.dtColumns = [
        this.DTColumnDefBuilder.newColumnDef(0).notSortable(),
        this.DTColumnDefBuilder.newColumnDef(1).notSortable(),
        this.DTColumnDefBuilder.newColumnDef(2).notSortable(),
        this.DTColumnDefBuilder.newColumnDef(3).notSortable(),
        this.DTColumnDefBuilder.newColumnDef(4).notSortable(),
        this.DTColumnDefBuilder.newColumnDef(5).notSortable(),
        this.DTColumnDefBuilder.newColumnDef(6).notSortable(),
        this.DTColumnDefBuilder.newColumnDef(7).notSortable(),
        this.DTColumnDefBuilder.newColumnDef(8).notSortable(),
        this.DTColumnDefBuilder.newColumnDef(9).notSortable(),
        this.DTColumnDefBuilder.newColumnDef(10).notSortable()
      ];

      this.getColumnInfo(); // get column infomation

      /* Listen! */
      // watch for the sorting changes (from colheader.component)
      this.$scope.$on('change:sort', (event, args) => {
        this.query.sorts = args.sorts;

        this.getData();
      });

      // watch for pagination changes (from pagination.component)
      this.$scope.$on('change:pagination', (event, args) => {
        // pagination affects length, currentPage, and start
        this.query.length = args.length;
        this.currentPage  = args.currentPage;
        this.query.start  = args.start;

        this.getData();
      });

      // watch for search expression and date range changes
      // IMPORTANT: this kicks off the inital search query
      this.$scope.$on('change:search', (event, args) => {
        this.query.startTime  = args.startTime;
        this.query.stopTime   = args.stopTime;
        this.query.expression = args.expression;

        this.getData();
      });
    } /* /$onInit */


    /* exposed functions --------------------------------------------------- */
    /**
     * Makes a request to the Session Service to get the list of sessions
     * that match the query parameters
     */
    getData() {
      this.loading  = true;
      this.error    = false;

      this.SessionService.get(this.query)
        .then((response) => {
          this.loading  = false;
          this.error    = false;
          this.sessions = response.data;
        })
        .catch((error) => {
          this.loading  = false;
          this.error    = error;
        });
    }

    getColumnInfo() {
      this.SessionService.getColumnInfo()
        .then((response) => {
          this.columnInfo = response.data;
        })
        .catch((error) => {
          this.columnInfoError = error;
        });
    }

  }

  SessionController.$inject = ['$scope', '$routeParams', 'SessionService',
    'DTOptionsBuilder', 'DTColumnDefBuilder'];


  angular.module('moloch')
    .component('session', {
      template  : require('html!../templates/session.html'),
      controller: SessionController
    });

})();
