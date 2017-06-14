(function() {

  'use strict';

  let basePath;

  /**
   * @class GraphMapController
   * @classdesc Interacts with the graph and map
   */
  class GraphMapController {

    /**
     * Initialize global variables for this controller
     * @param $scope    Angular application model object
     * @param $location Exposes browser address bar URL (window.location)
     *
     * @ngInject
     */
    constructor($scope, $location) {
      this.$scope     = $scope;
      this.$location  = $location;
    }

    /* Callback when component is mounted and ready */
    $onInit() {
      basePath = this.$location.path().split('/')[1];

      if (localStorage && localStorage[`${basePath}-open-map`] &&
          localStorage[`${basePath}-open-map`] !== 'false') {
        this.showMap = true;
      }

      if (this.open && this.open !== 'false') { this.showMap = true; }

      this.$scope.$on('open:map', () => {
        this.showMap = true;
      });
      this.$scope.$on('close:map', () => {
        this.showMap = false;
      });
    }


    /* exposed functions --------------------------------------------------- */
    toggleMap() {
      this.showMap = !this.showMap;

      if (this.primary && this.showMap) {
        this.$scope.$emit('open:maps');
        if (localStorage) { localStorage[`${basePath}-open-map`] = true; }
      } else if (this.primary && !this.showMap) {
        this.$scope.$emit('close:maps');
        if (localStorage) { localStorage[`${basePath}-open-map`] = false; }
      }
    }

  }

  GraphMapController.$inject = ['$scope','$location'];

  /**
   * Graph Map Component
   * Displays graph and map visualizations
   */
  angular.module('moloch')
    .component('molochGraphMap', {
      template    : require('html!../templates/graph.map.html'),
      controller  : GraphMapController,
      bindings    : {
        graphData : '<',
        mapData   : '<',
        graphType : '@',
        primary   : '@',
        open      : '@',
        timezone  : '@'
      }
    });

})();
