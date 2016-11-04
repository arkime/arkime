(function() {

  'use strict';

  /**
   * @class ColvisController
   * @classdesc Interacts with column visibility menu
   *
   * @example
   * '<colvis visible-headers="tableState.visibleHeaders"></colvis>'
   */
  class ColvisController {

    /**
     * Initialize global variables for this controller
     * @param $scope Angular application model object
     *
     * @ngInject
     */
    constructor($scope, FieldService) {
      this.$scope       = $scope;
      this.FieldService = FieldService;
    }

    $onInit() {
      // TODO: implement typeahead to search for fields
      this.FieldService.get()
        .then((result) => {
          this.fields       = result;
          this.loadingError = false;
        })
        .catch((error) => {
          this.loadingError = error;
        });
    }

    /* exposed functions --------------------------------------------------- */
    isVisible(id) {
      return this.visibleHeaders.indexOf(id);
    }

    toggleVisibility(id) {
      // TODO: persist column visibility using SessionService
      var index = this.isVisible(id);
      if (index >= 0) { // it's visible
        // remove it from the visible headers list
        this.visibleHeaders.splice(index,1);
      } else { // it's hidden
        // add it to the visible headers list
        this.visibleHeaders.push(id);
      }
    }
  }

  ColvisController.$inject = ['$scope', 'FieldService'];

  /**
   * Colvis Directive
   * Displays column visibility menu
   */
  angular.module('moloch')
    .component('colvis', {
      template  : require('html!../templates/colvis.html'),
      controller: ColvisController,
      bindings  : { visibleHeaders: '<' }
    });

})();
