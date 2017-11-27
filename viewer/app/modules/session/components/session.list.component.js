(function() {

  'use strict';

  // local variable to save query state
  let _query = {  // set query defaults:
    length: 50,   // page length
    start : 0,    // first item index
    facets: 1     // facets
  };

  const defaultTableState = {
    order         : [['firstPacket', 'asc']],
    visibleHeaders: ['firstPacket','lastPacket','src','srcPort','dst','dstPort','totPackets','dbby','node','info']
  };

  let customCols = require('./custom.columns.json');

  let componentInitialized = false, colResizeInitialized = false;
  let holdingClick = false, timeout;

  // window/table resize variables
  let resizeTimeout, windowResizeEvent, defaultInfoColWidth = 250;

  /**
   * @class SessionListController
   * @classdesc Interacts with session list
   */
  class SessionListController {

    /* setup --------------------------------------------------------------- */
    /**
     * Initialize global variables for this controller
     * @param $scope          Angular application model object
     * @param $window         Angular reference to the browser's window object
     * @param $timeout        Angular's wrapper for window.setTimeout
     * @param $location       Exposes browser address bar URL (based on the window.location)
     * @param $routeParams    Retrieve the current set of route parameters
     * @param $anchorScroll   Scrolls to the element related to given hash
     * @param SessionService  Transacts sessions with the server
     * @param FieldService    Retrieves available fields from the server
     * @param UserService     Transacts users and user data with the server
     *
     * @ngInject
     */
    constructor($scope, $window, $timeout, $location, $routeParams, $anchorScroll,
      SessionService, FieldService, UserService) {

      this.$scope         = $scope;
      this.$window        = $window;
      this.$timeout       = $timeout;
      this.$location      = $location;
      this.$routeParams   = $routeParams;
      this.$anchorScroll  = $anchorScroll;
      this.SessionService = SessionService;
      this.FieldService   = FieldService;
      this.UserService    = UserService;

      // offset anchor scroll position to account for navbars
      this.$anchorScroll.yOffset = 115;
    }

    /* Callback when component is mounted and ready */
    $onInit() { // initialize scope variables
      this.showSessions   = true;   // show sessions in table
      this.loading        = true;   // the page starts out in loading state
      this.currentPage    = 1;      // always start on the first page
      this.query          = _query; // load saved query
      this.stickySessions = [];     // array of open sessions

      this.getColumnWidths();

      this.getTableState(); // IMPORTANT: kicks off the initial search query!

      this.getCustomColumnConfigurations();

      if (this.$routeParams.length) {
        _query.length = this.query.length = this.$routeParams.length;
      }

      /* Listen! */
      // watch for pagination changes (from pagination.component)
      this.$scope.$on('change:pagination', (event, args) => {
        // pagination affects length, currentPage, and start
        // only update vars and get data if something has changed
        // this can be fired twice because there are 2 pagination controls
        if (this.query.length !== args.length ||
            this.query.start !== args.start ||
            this.currentPage !== args.currentPage) {
          _query.length = this.query.length = args.length;
          _query.start  = this.query.start  = args.start;

          this.currentPage = args.currentPage;

          this.getData(true);
        }
      });

      // watch for search expression and date range changes
      // (from search.component)
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
        if (args.interval) {_query.interval = this.query.interval = args.interval;}

        // reset to the first page, because we are issuing a new query
        // and there may only be 1 page of results
        _query.start = this.query.start = 0;

        this.query.view = args.view;

        // don't issue search when the first change:search event is fired
        if (!componentInitialized || this.loading) { return; }

        this.getData(true);
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

      // watch for window resizing and update the info column width
      // this is only registered when the user has not set widths for any
      // columns && the info column is visible
      windowResizeEvent = () => {
        if (resizeTimeout) { this.$timeout.cancel(resizeTimeout); }
        resizeTimeout = this.$timeout(() => {
          this.mapHeadersToFields();
        }, 300);
      };
      this.$window.addEventListener('resize', windowResizeEvent);
    } /* /$onInit */

    /* fired when controller's containing scope is destroyed */
    $onDestroy() {
      holdingClick = false;
      componentInitialized = false;
      colResizeInitialized = false;

      if (timeout) { this.$timeout.cancel(timeout); }

      $('#sessionsTable').colResizable({ disable:true });

      this.$window.removeEventListener('resize', windowResizeEvent);
    }

    /* Initializes resizable columns */
    initializeColResizable() {
      colResizeInitialized = true;
      $('#sessionsTable').colResizable({
        minWidth        : 50,
        headerOnly      : true,
        resizeMode      : 'overflow',
        disabledColumns : [0],
        hoverCursor     : 'col-resize',
        onResize        : (event, column, colIdx) => {
          this.$scope.$apply(() => {
            let header = this.headers[colIdx-1];
            if (header) {
              header.width = column.w;
              this.colWidths[header.dbField] = column.w;

              this.saveColumnWidths();

              this.mapHeadersToFields();
            }
          });
        }
      });
    }


    /* data retrieve/setup/update ------------------------------------------ */
    /**
     * Makes a request to the Session Service to get the list of sessions
     * that match the query parameters
     * @param {bool} updateTable Whether the table needs updating
     */
    getData(updateTable) {
      this.loading  = true;
      this.error    = false;

      this.stickySessions = []; // clear sticky sessions

      this.query.fields   = ['ipProtocol'];

      this.mapHeadersToFields();

      // set the fields to retrieve from the server for each session
      if (this.headers) {
        for (let i = 0; i < this.headers.length; ++i) {
          let field = this.headers[i];
          if (field.children) {
            for (let j = 0; j < field.children.length; ++j) {
              let child = field.children[j];
              if (child) { this.query.fields.push(child.dbField); }
            }
          } else {
            this.query.fields.push(field.dbField);
          }
        }
      }

      this.SessionService.get(this.query)
        .then((response) => {
          this.error      = false;
          this.loading    = false;
          this.sessions   = response.data;
          this.mapData    = response.data.map;
          this.graphData  = response.data.graph;

          if (updateTable) { this.reloadTable(); }

          if (parseInt(this.$routeParams.openAll) === 1) {
            this.openAll();
          }

          // initialize resizable columns now that there is data
          if (!colResizeInitialized) { this.initializeColResizable(); }
        })
        .catch((error) => {
          this.error = error;
          this.reloadTable(); // sets loading to false
        });
    }

    /* Gets the current user's settings */
    getUserSettings() {
      this.UserService.getSettings()
         .then((settings) => {
           this.settings = settings;

           // if settings has custom sort field and the custom sort field
           // exists in the table headers, apply it
           if (this.settings && this.settings.sortColumn !== 'last' &&
              this.tableState.visibleHeaders.indexOf(this.settings.sortColumn) > -1) {
             this.query.sorts = [[this.settings.sortColumn, this.settings.sortDirection]];
             this.tableState.order = this.query.sorts;
           }

           // IMPORTANT: kicks off the initial search query
           if (!this.settings.manualQuery || componentInitialized) { this.getData(); }
           else {
             this.loading  = false;
             this.error    = 'Now, issue a query!';
           }

           componentInitialized = true;
         })
         .catch((error) => {
           this.error = error;
         });
    }

    /* Gets the state of the table (sort order and column order/visibility) */
    getTableState() {
      this.SessionService.getState('sessionsNew')
         .then((response) => {
           this.tableState = response.data;
           if (Object.keys(this.tableState).length === 0 ||
              !this.tableState.visibleHeaders || !this.tableState.order) {
             this.tableState = defaultTableState;
           } else if (this.tableState.visibleHeaders[0] === '') {
             this.tableState.visibleHeaders.shift();
           }

           // update the sort order for the session table query
           this.query.sorts = this.tableState.order;

           this.FieldService.get()
             .then((result) => {
               this.fields = result;

               this.setupFields();

               this.getUserSettings(); // IMPORTANT: kicks off the initial search query!
             }).catch((error) => {
               this.loading  = false;
               this.error    = error;
             });
         }).catch((error) => {
           this.loading  = false;
           this.error    = error;
         });
    }

    /* Gets the column widths of the table if they exist */
    getColumnWidths() {
      this.SessionService.getState('sessionsColWidths')
        .then((response) => {
          this.colWidths = response.data || {};
        })
        .catch(() => { this.colWidths = {}; });
    }

    /* Gets the current user's custom column configurations */
    getCustomColumnConfigurations() {
      this.UserService.getColumnConfigs()
        .then((response) => {
          this.colConfigs = response;
        })
        .catch((error) => {
          this.colConfigError = error.text;
        });
    }

    /**
     * Saves the table state
     * @param {bool} stopLoading Whether to stop the loading state when promise returns
     */
    saveTableState(stopLoading) {
      this.SessionService.saveState(this.tableState, 'sessionsNew')
         .then(() => {
           if (stopLoading) { this.loading = false; }
         })
         .catch((error) =>  { this.error = error; });
    }

    /**
     * Saves the column widths to the db
     */
    saveColumnWidths() {
      this.SessionService.saveState(this.colWidths, 'sessionsColWidths')
         .catch((error) => { this.error = error; });
    }

    /**
     * Finds a field object given its id
     * @param {string} fieldId  The unique id of the field
     * @return {Object} field   The field object
     */
    getField(fieldId) {
      for (let key in this.fields) {
        if (this.fields.hasOwnProperty(key)) {
          let item = this.fields[key];
          if (item.dbField === fieldId) {
            return item;
          }
        }
      }

      return undefined;
    }

    /**
     * Maps visible column headers to their corresponding fields
     */
    mapHeadersToFields() {
      this.headers = [];
      this.sumOfColWidths = 80;

      if (!this.colWidths) { this.colWidths = {}; }

      for (let i = 0, len = this.tableState.visibleHeaders.length; i < len; ++i) {
        let headerId  = this.tableState.visibleHeaders[i];
        let field     = this.getField(headerId);

        if (field) {
          field.width = this.colWidths[headerId] || field.width || 100;
          if (field.dbField === 'info') { // info column is super special
            // reset info field width to default so it can always be recalculated
            // to take up all of the rest of the space that it can
            field.width = defaultInfoColWidth;
          } else { // don't account for info column's width because it changes
            this.sumOfColWidths += field.width;
          }
          this.headers.push(field);
        }
      }

      this.sumOfColWidths = Math.round(this.sumOfColWidths);

      this.calculateInfoColumnWidth(defaultInfoColWidth);
      this.$scope.$broadcast('$$rebind::refreshHeaders');
    }

    /**
     * Calculates the info column's width based on the width of the window
     * If the info column is visible, it should take up whatever space is left
     * @param infoColWidth
     */
    calculateInfoColumnWidth(infoColWidth) {
      this.showFitButton = false;
      if (!this.colWidths) { return; }
      let windowWidth = window.innerWidth - 45; // account for right and left margins
      if (this.tableState.visibleHeaders.indexOf('info') >= 0) {
        let fillWithInfoCol = windowWidth - this.sumOfColWidths;
        let newTableWidth = this.sumOfColWidths;
        for (let i = 0, len = this.headers.length; i < len; ++i) {
          if (this.headers[i].dbField === 'info') {
            let newInfoColWidth = Math.max(fillWithInfoCol, infoColWidth);
            this.headers[i].width = newInfoColWidth;
            newTableWidth += newInfoColWidth;
          }
        }
        this.tableWidth = newTableWidth;
      } else {
        this.tableWidth = this.sumOfColWidths;
        // display a button to fit the table to the width of the window
        // if the table is more than 10 pixels larger or smaller than the window
        if (Math.abs(this.tableWidth - windowWidth) > 10) {
          this.showFitButton = true;
        }
      }
    }

    /**
     * Sets up the fields for the column visibility typeahead and column headers
     * by adding custom columns to the visible columns list and table
     */
    setupFields() {
      for (let key in customCols) {
        if (customCols.hasOwnProperty(key)) {
          this.fields[key] = customCols[key];
          let children = this.fields[key].children;
          // expand all the children
          for (let c in children) {
            // (replace fieldId with field object)
            if (children.hasOwnProperty(c)) {
              if (typeof children[c] !== 'object') {
                children[c] = this.getField(children[c]);
              }
            }
          }
        }
      }

      // convert fields map to array (for ng-repeat with filter and group)
      // and remove duplicate fields (e.g. 'host.dns' & 'dns.host')
      let existingFieldsLookup = {}; // lookup map of fields in fieldsArray
      this.fieldsArray = [];
      for (let f in this.fields) {
        if (this.fields.hasOwnProperty(f)) {
          let field = this.fields[f];
          if (!existingFieldsLookup.hasOwnProperty(field.exp)) {
            existingFieldsLookup[field.exp] = field;
            this.fieldsArray.push(field);
          }
        }
      }
    }

    /* reloads the data in the table (even one time bindings) */
    reloadTable() {
      // disable resizable columns so it can be initialized after table reloads
      $('#sessionsTable').colResizable({ disable:true });
      colResizeInitialized = false;

      this.loading      = true;
      this.showSessions = false;
      this.$scope.$broadcast('$$rebind::refreshTable');

      this.$timeout(() => {
        this.loading      = false;
        this.showSessions = true;
        this.$scope.$broadcast('$$rebind::refreshTable');

        this.initializeColResizable();
      });
    }


    /* exposed functions --------------------------------------------------- */
    /* SESSION DETAIL */
    /**
     * Toggles the display of the session detail for each session
     * @param {Object} session The session to expand, collapse details
     */
    toggleSessionDetail(session) {
      session.expanded = !session.expanded;

      if (session.expanded) {
        this.stickySessions.push(session);
      } else {
        let index = this.stickySessions.indexOf(session);
        if (index >= 0) { this.stickySessions.splice(index, 1); }
      }
    }

    /* Opens up to 10 session details in the table */
    openAll() {
      // opening too many session details at once is bad!
      if (this.sessions.data.length > 10) {
        alert('You\'re trying to open too many session details at once! I\'ll only open the first 10 for you, sorry!');
      }

      let len = Math.min(this.sessions.data.length, 10);

      for (let i = 0; i < len; ++i) {
        this.toggleSessionDetail(this.sessions.data[i]);
      }

      // unset open all for future queries
      this.$location.search('openAll', null).replace();
    }


    /* TABLE SORTING */
    /**
     * Determines if the table is being sorted by specified column
     * @param {string} id The id of the column
     */
    isSorted(id) {
      for (let i = 0; i < this.tableState.order.length; ++i) {
        if (this.tableState.order[i][0] === id) { return i; }
      }

      return -1;
    }

    /**
     * Sorts the sessions by the clicked column
     * (if the user issues a click less than 300ms long)
     * @param {Object} event  The click event that triggered the sort
     * @param {string} id     The id of the column to sort by
     */
    sortBy(event, id) {
      // if the column click was a click and hold/drag, don't issue new query
      if (holdingClick) { return; }

      if (this.isSorted(id) >= 0) {
        // the table is already sorted by this element
        if (!event.shiftKey) {
          let item = this.toggleSortOrder(id);
          this.tableState.order = [item];
        } else {
          // if it's a shift click - toggle the order between 3 states:
          // 'asc' -> 'desc' -> removed from sorts
          if (this.getSortOrder(id) === 'desc' && this.tableState.order.length > 1) {
            for (let i = 0, len = this.tableState.order.length; i < len; ++i) {
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

      this.saveTableState();

      this.getData(true);
    }

    /**
     * Determines the sort order of a column
     * @param {string} id     The unique id of the column
     * @return {string} order The sort order of the column
     */
    getSortOrder(id) {
      for (let i = 0, len = this.tableState.order.length; i < len; ++i) {
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
      for (let i = 0, len = this.tableState.order.length; i < len; ++i) {
        let item = this.tableState.order[i];
        if (item[0] === id) {
          if (item[1] === 'asc') { item[1] = 'desc'; }
          else { item[1] = 'asc'; }
          return item;
        }
      }
    }

    /**
     * Updates the sort field and order if the current sort column has
     * been removed
     * @param {string} id The id of the sort field
     * @returns {boolean} Whether the table requires a data reload
     */
    updateSort(id) {
      let updated = false;

      // update the sort field and order if the table was being sorted by that field
      let sortIndex = this.isSorted(id);
      if (sortIndex > -1) {
        updated = true; // requires a data reload because the sort is different
        // if we are sorting by this column, remove it
        if (this.tableState.order.length === 1) {
          // this column is the only column we are sorting by
          // so reset it to the first sortable field in the visible headers
          let newSort;
          for (let i = 0, len = this.headers.length; i < len; ++i) {
            let header = this.headers[i];
            // find the first sortable column
            if ((!header.children || (header.children && header.sortBy)) &&
               (header.dbField !== id && header.sortBy !== id)) {
              newSort = header.sortBy || header.dbField;
              break;
            }
          }

          // if there are no columns to sort by, sort by start time
          if (!newSort) { newSort = 'firstPacket'; }

          this.tableState.order = [[newSort,'asc']];
        } else {
          // this column is one of many we are sorting by, so just remove it
          this.tableState.order.splice(sortIndex, 1);
        }

        // update the query
        this.query.sorts = this.tableState.order;
      }

      return updated;
    }

    /**
     * Sets holdingClick to true if the user holds the click for
     * 300ms or longer. If the user clicks and holds/drags, the
     * sortBy function returns immediately and does not issue query
     */
    mouseDown() {
      holdingClick = false;
      timeout = this.$timeout(() => {
        holdingClick = true;
      }, 300);
    }

    /**
     * Sets holdingClick to false 500ms after mouse up
     */
    mouseUp() {
      this.$timeout.cancel(timeout);
      this.$timeout(() => {
        holdingClick = false;
      }, 500);
    }


    /* COLUMN INTERACTIONS */
    /**
     * Fires when column drop is completed
     * @param {number} newIndex The index of the drop target
     * @param {object} obj      The object dropped
     */
    onDropComplete(newIndex, obj) {
      // set to the first position if dropped on far left column
      if (!newIndex || newIndex < 0) { newIndex = 0; }

      let draggedIndex = this.tableState.visibleHeaders.indexOf(obj.dbField);

      // reorder the visible headers
      if (newIndex >= this.tableState.visibleHeaders.length) {
        let k = newIndex - this.tableState.visibleHeaders.length;
        while ((k--) + 1) {
          this.tableState.visibleHeaders.push(undefined);
        }
      }

      this.tableState.visibleHeaders.splice(newIndex, 0,
         this.tableState.visibleHeaders.splice(draggedIndex, 1)[0]);

      this.mapHeadersToFields();

      this.reloadTable();

      this.saveTableState();
    }

    /**
     * Determines a column's visibility given its id
     * @param {string} id       The id of the column
     * @return {number} number  The index of the visible header
     */
    isVisible(id) {
      return this.tableState.visibleHeaders.indexOf(id);
    }

    /**
     * Toggles the visibility of a column given its id
     * @param {string} id   The id of the column to show/hide (toggle)
     * @param {string} sort Option sort id for columns that have sortBy
     */
    toggleVisibility(id, sort) {
      this.colVisOpen = false; // close the column visibility dropdown
      this.loading    = true;
      let reloadData  = false;

      let index = this.isVisible(id);

      if (index >= 0) { // it's visible
        // remove it from the visible headers list
        this.tableState.visibleHeaders.splice(index,1);
        reloadData = this.updateSort(sort || id);
      } else { // it's hidden
        reloadData = true; // requires a data reload
        // add it to the visible headers list
        this.tableState.visibleHeaders.push(id);
      }

      this.mapHeadersToFields();

      if (reloadData) { this.getData(true); } // need data from the server (reloads table)
      else { this.reloadTable(); } // have all the data, just need to reload the table

      this.saveTableState(true);
    }

    /**
     * Loads a previously saved custom column configuration and
     * reloads table and table data
     * If no index is given, loads the default columns
     * @param {int} index The index in the array of the column config to load
     */
    loadColumnConfiguration(index) {
      this.colConfigsOpen = false;  // close the column config dropdown

      if (this.isSameAsVisible(index)) { return; }

      this.loading = true;

      if (!index && index !== 0) {
        this.tableState.visibleHeaders  = defaultTableState.visibleHeaders.slice();
        this.tableState.order           = defaultTableState.order.slice();
      } else {
        this.tableState.visibleHeaders  = this.colConfigs[index].columns.slice();
        this.tableState.order           = this.colConfigs[index].order.slice();
      }

      this.mapHeadersToFields();

      this.query.sorts = this.tableState.order;

      this.saveTableState();

      this.getData(true);
    }

    /* Saves a custom column configuration */
    saveColumnConfiguration() {
      if (!this.newColConfigName) {
        this.colConfigError = 'You must name your new column configuration';
        return;
      }

      let data = {
        name    : this.newColConfigName,
        columns : this.tableState.visibleHeaders.slice(),
        order   : this.tableState.order.slice()
      };

      this.UserService.createColumnConfig(data)
        .then((response) => {
          data.name = response.name; // update column config name

          this.colConfigs.push(data);

          this.newColConfigName = null;
          this.colConfigsOpen   = false;
          this.colConfigError   = false;
        })
        .catch((error) => {
          this.colConfigError = error.text;
        });
    }

    /**
     * Deletes a previously saved custom column configuration
     * @param {string} name The name of the column config to remove
     * @param {int} index   The index in the array of the column config to remove
     */
    deleteColumnConfiguration(name, index) {
      this.UserService.deleteColumnConfig(name)
        .then((response) => {
          this.colConfigs.splice(index, 1);
          this.colConfigError = false;
        })
        .catch((error) => {
          this.colConfigError = error.text;
        });
    }

    /**
     * Determines whether the custom column configuration is the same as the
     * visible columns in the table
     * @param {int} index The index in the array of the column config to compare
     * @returns {boolean} Whether the custom column config is the same as the
     *                    visible columns in the table
     */
    isSameAsVisible(index) {
      let customCols;

      if (index === undefined) {
        customCols = defaultTableState.visibleHeaders.slice();
      } else { customCols = this.colConfigs[index].columns; }

      let tableCols   = this.tableState.visibleHeaders;

      if (customCols === tableCols)                   { return true; }
      if (customCols === null || tableCols === null)  { return false; }
      if (customCols.length !== tableCols.length)     { return false; }

      for (let i = 0, len = customCols.length; i < len; ++i) {
        if (customCols[i] !== tableCols[i]) { return false; }
      }

      return true;
    }

    /**
     * Opens the spi graph page in a new browser tab
     * @param {string} fieldID The field id (dbField) to display spi graph data for
     */
    openSpiGraph(fieldID) {
      this.SessionService.openSpiGraph(fieldID);
    }

    /* Fits the table to the width of the current window size */
    fitTable() {
      // disable resizable columns so it can be initialized after columns are resized
      $('#sessionsTable').colResizable({ disable:true });
      colResizeInitialized = false;

      let windowWidth   = window.innerWidth - 45; // account for right and left margins
      let leftoverWidth = windowWidth - this.sumOfColWidths;
      let percentChange = 1 + (leftoverWidth/this.sumOfColWidths);

      for (let i = 0, len = this.headers.length; i < len; ++i) {
        let header    = this.headers[i];
        let newWidth  = Math.floor(header.width * percentChange);
        header.width  = newWidth;
        this.colWidths[header.dbField] = newWidth;
      }

      this.tableWidth = windowWidth;
      this.showFitButton = false;

      this.saveColumnWidths();

      this.$timeout(() => { this.initializeColResizable(); });
    }


    /* UNIQUE VALUES */
    /**
     * Open a page to view unique values for different fields
     * @param {string} exp    The field to get unique values for
     * @param {number} counts 1 or 0 whether to include counts of the values
     */
    exportUnique(exp, counts) {
      this.SessionService.exportUniqueValues(exp, counts);
    }


    /* MISC */
    /**
     * Determines whether a reference is an array
     * @param {?} value   Reference to check
     * @returns {boolean} True if value is an array
     */
    isArray(value) {
      return angular.isArray(value);
    }

  }

  SessionListController.$inject = ['$scope', '$window', '$timeout', '$location',
    '$routeParams', '$anchorScroll', 'SessionService', 'FieldService', 'UserService'];


  angular.module('moloch')
    .component('session', {
      template  : require('../templates/session.list.html'),
      controller: SessionListController
    });

})();
