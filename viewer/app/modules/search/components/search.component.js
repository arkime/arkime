(function() {

  'use strict';

  const hourMS    = 3600000;
  let currentTime = new Date().getTime();

  /**
   * @class SearchController
   * @classdesc Interacts with the search controls
   */
  class SearchController {

    /**
     * Initialize global variables for this controller
     * @param $scope        Angular application model object
     * @param $location     Exposes browser address bar URL (based on the window.location)
     * @param $rootScope    Angular application main scope
     * @param $routeParams  Retrieve the current set of route parameters
     * @param ConfigService Transacts app configurations with the server
     * @param UserService   Transacts users with the server
     *
     * @ngInject
     */
    constructor($scope, $location, $rootScope, $routeParams, ConfigService, UserService) {
      this.$scope         = $scope;
      this.$location      = $location;
      this.$rootScope     = $rootScope;
      this.$routeParams   = $routeParams;
      this.ConfigService  = ConfigService;
      this.UserService    = UserService;
    }

    /* Callback when component is mounted and ready */
    $onInit() {
      this.ConfigService.getMolochClusters()
         .then((clusters) => {
           this.molochclusters = clusters;
         });

      this.UserService.getViews()
         .then((views) => {
           this.views = views || {};
         });

      this.actionFormItemRadio = 'visible';

      if (!this.openSessions && !this.numVisibleSessions) {
        this.actionFormItemRadio = 'matching';
      }

      if (this.$routeParams.date) { // time range is available
        this.timeRange = this.$routeParams.date;
        if (this.timeRange === '-1') { // all time
          this.startTime  = hourMS * 5;
          this.stopTime   = currentTime;
        }
        this.$location.search('stopTime', null);
        this.$location.search('startTime', null);
      } else if(this.$routeParams.startTime && this.$routeParams.stopTime) {
        // start and stop times available
        let stop  = parseInt(this.$routeParams.stopTime * 1000, 10);
        let start = parseInt(this.$routeParams.startTime * 1000, 10);
        if (stop && start && !isNaN(stop) && !isNaN(start)) {
          // if we can parse start and stop time, set them
          this.timeRange  = '0'; // custom time range
          this.stopTime   = stop;
          this.startTime  = start;
          if (stop < start) {
            this.timeError = 'Stop time cannot be before start time';
          }
          // update the displayed time range
          this.deltaTime = this.stopTime - this.startTime;
        } else { // if we can't parse stop or start time, set default
          this.timeRange = '1'; // default to 1 hour
          this.$location.search('date', this.timeRange);
          this.$location.search('stopTime', null);
          this.$location.search('startTime', null);
        }
      } else if (!this.$routeParams.date &&
          !this.$routeParams.startTime && !this.$routeParams.stopTime) {
        // there are no time query parameters, so set defaults
        this.timeRange = '1'; // default to 1 hour
        this.$location.search('date', this.timeRange); // update url params
      }

      if (this.$routeParams.expression) {
        this.expression = { value: this.$routeParams.expression };
      } else { this.expression = { value: null }; }

      this.timeBounding = 'last'; // default to lastPacket
      if (this.$routeParams.bounding) { this.timeBounding = this.$routeParams.bounding; }

      // load user's previous view choice
      if (sessionStorage && sessionStorage['moloch-view']) {
        this.view = sessionStorage['moloch-view'];
      }
      if (this.$routeParams.view) { // url param trumps storage
        this.view = this.$routeParams.view;
      }

      // date picker popups hidden to start
      this.startTimePopup   = { opened: false };
      this.stopTimePopup    = { opened: false };
      // date picker display format
      this.dateTimeFormat   = 'yyyy/MM/dd HH:mm:ss';
      // other acceptable formats
      this.altInputFormats  = ['yyyy/M!/d! H:mm:ss'];

      this.change();

      // watch for changes in time parameters
      this.$scope.$on('update:time', (event, args) => {
        if (args.start) { // start time changed
          this.startTime  = parseInt(args.start * 1000, 10);
        }
        if (args.stop) {  // stop time changed
          this.stopTime   = parseInt(args.stop * 1000, 10);
        }

        this.changeDate(true);
      });

      // watch for closing the action form
      this.$scope.$on('close:form:container', (event, args) => {
        this.actionForm = false;
        if (args && args.message) {
          this.message      = args.message;
          this.messageType  = args.success ? 'success' : 'warning';
        }
      });

      this.$scope.$on('update:views', (event, args) => {
        if (args.views) { this.views = args.views; }
      });
    }


    /* exposed functions --------------------------------------------------- */
    /**
     * Fired when the time range value changes
     */
    changeTimeRange() {
      this.timeError = false;

      this.$location.search('date', this.timeRange);
      this.$location.search('stopTime', null);
      this.$location.search('startTime', null);

      this.change();
    }

    /**
     * Fired when a date value is changed
     * @param {bool} loadData Whether to issue query after updating time
     */
     changeDate(loadData) {
       this.timeError = false;
       this.timeRange = '0'; // custom time range

       let stopSec  = parseInt((this.stopTime / 1000).toFixed());
       let startSec = parseInt((this.startTime / 1000).toFixed());

       // only continue if start and stop are valid numbers
       if (!startSec || !stopSec || isNaN(startSec) || isNaN(stopSec)) {
         return;
       }

       if (stopSec < startSec) { // don't continue if stop < start
         this.timeError = 'Stop time cannot be before start time';
         return;
       }

       // update the displayed time range
       this.deltaTime = this.stopTime - this.startTime;

       this.$location.search('date', null);
       this.$location.search('stopTime', stopSec);
       this.$location.search('startTime', startSec);

       if (loadData) { this.change(); }
     }

     /**
      * Fired when change bounded checkbox is (un)checked
      */
     changeTimeBounding() {
       if (this.timeBounding !== 'last') {
         this.$location.search('bounding', this.timeBounding);
       } else {
         this.$location.search('bounding', null);
       }

       this.change();
     }

    /**
     * Sets the view that applies the query expression to the results
     * @param {string} view The name of the view to set
     */
     setView(view) {
       this.view = view;

       // update url and session storage (to persist user's choice)
       if (!view) {
         delete sessionStorage['moloch-view'];
         this.$location.search('view', null);
       } else {
         sessionStorage['moloch-view'] = view;
         this.$location.search('view', view);
       }

       this.$scope.$emit('change:search', {
         expression : this.expression.value,
         view       : this.view
       });

       this.$rootScope.$broadcast('issue:search', {
         expression : this.expression.value,
         view       : this.view
       });
     }

    /**
     * Removes a view
     * @param {string} view The name of the view to remove
     */
    deleteView(view) {
      this.UserService.deleteView(view)
        .then((response) => {
          let args = {};

          if (response.text) {
            args.message = response.text;
            args.success = response.success;
          }

          // notify parent to close form and display message
          this.$scope.$emit('close:form:container', args);

          if (response.success) {
            if (this.view === view) {
              this.setView(undefined);
            }

            this.views[view] = null;
            delete this.views[view];
          }
        })
        .catch((err) => {
          // notify parent to close form and display message
          this.$scope.$emit('close:form:container', {
            message: err, success: false
          });
        });
    }

    /**
     * Fired when a search control value is changed
     * (startTime, stopTime, timeRange, expression, bounding)
     */
    change() {
      let useDateRange = false;

      // update the parameters with the expression
      if (this.expression.value && this.expression.value !== '') {
        this.$location.search('expression', this.expression.value);
      } else {
        this.$location.search('expression', null);
      }

      if (this.timeRange > 0) {
        // if it's not a custom time range or all, update the time
        currentTime = new Date().getTime();

        this.stopTime   = currentTime;
        this.startTime  = currentTime - (hourMS * this.timeRange);
      }

      if (parseInt(this.timeRange) === -1) { // all time
        this.startTime  = hourMS * 5;
        this.stopTime   = currentTime;
        useDateRange    = true;
      }

      // always use startTime and stopTime instead of date range (except for all)
      // querying with date range causes unexpected paging behavior
      // because there are always new sessions
      if (this.startTime && this.stopTime) {
        let args = {
          expression: this.expression.value,
          bounding  : this.timeBounding,
          view      : this.view
        };

        if (useDateRange) { args.date = -1; }
        else {
          args.startTime  = (this.startTime / 1000).toFixed();
          args.stopTime   = (this.stopTime / 1000).toFixed();
        }

        this.$scope.$emit('change:search', args);

        this.$rootScope.$broadcast('issue:search', {
          expression: this.expression.value,
          view      : this.view
        });
      }
    }

    /* remove the message when user is done with it or duration ends */
    messageDone() {
      this.message = null;
      this.messageType = null;
    }


    /* Action Menu Functions ----------------------------------------------- */
    /* displays the remove tag form */
    addTags() {
      this.actionForm = 'add:tags';
      this.showApplyButtons = true;
    }

    /* displays the remove tag form */
    removeTags() {
      this.actionForm = 'remove:tags';
      this.showApplyButtons = true;
    }

    /* displays the export pcap form */
    exportPCAP() {
      this.actionForm = 'export:pcap';
      this.showApplyButtons = true;
    }

    /* displays the export csv form */
    exportCSV() {
      this.actionForm = 'export:csv';
      this.showApplyButtons = true;
    }

    /* displays the scrub pcap form */
    scrubPCAP() {
      this.actionForm = 'scrub:pcap';
      this.showApplyButtons = true;
    }

    /* displays the delete session form */
    deleteSession() {
      this.actionForm = 'delete:session';
      this.showApplyButtons = true;
    }

    /**
     * displays the send session form
     * @param {string} cluster The name of the cluster
     */
    sendSession(cluster) {
      this.cluster = cluster;
      this.actionForm = 'send:session';
      this.showApplyButtons = true;
    }

    /* display the create view form */
    createView() {
      this.actionForm = 'create:view';
      this.showApplyButtons = false;
    }

  }

  SearchController.$inject = ['$scope','$location','$rootScope','$routeParams',
    'ConfigService', 'UserService'];

  /**
   * Search Component
   * Displays searching controls
   */
  angular.module('directives.search', [])
    .component('molochSearch', {
      template  : require('html!../templates/search.html'),
      controller: SearchController,
      bindings  : {
        openSessions        : '<',
        numVisibleSessions  : '<',
        numMatchingSessions : '<',
        start               : '<',
        timezone            : '<',
        fields              : '<'
      }
    });

})();
