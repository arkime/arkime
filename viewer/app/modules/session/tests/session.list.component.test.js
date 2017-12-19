(function() {

  'use strict';

  /* mock data ------------------------------- */
  // default session query
  let query = {
    length: 50,   // page length
    start : 0,    // first item index
    facets: 1,    // facets
    sorts : [ [ 'firstPacket', 'asc' ] ],
    fields: [ 'ipProtocol', 'firstPacket', 'lastPacket', 'srcIp', 'srcPort', 'dstIp', 'dstPort', 'totPackets', 'totBytes', 'node', 'http.uri', 'email.src', 'email.dst', 'email.subject', 'email.filename', 'dns.host', 'cert.alt', 'irc.channel' ]
  };

  // default table state
  let defaultTableState = {
    order         : [['firstPacket', 'asc']],
    visibleHeaders: ['firstPacket', 'lastPacket', 'src', 'srcPort', 'dst', 'dstPort', 'totPackets', 'dbby', 'node', 'info']
  };

  // sample session json
  let sessionsJSON = {
    recordsFiltered : 1,
    recordsTotal    : 50,
    data: [
      {
        pdstIp       :0,
        srcPort      :10000,
        node         :'demo',
        srcPackets   :1,
        dstPort      :2948,
        ipProtocol   :17,
        lastPacket   :0,
        firstPacket  :0,
        srcIp        :16843009,
        dstIp        :33686018,
        totPackets   :1,
        srcDataBytes :437,
        dstDataBytes :0,
        totBytes     :445,
        dstBytes     :0,
        srcBytes     :445,
        totDataBytes :437,
        index   :'sessions-',
        id      :'sessionid',
        expanded:false
      }
    ]
  };

  let fields = {
    'protocols': {
      dbField: 'protocol',
      exp: 'protocols',
      group: 'general',
      friendlyName: 'Protocols',
      help: 'Protocols set for session',
      type: 'termfield'
    },
    'starttime': {
      dbField: 'firstPacket',
      exp: 'starttime',
      group: 'general',
      friendlyName: 'Start Time',
      help: 'Session Start Time',
      type: 'seconds'
    },
    'stoptime': {
      dbField: 'lastPacket',
      exp: 'stoptime',
      group: 'general',
      friendlyName: 'Stop Time',
      help: 'Session Stop Time',
      type: 'seconds'
    },
    'ip.src': {
      dbField: 'srcIp',
      exp: 'ip.src',
      type: 'ip',
      category: 'ip',
      group: 'general',
      friendlyName: 'Src IP',
      help: 'Source IP'
    },
    'ip.dst': {
      dbField: 'dstIp',
      exp: 'ip.dst',
      type: 'ip',
      category: 'ip',
      group: 'general',
      friendlyName: 'Dst IP',
      help: 'Destination IP'
    },
    'port.src': {
      dbField: 'srcPort',
      exp: 'port.src',
      type: 'integer',
      category: 'port',
      group: 'general',
      friendlyName: 'Src Port',
      help: 'Source Port'
    },
    'port.dst': {
      dbField: 'dstPort',
      exp: 'port.dst',
      type: 'integer',
      category: 'port',
      group: 'general',
      friendlyName: 'Dst Port',
      help: 'Destination Port'
    },
    'packets': {
      dbField: 'totPackets',
      exp: 'packets',
      type: 'integer',
      group: 'general',
      friendlyName: 'Packets',
      help: 'Total number of packets sent AND received in a session'
    },
    'bytes': {
      dbField: 'totBytes',
      exp: 'bytes',
      type: 'integer',
      group: 'general',
      friendlyName: 'Bytes',
      help: 'Total number of raw bytes sent AND received in a session'
    },
    'node': {
      dbField: 'node',
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
      unsortable: true,
      children: ['http.uri', 'email.src', 'email.dst', 'email.subject', 'email.filename', 'dns.host', 'cert.alt', 'irc.channel']
    },
    'http.uri': { dbField: 'http.uri', exp: 'http.uri' },
    'email.src': { dbField: 'email.src', exp: 'email.src' },
    'email.dst': { dbField: 'email.dst', exp: 'email.dst' },
    'email.subject': { dbField: 'email.subject', exp: 'email.subject' },
    'email.fn': { dbField: 'email.filename', exp: 'email.fn' },
    'host.dns': { dbField: 'dns.host', exp: 'host.dns' },
    'cert.alt': { dbField: 'cert.alt', exp: 'cert.alt' },
    'irc.channel': { dbField: 'irc.channel', exp: 'irc.channel' }
  };

  let settings = {
    timezone      : 'local',
    detailFormat  : 'last',
    showTimestamps: 'last',
    sortColumn    : 'last',
    sortDirection : 'asc',
    spiGraph      : 'node',
    connSrcField  : 'srcIp',
    connDstField  : 'ip.dst:port',
    numPackets    : 'last',
    theme         : 'cotton-candy-theme'
  };

  describe('Session List Component ->', function() {

    // load the module
    beforeEach(angular.mock.module('moloch'));

    let scope, sessionComponent, sessionService, $httpBackend;
    let sessionsEndpoint    = 'sessions.json';
    let defaultParameters   = '?facets=1&flatten=1&fields=ipProtocol,firstPacket,lastPacket,srcIp,srcPort,dstIp,dstPort,totPackets,totBytes,node,http.uri,email.src,email.dst,email.subject,email.filename,dns.host,cert.alt,irc.channel&length=50&order=firstPacket:asc';
    let tableStateEndpoint  = 'state/sessionsNew';
    let columnWidthEndpoint = 'state/sessionsColWidths';

    // Initialize and a mock scope
    beforeEach(inject(function(
      $timeout,
      $location,
      $routeParams,
      $anchorScroll,
      _$httpBackend_,
      SessionService,
      FieldService,
      $componentController,
      $rootScope) {
        sessionService = SessionService;

        $httpBackend = _$httpBackend_;

        // initial query for table state
        $httpBackend.whenGET(tableStateEndpoint)
           .respond({});

        $httpBackend.whenGET('user/settings')
           .respond(settings);

        $httpBackend.whenGET('user/columns')
          .respond({});

        // initial query for fields
        $httpBackend.whenGET('fields')
           .respond(fields);

        // initial query for sessions
        $httpBackend.whenGET(sessionsEndpoint + defaultParameters)
           .respond(sessionsJSON);

       // initial query for table state
       $httpBackend.whenGET(columnWidthEndpoint)
          .respond({});

        scope = $rootScope.$new();

        sessionComponent = $componentController('session', {
          $scope            : scope,
          $timeout          : $timeout,
          $location         : $location,
          $routeParams      : { openAll:1 },
          $anchorScroll     : $anchorScroll,
          SessionService    : SessionService,
          FieldService      : FieldService
        });

        // spy on functions called in controller
        spyOn(sessionComponent, 'getData').and.callThrough();
        spyOn(sessionComponent, 'getTableState').and.callThrough();
        spyOn(sessionService, 'exportUniqueValues').and.callThrough();
        spyOn(sessionComponent, 'openAll').and.callThrough();
        spyOn(sessionComponent, 'toggleSessionDetail').and.callThrough();

        // initialize session component controller
        sessionComponent.$onInit();
        $httpBackend.flush();
    }));

    afterEach(function() {
      $httpBackend.verifyNoOutstandingExpectation();
      $httpBackend.verifyNoOutstandingRequest();
    });

    it('should exist and have dependencies', function() {
      expect(sessionComponent).toBeDefined();
      expect(sessionComponent.$scope).toBeDefined();
      expect(sessionComponent.$timeout).toBeDefined();
      expect(sessionComponent.$location).toBeDefined();
      expect(sessionComponent.$routeParams).toBeDefined();
      expect(sessionComponent.$anchorScroll).toBeDefined();
      expect(sessionComponent.SessionService).toBeDefined();
      expect(sessionComponent.FieldService).toBeDefined();
    });

    it('should fetch the table data', function() {
      sessionsJSON.data[0].expanded = true;

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
      expect(sessionComponent.getTableState.calls.count()).toBe(1);
      expect(sessionComponent.getTableState).toHaveBeenCalledWith();
      expect(sessionComponent.tableState).toBeDefined();
    });

    it('should reset the table state', function() {
      $httpBackend.expectPOST(tableStateEndpoint)
         .respond(200);

      expect(sessionComponent.tableState).toBeDefined();

      sessionComponent.tableState = {
        order         : [['ipProtocol', 'desc']],
        visibleHeaders: ['dst', 'src', 'info', 'srcPort', 'dstPort', 'totPackets', 'firstPacket', 'lastPacket', 'dbby', 'node', 'ipProtocol']
      };

      sessionComponent.loadColumnConfiguration();

      expect(sessionComponent.tableState).toEqual(defaultTableState);
      $httpBackend.flush();
    });

    it('should fetch the table state and remove empty entry in visible headers', function() {
      let tableState = {
        order         : [['firstPacket', 'asc']],
        visibleHeaders: ['', 'firstPacket', 'lastPacket', 'src', 'srcPort', 'dst', 'dstPort', 'totPackets', 'dbby', 'node', 'info']
      };

      $httpBackend.expectGET(tableStateEndpoint)
        .respond(tableState);

      sessionComponent.getTableState();

      $httpBackend.flush();

      tableState.visibleHeaders.shift();

      expect(sessionComponent.getTableState).toHaveBeenCalled();
      expect(sessionComponent.getTableState.calls.count()).toBe(2);
      expect(sessionComponent.getTableState).toHaveBeenCalledWith();
      expect(sessionComponent.tableState).toBeDefined();
      expect(sessionComponent.tableState).toEqual(tableState);
    });

    it('should have settings', function() {
      expect(sessionComponent.settings).toEqual(settings);
    });

    it('should toggle session detail', function() {
      sessionComponent.toggleSessionDetail(sessionsJSON.data[0]);
      expect(sessionComponent.stickySessions.length).toEqual(0);
      sessionComponent.toggleSessionDetail(sessionsJSON.data[0]);
      expect(sessionComponent.stickySessions.length).toEqual(1);
    });

    it('should accept an openAll parameter', function() {
      sessionsJSON.data[0].expanded = false;

      expect(sessionComponent.$routeParams.openAll).toEqual(1);

      expect(sessionComponent.openAll).toHaveBeenCalled();
      expect(sessionComponent.openAll).toHaveBeenCalledWith();

      expect(sessionComponent.toggleSessionDetail).toHaveBeenCalled();
      expect(sessionComponent.toggleSessionDetail).toHaveBeenCalledWith(sessionsJSON.data[0]);
    });


    describe('table sorting ->', function() {
      afterEach(function() {
        // cleanup table state
        sessionComponent.tableState.order = [['firstPacket', 'asc']];
        sessionComponent.query.sorts      = sessionComponent.tableState.order;
      });

      it('should have smart default sorts', function() {
        expect(sessionComponent.isSorted('firstPacket')).toBeGreaterThan(-1);
        expect(sessionComponent.getSortOrder('firstPacket')).toEqual('asc');
      });

      it('should toggle sort order', function() {
        $httpBackend.expectPOST(tableStateEndpoint)
           .respond(200);

        let newParameters = '?facets=1&flatten=1&fields=ipProtocol,firstPacket,lastPacket,srcIp,srcPort,dstIp,dstPort,totPackets,totBytes,node,http.uri,email.src,email.dst,email.subject,email.filename,dns.host,cert.alt,irc.channel&length=50&order=firstPacket:desc';
        $httpBackend.expectGET(sessionsEndpoint + newParameters)
           .respond(sessionsJSON);

        sessionComponent.sortBy({},'firstPacket');

        $httpBackend.flush();

        expect(sessionComponent.isSorted('firstPacket')).toBeGreaterThan(-1);
        expect(sessionComponent.getSortOrder('firstPacket')).toEqual('desc');
      });

      it('should change sort order', function() {
        $httpBackend.expectPOST(tableStateEndpoint)
           .respond(200);

        let newParameters = '?facets=1&flatten=1&fields=ipProtocol,firstPacket,lastPacket,srcIp,srcPort,dstIp,dstPort,totPackets,totBytes,node,http.uri,email.src,email.dst,email.subject,email.filename,dns.host,cert.alt,irc.channel&length=50&order=lastPacket:asc';
        $httpBackend.expectGET(sessionsEndpoint + newParameters)
           .respond(sessionsJSON);

        sessionComponent.sortBy({},'lastPacket');

        $httpBackend.flush();

        expect(sessionComponent.isSorted('lastPacket')).toBeGreaterThan(-1);
        expect(sessionComponent.getSortOrder('lastPacket')).toEqual('asc');
      });

      it('should add sort on shift click', function() {
        $httpBackend.expectPOST(tableStateEndpoint)
           .respond(200);

        let newParameters = '?facets=1&flatten=1&fields=ipProtocol,firstPacket,lastPacket,srcIp,srcPort,dstIp,dstPort,totPackets,totBytes,node,http.uri,email.src,email.dst,email.subject,email.filename,dns.host,cert.alt,irc.channel&length=50&order=firstPacket:asc,lastPacket:asc';
        $httpBackend.expectGET(sessionsEndpoint + newParameters)
           .respond(sessionsJSON);

        sessionComponent.sortBy({ shiftKey:true },'lastPacket');

        $httpBackend.flush();

        expect(sessionComponent.isSorted('firstPacket')).toBeGreaterThan(-1);
        expect(sessionComponent.getSortOrder('firstPacket')).toEqual('asc');

        expect(sessionComponent.isSorted('lastPacket')).toBeGreaterThan(-1);
        expect(sessionComponent.getSortOrder('lastPacket')).toEqual('asc');
      });
    });


    describe('column interactions ->', function() {
      afterEach(function() {
        // cleanup table state
        sessionComponent.tableState.order = [['firstPacket','asc']];
        sessionComponent.tableState.visibleHeaders = ['firstPacket', 'lastPacket', 'src', 'srcPort', 'dst', 'dstPort', 'totPackets', 'dbby', 'node', 'info'];
      });

      it('should have smart default visible headers', function() {
        let defaultHeaders = ['firstPacket', 'lastPacket', 'src', 'srcPort', 'dst', 'dstPort', 'totPackets', 'dbby', 'node', 'info'];

        expect(sessionComponent.tableState.visibleHeaders).toEqual(defaultHeaders);
      });

      it('should toggle header visibility', function() {
        $httpBackend.expectPOST(tableStateEndpoint)
           .respond(200);

        sessionComponent.toggleVisibility('lastPacket');

        $httpBackend.flush();

        expect(sessionComponent.isVisible('lastPacket')).toEqual(-1);
      });

      it('should issue query when adding a header', function() {
        $httpBackend.expectPOST(tableStateEndpoint)
           .respond(200);
        let newParameters = '?facets=1&fields=ipProtocol,firstPacket,srcIp,srcPort,dstIp,dstPort,totPackets,totBytes,node,http.uri,email.src,email.dst,email.subject,email.filename,dns.host,cert.alt,irc.channel,lastPacket&flatten=1&length=50&order=firstPacket:asc';
        $httpBackend.expectGET(sessionsEndpoint + newParameters)
           .respond(sessionsJSON);

        $httpBackend.expectPOST(tableStateEndpoint)
           .respond(200);

        sessionComponent.toggleVisibility('lastPacket');
        sessionComponent.toggleVisibility('lastPacket');

        $httpBackend.flush();

        expect(sessionComponent.isVisible('lastPacket')).toBeGreaterThan(-1);
      });

      it('should reset sort field and order to default', function() {
        $httpBackend.expectPOST(tableStateEndpoint)
          .respond(200);

        let newParameters = '?facets=1&fields=ipProtocol,firstPacket,lastPacket,srcIp,srcPort,dstIp,dstPort,totPackets,totBytes,node,http.uri,email.src,email.dst,email.subject,email.filename,dns.host,cert.alt,irc.channel&flatten=1&length=50&order=lastPacket:asc';
        $httpBackend.expectGET(sessionsEndpoint + newParameters)
           .respond(sessionsJSON);

        newParameters = '?facets=1&fields=ipProtocol,firstPacket,srcIp,srcPort,dstIp,dstPort,totPackets,totBytes,node,http.uri,email.src,email.dst,email.subject,email.filename,dns.host,cert.alt,irc.channel&flatten=1&length=50&order=firstPacket:asc';
        $httpBackend.expectGET(sessionsEndpoint + newParameters)
           .respond(sessionsJSON);

        $httpBackend.expectPOST(tableStateEndpoint)
           .respond(200);

        sessionComponent.sortBy({}, 'lastPacket');

        expect(sessionComponent.tableState.order).toEqual([['lastPacket','asc']]);

        sessionComponent.toggleVisibility('lastPacket');

        $httpBackend.flush();

        expect(sessionComponent.isVisible('lastPacket')).toEqual(-1);
        expect(sessionComponent.tableState.order).toEqual([['firstPacket','asc']]);
      });

      it('should remove non-visible column from sort order', function() {
        $httpBackend.expectPOST(tableStateEndpoint)
          .respond(200);

        let newParameters = '?facets=1&fields=ipProtocol,firstPacket,lastPacket,srcIp,srcPort,dstIp,dstPort,totPackets,totBytes,node,http.uri,email.src,email.dst,email.subject,email.filename,dns.host,cert.alt,irc.channel&flatten=1&length=50&order=firstPacket:asc,lastPacket:asc';
        $httpBackend.expectGET(sessionsEndpoint + newParameters)
           .respond(sessionsJSON);

        newParameters = '?facets=1&fields=ipProtocol,firstPacket,srcIp,srcPort,dstIp,dstPort,totPackets,totBytes,node,http.uri,email.src,email.dst,email.subject,email.filename,dns.host,cert.alt,irc.channel&flatten=1&length=50&order=firstPacket:asc';
        $httpBackend.expectGET(sessionsEndpoint + newParameters)
           .respond(sessionsJSON);

        $httpBackend.expectPOST(tableStateEndpoint)
           .respond(200);

        sessionComponent.sortBy({ shiftKey:true },'lastPacket');

        expect(sessionComponent.tableState.order).toEqual([['firstPacket','asc'],['lastPacket','asc']]);

        sessionComponent.toggleVisibility('lastPacket');

        $httpBackend.flush();

        expect(sessionComponent.isVisible('lastPacket')).toEqual(-1);
        expect(sessionComponent.tableState.order).toEqual([['firstPacket','asc']]);
      });

      it('should load a saved column configuration', function() {
        let columnConfigurations = [{
          name    : 'column config name',
          columns : ['firstPacket', 'srcIp', 'srcPort'],
          order   : [['srcIp','asc']]
        }];

        $httpBackend.expectPOST(tableStateEndpoint)
          .respond(200);

        let newParameters = '?facets=1&fields=ipProtocol,firstPacket,srcIp,srcPort&flatten=1&length=50&order=srcIp:asc';
        $httpBackend.expectGET(sessionsEndpoint + newParameters)
           .respond(sessionsJSON);

        sessionComponent.colConfigs = columnConfigurations;

        sessionComponent.loadColumnConfiguration(0);

        $httpBackend.flush();

        expect(sessionComponent.tableState.visibleHeaders).toEqual(columnConfigurations[0].columns);
        expect(sessionComponent.tableState.order).toEqual(columnConfigurations[0].order);
        expect(sessionComponent.query.sorts).toEqual(columnConfigurations[0].order);
      });

      it('should load a default column configuration', function() {
        sessionComponent.loadColumnConfiguration();

        expect(sessionComponent.tableState.visibleHeaders).toEqual(defaultTableState.visibleHeaders);
        expect(sessionComponent.tableState.order).toEqual(defaultTableState.order);
        expect(sessionComponent.query.sorts).toEqual(defaultTableState.order);
      });

      it('should save a custom column configuration', function() {
        $httpBackend.expectPOST('user/columns/create',
           function(postData) {
             let jsonData = JSON.parse(postData);
             expect(jsonData.name).toEqual('new col config name');
             expect(jsonData.columns).toEqual(['firstPacket', 'srcIp', 'srcPort']);
             expect(jsonData.order).toEqual([['srcIp','asc']]);
             return true;
           }).respond({name: 'new col config name'}, 200);

        sessionComponent.colConfigs = [];
        sessionComponent.newColConfigName = 'new col config name';
        sessionComponent.tableState.visibleHeaders = ['firstPacket', 'srcIp', 'srcPort'];
        sessionComponent.tableState.order = [['srcIp','asc']];

        sessionComponent.saveColumnConfiguration();

        $httpBackend.flush();

        expect(sessionComponent.colConfigs[0].columns).toEqual(sessionComponent.tableState.visibleHeaders);
        expect(sessionComponent.colConfigs[0].order).toEqual(sessionComponent.tableState.order);
        expect(sessionComponent.newColConfigName).toBeNull();
        expect(sessionComponent.colConfigsOpen).toBeFalsy();
        expect(sessionComponent.colConfigError).toBeFalsy();
      });

      it('should not save a custom column configuration if provided no name', function() {
        let error = 'You must name your new column configuration';

        sessionComponent.newColConfigName = null;
        sessionComponent.saveColumnConfiguration();
        expect(sessionComponent.colConfigError).toEqual(error);

        sessionComponent.newColConfigName = '';
        sessionComponent.saveColumnConfiguration();
        expect(sessionComponent.colConfigError).toEqual(error);
      });

      it('should delete a custom column configuration', function() {
        $httpBackend.expectPOST('user/columns/delete',
           function(postData) {
             let jsonData = JSON.parse(postData);
             expect(jsonData.name).toEqual('new col config name');
             return true;
           }).respond(200);

        sessionComponent.colConfigs = [{
          name    : 'new col config name',
          columns : ['firstPacket', 'srcIp', 'srcPort'],
          order   : [['firstPacket','asc']]
        }];

        sessionComponent.deleteColumnConfiguration('new col config name');

        $httpBackend.flush();

        expect(sessionComponent.colConfigs).toEqual([]);
        expect(sessionComponent.colConfigError).toBeFalsy();
      });

      it('should determine if a col config is the same as the visible headers', function() {
        sessionComponent.colConfigs = [{
          name    : 'new col config name',
          columns : ['firstPacket', 'srcIp', 'srcPort'],
          order   : [['firstPacket','asc']]
        }];

        sessionComponent.tableState.visibleHeaders = ['firstPacket', 'srcIp', 'srcPort'];
        let isSame = sessionComponent.isSameAsVisible(0);
        expect(isSame).toBeTruthy();

        sessionComponent.tableState.visibleHeaders = ['firstPacket', 'srcIp', 'srcPort', 'lastPacket', 'dstIp', 'dstPort'];
        isSame = sessionComponent.isSameAsVisible(0);
        expect(isSame).toBeFalsy();
      });
    });


    describe('listeners ->', function() {
      let sorts       = [['firstPacket', 'asc']];
      let length      = 200;
      let currentPage = 2;
      let start       = (currentPage - 1) * length;
      let sub_scope;

      beforeEach(function() {
        sub_scope = scope.$new();

        spyOn(scope, '$broadcast').and.callThrough();
      });

      afterEach(function() {
        $httpBackend.verifyNoOutstandingExpectation();
        $httpBackend.verifyNoOutstandingRequest();

        // cleanup
        sessionComponent.query.date   = null;
        sessionComponent.query.start  = 0;
        sessionComponent.query.length = 50;
      });

      it('should listen for "change:search" event', function() {
        let newParameters = '?date=-1&facets=1&flatten=1&fields=ipProtocol,firstPacket,lastPacket,srcIp,srcPort,dstIp,dstPort,totPackets,totBytes,node,http.uri,email.src,email.dst,email.subject,email.filename,dns.host,cert.alt,irc.channel&length=50&order=firstPacket:asc';
        $httpBackend.whenGET(sessionsEndpoint + newParameters)
          .respond(sessionsJSON);

        sub_scope.$emit('change:search', {
          start: 0, stop: 0, expression: ''
        });

        sub_scope.$emit('change:search', {
          date: -1, expression: ''
        });

        $httpBackend.flush();

        expect(sessionComponent.getData).toHaveBeenCalled();
        expect(sessionComponent.getData.calls.count()).toBe(2);
        expect(sessionComponent.getData).toHaveBeenCalledWith();
      });

      it('should listen for "change:pagination" event', function() {
        let newParameters = '?facets=1&flatten=1&fields=ipProtocol,firstPacket,lastPacket,srcIp,srcPort,dstIp,dstPort,totPackets,totBytes,node,http.uri,email.src,email.dst,email.subject,email.filename,dns.host,cert.alt,irc.channel&length=200&order=firstPacket:asc&start=200';
        $httpBackend.whenGET(sessionsEndpoint + newParameters)
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

      it('should listen for "add:to:search" event', function() {
        let args = { expression:'full == expression' };
        scope.$emit('add:to:search', args);

        expect(scope.$broadcast).toHaveBeenCalled();
        expect(scope.$broadcast).toHaveBeenCalledWith('add:to:typeahead', args);
        expect(scope.$broadcast.calls.count()).toBe(1);
      });

      it('should listen for "change:time" event', function() {
        let args = { start: 0, stop: 0 };
        scope.$emit('change:time', args);

        expect(scope.$broadcast).toHaveBeenCalled();
        expect(scope.$broadcast).toHaveBeenCalledWith('update:time', args);
        expect(scope.$broadcast.calls.count()).toBe(1);
      });

      it('should call SessionService when exporting unique column values', function() {
        sessionComponent.exportUnique('srcIp', 0);
        expect(sessionService.exportUniqueValues).toHaveBeenCalled();
        expect(sessionService.exportUniqueValues).toHaveBeenCalledWith('srcIp', 0);

        sessionComponent.exportUnique('srcIp:srcPort', 0);
        expect(sessionService.exportUniqueValues).toHaveBeenCalled();
        expect(sessionService.exportUniqueValues).toHaveBeenCalledWith('srcIp:srcPort', 0);

        sessionComponent.exportUnique('g2', 1);
        expect(sessionService.exportUniqueValues).toHaveBeenCalled();
        expect(sessionService.exportUniqueValues).toHaveBeenCalledWith('g2', 1);
      });

    });

  });

})();
