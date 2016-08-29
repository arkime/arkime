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

        scope.message = 'Error message!';

        var element = angular.element('<error message="message"></error>');
        var template = $compile(element)(scope);

        scope.$digest();

        templateAsHtml = template.html();
    }));

    it('should render html with error message', function() {
      expect(scope.message).toEqual('Error message!');
      expect(templateAsHtml).toBeDefined();
      expect(templateAsHtml).toContain(scope.message);
    });

  });

})();
