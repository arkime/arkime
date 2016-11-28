(function() {

  'use strict';

  var graph = {
    dbHisto : [[1, 1], [2, 2]],
    lpHisto : [[1, 1], [2, 2]],
    paHisto : [[1, 1], [2, 2]],
    interval: 1,
    xmax    : 2,
    xmin    : 1
  };

  describe('Session Graph Component ->', function() {

    // load the module
    beforeEach(angular.mock.module('moloch'));
    beforeEach(angular.mock.module('directives.search'));

    var scope, isolateScope, compiledTemplate, templateAsHtml, $timeout, $httpBackend;

    // Initialize and a mock scope
    beforeEach(inject(function(
      $compile,
      _$timeout_,
      $rootScope, _$httpBackend_) {
        $httpBackend = _$httpBackend_;
        $timeout = _$timeout_;

        scope = $rootScope.$new();

        scope.graphData   = graph;

        var htmlString    = '<session-graph graph-data="graphData"></session-graph>';

        var element       = angular.element(htmlString);
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
      var plotArea = compiledTemplate.find('.plot-area');
      expect(plotArea).toBeDefined();
      expect(plotArea.length).toBeGreaterThan(0);

      var canvas = plotArea.find('canvas');
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
      var plotArea = compiledTemplate.find('.plot-area');
      var ranges = { xaxis: { from: 3600, to: 36000000000 }};
      plotArea.triggerHandler('plotselected', ranges);
      expect(isolateScope.$emit).toHaveBeenCalled();
    });

  });

})();
