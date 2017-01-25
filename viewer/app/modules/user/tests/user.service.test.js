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
      settings          : {
        timezone        : 'local',
        detailFormat    : 'last',
        showTimestamps  : 'last',
        sortColumn      : 'start',
        sortDirection   : 'asc',
        spiGraph        : 'no',
        connSrcField    : 'a1',
        connDstField    : 'ip.dst:port',
        numPackets      : 'last'
      }
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

        $httpBackend.when('GET', 'user/current').respond(200, user);
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

      it('should get the current user\'s settings', function() {
        UserService.getSettings();
        $httpBackend.expectGET('user/settings').respond({});
        $httpBackend.flush();
      });

      it('should get another user\'s settings', function() {
        UserService.getSettings('userid');
        $httpBackend.expectGET('user/settings?userId=userid').respond({});
        $httpBackend.flush();
      });

      it('should save user\'s settings', function() {
        UserService.saveSettings(user.settings);

        $httpBackend.expect('POST', 'user/settings/update',
           function(postData) {
             let jsonData = JSON.parse(postData);
             expect(jsonData.timezone).toEqual('local');
             expect(jsonData.detailFormat).toEqual('last');
             expect(jsonData.showTimestamps).toEqual('last');
             expect(jsonData.sortColumn).toEqual('start');
             expect(jsonData.sortDirection).toEqual('asc');
             expect(jsonData.spiGraph).toEqual('no');
             expect(jsonData.connSrcField).toEqual('a1');
             expect(jsonData.connDstField).toEqual('ip.dst:port');
             expect(jsonData.numPackets).toEqual('last');
             return true;
           }
        ).respond(200);

        $httpBackend.flush();
      });

      it('should save another user\'s settings', function() {
        UserService.saveSettings(user.settings, 'userid');

        $httpBackend.expect('POST', 'user/settings/update?userId=userid',
           function(postData) {
             let jsonData = JSON.parse(postData);
             expect(jsonData.timezone).toEqual('local');
             expect(jsonData.detailFormat).toEqual('last');
             expect(jsonData.showTimestamps).toEqual('last');
             expect(jsonData.sortColumn).toEqual('start');
             expect(jsonData.sortDirection).toEqual('asc');
             expect(jsonData.spiGraph).toEqual('no');
             expect(jsonData.connSrcField).toEqual('a1');
             expect(jsonData.connDstField).toEqual('ip.dst:port');
             expect(jsonData.numPackets).toEqual('last');
             return true;
           }
        ).respond(200);

        $httpBackend.flush();
      });

      it('should get user\'s views', function() {
        UserService.getViews();
        $httpBackend.expectGET('user/views').respond({});
        $httpBackend.flush();
      });

      it('should get another user\'s views', function() {
        UserService.getViews('userid');
        $httpBackend.expectGET('user/views?userId=userid').respond({});
        $httpBackend.flush();
      });

      it('should create a specified view', function() {
        UserService.createView({viewName: 'viewName', expression: 'protocols == udp'});

        $httpBackend.expect('POST', 'user/views/create',
           function(postData) {
             let jsonData = JSON.parse(postData);
             expect(jsonData.viewName).toEqual('viewName');
             expect(jsonData.expression).toEqual('protocols == udp');
             return true;
           }
        ).respond(200);

        $httpBackend.flush();
      });

      it('should create a specified view for another user', function() {
        UserService.createView(
          {viewName: 'viewName', expression: 'protocols == udp'},
          'userid'
        );

        $httpBackend.expect('POST', 'user/views/create?userId=userid',
           function(postData) {
             let jsonData = JSON.parse(postData);
             expect(jsonData.viewName).toEqual('viewName');
             expect(jsonData.expression).toEqual('protocols == udp');
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
             expect(jsonData.view).toEqual('viewName');
             return true;
           }
        ).respond(200);

        $httpBackend.flush();
      });

      it('should delete another user\'s specified view', function() {
        UserService.deleteView('viewName', 'userid');

        $httpBackend.expect('POST', 'user/views/delete?userId=userid',
           function(postData) {
             let jsonData = JSON.parse(postData);
             expect(jsonData.view).toEqual('viewName');
             return true;
           }
        ).respond(200);

        $httpBackend.flush();
      });

      it('should update a user\'s specified view', function() {
        UserService.updateView({
          key       : 'viewName',
          viewName  : 'viewName',
          expression: 'protocols == udp'
        });

        $httpBackend.expect('POST', 'user/views/update',
           function(postData) {
             let jsonData = JSON.parse(postData);
             expect(jsonData.key).toEqual('viewName');
             expect(jsonData.viewName).toEqual('viewName');
             expect(jsonData.expression).toEqual('protocols == udp');
             return true;
           }
        ).respond(200, { views: {
          viewName : { expression: 'protocols == udp' }
        }});

        $httpBackend.flush();
      });

      it('should update another user\'s specified view', function() {
        UserService.updateView({
          key       : 'viewName',
          viewName  : 'viewName',
          expression: 'protocols == udp'
        }, 'userid');

        $httpBackend.expect('POST', 'user/views/update?userId=userid',
           function(postData) {
             let jsonData = JSON.parse(postData);
             expect(jsonData.key).toEqual('viewName');
             expect(jsonData.viewName).toEqual('viewName');
             expect(jsonData.expression).toEqual('protocols == udp');
             return true;
           }
        ).respond(200, { views: {
          viewName : { expression: 'protocols == udp' }
        }});

        $httpBackend.flush();
      });

      it('should get user\'s cron queries', function() {
        UserService.getCronQueries();
        $httpBackend.expectGET('user/cron').respond({});
        $httpBackend.flush();
      });

      it('should get another user\'s cron queries', function() {
        UserService.getCronQueries('userid');
        $httpBackend.expectGET('user/cron?userId=userid').respond({});
        $httpBackend.flush();
      });

      it('should create a specified cron query', function() {
        UserService.createCronQuery({
          enabled : true,
          name    : 'cron query name',
          query   : 'expression',
          tags    : 'taggy',
          action  : 'tag'
        });

        $httpBackend.expect('POST', 'user/cron/create',
           function(postData) {
             let jsonData = JSON.parse(postData);
             expect(jsonData.enabled).toBeTruthy();
             expect(jsonData.name).toEqual('cron query name');
             expect(jsonData.query).toEqual('expression');
             expect(jsonData.tags).toEqual('taggy');
             expect(jsonData.action).toEqual('tag');
             return true;
           }
        ).respond(200);

        $httpBackend.flush();
      });

      it('should create a specified cron query for another user', function() {
        UserService.createCronQuery({
          enabled : true,
          name    : 'cron query name',
          query   : 'expression',
          tags    : 'taggy',
          action  : 'tag'
        }, 'userid');

        $httpBackend.expect('POST', 'user/cron/create?userId=userid',
           function(postData) {
             let jsonData = JSON.parse(postData);
             expect(jsonData.enabled).toBeTruthy();
             expect(jsonData.name).toEqual('cron query name');
             expect(jsonData.query).toEqual('expression');
             expect(jsonData.tags).toEqual('taggy');
             expect(jsonData.action).toEqual('tag');
             return true;
           }
        ).respond(200);

        $httpBackend.flush();
      });

      it('should delete a user\'s specified cron query', function() {
        UserService.deleteCronQuery('key1');

        $httpBackend.expect('POST', 'user/cron/delete',
           function(postData) {
             let jsonData = JSON.parse(postData);
             expect(jsonData.key).toEqual('key1');
             return true;
           }
        ).respond(200);

        $httpBackend.flush();
      });

      it('should delete another user\'s specified cron query', function() {
        UserService.deleteCronQuery('key1', 'userid');

        $httpBackend.expect('POST', 'user/cron/delete?userId=userid',
           function(postData) {
             let jsonData = JSON.parse(postData);
             expect(jsonData.key).toEqual('key1');
             return true;
           }
        ).respond(200);

        $httpBackend.flush();
      });

      it('should update a user\'s specified cron query', function() {
        UserService.updateCronQuery({
          enabled : false,
          name    : 'new cron query name',
          query   : 'new expression',
          tags    : 'taggy,tag2',
          action  : 'tag'
        });

        $httpBackend.expect('POST', 'user/cron/update',
           function(postData) {
             let jsonData = JSON.parse(postData);
             expect(jsonData.enabled).toBeFalsy();
             expect(jsonData.name).toEqual('new cron query name');
             expect(jsonData.query).toEqual('new expression');
             expect(jsonData.tags).toEqual('taggy,tag2');
             expect(jsonData.action).toEqual('tag');
             return true;
           }
        ).respond(200);

        $httpBackend.flush();
      });

      it('should update another user\'s specified cron query', function() {
        UserService.updateCronQuery({
          enabled : false,
          name    : 'new cron query name',
          query   : 'new expression',
          tags    : 'taggy,tag2',
          action  : 'tag'
        }, 'userid');

        $httpBackend.expect('POST', 'user/cron/update?userId=userid',
           function(postData) {
             let jsonData = JSON.parse(postData);
             expect(jsonData.enabled).toBeFalsy();
             expect(jsonData.name).toEqual('new cron query name');
             expect(jsonData.query).toEqual('new expression');
             expect(jsonData.tags).toEqual('taggy,tag2');
             expect(jsonData.action).toEqual('tag');
             return true;
           }
        ).respond(200);

        $httpBackend.flush();
      });

      it('should change a user\'s password', function() {
        let data = {
          currentPassword :'currentpassword',
          newPassword     :'newawesomepassword!'
        };

        UserService.changePassword(data);

        $httpBackend.expect('POST', 'user/password/change',
           function(postData) {
             let jsonData = JSON.parse(postData);
             expect(jsonData.currentPassword).toEqual('currentpassword');
             expect(jsonData.newPassword).toEqual('newawesomepassword!');
             return true;
           }
        ).respond(200);

        $httpBackend.flush();
      });

      it('should change another user\'s password', function() {
        let data = {
          newPassword     :'newawesomepassword!'
        };

        UserService.changePassword(data, 'userid');

        $httpBackend.expect('POST', 'user/password/change?userId=userid',
           function(postData) {
             let jsonData = JSON.parse(postData);
             expect(jsonData.currentPassword).not.toBeDefined();
             expect(jsonData.newPassword).toEqual('newawesomepassword!');
             return true;
           }
        ).respond(200);

        $httpBackend.flush();
      });

    });

  });

})();
