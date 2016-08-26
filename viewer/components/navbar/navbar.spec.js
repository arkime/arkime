(function() {

  'use strict';

  describe('Navbar Directive ->', function() {

    // load the module
    beforeEach(module('directives.navbar'));

    var scope, navbar, $httpBackend;

    // Initialize and a mock scope
    beforeEach(inject(function($componentController, $location, $rootScope) {
      scope = $rootScope.$new();

      navbar = $componentController('navbar', {
        $location : $location
      });

      spyOn(navbar, 'isActive').and.callThrough();

      // set default location
      navbar.$location.path('/session');

      // initialize pagination component controller
      navbar.$onInit();
    }));

    it('should exist and have dependencies', function() {
      expect(navbar).toBeDefined();
      expect(navbar.$location).toBeDefined();
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
