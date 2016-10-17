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

    var scope, compiledTemplate, templateAsHtml;

    // Initialize and a mock scope
    beforeEach(inject(function(
      $compile,
      $rootScope) {
        scope = $rootScope.$new();

        scope.graphData   = graph;

        var htmlString    = '<session-graph graph-data="graphData"></session-graph>';

        var element       = angular.element(htmlString);
        compiledTemplate  = $compile(element)(scope);

        scope.$digest();

        templateAsHtml    = compiledTemplate.html();
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

  });

})();
