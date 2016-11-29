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
      }, { sessionid: id });

      // spy on functions called in controller
      spyOn(scope, '$emit').and.callThrough();
      spyOn(sessionExportPCAPComponent.SessionService, 'exportPCAP');

      // initialize session component controller
      sessionExportPCAPComponent.$onInit();
    }));

    it('should exist and have dependencies', function() {
      expect(sessionExportPCAPComponent).toBeDefined();
      expect(sessionExportPCAPComponent.$scope).toBeDefined();
      expect(sessionExportPCAPComponent.SessionService).toBeDefined();
      expect(sessionExportPCAPComponent.sessionid).toBeDefined();
      expect(sessionExportPCAPComponent.sessionid).toEqual(id);
    });

    it('should have smart defaults', function() {
      expect(sessionExportPCAPComponent.include).toBeDefined();
      expect(sessionExportPCAPComponent.include).toEqual('no');
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
      sessionExportPCAPComponent.exportPCAP();

      expect(sessionExportPCAPComponent.SessionService.exportPCAP).toHaveBeenCalled();
      expect(sessionExportPCAPComponent.SessionService.exportPCAP).toHaveBeenCalledWith(id, 'sessions.pcap', 'no');
      expect(sessionExportPCAPComponent.SessionService.exportPCAP.calls.count()).toBe(1);

      expect(scope.$emit).toHaveBeenCalled();
      expect(scope.$emit).toHaveBeenCalledWith('close:form:container');
      expect(scope.$emit.calls.count()).toBe(1);
    });

  });

})();
