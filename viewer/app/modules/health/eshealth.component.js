(function() {

  'use strict';

  /**
   * @class ESHealthController
   * @classdesc Interacts with elasticsearch status
   * @example
   * '<eshealth></eshealth>'
   */
  class ESHealthController {

    /**
     * Initialize global variables for this controller
     * @param $interval     Angular's wrapper for window.setInterval
     * @param HealthService Transacts es health with the server
     *
     * @ngInject
     */
    constructor($interval, HealthService) {
      this.$interval      = $interval;
      this.HealthService  = HealthService;
    }

    /* Callback when component is mounted and ready */
    $onInit() {
      this.tooltipVisible = false;

      this.getData();

      this.$interval(() => { this.getData(); }, 10000);
    }

    /**
     * Retrieves ES Health
     */
    getData() {
      this.error = false;

      this.HealthService.esHealth()
        .then((result) => {
          this.error    = false;
          this.esHealth = result;
        })
        .catch((error) => {
          this.error    = error;
        });
    }

  }

  ESHealthController.$inject = ['$interval','HealthService'];

  /**
   * ES Health Directive
   * Displays elasticsearch health status
   */
  angular.module('moloch')
    .component('esHealth', {
      template  : require('./eshealth.html'),
      controller: ESHealthController
    });

})();
