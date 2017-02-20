(function() {

  'use strict';

  /**
   * @class SpigraphController
   * @classdesc Interacts with moloch stats page
   * @example
   * '<moloch-spigraph></moloch-spigraph>'
   */
  class SpigraphController {

    /**
     * Initialize global variables for this controller
     * @param SpigraphService  Transacts stats with the server
     * @param FieldService        Retrieves available fields from the server
     * @param UserService         Transacts users and user data with the server
     *
     * @ngInject
     */
    constructor($scope, $http, SpigraphService, FieldService, UserService) {
      this.$scope             = $scope;
      this.$http              = $http;
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
        .catch((error)   => {this.settings = {timezone: 'local'}; });

      this.FieldService.get(true)
        .then((response) => {
          this.fields = response.concat([{dbField: "ip.dst:port", exp: "ip.dst:port"}])
                                .filter(function(a) {return a.dbField !== undefined;})
                                .sort(function(a,b) {return (a.exp > b.exp?1:-1);}); 
        })
        .catch((error)   => {this.settings = {timezone: 'local'}; });

       
      this.field       = "no";
      this.maxElements = 20;
      this.sortBy      = "count";
      this.refresh     = 0;
      this.items       = [];

      this.$scope.$on('change:search', (event, args) => {
        this.loadData(args);
      });
    }

    loadData(params) {
      params.size     = this.maxElements;
      params.field    = this.field;
      params.sort     = "lpHisto"; // ALW Fix, needs to be from the Session/Packes/Databytes selector

      this.$http({ url:'spigraph.json', method:'GET', cache:false, params: params })
        .then((response) => {
          this.processData(response.data);
        }, (error) => {
          console.log("ERROR", error);
        });
    }

    db2Field(dbField) {
      for (var k = 0; k < this.fields.length; k++) {
        if (dbField === this.fields[k].dbField ||
            dbField === this.fields[k].rawField) {
          return this.fields[k];
        }
      }
      return undefined;
    }

    processData(json) {
      this.mapData = json.map;
      this.graphData = json.graph;

      if (this.sortBy === "name") {
        json.items = json.items.sort(function (a, b) {
          return a.name.localeCompare(b.name);
        });
      }

      var finfo = this.db2Field(this.filed);

      for (var i = 0; i < json.items.length; i++) {
        json.items[i].type = finfo.type;
      }
      this.items = json.items;
    }

    addExpression(item) {
      var field = this.db2Field(this.field);
      let fullExpression = `${field.exp} == ${item.name}`;

      this.$scope.$broadcast('add:to:typeahead', { expression: fullExpression});
    }
  }

SpigraphController.$inject = ['$scope', '$http', 'SpigraphService', 'FieldService', 'UserService'];

  /**
   * Moloch Spigraph Directive
   */
  angular.module('moloch')
     .component('molochSpigraph', {
       template  : require('html!./spigraph.html'),
       controller: SpigraphController
     });

})();
