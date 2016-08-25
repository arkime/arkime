(function() {

  'use strict';

  /**
   * Util Directives
   * Exposes utility directives
   */
  angular.module('moloch.util', [])

    /**
     * Convert To Number Directive
     * Parses strings to integers
     * @see {@link https://docs.angularjs.org/api/ng/directive/select|Angular Select}
     *
     * @example
     * '<select ng-model="$ctrl.stringThing" convert-to-number>...</select>'
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
        if (session['tipv62-term'] || session['tipv61-term']) {
          var ip6 = session['tipv62-term'] || session['tipv61-term'];

          var ip = ip6.match(/.{1,4}/g).join(":").replace(/:0{1,3}/g, ":").replace(/^0000:/, "0:");
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
          var ip = session.a1 || session.a2;

          return (ip>>24 & 0xff) + '.' + (ip>>16 & 0xff) +
                  '.' + (ip>>8 & 0xff) + '.' + (ip & 0xff);
        }
      }
    });

})();
