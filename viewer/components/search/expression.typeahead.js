(function() {

  'use strict';

  var tokens;
  var operations = ['==', '!=', '<', '<=', '>', '>='];

  /**
   * @class ExpressionController
   * @classdesc Interacts with the expression typeahead
   * @example
   * '<expression-typeahead></expression-typeahead>'
   */
  class ExpressionController {

    /**
     * Initialize global variables for this controller
     * @param $routeParams Retrieve the current set of route parameters
     * @param FieldService Retrieve fields
     *
     * @ngInject
     */
    constructor($routeParams, FieldService) {
      this.$routeParams = $routeParams;
      this.FieldService = FieldService;
    }

    /* Callback when component is mounted and ready */
    $onInit() {
      this.caretPos   = 0;
      this.focusInput = true;

      this.FieldService.get()
        .then((result) => {
          this.fields       = result;
          this.loadingError = false;
        })
        .catch((error) => {
          this.loadingError = error;
        });
    }


    /* exposed functions --------------------------------------------------- */
    /**
     * Fired when the search input is changed (with a 300 ms delay)
     * Displays appropriate autocomplete suggestions
     */
     changeExpression() {
       this.focusInput        = false;
       this.fieldResults      = null;
       this.operationResults  = null;
       this.loadingValues     = false;
       this.loadingError      = false;

       // if the cursor pointer is at a space
       var spaceCP = (this.caretPos > 0 &&
         this.caretPos === this.query.value.length &&
         this.query.value[this.caretPos - 1] === ' ');

       var end    = this.caretPos;
       var endLen = this.query.value.length;
       for (end; end < endLen; ++end) {
         if (this.query.value[end] === ' ') {
           break;
         }
       }

       var currentInput = this.query.value.substr(0, end);
       tokens = ExpressionController.splitExpression(currentInput);
       var lastToken = tokens[tokens.length - 1];

       if (spaceCP) { tokens.push(' '); }

       // field
       if (tokens.length <= 1) {
         this.fieldResults =
           ExpressionController.findMatch(lastToken, this.fields);

         return;
       }

       // operator
       var token = tokens[tokens.length - 2];
       var field = this.fields[token];
       if (field) {
         if (field.type === 'integer') {
          if(tokens[tokens.length - 1] === ' ') {
            this.operationResults = operations;
          } else {
            this.operationResults =
              ExpressionController.findMatch(lastToken, operations);
          }
         } else { this.operationResults = ['!=', '==']; }

         return;
       }

       token = tokens[tokens.length - 3];
       field = this.fields[token];

       if (!field) {
         if (/^[!<=>]/.test(token)) {
          if(tokens[tokens.length - 1] === ' ') {
            this.operationResults = ['&&', '||'];
          } else {
            this.operationResults =
              ExpressionController.findMatch(lastToken, ['&&', '||']);
          }
         } else {
           this.fieldResults =
             ExpressionController.findMatch(lastToken, this.fields);
         }

         return;
       }

       // value
       if (/^(country)/.test(token)) {
         this.loadingValues = true;
         this.FieldService.getCountryCodes()
           .then((result) => {
             this.loadingValues = false;
             this.fieldResults  =
               ExpressionController.findMatch(lastToken, result);
           })
           .catch((error) => {
             this.loadingValues = false;
             this.loadingError  = error;
           });

         return;
       }

       if (lastToken.length >= 1) {
         if (/^(tags|http.hasheader)/.test(token)) {
           this.loadingValues = true;
           this.FieldService.getHasheader({type:token,filter:lastToken})
             .then((result) => {
               this.loadingValues = false;
               this.fieldResults  = result;
               return;
             })
             .catch((error) => {
               this.loadingValues = false;
               this.loadingError  = error;
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

         // TODO view parameter
         if (field.type === 'ip') {
           params.expression = token + '==' + lastToken;
         } else {
           params.expression = token + '==' + lastToken + '*';
         }

         this.loadingValues = true;
         this.FieldService.getValue(params)
           .then((result) => {
             this.loadingValues = false;
             this.fieldResults  = result;
             return;
           })
           .catch((error) => {
             this.loadingValues = false;
             this.loadingError  = error;
           });
       }

       return;
     }

     /**
      * Fired when a value from the autocomplete menu is selected
      * @param {Object} val The value to be added to the query
      */
     addToQuery(val) {
       var str = val;
       if (val.exp) { str = val.exp; }

       var newValue = ExpressionController.rebuildQuery(this.query.value, str);
       this.query.value = newValue;

       this.fieldResults      = null;
       this.operationResults  = null;
       this.focusInput        = true; // re-focus on input
     }

     /**
      * Removes the query text from the input
      */
     clear() {
       this.query.value = null;
     }


     /* internal functions -------------------------------------------------- */
     /**
      * Finds matching items from an array or map of values
      * @param {string} strToMatch  The string to compare with the values
      * @param {Object} values      Map or Array of values to compare against
      */
     static findMatch(strToMatch, values) {
       var results = [], exact = false;

       for (var key in values) {
         if (values.hasOwnProperty(key)) {
           var field = values[key], str;
           strToMatch = strToMatch.toLowerCase();

           if (field.exp) { str = field.exp.toLowerCase(); }
           else { str = field.toLowerCase(); }

           if (str === strToMatch) { exact = field; }
           else {
             var match = str.match(strToMatch);
             if (match) { results.push(field); }
           }
         }
       }

       // put the exact match at the top
       if (exact) { results.unshift(exact); }

       return results;
     }

     /**
      * Builds the query by either appending a string onto it or replacing
      * the last token with a string and rebuilding the query string
      * @param {string} q   The query string to be appended to or rebuilt
      * @param {string} str The string to add to the query
      */
     static rebuildQuery(q, str) {
       var lastToken = tokens[tokens.length - 1], result = '';
       var allTokens = ExpressionController.splitExpression(q);

       if (lastToken === ' ') { q += str + ' '; }
       else { // replace the last token and rebuild query
         var t;
         tokens[tokens.length - 1] = str;

         for (var i = 0; i < tokens.length; ++i) {
           t = tokens[i];
           if (t === ' ') { break; }
           result += t + ' ';
         }

         if (allTokens.length > tokens.length) {
           // add the rest of the tokens
           for(i; i < allTokens.length; ++i) {
             t = allTokens[i];
             if (t === ' ') { break; }
             result += t + ' ';
           }
         }
       }

       return result;
     }

     /**
      * Splits a string into tokens
      * @param {string} input The string to be tokenized
      */
     static splitExpression(input) {
       input = input.replace(/ /g, '');
       var output = [];
       var cur = '';

       for (var i = 0, ilen = input.length; i < ilen; i++) {
           if (/[)(]/.test(input[i])) {
             if (cur !== '') {
               output.push(cur);
             }
             output.push(input[i]);
             cur = '';
           } else if (cur === '') {
             cur += input[i];
           } else if (/[!&|=<>]/.test(cur)) {
             if (/[&|=<>]/.test(input[i])) {
               cur += input[i];
             } else {
               output.push(cur);
               cur = input[i];
             }
           } else if (/[!&|=<>]/.test(input[i])) {
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

  ExpressionController.$inject = ['$routeParams','FieldService'];

  /**
   * Expression Typeahead Directive
   * Displays the search box with interactive autocomplete
   */
  angular.module('directives.search')
    .component('expressionTypeahead', {
      template  : require('html!./expression.typeahead.html'),
      controller: ExpressionController,
      bindings  : { query: '=' }
    });

})();
