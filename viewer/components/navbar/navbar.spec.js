(function() {

  'use strict';

  describe('Navbar Directive ->', function() {

    // load the module
    beforeEach(module('directives.navbar'));

    // load templates
    beforeEach(module('templates'));

    var scope, navbar, templateAsHtml;

    // Initialize and a mock scope
    beforeEach(inject(function($componentController, $location, $rootScope, $compile) {
      scope = $rootScope.$new();

      var element   = angular.element('<navbar></navbar>');
      var template  = $compile(element)(scope);

      navbar = $componentController('navbar', {
        $location : $location
      });

      spyOn(navbar, 'isActive').and.callThrough();

      scope.$digest();
      templateAsHtml = template.html();

      // set default location
      navbar.$location.path('/session');

      // initialize pagination component controller
      navbar.$onInit();
    }));

    it('should exist and have dependencies', function() {
      expect(navbar).toBeDefined();
      expect(navbar.$location).toBeDefined();
    });

    it('should render html with menu items', function() {
      expect(templateAsHtml).toBeDefined();
      for (var key in navbar.menu) {
        expect(templateAsHtml).toContain(navbar.menu[key].title);
      }
    });

    it('should have menu items', function() {
      expect(navbar.menu).toBeDefined();
      expect(navbar.menu.session).toBeDefined();
    });

    it('should verify active route', function() {
      expect(navbar.isActive('/app#/session')).toBeTruthy();
    });

  });

})();
