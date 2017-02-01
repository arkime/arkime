(function() {

  'use strict';

  /**
   * @class SessionFieldController
   * @classdesc Interacts with all clickable session fields
   *
   * @example
   * '<session-field field="::fieldObj" expr="src.ip"
   *   value="{{::sessionObj.field}}" session="::sessionObj"
   *   parse="true" stringify="true"></session-field>'
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

    /* Callback when component is mounted and ready */
    $onInit() {
      // exit out asap: only display fields that have a value or children
      if ((this.value === undefined || this.value === '') &&
         (!this.field || !this.field.children)) { return; }

      // setup parse flag if it's a string
      if (typeof this.parse === 'string') {
        this.parse = this.parse === 'true';
      }

      // setup stringify flag if it's a string
      if (typeof this.stringify === 'string') {
        this.stringify = this.stringify === 'true';
      }

      // only parse values if we know how to (requires field param)
      if (this.field) { this.parseValue(); }
      // otherwise get the corresponding field definition before parsing
      else { this.getField(); }
    }

    /* exposed functions --------------------------------------------------- */
    /**
     * Triggered when a field's menu item is clicked
     * Emits an event to add an expression to the query in the search bar
     * @param {string} field  The field name
     * @param {string} value  The field value
     * @param {string} op     The relational operator
     */
    fieldClick(field, value, op) {
      // close the dropdown
      this.isopen = false;

      // for values required to be strings in the search expression
      if (this.stringify) { value = `"${value}"`; }

      let fullExpression = `${field} ${op} ${value}`;

      this.$scope.$emit('add:to:search', { expression: fullExpression });
    }

    /**
     * Triggered when a time field is clicked
     * Emits an event to update the time constraints in the search bar
     * @param {string} field The field name ('starttime' || 'stoptime')
     * @param {string} value The value of the field
     */
    timeClick(field, value) {
      let result = {};

      // starttime & stoptime are the search bar time constraints
      if (field === 'starttime') { result.start = value; }
      if (field === 'stoptime')  { result.stop  = value; }

      if (result.start || result.stop) {
        this.$scope.$emit('change:time', result);
      }
    }

    /**
     * Toggles the dropdown menu for a field
     * If the dropdown menu is opened for the first time, get more menu options
     * @param {object} $event The click event
     */
    toggleDropdown($event) {
      $event.preventDefault();
      $event.stopPropagation();

      this.isopen = !this.isopen;

      if (this.isopen && !this.molochClickables) {
        this.ConfigService.getMolochClickables()
        .then((response) => {
          this.molochClickables = response;

          if (Object.keys(this.molochClickables).length !== 0) {
            // add items to the menu if they exist
            this.buildMenu();
          }
        });
      }
    }

    /* utility functions --------------------------------------------------- */
    /* Retrieves fields and finds the corresponding field
     * Should only get fields if field binding was not passed into directive
     * (like from session detail) */
    getField() {
      this.FieldService.get()
        .then((response) => {
          // set the field object for the view and parsing
          this.field = response[this.expr];

          // if the value has not been parsed, parse it!
          if (!this.parsed) { this.parseValue(); }
        });
    }

    /**
     * Gets info to display the menu for a field
     * @returns {Object} The info to be displayed in the menu
     */
    getInfo() {
      if (!this.field) { return { category: [] }; }

      if (Array.isArray(this.field.category)) {
        return { field:this.expr, category:this.field.category, info:this.field };
      } else {
        return { field:this.expr, category:[this.field.category], info:this.field };
      }
    }

    /* Parses a session field value based on its type */
    parseValue() {
      if (!this.field || !this.value) { return; }

      // TODO: this goes away with ES5
      if (this.session && this.field.dbField === 'a1' && this.session['tipv61-term']) {
        this.expr   = 'tipv6.src';
        this.field  = {dbField:'tipv61-term',exp:'tipv6.src',friendlyName:'IPv6 Src',group:'general',help:'Temporary IPv6 Source',portField:'p1',transform:'ipv6ToHex',type:'lotermfield'};
        this.value  = this.session['tipv61-term'];
      } else if (this.session && this.field.dbField === 'a2' && this.session['tipv62-term']) {
        this.expr   = 'tipv6.dst';
        this.field  = {dbField:'tipv62-term',exp:'tipv6.dst',friendlyName:'IPv6 Dst',group:'general',help:'Temporary IPv6 Destination',portField:'p2',transform:'ipv6ToHex',type:'lotermfield'};
        this.value  = this.session['tipv62-term'];
      }

      this.parsed = {
        queryVal: this.value,
        value   : this.value
      };

      // the parsed value is always an array
      if (!Array.isArray(this.value)) { this.parsed = [this.parsed]; }

      for (let i = 0, len = this.parsed.length; i < len; ++i) {
        let val   = this.parsed[i].value;
        let qVal  = this.parsed[i].queryVal;

        switch (this.field.type) {
          case 'seconds':
            this.time = true;
            qVal  = val; // save original value as the query value
            val   = this.$filter('timezone-date')(val, this.timezone);
            let dateFormat = 'yyyy/MM/dd HH:mm:ss';
            if (this.timezone === 'gmt') {
              dateFormat = 'yyyy/MM/dd HH:mm:ss\'Z\'';
            }
            val = this.$filter('date')(val, dateFormat);
            break;
          case 'ip':
            val   = this.$filter('extractIPString')(val);
            qVal  = val; // save the parsed value as the query value
            break;
          case 'lotermfield':
            if (this.field.dbField === 'tipv61-term' ||
                this.field.dbField === 'tipv62-term') {
              val   = this.$filter('extractIPv6String')(val);
              qVal  = val; // don't save original value
            }
            break;
          case 'termfield':
            if (this.field.dbField === 'prot-term') {
              val   = this.$filter('protocol')(val);
              qVal  = val; // save the parsed value as the query value
            }
            break;
          case 'integer':
            if (this.field.category !== 'port') {
              qVal  = val; // save original value as the query value
              val   = this.$filter('number')(val, 0);
            }
            break;
        }

        this.parsed[i].value    = val;  // update parsed value in array
        this.parsed[i].queryVal = qVal; // update query value in array
      }
    }

    /* Builds the dropdown menu items to display */
    buildMenu() {
      if (!this.parsed[0].value || !this.molochClickables) { return; }

      let info  = this.getInfo();
      let text  = this.parsed[0].value.toString();
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
        // the session object
        // [required for fields with children]
        session   : '<',

        // the field object that describes the field (for table)
        // [required for fields in table]
        field     : '<',

        // the query expression to be put in the search
        // [required]
        expr      : '@',

        // the value of the session field or undefined if field has children
        // [required for fields without children]
        value     : '@',

        // whether to parse the value
        // [optional, default is false]
        parse     : '@',

        // whether to stringify the value in the search expression
        // [optional, default is false]
        stringify : '@',

        //  whether the dropdown should drop down from the left
        // [optional, default is false]
        pullLeft  : '@',

        // what timezone date fields should be in ('gmt' or 'local')
        // [required for time values]
        timezone  : '@'
      }
    });

})();
