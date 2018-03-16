(function() {

  'use strict';

  describe('Field Service ->', function() {

    // load the module
    beforeEach(angular.mock.module('directives.search'));

    var FieldService;

    // load service
    beforeEach(inject(function(_FieldService_) {
      FieldService = _FieldService_;
    }));

    it('should exist', function() {
      expect(FieldService).toBeDefined();
    });

    describe('http requests ->', function() {
      var $httpBackend;

      beforeEach(inject(function(_$httpBackend_) {
        $httpBackend = _$httpBackend_;

        $httpBackend.when('GET', 'fields')
          .respond(200, {});

        $httpBackend.when('GET', 'unique.txt')
          .respond(200, {});

        $httpBackend.when('GET', 'uniqueValue.json')
          .respond(200, {});
      }));

      afterEach(function() {
        $httpBackend.verifyNoOutstandingExpectation();
        $httpBackend.verifyNoOutstandingRequest();
      });

      it('should send a GET request for fields', function() {
        var result = FieldService.get();
        $httpBackend.expectGET('fields');
        $httpBackend.flush();
      });

      it('should send a not send a GET request for cached fields', function() {
        var result = FieldService.get();
        $httpBackend.flush();
      });

      it('should send a GET request for values (unique.txt)', function() {
        var result = FieldService.getValues();
        $httpBackend.expectGET('unique.txt');
        $httpBackend.flush();
      });

    });

  });

})();
