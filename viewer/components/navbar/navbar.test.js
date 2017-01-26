(function() {

  'use strict';

  let fakeWindow = { location: { href: '' } };

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
      $httpBackend.expectGET('users/current').respond({});

      scope = $rootScope.$new();

      let element   = angular.element('<navbar></navbar>');
      let template  = $compile(element)(scope);

      navbar = $componentController('navbar', {
        $location     : $location,
        $window       : fakeWindow,
        molochVersion : molochVersion
      });

      scope.$digest();
      templateAsHtml = template.html();

      // set default location
      navbar.$location.path('/app');

      // initialize component controller
      navbar.$onInit();
    }));

    it('should exist and have dependencies', function() {
      expect(navbar).toBeDefined();
      expect(navbar.$location).toBeDefined();
      expect(navbar.$window).toBeDefined();
      expect(navbar.molochVersion).toBeDefined();
    });

    it('should render html with menu items', function() {
      expect(templateAsHtml).toBeDefined();
      for (let key in navbar.menu) {
        expect(templateAsHtml).toContain(navbar.menu[key].title);
      }
    });

    it('should have menu items', function() {
      expect(navbar.menu).toBeDefined();
      expect(navbar.menu.session).toBeDefined();
    });

    it('should verify active route', function() {
      expect(navbar.isActive('app')).toBeTruthy();
    });

    it('should add #settings to the url', function() {
      // navigating from the settings page to the help page should append
      // #settings to the url
      navbar.$location.path('/settings');
      navbar.navTabClick('help');

      expect(fakeWindow.location.href.contains('#settings')).toBeTruthy();
    });

  });

})();
