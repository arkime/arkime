(function() {

  'use strict';

  describe('Footer Directive ->', function() {

    // load the module
    beforeEach(angular.mock.module('directives.footer'));

    var scope, templateAsHtml;

    // Initialize and a mock scope
    beforeEach(inject(function($rootScope, $compile) {
      scope = $rootScope.$new();

      var element = angular.element('<footer></footer>');
      var template = $compile(element)(scope);

      scope.$digest();

      templateAsHtml = template.html();
    }));

    it('should exist', function() {
      expect(templateAsHtml).toBeDefined();
    });

  });

})();
