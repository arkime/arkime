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
      }, {
        add         : false,
        sessions    : [{id:id}],
        start       : 0,
        applyTo     : 'open',
        numVisible  : 100,
        numMatching : 999999
      });

      $httpBackend.whenPOST('addTags',
         function(postData) {
           let jsonData = JSON.parse(postData);
           expect(jsonData.ids).toBe(id);
           expect(jsonData.tags).toEqual(sessionTagComponent.tags);
           expect(jsonData.segments).toEqual(sessionTagComponent.segments);
           return true;
         }
      ).respond(200, { text: 'Tag success' });

      $httpBackend.whenPOST('removeTags',
         function(postData) {
           let jsonData = JSON.parse(postData);
           expect(jsonData.ids).toBe(id);
           expect(jsonData.tags).toEqual(sessionTagComponent.tags);
           expect(jsonData.segments).toEqual(sessionTagComponent.segments);
           return true;
         }
      ).respond(200, { text: 'Tag success' });

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
      expect(sessionTagComponent.sessions).toBeDefined();
      expect(sessionTagComponent.sessions).toEqual([{id:id}]);
    });

    it('should have smart defaults', function() {
      expect(sessionTagComponent.segments).toEqual('no');
      expect(sessionTagComponent.tags).toEqual('');
    });

    it('should be able to close the form', function() {
      sessionTagComponent.cancel();

      expect(scope.$emit).toHaveBeenCalled();
      expect(scope.$emit).toHaveBeenCalledWith('close:form:container');
      expect(scope.$emit.calls.count()).toBe(1);
    });

    it('should send add tags request and close form', function() {
      $httpBackend.expectPOST('addTags');

      sessionTagComponent.tags = 'tag1,tag2';

      sessionTagComponent.apply(true);

      $httpBackend.flush();

      let args = { message: 'Tag success', reloadData: true, success: undefined };
      expect(scope.$emit).toHaveBeenCalled();
      expect(scope.$emit).toHaveBeenCalledWith('close:form:container', args);
      expect(scope.$emit.calls.count()).toBe(1);
    });

    it('should send add tags request with segments and close form', function() {
      $httpBackend.expectPOST('addTags');

      sessionTagComponent.tags = 'tag1,tag2';
      sessionTagComponent.segments = 'all';

      sessionTagComponent.apply(true);

      $httpBackend.flush();

      let args = { message: 'Tag success', reloadData: true, success: undefined };
      expect(scope.$emit).toHaveBeenCalled();
      expect(scope.$emit).toHaveBeenCalledWith('close:form:container', args);
      expect(scope.$emit.calls.count()).toBe(1);
    });

    it('should not send add tags request and close form', function() {
      sessionTagComponent.apply(true);

      expect(sessionTagComponent.error).toEqual('No tag(s) specified.');
    });

    it('should send remove tags request and close form', function() {
      $httpBackend.expectPOST('removeTags');

      sessionTagComponent.tags = 'tag1,tag2';

      sessionTagComponent.apply(false);

      $httpBackend.flush();

      let args = { message: 'Tag success', reloadData: true, success: undefined };
      expect(scope.$emit).toHaveBeenCalled();
      expect(scope.$emit).toHaveBeenCalledWith('close:form:container', args);
      expect(scope.$emit.calls.count()).toBe(1);
    });

    it('should not send remove tags request and close form', function() {
      sessionTagComponent.apply(false);

      expect(sessionTagComponent.error).toEqual('No tag(s) specified.');
    });

  });

})();
