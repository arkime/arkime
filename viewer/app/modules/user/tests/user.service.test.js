(function() {

  'use strict';

  describe('User Service ->', function() {

    // load the module
    beforeEach(angular.mock.module('moloch'));

    let UserService, user = {
      userId            : 'anonymous',
      enabled           : true,
      webEnabled        : true,
      emailSearch       : true,
      createEnabled     : true,
      removeEnabled     : true,
      headerAuthEnabled : false,
      settings          : {}
    };

    // load service
    beforeEach(inject(function(_UserService_) {
      UserService = _UserService_;
    }));

    it('should exist', function() {
      expect(UserService).toBeDefined();
    });

    describe('http requests ->', function() {
      let $httpBackend;

      beforeEach(inject(function(_$httpBackend_) {
        $httpBackend = _$httpBackend_;

        $httpBackend.when('GET', 'user/current')
          .respond(200, user);
      }));

      afterEach(function() {
        $httpBackend.verifyNoOutstandingExpectation();
        $httpBackend.verifyNoOutstandingRequest();
      });

      it('should send a GET request for the current user', function() {
        UserService.getCurrent();

        $httpBackend.expectGET('user/current');

        $httpBackend.flush();
      });

      it('should cache the user', function() {
        UserService.getCurrent();

        $httpBackend.flush();
      });

      it('should determine user permissions', function() {
        UserService.hasPermission('enabled')
          .then((result) => { expect(result).toBeTruthy(); });

        UserService.hasPermission('webEnabled')
          .then((result) => { expect(result).toBeTruthy(); });

        UserService.hasPermission('emailSearch')
          .then((result) => { expect(result).toBeTruthy(); });

        UserService.hasPermission('createEnabled')
          .then((result) => { expect(result).toBeTruthy(); });

        UserService.hasPermission('removeEnabled')
          .then((result) => { expect(result).toBeTruthy(); });

        UserService.hasPermission('headerAuthEnabled')
          .then((result) => { expect(result).toBeFalsy(); });

        $httpBackend.flush();
      });

      it('should get user\'s view', function() {
        UserService.getViews();
        $httpBackend.expectGET('user/views').respond({});
        $httpBackend.flush();
      });

      it('should create a specified view', function() {
        UserService.createView({viewName: 'viewName', expression: 'protocols == udp'});

        $httpBackend.expect('POST', 'user/views/create',
           function(postData) {
             let jsonData = JSON.parse(postData);
             expect(jsonData.viewName).toBe('viewName');
             expect(jsonData.expression).toBe('protocols == udp');
             return true;
           }
        ).respond(200);

        $httpBackend.flush();
      });

      it('should delete a user\'s specified view', function() {
        UserService.deleteView('viewName');

        $httpBackend.expect('POST', 'user/views/delete',
           function(postData) {
             let jsonData = JSON.parse(postData);
             expect(jsonData.view).toBe('viewName');
             return true;
           }
        ).respond(200);

        $httpBackend.flush();
      });

      it('should get user\'s settings', function() {
        UserService.getSettings();
        $httpBackend.expectGET('user/settings').respond({});
        $httpBackend.flush();
      });

    });

  });

})();
