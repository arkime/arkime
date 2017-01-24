(function() {

  'use strict';

  /**
   * Util Directives
   * Exposes utility directives
   */
  angular.module('moloch.util', [])


   .filter('commaString', () => {
     return (input) => {
       if (isNaN(input)) { return '0'; }
       return input.toString().replace(/\B(?=(\d{3})+(?!\d))/g, ',');
     };
   })

    /**
     * Protocol filter
     * Displays the protocol string
     *
     * @example
     * '{{session.pr | protocol}}'
     */
    .filter('protocol', () => {
      let lookup = { 1:'icmp', 6:'tcp', 17:'udp', 47:'gre', 58:'icmp6' };

      return (input) => {
        let result = lookup[input];
        if (!result) { result = input; }
        return result;
      };
    })

    /**
     * Extract IP String filter
     * Parses the ip
     *
     * @example
     * '{{session.a1 | extractIPString}}'
     */
    .filter('extractIPString', () => {
      return (ip) => {
        if (!ip) { return ''; }
        if (typeof ip === 'string' && ip.indexOf('.') !== -1) { return ip; }

        return (ip>>24 & 0xff) + '.' + (ip>>16 & 0xff) +
                '.' + (ip>>8 & 0xff) + '.' + (ip & 0xff);
      };
    })

    /**
     * Extract IPv6 String filter
     * Parses the ipv6
     *
     * @example
     * '{{session['tipv61-term'] | extractIPv6String}}'
     */
    .filter('extractIPv6String', () => {
      return (ipv6) => {
        if (!ipv6) { return ''; }

        ipv6 = ipv6.toString();

        let ip = ipv6.match(/.{1,4}/g).join(':').replace(/:0{1,3}/g, ':').replace(/^0000:/, '0:');
        [/(^|:)0:0:0:0:0:0:0:0($|:)/,
         /(^|:)0:0:0:0:0:0:0($|:)/,
         /(^|:)0:0:0:0:0:0($|:)/,
         /(^|:)0:0:0:0:0($|:)/,
         /(^|:)0:0:0:0($|:)/,
         /(^|:)0:0:0($|:)/,
         /(^|:)0:0($|:)/].every(function(re) {
           if (ipv6.match(re)) {
             ip = ipv6.replace(re, '::');
             return false;
           }
           return true;
         });

        return ip;
      };
    })


    /**
     * Turns milliseconds into a human readable time range
     * Output example: 1 day 10:42:01
     *
     * @example
     * '{{ms | readableTime}}'
     */
    .filter('readableTime', () => {
      return (ms) => {
        if (isNaN(ms)) { return '?'; }

        let seconds = parseInt((ms/1000)%60);
        let minutes = parseInt((ms/(1000*60))%60);
        let hours   = parseInt((ms/(1000*60*60))%24);
        let days    = parseInt((ms/(1000*60*60*24)));

        let result = '';

        if (days) {
          result += days + ' day';
          if (days > 1) { result += 's '; }
          else { result += ' '; }
        }

        if (hours || minutes || seconds) {
          result += (hours < 10) ? '0' + hours : hours;
          result += ':';
          result += (minutes < 10) ? '0' + minutes : minutes;
          result += ':';
          result += (seconds < 10) ? '0' + seconds : seconds;
        }

        return result || '0';
      };
    })


    /**
     * Field filter
     * Displays fields with a friendlyName that matches the search input
     *
     * @example
     * '(key,val) in $ctrl.fieldsArray | fieldFilter:searchInput'
     */
    .filter('fieldFilter', function () {
      return function (items, search) {
        let results = [];

        for (let i = 0, len = items.length; i < len; ++i) {
          let str     = items[i].friendlyName.toLowerCase();
          let match   = str.match(search);
          if (match) { results.push(items[i]); }
        }

        return results;
      };
    })

    /**
     * Capitalize filter
     * Capitalizes the first letter in a string
     *
     * @example
     * '{{someString | capitalize}}'
     */
    .filter('capitalize', function() {
      return function(input) {
        return (!!input) ? input.charAt(0).toUpperCase() +
                input.substr(1).toLowerCase() : '';
      };
    })

    /**
     * Lowercase filter
     * Sets every letter to lower case in a string
     *
     * @example
     * '{{someString | capitalize}}'
     */
     .filter('lowercase', function() {
       return function(input) {
         return input.toLowerCase();
       };
     })

    /**
     * Min filter
     * Returns the minimum number in a number array
     *
     * @example
     * '{{[num1, num2, ...] | min}}'
     */
     .filter('min', function () {
       return function (input) {
         if(!Array.isArray(input)) { return input; }

         return Math.min.apply(Math, input);
       };
     })

     /**
      * Applies the selected timezone to the given time
      * Returns local time by default
      *
      * @example
      * '{{seconds | 'timezone-date' : 'gmt'}}
      *
      * @param {int} seconds      The time in seconds from epoch
      * @param {string} timezone  The timezone to use ('gmt' or 'local')
      */
     .filter('timezone-date', function() {
       return function (seconds, timezone) {
         let d = new Date(seconds * 1000);

         if (timezone === 'gmt') {
           d = new Date(d.getTime() + (60000 * d.getTimezoneOffset()));
         }

         return +d;
       };
     })

    /**
     * Convert To Number Directive
     * Parses strings to integers
     * @see {@link https://docs.angularjs.org/api/ng/directive/select|Angular Select}
     *
     * @example
     * '<select ng-model="$ctrl.value" convert-to-number>...</select>'
     */
    .directive('convertToNumber', function() {
      return {
        require: 'ngModel',
        link: function(scope, element, attrs, ngModel) {
          ngModel.$parsers.push(function(val) {
            return val ? parseInt(val, 10) : null;
          });
          ngModel.$formatters.push(function(val) {
            return val ? '' + val : null;
          });
        }
      };
    })

    /**
     * Epoch Date Directive
     * Parses date strings to integers (ms from 1970) based on timezone
     *
     * In the future, maybe we can use ng-model-options on the input?
     * https://github.com/angular-ui/bootstrap/issues/2952
     * https://github.com/angular-ui/bootstrap/issues/4837
     *
     * @example
     * '<input epoch-date="'gmt'" ng-model="$ctrl.time" type="text"
     *   uib-datepicker-popup="{{$ctrl.dateTimeFormat}}" />'
     */
    .directive('epochDate', function() {
      return {
    		require: 'ngModel',
    		link: function(scope, element, attrs, ngModel) {
          ngModel.$formatters.push(function (value) {
            let date = new Date(value);
            if (attrs.epochDate === 'gmt') { // apply gmt instead of local time
              date = new Date(date.getTime() + (60000 * date.getTimezoneOffset()));
            }
            return +date;
          });

          ngModel.$parsers.push(function (value) {
            let date = value;
            if (attrs.epochDate === 'gmt') { // apply gmt instead of local time
              date = new Date(value.getTime() - (60000 * value.getTimezoneOffset()));
            }
            return +date;
          });
        }
    	};
    })

    /**
     * Determines the cursor position in an input
     *
     * @example
     * <input type="text" caret-pos="$ctrl.caretPos"
     *  ng-model="$ctrl.query" class="form-control" />
     */
    .directive('caretPos', function() {
      return {
        restrict: 'A',
        scope   : { caretPos: '=', },
        link    : function(scope, element, attrs) {
          if (!scope.caretPos) { scope.caretPos = 0; }
          element.on('keydown keyup click', function(event) {
            scope.$apply(function() {
              if ('selectionStart' in element[0]) {
                scope.caretPos = element[0].selectionStart;
              } else if (document.selection) {
                // the user has highlighted text in the input
                element[0].focus();
                let selection = document.selection.createRange();
                let selectionLen = document.selection.createRange().text.length;
                selection.moveStart('character', -element[0].value.length);
                scope.caretPos = selection.text.length - selectionLen;
              }
            });
          });
        }
      };
    })


    /**
     * Sets focus on an input element
     *
     * @example
     * <input type="text" focus-input="$ctrl.focus"
     *  ng-model="$ctrl.query" class="form-control" />
     *
     * Note: To re-focus, make sure to set $ctrl.focus in parent
     * controller to false before setting it to true again
     */
    .directive('focusInput', function() {
      return {
        restrict: 'A',
        link    : function(scope, element, attrs) {
          let dom = element[0];

          if (attrs.focusInput) { dom.focus(); }

          scope.$watch(attrs.focusInput, function(value) {
            if (value) { dom.focus(); }
          });
        }
      };
    })


    /**
     * Evaluates a given function when enter key is pressed
     *
     * @example
     * <input type="text" enter-click="$ctrl.change()"
     *  ng-model="$ctrl.query" class="form-control" />
     *
     * Note: can be applied to any element, not just inputs
     */
    .directive('enterClick', function() {
      return function(scope, element, attrs) {
        element.bind('keydown keypress', function(event) {
          if (event.which === 13) {
            event.preventDefault();
            scope.$apply(function() {
              scope.$eval(attrs.enterClick);
            });
          }
        });
      };
    });

})();
