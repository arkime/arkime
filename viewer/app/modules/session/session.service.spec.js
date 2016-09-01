(function() {

  'use strict';

  describe('Session Service ->', function() {

    // load the module
    beforeEach(angular.mock.module('moloch'));

    var SessionService;

    // load service
    beforeEach(inject(function(_SessionService_) {
      SessionService = _SessionService_;
    }));

    it('should exist', function() {
      expect(SessionService).toBeDefined();
    });

    describe('http requests ->', function() {
      var $httpBackend;

      beforeEach(inject(function(_$httpBackend_) {
        $httpBackend = _$httpBackend_;

        $httpBackend.when('GET', 'sessions.json')
          .respond(200, {});

        $httpBackend.when('GET', 'sessions.json?date=1&draw=1&facets=1&length=50&order=fp:asc')
          .respond(200, {});

        $httpBackend.when('GET', 'tableState/sessions')
          .respond(200, {});
      }));

      afterEach(function() {
        $httpBackend.verifyNoOutstandingExpectation();
        $httpBackend.verifyNoOutstandingRequest();
      });

      it('should send a GET request (without parameters) for sessions', function() {
        var result = SessionService.get();
        $httpBackend.expectGET('sessions.json');
        $httpBackend.flush();
      });

      it('should send a GET request (with parameters) for sessions', function() {
        var query = {
          length      : 50,   // page length
          start       : 0,    // first item index
          sortElement : 'fp', // sort element (key of session field)
          sortOrder   : 'asc',// sort order ('asc' or 'desc')
          date        : 1,    // date range
          facets      : 1,    // facets
          draw        : 1     // draw
        };
        var result = SessionService.get(query);
        $httpBackend.expectGET('sessions.json?date=1&draw=1&facets=1&length=50&order=fp:asc');
        $httpBackend.flush();
      });

      it('should send a GET request for tablestate', function() {
        var result = SessionService.getColumnInfo();
        $httpBackend.expectGET('tableState/sessions');
        $httpBackend.flush();
      });

    });

  });

})();
