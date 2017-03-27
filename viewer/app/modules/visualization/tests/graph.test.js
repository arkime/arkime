(function() {

  'use strict';

  let graph = {
    dbHisto : [[1, 1], [2, 2]],
    lpHisto : [[1, 1], [2, 2]],
    paHisto : [[1, 1], [2, 2]],
    interval: 1,
    xmax    : 2,
    xmin    : 1
  };

  describe('Session Graph Component ->', function() {

    // load the module// load the module and enable debug info (to access isolateScope)
    beforeEach(function() {
      angular.mock.module('moloch', function (_$compileProvider_, $provide) {
        _$compileProvider_.debugInfoEnabled(true);
        $provide.value('$window', {
          getComputedStyle: () => {
            return {
              getPropertyValue: () => {
                return '#000';
              }
            };
          }
        });
        $provide.value('$document', angular.element(document));
      });
    });

    let scope, isolateScope, compiledTemplate, templateAsHtml, $timeout, $httpBackend;

    // Initialize and a mock scope
    beforeEach(inject(function(
      $compile,
      _$timeout_,
      $rootScope, _$httpBackend_) {
        $httpBackend = _$httpBackend_;
        $timeout = _$timeout_;

        scope = $rootScope.$new();

        scope.graphData   = graph;

        let htmlString    = '<session-graph graph-data="graphData"></session-graph>';

        let element       = angular.element(htmlString);
        compiledTemplate  = $compile(element)(scope);
        isolateScope      = compiledTemplate.isolateScope();

        scope.$digest();

        templateAsHtml    = compiledTemplate.html();

        spyOn(isolateScope, '$emit').and.callThrough();
    }));

    it('should render html with graph data', function() {
      expect(templateAsHtml).toBeDefined();
      expect(scope.graphData).toBeDefined();
    });

    it('should render the graph', function() {
      let plotArea = compiledTemplate.find('.plot-area');
      expect(plotArea).toBeDefined();
      expect(plotArea.length).toBeGreaterThan(0);

      let canvas = plotArea.find('canvas');
      expect(canvas).toBeDefined();
      expect(canvas.length).toBeGreaterThan(0);
    });

    it('should be able to zoom out', function() {
      isolateScope.zoomOut();
      $timeout.flush();
      expect(isolateScope.$emit).toHaveBeenCalled();
    });

    it('should be able pan left', function() {
      isolateScope.panLeft();
      $timeout.flush();
      expect(isolateScope.$emit).toHaveBeenCalled();
    });

    it('should be able pan right', function() {
      isolateScope.panRight();
      $timeout.flush();
      expect(isolateScope.$emit).toHaveBeenCalled();
    });

    it('should be able to select time range within plot', function() {
      let plotArea = compiledTemplate.find('.plot-area');
      let ranges = { xaxis: { from: 3600, to: 36000000000 }};
      plotArea.triggerHandler('plotselected', ranges);
      expect(isolateScope.$emit).toHaveBeenCalled();
    });

  });

})();
