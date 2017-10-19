(function() {

  'use strict';

  describe('Search Component ->', function() {

    // load modules
    beforeEach(angular.mock.module('moloch'));

    let scope, rootScope, timeComponent, templateAsHtml, $httpBackend;

    // Initialize and a mock scope
    beforeEach(inject(function(
       _$httpBackend_,
       $componentController,
       $rootScope,
       $compile,
       $location) {

      $httpBackend = _$httpBackend_;

      rootScope = $rootScope;

      scope = $rootScope.$new();
      let htmlString = '<moloch-time></moloch-time>';

      let element   = angular.element(htmlString);
      let template  = $compile(element)(scope);

      scope.$digest();

      templateAsHtml  = template.html();

      timeComponent   = $componentController('molochTime', {
        $scope        : scope,
        $location     : $location,
        $routeParams  : {}
      }, { timezone   : 'local' });

      // spy on functions in controller
      spyOn(scope, '$emit').and.callThrough();
      spyOn(rootScope, '$broadcast').and.callThrough();
      spyOn(timeComponent.$location, 'search').and.callThrough();

      // initialize time component controller
      timeComponent.$onInit();
    }));

    afterEach(function() {
      // cleanup
      timeComponent.$location.search.calls.reset();
    });

    it('should exist and have dependencies', function() {
      expect(timeComponent).toBeDefined();
      expect(timeComponent.$scope).toBeDefined();
      expect(timeComponent.$location).toBeDefined();
      expect(timeComponent.$routeParams).toBeDefined();
    });

    it('should render html', function() {
      expect(templateAsHtml).toBeDefined();
    });

    it('should have bindings', function() {
      expect(timeComponent.timezone).toBeDefined();
      expect(timeComponent.timezone).toEqual('local');
    });

    it('should emit a "change:time:input" event when time range is changed', function() {
      function changeTimeRange(timeRange) {
        timeComponent.timeRange = timeRange;
        timeComponent.changeTimeRange();

        expect(scope.$emit).toHaveBeenCalled();
        expect(timeComponent.$location.search).toHaveBeenCalledWith('date', timeRange);
      }

      changeTimeRange(1);
      changeTimeRange(6);
      changeTimeRange(24);
    });

    it('should emit a "change:time:input" event when start or stop time is changed', function() {
      function changeDate(stopTime, startTime) {
        timeComponent.stopTime   = stopTime;
        timeComponent.startTime  = startTime;
        timeComponent.changeDate(true);

        expect(scope.$emit).toHaveBeenCalled();
        expect(timeComponent.$location.search).toHaveBeenCalledWith('startTime', parseInt((startTime/1000).toFixed()));
        expect(timeComponent.$location.search).toHaveBeenCalledWith('stopTime', parseInt((stopTime/1000).toFixed()));
      }

      changeDate(1473775168701, 1473773809000);
      changeDate(1473083968701, 1473071809000);
      changeDate(1472829659701, 1472738480000);
    });

    it('should emit a "change:time:input" event when bounding flag is changed', function() {
      timeComponent.timeBounding = 'last';

      function changeTimeBounding(bounding) {
        timeComponent.timeRange     = 1;
        timeComponent.timeBounding  = bounding;
        timeComponent.changeTimeBounding();

        expect(scope.$emit).toHaveBeenCalled();

        if (bounding !== 'last') {
          expect(timeComponent.$location.search).toHaveBeenCalledWith('bounding', bounding);
        } else {
          expect(timeComponent.$location.search).toHaveBeenCalledWith('bounding', null);
        }
      }

      changeTimeBounding('first');
      changeTimeBounding('last');
      changeTimeBounding('first');
    });

    it('should not emit a "change:time:input" event when start or stop time is invalid', function() {
      function changeDate(stopTime, startTime) {
        timeComponent.stopTime   = stopTime;
        timeComponent.startTime  = startTime;
        timeComponent.changeDate();

        expect(timeComponent.$location.search.calls.mostRecent()).not.toEqual('startTime', (startTime/1000).toFixed());
        expect(timeComponent.$location.search.calls.mostRecent()).not.toEqual('stopTime', (stopTime/1000).toFixed());
      }

      changeDate(1473775168701, 'asdf');
      changeDate('asdf', 1473071809000);
    });

  });

})();
