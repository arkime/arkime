(function() {

  'use strict';

  describe('Session Export CSV Component ->', function() {

    // load the module
    beforeEach(angular.mock.module('moloch'));

    let scope, sessionExportCSVComponent;
    let id = 'sessionid';

    // Initialize and a mock scope
    beforeEach(inject(function(
       SessionService,
       $componentController,
       $rootScope) {

      scope = $rootScope.$new();

      sessionExportCSVComponent = $componentController('exportCsv', {
        $scope        : scope,
        SessionService: SessionService
      }, { sessionid: id });

      // spy on functions called in controller
      spyOn(scope, '$emit').and.callThrough();
      spyOn(sessionExportCSVComponent.SessionService, 'exportCSV');

      // initialize session component controller
      sessionExportCSVComponent.$onInit();
    }));

    it('should exist and have dependencies', function() {
      expect(sessionExportCSVComponent).toBeDefined();
      expect(sessionExportCSVComponent.$scope).toBeDefined();
      expect(sessionExportCSVComponent.SessionService).toBeDefined();
      expect(sessionExportCSVComponent.sessionid).toBeDefined();
      expect(sessionExportCSVComponent.sessionid).toEqual(id);
    });

    it('should have smart defaults', function() {
      expect(sessionExportCSVComponent.include).toBeDefined();
      expect(sessionExportCSVComponent.include).toEqual('no');
      expect(sessionExportCSVComponent.filename).toBeDefined();
      expect(sessionExportCSVComponent.filename).toEqual('sessions.csv');
    });

    it('should be able to close the form', function() {
      sessionExportCSVComponent.cancel();

      expect(scope.$emit).toHaveBeenCalled();
      expect(scope.$emit).toHaveBeenCalledWith('close:form:container');
      expect(scope.$emit.calls.count()).toBe(1);
    });

    it('should send export csv request and close form', function() {
      sessionExportCSVComponent.exportCSV();

      expect(sessionExportCSVComponent.SessionService.exportCSV).toHaveBeenCalled();
      expect(sessionExportCSVComponent.SessionService.exportCSV).toHaveBeenCalledWith(id, 'sessions.csv', 'no');
      expect(sessionExportCSVComponent.SessionService.exportCSV.calls.count()).toBe(1);

      expect(scope.$emit).toHaveBeenCalled();
      expect(scope.$emit).toHaveBeenCalledWith('close:form:container');
      expect(scope.$emit.calls.count()).toBe(1);
    });

  });

})();
