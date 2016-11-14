(function() {

  'use strict';

  // local variable to save query state
  var _query = {  // set query defaults:
    length: 100,  // page length
    start : 0,    // first item index
    facets: 1     // facets
  };

  var defaultTableState = {
    order         : [['fp', 'asc']],
    visibleHeaders: ['', 'fp', 'lp', 'a1', 'p1', 'a2', 'p2', 'pa', 'by', 'no', 'info']
  };

  var customCols = require('json!./custom.columns.json');

  /**
   * @class SessionListController
   * @classdesc Interacts with session list
   */
  class SessionListController {

    /* setup --------------------------------------------------------------- */
    /**
     * Initialize global variables for this controller
     * @param $scope          Angular application model object
     * @param $location       Exposes browser address bar URL (based on the window.location)
     * @param $routeParams    Retrieve the current set of route parameters
     * @param $anchorScroll   Scrolls to the element related to given hash
     * @param SessionService  Transacts sessions with the server
     * @param FieldService    Retrieves available fiels from the server
     *
     * @ngInject
     */
    constructor($scope, $location, $routeParams, $anchorScroll,
      SessionService, FieldService) {

      this.$scope         = $scope;
      this.$location      = $location;
      this.$routeParams   = $routeParams;
      this.$anchorScroll  = $anchorScroll;
      this.SessionService = SessionService;
      this.FieldService   = FieldService;

      // offset anchor scroll position to account for navbars
      this.$anchorScroll.yOffset = 140;
    }

    /* Callback when component is mounted and ready */
    $onInit() { // initialize scope variables
      this.loading      = true;
      this.currentPage  = 1;    // always start on the first page

      this.query = _query;      // load saved query

      this.stickySessions = []; // array of open sessions

      // get the state of the table (sort order and column visibility)
      this.SessionService.getTableState()
        .then((response) => {
          this.tableState = response.data;
          if (Object.keys(this.tableState).length === 0) {
            this.tableState = defaultTableState;
          }

          // update the sort order for the session table query
          this.query.sorts = this.tableState.order;

          this.FieldService.get()
            .then((result) => {
              this.fields = result;

              // add custom columns to the visible columns list and table
              for (var key in customCols) {
                this.fields[key] = customCols[key];
                var children = this.fields[key].children;
                // expand all the children
                for (var c in children) {
                  // (replace fieldId with field object)
                  children[c] = this.getField(children[c]);
                }
              }

              this.mapHeadersToFields();

              // convert fieldsmap to array (for ng-repeat with filter and group)
              // and remove duplicate fields (e.g. 'host.dns' & 'dns.host')
              var map = {}; // lookup table to save fields added to fieldsArray
              this.fieldsArray = [];
              for (var f in this.fields) {
                if (this.fields.hasOwnProperty(f)) {
                  var field = this.fields[f];
                  if (!map.hasOwnProperty(field.exp)) {
                    map[field.exp] = field;
                    this.fieldsArray.push(field);
                  }
                }
              }

              // IMPORTANT: kicks off the inital search query
              this.getData();
            })
            .catch((error) => {
              this.error = error;
            });
        })
        .catch((error) => {
          this.error = error;
        });


      /* Listen! */
      // watch for pagination changes (from pagination.component)
      this.$scope.$on('change:pagination', (event, args) => {
        // pagination affects length, currentPage, and start
        _query.length = this.query.length = args.length;
        _query.start  = this.query.start  = args.start;

        this.currentPage = args.currentPage;

        this.getData();
      });

      // watch for search expression and date range changes
      // (from search.component)
      var initalized;
      this.$scope.$on('change:search', (event, args) => {
        // either startTime && stopTime || date
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

        // reset the user to the first page, because we are issuing a new query
        // and there may only be 1 page of results
        _query.start = this.query.start = 0;

        if (!initalized) { initalized = true; return; }

        this.getData();
      });

      // watch for additions to search parameters from session detail or map
      this.$scope.$on('add:to:search', (event, args) => {
        // notify children (namely expression typeahead)
        this.$scope.$broadcast('add:to:typeahead', args);
      });

      // watch for changes to time parameters from session detail
      this.$scope.$on('change:time', (event, args) => {
        // notify children (namely search component)
        this.$scope.$broadcast('update:time', args);
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

      this.stickySessions = []; // clear sticky sessions
      // clear fields to query for but always include protocols field
      this.query.fields   = ['pr'];
      
      // set the fields to retrieve from the server for each session
      if (this.headers) {
        for (var i = 0; i < this.headers.length; ++i) {
          var field = this.headers[i];
          if (field.children) {
            for (var j = 0; j < field.children.length; ++j) {
              this.query.fields.push(field.children[j].dbField);
            }
          } else {
            this.query.fields.push(field.dbField);
          }
        }
      }

      this.SessionService.get(this.query)
        .then((response) => {
          this.loading  = false;
          this.error    = false;
          this.sessions = response.data;

          this.graphData  = response.data.graph;
          this.mapData    = response.data.map;
        })
        .catch((error) => {
          this.loading  = false;
          this.error    = error;
        });
    }

    /**
     * Toggles the display of the session detail for each session
     * @param {Object} session The session to expand, collapse details
     */
    toggleSessionDetail(session) {
      session.expanded = !session.expanded;

      if (session.expanded) {
        this.stickySessions.push(session);
      } else {
        var index = this.stickySessions.indexOf(session);
        if (index >= 0) { this.stickySessions.splice(index, 1); }
      }
    }


    /**
     * Maps visible column headers to their corresponding fields
     */
    mapHeadersToFields() {
      this.headers = [];
      for (var i = 0, len = this.tableState.visibleHeaders.length; i < len; ++i) {
        var headerId = this.tableState.visibleHeaders[i];
        var field = this.getField(headerId);
        if (field) { this.headers.push(field); }
        else { this.tableState.visibleHeaders.splice(i, 1); }
      }
    }

    /**
     * Finds a field object given its id
     * @param {string} fieldId  The unique id of the field
     * @return {Object} field   The field object
     */
    getField(fieldId) {
      for (var key in this.fields) {
        if (this.fields.hasOwnProperty(key)) {
          var item = this.fields[key];
          if (item.dbField === fieldId) {
            return item;
          }
        }
      }

      return undefined;
    }


    /* TABLE SORTING */
    /**
     * Sorts the sessions by the clicked column
     * @param {Object} event  The click event that triggered the sort
     * @param {string} id     The id of the column to sort by
     */
    sortBy(event, id) {
      if (this.isSorted(id) >= 0) {
        // the table is already sorted by this element
        if (!event.shiftKey) {
          var item = this.toggleSortOrder(id);
          this.tableState.order = [item];
        } else {
          // if it's a shift click - toggle the order between 3 states:
          // 'asc' -> 'desc' -> removed from sorts
          if (this.getSortOrder(id) === 'desc') {
            for (var i = 0, len = this.tableState.order.length; i < len; ++i) {
              if (this.tableState.order[i][0] === id) {
                this.tableState.order.splice(i, 1);
                break;
              }
            }
          } else {
            this.toggleSortOrder(id);
          }
        }
      } else { // sort by a new column
        if (!event.shiftKey) {
          // if it's a regular click - remove other sorts and add this one
          this.tableState.order = [[ id, 'asc' ]];
        } else {
          // if it's a shift click - add it to the list
          this.tableState.order.push([ id, 'asc' ]);
        }
      }

      this.query.sorts = this.tableState.order;

      this.SessionService.saveTableState(this.tableState)
        .catch((error) => {
          this.error = error;
        });

      this.getData();
    }

    /**
     * Determines if the table is being sorted by specified column
     * @param {string} id The id of the column
     */
    isSorted(id) {
      for (var i = 0; i < this.tableState.order.length; ++i) {
        if (this.tableState.order[i][0] === id) { return i; }
      }

      return -1;
    }

    /**
     * Determines the sort order of a column
     * @param {string} id     The unique id of the column
     * @return {string} order The sort order of the column
     */
    getSortOrder(id) {
      for (var i = 0, len = this.tableState.order.length; i < len; ++i) {
        if (this.tableState.order[i][0] === id) {
          return this.tableState.order[i][1];
        }
      }
    }

    /**
     * Toggles the sort order of a column, given its id
     * ('asc' -> 'desc' & 'desc' -> 'asc')
     * @param {string} id   The id of the column to toggle
     * @return {Array} item The sort item with updated order
     */
    toggleSortOrder(id) {
      for (var i = 0, len = this.tableState.order.length; i < len; ++i) {
        var item = this.tableState.order[i];
        if (item[0] === id) {
          if (item[1] === 'asc') { item[1] = 'desc'; }
          else { item[1] = 'asc'; }
          return item;
        }
      }
    }


    /* COLUMN VISIBILITY */
    /**
     * Determines a column's visibility given its id
     * @param {string} id       The id of the column
     * @return {number} number  The index of the visible header
     */
    isVisible(id) {
      return this.tableState.visibleHeaders.indexOf(id);
    }

    /**
     * Toggles the visiblity of a column given its id
     * @param {string} id The id of the column to show/hide (toggle)
     */
    toggleVisibility(id) {
      this.loading = true;

      var index = this.isVisible(id);

      if (index >= 0) { // it's visible
        // remove it from the visible headers list
        this.tableState.visibleHeaders.splice(index,1);
        this.mapHeadersToFields();
      } else { // it's hidden
        // add it to the visible headers list
        this.tableState.visibleHeaders.push(id);
        this.mapHeadersToFields();
        this.getData();
      }

      this.SessionService.saveTableState(this.tableState)
        .then(() => { this.loading = false; })
        .catch((error) => {
          this.error = error;
        });
    }





    onDropComplete(index, obj, event) {
      var target        = this.tableState.visibleHeaders[index];
      var draggedIndex  = this.tableState.visibleHeaders.indexOf(obj.dbField);

      this.tableState.visibleHeaders[index] = obj.dbField;
      this.tableState.visibleHeaders[draggedIndex] = target;

      this.mapHeadersToFields();
      this.SessionService.saveTableState(this.tableState)
         .then(() => { this.loading = false; })
         .catch((error) => {
           this.error = error;
         });
    }






  }

  SessionListController.$inject = ['$scope', '$location', '$routeParams',
    '$anchorScroll', 'SessionService', 'FieldService'];


  angular.module('moloch')
    .component('session', {
      template  : require('html!../templates/session.list.html'),
      controller: SessionListController
    });

})();
