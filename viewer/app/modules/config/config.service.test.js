(function() {

  'use strict';

  describe('Config Service ->', function() {

    // load the module
    beforeEach(angular.mock.module('moloch'));
    beforeEach(angular.mock.module('moloch.config'));

    let ConfigService, $rootScope;

    // load service
    beforeEach(inject(function(_$rootScope_, _ConfigService_) {
      $rootScope    = _$rootScope_;
      ConfigService = _ConfigService_;
    }));

    it('should exist', function() {
      expect(ConfigService).toBeDefined();
    });

    describe('http requests ->', function() {
      let $httpBackend;

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
        ConfigService.getTitleConfig();
        $httpBackend.expectGET('titleconfig');
        $httpBackend.flush();
      });

      it('should update the title (page)', function() {
        ConfigService.setTitle('Page 1', null, null)
          .then((title) => {
            expect(title).toContain('Page 1');
          });

        $httpBackend.flush();
      });

      it('should update the title (expression)', function() {
        ConfigService.setTitle(null, 'country == USA', null)
          .then((title) => {
            expect(title).toContain('country == USA');
          });

        $httpBackend.flush();
      });

      it('should update the title (view)', function() {
        ConfigService.setTitle('Page 1', 'country == USA', 'super cool view')
          .then((title) => {
            expect(title).toContain('super cool view');
          });

        $httpBackend.flush();
      });

      it('should send a GET request for moloch clickable fields', function() {
        ConfigService.getMolochClickables();
        $httpBackend.expectGET('molochRightClick').respond(200);
        $httpBackend.flush();
      });

      it('should send a GET request for moloch clusters', function() {
        ConfigService.getMolochClusters();
        $httpBackend.expectGET('molochclusters').respond(200);
        $httpBackend.flush();
      });

    });

  });

})();
