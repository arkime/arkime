(function() {

  'use strict';

  angular.module('moloch')

    /**
     * Moloch Map Directive
     * uses JQuery Vector Map
     *
     * @example
     * <moloch-map></moloch-map>
     */
    .directive('molochMap', ['FieldService', function(FieldService) {
      return {
        link: function(scope, element, attrs) {
          element.vectorMap({ map: 'world_en' });

          var map = element.children('.jvectormap-container').data('mapObject');
          var countryCodes = map.regions;

          FieldService.saveCountryCodes(countryCodes);
        }
      };
    }]);

})();
