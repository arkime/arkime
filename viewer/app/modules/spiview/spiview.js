(function() {

  'use strict';

  // local variable to save query state
  let _query = {  // set query defaults:
    facets: 1,    // facets
    spi   : 'a2:100,prot-term:100,a1:100' // which spi data values to query for
  };

  let newQuery = true, openedCategories = false;

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
     * @param $scope          Angular application model object
     * @param $location       Exposes browser address bar URL (based on the window.location)
     * @param $routeParams    Retrieve the current set of route parameters
     * @param UserService     Transacts users and user data with the server
     * @param FieldService    Retrieves available fields from the server
     * @param SpiviewService  Transacts spiview data with the server
     * @param SessionService  Transacts sessions with the server
     * @ngInject
     */
    constructor($scope, $location, $routeParams,
      UserService, FieldService, SpiviewService, SessionService) {
      this.$scope         = $scope;
      this.$location      = $location;
      this.$routeParams   = $routeParams;
      this.UserService    = UserService;
      this.FieldService   = FieldService;
      this.SpiviewService = SpiviewService;
      this.SessionService = SessionService;
    }

    /* Callback when component is mounted and ready */
    $onInit() {
      this.loading  = true;
      this.query    = _query; // load saved query

      if (this.$routeParams.spi) {
        // if there's a spi param use it
        this.query.spi = _query.spi = this.$routeParams.spi;
      } else { // if there isn't, set it for future use
        this.$location.search('spi', this.query.spi);
      }

      this.getFields(); // IMPORTANT: kicks off initial query for spi data!
      this.getUserSettings();

      let initialized;
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

        this.query.view = args.view;

        // don't issue search when the first change:search event is fired
        if (!initialized) { initialized = true; return; }

        newQuery = true;
        this.getSpiData();
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


    /* data retrieve/setup/update ------------------------------------------ */
    /* Retrieves the list of fields from the server and groups them into
     * categories, then kicks of the query for spi data */
    getFields() {
      this.FieldService.get(true)
        .then((response) => {
          this.error = false;
          this.fields = response;
          this.categoryObjects = {};

          for (let i = 0, len = this.fields.length; i < len; ++i) {
            let field = this.fields[i], newField;

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
              this.categoryObjects[field.group].fields.push(newField);
              this.fields.push(newField);
            }
          }

          // sorted list of categories
          this.categoryList = Object.keys(this.categoryObjects).sort();
          this.categoryList.splice(this.categoryList.indexOf('general'), 1);
          this.categoryList.unshift('general');

          this.getSpiData(); // IMPORTANT: queries for spi data!
        })
        .catch((error) => {
          this.loading  = false;
          this.error    = error.text;
        });
    }

    /* Retrieves spiview data from the server and puts each piece of data into
     * it's appropriate category */
    getSpiData() {
      let spiParamsArray = this.query.spi.split(',');

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
          }
        }

        if (field) {
          this.getSingleSpiData(field, count);
        }
      }
    }

    /**
     * Gets spi data for the specified field and adds it to the category object
     * @param {object} field  The field to get spi data for
     * @param {int} count     The amount of spi data to query for
     */
    getSingleSpiData(field, count) {
      if (!count) { count = 100; } // default amount of spi data to retrieve

      let category = this.categoryObjects[field.group];

      if (!category.spi) { category.spi = {}; }

      // setup new spi information if it doesn't exist
      if (!category.spi[field.dbField]) {
        category.spi[field.dbField] = { active:true, field:field };
      }

      category.spi[field.dbField].loading = true;
      category.spi[field.dbField].error   = false;

      let query = {
        facets    : 1,
        spi       : `${field.dbField}:${count}`,
        date      : this.query.date,
        startTime : this.query.startTime,
        stopTime  : this.query.stopTime,
        expression: this.query.expression,
        bounding  : this.query.bounding
      };

      this.SpiviewService.get(query)
        .then((response) => {
          if (newQuery) {
            // if issuing a new query, update the map, graph protocol counts,
            // total records, and filtered records
            newQuery        = false;
            this.loading    = false;
            this.mapData    = response.map;
            this.graphData  = response.graph;
            this.protocols  = response.protocols;
            this.total      = response.recordsTotal;
            this.filtered   = response.recordsFiltered;

            this.updateProtocols();
          }

          if (!openedCategories) { this.openCategories(); }

          // only update the requested spi data
          category.spi[field.dbField].loading = false;
          category.spi[field.dbField].value   = response.spi[field.dbField];
        })
        .catch((error) => {
          // display error for the requested spi data
          category.spi[field.dbField].loading = false;
          category.spi[field.dbField].error   = error.text;
        });
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


    /* exposed functions --------------------------------------------------- */
    /**
     * Toggles the view of field spi data by updating the active state
     * or fetching new spi data as necessary
     * Also updates the spi query parameter in the url
     * @param {object} field The field to get spi data for
     */
    toggleSpiData(field) {
      let spiData;
      if (this.categoryObjects[field.group].spi) {
        spiData = this.categoryObjects[field.group].spi[field.dbField];
      }

      let addToQuery = false;

      if (spiData) { // spi data exists, so we only need to toggle active state
        spiData.active  = !spiData.active;
        addToQuery      = spiData.active;
      } else { // spi data doesn't exist, so fetch it
        addToQuery = true;
        this.getSingleSpiData(field);
      }

      // update spi query parameter by adding or removing field id
      if (addToQuery) {
        if (this.query.spi && this.query.spi !== '') {
          this.query.spi += ',';
        }
        this.query.spi += `${field.dbField}:100`;
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
      this.$location.search('spi', this.query.spi); // update url param
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
     * @param {object} field The field to get more spi data for
     */
    showMoreValues(field) {
      let count;
      if (this.query.spi.contains(field.dbField)) {
        // make sure field is in the spi query parameter
        let spiParamsArray = this.query.spi.split(',');
        for (let i = 0, len = spiParamsArray.length; i < len; ++i) {
          if (spiParamsArray[i].contains(field.dbField)) {
            let spiParam        = spiParamsArray[i].split(':');
            count = spiParam[1] = parseInt(spiParam[1]) + 100;
            spiParamsArray[i]   = spiParam.join(':');
            break;
          }
        }
        this.query.spi = spiParamsArray.join(',');
      }

      _query.spi = this.query.spi;
      this.$location.search('spi', this.query.spi); // update url param

      this.getSingleSpiData(field, count);
    }

    /**
     * Show/hide all values for a category
     * @param {string} categoryName The name of the category to toggle values for
     * @param {object} $event       The click event that triggered this function
     */
    toggleAllValues(categoryName, $event) {
      $event.preventDefault();
      $event.stopPropagation();

      let category = this.categoryObjects[categoryName];
      category.showingAll = !category.showingAll;

      for (let i = 0, len = category.fields.length; i < len; ++i) {
        let field = category.fields[i];
        let spiData = category.spi[field.dbField];
        if (spiData) { // the spi data exists in the category
          if ((spiData.active && !category.showingAll) ||
             (!spiData.active && category.showingAll)) {
            // the spi data for this field is already visible and we don't want
            // it to be, or it's NOT visible and we want it to be
            this.toggleSpiData(field);
          }
        } else if (category.showingAll) { // spi data doesn't exist in the category
          this.toggleSpiData(field);
        }
      }
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
     * @param {string} fieldID  The field id (dbField) to display unique values for
     * @param {int} counts      Whether to display the unique values with counts (1 or 0)
     */
    exportUnique(fieldID, counts) {
      this.SessionService.exportUniqueValues(fieldID, counts);
    }

  }

  SpiviewController.$inject = ['$scope','$location','$routeParams',
    'UserService','FieldService','SpiviewService','SessionService'];

  /**
   * SPI View Directive
   * Displays SPI View page
   */
  angular.module('moloch')
    .component('molochSpiview', {
      template  : require('html!./spiview.html'),
      controller: SpiviewController
    });

})();
