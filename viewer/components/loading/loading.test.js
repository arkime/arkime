(function() {

  'use strict';

  describe('Loading Directive ->', function() {

    // load the module
    beforeEach(angular.mock.module('directives.loading'));

    var scope, templateAsHtml;

    // Initialize and a mock scope
    beforeEach(inject(function($rootScope, $compile) {
      scope = $rootScope.$new();

      var element = angular.element('<loading></loading>');
      var template = $compile(element)(scope);

      scope.$digest();

      templateAsHtml = template.html();
    }));

    it('should exist', function() {
      expect(templateAsHtml).toBeDefined();
    });

  });

})();
