(function() {

  'use strict';

  describe('Session Service ->', function() {

    // load the module
    beforeEach(angular.mock.module('moloch'));

    let SessionService;

    let openSessionsParams = {
      sessions:[{ id:'sessionid' }],
      segments:'no'
    };

    let visibleSessionsParams = {
      segments  : 'no',
      applyTo   : 'visible',
      start     : 1,
      numVisible: 100
    };

    let matchingSessionsParams = {
      segments    : 'all',
      applyTo     : 'matching',
      numMatching : 500
    };

    let tags = 'tag1,tag2';

    // load service
    beforeEach(inject(function(_SessionService_) {
      SessionService = _SessionService_;
    }));

    it('should exist', function() {
      expect(SessionService).toBeDefined();
    });

    describe('http requests ->', function() {
      let $httpBackend;

      beforeEach(inject(function(_$httpBackend_) {
        $httpBackend = _$httpBackend_;

        $httpBackend.when('GET', 'sessions.json?flatten=1')
           .respond(200, {});

        $httpBackend.when('GET', 'sessions.json?facets=1&flatten=1&length=100&order=fp:asc')
          .respond(200, {});

        $httpBackend.when('GET', 'node/session/sessionid/detail')
           .respond(200, '');

        $httpBackend.when('GET', 'node/session/sessionid/packets')
           .respond(200, '');

        $httpBackend.when('GET', 'node/session/sessionid/packets?base=hex&decode=%7B%7D&gzip=false&image=false&line=false&packets=200&ts=false')
           .respond(200, '');

        $httpBackend.when('GET', 'state/sessionsNew')
          .respond(200, {});

        $httpBackend.when('POST', 'state/sessionsNew')
          .respond(200, {});

        // add tags posts
        $httpBackend.when('POST', 'addTags',
           function(postData) {
             let jsonData = JSON.parse(postData);
             expect(jsonData.ids).toBe(openSessionsParams.sessions[0].id);
             expect(jsonData.tags).toBe(tags);
             expect(jsonData.segments).toBe(openSessionsParams.segments);
             return true;
           }
        ).respond(200);
        $httpBackend.when('POST', 'addTags?start=1&length=100',
           function(postData) {
             let jsonData = JSON.parse(postData);
             expect(jsonData.ids).not.toBeDefined();
             expect(jsonData.tags).toBe(tags);
             expect(jsonData.segments).toBe(visibleSessionsParams.segments);
             return true;
           }
        ).respond(200);
        $httpBackend.when('POST', 'addTags?start=0&length=500',
           function(postData) {
             let jsonData = JSON.parse(postData);
             expect(jsonData.ids).not.toBeDefined();
             expect(jsonData.tags).toBe(tags);
             expect(jsonData.segments).toBe(matchingSessionsParams.segments);
             return true;
           }
        ).respond(200);

        // remove tags posts
        $httpBackend.when('POST', 'removeTags',
           function(postData) {
             let jsonData = JSON.parse(postData);
             expect(jsonData.ids).toBe(openSessionsParams.sessions[0].id);
             expect(jsonData.tags).toBe(tags);
             expect(jsonData.segments).toBe(openSessionsParams.segments);
             return true;
           }
        ).respond(200);
        $httpBackend.when('POST', 'removeTags?start=1&length=100',
           function(postData) {
             let jsonData = JSON.parse(postData);
             expect(jsonData.ids).not.toBeDefined();
             expect(jsonData.tags).toBe(tags);
             expect(jsonData.segments).toBe(visibleSessionsParams.segments);
             return true;
           }
        ).respond(200);
        $httpBackend.when('POST', 'removeTags?start=0&length=500',
           function(postData) {
             let jsonData = JSON.parse(postData);
             expect(jsonData.ids).not.toBeDefined();
             expect(jsonData.tags).toBe(tags);
             expect(jsonData.segments).toBe(matchingSessionsParams.segments);
             return true;
           }
        ).respond(200);

        // scrub posts
        $httpBackend.when('POST', 'scrub',
           function(postData) {
             let jsonData = JSON.parse(postData);
             expect(jsonData.ids).toBe(openSessionsParams.sessions[0].id);
             expect(jsonData.segments).toBe(openSessionsParams.segments);
             return true;
           }
        ).respond(200);
        $httpBackend.when('POST', 'scrub?start=1&length=100',
           function(postData) {
             let jsonData = JSON.parse(postData);
             expect(jsonData.ids).not.toBeDefined();
             expect(jsonData.segments).toBe(visibleSessionsParams.segments);
             return true;
           }
        ).respond(200);
        $httpBackend.when('POST', 'scrub?start=0&length=500',
           function(postData) {
             let jsonData = JSON.parse(postData);
             expect(jsonData.ids).not.toBeDefined();
             expect(jsonData.segments).toBe(matchingSessionsParams.segments);
             return true;
           }
        ).respond(200);

        // delete posts
        $httpBackend.when('POST', 'delete',
           function(postData) {
             let jsonData = JSON.parse(postData);
             expect(jsonData.ids).toBe(openSessionsParams.sessions[0].id);
             expect(jsonData.segments).toBe(openSessionsParams.segments);
             return true;
           }
        ).respond(200);
        $httpBackend.when('POST', 'delete?start=1&length=100',
           function(postData) {
             let jsonData = JSON.parse(postData);
             expect(jsonData.ids).not.toBeDefined();
             expect(jsonData.segments).toBe(visibleSessionsParams.segments);
             return true;
           }
        ).respond(200);
        $httpBackend.when('POST', 'delete?start=0&length=500',
           function(postData) {
             let jsonData = JSON.parse(postData);
             expect(jsonData.ids).not.toBeDefined();
             expect(jsonData.segments).toBe(matchingSessionsParams.segments);
             return true;
           }
        ).respond(200);

        // send sessions posts
        $httpBackend.when('POST', 'sendSessions',
           function(postData) {
             let jsonData = JSON.parse(postData);
             expect(jsonData.ids).toBe(openSessionsParams.sessions[0].id);
             expect(jsonData.segments).toBe(openSessionsParams.segments);
             return true;
           }
        ).respond(200);
        $httpBackend.when('POST', 'sendSessions?start=1&length=100',
           function(postData) {
             let jsonData = JSON.parse(postData);
             expect(jsonData.ids).not.toBeDefined();
             expect(jsonData.segments).toBe(visibleSessionsParams.segments);
             return true;
           }
        ).respond(200);
        $httpBackend.when('POST', 'sendSessions?start=0&length=500',
           function(postData) {
             let jsonData = JSON.parse(postData);
             expect(jsonData.ids).not.toBeDefined();
             expect(jsonData.segments).toBe(matchingSessionsParams.segments);
             return true;
           }
        ).respond(200);
      }));

      afterEach(function() {
        $httpBackend.verifyNoOutstandingExpectation();
        $httpBackend.verifyNoOutstandingRequest();
      });

      it('should send a GET request (without parameters) for sessions', function() {
        SessionService.get();
        $httpBackend.expectGET('sessions.json?flatten=1');
        $httpBackend.flush();
      });

      it('should send a GET request (with parameters) for sessions', function() {
        let query = {
          length: 100,// page length
          start : 0,  // first item index
          sorts : [['fp','asc']], // array of sort objects
          facets: 1   // facets
        };

        SessionService.get(query);
        $httpBackend.expectGET('sessions.json?facets=1&flatten=1&length=100&order=fp:asc');
        $httpBackend.flush();
      });

      it('should send a GET request for session detail', function() {
        SessionService.getDetail('sessionid', 'node');
        $httpBackend.expectGET('node/session/sessionid/detail');
        $httpBackend.flush();
      });

      it('should send a GET request for session packets', function() {
        SessionService.getPackets('sessionid', 'node');
        $httpBackend.expectGET('node/session/sessionid/packets');
        $httpBackend.flush();
      });

      it('should send a GET request for session packets (with parameters)', function() {
        let params = {
          base    : 'hex',
          line    : false,
          image   : false,
          gzip    : false,
          ts      : false,
          decode  : {},
          packets : 200
        };
        SessionService.getPackets('sessionid', 'node', params);
        $httpBackend.expectGET('node/session/sessionid/packets?base=hex&decode=%7B%7D&gzip=false&image=false&line=false&packets=200&ts=false');
        $httpBackend.flush();
      });

      it('should be able to cancel a GET request for session packets', function() {
        let promise = SessionService.getPackets('sessionid', 'node');
        promise.abort();
      });

      it('should send a GET request for state', function() {
        SessionService.getState('sessionsNew');
        $httpBackend.expectGET('state/sessionsNew');
        $httpBackend.flush();
      });

      it('should send a POST request for state', function() {
        SessionService.saveState({}, 'sessionsNew');
        $httpBackend.expectPOST('state/sessionsNew');
        $httpBackend.flush();
      });

      it('should send a POST request to add tags', function() {
        openSessionsParams.tags = tags;
        SessionService.addTags(openSessionsParams);
        $httpBackend.expectPOST('addTags');

        visibleSessionsParams.tags = tags;
        SessionService.addTags(visibleSessionsParams);
        $httpBackend.expectPOST('addTags?start=1&length=100');

        matchingSessionsParams.tags = tags;
        SessionService.addTags(matchingSessionsParams);
        $httpBackend.expectPOST('addTags?start=0&length=500');

        $httpBackend.flush();

        // cleanup
        openSessionsParams.tags = null;
        delete openSessionsParams.tags;

        visibleSessionsParams.tags = null;
        delete visibleSessionsParams.tags;

        matchingSessionsParams.tags = null;
        delete matchingSessionsParams.tags;
      });

      it('should send a POST request to remove tags', function() {
        openSessionsParams.tags = tags;
        SessionService.removeTags(openSessionsParams);
        $httpBackend.expectPOST('removeTags');

        visibleSessionsParams.tags = tags;
        SessionService.removeTags(visibleSessionsParams);
        $httpBackend.expectPOST('removeTags?start=1&length=100');

        matchingSessionsParams.tags = tags;
        SessionService.removeTags(matchingSessionsParams);
        $httpBackend.expectPOST('removeTags?start=0&length=500');

        $httpBackend.flush();

        // cleanup
        openSessionsParams.tags = null;
        delete openSessionsParams.tags;

        visibleSessionsParams.tags = null;
        delete visibleSessionsParams.tags;

        matchingSessionsParams.tags = null;
        delete matchingSessionsParams.tags;
      });

      it('should send a POST request to scrub a session', function() {
        SessionService.scrubPCAP(openSessionsParams);
        $httpBackend.expectPOST('scrub');

        SessionService.scrubPCAP(visibleSessionsParams);
        $httpBackend.expectPOST('scrub?start=1&length=100');

        SessionService.scrubPCAP(matchingSessionsParams);
        $httpBackend.expectPOST('scrub?start=0&length=500');

        $httpBackend.flush();
      });

      it('should send a POST request to delete a session', function() {
        SessionService.remove(openSessionsParams);
        $httpBackend.expectPOST('delete');

        SessionService.remove(visibleSessionsParams);
        $httpBackend.expectPOST('delete?start=1&length=100');

        SessionService.remove(matchingSessionsParams);
        $httpBackend.expectPOST('delete?start=0&length=500');

        $httpBackend.flush();
      });

      it('should send a POST request to send a session', function() {
        SessionService.send(openSessionsParams);
        $httpBackend.expectPOST('sendSessions');

        SessionService.send(visibleSessionsParams);
        $httpBackend.expectPOST('sendSessions?start=1&length=100');

        SessionService.send(matchingSessionsParams);
        $httpBackend.expectPOST('sendSessions?start=0&length=500');

        $httpBackend.flush();
      });

    });

  });

})();
