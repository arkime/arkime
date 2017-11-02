(function() {

  'use strict';

  describe('Error Directive ->', function() {

    // load the module
    beforeEach(angular.mock.module('directives.noresults'));

    var scope, templateAsHtml, compile;

    // Initialize and a mock scope
    beforeEach(inject(function($rootScope, $compile) {
        compile = $compile;

        scope = $rootScope.$new();

        scope.recordsTotal  = '100';
        scope.view          = 'superview';

        var element   = angular.element(
          `<noresults records-total="100" view="superview"></noresults>`
        );
        var template  = compile(element)(scope);

        scope.$digest();

        templateAsHtml = template.html();
    }));

    it('should render html', function() {
      expect(templateAsHtml).toBeDefined();
    });

    it('should have the proper messages displayed', function() {
      expect(templateAsHtml).toContain('No results or none that match your search within your time range.');
      expect(templateAsHtml).toContain(scope.view);
    });

    it('should not display view area if no view is specified', function() {
      var element   = angular.element(
        `<noresults records-total="100"></noresults>`
      );
      var template  = compile(element)(scope);

      scope.$digest();

      templateAsHtml = template.html();

      expect(templateAsHtml).not.toContain(scope.view);
    });

    it('should display that moloch has no data', function() {
      var element   = angular.element(
        `<noresults records-total="0"></noresults>`
      );
      var template  = compile(element)(scope);

      scope.$digest();

      templateAsHtml = template.html();

      expect(templateAsHtml).toContain('Oh no, Moloch is empty! There is no data to search.');
    });

  });

})();
