(function() {

  'use strict';

  /* mock data ------------------------------- */
  // default session query
  var query = {
    length: 100,  // page length
    start : 0,    // first item index
    sorts : [{element:'fp', order:'asc'}], // array of sort objects
    facets: 1     // facets
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

  describe('Session Component ->', function() {

    // load the module
    beforeEach(angular.mock.module('moloch'));

    var scope, sessionComponent, $httpBackend;
    var sessionsEndpoint    = 'sessions.json';
    var defaultParameters   = '?facets=1&length=100&order=fp:asc';
    var tableStateEndpoint  = 'tableState/sessions';

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

        scope = $rootScope.$new();

        sessionComponent = $componentController('session', {
          $scope            : scope,
          $routeParams      : $routeParams,
          SessionService    : SessionService
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
    });

    it('should have smart query defaults', function() {
      expect(sessionComponent.query).toBeDefined();
      expect(sessionComponent.query).toEqual(query);
      expect(sessionComponent.currentPage).toEqual(1);
    });

    it('should fetch the table state', function() {
      expect(sessionComponent.getColumnInfo).toHaveBeenCalled();
      expect(sessionComponent.getColumnInfo).toHaveBeenCalledWith();
      expect(sessionComponent.columnInfo).toEqual({});
    });

    describe('listeners ->', function() {
      var sorts       = [{element:'fp', order:'asc'}];
      var length      = 10;
      var currentPage = 2;
      var start       = (currentPage - 1) * length;

      beforeEach(function() {
        var sub_scope = scope.$new();

        // emit change:sort event
        sub_scope.$emit('change:sort', {
          sorts:sorts
        });

        // expect GET request with new parameters
        var newParameters = '?facets=1&length=100&order=fp:asc';
        $httpBackend.expectGET(sessionsEndpoint + newParameters)
          .respond(sessionsJSON);

        // emit change:pagination event
        sub_scope.$emit('change:pagination',
          { length:length, currentPage:currentPage, start:start }
        );

        // expect GET request with new parameters
        newParameters = '?facets=1&length=10&order=fp:asc&start=10';
        $httpBackend.expectGET(sessionsEndpoint + newParameters)
          .respond(sessionsJSON);

        $httpBackend.flush();
      });

      it('should listen for "change:search" event', function() {
        expect(sessionComponent.getData).toHaveBeenCalled();
        expect(sessionComponent.getData).toHaveBeenCalledWith();
        expect(sessionComponent.loading).toEqual(false);
        expect(sessionComponent.error).toEqual(false);
      });

      it('should listen for "change:sort" event', function() {
        expect(sessionComponent.query.sorts).toEqual(sorts);
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
