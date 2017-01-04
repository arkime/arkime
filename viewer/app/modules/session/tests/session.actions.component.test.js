(function() {

  'use strict';

  describe('Session Actions Component ->', function() {

    // load the module
    beforeEach(angular.mock.module('moloch'));

    let scope, sessionActionsComponent, $httpBackend;

    // Initialize and a mock scope
    beforeEach(inject(function(
       _$httpBackend_,
       ConfigService,
       $componentController,
       $rootScope) {

      $httpBackend = _$httpBackend_;

      // initial query for moloch clusters to populate actions menu
      $httpBackend.expectGET('molochclusters')
         .respond({});

      scope = $rootScope.$new();

      sessionActionsComponent = $componentController('sessionActions', {
        $scope        : scope,
        ConfigService : ConfigService
      });

      // spy on functions called in controller
      spyOn(scope, '$emit').and.callThrough();

      // initialize session component controller
      sessionActionsComponent.$onInit();
      $httpBackend.flush();
    }));

    afterEach(function() {
      $httpBackend.verifyNoOutstandingExpectation();
      $httpBackend.verifyNoOutstandingRequest();
    });

    it('should exist and have dependencies', function() {
      expect(sessionActionsComponent).toBeDefined();
      expect(sessionActionsComponent.$scope).toBeDefined();
      expect(sessionActionsComponent.ConfigService).toBeDefined();
    });

    it('should be closed to start', function() {
      expect(sessionActionsComponent.isopen).toBeFalsy();
    });

    it('should emit an "open:form:container" event when removing tag', function() {
      let args = { form: 'remove:tags' };
      sessionActionsComponent.removeTags();

      expect(scope.$emit).toHaveBeenCalled();
      expect(scope.$emit).toHaveBeenCalledWith('open:form:container', args);
      expect(scope.$emit.calls.count()).toBe(1);
    });

    it('should emit an "open:form:container" event when exporting pcap', function() {
      let args = { form: 'export:pcap' };
      sessionActionsComponent.exportPCAP();

      expect(scope.$emit).toHaveBeenCalled();
      expect(scope.$emit).toHaveBeenCalledWith('open:form:container', args);
      expect(scope.$emit.calls.count()).toBe(1);
    });

    it('should emit an "open:form:container" event when scrubbing pcap', function() {
      let args = { form: 'scrub:pcap' };
      sessionActionsComponent.scrubPCAP();

      expect(scope.$emit).toHaveBeenCalled();
      expect(scope.$emit).toHaveBeenCalledWith('open:form:container', args);
      expect(scope.$emit.calls.count()).toBe(1);
    });

    it('should emit an "open:form:container" event when deleting session', function() {
      let args = { form: 'delete:session' };
      sessionActionsComponent.deleteSession();

      expect(scope.$emit).toHaveBeenCalled();
      expect(scope.$emit).toHaveBeenCalledWith('open:form:container', args);
      expect(scope.$emit.calls.count()).toBe(1);
    });

    it('should emit an "open:form:container" event when sending session to moloch cluster', function() {
      let cluster = {};
      let args = { form:'send:session', cluster:cluster };
      sessionActionsComponent.sendSession(cluster);

      expect(scope.$emit).toHaveBeenCalled();
      expect(scope.$emit).toHaveBeenCalledWith('open:form:container', args);
      expect(scope.$emit.calls.count()).toBe(1);
    });

  });

})();
