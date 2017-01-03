(function() {

  'use strict';

  let session = {
    pa2   :0,
    p1    :10000,
    no    :'node',
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
    id    :'sessionid'
  };

  describe('Session Detail Component ->', function() {

    // load the module
    beforeEach(angular.mock.module('moloch'));

    let scope, sessionDtlsComponent, $httpBackend, templateAsHtml;
    let sessionDtlsEndpoint = 'node/sessionid/sessionDetailNew';
    let defaultParameters   = '?base=hex&decode=%7B%7D&gzip=false&image=false&line=false&ts=false';
    let configEndpoint      = 'molochRightClick';
    let fieldEndpoint       = 'fields';

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

        // initial query for session detail
        $httpBackend.expectGET(sessionDtlsEndpoint + defaultParameters)
          .respond('');

        // query for moloch clickable values
        $httpBackend.expectGET(configEndpoint)
          .respond({});

        // query for moloch fields
        $httpBackend.expectGET(fieldEndpoint)
          .respond({});

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
      expect(sessionDtlsComponent.$scope.$emit).toHaveBeenCalledWith('add:to:search', { expression:'rootId == "rootId"' });

      expect(sessionDtlsComponent.$scope.$emit).toHaveBeenCalled();
      expect(sessionDtlsComponent.$scope.$emit).toHaveBeenCalledWith('change:time', { start:0 });
    });

    it('should issue query for all sessions with current start time', function() {
      sessionDtlsComponent.allSessions('rootId', 1476102173);

      sessionDtlsComponent.$routeParams.startTime = 1476102172;

      expect(sessionDtlsComponent.$scope.$emit).toHaveBeenCalled();
      expect(sessionDtlsComponent.$scope.$emit).toHaveBeenCalledWith('add:to:search', { expression:'rootId == "rootId"' });

      expect(sessionDtlsComponent.$scope.$emit).toHaveBeenCalled();
      expect(sessionDtlsComponent.$scope.$emit).toHaveBeenCalledWith('change:time', { start:1476102173 });
    });

  });

})();
