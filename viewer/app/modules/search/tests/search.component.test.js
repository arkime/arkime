(function() {

  'use strict';

  describe('Search Component ->', function() {

    // load modules
    beforeEach(angular.mock.module('moloch'));
    beforeEach(angular.mock.module('directives.search'));
    beforeEach(angular.mock.module('moloch.util'));

    let scope, rootScope, search, templateAsHtml, $httpBackend;

    // Initialize and a mock scope
    beforeEach(inject(function(
      _$httpBackend_,
      $componentController,
      $rootScope,
      $compile,
      $location,
      ConfigService,
      UserService) {

      $httpBackend = _$httpBackend_;

      $httpBackend.expectGET('molochclusters').respond({});

      $httpBackend.expectGET('user/views').respond({});

      $httpBackend.expectGET('titleconfig').respond('');

      $httpBackend.expectGET('user/current').respond({});

      $httpBackend.expectGET('fields').respond({});

      $httpBackend.expectGET('user/views').respond({});

      rootScope = $rootScope;

      scope = $rootScope.$new();
      let htmlString = '<moloch-search></moloch-search>';

      let element   = angular.element(htmlString);
      let template  = $compile(element)(scope);

      scope.$digest();

      templateAsHtml = template.html();

      search = $componentController('molochSearch', {
        $scope        : scope,
        $location     : $location,
        $rootScope    : $rootScope,
        $routeParams  : {},
        ConfigService : ConfigService,
        UserService   : UserService
      }, {
        openSessions        : [],
        numVisibleSessions  : 100,
        numMatchingSessions : 10,
        start               : 0,
        timezone            : 'local'
      });

      // spy on functions in controller
      spyOn(scope, '$emit').and.callThrough();
      spyOn(rootScope, '$broadcast').and.callThrough();
      spyOn(search.$location, 'search').and.callThrough();

      // initialize search component controller
      search.$onInit();
      $httpBackend.flush();
    }));

    afterEach(function() {
      // cleanup
      search.$location.search.calls.reset();

      $httpBackend.verifyNoOutstandingExpectation();
      $httpBackend.verifyNoOutstandingRequest();
    });

    it('should exist and have dependencies', function() {
      expect(search).toBeDefined();
      expect(search.$scope).toBeDefined();
      expect(search.$location).toBeDefined();
      expect(search.$rootScope).toBeDefined();
      expect(search.$routeParams).toBeDefined();
      expect(search.ConfigService).toBeDefined();
      expect(search.UserService).toBeDefined();
    });

    it('should render html', function() {
      expect(templateAsHtml).toBeDefined();
    });

    it('should have bindings', function() {
      expect(search.openSessions).toBeDefined();
      expect(search.openSessions).toEqual([]);

      expect(search.numVisibleSessions).toBeDefined();
      expect(search.numVisibleSessions).toEqual(100);

      expect(search.numMatchingSessions).toBeDefined();
      expect(search.numMatchingSessions).toEqual(10);

      expect(search.start).toBeDefined();
      expect(search.start).toEqual(0);

      expect(search.timezone).toBeDefined();
      expect(search.timezone).toEqual('local');
    });

    it('should emit a "change:search" event when time range is changed', function() {
      function changeTimeRange(timeRange) {
        search.timeRange = timeRange;
        search.changeTimeRange();

        expect(scope.$emit).toHaveBeenCalled();
        expect(search.$location.search).toHaveBeenCalledWith('date', timeRange);
      }

      changeTimeRange(1);
      changeTimeRange(6);
      changeTimeRange(24);
    });

    it('should emit a "change:search" event when start or stop time is changed', function() {
      function changeDate(stopTime, startTime) {
        search.stopTime   = stopTime;
        search.startTime  = startTime;
        search.changeDate(true);

        expect(scope.$emit).toHaveBeenCalled();
        expect(search.$location.search).toHaveBeenCalledWith('startTime', parseInt((startTime/1000).toFixed()));
        expect(search.$location.search).toHaveBeenCalledWith('stopTime', parseInt((stopTime/1000).toFixed()));
      }

      changeDate(1473775168701, 1473773809000);
      changeDate(1473083968701, 1473071809000);
      changeDate(1472829659701, 1472738480000);
    });

    it('should emit a "change:search" event when bounding flag is changed', function() {
      search.timeBounding = "last";

      function changeTimeBounding(bounding) {
        search.timeRange  = 1;
        search.timeBounding   = bounding;
        search.changeTimeBounding();

        expect(scope.$emit).toHaveBeenCalled();

        if (bounding !== "last") {
          expect(search.$location.search).toHaveBeenCalledWith('bounding', bounding);
        } else {
          expect(search.$location.search).toHaveBeenCalledWith('bounding', null);
        }
      }

      changeTimeBounding("first");
      changeTimeBounding("last");
      changeTimeBounding("first");
    });

    it('should not emit a "change:search" event when start or stop time is invalid', function() {
      function changeDate(stopTime, startTime) {
        search.stopTime   = stopTime;
        search.startTime  = startTime;
        search.changeDate();

        expect(search.$location.search.calls.mostRecent()).not.toEqual('startTime', (startTime/1000).toFixed());
        expect(search.$location.search.calls.mostRecent()).not.toEqual('stopTime', (stopTime/1000).toFixed());
      }

      changeDate(1473775168701, 'asdf');
      changeDate('asdf', 1473071809000);
    });

    it('should set a view', function() {
      search.setView('viewy');

      expect(search.view).toEqual('viewy');
      expect(sessionStorage['moloch-view']).toEqual('viewy');

      expect(scope.$emit).toHaveBeenCalled();
      expect(scope.$emit).toHaveBeenCalledWith('change:search', {
        expression: undefined,
        view      : 'viewy'
      });

      expect(rootScope.$broadcast).toHaveBeenCalled();
      expect(rootScope.$broadcast).toHaveBeenCalledWith('issue:search', {
        expression: undefined,
        view      : 'viewy'
      });
    });

    it('should set a view and expression', function() {
      let expression = 'protocols == tcp';
      let view = 'viewy';

      rootScope.expression = expression;
      search.setView(view);

      expect(search.view).toEqual(view);
      expect(sessionStorage['moloch-view']).toEqual(view);

      expect(scope.$emit).toHaveBeenCalled();
      expect(scope.$emit).toHaveBeenCalledWith('change:search', {
        expression: expression,
        view      : view
      });

      expect(rootScope.$broadcast).toHaveBeenCalled();
      expect(rootScope.$broadcast).toHaveBeenCalledWith('issue:search', {
        expression: expression,
        view      : view
      });
    });

    it('should unset a view', function() {
      search.setView();

      expect(search.view).not.toBeDefined();
      expect(sessionStorage['moloch-view']).not.toBeDefined();

      expect(scope.$emit).toHaveBeenCalled();
      expect(scope.$emit).toHaveBeenCalledWith('change:search', {
        expression: undefined,
        view      : undefined
      });

      expect(rootScope.$broadcast).toHaveBeenCalled();
      expect(rootScope.$broadcast).toHaveBeenCalledWith('issue:search', {
        expression: undefined,
        view      : undefined
      });
    });

    it('should send a request to delete a view', function() {
      search.deleteView('viewy');

      $httpBackend.expectPOST('user/views/delete',
        function(postData) {
          let jsonData = JSON.parse(postData);
          expect(jsonData.view).toBe('viewy');
          return true;
        }
      ).respond(200, { text: 'SUCCESS', success: true });

      $httpBackend.flush();

      expect(scope.$emit).toHaveBeenCalled();
      expect(scope.$emit).toHaveBeenCalledWith('close:form:container', {
        message: 'SUCCESS', success: true
      });
    });

    it('should  delete a selected view and unset it', function() {
      // set view
      search.setView('viewy');

      expect(search.view).toEqual('viewy');
      expect(sessionStorage['moloch-view']).toEqual('viewy');

      expect(scope.$emit).toHaveBeenCalled();
      expect(scope.$emit).toHaveBeenCalledWith('change:search', {
        expression: undefined,
        view      : 'viewy'
      });

      expect(rootScope.$broadcast).toHaveBeenCalled();
      expect(rootScope.$broadcast).toHaveBeenCalledWith('issue:search', {
        expression: undefined,
        view      : 'viewy'
      });

      // delete view
      search.deleteView('viewy');

      $httpBackend.expectPOST('user/views/delete',
         function(postData) {
           let jsonData = JSON.parse(postData);
           expect(jsonData.view).toBe('viewy');
           return true;
         }
      ).respond(200, { text: 'SUCCESS', success: true });

      $httpBackend.flush();

      expect(scope.$emit).toHaveBeenCalled();
      expect(scope.$emit).toHaveBeenCalledWith('close:form:container', {
        message: 'SUCCESS', success: true
      });

      // expect it to be unset as the view
      expect(search.view).not.toBeDefined();
      expect(sessionStorage['moloch-view']).not.toBeDefined();

      expect(scope.$emit).toHaveBeenCalled();
      expect(scope.$emit).toHaveBeenCalledWith('change:search', {
        expression: undefined,
        view      : undefined
      });

      expect(rootScope.$broadcast).toHaveBeenCalled();
      expect(rootScope.$broadcast).toHaveBeenCalledWith('issue:search', {
        expression: undefined,
        view      : undefined
      });
    });

    it('should set message and type to null', function() {
      search.message = 'MESSAGE';
      search.messageType = 'success';

      search.messageDone();

      expect(search.message).toEqual(null);
      expect(search.messageType).toEqual(null);
    });

    it('should show add tags form', function() {
      search.addTags();

      expect(search.actionForm).toEqual('add:tags');
      expect(search.showApplyButtons).toBeTruthy();
    });

    it('should show remove tags form', function() {
      search.removeTags();

      expect(search.actionForm).toEqual('remove:tags');
      expect(search.showApplyButtons).toBeTruthy();
    });

    it('should show export pcap form', function() {
      search.exportPCAP();

      expect(search.actionForm).toEqual('export:pcap');
      expect(search.showApplyButtons).toBeTruthy();
    });

    it('should show export csv form', function() {
      search.exportCSV();

      expect(search.actionForm).toEqual('export:csv');
      expect(search.showApplyButtons).toBeTruthy();
    });

    it('should show scrub pcap form', function() {
      search.scrubPCAP();

      expect(search.actionForm).toEqual('scrub:pcap');
      expect(search.showApplyButtons).toBeTruthy();
    });

    it('should show delete session form', function() {
      search.deleteSession();

      expect(search.actionForm).toEqual('delete:session');
      expect(search.showApplyButtons).toBeTruthy();
    });

    it('should show send session form', function() {
      search.sendSession('clusterid');

      expect(search.cluster).toEqual('clusterid');
      expect(search.actionForm).toEqual('send:session');
      expect(search.showApplyButtons).toBeTruthy();
    });

    it('should show create view form', function() {
      search.createView();

      expect(search.actionForm).toEqual('create:view');
      expect(search.showApplyButtons).toBeFalsy();
    });

  });

})();
