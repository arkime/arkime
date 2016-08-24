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
     * @example
     * {{session.pr|protocol}}
     */
    .filter('protocol', function() {
      return function(input) {
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
    });

})();
