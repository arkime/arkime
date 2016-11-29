(function() {

  'use strict';

  describe('Session Delete Component ->', function() {

    // load the module
    beforeEach(angular.mock.module('moloch'));

    let scope, sessionDeleteComponent, $httpBackend;
    let id = 'sessionid';

    // Initialize and a mock scope
    beforeEach(inject(function(
       _$httpBackend_,
       SessionService,
       $componentController,
       $rootScope) {

      $httpBackend = _$httpBackend_;

      scope = $rootScope.$new();

      sessionDeleteComponent = $componentController('sessionDelete', {
        $scope        : scope,
        SessionService: SessionService
      }, { sessionid: id });

      // spy on functions called in controller
      spyOn(scope, '$emit').and.callThrough();

      // initialize session component controller
      sessionDeleteComponent.$onInit();
    }));

    afterEach(function() {
      $httpBackend.verifyNoOutstandingExpectation();
      $httpBackend.verifyNoOutstandingRequest();
    });

    it('should exist and have dependencies', function() {
      expect(sessionDeleteComponent).toBeDefined();
      expect(sessionDeleteComponent.$scope).toBeDefined();
      expect(sessionDeleteComponent.SessionService).toBeDefined();
      expect(sessionDeleteComponent.sessionid).toBeDefined();
      expect(sessionDeleteComponent.sessionid).toEqual(id);
    });

    it('should have smart defaults', function() {
      expect(sessionDeleteComponent.include).toEqual('no');
    });

    it('should be able to close the form', function() {
      sessionDeleteComponent.cancel();

      expect(scope.$emit).toHaveBeenCalled();
      expect(scope.$emit).toHaveBeenCalledWith('close:form:container');
      expect(scope.$emit.calls.count()).toBe(1);
    });

    it('should send delete request and close form', function() {
      $httpBackend.whenPOST('delete',
         function(postData) {
           let jsonData = JSON.parse(postData);
           expect(jsonData.ids).toBe(id);
           expect(jsonData.segments).not.toBeDefined();
           return true;
         }
      ).respond(200);

      sessionDeleteComponent.delete();

      $httpBackend.flush();

      let args = { reloadData: true };
      expect(scope.$emit).toHaveBeenCalled();
      expect(scope.$emit).toHaveBeenCalledWith('close:form:container', args);
      expect(scope.$emit.calls.count()).toBe(1);
    });

    it('should send delete request with segment and close form', function() {
      sessionDeleteComponent.include = 'all';

      $httpBackend.whenPOST('delete',
         function(postData) {
           let jsonData = JSON.parse(postData);
           expect(jsonData.ids).toBe(id);
           expect(jsonData.segments).toEqual(sessionDeleteComponent.include);
           return true;
         }
      ).respond(200);

      sessionDeleteComponent.delete();

      $httpBackend.flush();

      let args = { reloadData: true };
      expect(scope.$emit).toHaveBeenCalled();
      expect(scope.$emit).toHaveBeenCalledWith('close:form:container', args);
      expect(scope.$emit.calls.count()).toBe(1);
    });

  });

})();
