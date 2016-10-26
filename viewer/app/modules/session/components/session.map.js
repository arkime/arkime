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
          var map, countryCodes;

          FieldService.saveCountryCodes(countryCodes);

          element.vectorMap({
            map             : 'world_en',
            backgroundColor : '#CCC',
            hoverColor      : 'black',
            hoverOpacity    : 0.7,
            series: {
              regions: [{
                scale: ['#bae4b3', '#006d2c'],
                normalizeFunction: 'polynomial'
              }]
            },
            onRegionLabelShow: function(e, el, code){
              el.html(el.html() + ' - ' + code);
            }
          });

          map = element.children('.jvectormap-container').data('mapObject');
          countryCodes = map.regions;
        }
      };
    }]);

})();
