(function() {

  'use strict';

  describe('Session Export PCAP Component ->', function() {

    // load the module
    beforeEach(angular.mock.module('moloch'));

    let scope, sessionExportPCAPComponent;
    let id = 'sessionid';

    // Initialize and a mock scope
    beforeEach(inject(function(
       SessionService,
       $componentController,
       $rootScope) {

      scope = $rootScope.$new();

      sessionExportPCAPComponent = $componentController('exportPcap', {
        $scope        : scope,
        SessionService: SessionService
      }, {
        sessions    : [{id:id}],
        start       : 0,
        applyTo     : 'open',
        numVisible  : 100,
        numMatching : 999999
      });

      // spy on functions called in controller
      spyOn(scope, '$emit').and.callThrough();
      spyOn(sessionExportPCAPComponent.SessionService, 'exportPCAP');

      // initialize component controller
      sessionExportPCAPComponent.$onInit();
    }));

    it('should exist and have dependencies', function() {
      expect(sessionExportPCAPComponent).toBeDefined();
      expect(sessionExportPCAPComponent.$scope).toBeDefined();
      expect(sessionExportPCAPComponent.SessionService).toBeDefined();
      expect(sessionExportPCAPComponent.sessions).toBeDefined();
      expect(sessionExportPCAPComponent.sessions).toEqual([{id:id}]);
    });

    it('should have smart defaults', function() {
      expect(sessionExportPCAPComponent.segments).toBeDefined();
      expect(sessionExportPCAPComponent.segments).toEqual('no');
      expect(sessionExportPCAPComponent.filename).toBeDefined();
      expect(sessionExportPCAPComponent.filename).toEqual('sessions.pcap');
    });

    it('should be able to close the form', function() {
      sessionExportPCAPComponent.cancel();

      expect(scope.$emit).toHaveBeenCalled();
      expect(scope.$emit).toHaveBeenCalledWith('close:form:container');
      expect(scope.$emit.calls.count()).toBe(1);
    });

    it('should send export pcap request and close form', function() {
      let params = {
        start       : sessionExportPCAPComponent.start,
        applyTo     : sessionExportPCAPComponent.applyTo,
        filename    : sessionExportPCAPComponent.filename,
        segments    : sessionExportPCAPComponent.segments,
        sessions    : sessionExportPCAPComponent.sessions,
        numVisible  : sessionExportPCAPComponent.numVisible,
        numMatching : sessionExportPCAPComponent.numMatching
      };

      sessionExportPCAPComponent.exportPCAP();

      expect(sessionExportPCAPComponent.SessionService.exportPCAP).toHaveBeenCalled();
      expect(sessionExportPCAPComponent.SessionService.exportPCAP).toHaveBeenCalledWith(params);
      expect(sessionExportPCAPComponent.SessionService.exportPCAP.calls.count()).toBe(1);

      expect(scope.$emit).toHaveBeenCalled();
      expect(scope.$emit).toHaveBeenCalledWith('close:form:container');
      expect(scope.$emit.calls.count()).toBe(1);
    });

  });

})();
