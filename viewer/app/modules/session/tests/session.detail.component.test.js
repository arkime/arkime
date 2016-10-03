(function() {

  'use strict';

  var session = {
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

    var scope, sessionDtlsComponent, $httpBackend, templateAsHtml;
    var sessionDtlsEndpoint = 'node/sessionid/sessionDetail';
    var defaultParameters   = '?base=hex&decode=%7B%7D&gzip=false&image=false&line=false&ts=false';
    var configEndpoint      = 'molochRightClick';
    var fieldEndpoint       = 'fields';

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

        var htmlString  = '<session-detail session="session"></session-detail>';

        var element     = angular.element(htmlString);
        var template    = $compile(element)(scope);

        scope.$digest();

        templateAsHtml  = template.html();

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

  });

})();
