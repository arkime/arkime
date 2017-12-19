(function() {

  'use strict';

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

      // load user's previous view choice
      if (sessionStorage && sessionStorage['moloch-view']) {
        this.view = sessionStorage['moloch-view'];
      }
      if (this.$routeParams.view) { // url param trumps storage
        this.view = this.$routeParams.view;
      }

      if (issueChange) { this.change(); }

      /* LISTEN! */
      // update the temporal parameters
      this.$scope.$on('change:time:input', (event, args) => {
        if (args.bounding)  { this.timeBounding = args.bounding;  }
        if (args.interval)  { this.timeInterval = args.interval;  }

        if (args.date) {
          this.timeRange = args.date;
          this.startTime = null;
          this.stopTime  = null;
        } else if (args.startTime && args.stopTime) {
          this.timeRange = null;
          this.startTime = args.startTime;
          this.stopTime  = args.stopTime;
        }

        this.change();
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

          if (current.params.view !== lastParams.view) {
            change = true;
            this.view = current.params.view;
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
     * Clones the view and expression url parameters to lastParams so the
     * $routeUpdate event knows if these parameters have changed
     */
    cloneParams() {
      lastParams = {}; // update the parameters
      let params = this.$location.search();
      for (let k in params) {
        if (params.hasOwnProperty(k) && (k === 'expression' || k === 'view')) {
          lastParams[k] = params[k];
        }
      }
    }

    /**
     * Fired when the url parameters for search have changed
     * (date, startTime, stopTime, expression, bounding, interval, view)
     */
    change() {
      let args = {
        expression: this.$rootScope.expression,
        bounding  : this.timeBounding,
        interval  : this.timeInterval,
        view      : this.view
      };

      if (this.timeRange) { args.date       = this.timeRange; }
      if (this.startTime) { args.startTime  = this.startTime; }
      if (this.stopTime)  { args.stopTime   = this.stopTime;  }

      this.$scope.$emit('change:search', args);
      this.$rootScope.$broadcast('issue:search', {
        expression: this.$rootScope.expression,
        view      : this.view
      });

      this.$timeout(() => { // wait for digest cycle to finish so $routeUpdate
        // event has the correct flag value
        manualChange = false;
      });
    }


    /* exposed functions --------------------------------------------------- */

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
     * Updates the expression
     */
    applyParams() {
      manualChange = true;

      this.applyExpression();

      // update the stop/start times in time.component, which in turn
      // notifies this controller (usin the 'change:time:input' event), which
      // then updates the time params and calls this.change()
      this.$scope.$broadcast('update:time');
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
      template  : require('../templates/search.html'),
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
