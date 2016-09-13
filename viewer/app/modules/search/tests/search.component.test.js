(function() {

  'use strict';

  describe('Search Component ->', function() {

    // load modules
    beforeEach(angular.mock.module('moloch'));
    beforeEach(angular.mock.module('directives.search'));
    beforeEach(angular.mock.module('moloch.util'));

    var scope, search, templateAsHtml, $httpBackend;

    // Initialize and a mock scope
    beforeEach(inject(function(
      _$httpBackend_,
      $componentController,
      $rootScope,
      $compile,
      $location) {

      $httpBackend = _$httpBackend_;

      $httpBackend.expectGET('fields').respond({});

      scope = $rootScope.$new();
      var htmlString = '<moloch-search></moloch-search>';

      var element   = angular.element(htmlString);
      var template  = $compile(element)(scope);

      scope.$digest();

      templateAsHtml = template.html();

      search = $componentController('molochSearch', {
        $scope      : scope,
        $routeParams: {},
        $location   : $location
      });

      // spy on emit event
      spyOn(scope, '$emit').and.callThrough();
      spyOn(search.$location, 'search').and.callThrough();

      // initialize search component controller
      search.$onInit();
      $httpBackend.flush();
    }));

    afterEach(function() {
      $httpBackend.verifyNoOutstandingExpectation();
      $httpBackend.verifyNoOutstandingRequest();
    });

    it('should exist and have dependencies', function() {
      expect(search).toBeDefined();
      expect(search.$scope).toBeDefined();
      expect(search.$routeParams).toBeDefined();
      expect(search.$location).toBeDefined();
    });

    it('should render html', function() {
      expect(templateAsHtml).toBeDefined();
    });

    it('should emit a "change:search" event when time range is changed', function() {
      var callCount = 2;

      function changeTimeRange(timeRange) {
        search.timeRange = timeRange;
        search.changeTimeRange();

        var currentTime = new Date().getTime();
        var thePast = currentTime - (3600000 * timeRange);

        expect(scope.$emit).toHaveBeenCalled();
        expect(scope.$emit).toHaveBeenCalledWith('change:search', {
          expression: null,
          startTime : thePast,
          stopTime  : currentTime,
          strictly  : false
        });

        callCount = callCount + 4;
        expect(search.$location.search.calls.count()).toBe(callCount);
        expect(search.$location.search).toHaveBeenCalledWith('date', timeRange);
      }

      changeTimeRange(1);
      changeTimeRange(6);
      changeTimeRange(24);
    });

    it('should emit a "change:search" event when start or stop time is changed', function() {
      var callCount = 2;

      function changeDate(stopTime, startTime) {
        search.stopTime   = stopTime;
        search.startTime  = startTime;
        search.changeDate();

        expect(scope.$emit).toHaveBeenCalled();
        expect(scope.$emit).toHaveBeenCalledWith('change:search', {
          expression: null,
          startTime : startTime,
          stopTime  : stopTime,
          strictly  : false
        });

        callCount = callCount + 4;
        expect(search.$location.search.calls.count()).toBe(callCount);
        expect(search.$location.search).toHaveBeenCalledWith('startTime', startTime);
        expect(search.$location.search).toHaveBeenCalledWith('stopTime', stopTime);
      }

      changeDate(1473775168701, 1473773809000);
      changeDate(1473083968701, 1473071809000);
      changeDate(1472829659701, 1472738480000);
    });

    it('should emit a "change:search" event when strictly flag is changed', function() {
      var callCount = 2;

      function changeBounded(bounded) {
        search.timeRange  = 1;
        search.strictly   = bounded;
        search.changeBounded();

        var currentTime = new Date().getTime();

        expect(scope.$emit).toHaveBeenCalled();
        expect(scope.$emit).toHaveBeenCalledWith('change:search', {
          expression: null,
          startTime : currentTime - (3600000 * search.timeRange),
          stopTime  : currentTime,
          strictly  : !bounded
        });

        callCount = callCount + 2;
        expect(search.$location.search.calls.count()).toBe(callCount);
        if (!bounded) {
          expect(search.$location.search).toHaveBeenCalledWith('strictly', 'true');
        } else {
          expect(search.$location.search).toHaveBeenCalledWith('strictly', null);
        }
      }

      changeBounded(true);
      changeBounded(false);
      changeBounded(true);
    });

  });

})();
