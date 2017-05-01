(function() {

  'use strict';

  // save query parameters
  let _query = {};

  let interval;

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
     * @param $interval     Angular's wrapper for window.setInterval
     * @param $location       Exposes browser address bar URL (based on the window.location)
     * @param $routeParams    Retrieve the current set of route parameters
     * @param SpigraphService Transacts stats with the server
     * @param FieldService    Retrieves available fields from the server
     * @param UserService     Transacts users and user data with the server
     *
     * @ngInject
     */
    constructor($scope, $interval, $location, $routeParams,
                SpigraphService, FieldService, UserService) {
      this.$scope             = $scope;
      this.$interval          = $interval;
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
      this.sort         = _query.sort   = this.$routeParams.sort  || 'lpHisto';
      this.field        = _query.field  = this.$routeParams.field || 'no';
      this.maxElements  = _query.size   = this.$routeParams.size  || '20';

      this.sortBy       = 'count';
      this.refresh      = '0';
      this.items        = [];

      this.$scope.$on('change:search', (event, args) => {
        if (args.startTime && args.stopTime) {
          _query.startTime  = args.startTime;
          _query.stopTime   = args.stopTime;
          _query.date       = null;
        } else if (args.date) {
          _query.date      = args.date;
          _query.startTime = null;
          _query.stopTime  = null;
        }

        _query.expression = args.expression;
        if (args.bounding) {_query.bounding = args.bounding;}

        this.loadData();
      });

      this.$scope.$on('change:time', (event, args) => {
        _query.startTime  = args.start;
        _query.stopTime   = args.stop;
        _query.date       = null;

        // notify children (namely search component)
        this.$scope.$broadcast('update:time', args);

        this.loadData();
      });

      this.$scope.$on('change:histo:type', (event, newType) => {
        this.sort = newType;
        this.$location.search('sort', this.sort);

        // update all the other graphs
        this.$scope.$broadcast('update:histo:type', newType);
      });

      // watch for additions to search parameters from session detail or map
      this.$scope.$on('add:to:search', (event, args) => {
        // notify children (namely expression typeahead)
        this.$scope.$broadcast('add:to:typeahead', args);
      });

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
    }

    loadData(reload) {
      this.loading  = true;
      this.error    = false;

      if (reload && interval) { this.$interval.cancel(interval); }

      // build new query and save values in url parameters
      _query.size   = this.maxElements;
      this.$location.search('size', this.maxElements);

      _query.field  = this.field;
      this.$location.search('field', this.field);

      this.SpigraphService.get(_query)
        .then((response) => {
          this.loading = false;
          this.processData(response);

          this.recordsTotal     = response.recordsTotal;
          this.recordsFiltered  = response.recordsFiltered;

          if (reload && this.refresh && this.refresh > 0) {
            interval = this.$interval(() => {
              this.loadData();
            }, this.refresh * 1000);
          }
        })
        .catch((error) => {
          this.loading  = false;
          this.error    = error.text;
        });
    }

    changeSortBy() {
      if (this.sortBy === 'name') {
        this.items = this.items.sort(function (a, b) {
          return a.name.localeCompare(b.name);
        });
      } else {
        this.items = this.items.sort(function (a, b) {
          return a.count < b.count;
        });
      }
    }

    changeRefreshInterval() {
      if (interval) { this.$interval.cancel(interval); }

      if (this.refresh && this.refresh > 0) { this.loadData(true); }
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

      if (this.sortBy === 'name') {
        json.items = json.items.sort(function (a, b) {
          return a.name.localeCompare(b.name);
        });
      }

      let finfo = this.db2Field(this.filed);

      for (let i = 0, len = json.items.length; i < len; i++) {
        json.items[i].type = finfo.type;
      }

      this.items = json.items;
    }

    addExpression(item) {
      let field = this.db2Field(this.field);
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
          return this.fields[i].exp;
        }
      }
    }
  }

SpigraphController.$inject = ['$scope','$interval','$location','$routeParams',
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
