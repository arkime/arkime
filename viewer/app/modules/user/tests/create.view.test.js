(function() {

  'use strict';

  describe('Create View Component ->', function() {

    // load the module
    beforeEach(angular.mock.module('moloch'));

    let scope, createViewComponent, $httpBackend;
    let expression = 'protocols == udp';

    // Initialize and a mock scope
    beforeEach(inject(function(
       _$httpBackend_,
       UserService,
       $componentController,
       $rootScope) {

      $httpBackend = _$httpBackend_;

      scope = $rootScope.$new();

      createViewComponent = $componentController('createView', {
        $scope      : scope,
        UserService : UserService
      }, {
        expression  : expression
      });

      // spy on functions called in controller
      spyOn(scope, '$emit').and.callThrough();

      // initialize session component controller
      createViewComponent.$onInit();
      $httpBackend.flush();
    }));

    afterEach(function() {
      $httpBackend.verifyNoOutstandingExpectation();
      $httpBackend.verifyNoOutstandingRequest();
    });

    it('should exist and have dependencies', function() {
      expect(createViewComponent).toBeDefined();
      expect(createViewComponent.$scope).toBeDefined();
      expect(createViewComponent.UserService).toBeDefined();
    });

    it('should populate the expression field with search expression', function() {
      expect(createViewComponent.expression).toEqual(expression);
    });

    it('should emit a "close:form:container" event to close form', function() {
      createViewComponent.cancel();

      expect(scope.$emit).toHaveBeenCalled();
      expect(scope.$emit).toHaveBeenCalledWith('close:form:container');
      expect(scope.$emit.calls.count()).toBe(1);
    });

    it('should emit a "close:form:container" event after successfully creating view', function() {
      createViewComponent.viewName = 'viewName';
      createViewComponent.createView();

      $httpBackend.expect('POST', 'views/create',
         function(postData) {
           let jsonData = JSON.parse(postData);
           expect(jsonData.viewName).toBe('viewName');
           expect(jsonData.expression).toBe(expression);
           return true;
         }
      ).respond(200, { data: { text: 'success', success: true } });

      let args = { message: 'success', success: true };
      expect(scope.$emit).toHaveBeenCalled();
      expect(scope.$emit).toHaveBeenCalledWith('close:form:container', args);
      expect(scope.$emit.calls.count()).toBe(1);

      $httpBackend.flush();
    });

  });

})();
