(function() {

  'use strict';

  describe('Config Service ->', function() {

    // load the module
    beforeEach(angular.mock.module('moloch'));
    beforeEach(angular.mock.module('moloch.config'));

    var ConfigService, $rootScope;

    // load service
    beforeEach(inject(function(_$rootScope_, _ConfigService_) {
      $rootScope    = _$rootScope_;
      ConfigService = _ConfigService_;
    }));

    it('should exist', function() {
      expect(ConfigService).toBeDefined();
    });

    describe('http requests ->', function() {
      var $httpBackend;

      beforeEach(inject(function(_$httpBackend_) {
        $httpBackend = _$httpBackend_;

        $httpBackend.when('GET', 'titleconfig')
          .respond(200, '_page_ _-view_ _-expression_');
      }));

      afterEach(function() {
        $httpBackend.verifyNoOutstandingExpectation();
        $httpBackend.verifyNoOutstandingRequest();
      });

      it('should send a GET request for title config', function() {
        var result = ConfigService.getTitleConfig();
        $httpBackend.expectGET('titleconfig');
        $httpBackend.flush();
      });

      it('should update the title (page)', function() {
        ConfigService.setTitle('Page 1', null, null);
        $httpBackend.flush();
        expect($rootScope.title).toContain('Page 1');
      });

      it('should update the title (expression)', function() {
        ConfigService.setTitle('Page 1', 'country == USA', null);
        $httpBackend.flush();
        expect($rootScope.title).toContain('country == USA');
      });

      it('should update the title (view)', function() {
        ConfigService.setTitle('Page 1', 'country == USA', 'super cool view');
        $httpBackend.flush();
        expect($rootScope.title).toContain('super cool view');
      });

    });

  });

})();
