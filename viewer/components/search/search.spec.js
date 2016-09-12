(function() {

  'use strict';

  describe('Search Directive ->', function() {

    // load the module
    beforeEach(angular.mock.module('moloch'));
    beforeEach(angular.mock.module('directives.search'));
    beforeEach(angular.mock.module('moloch.util'));

    var scope, search, templateAsHtml, $httpBackend;

    // Initialize and a mock scope
    beforeEach(inject(function(_$httpBackend_, $componentController, $rootScope, $compile, $location) {
      $httpBackend = _$httpBackend_;

      $httpBackend.expectGET('/fields')
        .respond({});

      scope = $rootScope.$new();
      var htmlString = '<moloch-search></moloch-search>';

      var element   = angular.element(htmlString);
      var template  = $compile(element)(scope);

      // scope.$digest();

      templateAsHtml = template.html();

      search = $componentController('molochSearch', {
        $scope      : scope,
        $routeParams: {},
        $location   : $location
      });

      // spy on emit event
      spyOn(scope, '$emit').and.callThrough();

      // initialize search component controller
      // search.$onInit();
      // $httpBackend.flush();
    }));

    // afterEach(function() {
    //   $httpBackend.verifyNoOutstandingExpectation();
    //   $httpBackend.verifyNoOutstandingRequest();
    // });

    it('should exist and have dependencies', function() {
      expect(search).toBeDefined();
      expect(search.$scope).toBeDefined();
      expect(search.$routeParams).toBeDefined();
      expect(search.$location).toBeDefined();
    });

    it('should render html', function() {
      expect(templateAsHtml).toBeDefined();
    });

  });

})();
