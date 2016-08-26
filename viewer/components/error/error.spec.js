(function() {

  'use strict';

  describe('Error Directive ->', function() {

    // load the module
    beforeEach(module('directives.error'));

    // load templates
    beforeEach(module('templates'));

    var scope, templateAsHtml;

    // Initialize and a mock scope
    beforeEach(inject(function($rootScope, $compile) {
        scope = $rootScope.$new();

        var element = angular.element('<error></error>');
        var template = $compile(element)(scope);

        scope.message = 'message';

        scope.$digest();

        templateAsHtml = template.html();
    }));

    it('should bind the error message', function() {
      expect(scope.message).toEqual('message');
      expect(templateAsHtml).toContain(scope.message);
    });

  });

})();
