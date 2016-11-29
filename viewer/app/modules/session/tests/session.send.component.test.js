(function() {

  'use strict';

  describe('Session Send Component ->', function() {

    // load the module
    beforeEach(angular.mock.module('moloch'));

    let scope, sessionSendComponent, $httpBackend;
    let id = 'sessionid';
    let cluster = { name: 'test', url: 'http://test.cluster' };

    // Initialize and a mock scope
    beforeEach(inject(function(
       _$httpBackend_,
       SessionService,
       $componentController,
       $rootScope) {

      $httpBackend = _$httpBackend_;

      scope = $rootScope.$new();

      sessionSendComponent = $componentController('sessionSend', {
        $scope        : scope,
        SessionService: SessionService
      }, { sessionid: id, cluster:cluster });

      // spy on functions called in controller
      spyOn(scope, '$emit').and.callThrough();

      // initialize session component controller
      sessionSendComponent.$onInit();
    }));

    afterEach(function() {
      $httpBackend.verifyNoOutstandingExpectation();
      $httpBackend.verifyNoOutstandingRequest();
    });

    it('should exist and have dependencies', function() {
      expect(sessionSendComponent).toBeDefined();
      expect(sessionSendComponent.$scope).toBeDefined();
      expect(sessionSendComponent.SessionService).toBeDefined();
      expect(sessionSendComponent.sessionid).toBeDefined();
      expect(sessionSendComponent.sessionid).toEqual(id);
    });

    it('should have smart defaults', function() {
      expect(sessionSendComponent.include).toEqual('no');
      expect(sessionSendComponent.tags).toEqual('');
    });

    it('should be able to close the form', function() {
      sessionSendComponent.cancel();

      expect(scope.$emit).toHaveBeenCalled();
      expect(scope.$emit).toHaveBeenCalledWith('close:form:container');
      expect(scope.$emit.calls.count()).toBe(1);
    });

    it('should send send request and close form', function() {
      $httpBackend.whenPOST('sendSessions',
         function(postData) {
           let jsonData = JSON.parse(postData);
           expect(jsonData.ids).toBe(id);
           expect(jsonData.tags).toEqual('');
           expect(jsonData.cluster).toEqual(cluster);
           expect(jsonData.segments).not.toBeDefined();
           return true;
         }
      ).respond({ status: 200, text: 'success' });

      sessionSendComponent.send();

      $httpBackend.flush();

      let args = { message: 'success' };
      expect(scope.$emit).toHaveBeenCalled();
      expect(scope.$emit).toHaveBeenCalledWith('close:form:container', args);
      expect(scope.$emit.calls.count()).toBe(1);
    });

    it('should send send request with segment and close form', function() {
      sessionSendComponent.include = 'all';

      $httpBackend.whenPOST('sendSessions',
         function(postData) {
           let jsonData = JSON.parse(postData);
           expect(jsonData.ids).toBe(id);
           expect(jsonData.tags).toEqual('');
           expect(jsonData.cluster).toEqual(cluster);
           expect(jsonData.segments).toEqual(sessionSendComponent.include);
           return true;
         }
      ).respond({ status: 200, text: 'success' });

      sessionSendComponent.send();

      $httpBackend.flush();

      let args = { message: 'success' };
      expect(scope.$emit).toHaveBeenCalled();
      expect(scope.$emit).toHaveBeenCalledWith('close:form:container', args);
      expect(scope.$emit.calls.count()).toBe(1);
    });

    it('should send send request with tags and close form', function() {
      sessionSendComponent.tags = 'tag1,tag2';

      $httpBackend.whenPOST('sendSessions',
         function(postData) {
           let jsonData = JSON.parse(postData);
           expect(jsonData.ids).toBe(id);
           expect(jsonData.tags).toEqual(sessionSendComponent.tags);
           expect(jsonData.cluster).toEqual(cluster);
           expect(jsonData.segments).not.toBeDefined();
           return true;
         }
      ).respond({ status: 200, text: 'success' });

      sessionSendComponent.send();

      $httpBackend.flush();

      let args = { message: 'success' };
      expect(scope.$emit).toHaveBeenCalled();
      expect(scope.$emit).toHaveBeenCalledWith('close:form:container', args);
      expect(scope.$emit.calls.count()).toBe(1);
    });

  });

})();
