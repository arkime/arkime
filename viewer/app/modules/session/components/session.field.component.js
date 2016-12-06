(function() {

  'use strict';

  let customCols = require('json!./custom.columns.json');

  /**
   * @class SessionFieldController
   * @classdesc Interacts with add session fields
   *
   * @example
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
    constructor($sce, $scope, $filter, $location, FieldService, ConfigService) {
      this.$sce           = $sce;
      this.$scope         = $scope;
      this.$filter        = $filter;
      this.$location      = $location;
      this.FieldService   = FieldService;
      this.ConfigService  = ConfigService;
    }

    $onInit() {
      if (this.column) { this.parseValue(this.column); }

      // TODO: only do this if the user opens on dropdown menu
      this.ConfigService.getMolochClickables()
         .then((response) => {
           this.molochClickables = {
             threatqip: {
               name     :"ThreatQ",
               url      :"https://osint.threatq.com/search.php?search=%TEXT%",
               category :"ip"
             }
           };

           this.FieldService.get()
              .then((response) => {
                this.molochFields = response;

                for (let key in customCols) {
                  if (customCols.hasOwnProperty(key)) {
                    this.molochFields[key] = customCols[key];
                    let children = this.molochFields[key].children;
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

                console.log('---');
                console.log(this.expr);
                console.log(this.value);

                if (!this.column) {
                  this.parseValue(this.molochFields[this.expr]);
                }
                // this.buildMenu();
              });
         });
    }

    /* exposed functions --------------------------------------------------- */
    fieldClick(field, value, expr) {
      // TODO: if user is selecting text, don't add expression to query
      // TODO: if there is no expression, send it to new page
      let fullExpression = `${field} ${expr} ${value}`;

      this.$scope.$emit('add:to:search', { expression: fullExpression });
    }


    parseValue(fieldObj) {
      this.fieldObj = fieldObj;

      this.parsed = this.value;

      if (!this.fieldObj) { return; }

      switch(this.fieldObj.type) {
        case 'seconds':
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

      if (this.parsed || this.parsed === 0) {
        this.parsed = this.$sce.trustAsHtml(this.parsed.toString());
      }
    }



    buildMenu() {
      let urlParams = this.$location.search();
      let dateparams, isostart, isostop;

      if (urlParams.startTime && urlParams.stopTime) {
        dateparams  = `startTime=${urlParams.startTime}&stopTime=${urlParams.stopTime}`;
        isostart    = new Date(parseInt(urlParams.startTime) * 1000);
        isostop     = new Date(parseInt(urlParams.stopTime) * 1000);
      }
      else {
        dateparams  = `date=${urlParams.date}`;
        isostart    = new Date();
        isostop     = new Date();
        isostart.setHours(isostart.getHours() - parseInt(urlParams.date));
      }

      this.menuItems = {};

      for (let key in this.molochClickables) {
        let rc = this.molochClickables[key];
        if (this.fieldObj.category &&
           (!rc.category || this.fieldObj.category.indexOf(rc.category) === -1) &&
           (!rc.fields || rc.fields.indexOf(this.fieldObj) === -1)) {
          continue;
        }

        let result = this.molochClickables[key].url
           .replace('%EXPRESSION%', encodeURIComponent(urlParams.expression))
           .replace('%DATE%', dateparams)
           .replace('%ISOSTART%', isostart.toISOString())
           .replace('%ISOSTOP%', isostop.toISOString())
           .replace('%FIELD%', this.fieldObj)
           .replace('%TEXT%', this.value)
           .replace('%UCTEXT%', this.value.toUpperCase())
           .replace('%HOST%', host)
           .replace('%URL%', encodeURIComponent('http:' + url));

        let nameDisplay = '<b>' + (this.molochClickables[key].name || key) + '</b> %URL%';
        if (rc.category === 'host') {
          nameDisplay = '<b>' + (this.molochClickables[key].name || key) + '</b> %HOST%';
        }

        let name = (this.molochClickables[key].nameDisplay || nameDisplay)
           .replace('%FIELD%', this.fieldObj)
           .replace('%TEXT%', this.value)
           .replace('%HOST%', host)
           .replace('%URL%', url);

        if (rc.regex) {
          if (!rc.cregex) {
            rc.cregex = new RegExp(rc.regex);
          }
          let matches = this.value.match(rc.cregex);
          if (matches && matches[1]) {
            result = result.replace('%REGEX%', matches[1]);
          } else {
            continue;
          }
        }

        // TODO: remove tag

        this.menuItems[key] = { name: name, url: result };
      }
    }

  }

  SessionFieldController.$inject = ['$sce','$scope','$filter','$location',
    'FieldService','ConfigService'];

  /**
   * SessionField Directive
   * Displays session fields
   */
  angular.module('moloch')
    .component('sessionField', {
      template  : require('html!../templates/session.field.html'),
      controller: SessionFieldController,
      bindings  : { expr : '<', value : '<', session: '<', column: '<' }
    });

})();
