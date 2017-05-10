(function() {

  'use strict';

  describe('Footer Directive ->', function() {

    // load the module
    beforeEach(angular.mock.module('moloch'));
    beforeEach(angular.mock.module('directives.footer'));

    var scope, footer, templateAsHtml;

    // Initialize and a mock scope
    beforeEach(inject(function(
      $componentController,
      $rootScope,
      $compile,
      Constants) {

      scope = $rootScope.$new();

      var element = angular.element('<footer></footer>');
      var template = $compile(element)(scope);

      footer = $componentController('footer', {
        Constants: Constants
      });

      scope.$digest();

      templateAsHtml = template.html();
    }));

    it('should exist and have version', function() {
      expect(templateAsHtml).toBeDefined();
      expect(footer.molochVersion).toBeDefined();
    });

  });

})();
