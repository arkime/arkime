(function() {

  'use strict';

  let session = {
    pa2   : 0,
    p1    : 10000,
    no    : 'demo',
    pa1   : 1,
    p2    : 2948,
    pr    : 17,
    lp    : 1055289981,
    fp    : 1055289978,
    a1    : 16843009,
    a2    : 33686018,
    pa    : 1,
    db1   : 437,
    db2   : 0,
    by    : 445,
    by2   : 0,
    by1   : 445,
    db    : 437,
    index : 'sessions-',
    id    : '--RzB4CrWwqlMZIHRa0CzjUYn'
  };

  let startTimeCol = {
    dbField     : 'fp',
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
        value   : session.fp,
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
        dbField     : 'prot-term',
        exp         : 'protocols',
        friendlyName: 'Protocols',
        group       : 'general'
      };
      session.parse = false;

      sessionField.parseValue(sessionField.field);

      expect(sessionField.parsed[0].queryVal).toEqual(sessionField.value);
    });

    it('should be able to click the field', function() {
      sessionField.fieldClick('ip.src', session.a1, '==');

      expect(sessionField.$scope.$emit).toHaveBeenCalled();
      expect(sessionField.$scope.$emit).toHaveBeenCalledWith('add:to:search', {
        expression: `ip.src == ${session.a1}` });
    });

    it('should be able to click a time field', function() {
      sessionField.timeClick('starttime', session.fp);

      expect(sessionField.$scope.$emit).toHaveBeenCalled();
      expect(sessionField.$scope.$emit).toHaveBeenCalledWith('change:time', {
        start: session.fp });
    });

  });

})();
