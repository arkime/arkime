(function() {

  'use strict';

  let tokens, timeout;
  const operations = ['==', '!=', '<', '<=', '>', '>='];

  /**
   * @class ExpressionController
   * @classdesc Interacts with the expression typeahead
   * @example
   * '<expression-typeahead></expression-typeahead>'
   */
  class ExpressionController {

    /**
     * Initialize global variables for this controller
     * @param $scope        Angular application model object
     * @param $timeout      Angular's wrapper for window.setTimeout
     * @param $location     Exposes browser address bar URL (based on the window.location)
     * @param $routeParams  Retrieve the current set of route parameters
     * @param FieldService  Retrieve fields
     *
     * @ngInject
     */
    constructor($scope, $timeout, $location, $routeParams, FieldService) {
      this.$scope       = $scope;
      this.$timeout     = $timeout;
      this.$location    = $location;
      this.$routeParams = $routeParams;
      this.FieldService = FieldService;
    }

    /* Callback when component is mounted and ready */
    $onInit() {
      this.activeIdx  = -1;   // active index of typeahead results
      this.focusInput = true; // set focus on search input
      this.results    = null; // typeahead results
      // the typeahead results menu
      this.resultsElement = angular.element(document.getElementById('typeahead-results'));

      if (!this.query) { this.query = { value: '' }; }

      // get the available fields for autocompleting
      this.FieldService.get()
        .then((result) => {
          this.fields       = result;
          this.loadingError = false;
        })
        .catch((error) => {
          this.loadingError = error;
        });

      /* Listen! */
      // watch for search (from search.component)
      this.$scope.$on('issue:search', (event, args) => {
        // remove typeahead results once query has been issued
        if (timeout) { this.$timeout.cancel(timeout); }

        this.results    = null;
        this.activeIdx  = -1;
      });

      // watch for additions to search parameters
      this.$scope.$on('add:to:typeahead', (event, args) => {
        let newExpr = '';

        if (!this.query.value) { this.query.value = ''; }

        if (this.query.value && this.query.value !== '') {
          if (this.query.value[this.query.value.length - 1] !== ' ') {
            // if last char is not a space, add it
            newExpr += ' ';
          }
          newExpr += (args.op || '&&') + ' ';
        }

        newExpr += args.expression;

        this.query.value += newExpr;

        this.updateUrl();
      });
    }


    /* exposed functions --------------------------------------------------- */
    /* Removes typeahead results */
    onOffFocus() {
      this.$timeout(() => {
        if (timeout) { this.$timeout.cancel(timeout); }

        // if there's a request in progress, cancel it
        if (this.promise) { this.cancelPromise(); }

        this.results    = null;
        this.activeIdx  = -1;
      }, 300);
    }

    /* Fired when the search input is changed */
    debounceExprChange() {
      if (timeout) { this.$timeout.cancel(timeout); }
      timeout = this.$timeout(() => {
        this.changeExpression();
        this.updateUrl();
      }, 300);
    }

    /* Apply the search expression value to the expression url parameter */
    updateUrl() {
      if (this.query.value && this.query.value !== '') {
        this.$location.search('expression', this.query.value);
      } else {
        this.$location.search('expression', null);
      }
    }

    /* Displays appropriate autocomplete suggestions */
    changeExpression() {
       this.activeIdx     = -1;
       this.results       = null;
       this.focusInput    = false;
       this.loadingValues = false;
       this.loadingError  = false;

       // if the cursor is at a space
       let spaceCP = (this.caretPos > 0 &&
         this.caretPos === this.query.value.length &&
         this.query.value[this.caretPos - 1] === ' ');

       let end    = this.caretPos;
       let endLen = this.query.value.length;
       for (end; end < endLen; ++end) {
         if (this.query.value[end] === ' ') {
           break;
         }
       }

       let currentInput = this.query.value.substr(0, end);
       tokens = ExpressionController.splitExpression(currentInput);

       // add the space to the tokens
       if (spaceCP) { tokens.push(' '); }

       let lastToken = tokens[tokens.length - 1];

       // display fields
       if (tokens.length <= 1) {
         this.results = ExpressionController.findMatch(lastToken, this.fields);
         return;
       }

       // display operators (depending on field type)
       let token = tokens[tokens.length - 2];
       let field = this.fields[token];
       if (field) {
         if (field.type === 'integer') {
           // if at a space, show all operators
           if(tokens[tokens.length - 1] === ' ') {
             this.results = operations;
           } else {
             this.results = ExpressionController.findMatch(lastToken, operations);
           }
         } else { this.results = ['!=', '==']; }

         return;
       }

       // save the operator token for possibly adding 'EXISTS!' result
       let operatorToken = token;

       token = tokens[tokens.length - 3];
       field = this.fields[token];

       if (!field) {
         if (/^[!<=>]/.test(token)) {
           // if at a space, show all operators
           if(tokens[tokens.length - 1] === ' ') {
             this.results = ['&&', '||'];
           } else {
             this.results = ExpressionController.findMatch(lastToken, ['&&', '||']);
           }
         } else {
           this.results = ExpressionController.findMatch(lastToken, this.fields);
         }

         return;
       }

       // display values
       // autocomplete country values
       if (/^(country)/.test(token)) {
         this.loadingValues = true;
         this.FieldService.getCountryCodes()
           .then((result) => {
             this.loadingValues = false;
             this.results = ExpressionController.findMatch(lastToken, result);
             this.addExistsItem(lastToken, operatorToken);
           })
           .catch((error) => {
             this.loadingValues = false;
             this.loadingError  = error;
           });

         return;
       }

       // autocomplete http.hasheader values after 1 char
       if (lastToken.trim().length >= 1) {
         if (/^(tags|http.hasheader)/.test(token)) {
           // if there's a request in progress, cancel it
           if (this.promise) { this.cancelPromise(); }

           this.loadingValues = true;

           this.promise = this.FieldService.getHasheaderValues({
             type:token, filter:lastToken
           });

           this.promise.then((result) => {
             this.promise         = null;
             if (result) {
               this.loadingValues = false;
               this.results       = result;
               this.addExistsItem(lastToken, operatorToken);
             }
           }).catch((error) => {
             this.promise       = null;
             this.loadingValues = false;
             this.loadingError  = error;
           });

           return;
         }
       }

       // autocomplete other values after 2 chars
       if (lastToken.trim().length >= 2) {
         let params = { // build parameters for getting value(s)
           autocomplete : true,
           field        : field.dbField
         };

         if (this.$routeParams && this.$routeParams.date) {
           params.date = this.$routeParams.date;
         } else if (this.$routeParams.startTime && this.$routeParams.stopTime) {
           params.startTime = this.$routeParams.startTime;
           params.stopTime  = this.$routeParams.stopTime;
         }

         if (field.type === 'ip') {
           params.expression = token + '==' + lastToken;
         } else {
           params.expression = token + '==' + lastToken + '*';
         }

         // if there's a request in progress, cancel it
         if (this.promise) { this.cancelPromise(); }

         this.loadingValues = true;

         this.promise = this.FieldService.getValues(params);

         this.promise.then((result) => {
           this.promise         = null;
           if (result) {
             this.loadingValues = false;
             this.results       = result;
             this.addExistsItem(lastToken, operatorToken);
           }
         }).catch((error) => {
           this.promise       = null;
           this.loadingValues = false;
           this.loadingError  = error;
         });
       }
     }

    /**
     * Fired when a value from the autocomplete menu is selected
     * @param {Object} val The value to be added to the query
     */
    addToQuery(val) {
      let str = val;
      if (val.exp) { str = val.exp; }

      this.query.value = ExpressionController.rebuildQuery(this.query.value, str);

      this.results     = null;
      this.focusInput  = true; // re-focus on input
      this.activeIdx   = -1;

      this.updateUrl();
    }

    /**
     * Removes the query text from the input
     */
    clear() {
      this.query.value = null;
      this.updateUrl();
    }

    /**
     * Watches for keydown events and determines if a user is
     * pressing the up and down arrows to navigate the drop down.
     * When the user presses enter, their selection is added to the query.
     * When the user presses escape, the typeahead results are removed.
     * @param {Object} event The keydown event fired by the input
     */
    keydown(event) {
      let target;

      // always check for escape before anything else
      if (event.keyCode === 27) {
        // if there's a request in progress, cancel it
        if (this.promise) { this.cancelPromise(); }

        this.loadingValues = false;
        this.loadingError  = false;
        this.results       = null;
        this.activeIdx     = -1;

        return;
      }

      // if there are no results, just check for enter click to remove typeahead
      if (!this.results || this.results.length === 0) {
        if (event.keyCode === 13) {
          if (this.promise) { this.cancelPromise(); }

          this.loadingValues = false;
          this.loadingError  = false;
          this.results       = null;
          this.activeIdx     = -1;
        }

        return;
      }

      if (!this.activeIdx && this.activeIdx !== 0) { this.activeIdx = -1; }

      switch (event.keyCode) {
       case 40: // down arrow
         event.preventDefault();
         this.activeIdx = (this.activeIdx + 1) % this.results.length;
         target = this.resultsElement[0].querySelectorAll('li')[this.activeIdx];
         target.parentNode.scrollTop = target.offsetTop;
         break;
       case 38: // up arrow
         event.preventDefault();
         this.activeIdx = (this.activeIdx > 0 ?
           this.activeIdx : this.results.length) - 1;
         target = this.resultsElement[0].querySelectorAll('li')[this.activeIdx];
         target.parentNode.scrollTop = target.offsetTop;
         break;
       case 13: // enter
         if (this.activeIdx >= 0) {
           event.preventDefault();
           let result = this.results[this.activeIdx];
           if (result) { this.addToQuery(result); }
         }
         break;
      }
    }

    /* aborts a pending promise */
    cancelPromise() {
      this.promise.abort();
      this.promise = null;

      this.loadingValues = false;
    }

    /**
     * Adds 'EXISTS!' result item to the typeahead options
     * @param {string} strToMatch The string to compare 'EXISTS!' to
     * @param {string} operator   The operator of the expression
     */
    addExistsItem(strToMatch, operator) {
      if (operator !== '==' && operator !== '!=') { return; }

      if ('EXISTS!'.match(new RegExp(strToMatch + '.*'))) {
        this.results.push('EXISTS!');
      }
    }

     /* internal functions -------------------------------------------------- */
     /**
      * Finds matching items from an array or map of values
      * @param {string} strToMatch  The string to compare with the values
      * @param {Object} values      Map or Array of values to compare against
      */
     static findMatch(strToMatch, values) {
       if (!strToMatch || strToMatch === '') { return null; }

       let results = [], exact = false;

       for (let key in values) {
         if (values.hasOwnProperty(key)) {
           let field = values[key], str;
           strToMatch = strToMatch.toLowerCase();

           if (field.exp) { str = field.exp.toLowerCase(); }
           else { str = field.toLowerCase(); }

           if (str === strToMatch) { exact = field; }
           else {
             let match = str.match(strToMatch);
             if (match) { results.push(field); }
           }
         }
       }

       // put the exact match at the top (the rest are in the order received)
       if (exact) { results.unshift(exact); }

       if (!results.length) { results = null; }

       return results;
     }

     /**
      * Builds the query by either appending a string onto it or replacing
      * the last token with a string and rebuilding the query string
      * @param {string} q   The query string to be appended to or rebuilt
      * @param {string} str The string to add to the query
      */
     static rebuildQuery(q, str) {
       let lastToken = tokens[tokens.length - 1], result = '';
       let allTokens = ExpressionController.splitExpression(q);

       if (lastToken === ' ') { result = q += str + ' '; }
       else { // replace the last token and rebuild query
         let t, i;
         tokens[tokens.length - 1] = str;

         for (i = 0; i < tokens.length; ++i) {
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
       // replace spaces that are not enclosed in quotes
       input = input.replace(/ (?=([^"]*"[^"]*")*[^"]*$)/g, '');
       let output = [];
       let cur = '';

       for (let i = 0, ilen = input.length; i < ilen; i++) {
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

  ExpressionController.$inject = ['$scope','$timeout','$location','$routeParams',
    'FieldService'];

  /**
   * Expression Typeahead Directive
   * Displays the search box with interactive autocomplete
   */
  angular.module('directives.search')
    .component('expressionTypeahead', {
      template  : require('html!../templates/expression.typeahead.html'),
      controller: ExpressionController,
      bindings  : { query: '<' }
    });

})();
