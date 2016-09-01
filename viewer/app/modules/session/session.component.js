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
        length      : 50,   // page length
        start       : 0,    // first item index
        sortElement : 'fp', // sort element (key of session field)
        sortOrder   : 'asc',// sort order ('asc' or 'desc')
        date        : 1,    // date range
        facets      : 1,    // facets
        draw        : 1     // draw
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


      // call backend for data!
      this.getData();       // get table data
      this.getColumnInfo(); // get column infomation


      /* Listen! */
      // watch for the sorting changes (from colheader.component)
      this.$scope.$on('change:sort', (event, args) => {
        // sorting affects sortElement and sortOrder
        this.query.sortElement  = args.sortElement;
        this.query.sortOrder    = args.sortOrder;

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
          // TODO: display column information error
          this.columnInfoError = error;
        });
    }

  }

  SessionController.$inject = ['$scope', '$routeParams', 'SessionService',
    'DTOptionsBuilder', 'DTColumnDefBuilder'];


  angular.module('moloch')
    .component('session', {
      template  : require('html!./session.html'),
      controller: SessionController
    });

})();
