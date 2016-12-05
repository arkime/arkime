(function() {

  'use strict';

  describe('Session Scrub PCAP Component ->', function() {

    // load the module
    beforeEach(angular.mock.module('moloch'));

    let scope, sessionScrubPCAPComponent, $httpBackend;
    let id = 'sessionid';

    // Initialize and a mock scope
    beforeEach(inject(function(
       _$httpBackend_,
       SessionService,
       $componentController,
       $rootScope) {

      $httpBackend = _$httpBackend_;

      scope = $rootScope.$new();

      sessionScrubPCAPComponent = $componentController('scrubPcap', {
        $scope        : scope,
        SessionService: SessionService
      }, {
        sessions    : [{id:id}],
        start       : 0,
        applyTo     : 'open',
        numVisible  : 100,
        numMatching : 999999
      });

      $httpBackend.whenPOST('scrub&expression=undefined',
         function(postData) {
           let jsonData = JSON.parse(postData);
           expect(jsonData.ids).toBe(id);
           expect(jsonData.segments).toEqual(sessionScrubPCAPComponent.segments);
           return true;
         }
      ).respond(200, { text:'Scrub success' });

      // spy on functions called in controller
      spyOn(scope, '$emit').and.callThrough();

      // initialize component controller
      sessionScrubPCAPComponent.$onInit();
    }));

    afterEach(function() {
      $httpBackend.verifyNoOutstandingExpectation();
      $httpBackend.verifyNoOutstandingRequest();
    });

    it('should exist and have dependencies', function() {
      expect(sessionScrubPCAPComponent).toBeDefined();
      expect(sessionScrubPCAPComponent.$scope).toBeDefined();
      expect(sessionScrubPCAPComponent.SessionService).toBeDefined();
      expect(sessionScrubPCAPComponent.sessions).toBeDefined();
      expect(sessionScrubPCAPComponent.sessions).toEqual([{id:id}]);
    });

    it('should have smart defaults', function() {
      expect(sessionScrubPCAPComponent.segments).toEqual('no');
    });

    it('should be able to close the form', function() {
      sessionScrubPCAPComponent.cancel();

      expect(scope.$emit).toHaveBeenCalled();
      expect(scope.$emit).toHaveBeenCalledWith('close:form:container');
      expect(scope.$emit.calls.count()).toBe(1);
    });

    it('should send scrub request and close form', function() {
      $httpBackend.expectPOST('scrub&expression=undefined');

      sessionScrubPCAPComponent.scrubPCAP();

      $httpBackend.flush();

      let args = { reloadData: true, message: 'Scrub success' };
      expect(scope.$emit).toHaveBeenCalled();
      expect(scope.$emit).toHaveBeenCalledWith('close:form:container', args);
      expect(scope.$emit.calls.count()).toBe(1);
    });

    it('should send scrub request with segment and close form', function() {
      $httpBackend.expectPOST('scrub&expression=undefined');

      sessionScrubPCAPComponent.include = 'all';

      sessionScrubPCAPComponent.scrubPCAP();

      $httpBackend.flush();

      let args = { reloadData: true, message: 'Scrub success' };
      expect(scope.$emit).toHaveBeenCalled();
      expect(scope.$emit).toHaveBeenCalledWith('close:form:container', args);
      expect(scope.$emit.calls.count()).toBe(1);
    });

  });

})();
