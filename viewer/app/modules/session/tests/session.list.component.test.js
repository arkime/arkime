(function() {

  'use strict';

  /* mock data ------------------------------- */
  // default session query
  var query = {
    length: 100,  // page length
    start : 0,    // first item index
    facets: 1,    // facets
    sorts : [ [ 'fp', 'asc' ] ],
    fields: [ 'pr', 'fp', 'lp', 'a1', 'p1', 'a2', 'p2', 'pa', 'by', 'no' ]
  };

  // sample session json
  var sessionsJSON = {
    recordsFiltered : 1,
    recordsTotal    : 100,
    data: [
      {
        pa2   :0,
        p1    :10000,
        no    :'demo',
        pa1   :1,
        p2    :2948,
        pr    :17,
        lp    :0,
        fp    :0,
        a1    :16843009,
        a2    :33686018,
        pa    :1,
        db1   :437,
        db2   :0,
        by    :445,
        by2   :0,
        by1   :445,
        db    :437,
        index :'sessions-',
        id    :'--RzB4CrWwqlMZIHRa0CzjUYn'
      }
    ]
  };

  var fields = {
    'protocols': {
      dbField: 'prot-term',
      exp: 'protocols',
      group: 'general',
      friendlyName: 'Protocols',
      help: 'Protocols set for session',
      type: 'termfield'
    },
    'starttime': {
      dbField: 'fp',
      exp: 'starttime',
      group: 'general',
      friendlyName: 'Start Time',
      help: 'Session Start Time',
      type: 'seconds'
    },
    'stoptime': {
      dbField: 'lp',
      exp: 'stoptime',
      group: 'general',
      friendlyName: 'Stop Time',
      help: 'Session Stop Time',
      type: 'seconds'
    },
    'ip.src': {
      dbField: 'a1',
      exp: 'ip.src',
      type: 'ip',
      category: 'ip',
      group: 'general',
      friendlyName: 'Src IP',
      help: 'Source IP'
    },
    'ip.dst': {
      dbField: 'a2',
      exp: 'ip.dst',
      type: 'ip',
      category: 'ip',
      group: 'general',
      friendlyName: 'Dst IP',
      help: 'Destination IP'
    },
    'port.src': {
      dbField: 'p1',
      exp: 'port.src',
      type: 'integer',
      category: 'port',
      group: 'general',
      friendlyName: 'Src Port',
      help: 'Source Port'
    },
    'port.dst': {
      dbField: 'p2',
      exp: 'port.dst',
      type: 'integer',
      category: 'port',
      group: 'general',
      friendlyName: 'Dst Port',
      help: 'Destination Port'
    },
    'packets': {
      dbField: 'pa',
      exp: 'packets',
      type: 'integer',
      group: 'general',
      friendlyName: 'Packets',
      help: 'Total number of packets sent AND received in a session'
    },
    'bytes': {
      dbField: 'by',
      exp: 'bytes',
      type: 'integer',
      group: 'general',
      friendlyName: 'Bytes',
      help: 'Total number of raw bytes sent AND received in a session'
    },
    'no': {
      dbField: 'no',
      exp: 'node',
      group: 'general',
      friendlyName: 'Moloch Node',
      help: 'Moloch node name the session was recorded on',
      type: 'termfield'
    },
    'info': {
      width: 250,
      dbField: 'info',
      exp: 'info',
      group: 'general',
      friendlyName: 'Info',
      help: 'Information',
      children: ['us', 'esrc', 'edst', 'esub', 'efn', 'dnsho', 'tls.alt', 'ircch']
    },
    'http.uri': { dbField: 'us', exp: 'http.uri' },
    'email.src': { dbField: 'esrc', exp: 'email.src' },
    'email.dst': { dbField: 'edst', exp: 'email.dst' },
    'email.subject': { dbField: 'esub', exp: 'email.subject' },
    'email.fn': { dbField: 'efn', exp: 'email.fn' },
    'host.dns': { dbField: 'dnsho', exp: 'host.dns' },
    'cert.alt': { dbField: 'tls.alt', exp: 'cert.alt' },
    'irc.channel': { dbField: 'ircch', exp: 'irc.channel' }
  };

  describe('Session Component ->', function() {

    // load the module
    beforeEach(angular.mock.module('moloch'));

    var scope, sessionComponent, $httpBackend;
    var sessionsEndpoint    = 'sessions.json';
    var defaultParameters   = '?facets=1&fields=pr,fp,lp,a1,p1,a2,p2,pa,by,no,us,esrc,edst,esub,efn,dnsho,tls.alt,ircch&length=100&order=fp:asc';
    var tableStateEndpoint  = 'tableState/sessionsNew';

    // Initialize and a mock scope
    beforeEach(inject(function(
      _$httpBackend_,
      $routeParams,
      SessionService,
      $componentController,
      $rootScope) {
        $httpBackend = _$httpBackend_;

        // initial query for table state
        $httpBackend.expectGET(tableStateEndpoint)
           .respond({});

        // initial query for fields
        $httpBackend.expectGET('fields')
           .respond(fields);

        // initial query for sessions
        $httpBackend.expectGET(sessionsEndpoint + defaultParameters)
           .respond(sessionsJSON);

        scope = $rootScope.$new();

        sessionComponent = $componentController('session', {
          $scope            : scope,
          $routeParams      : $routeParams,
          SessionService    : SessionService
        });

        // spy on functions called in controller
        spyOn(sessionComponent, 'getData').and.callThrough();
        spyOn(sessionComponent, 'getTableState').and.callThrough();

        // initialize session component controller
        sessionComponent.$onInit();
        $httpBackend.flush();
    }));

    afterEach(function() {
      $httpBackend.verifyNoOutstandingExpectation();
      $httpBackend.verifyNoOutstandingRequest();
      sessionComponent.tableState = {};
    });

    it('should exist, have dependencies, and fetch data', function() {
      expect(sessionComponent).toBeDefined();
      expect(sessionComponent.$scope).toBeDefined();
      expect(sessionComponent.$routeParams).toBeDefined();
      expect(sessionComponent.SessionService).toBeDefined();
    });

    it('should fetch the table data', function() {
      expect(sessionComponent.getData).toHaveBeenCalled();
      expect(sessionComponent.getData).toHaveBeenCalledWith();
      expect(sessionComponent.sessions).toEqual(sessionsJSON);
    });

    it('should have smart query defaults', function() {
      expect(sessionComponent.query).toBeDefined();
      expect(sessionComponent.query).toEqual(query);
      expect(sessionComponent.currentPage).toEqual(1);
    });

    it('should fetch the table state', function() {
      expect(sessionComponent.getTableState).toHaveBeenCalled();
      expect(sessionComponent.getTableState).toHaveBeenCalledWith();
      expect(sessionComponent.columnInfo).toEqual({});
    });

    describe('listeners ->', function() {
      var sorts       = [['fp', 'asc']];
      var length      = 10;
      var currentPage = 2;
      var start       = (currentPage - 1) * length;
      var sub_scope;

      beforeEach(function() {
        sub_scope = scope.$new();
      });

      it('should listen for "change:search" event', function() {
        var newParameters = '?facets=1&length=100&order=fp:asc';
        $httpBackend.expectGET(sessionsEndpoint + newParameters)
          .respond(sessionsJSON);

        sub_scope.$emit('change:search', {
          startTime: 0, stopTime: 0, expression: ''
        });

        $httpBackend.flush();

        expect(sessionComponent.getData).toHaveBeenCalled();
        expect(sessionComponent.getData).toHaveBeenCalledWith();
      });

      it('should listen for "change:sort" event', function() {
        var newParameters = '?facets=1&length=100&order=fp:asc';
        $httpBackend.expectGET(sessionsEndpoint + newParameters)
          .respond(sessionsJSON);

        sub_scope.$emit('change:sort', { sorts:sorts });

        $httpBackend.flush();

        expect(sessionComponent.query.sorts).toEqual(sorts);
        expect(sessionComponent.getData).toHaveBeenCalled();
        expect(sessionComponent.getData).toHaveBeenCalledWith();
      });

      it('should listen for "change:pagination" event', function() {
        var newParameters = '?facets=1&length=10&order=fp:asc&start=10';
        $httpBackend.expectGET(sessionsEndpoint + newParameters)
          .respond(sessionsJSON);

        sub_scope.$emit('change:pagination',
          { length:length, currentPage:currentPage, start:start }
        );

        $httpBackend.flush();

        expect(sessionComponent.query.length).toEqual(length);
        expect(sessionComponent.query.start).toEqual(start);
        expect(sessionComponent.currentPage).toEqual(currentPage);
        expect(sessionComponent.getData).toHaveBeenCalled();
        expect(sessionComponent.getData).toHaveBeenCalledWith();
      });
    });

  });

})();
