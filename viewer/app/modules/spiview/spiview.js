(function() {

  'use strict';

  // local variable to save query state
  let _query = {  // set query defaults:
    length: 50,   // page length
    start : 0,    // first item index
    facets: 1,    // facets
    spi   : 'a2:100,prot-term:100,a1:100' //?spi=a2:100,prot-term:100,a1:100&date=1&length=100
  };

  /**
   * @class SpiviewController
   * @classdesc Interacts with moloch spiview page
   * @example
   * '<moloch-spiview></moloch-spiview>'
   */
  class SpiviewController {

    /**
     * Initialize global variables for this controller
     * TODO
     * @ngInject
     */
    constructor($scope, SpiviewService) {
      this.$scope         = $scope;
      this.SpiviewService = SpiviewService;
    }

    /* Callback when component is mounted and ready */
    $onInit() {
      this.loading  = true;
      this.query    = _query; // load saved query

      // TODO default query?
      // TODO read query parameters in url
      // TODO build spi query string

      // TODO chain promises?
      this.getCategories();
      this.getSpiData();

      let initialized;
      this.$scope.$on('change:search', (event, args) => {
        // either (startTime && stopTime) || date
        if (args.startTime && args.stopTime) {
          _query.startTime  = this.query.startTime  = args.startTime;
          _query.stopTime   = this.query.stopTime   = args.stopTime;
          _query.date       = this.query.date       = null;
        } else if (args.date) {
          _query.date       = this.query.date       = args.date;
          _query.startTime  = this.query.startTime  = null;
          _query.stopTime   = this.query.stopTime   = null;
        }

        _query.expression = this.query.expression = args.expression;
        if (args.bounding) {_query.bounding = this.query.bounding = args.bounding;}

        // reset to the first page, because we are issuing a new query
        // and there may only be 1 page of results
        _query.start = this.query.start = 0;

        this.query.view = args.view;

        // don't issue search when the first change:search event is fired
        if (!initialized) { initialized = true; return; }

        this.getSpiData();
      });
    }

    getCategories() {
      this.SpiviewService.getCategories()
        .then((response) => {
          this.categories = response;
        })
        .catch((error) => {
          this.error = error.text;
        });
    }

    getSpiData() {
      this.loading = true;

      this.SpiviewService.get(this.query)
        .then((response) => {
          this.loading  = false;
          this.error    = false;

          this.spi        = response.spi;
          this.mapData    = response.map;
          this.graphData  = response.graph;
          this.protocols  = response.protocols;
          this.total      = response.recordsTotal;
          this.filtered   = response.recordsFiltered;

          console.log(response);
        })
        .catch((error) => {
          this.loading  = false;
          this.error    = error.text;
        });
    }

  }

  SpiviewController.$inject = ['$scope','SpiviewService'];

  /**
   * SPI View Directive
   * Displays SPI View page
   */
  angular.module('moloch')
  .component('molochSpiview', {
    template  : require('html!./spiview.html'),
    controller: SpiviewController
  });

})();
