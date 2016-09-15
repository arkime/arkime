(function() {

  'use strict';

  describe('Health Service ->', function() {

    // load the module
    beforeEach(angular.mock.module('moloch'));

    var HealthService;

    // load service
    beforeEach(inject(function(_HealthService_) {
      HealthService = _HealthService_;
    }));

    it('should exist', function() {
      expect(HealthService).toBeDefined();
    });

    describe('http requests ->', function() {
      var $httpBackend;

      beforeEach(inject(function(_$httpBackend_) {
        $httpBackend = _$httpBackend_;

        $httpBackend.when('GET', 'eshealth.json')
          .respond(200, {});
      }));

      afterEach(function() {
        $httpBackend.verifyNoOutstandingExpectation();
        $httpBackend.verifyNoOutstandingRequest();
      });

      it('should send a GET request for es health', function() {
        var result = HealthService.esHealth();
        $httpBackend.expectGET('eshealth.json');
        $httpBackend.flush();
      });

    });

  });

})();
