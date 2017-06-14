(function() {

  'use strict';

  let timeout;

  /**
   * @class SpigraphController
   * @classdesc Interacts with moloch stats page
   * @example
   * '<moloch-spigraph></moloch-spigraph>'
   */
  class SpigraphController {

    /**
     * Initialize global variables for this controller
     * @param $scope          Angular application model object
     * @param $timeout        Angular's wrapper for window.setTimeout
     * @param $location       Exposes browser address bar URL (based on the window.location)
     * @param $routeParams    Retrieve the current set of route parameters
     * @param SpigraphService Transacts stats with the server
     * @param FieldService    Retrieves available fields from the server
     * @param UserService     Transacts users and user data with the server
     *
     * @ngInject
     */
    constructor($scope, $timeout, $location, $routeParams,
                SpigraphService, FieldService, UserService) {
      this.$scope             = $scope;
      this.$timeout           = $timeout;
      this.$location          = $location;
      this.$routeParams       = $routeParams;
      this.SpigraphService    = SpigraphService;
      this.FieldService       = FieldService;
      this.UserService        = UserService;
    }

    /* Callback when component is mounted and ready */
    $onInit() {
      this.UserService.getSettings()
        .then((response) => {
          this.settings = response; 
          if (this.settings.timezone === undefined) {
            this.settings.timezone = 'local';
          }
        })
        .catch((error) => { this.settings = { timezone:'local' }; });

      this.FieldService.get(true)
        .then((response) => {
          this.fields = response.concat([{dbField: 'ip.dst:port', exp: 'ip.dst:port'}])
                                .filter(function(a) {return a.dbField !== undefined;})
                                .sort(function(a,b) {return (a.exp > b.exp?1:-1);}); 
        });

      // load route params
      this.query        = {};
      this.query.field  = this.$routeParams.field       || 'no';
      this.query.size   = this.$routeParams.size        || '20';
      this.sortBy       = this.$routeParams.sort        || 'graph';
      this.graphType    = this.$routeParams.graphType   || 'lpHisto';
      this.seriesType   = this.$routeParams.seriesType  || 'bars';

      if (this.sortBy === 'graph') { this.query.sort = this.graphType; }
      else { this.query.sort = this.sortBy; }

      this.refresh      = '0';
      this.items        = [];

      this.$scope.$on('change:search', (event, args) => {
        if (args.startTime && args.stopTime) {
          this.query.startTime  = args.startTime;
          this.query.stopTime   = args.stopTime;
          this.query.date       = null;
        } else if (args.date) {
          this.query.startTime  = null;
          this.query.stopTime   = null;
          this.query.date       = args.date;
        }

        this.query.expression = args.expression;
        if (args.bounding) { this.query.bounding = args.bounding; }

        this.loadData(true);
      });

      this.$scope.$on('change:time', (event, args) => {
        this.query.startTime  = args.start;
        this.query.stopTime   = args.stop;
        this.query.date       = null;

        // notify children (namely search component)
        this.$scope.$broadcast('update:time', args);

        this.loadData(true);
      });

      this.$scope.$on('change:histo:type', (event, newType) => {
        this.graphType = newType;
        if (this.sortBy === 'graph') {
          this.query.sort = this.graphType;
        }
        this.$location.search('graphType', this.graphType);

        this.loadData(true);

        // update all the other graphs
        this.$scope.$broadcast('update:histo:type', newType);
      });

      this.$scope.$on('change:series:type', (event, newType) => {
        // update all the other graphs
        this.$scope.$broadcast('update:series:type', newType);
      });

      // watch for additions to search parameters from session detail or map
      this.$scope.$on('add:to:search', (event, args) => {
        // notify children (namely expression typeahead)
        this.$scope.$broadcast('add:to:typeahead', args);
      });

      // watch for map events
      this.$scope.$on('open:maps', () => {
        this.openMaps = true;
        this.$scope.$broadcast('open:map');
      });
      this.$scope.$on('close:maps', () => {
        this.openMaps = false;
        this.$scope.$broadcast('close:map');
      });
      this.$scope.$on('toggle:src:dst', (event, state) => {
        this.$scope.$broadcast('update:src:dst', state);
      });

      // watch for the url parameters to change and update the page
      // size, field, and sort parameters are managed by the spigraph component
      this.$scope.$on('$routeUpdate', (event, current) => {
        let change = false;

        let size = current.params.size || '20';
        if (size !== this.query.size) {
          change = true;
          this.query.size = size;
        }

        let field = current.params.field || 'no';
        if (field !== this.query.field) {
          change = true;
          this.query.field = field;
        }

        let sort = current.params.sort || 'graph';
        if (sort !== this.sortBy) {
          change = true;
          this.sortBy = sort;
          if (sort === 'graph') {
            this.query.sort = this.graphType;
          } else {
            this.query.sort = sort;
          }
        }

        let graphType = current.params.graphType || 'lpHisto';
        if (current.params.graphType !== this.graphType) {
          change = true;
          this.graphType = graphType;
          // update sort parameter if sorting on graph
          if (this.sortBy === 'graph') {
            this.query.sort = this.graphType;
          }
          this.$scope.$broadcast('update:histo:type', this.graphType);
        }

        if (change) { this.loadData(true); }
      });
    }

    /* fired when controller's containing scope is destroyed */
    $onDestroy() {
      if (timeout) {
        this.$timeout.cancel(timeout);
        timeout = null;
      }
    }

    loadData(reload) {
      this.loading  = true;
      this.error    = false;

      if (reload && timeout) {
        this.$timeout.cancel(timeout);
        timeout = null;
      }

      this.SpigraphService.get(this.query)
        .then((response) => {
          this.loading = false;
          this.processData(response);

          this.recordsTotal     = response.recordsTotal;
          this.recordsFiltered  = response.recordsFiltered;

          if (reload && this.refresh && this.refresh > 0) {
            // reset the timeout for the request interval
            timeout = this.$timeout(() => {
              this.shiftTime();
            }, this.refresh * 1000);
          }
        })
        .catch((error) => {
          this.loading    = false;
          this.error      = error.text || error;
          this.items      = null;
          this.mapData    = null;
          this.graphData  = null;
        });
    }


    /* exposed functions --------------------------------------------------- */
    /* fired when a field is selected from the typeahead */
    changeField() {
      this.$location.search('field', this.query.field);
      this.$scope.$broadcast('apply:expression');
    }

    /* fired when max elements input is changed */
    changeMaxElements() {
      this.$location.search('size', this.query.size);
      this.$scope.$broadcast('apply:expression');
    }

    changeSortBy() {
      if (this.sortBy === 'graph') {
        this.query.sort = this.graphType;
      } else {
        this.query.sort = this.sortBy;
      }

      this.$location.search('sort', this.sortBy);

      this.$scope.$broadcast('apply:expression');
    }

    changeRefreshInterval() {
      if (timeout) {
        this.$timeout.cancel(timeout);
        timeout = null;
      }

      if (this.refresh && this.refresh > 0) {
        this.shiftTime();
      }
    }

    shiftTime() {
      this.$scope.$broadcast('shift:time');
    }

    db2Field(dbField) {
      for (let k = 0, len = this.fields.length; k < len; k++) {
        if (dbField === this.fields[k].dbField ||
            dbField === this.fields[k].rawField) {
          return this.fields[k];
        }
      }

      return undefined;
    }

    processData(json) {
      this.mapData    = json.map;
      this.graphData  = json.graph;

      let finfo = this.db2Field(this.filed);

      for (let i = 0, len = json.items.length; i < len; i++) {
        json.items[i].type = finfo.type;
      }

      this.items = json.items;
    }

    addExpression(item) {
      let field = this.db2Field(this.query.field);
      let fullExpression = `${field.exp} == ${item.name}`;

      this.$scope.$broadcast('add:to:typeahead', { expression: fullExpression});
    }

    /**
     * Displays the field.exp instead of field.dbField in the field typeahead
     * @param {string} value The dbField of the field
     */
    formatField(value) {
      for (let i = 0, len = this.fields.length; i < len; i++) {
        if (value === this.fields[i].dbField) {
          this.fieldObj = this.fields[i];
          return this.fields[i].exp;
        }
      }
    }

  }

  SpigraphController.$inject = ['$scope','$timeout','$location','$routeParams',
    'SpigraphService','FieldService','UserService'];

  /**
   * Moloch Spigraph Directive
   */
  angular.module('moloch')
     .component('molochSpigraph', {
       template  : require('html!./spigraph.html'),
       controller: SpigraphController
     });

})();
