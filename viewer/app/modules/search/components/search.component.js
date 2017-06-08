(function() {

  'use strict';

  const hourMS    = 3600000;
  let currentTime = new Date().getTime();
  let initialized;
  let lastParams = {};
  let manualChange;

  /**
   * @class SearchController
   * @classdesc Interacts with the search controls
   */
  class SearchController {

    /**
     * Initialize global variables for this controller
     * @param $scope        Angular application model object
     * @param $timeout      Angular's wrapper for window.setTimeout
     * @param $location     Exposes browser address bar URL (based on the window.location)
     * @param $rootScope    Angular application main scope
     * @param $routeParams  Retrieve the current set of route parameters
     * @param ConfigService Transacts app configurations with the server
     * @param UserService   Transacts users with the server
     *
     * @ngInject
     */
    constructor($scope, $timeout, $location, $rootScope, $routeParams,
                ConfigService, UserService) {
      this.$scope         = $scope;
      this.$timeout       = $timeout;
      this.$location      = $location;
      this.$rootScope     = $rootScope;
      this.$routeParams   = $routeParams;
      this.ConfigService  = ConfigService;
      this.UserService    = UserService;
    }

    /* Callback when component is mounted and ready */
    $onInit() {
      this.cloneParams();

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

      // load routeParameter expression on initialization
      if (!initialized && this.$routeParams.expression) {
        this.$rootScope.expression = this.$routeParams.expression;
      }
      initialized = true;

      // update the url parameters with the expression
      let issueChange = true;
      if (this.$routeParams.expression !== this.$rootScope.expression) {
        issueChange = false; // don't issue a change event
        // this function will issue the change because it updates the url params
        this.applyExpression();
      }

      // update the time inputs based on the url parameters
      this.setupTimeParams(this.$routeParams.date, this.$routeParams.startTime,
         this.$routeParams.stopTime);

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

      if (issueChange) { this.change(); }

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

      // watch for the url parameters to change and update the page
      // date, startTime, stopTime, expression, bounding, and view parameters
      // are managed by the search component
      this.$scope.$on('$routeUpdate', (event, current) => {
        if (!manualChange) {
          let change = false;

          if (current.params.expression !== lastParams.expression) {
            change = true;
            this.$rootScope.expression = current.params.expression;
          }
          if (current.params.bounding !== lastParams.bounding) {
            change = true;
            this.timeBounding = current.params.bounding || 'last';
          }
          if (current.params.view !== lastParams.view) {
            change = true;
            this.view = current.params.view;
          }
          if (current.params.date !== lastParams.date ||
              current.params.stopTime !== lastParams.stopTime ||
              current.params.startTime !== lastParams.startTime) {
            change = true;
            this.setupTimeParams(current.params.date, current.params.startTime,
               current.params.stopTime);
          }

          if (change) { this.change(); }
        }

        this.cloneParams();
      });

      this.$scope.$on('apply:expression', () => {
        this.applyExpression();
        this.change();
      });

      this.$scope.$on('shift:time', () => {
        this.change();
      });
    } /* /onInit */

    /**
     * Clones the url parameters to lastParams so the $routeUpdate event
     * knows if the params that matter have changed
     */
    cloneParams() {
      lastParams = {}; // update the parameters
      for (let k in this.$location.search()) {
        lastParams[k] = this.$location.search()[k];
      }
    }

    /**
     * Sets up time query parameters and updates the url if necessary
     * @param {string} date           The time range to query within
     * @param {string} startTime      The start time for a custom time range
     * @param {string} stopTime       The stop time for a custom time range
     */
    setupTimeParams(date, startTime, stopTime) {
      if (date) { // time range is available
        this.timeRange = date;
        if (this.timeRange === '-1') { // all time
          this.startTime  = hourMS * 5;
          this.stopTime   = currentTime;
        } else if (this.timeRange > 0) {
          this.stopTime   = currentTime;
          this.startTime  = currentTime - (hourMS * this.timeRange);
        }
      } else if(startTime && stopTime) {
        // start and stop times available
        let stop  = parseInt(stopTime * 1000, 10);
        let start = parseInt(startTime * 1000, 10);

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
        }
      } else if (!date && !startTime && !stopTime) {
        // there are no time query parameters, so set defaults
        this.timeRange = '1'; // default to 1 hour
      }
    }

    /**
     * Fired when the url parameters for search have changed
     * (date, startTime, stopTime, expression, bounding, view)
     */
    change() {
      let useDateRange = false;

      // build the parameters to send to the parent controller that makes the req
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
          expression: this.$rootScope.expression,
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
          expression: this.$rootScope.expression,
          view      : this.view
        });
      }

      this.$timeout(() => { // wait for digest cycle to finish so $routeUpdate
        // event has the correct flag value
        manualChange = false;
      });
    }


    /* exposed functions --------------------------------------------------- */
    /**
     * Fired when the time range value changes
     * Updating the url parameter triggers $routeUpdate which triggers change()
     */
    changeTimeRange() {
      this.timeError = false;

      this.$location.search('date', this.timeRange);
      this.$location.search('stopTime', null);
      this.$location.search('startTime', null);
    }

    /**
     * Validates a date and updates delta time (stop time - start time)
     * Fired when a date value is changed (with 500 ms delay)
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

      if (loadData) { this.applyDate(); }
     }

     /**
      * Fired when change bounded checkbox is (un)checked
      * Applies the timeBounding url parameter
      * Updating the url parameter triggers $routeUpdate which triggers change()
      */
     changeTimeBounding() {
       if (this.timeBounding !== 'last') {
         this.$location.search('bounding', this.timeBounding);
       } else {
         this.$location.search('bounding', null);
       }
     }

    /**
     * Fired when search button or enter is clicked
     * Updates the date, stopTime, and startTime url parameters
     */
    applyDate() {
      this.$location.search('date', null);
      this.$location.search('stopTime', parseInt((this.stopTime / 1000).toFixed()));
      this.$location.search('startTime', parseInt((this.startTime / 1000).toFixed()));
    }

    /**
     * Fired when search button or enter is clicked
     * Updates the expression url parameter
     */
    applyExpression() {
      if (this.$rootScope.expression && this.$rootScope.expression !== '') {
        this.$location.search('expression', this.$rootScope.expression);
      } else {
        this.$location.search('expression', null);
      }
    }

    /**
     * Fired when the search button or enter is clicked
     * Updates the expression, date, startTime, and stopTime url parameters
     */
    applyParams() {
      manualChange = true;

      this.applyExpression();
      if (parseInt(this.timeRange) === 0) { this.applyDate(); }

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
         expression : this.$rootScope.expression,
         view       : this.view
       });

       this.$rootScope.$broadcast('issue:search', {
         expression : this.$rootScope.expression,
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

  SearchController.$inject = ['$scope','$timeout','$location','$rootScope',
    '$routeParams','ConfigService','UserService'];

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
