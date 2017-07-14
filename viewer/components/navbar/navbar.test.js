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
      Constants) {

      $httpBackend = _$httpBackend_;

      $httpBackend.expectGET('eshealth.json').respond({});
      $httpBackend.expectGET('users/current').respond({});

      scope = $rootScope.$new();

      let element   = angular.element('<navbar></navbar>');
      let template  = $compile(element)(scope);

      navbar = $componentController('navbar', {
        $location: $location,
        Constants: Constants
      });

      scope.$digest();
      templateAsHtml = template.html();

      // set default location
      navbar.$location.path('/sessions');

      // initialize component controller
      navbar.$onInit();
    }));

    it('should exist and have dependencies', function() {
      expect(navbar).toBeDefined();
      expect(navbar.$location).toBeDefined();
      expect(navbar.molochVersion).toBeDefined();
      expect(navbar.demoMode).toBeDefined();
    });

    it('should render html with menu items', function() {
      expect(templateAsHtml).toBeDefined();
      for (let key in navbar.menu) {
        expect(templateAsHtml).toContain(navbar.menu[key].title);
      }
    });

    it('should have menu items', function() {
      expect(navbar.menu).toBeDefined();
      expect(navbar.menu.sessions).toBeDefined();
      expect(navbar.menu.spiview).toBeDefined();
      expect(navbar.menu.spigraph).toBeDefined();
      expect(navbar.menu.connections).toBeDefined();
      expect(navbar.menu.files).toBeDefined();
      expect(navbar.menu.stats).toBeDefined();
      expect(navbar.menu.settings).toBeDefined();
      expect(navbar.menu.users).toBeDefined();
    });

    it('should verify active route', function() {
      expect(navbar.isActive('sessions')).toBeTruthy();
    });

    it('should add #settings to the url', function() {
      // navigating from the settings page to the help page should append
      // #settings to the url
      navbar.$location.url('settings');
      navbar.navTabClick('help', { preventDefault: () => {}, stopPropagation: () => {} });

      expect(navbar.$location.hash()).toEqual('settings');
    });

  });

})();
