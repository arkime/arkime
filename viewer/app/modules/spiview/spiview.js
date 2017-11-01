(function() {

  'use strict';

  const defaultSpi = 'a2:100,prot-term:100,a1:100';

  // local variable to save query state
  let _query = {  // set query defaults:
    facets: 1,    // facets
    spi   : defaultSpi // which spi data values to query for
  };

  let newQuery = true, openedCategories = false;

  // object to store loading categories and how many fields are loading within
  let categoryLoadingCounts = {};

  // save currently executing promise
  let pendingPromise;

  let timeout;

  /**
   * @class SpiviewController
   * @classdesc Interacts with moloch spiview page
   * @example
   * '<moloch-spiview></moloch-spiview>'
   */
  class SpiviewController {

    /* setup --------------------------------------------------------------- */
    /**
     * Initialize global variables for this controller
     * @param $q              Service to run functions asynchronously
     * @param $scope          Angular application model object
     * @param $timeout        Angular's wrapper for window.setTimeout
     * @param $routeParams    Retrieve the current set of route parameters
     * @param UserService     Transacts users and user data with the server
     * @param FieldService    Retrieves available fields from the server
     * @param SpiviewService  Transacts spiview data with the server
     * @param SessionService  Transacts sessions with the server
     * @ngInject
     */
    constructor($q, $scope, $timeout, $routeParams,
      UserService, FieldService, SpiviewService, SessionService) {
      this.$q             = $q;
      this.$scope         = $scope;
      this.$timeout       = $timeout;
      this.$routeParams   = $routeParams;
      this.UserService    = UserService;
      this.FieldService   = FieldService;
      this.SpiviewService = SpiviewService;
      this.SessionService = SessionService;
    }

    /* Callback when component is mounted and ready */
    $onInit() {
      this.loadingVisualizations = true;

      this.loading  = true;
      this.query    = _query; // load saved query

      if (this.$routeParams.spi) {
        // if there's a spi param use it
        this.query.spi = _query.spi = this.$routeParams.spi;
        this.issueQueries();
      } else { // use what's saved in the database
        this.SessionService.getState('spiview')
          .then((response) => {
            this.query.spi = response.data.visibleFields;
            if (!this.query.spi) { this.query.spi = defaultSpi; }
            this.issueQueries();
          })
          .catch((error) => {
            this.query.spi = defaultSpi;
            this.issueQueries();
          });
      }

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

        this.query.view = args.view;

        // don't issue search when the first change:search event is fired
        // fields are needed first to complete requests
        if (!this.fields) { return; }

        newQuery = true;

        if (pendingPromise) {   // if there's already a req (or series of reqs)
          this.cancelLoading(); // cancel any current requests
          timeout = this.$timeout(() => { // wait for promise abort to complete
            this.getSpiData(this.query.spi);
          }, 100);
        } else {
          this.getSpiData(this.query.spi);
        }
      });

      // watch for additions to search parameters from spi values
      this.$scope.$on('add:to:search', (event, args) => {
        // notify children (namely expression typeahead)
        this.$scope.$broadcast('add:to:typeahead', args);
      });

      // watch for changes to time parameters from graph
      this.$scope.$on('change:time', (event, args) => {
        // notify children (namely search component)
        this.$scope.$broadcast('update:time', args);
      });
    }

    /* fired when controller's containing scope is destroyed */
    $onDestroy() {
      // reset state variables
      newQuery = true;
      openedCategories = false;
      categoryLoadingCounts = {};

      if (timeout) { this.$timeout.cancel(timeout); }

      if (pendingPromise) {     // if there's  a req (or series of reqs)
        pendingPromise.abort(); // cancel current server request
        pendingPromise  = null; // reset
      }
    }


    /* data retrieve/setup/update ------------------------------------------ */
    /* issues queries to populate the page
     * IMPORTANT: kicks off initial query for spi data! */
    issueQueries() {
      this.getFields(); // IMPORTANT: kicks off initial query for spi data!
      this.getUserSettings();
      this.getSpiviewFieldConfigs();
    }

    /* Retrieves the list of fields from the server and groups them into
     * categories, then kicks of the query for spi data */
    getFields() {
      this.FieldService.get(true)
        .then((response) => {
          this.loading  = false;
          this.error    = false;
          this.fields   = response;
          this.categoryObjects = {};

          for (let i = 0, len = this.fields.length; i < len; ++i) {
            let field = this.fields[i], newField;

            field.active = false;

            if (field.noFacet || field.regex) { continue; }
            else if (field.dbField.match(/\.snow$/)) {
              newField = {
                friendlyName: field.friendlyName + ' Tokens',
                dbField     : field.dbField,
                group       : field.group,
                exp         : field.exp
              };
              field.dbField = field.dbField.replace('.snow', '.raw');
            } else if (field.rawField) {
              newField = {
                friendlyName: field.friendlyName + ' Tokens',
                dbField     : field.dbField,
                group       : field.group,
                exp         : field.exp
              };
              field.dbField = field.rawField;
            }

            if (this.categoryObjects.hasOwnProperty(field.group)) {
              // already created, just add a new field
              this.categoryObjects[field.group].fields.push(field);
            } else { // create it
              this.categoryObjects[field.group] = { fields: [ field ] };
            }

            if (newField) {
              newField.active = false;
              this.categoryObjects[field.group].fields.push(newField);
              this.fields.push(newField);
            }
          }

          // sorted list of categories for the view
          this.categoryList = Object.keys(this.categoryObjects).sort();
          this.categoryList.splice(this.categoryList.indexOf('general'), 1);
          this.categoryList.unshift('general');

          this.getSpiData(this.query.spi); // IMPORTANT: queries for spi data!
        })
        .catch((error) => {
          this.loading  = false;
          this.error    = error.text;
        });
    }

    /* restarts the page with a new query by deactivating unnecessary spi data,
       canceling any pending promises/timeouts, and issuing a new query */
    restart() {
      newQuery = true;
      openedCategories = false;
      categoryLoadingCounts = {};

      this.deactivateSpiData(); // hide any removed fields from spi url param

      if (pendingPromise) {   // if there's already a req (or series of reqs)
        this.cancelLoading(); // cancel any current requests
        timeout = this.$timeout(() => { // wait for promise abort to complete
          this.getSpiData(this.query.spi);
        }, 100);
      } else {
        this.getSpiData(this.query.spi);
      }
    }

    /**
     * Finds the category that contains the given field and (if necessary) sets
     * it up to display spi data
     * @param {object} catMap     Map of spiview field categories
     * @param {object} field      The field to setup the category for
     * @returns {object} category The updated category that the field belongs to
     */
    static setupCategory(catMap, field) {
      let category = catMap[field.group];

      category.name = field.group;

      if (!category.spi) { category.spi = {}; }

      if (!category.spi[field.dbField]) {
        category.spi[field.dbField] = { field:field };
      }

      return category;
    }

    /**
     * Counts category fields that are being loaded so the user can know
     * when there are fields in the category that are loading
     * @param {object} category The category to count
     * @param {bool} increment  Whether to increment or decrement the count
     */
    static countCategoryFieldsLoading(category, increment) {
      if (increment) {
        if (categoryLoadingCounts[category.name]) {
          ++categoryLoadingCounts[category.name];
        } else { categoryLoadingCounts[category.name] = 1; }
      } else {
        if (categoryLoadingCounts[category.name] &&
            categoryLoadingCounts[category.name] > 1) {
          --categoryLoadingCounts[category.name];
        } else { category.loading = false; }
      }
    }

    /**
     * Chains sequential promises together
     * @param {object} tasks          List or map of tasks to complete
     * @returns {promise} prevPromise The previously executed promise
     */
    static serial(tasks) {
      let prevPromise;

      for (let t in tasks) {
        if (tasks.hasOwnProperty(t)) {
          let task = tasks[t];
          if (!prevPromise) { // first task
            prevPromise = task();
          } else { // subsequent tasks
            prevPromise = prevPromise.then(task);
          }
        }
      }

      return prevPromise;
    }

    /**
     * Creates a task function to be executed in sequence
     * @param {object} field  The field to get spi data for
     * @param {int} count     The amount of spi data to query for
     * @returns {function()}  The function to be executed
     */
    createTask(field, count) {
      return () => {
        let taskDeferred = this.$q.defer();

        timeout = this.$timeout(() => { // timeout for angular to render previous data
          if (this.canceled) { taskDeferred.promise.then(angular.noop); }
          else { taskDeferred.resolve(this.getSingleSpiData(field, count)); }
        }, 100);

        return taskDeferred.promise;
      };
    }

    /**
     * Retrieves spi data from the server serially.
     * Waits for previous call to the server to return before issuing another
     * @param {string} spiQuery What to spidata to query the db for
     *                          e.g. 'fp:100,lp:200'
     */
    getSpiData(spiQuery) {
      if (!spiQuery) { return; }

      // reset loading counts for categories
      categoryLoadingCounts = {};

      this.dataLoading = true;
      this.staleData = false;
      this.canceled = false;
      this.error = false;

      let spiParamsArray = spiQuery.split(',');

      let tasks = [], category;

      // get each field from the spi query parameter and issue
      // a query for one field at a time
      for (let i = 0, len = spiParamsArray.length; i < len; ++i) {
        let param   = spiParamsArray[i];
        let split   = param.split(':');
        let fieldID = split[0];
        let count   = split[1];

        let field;

        for (let key in this.fields) {
          if (this.fields[key].dbField === fieldID) {
            field = this.fields[key];
            break;
          }
        }

        if (field) {
          category = SpiviewController.setupCategory(this.categoryObjects, field);
          
          category.isopen   = true; // open the category to display the field
          category.loading  = true; // loading is set to false in getSingleSpiData

          // count the number of fields fetched for each category
          SpiviewController.countCategoryFieldsLoading(category, true);

          let spiData = category.spi[field.dbField];

          field.active    = true;
          spiData.active  = true;
          spiData.loading = true;
          spiData.error   = false;

          tasks.push(this.createTask(field, count));
        }
      }

      if (!openedCategories) { this.openCategories(); }

      if (tasks.length) {
        // start processing tasks serially
        SpiviewController.serial(tasks)
          .then((response) => { // returns the last result in the series
            if (response && response.bsqErr) {
              this.error = response.bsqErr;
            }
            this.dataLoading = false;
            pendingPromise = null;
          })
          .catch((error) => {
            this.error = error;
            this.dataLoading = false;
            pendingPromise = null;
          });
      } else if (this.fields) {
        // if we couldn't figure out the fields to request,
        // request the default ones
        this.getSpiData(defaultSpi);
      }
    }

    /**
     * Gets spi data for the specified field and adds it to the category object
     * @param {object} field  The field to get spi data for
     * @param {int} count     The amount of spi data to query for
     */
    getSingleSpiData(field, count) {
      let category  = SpiviewController.setupCategory(this.categoryObjects, field);
      let spiData   = category.spi[field.dbField];

      // don't continue if the active flag is defined and false
      if (spiData.active !== undefined && !spiData.active) { return; }

      if (!count) { count = 100; } // default amount of spi data to retrieve

      spiData.active  = true;
      spiData.loading = true;
      spiData.error   = false;

      let query = {
        facets    : 1,
        spi       : `${field.dbField}:${count}`,
        date      : this.query.date,
        startTime : this.query.startTime,
        stopTime  : this.query.stopTime,
        expression: this.query.expression,
        bounding  : this.query.bounding,
        interval  : this.query.interval,
        view      : this.query.view
      };

      pendingPromise = this.SpiviewService.get(query);

      pendingPromise
        .then((response) => {
          SpiviewController.countCategoryFieldsLoading(category, false);

          if (response.bsqErr) { spiData.error = response.bsqErr; }

          // only update the requested spi data
          spiData.loading     = false;
          spiData.value       = response.spi[field.dbField];
          spiData.count       = count;

          if (newQuery) { // this data comes back with every request
            // we should show it in the view ASAP (on first request)
            newQuery        = false;
            this.mapData    = response.map;
            this.graphData  = response.graph;
            this.protocols  = response.protocols;
            this.total      = response.recordsTotal;
            this.filtered   = response.recordsFiltered;

            this.loadingVisualizations = false;

            this.updateProtocols();
          }
        })
        .catch((error) => {
          SpiviewController.countCategoryFieldsLoading(category, false);

          // display error for the requested spi data
          spiData.loading     = false;
          spiData.error       = error.text;

          this.loadingVisualizations = false;
        });

      return pendingPromise;
    }

    /* Retrieves the current user's settings (specifically for timezone) */
    getUserSettings() {
      this.UserService.getSettings()
        .then((settings) => {
          this.settings = settings;
        })
        .catch((error) => {
          this.error = error;
        });
    }

    /* opens categories that were opened in a previous session
       should only run once on page load */
    openCategories() {
      openedCategories = true;
      for (let key in this.categoryObjects) {
        if (this.categoryObjects.hasOwnProperty(key)) {
          if (localStorage && localStorage['spiview-collapsible']) {
            if (localStorage['spiview-collapsible'].contains(key)) {
              this.categoryObjects[key].isopen = true;
            }
          }
        }
      }
    }

    /* updates protocols and protocol counts for categories
       should only run when issuing a new query */
    updateProtocols() {
      for (let key in this.protocols) {
        if (this.protocols.hasOwnProperty(key)) {

          let category;

          if (this.categoryObjects.hasOwnProperty(key)) {
            category = this.categoryObjects[key];
          } else { // categorize special protocols that don't match category
            if (key === 'tcp' || key === 'udp' || key === 'icmp') {
              category = this.categoryObjects.general;
            } else if (key === 'smtp' || key === 'lmtp') {
              category = this.categoryObjects.email;
            }
          }

          if (category) {
            if (!category.protocols) { category.protocols = {}; }
            category.protocols[key] = this.protocols[key];
          }

        }
      }
    }

    /* deactivate spi data that is no longer in url params */
    deactivateSpiData() {
      let spiParamsArray = this.query.spi.split(',');
      for (let key in this.categoryObjects) {
        if (this.categoryObjects.hasOwnProperty(key)) {
          let category = this.categoryObjects[key];
          for (let k in category.spi) {
            if (category.spi.hasOwnProperty(k)) {
              let spiData = category.spi[k];
              if (spiData.active) {
                let inactive = true;
                for (let i = 0, len = spiParamsArray.length; i < len; ++i) {
                  // if it exists in spi url param, it's still active
                  if (spiParamsArray[i] === k) { inactive = false; }
                }
                // it's no longer in the spi url param, so it's not active
                if (inactive) { spiData.active = false; }
              }
            }
          }
        }
      }
    }


    /* exposed functions --------------------------------------------------- */
    /**
     * Toggles the view of field spi data by updating the active state
     * or fetching new spi data as necessary
     * Also updates the spi query parameter in the url
     * @param {object} field      The field to get spi data for
     * @param {bool} issueQuery   Whether to issue query for the data
     * @param {bool} saveFields   Whether to save the visible fields
     * @returns {string} spiQuery The query string for the toggled on fields
     *                            e.g. 'lp:200,fp:100'
     */
    toggleSpiData(field, issueQuery, saveFields) {
      field.active = !field.active;

      let spiData;
      if (this.categoryObjects[field.group].spi) {
        spiData = this.categoryObjects[field.group].spi[field.dbField];
      }

      let addToQuery = false, spiQuery = '';

      if (spiData) { // spi data exists, so we need to toggle active state
        spiData.active  = !spiData.active;
        addToQuery      = spiData.active;
        // if spiData was not populated with a value and it's now active
        // we need to show get the spi data from the server
        if (!spiData.value && spiData.active) { this.getSingleSpiData(field); }
      } else { // spi data doesn't exist, so fetch it
        addToQuery = true;
        if (issueQuery) { this.getSingleSpiData(field); }
      }

      // update spi query parameter by adding or removing field id
      if (addToQuery) {
        if (this.query.spi && this.query.spi !== '') {
          this.query.spi += ',';
        }
        this.query.spi += spiQuery += `${field.dbField}:100`;
      } else {
        let spiParamsArray = this.query.spi.split(',');
        for (let i = 0, len = spiParamsArray.length; i < len; ++i) {
          if (spiParamsArray[i].contains(field.dbField)) {
            spiParamsArray.splice(i, 1);
            break;
          }
        }
        this.query.spi = spiParamsArray.join(',');
      }

      _query.spi = this.query.spi;

      // save field state if method was invoked from field button click
      if (saveFields) { this.saveFieldState(); }

      return spiQuery;
    }

    /**
     * Saves the open categories to spiview-collapsible localStorage
     * @param {string} name   The name of the category
     * @param {bool} isclosed Whether the category is closed
     */
    toggleCategory(name, isclosed) {
      if (localStorage) {
        if (localStorage['spiview-collapsible']) {
          let visiblePanels = localStorage['spiview-collapsible'];
          if (isclosed) {
            let split = visiblePanels.split(',');
            for (let i = 0, len = split.length; i < len; ++i) {
              if (split[i].contains(name)) {
                split.splice(i, 1);
                break;
              }
            }
            visiblePanels = split.join(',');
          } else {
            if (!visiblePanels.contains(name)) {
              if (visiblePanels !== '') { visiblePanels += ','; }
              visiblePanels += `${name}`;
            }
          }
          localStorage['spiview-collapsible'] = visiblePanels;
        } else {
          localStorage['spiview-collapsible'] = name;
        }
      }
    }

    /**
     * Shows more values for a specific field
     * @param {object} field  The field to get more spi data for
     * @param {bool} more     Whether to display more or less values
     */
    showValues(value, more) {
      let count, field = value.field;
      if (this.query.spi.contains(field.dbField)) {
        // make sure field is in the spi query parameter
        let spiParamsArray = this.query.spi.split(',');
        for (let i = 0, len = spiParamsArray.length; i < len; ++i) {
          if (spiParamsArray[i].contains(field.dbField)) {
            let spiParam        = spiParamsArray[i].split(':');
            if (more) { count   = spiParam[1] = parseInt(spiParam[1]) + 100; }
            else      { count   = spiParam[1] = parseInt(spiParam[1]) - 100; }
            spiParamsArray[i]   = spiParam.join(':');
            break;
          }
        }
        this.query.spi = spiParamsArray.join(',');
      }

      value.count = count;

      _query.spi = this.query.spi;

      this.saveFieldState();

      this.getSingleSpiData(field, count);
    }

    /**
     * Show/hide all values for a category
     * @param {string} categoryName The name of the category to toggle values for
     * @param {bool} load           Whether to load (or unload) all values
     * @param {object} $event       The click event that triggered this function
     */
    toggleAllValues(categoryName, load, $event) {
      $event.preventDefault();
      $event.stopPropagation();

      let query     = '';
      let category  = this.categoryObjects[categoryName];

      for (let i = 0, len = category.fields.length; i < len; ++i) {
        let field = category.fields[i];
        if (category.spi && category.spi[field.dbField]) {
          let spiData = category.spi[field.dbField];
          if ((spiData.active && !load) ||
             (!spiData.active && load)) {
            // the spi data for this field is already visible and we don't want
            // it to be, or it's NOT visible and we want it to be
            this.toggleSpiData(field);
          }
        } else if (load) { // spi data doesn't exist in the category
          if (query) { query += ','; }
          query += this.toggleSpiData(field);
        }
      }

      if (load && query) { this.getSpiData(query); }

      this.saveFieldState();
    }

    /**
     * Opens the spi graph page in a new browser tab
     * @param {string} fieldID The field id (dbField) to display spi graph data for
     */
    openSpiGraph(fieldID) {
      this.SessionService.openSpiGraph(fieldID);
    }

    /**
     * Opens a new browser tab containing all the unique values for a given field
     * @param {string} exp  The field id to display unique values for
     * @param {int} counts  Whether to display the unique values with counts (1 or 0)
     */
    exportUnique(exp, counts) {
      this.SessionService.exportUniqueValues(exp, counts);
    }

    /* Cancels the loading of all server requests */
    cancelLoading() {
      if (pendingPromise) {
        pendingPromise.abort(); // cancel current server request
        pendingPromise = null; // reset
      }

      this.canceled     = true;   // indicate cancellation for future requests
      this.dataLoading  = false;
      this.staleData    = newQuery;

      // set loading to false for all categories and fields
      for (let key in this.categoryObjects) {
        if (this.categoryObjects.hasOwnProperty(key)) {
          let cat = this.categoryObjects[key];
          cat.loading = false;
          if (cat.spi) {
            for (let field in cat.spi) {
              if (cat.spi.hasOwnProperty(field)) {
                cat.spi[field].loading = false;
              }
            }
          }
        }
      }
    }

    /* Gets the current user's custom spiview fields configurations */
    getSpiviewFieldConfigs() {
      this.UserService.getSpiviewFields()
         .then((response) => {
           this.fieldConfigs = response;
         })
         .catch((error) => {
           this.fieldConfigError = error.text;
         });
    }

    /* Saves a custom spiview fields configuration */
    saveFieldConfiguration() {
      if (!this.newFieldConfigName) {
        this.fieldConfigError = 'You must name your new spiview field configuration';
        return;
      }

      let data = {
        name  : this.newFieldConfigName,
        fields: this.query.spi
      };

      this.UserService.createSpiviewFieldConfig(data)
        .then((response) => {
          data.name = response.name; // update column config name

          this.fieldConfigs.push(data);

          this.newFieldConfigName = null;
          this.fieldConfigsOpen   = false;
          this.fieldConfigError   = false;
        })
        .catch((error) => {
          this.fieldConfigError = error.text;
        });
    }

    /**
     * Deletes a previously saved custom spiview fields configuration
     * @param {string} name The name of the spiview fields config to remove
     * @param {int} index   The index in the array of the spiview fields config to remove
     */
    deleteFieldConfiguration(name, index) {
      this.UserService.deleteSpiviewFieldConfig(name)
         .then(() => {
           this.fieldConfigs.splice(index, 1);
           this.fieldConfigError = false;
         })
         .catch((error) => {
           this.fieldConfigError = error.text;
         });
    }

    /**
     * Loads a previously saved custom spiview fields configuration and
     * reloads the fields
     * If no index is given, loads the default spiview fields
     * @param {int} index The index in the array of the spiview fields configs to load
     */
    loadFieldConfiguration(index) {
      this.fieldConfigsOpen = false;  // close the field config dropdown

      if (!index && index !== 0) {
        this.query.spi = defaultSpi;
      } else {
        this.query.spi = this.fieldConfigs[index].fields;
      }

      this.saveFieldState();
      this.restart();
    }

    /* saves the visible fields */
    saveFieldState() {
      this.SessionService.saveState({visibleFields:this.query.spi}, 'spiview');
    }

  }

  SpiviewController.$inject = ['$q','$scope','$timeout','$routeParams',
    'UserService','FieldService','SpiviewService','SessionService'];

  /**
   * SPI View Directive
   * Displays SPI View page
   */
  angular.module('moloch')
    .component('molochSpiview', {
      template  : require('./spiview.html'),
      controller: SpiviewController
    });

})();
