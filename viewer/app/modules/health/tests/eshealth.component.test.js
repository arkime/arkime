(function() {

  'use strict';

  describe('ES Health Directive ->', function() {

    // load the module
    beforeEach(angular.mock.module('moloch'));

    var scope, eshealth, templateAsHtml, $httpBackend;

    // Initialize and a mock scope
    beforeEach(inject(function(
      _$httpBackend_,
      $componentController,
      $interval,
      $rootScope,
      $compile,
      HealthService) {

      $httpBackend = _$httpBackend_;

      $httpBackend.expectGET('eshealth.json').respond({});

      scope = $rootScope.$new();

      var element   = angular.element('<es-health></es-health>');
      var template  = $compile(element)(scope);

      eshealth = $componentController('esHealth', {
        $interval     : $interval,
        HealthService : HealthService
      });

      scope.$digest();
      templateAsHtml = template.html();

      // initialize component controller
      eshealth.$onInit();
    }));

    it('should exist and have dependencies', function() {
      expect(eshealth).toBeDefined();
      expect(eshealth.$interval).toBeDefined();
      expect(eshealth.HealthService).toBeDefined();
    });

  });

})();
