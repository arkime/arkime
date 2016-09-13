(function() {

  'use strict';

  /**
   * Util Directives
   * Exposes utility directives
   */
  angular.module('moloch.util', [])

    /**
     * Protocol filter
     * Displays the protocol string
     *
     * @example
     * '{{session.pr | protocol}}'
     */
    .filter('protocol', () => {
      return (input) => {
        var result;

        switch (input) {
          case 1:
            result = 'icmp';
            break;
          case 6:
            result = 'tcp';
            break;
          case 17:
            result = 'udp';
            break;
          case 47:
            result = 'gre';
            break;
          case 58:
            result = 'icmp6';
            break;
          default:
            result = input.toString();
            break;
        }

        return result;
      };
    })

    /**
     * Extract IP String filter
     * Displays the ip or ip6 string, given the session
     *
     * @example
     * '{{session | extractIPString}}'
     */
    .filter('extractIPString', () => {
      return (session) => {
        var ip;

        if (session['tipv62-term'] || session['tipv61-term']) {
          var ip6 = (session['tipv62-term'] || session['tipv61-term']).toString();

          ip = ip6.match(/.{1,4}/g).join(":").replace(/:0{1,3}/g, ":").replace(/^0000:/, "0:");
          [/(^|:)0:0:0:0:0:0:0:0($|:)/,
           /(^|:)0:0:0:0:0:0:0($|:)/,
           /(^|:)0:0:0:0:0:0($|:)/,
           /(^|:)0:0:0:0:0($|:)/,
           /(^|:)0:0:0:0($|:)/,
           /(^|:)0:0:0($|:)/,
           /(^|:)0:0($|:)/].every(function(re) {
             if (ip6.match(re)) {
               ip = ip6.replace(re, "::");
               return false;
             }
             return true;
           });

          return ip;
        } else {
          ip = session.a1 || session.a2;

          return (ip>>24 & 0xff) + '.' + (ip>>16 & 0xff) +
                  '.' + (ip>>8 & 0xff) + '.' + (ip & 0xff);
        }
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

        var seconds = parseInt((ms/1000)%60);
        var minutes = parseInt((ms/(1000*60))%60);
        var hours   = parseInt((ms/(1000*60*60))%24);
        var days    = parseInt((ms/(1000*60*60*24)));

        var result = '';

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

        return result;
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
     * Parses date strings to integers (ms from 1970)
     *
     * @example
     * '<input epoch-date ng-model="$ctrl.time" type="text"
     *   uib-datepicker-popup="{{$ctrl.dateTimeFormat}}" />'
     */
    .directive('epochDate', function() {
      return {
    		require: 'ngModel',
    		link: function(scope, element, attrs, ngModel) {
      		ngModel.$parsers.push(function(viewValue) {
           	return +viewValue;
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
                var selection = document.selection.createRange();
                var selectionLen = document.selection.createRange().text.length;
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
          var dom = element[0];

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
     * <input type="text" ng-enter="$ctrl.change()"
     *  ng-model="$ctrl.query" class="form-control" />
     *
     * Note: can be applied to any element, not just inputs
     */
    .directive('ngEnter', function() {
      return function(scope, element, attrs) {
        element.bind('keydown keypress', function(event) {
          if (event.which === 13) {
            event.preventDefault();
            scope.$apply(function() {
              scope.$eval(attrs.ngEnter);
            });
          }
        });
      };
    });

})();
