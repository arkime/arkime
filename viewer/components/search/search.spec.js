(function() {

  'use strict';

  describe('Search Directive ->', function() {

    // load the module
    beforeEach(angular.mock.module('directives.search'));

    var scope, search, templateAsHtml;

    // Initialize and a mock scope
    beforeEach(inject(function($componentController, $rootScope, $compile) {
      scope = $rootScope.$new();
      var htmlString = '<moloch-search></moloch-search>';

      var element   = angular.element(htmlString);
      var template  = $compile(element)(scope);

      scope.$digest();

      templateAsHtml = template.html();

      search = $componentController('molochSearch', {
        $scope: scope
      });

      // spy on emit event
      spyOn(scope, '$emit').and.callThrough();

      // initialize search component controller
      search.$onInit();
    }));

    it('should exist and have dependencies', function() {
      expect(search).toBeDefined();
      expect(search.$scope).toBeDefined();
    });

    it('should render html', function() {
      expect(templateAsHtml).toBeDefined();
    });

  });

})();
