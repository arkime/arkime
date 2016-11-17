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

        $httpBackend.when('GET', 'sessions.json?facets=1&length=100&order=fp:asc')
          .respond(200, {});

        $httpBackend.when('GET', 'tableState/sessions')
          .respond(200, {});

        $httpBackend.when('GET', 'sessionid/node/sessionDetailNew')
          .respond(200, '');

        $httpBackend.when('POST', 'addTags')
          .respond(200, {});

        $httpBackend.when('POST', 'removeTags')
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
          length: 100,// page length
          start : 0,  // first item index
          sorts : [{element:'fp',order:'asc'}], // array of sort objects
          facets: 1   // facets
        };

        SessionService.get(query);
        $httpBackend.expectGET('sessions.json?facets=1&length=100&order=fp:asc');
        $httpBackend.flush();
      });

      it('should send a GET request for tablestate', function() {
        SessionService.getColumnInfo();
        $httpBackend.expectGET('tableState/sessions');
        $httpBackend.flush();
      });

      it('should send a GET request for session detail', function() {
        SessionService.getDetail('node', 'sessionid', {});
        $httpBackend.expectGET('sessionid/node/sessionDetailNew');
        $httpBackend.flush();
      });

      it('should send a POST request to add tags', function() {
        SessionService.addTags('sessionid', 'tag', 'no');
        $httpBackend.expectPOST('addTags');
        $httpBackend.flush();
      });

      it('should send a POST request to remove tags', function() {
        SessionService.removeTags('sessionid', 'tag', 'no');
        $httpBackend.expectPOST('removeTags');
        $httpBackend.flush();
      });

    });

  });

})();
