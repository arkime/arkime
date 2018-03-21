(function() {

  'use strict';

  let session = {
    dstPackets   : 0,
    srcPort    : 10000,
    node    : 'demo',
    srcPackets   : 1,
    dstPort    : 2948,
    ipProtocol    : 17,
    lastPacket    : 1055289981000,
    firstPacket    : 1055289978000,
    srcIp    : 16843009,
    dstIp    : 33686018,
    totPackets    : 1,
    srcDataBytes   : 437,
    dstDataBytes   : 0,
    totBytes    : 445,
    dstBytes   : 0,
    srcBytes   : 445,
    totDataBytes    : 437,
    index : 'sessions-',
    id    : '--RzB4CrWwqlMZIHRa0CzjUYn'
  };

  let startTimeCol = {
    dbField     : 'firstPacket',
    exp         : 'starttime',
    friendlyName: 'Start Time',
    group       : 'general',
    type        : 'seconds'
  };

  describe('Session Field Directive ->', function() {

    // load the module
    beforeEach(angular.mock.module('moloch'));

    let scope, sessionField, templateAsHtml;

    // Initialize and a mock scope
    beforeEach(inject(function(
      $componentController,
      $rootScope,
      $compile,
      _$filter_,
      _$location_,
      FieldService,
      ConfigService,
      SessionService) {

      scope = $rootScope.$new();

      let htmlString  = '<session-field></session-field>';
      let element     = angular.element(htmlString);
      let template    = $compile(element)(scope);

      scope.$digest();

      templateAsHtml  = template.html();

      sessionField    = $componentController('sessionField', {
        $scope        : scope,
        $filter       : _$filter_,
        $location     : _$location_,
        FieldService  : FieldService,
        ConfigService : ConfigService,
        SessionService: SessionService
      }, {
        expr    : 'starttime',
        value   : session.firstPacket,
        session : session,
        field   : startTimeCol,
        parse   : true
      });

      // spy on functions called in controller
      spyOn(sessionField.$scope, '$emit').and.callThrough();

      sessionField.$onInit();
    }));

    it('should render html with session data and dependencies', function() {
      expect(sessionField).toBeDefined();
      expect(templateAsHtml).toBeDefined();

      expect(sessionField.expr).toBeDefined();
      expect(sessionField.parse).toBeDefined();
      expect(sessionField.value).toBeDefined();
      expect(sessionField.field).toBeDefined();
      expect(sessionField.session).toBeDefined();

      expect(sessionField.$scope).toBeDefined();
      expect(sessionField.$filter).toBeDefined();
      expect(sessionField.$location).toBeDefined();
      expect(sessionField.FieldService).toBeDefined();
      expect(sessionField.ConfigService).toBeDefined();
      expect(sessionField.SessionService).toBeDefined();
    });

    it('should set the parsed value', function() {
      expect(sessionField.parsed[0].value).not.toEqual(session.value);
      expect(sessionField.parsed[0].value).toEqual('2003/06/10 20:06:18');
    });

    it('should not update the parsed value for unparsed values', function() {
      sessionField.expr = 'protocols';
      sessionField.value = 'udp';
      sessionField.field = {
        dbField     : 'protocols',
        exp         : 'protocols',
        friendlyName: 'Protocols',
        group       : 'general'
      };
      session.parse = false;

      sessionField.parseValue(sessionField.field);

      expect(sessionField.parsed[0].queryVal).toEqual(sessionField.value);
    });

    it('should be able to click a time field', function() {
      sessionField.timeClick('starttime', session.firstPacket);

      expect(sessionField.$scope.$emit).toHaveBeenCalled();
      expect(sessionField.$scope.$emit).toHaveBeenCalledWith('change:time', {
        start: session.firstPacket });
    });

  });

})();
