(function() {

  'use strict';

  /**
   * @class GraphMapController
   * @classdesc Interacts with the graph and map
   */
  class GraphMapController {

    /**
     * Initialize global variables for this controller
     * TODO
     * @ngInject
     */
    constructor($scope) {
      this.$scope = $scope;
    }

    /* Callback when component is mounted and ready */
    $onInit() {
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
      } else if (this.primary && !this.showMap) {
        this.$scope.$emit('close:maps');
      }
    }

  }

  GraphMapController.$inject = ['$scope'];

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
        primary   : '@'
      }
    });

})();
