(function() {

  'use strict';

  let customCols = require('json!./custom.columns.json');

  /**
   * @class SessionFieldController
   * @classdesc Interacts with add session fields
   *
   * @example
   * TODO: fix this example
   * '<session-field column="col" session="session"></session-field>'
   */
  class SessionFieldController {

    /**
     * Initialize global variables for this controller
     * @param $scope          Angular application model object
     * @param ConfigService   Transacts app configurations with the server
     *
     * @ngInject
     */
    constructor($scope, $filter, $location, FieldService, ConfigService) {
      this.$scope         = $scope;
      this.$filter        = $filter;
      this.$location      = $location;
      this.FieldService   = FieldService;
      this.ConfigService  = ConfigService;
    }

    $onInit() {
      if (typeof this.parse === 'string') {
        this.parse = this.parse === 'true';
      }

      if (this.column) { this.parseValue(this.column); }

      // TODO: only do this if the user opens on dropdown menu
      this.ConfigService.getMolochClickables()
         .then((response) => {
           this.molochClickables = {
           };

           this.FieldService.get()
              .then((response) => {
                this.molochFields = response;

                if (!this.column) {
                  this.parseValue(this.molochFields[this.expr]);
                }

                this.buildMenu();
              });
         });
    }

    /* exposed functions --------------------------------------------------- */
    fieldClick(field, value, expr) {
      // TODO: if there is no expression, send it to new page
      let fullExpression = `${field} ${expr} ${value}`;

      this.$scope.$emit('add:to:search', { expression: fullExpression });
    }

    timeClick(field, value) {
      let result = {};

      if (field === 'starttime') { result.start = value; }
      if (field === 'stoptime')  { result.stop  = value; }

      if (result.start || result.stop) {
        this.$scope.$emit('change:time', result);
      }
    }


    parseValue(fieldObj) {
      this.fieldObj = fieldObj;

      this.parsed   = this.value;

      if (!this.fieldObj || !this.parse) { return; }

      switch(this.fieldObj.type) {
        case 'seconds':
          this.time   = true;
          this.value  = this.parsed;
          this.parsed = this.$filter('date')(this.parsed * 1000, 'yyyy/MM/dd HH:mm:ss');
          break;
        case 'ip':
          this.parsed = this.$filter('extractIPString')(this.parsed);
          this.value  = this.parsed;
          break;
        case 'lotermfield':
          if (this.fieldObj.dbField === 'tipv61-term' || this.fieldObj.dbField === 'tipv62-term') {
            this.parsed = this.$filter('extractIPv6String')(this.parsed);
            this.value  = this.parsed;
          }
          break;
        case 'integer':
          if (this.fieldObj.category !== 'port') {
            this.value  = this.parsed;
            this.parsed = this.$filter('number')(this.parsed, 0);
          }
          break;
      }
    }


    // TODO
    buildMenu() {

    }

  }

  SessionFieldController.$inject = ['$scope','$filter','$location',
    'FieldService','ConfigService'];

  /**
   * SessionField Directive
   * Displays session fields
   */
  angular.module('moloch')
    .component('sessionField', {
      template  : require('html!../templates/session.field.html'),
      controller: SessionFieldController,
      bindings  : {
        expr    : '<',  // the query expression to be put in the search
        value   : '<',  // the value of the session field
        session : '<',  // the session (required for custom columns)
        column  : '<',  // the column the field belongs to (for table)
        parse   : '<'   // whether to parse the value
      }
    });

})();
