(function() {

  'use strict';

  /* mock data ------------------------------- */
  // default session query
  var query = {
    length      : 50,   // page length
    start       : 0,    // first item index
    sortElement : 'fp', // sort element (key of session field)
    sortOrder   : 'asc',// sort order ('asc' or 'desc')
    date        : 1,    // date range
    facets      : 1,    // facets
    draw        : 1     // draw
  };

  // sample session json
  var sessionsJSON = {
    draw            : '1',
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

  describe('Session Component ->', function() {

    // load the module
    beforeEach(module('moloch'));

    var scope, sessionComponent, $httpBackend;
    var sessionsEndpoint    = 'sessions.json';
    var defaultParameters   = '?date=1&draw=1&facets=1&length=50&order=fp:asc';
    var tableStateEndpoint  = 'tableState/sessions';

    // Initialize and a mock scope
    beforeEach(inject(function(
      _$httpBackend_,
      $routeParams,
      SessionService,
      DTOptionsBuilder,
      DTColumnDefBuilder,
      $componentController,
      $rootScope) {
        $httpBackend = _$httpBackend_;

        // itital query for sessions
        $httpBackend.expectGET(sessionsEndpoint + defaultParameters)
          .respond(sessionsJSON);

        // initial query for table state
        $httpBackend.expectGET(tableStateEndpoint)
          .respond({});

        scope = $rootScope.$new();

        sessionComponent = $componentController('session', {
          $scope            : scope,
          $routeParams      : $routeParams,
          SessionService    : SessionService,
          DTOptionsBuilder  : DTOptionsBuilder,
          DTColumnDefBuilder: DTColumnDefBuilder
        });

        // spy on functions called in controller
        spyOn(sessionComponent, 'getData').and.callThrough();
        spyOn(sessionComponent, 'getColumnInfo').and.callThrough();

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
      expect(sessionComponent.$routeParams).toBeDefined();
      expect(sessionComponent.SessionService).toBeDefined();
      expect(sessionComponent.DTOptionsBuilder).toBeDefined();
      expect(sessionComponent.DTColumnDefBuilder).toBeDefined();
    });

    it('should have smart query defaults', function() {
      expect(sessionComponent.query).toBeDefined();
      expect(sessionComponent.query).toEqual(query);
      expect(sessionComponent.currentPage).toEqual(1);
    });

    it('should fetch sessions', function() {
      expect(sessionComponent.getData).toHaveBeenCalled();
      expect(sessionComponent.getData).toHaveBeenCalledWith();
      expect(sessionComponent.loading).toEqual(false);
      expect(sessionComponent.error).toEqual(false);
    });

    it('should fetch the table state', function() {
      expect(sessionComponent.getColumnInfo).toHaveBeenCalled();
      expect(sessionComponent.getColumnInfo).toHaveBeenCalledWith();
      expect(sessionComponent.columnInfo).toEqual({});
    });

    describe('listeners ->', function() {
      var sortElement = 'lp', sortOrder = 'desc';
      var length      = 10,   currentPage = 2;
      var start       = (currentPage - 1) * length;

      beforeEach(function() {
        var sub_scope = scope.$new();

        // emit change:sort event
        sub_scope.$emit('change:sort', {
          sortElement:sortElement, sortOrder:sortOrder
        });

        // expect GET request with new parameters
        var newParameters = '?date=1&draw=1&facets=1&length=50&order=lp:desc';
        $httpBackend.expectGET(sessionsEndpoint + newParameters)
          .respond(sessionsJSON);

        // emit change:pagination event
        sub_scope.$emit('change:pagination',
          { length:length, currentPage:currentPage, start:start }
        );

        // expect GET request with new parameters
        newParameters = '?date=1&draw=1&facets=1&length=10&order=lp:desc&start=10';
        $httpBackend.expectGET(sessionsEndpoint + newParameters)
          .respond(sessionsJSON);

        $httpBackend.flush();
      });

      it('should listen for "change:sort" event', function() {
        expect(sessionComponent.query.sortElement).toEqual(sortElement);
        expect(sessionComponent.query.sortOrder).toEqual(sortOrder);
        expect(sessionComponent.getData).toHaveBeenCalled();
        expect(sessionComponent.getData).toHaveBeenCalledWith();
      });

      it('should listen for "change:pagination" event', function() {
        expect(sessionComponent.query.length).toEqual(length);
        expect(sessionComponent.query.start).toEqual(start);
        expect(sessionComponent.currentPage).toEqual(currentPage);
        expect(sessionComponent.getData).toHaveBeenCalled();
        expect(sessionComponent.getData).toHaveBeenCalledWith();
      });
    });

  });

})();
