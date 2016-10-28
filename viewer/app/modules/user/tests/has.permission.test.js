(function() {

  'use strict';

  describe('Has Permission Directive ->', function() {

    // load the module
    beforeEach(angular.mock.module('moloch'));

    var $httpBackend, $compile, element, template, templateAsHtml, scope;
    var user = {
      userId            : 'anonymous',
      enabled           : true,
      webEnabled        : true,
      emailSearch       : true,
      createEnabled     : true,
      removeEnabled     : true,
      headerAuthEnabled : false,
      settings          : {}
    };

    beforeEach(inject(function(
      _$httpBackend_,
      _$rootScope_,
      _$compile_) {

      $httpBackend = _$httpBackend_;
      $compile = _$compile_;

      $httpBackend.expectGET('currentUser')
        .respond(200, user);

      scope = _$rootScope_.$new();

      scope.hasPermission = 'enabled';

      var htmlString = '<div class="btn" has-permission="enabled">Special Button!</div>';

      element = angular.element(htmlString);
      template  = $compile(element)(scope);

      scope.$digest();

      templateAsHtml = template.html();

      $httpBackend.flush();
    }));

    afterEach(function() {
      $httpBackend.verifyNoOutstandingExpectation();
      $httpBackend.verifyNoOutstandingRequest();
    });

    it('should render html', function() {
      expect(templateAsHtml).toBeDefined();
    });

    it('should show the element if necessary', function() {
      expect($(element).css('display') === 'none').toBeFalsy();
    });

    it('should hide the element if necessary', function() {
      scope.hasPermission = 'headerAuthEnabled';

      var htmlString = '<div class="btn" has-permission="headerAuthEnabled">Special Button!</div>';

      element = angular.element(htmlString);
      template  = $compile(element)(scope);

      scope.$digest();

      expect($(element).css('display') === 'none').toBeTruthy();
    });

  });

})();
