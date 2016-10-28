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
        template: require('html!../templates/session.map.html'),
        link    : function(scope, element, attrs) {
          /* setup --------------------------------------------------------- */
          var map, countryCodes;
          var mapEl = element.find('.moloch-map-container > #moloch-map');

          // setup map
          mapEl.vectorMap({
            map             : 'world_en',
            backgroundColor : '#6FB5B5',
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

          // save reference to the map
          map = mapEl.children('.jvectormap-container').data('mapObject');
          countryCodes = map.regions;

          // save the maps country codes to be used throughout the app
          FieldService.saveCountryCodes(countryCodes);


          /* exposed functions --------------------------------------------- */
          /* Opens/closes the opened sessions panel */
          scope.state = { open:false };
          scope.toggleMap = function() {
            scope.state.open = !scope.state.open;
          };

        }
      };
    }]);

})();
