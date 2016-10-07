(function() {

  'use strict';

  describe('Navbar Directive ->', function() {

    // load the module
    beforeEach(angular.mock.module('moloch'));
    beforeEach(angular.mock.module('directives.navbar'));

    var scope, navbar, templateAsHtml, $httpBackend;

    // Initialize and a mock scope
    beforeEach(inject(function(
      _$httpBackend_,
      $componentController,
      $location,
      $rootScope,
      $compile,
      molochVersion) {

      $httpBackend = _$httpBackend_;

      $httpBackend.expectGET('eshealth.json').respond({});
      $httpBackend.expectGET('currentuser').respond({});

      scope = $rootScope.$new();

      var element   = angular.element('<navbar></navbar>');
      var template  = $compile(element)(scope);

      navbar = $componentController('navbar', {
        $location     : $location,
        molochVersion : molochVersion
      });

      spyOn(navbar, 'isActive').and.callThrough();

      scope.$digest();
      templateAsHtml = template.html();

      // set default location
      navbar.$location.path('/session');

      // initialize component controller
      navbar.$onInit();
    }));

    it('should exist and have dependencies', function() {
      expect(navbar).toBeDefined();
      expect(navbar.$location).toBeDefined();
      expect(navbar.molochVersion).toBeDefined();
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
