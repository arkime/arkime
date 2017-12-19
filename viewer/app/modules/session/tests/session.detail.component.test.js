(function() {

  'use strict';

  let session = {
    dstPackets   :0,
    srcPort      :10000,
    node         :'node',
    srcPackets   :1,
    dstPort      :2948,
    ipProtocol   :17,
    lastPacket   :0,
    firstPacket  :0,
    srcIp    :16843009,
    dstIp    :33686018,
    pa    :1,
    db1   :437,
    db2   :0,
    by    :445,
    by2   :0,
    by1   :445,
    db    :437,
    index :'sessions-',
    id    :'sessionid'
  };

  let userSettings = {
    timezone        : 'local',
    detailFormat    : 'last',
    showTimestamps  : 'last',
    sortColumn      : 'start',
    sortDirection   : 'asc',
    spiGraph        : 'no',
    connSrcField    : 'a1',
    connDstField    : 'ip.dst:port',
    numPackets      : 'last'
  };

  describe('Session Detail Component ->', function() {

    // load the module
    beforeEach(angular.mock.module('moloch'));

    let scope, sessionDtlsComponent, $httpBackend, templateAsHtml;
    let sessionDtlsEndpoint = 'node/session/sessionid/detail';
    let sessionPktsEndpoint = 'node/session/sessionid/packets?base=last&decode=%7B%7D&gzip=false&image=false&line=false&packets=last&ts=false';
    let configEndpoint      = 'molochRightClick';
    let fieldEndpoint       = 'fields';
    let decodingsEndpoint   = 'decodings';

    // Initialize and a mock scope
    beforeEach(inject(function(
      _$httpBackend_,
      $compile,
      $sce,
      SessionService,
      ConfigService,
      FieldService,
      $controller,
      $rootScope) {

      $httpBackend = _$httpBackend_;

      // query for user settings
      $httpBackend.expectGET('user/settings')
        .respond(200, userSettings);

      // initial query for session detail
      $httpBackend.expectGET(sessionDtlsEndpoint)
         .respond(200, '');

      // query for moloch clickable values
      $httpBackend.expectGET(configEndpoint)
         .respond(200, {});

      // query for moloch fields
      $httpBackend.expectGET(fieldEndpoint)
        .respond(200, {});

      // query for other decodings
      $httpBackend.expectGET(decodingsEndpoint)
        .respond(200, {});

      // initial query for session packets
      $httpBackend.expectGET(sessionPktsEndpoint)
         .respond(200, '');

      scope = $rootScope.$new();

      scope.session   = session;

      let htmlString  = '<session-detail session="session"></session-detail>';

      let element     = angular.element(htmlString);
      let template    = $compile(element)(scope);

      scope.$digest();

      sessionDtlsComponent = element.controller('sessionDetail');
      templateAsHtml = template.html();

      spyOn(sessionDtlsComponent.$scope, '$emit').and.callThrough();

      $httpBackend.flush();
    }));

    afterEach(function() {
      $httpBackend.verifyNoOutstandingExpectation();
      $httpBackend.verifyNoOutstandingRequest();
    });

    it('should render html with session info', function() {
      expect(templateAsHtml).toBeDefined();
      expect(scope.session).toBeDefined();
    });

    it('should issue query for all sessions with new start time', function() {
      sessionDtlsComponent.allSessions('rootId', 0);

      expect(sessionDtlsComponent.$scope.$emit).toHaveBeenCalled();
      expect(sessionDtlsComponent.$scope.$emit).toHaveBeenCalledWith('add:to:search',
         {expression:'rootId == "rootId"' });

      expect(sessionDtlsComponent.$scope.$emit).toHaveBeenCalled();
      expect(sessionDtlsComponent.$scope.$emit).toHaveBeenCalledWith('change:time',
         { start:0 });
    });

    it('should issue query for all sessions with current start time', function() {
      sessionDtlsComponent.allSessions('rootId', 1476102172);

      sessionDtlsComponent.$routeParams.startTime = 1476102173;

      expect(sessionDtlsComponent.$scope.$emit).toHaveBeenCalled();
      expect(sessionDtlsComponent.$scope.$emit).toHaveBeenCalledWith('add:to:search',
         { expression:'rootId == "rootId"' });

      expect(sessionDtlsComponent.$scope.$emit).toHaveBeenCalled();
      expect(sessionDtlsComponent.$scope.$emit).toHaveBeenCalledWith('change:time',
         { start:1476102172 });
    });

    it('should issue query for all sessions with "-" in rootId', function() {
      sessionDtlsComponent.allSessions('rootId-1234567890', 0);

      expect(sessionDtlsComponent.$scope.$emit).toHaveBeenCalled();
      expect(sessionDtlsComponent.$scope.$emit).toHaveBeenCalledWith('add:to:search',
         { expression:'rootId == "rootId-1234567890"' });

      expect(sessionDtlsComponent.$scope.$emit).toHaveBeenCalled();
      expect(sessionDtlsComponent.$scope.$emit).toHaveBeenCalledWith('change:time',
         { start:0 });
    });

    it('should set promise for packet request', function() {
      $httpBackend.expectGET(sessionPktsEndpoint)
         .respond(200, '');

      sessionDtlsComponent.getPackets();
      expect(sessionDtlsComponent.packetPromise).toBeDefined();
      expect(sessionDtlsComponent.packetPromise).not.toBeNull();

      $httpBackend.flush();
    });

    it('should cancel a query for packets', function() {
      $httpBackend.expectGET(sessionPktsEndpoint)
         .respond(200, '');

      sessionDtlsComponent.getPackets();
      expect(sessionDtlsComponent.packetPromise).toBeDefined();
      expect(sessionDtlsComponent.packetPromise).not.toBeNull();

      sessionDtlsComponent.cancelPacketLoad();
      expect(sessionDtlsComponent.packetPromise).toBeNull();
    });

  });

})();
