(function() {

  'use strict';

  describe('Help Directive ->', function() {

    // load the module
    beforeEach(angular.mock.module('moloch'));

    let scope, help, templateAsHtml, $httpBackend;

    // Initialize and a mock scope
    beforeEach(inject(function(
      _$httpBackend_,
      $componentController,
      _$anchorScroll_,
      $rootScope,
      $compile,
      FieldService) {

      $httpBackend = _$httpBackend_;

      $httpBackend.expectGET('fields?array=true').respond({});
      $httpBackend.expectGET('user/current').respond({});

      scope = $rootScope.$new();

      let element   = angular.element('<moloch-help></moloch-help>');
      let template  = $compile(element)(scope);

      help = $componentController('molochHelp', {
        $anchorScroll : _$anchorScroll_,
        FieldService  : FieldService
      });

      scope.$digest();
      templateAsHtml = template.html();

      // initialize component controller
      help.$onInit();
    }));

    it('should exist and have dependencies', function() {
      expect(help).toBeDefined();
      expect(help.$anchorScroll).toBeDefined();
      expect(help.FieldService).toBeDefined();
    });

    it('should create an info object for the field table', function() {
      expect(help.info).toBeDefined();
    });

  });

})();
