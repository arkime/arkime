(function() {

  'use strict';

  angular.module('moloch')

    /**
     * Moloch Map Directive
     * uses JQuery Vector Map
     *
     * @example
     * <moloch-map ng-if="$ctrl.mapData" map-data="$ctrl.mapData"></moloch-map>
     */
    .directive('molochMap', ['FieldService', '$filter', '$document', '$timeout', '$window',
    function(FieldService, $filter, $document, $timeout, $window) {
      return {
        scope   : { 'mapData': '=' },
        template: require('html!../templates/session.map.html'),
        link    : function(scope, element, attrs) {
          /* setup --------------------------------------------------------- */
          scope.state = { open:false, src:true, dst:true };

          let map, countryCodes;
          let mapEl = element.find('.moloch-map-container > #moloch-map');

          mapEl.vectorMap({ // setup map
            map             : 'world_en',
            backgroundColor : '#6FB5B5',
            hoverColor      : 'black',
            hoverOpacity    : 0.7,
            series: {
              regions: [{
                scale: ['#CFAED6', '#630078'],
                normalizeFunction: 'polynomial'
              }]
            },
            onRegionLabelShow: function(e, el, code){
              el.html(el.html() + ' (' + code + ') - ' +
                $filter('commaString')(map.series.regions[0].values[code] || 0));
            },
            onRegionClick: function(e, code){
              scope.$apply(() => {
                scope.$emit('add:to:search', { expression: 'country == ' + code });
              });
            }
          });

          // save reference to the map
          map = mapEl.children('.jvectormap-container').data('mapObject');
          countryCodes = map.regions;

          // save the maps country codes to be used throughout the app
          FieldService.saveCountryCodes(countryCodes);


          function setup(data) {
            map.series.regions[0].clear();
            delete map.series.regions[0].params.min;
            delete map.series.regions[0].params.max;

            if (scope.state.src && scope.state.dst) {
              if (!data.tot) {
                data.tot = {};
                let k;
                for (k in data.src) {
                  data.tot[k] = data.src[k];
                }

                for (k in data.dst) {
                  if (data.tot[k]) {
                    data.tot[k] += data.dst[k];
                  } else {
                    data.tot[k] = data.dst[k];
                  }
                }
              }
              map.series.regions[0].setValues(data.tot);
            } else if (scope.state.src) {
              map.series.regions[0].setValues(data.src);
            } else if (scope.state.dst) {
              map.series.regions[0].setValues(data.dst);
            }
          }

          /**
           * Determines whether a click was outside of the map element
           * and then closes the map if click was outside map
           * @param {Object} e The click event
           */
          function isOutsideClick(e) {
            if (!element.is(e.target) && element.has(e.target).length === 0) {
              $timeout(() => {
                scope.state.open = false;
              });
            }
          }


          /* LISTEN! */
          // watch for map data to change to update the map
          scope.$watch('mapData', (data) => {
            setup(data);
          });


          /* exposed functions --------------------------------------------- */
          /* Opens/closes the opened sessions panel */
          scope.toggleMap = function() {
            scope.state.open = !scope.state.open;

            // when the map is open, watch for user to click outside map
            if (scope.state.open) {
              $timeout(() => {
                // resize the map in case window has changed while map was closed
                $($window).trigger('resize');
              }, 50);
              $document.on('mouseup', isOutsideClick);
            } else {
              $document.off('mouseup', isOutsideClick);
            }
          };

          /* toggles source and destination buttons and updates map */
          scope.toggleSrcDst = function(type) {
            scope.state[type] = !scope.state[type];
            setup(scope.mapData);
          };

        }
      };
    }]);

})();
