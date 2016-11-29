(function() {

  'use strict';

  describe('Session Tag Component ->', function() {

    // load the module
    beforeEach(angular.mock.module('moloch'));

    let scope, sessionTagComponent, $httpBackend;
    let id = 'sessionid';

    // Initialize and a mock scope
    beforeEach(inject(function(
       _$httpBackend_,
       SessionService,
       $componentController,
       $rootScope) {

      $httpBackend = _$httpBackend_;

      scope = $rootScope.$new();

      sessionTagComponent = $componentController('sessionTag', {
        $scope        : scope,
        SessionService: SessionService
      }, { sessionid: id, add: false });

      // spy on functions called in controller
      spyOn(scope, '$emit').and.callThrough();

      // initialize session component controller
      sessionTagComponent.$onInit();
    }));

    afterEach(function() {
      $httpBackend.verifyNoOutstandingExpectation();
      $httpBackend.verifyNoOutstandingRequest();
    });

    it('should exist and have dependencies', function() {
      expect(sessionTagComponent).toBeDefined();
      expect(sessionTagComponent.$scope).toBeDefined();
      expect(sessionTagComponent.SessionService).toBeDefined();
      expect(sessionTagComponent.sessionid).toBeDefined();
      expect(sessionTagComponent.sessionid).toEqual(id);
    });

    it('should have smart defaults', function() {
      expect(sessionTagComponent.include).toEqual('no');
      expect(sessionTagComponent.tags).toEqual('');
    });

    it('should be able to close the form', function() {
      sessionTagComponent.cancel();

      expect(scope.$emit).toHaveBeenCalled();
      expect(scope.$emit).toHaveBeenCalledWith('close:form:container');
      expect(scope.$emit.calls.count()).toBe(1);
    });

    it('should send add tags request and close form', function() {
      sessionTagComponent.tags = 'tag1,tag2';

      $httpBackend.whenPOST('addTags',
         function(postData) {
           let jsonData = JSON.parse(postData);
           expect(jsonData.ids).toBe(id);
           expect(jsonData.tags).toEqual(sessionTagComponent.tags);
           expect(jsonData.segments).not.toBeDefined();
           return true;
         }
      ).respond(200);

      sessionTagComponent.addTags();

      $httpBackend.flush();

      expect(scope.$emit).toHaveBeenCalled();
      expect(scope.$emit).toHaveBeenCalledWith('update:tags', { id: id });
      expect(scope.$emit.calls.count()).toBe(1);
    });

    it('should send add tags request with segments and close form', function() {
      sessionTagComponent.tags = 'tag1,tag2';
      sessionTagComponent.include = 'all';

      $httpBackend.whenPOST('addTags',
         function(postData) {
           let jsonData = JSON.parse(postData);
           expect(jsonData.ids).toBe(id);
           expect(jsonData.tags).toEqual(sessionTagComponent.tags);
           expect(jsonData.segments).toEqual(sessionTagComponent.include);
           return true;
         }
      ).respond(200);

      sessionTagComponent.addTags();

      $httpBackend.flush();

      expect(scope.$emit).toHaveBeenCalled();
      expect(scope.$emit).toHaveBeenCalledWith('update:tags', { id: id });
      expect(scope.$emit.calls.count()).toBe(1);
    });

    it('should not send add tags request and close form', function() {
      sessionTagComponent.addTags();

      expect(sessionTagComponent.error).toEqual('No tag(s) specified.');
    });

    it('should send remove tags request and close form', function() {
      sessionTagComponent.tags = 'tag1,tag2';

      $httpBackend.whenPOST('removeTags',
         function(postData) {
           let jsonData = JSON.parse(postData);
           expect(jsonData.ids).toBe(id);
           expect(jsonData.tags).toEqual(sessionTagComponent.tags);
           expect(jsonData.segments).not.toBeDefined();
           return true;
         }
      ).respond(200);

      sessionTagComponent.removeTags();

      $httpBackend.flush();

      expect(scope.$emit).toHaveBeenCalled();
      expect(scope.$emit).toHaveBeenCalledWith('update:tags', { id: id });
      expect(scope.$emit.calls.count()).toBe(1);
    });

    it('should send remove tags request with segments and close form', function() {
      sessionTagComponent.tags = 'tag1,tag2';
      sessionTagComponent.include = 'all';

      $httpBackend.whenPOST('removeTags',
         function(postData) {
           let jsonData = JSON.parse(postData);
           expect(jsonData.ids).toBe(id);
           expect(jsonData.tags).toEqual(sessionTagComponent.tags);
           expect(jsonData.segments).toEqual(sessionTagComponent.include);
           return true;
         }
      ).respond(200);

      sessionTagComponent.removeTags();

      $httpBackend.flush();

      expect(scope.$emit).toHaveBeenCalled();
      expect(scope.$emit).toHaveBeenCalledWith('update:tags', { id: id });
      expect(scope.$emit.calls.count()).toBe(1);
    });

    it('should not send remove tags request and close form', function() {
      sessionTagComponent.removeTags();

      expect(sessionTagComponent.error).toEqual('No tag(s) specified.');
    });

  });

})();
