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
      }, {
        sessions    : [{id:id}],
        cluster     : cluster,
        start       : 0,
        applyTo     : 'open',
        numVisible  : 100,
        numMatching : 999999
      });

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
      expect(sessionSendComponent.sessions).toBeDefined();
      expect(sessionSendComponent.sessions).toEqual([{id:id}]);
    });

    it('should have smart defaults', function() {
      expect(sessionSendComponent.segments).toEqual('no');
      expect(sessionSendComponent.tags).toEqual('');
    });

    it('should be able to close the form', function() {
      sessionSendComponent.cancel();

      expect(scope.$emit).toHaveBeenCalled();
      expect(scope.$emit).toHaveBeenCalledWith('close:form:container');
      expect(scope.$emit.calls.count()).toBe(1);
    });

    it('should send send request and close form', function() {
      $httpBackend.expectPOST('sendSessions&expression=undefined');
      $httpBackend.whenPOST('sendSessions&expression=undefined',
         function(postData) {
           let jsonData = JSON.parse(postData);
           expect(jsonData.ids).toBe(id);
           expect(jsonData.tags).toEqual('');
           expect(jsonData.cluster).toEqual(cluster);
           expect(jsonData.segments).toEqual(sessionSendComponent.segments);
           return true;
         }
      ).respond({ status: 200, text: 'Send success' });

      sessionSendComponent.send();

      $httpBackend.flush();

      let args = { message: 'Send success' };
      expect(scope.$emit).toHaveBeenCalled();
      expect(scope.$emit).toHaveBeenCalledWith('close:form:container', args);
      expect(scope.$emit.calls.count()).toBe(1);
    });

    it('should send send request with segment and close form', function() {
      $httpBackend.expectPOST('sendSessions&expression=undefined');

      $httpBackend.whenPOST('sendSessions&expression=undefined',
         function(postData) {
           let jsonData = JSON.parse(postData);
           expect(jsonData.ids).toBe(id);
           expect(jsonData.tags).toEqual('');
           expect(jsonData.cluster).toEqual(cluster);
           expect(jsonData.segments).toEqual(sessionSendComponent.segments);
           return true;
         }
      ).respond({ status: 200, text: 'Send success' });

      sessionSendComponent.segments = 'all';

      sessionSendComponent.send();

      $httpBackend.flush();

      let args = { message: 'Send success' };
      expect(scope.$emit).toHaveBeenCalled();
      expect(scope.$emit).toHaveBeenCalledWith('close:form:container', args);
      expect(scope.$emit.calls.count()).toBe(1);
    });

    it('should send send request with tags and close form', function() {
      $httpBackend.expectPOST('sendSessions&expression=undefined');

      $httpBackend.whenPOST('sendSessions&expression=undefined',
         function(postData) {
           let jsonData = JSON.parse(postData);
           expect(jsonData.ids).toBe(id);
           expect(jsonData.tags).toEqual(sessionSendComponent.tags);
           expect(jsonData.cluster).toEqual(cluster);
           expect(jsonData.segments).toEqual(sessionSendComponent.segments);
           return true;
         }
      ).respond({ status: 200, text: 'Send success' });

      sessionSendComponent.tags = 'tag1,tag2';

      sessionSendComponent.send();

      $httpBackend.flush();

      let args = { message: 'Send success' };
      expect(scope.$emit).toHaveBeenCalled();
      expect(scope.$emit).toHaveBeenCalledWith('close:form:container', args);
      expect(scope.$emit.calls.count()).toBe(1);
    });

  });

})();
