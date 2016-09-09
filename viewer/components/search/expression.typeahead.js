(function() {

  'use strict';

  var tokens;

  /**
   * @class ExpressionController
   * @classdesc Interacts with the expression typeahead
   * @example
   * '<expression-typeahead></expression-typeahead>'
   */
  class ExpressionController {

    /**
     * Initialize global variables for this controller
     * @param $location Exposes browser address bar URL
     *                  (based on the window.location)
     *
     * @ngInject
     */
    constructor($scope, $location, $routeParams, FieldService) {
      this.$scope       = $scope;
      this.$location    = $location;
      this.$routeParams = $routeParams;
      this.FieldService = FieldService;
    }

    /* Callback when component is mounted and ready */
    $onInit() {
      this.caretPos = 0;

      this.FieldService.get()
        .then((result) => {
          this.fields = result;
        })
        .catch((err) => {
          // TODO: show field retrieval error
        });
    }


    /* exposed functions --------------------------------------------------- */
    /**
     *
     */
     changeExpression() {
       this.fieldResults = null;
       this.operationResults = null;

       // if the cursor pointer is at a space
       var spaceCP = (this.caretPos > 0 &&
         this.caretPos === this.query.length &&
         this.query[this.caretPos - 1] === ' ');

       var end    = this.caretPos;
       var endLen = this.query.length;
       for (end; end < endLen; ++end) {
         if (this.query[end] === ' ') {
           break;
         }
       }

       var currentInput = this.query.substr(0, end);
       tokens = ExpressionController.splitExpression(currentInput);
       var lastToken = tokens[tokens.length - 1];

       if (spaceCP) { tokens.push(' '); }

       // field
       if (tokens.length <= 1) {
         this.fieldResults  = ExpressionController.findMatch(lastToken, this.fields);

         return;
       }

       // operator
       var token = tokens[tokens.length - 2];
       var field = this.fields[token];
       if (field) {
         if (field.type === 'integer') {
           this.operationResults = ['==', '!=', '<', '<=', '>', '>='];
         } else { this.operationResults = ['!=', '==']; }

         return;
       }

       token = tokens[tokens.length - 3];
       field = this.fields[token];

       if (!field) {
         if (/^[!<=>]/.test(token)) {
           this.operationResults = ['&&', '||'];
         } else {
           this.fieldResults  = ExpressionController.findMatch(lastToken, this.fields);
         }

         return;
       }

       // value
       if (/^(country)/.test(token)) {
         // TODO display countries
         return;
       }

       if (lastToken.length >= 1) {
         if (/^(tags|http.hasheader)/.test(token)) {
           this.FieldService.getHasheader({type:token,filter:lastToken})
             .then((result) => {
               this.fieldResults = result;
               return;
             })
             .catch((error) => {
               // TODO handle value retrieval error
             });

           return;
         }
       }

       // only autocomplete value after 2 chars
       if (lastToken.length > 2) {
         var params = {
           autocomplete : true,
           field        : field.dbField
         };

         if (this.$routeParams && this.$routeParams.date) {
           params.date = this.$routeParams.date;
         } else if (this.$routeParams.startTime && this.$routeParams.stopTime) {
           params.startTime = this.$routeParams.startTime;
           params.stopTime  = this.$routeParams.stopTime;
         }

         // TODO view parameter?
         if (field.type === 'ip') { params.expression = token + '==' + lastToken; }
         else { params.expression = token + '==' + lastToken + '*'; }

         this.FieldService.getValue(params)
           .then((result) => {
             this.fieldResults = result;
             return;
           })
           .catch((error) => {
             // TODO handle value retrieval error
           });
       }

       return;
     }

     addToQuery(val) {
       var str = val;
       if (val.exp) { str = val.exp; }
       this.query = ExpressionController.rebuildQuery(this.query, str);
       this.fieldResults = null;
       this.operationResults = null;
     }

     /**
      *
      */
     clear() {
       this.query = null;
     }


     /* internal functions -------------------------------------------------- */
     static findMatch(strToMatch, map) {
       var results = [], exact = false;

       for (var key in map) {
         if (map.hasOwnProperty(key)) {
           var field = map[key];
           if (field.exp === strToMatch) { exact = field; break; }
           var match = field.exp.match(strToMatch);
           if (match) { results.push(field); }
         }
       }

       // put the exact match at the top
       if (exact) { results.unshift(exact); }

       return results;
     }

     static rebuildQuery(q, str) {
       var lastToken = tokens[tokens.length - 1];

       if (lastToken === ' ') { q += str + ' '; }
       else { // replace the last token and rebuild query
         tokens[tokens.length - 1] = str;

         q = '';

         for (var i = 0; i < tokens.length; ++i) {
           var t = tokens[i];
           if (t === ' ') { break; }
           q += t + ' ';
         }
       }

       return q;
     }

     static splitExpression(input) {
       input = input.replace(/ /g, '');
       var output = [];
       var cur = "";

       for (var i = 0, ilen = input.length; i < ilen; i++) {
           if (/[)(]/.test(input[i])) {
             if (cur !== '') {
               output.push(cur);
             }
             output.push(input[i]);
             cur = '';
           } else if (cur === '') {
             cur += input[i];
           } else if (/[!&|=]/.test(cur)) {
             if (/[&|=]/.test(input[i])) {
               cur += input[i];
             } else {
               output.push(cur);
               cur = input[i];
             }
           } else if (/[!&|=]/.test(input[i])) {
             output.push(cur);
             cur = input[i];
           } else {
             cur += input[i];
           }
       }
       if (cur !== '') {
         output.push(cur);
       }
       return output;
     }
  }

  ExpressionController.$inject = ['$scope','$location','$routeParams','FieldService'];

  function expressionLink(scope, element, attrs) {}

  /**
   * Navbar Directive
   * Displays the navbar
   */
  angular.module('directives.search')
    // .component('expressionTypeahead', {
    //   template  : require('html!./expression.typeahead.html'),
    //   controller: ExpressionController
    // });
    .controller('ExpressionController', ExpressionController)
    .directive('expressionTypeahead', function() {
      return {
        template: require('html!./expression.typeahead.html'),
        link    : expressionLink
      };
    });

})();
