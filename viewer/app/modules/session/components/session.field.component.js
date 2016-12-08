(function() {

  'use strict';

  let customCols = require('json!./custom.columns.json');

  /**
   * @class SessionFieldController
   * @classdesc Interacts with add session fields
   *
   * @example
   * TODO: fix this example add add params to constructor documentation
   * '<session-field column="col" session="session"></session-field>'
   */
  class SessionFieldController {

    /**
     * Initialize global variables for this controller
     * @param $scope          Angular application model object
     * @param $filter         Filters format the value of an expression
     * @param $location       Exposes browser address bar URL (based on the window.location)
     * @param FieldService    Retrieves available fields from the server
     * @param ConfigService   Transacts app configurations with the server
     * @param SessionService  Transacts sessions with the server
     *
     * @ngInject
     */
    constructor($scope, $filter, $location, FieldService, ConfigService, SessionService) {
      this.$scope         = $scope;
      this.$filter        = $filter;
      this.$location      = $location;
      this.FieldService   = FieldService;
      this.ConfigService  = ConfigService;
      this.SessionService = SessionService;
    }

    $onInit() {
      // only display fields that have a value
      if (!this.value && (!this.column || !this.column.children)) { return; }

      if (typeof this.parse === 'string') {
        this.parse = this.parse === 'true';
      }

      if (typeof this.stringify === 'string') {
        this.stringify = this.stringify === 'true';
      }

      // for values required to be strings in the search expression
      if (this.stringify) { this.stringVal = '\"' + this.value + '\"'; }

      if (this.column) { this.parseValue(this.column); }

      this.FieldService.get()
        .then((response) => {
          this.molochFields = response;

          if (!this.column) {
            this.parseValue(this.molochFields[this.expr]);
          }
        });

      // TODO: only do this if the user opens on dropdown menu
      this.ConfigService.getMolochClickables()
         .then((response) => {
           this.molochClickables = response;

           this.buildMenu();
         });
    }

    /* exposed functions --------------------------------------------------- */
    fieldClick(field, value, expr) {
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

    getInfo() {
      let field = this.molochFields[this.expr];
      if (!field) { return { category: [] }; }

      if (Array.isArray(field.category)) {
        return { field: this.expr, category: field.category, info: field };
      } else {
        return { field: this.expr, category: [field.category], info: field };
      }
    }

    /* utility functions --------------------------------------------------- */
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

    buildMenu() {
      if (!this.value) { return; }

      let info  = this.getInfo();
      let text  = this.value.toString();
      let url   = text.indexOf('?') === -1 ? text :
         text.substring(0, text.indexOf('?'));
      let host  = url;
      let pos   = url.indexOf('//');

      if (pos >= 0) { host = url.substring(pos+2); }
      pos = host.indexOf('/');
      if (pos >= 0) { host = host.substring(0, pos); }
      pos = host.indexOf(':');
      if (pos >= 0) { host = host.substring(0, pos); }

      let urlParams = this.$location.search();
      let dateparams, isostart, isostop;
      this.menuItems = {};

      if (urlParams.startTime && urlParams.stopTime) {
        dateparams = `startTime=${urlParams.startTime}&stopTime=${urlParams.stopTime}`;
        isostart = new Date(parseInt(urlParams.startTime) * 1000);
        isostop  = new Date(parseInt(urlParams.stopTime) * 1000);
      }
      else {
        dateparams = `date=${urlParams.date}`;
        isostart = new Date();
        isostop  = new Date();
        isostart.setHours(isostart.getHours() - parseInt(urlParams.date));
      }

      for (let key in this.molochClickables) {
        if (this.molochClickables.hasOwnProperty(key)) {
          let rc = this.molochClickables[key];
          if ((!rc.category || info.category.indexOf(rc.category) === -1) &&
             (!rc.fields || rc.fields.indexOf(info.field) === -1)) {
            continue;
          }

          let result = this.molochClickables[key].url
             .replace('%EXPRESSION%', encodeURIComponent(urlParams.expression))
             .replace('%DATE%', dateparams)
             .replace('%ISOSTART%', isostart.toISOString())
             .replace('%ISOSTOP%', isostop.toISOString())
             .replace('%FIELD%', info.field)
             .replace('%TEXT%', text)
             .replace('%UCTEXT%', text.toUpperCase())
             .replace('%HOST%', host)
             .replace('%URL%', encodeURIComponent('http:' + url));

          let name = this.molochClickables[key].name || key;
          let value = '%URL%';
          if (rc.category === 'host') { value = '%HOST%'; }

          value = (value)
             .replace('%FIELD%', info.field)
             .replace('%TEXT%', text)
             .replace('%HOST%', host)
             .replace('%URL%', url);

          if (rc.regex) {
            if (!rc.cregex) { rc.cregex = new RegExp(rc.regex); }
            let matches = text.match(rc.cregex);
            if (matches && matches[1]) {
              result = result.replace('%REGEX%', matches[1]);
            } else {
              continue;
            }
          }

          this.menuItems[key] = { name: name, value: value, url: result };
        }
      }
    }

  }

  SessionFieldController.$inject = ['$scope','$filter','$location',
    'FieldService','ConfigService','SessionService'];

  /**
   * SessionField Directive
   * Displays session fields
   */
  angular.module('moloch')
    .component('sessionField', {
      template  : require('html!../templates/session.field.html'),
      controller: SessionFieldController,
      bindings  : {
        expr      : '<',  // the query expression to be put in the search
        value     : '<',  // the value of the session field
        session   : '<',  // the session (required for custom columns)
        column    : '<',  // the column the field belongs to (for table)
        parse     : '<',  // whether to parse the value
        stringify : '<'   // whether to stringify the value in the search expression
      }
    });

})();
