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
        template: require('html!../templates/map.html'),
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


          /* LISTEN! */
          // watch for map data to change to update the map
          scope.$watch('mapData', (data) => {
            setup(data);
          });


          /* utility functions --------------------------------------------- */
          /* shrinks the map element and resizes the map */
          function shrinkMapElement() {
            mapEl.css({
              position  : 'relative',
              top       : '0',
              right     : '0',
              height    : '180px',
              width     : '100%',
              'z-index' : 996,
              'margin-bottom': '-25px'
            });

            mapEl.closest('moloch-graph-map').removeClass('expanded');

            mapEl.resize();
          }

          /**
           * Determines whether a click was outside of the map element
           * and then closes the map if click was outside map
           * @param {Object} e The click event
           */
          function isOutsideClick(e) {
            if (!element.is(e.target) && element.has(e.target).length === 0) {
              $timeout(() => {
                scope.status.expanded = false;
                shrinkMapElement();
              });
            }
          }

          /* expands the map element and resizes the map */
          function expandMapElement() {
            let top = Math.max(mapEl.offset().top - $(window).scrollTop(), 0);

            mapEl.css({
              position: 'fixed',
              right   : '15px',
              'z-index' : 998,
              top     : Math.min(top, $(window).height() * 0.25),
              width   : $(window).width()*0.75,
              height  : $(window).height()*0.75
            });

            mapEl.closest('moloch-graph-map').addClass('expanded');

            mapEl.resize();
          }


          /* exposed functions --------------------------------------------- */
          // map is initially not expanded
          scope.status = { expanded:false };

          /* Expands/shrinks the opened map panel */
          scope.expandMap = function() {
            scope.status.expanded = !scope.status.expanded;

            if (scope.status.expanded) {
              expandMapElement();
              $document.on('mouseup', isOutsideClick);
            } else {
              shrinkMapElement();
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
