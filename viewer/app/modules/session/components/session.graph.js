(function() {

  'use strict';

  angular.module('moloch')

    /**
     * Moloch Session Graph Directive
     * uses Angular Flot
     *
     * @example
     * <session-graph graph-data="$ctrl.graphData"></session-graph>
     */
    .directive('sessionGraph', ['$filter', function($filter) {
      return {
        template: require('html!../templates/session.graph.html'),
        scope   : { graphData: '=' },
        link    : function(scope, element, attrs) {

          scope.type = 'lpHisto';

          scope.changeHistoType = function() {
            scope.graph = [{ data:scope.graphData[scope.type] }];
          };

          var interval = scope.graphData.interval * 1000;

          scope.graph = [{ data:scope.graphData.lpHisto }];
          scope.graphOptions = { // flot graph options
            series  : {
              bars  : {
                show: true,
                fill: 1,
                barWidth: (scope.graphData.interval * 1000)/1.7
              },
              color : '#28A482'
            },
            selection : {
              mode    : 'x',
              color   : '#333333'
            },
            xaxis   : {
              mode  : 'time',
              label : 'Datetime',
              color : '#777',
              tickFormatter: function(v, axis) {
                return $filter('date')(v, 'yyyy/MM/dd HH:mm:ss');
              },
              min   : scope.graphData.xmin || null,
              max   : scope.graphData.xmax || null
            },
            yaxis   : {
              min   : 0,
              color : '#777',
              tickFormatter: function(v, axis) {
                if (v === undefined) { return '0'; }
                return scope.commaString(v);
              }
            },
            grid          : {
              borderWidth : 0,
              color       : '#777',
              hoverable   : true,
              clickable   : true
            },
            zoom          : {
              interactive : false,
              trigger     : 'dblclick',
              amount      : 2
            },
            pan           : {
              interactive : false,
              cursor      : 'move',
              frameRate   : 20
            }
          };


          element.bind('plotselected', function (event, ranges) {
            var result = {
              start : ranges.xaxis.from / 1000,
              stop  : ranges.xaxis.to / 1000
            };

            if (result.start && result.stop) {
              scope.$emit('change:time', result);
            }
      		});


          var previousPoint;
          element.bind('plothover', function(event, pos, item) {
            if (item) {
              if (previousPoint !== item.dataIndex) {
                previousPoint = item.dataIndex;

                element.find('#tooltip').remove();

                var y = scope.commaString(Math.round(item.datapoint[1]*100)/100);
                var d = $filter('date')(item.datapoint[0].toFixed(0), 'yyyy/MM/dd HH:mm:ss');

                var tooltipHTML = `<div id="tooltip" class="graph-tooltip">
                                    ${y} at ${d}</div>`;
                $(tooltipHTML).css({
                  top : item.pageY - 30,
                  left: item.pageX - 8
                }).appendTo(element);
              }
            } else {
              element.find('#tooltip').remove();
              previousPoint = null;
            }
          });

          scope.commaString = function(val) {
            return val.toString().replace(/\B(?=(\d{3})+(?!\d))/g, ',');
          };

        }
      };
    }]);

})();
