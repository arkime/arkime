(function() {

  'use strict';

  describe('User Service ->', function() {

    // load the module
    beforeEach(angular.mock.module('moloch'));

    var UserService, user = {
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
      var $httpBackend;

      beforeEach(inject(function(_$httpBackend_) {
        $httpBackend = _$httpBackend_;

        $httpBackend.when('GET', 'currentUser')
          .respond(200, user);
      }));

      afterEach(function() {
        $httpBackend.verifyNoOutstandingExpectation();
        $httpBackend.verifyNoOutstandingRequest();
      });

      it('should send a GET request for the current user', function() {
        UserService.getCurrent();
        $httpBackend.expectGET('currentUser');
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

    });

  });

})();
