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
    constructor($scope, $routeParams, SessionService, DTOptionsBuilder, DTColumnDefBuilder) {
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
      this.currentPage  = 1;

      this.query = {
        length      : 100,  // default page size
        start       : 0,    // start at first item
        sortElement : 'fp', // default sort element
        sortOrder   : 'asc',// default sort order
        date        : 1,    // default date range
        facets      : 1,    // default facets
        draw        : 1     // default draw
      }

      // configure table
      this.dtOptions = this.DTOptionsBuilder.newOptions()
        .withDOM('t')
        .withBootstrap()
        .withColReorder()
        .withDisplayLength(this.query.length)
        .withPaginationType('full_numbers')
        .withOption('responsive', true);
        // .withFixedColumns({
        //   leftColumns: 1
        // });

      // get table data
      this.getData();

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
        this.DTColumnDefBuilder.newColumnDef(8).notSortable()
      ];

      this.$scope.$on('change:sort', (event, args) => {
        this.query.sortElement  = args.sortElement;
        this.query.sortOrder    = args.sortOrder;

        this.getData();
      });
    }; /* /$onInit */


    /* exposed functions --------------------------------------------------- */
    /**
     * Makes a request to the Session Service to get the list of sessions
     * that match the query parameters
     */
    getData() {
      this.loading  = true;
      this.error    = false;

      this.query.start  = (this.currentPage - 1) * this.query.length;

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
    };

  }

  SessionController.$inject = ['$scope', '$routeParams', 'SessionService',
    'DTOptionsBuilder', 'DTColumnDefBuilder'];


  angular.module('moloch')
    .component('session', {
      templateUrl : '/modules/session/session.html',
      controller  : SessionController
    });

})();
