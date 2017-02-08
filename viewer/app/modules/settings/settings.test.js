(function() {

  'use strict';

  /* mock data ============================================================= */
  let user = {
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

  let userViews = { viewy: { name:'viewy', expression:'expression' } };

  let userCronQueries = {
    key1: {
      enabled : true,
      name    : 'cron query name',
      query   : 'expression',
      tags    : 'taggy',
      action  : 'tag',
      lastRun : 1485197671,
      lpValue : 1485197121,
      count   : 0,
      creator : 'moloch'
    }
  };

  let fields = [{
    friendlyName: 'All ASN fields',
    group       : 'general',
    type        : 'textfield',
    dbField     : 'all',
    exp         : 'asn'
  }];


  /* settings tests ======================================================== */
  describe('Settings Component ->', function() {

    // load modules
    beforeEach(angular.mock.module('moloch'));

    let scope, settingsCtrl, $httpBackend;


    /* normal user tests --------------------------------------------------- */
    describe('User editing their own settings ->', function () {

      // Initialize and a mock scope
      beforeEach(inject(function (
        _$httpBackend_,
        $componentController,
        $rootScope,
        $compile,
        $interval,
        $location,
        FieldService,
        ConfigService,
        UserService) {

        $httpBackend = _$httpBackend_;

        $httpBackend.expectGET('user/current').respond(200, user);

        $httpBackend.expectGET('molochclusters').respond(200, {});

        $httpBackend.expectGET('fields?array=true').respond(200, fields);

        $httpBackend.expectGET('user/views').respond(200, userViews);

        $httpBackend.expectGET('user/cron').respond(200, userCronQueries);

        scope = $rootScope.$new();

        settingsCtrl = $componentController('molochSettings', {
          $interval     : $interval,
          $location     : $location,
          $routeParams  : {userId: 'anonymous'},
          UserService   : UserService,
          FieldService  : FieldService,
          ConfigService : ConfigService
        });

        // initialize settings component controller
        settingsCtrl.$onInit();
        $httpBackend.flush();
      }));

      afterEach(function () {
        $httpBackend.verifyNoOutstandingExpectation();
        $httpBackend.verifyNoOutstandingRequest();
      });

      it('should exist and have dependencies', function () {
        expect(settingsCtrl).toBeDefined();
        expect(settingsCtrl.$interval).toBeDefined();
        expect(settingsCtrl.$location).toBeDefined();
        expect(settingsCtrl.$routeParams).toBeDefined();
        expect(settingsCtrl.UserService).toBeDefined();
        expect(settingsCtrl.FieldService).toBeDefined();
        expect(settingsCtrl.ConfigService).toBeDefined();
      });

      it('should have user\'s settings', function () {
        expect(settingsCtrl.settings).toBeDefined();
        expect(settingsCtrl.settings).toEqual(user.settings);
      });

      it('should have user\'s views', function () {
        expect(settingsCtrl.views).toBeDefined();
        expect(settingsCtrl.views).toEqual(userViews);
      });

      it('should have user\'s cron queries', function () {
        expect(settingsCtrl.cronQueries).toBeDefined();
        expect(settingsCtrl.cronQueries).toEqual(userCronQueries);
      });

      it('should have fields', function () {
        fields.push({
          dbField : 'ip.dst:port',
          exp     : 'ip.dst:port',
          help    : 'Destination IP:Destination Port'
        });

        expect(settingsCtrl.fields).toBeDefined();
        expect(settingsCtrl.fields).toEqual(fields);
      });

      it('should be able to switch settings tabs', function () {
        settingsCtrl.openView('general');
        expect(settingsCtrl.visibleTab).toEqual('general');

        settingsCtrl.openView('cron');
        expect(settingsCtrl.visibleTab).toEqual('cron');
      });

      it('should be able to dismiss message', function () {
        settingsCtrl.msg = 'super awesome message';
        settingsCtrl.msgType = 'danger';

        settingsCtrl.messageDone();

        expect(settingsCtrl.msg).toBeNull();
        expect(settingsCtrl.msgType).toBeNull();
      });

      /* general tab */
      describe('General Settings ->', function () {

        it('should update a user\'s settings', function () {
          let newSettings = {
            timezone      : 'gmt',
            detailFormat  : 'last',
            showTimestamps: 'last',
            sortColumn    : 'start',
            sortDirection : 'asc',
            spiGraph      : 'no',
            connSrcField  : 'a1',
            connDstField  : 'ip.dst:port',
            numPackets    : 'last'
          };

          expect(settingsCtrl.settings).toEqual(user.settings);

          settingsCtrl.settings = newSettings;

          $httpBackend.expectPOST('user/settings/update').respond(200, {
            text: 'SUCCESS!'
          });

          settingsCtrl.update();

          $httpBackend.flush();

          expect(settingsCtrl.msg).toEqual('SUCCESS!');
          expect(settingsCtrl.msgType).toEqual('success');
          expect(settingsCtrl.settings).toEqual(newSettings);
        });

        it('should be able to update the time', function () {
          let date = new Date();

          $httpBackend.expectPOST('user/settings/update').respond(200, {
            text: 'SUCCESS!'
          });

          settingsCtrl.updateTime();

          $httpBackend.flush();

          expect(settingsCtrl.dateFormat).toEqual('yyyy/MM/dd HH:mm:ss');

          // change the timezone and update time
          $httpBackend.expectPOST('user/settings/update').respond(200, {
            text: 'SUCCESS!'
          });

          settingsCtrl.settings.timezone = 'gmt';

          date = new Date();

          settingsCtrl.updateTime();

          $httpBackend.flush();

          expect(settingsCtrl.msg).toEqual('SUCCESS!');
          expect(settingsCtrl.msgType).toEqual('success');
          expect(settingsCtrl.dateFormat).toEqual('yyyy/MM/dd HH:mm:ss\'Z\'');
        });

        it('should be able to change dbField to exp', function () {
          let exp = settingsCtrl.formatField('all');

          expect(exp).toEqual('asn');
        });

        it('should be able to get a field given the dbField value', function () {
          let field = settingsCtrl.getField('all');

          expect(field).toEqual(fields[0]);
        });

      });

      /* view tab */
      describe('View Settings ->', function () {

        it('should create a view', function () {
          // should show error if no view name
          settingsCtrl.createView();

          expect(settingsCtrl.viewFormError).toEqual('No view name specified.');

          // should show error if no view expression
          settingsCtrl.newViewName = 'new view name';

          settingsCtrl.createView();

          expect(settingsCtrl.viewFormError).toEqual('No view expression specified.');

          // should execute creation if view name and expression exist
          settingsCtrl.newViewExpression = 'new view expression';

          $httpBackend.expectPOST('user/views/create',
             function (postData) {
               let jsonData = JSON.parse(postData);
               expect(jsonData.viewName).toEqual('new view name');
               expect(jsonData.expression).toEqual('new view expression');
               return true;
             }
          ).respond(200, { text: 'SUCCESS!' });

          settingsCtrl.createView();

          $httpBackend.flush();

          userViews['new view name'] = {
            name      : 'new view name',
            expression: 'new view expression'
          };

          expect(settingsCtrl.views).toEqual(userViews);
          expect(settingsCtrl.newViewName).toBeNull();
          expect(settingsCtrl.newViewExpression).toBeNull();
          expect(settingsCtrl.msg).toEqual('SUCCESS!');
          expect(settingsCtrl.msgType).toEqual('success');
        });

        it('should set a view as changed', function () {
          expect(settingsCtrl.views.viewy.changed).toBeFalsy();

          settingsCtrl.viewChanged('viewy');

          expect(settingsCtrl.views.viewy.changed).toBeTruthy();
        });

        it('should cancel a view change', function () {
          settingsCtrl.views.viewy.name = 'new viewy name';

          $httpBackend.expectGET('user/views').respond(200, userViews);

          settingsCtrl.cancelViewChange('viewy');

          $httpBackend.flush();

          expect(settingsCtrl.views.viewy).toEqual(userViews.viewy);
        });

        it('should update a view', function () {
          settingsCtrl.views.viewy.name = 'new viewy name';

          settingsCtrl.viewChanged('viewy');

          $httpBackend.expectPOST('user/views/update',
             function (postData) {
               let jsonData = JSON.parse(postData);
               expect(jsonData.key).toEqual('viewy');
               expect(jsonData.name).toEqual('new viewy name');
               expect(jsonData.expression).toEqual('expression');
               return true;
             }
          ).respond(200, {
            text  : 'SUCCESS!',
            views : {
              'new viewy name': {
                name      : 'new viewy name',
                expression: 'expression'
              }
            }
          });

          settingsCtrl.updateView('viewy');

          $httpBackend.flush();

          expect(settingsCtrl.views.viewy).not.toBeDefined();
          expect(settingsCtrl.views['new viewy name']).toEqual({
            name: 'new viewy name', expression: 'expression'
          });
        });

        it('should not update a view if it hasn\'t changed', function () {
          settingsCtrl.updateView('viewy');

          expect(settingsCtrl.views.viewy).toEqual(userViews.viewy);
          expect(settingsCtrl.msg).toEqual('This view has not changed');
          expect(settingsCtrl.msgType).toEqual('warning');
        });

        it('should not update a view if it doesn\'t exist', function () {
          settingsCtrl.viewChanged('viewy');
          settingsCtrl.updateView('view');

          expect(settingsCtrl.msg).toEqual('Could not find corresponding view');
          expect(settingsCtrl.msgType).toEqual('danger');
        });

        it('should delete a view', function () {
          $httpBackend.expectPOST('user/views/delete',
             function (postData) {
               let jsonData = JSON.parse(postData);
               expect(jsonData.view).toEqual('viewy');
               return true;
             }
          ).respond(200, { text: 'SUCCESS!' });

          settingsCtrl.deleteView('viewy');

          $httpBackend.flush();

          expect(settingsCtrl.views.viewy).not.toBeDefined();
          expect(settingsCtrl.msg).toEqual('SUCCESS!');
          expect(settingsCtrl.msgType).toEqual('success');
        });

      });

      /* cron query tab */
      describe('Cron Query Settings ->', function () {

        it('should create a cron query', function () {
          // should show error if no cron query name
          settingsCtrl.createCronQuery();

          expect(settingsCtrl.cronQueryFormError).toEqual('No cron query name specified.');

          // should show error if no cron query expression
          settingsCtrl.newCronQueryName = 'new cron query name';

          settingsCtrl.createCronQuery();

          expect(settingsCtrl.cronQueryFormError).toEqual('No cron query expression specified.');

          // should show error if no cron query tags
          settingsCtrl.newCronQueryExpression = 'new cron query expression';

          settingsCtrl.createCronQuery();

          expect(settingsCtrl.cronQueryFormError).toEqual('No cron query tags specified.');

          // should execute creation if view name and expression exist
          settingsCtrl.newCronQueryTags = 'tag1, tag2';

          $httpBackend.expectPOST('user/cron/create',
             function (postData) {
               let jsonData = JSON.parse(postData);
               expect(jsonData.enabled).toBeTruthy();
               expect(jsonData.name).toEqual('new cron query name');
               expect(jsonData.query).toEqual('new cron query expression');
               expect(jsonData.action).toEqual('tag');
               expect(jsonData.tags).toEqual('tag1, tag2');
               expect(jsonData.since).toEqual('0');
               return true;
             }
          ).respond(200, {
            key : 'newcronkey',
            text: 'SUCCESS!'
          });

          settingsCtrl.createCronQuery();

          $httpBackend.flush();

          userCronQueries.newcronkey = {
            enabled : true,
            name    : 'new cron query name',
            query   : 'new cron query expression',
            action  : 'tag',
            tags    : 'tag1, tag2',
            since   : '0',
            count   : 0
          };

          expect(settingsCtrl.cronQueries).toEqual(userCronQueries);
          expect(settingsCtrl.newCronQueryTags).toBeNull();
          expect(settingsCtrl.newCronQueryName).toBeNull();
          expect(settingsCtrl.newCronQueryExpression).toBeNull();
          expect(settingsCtrl.msg).toEqual('SUCCESS!');
          expect(settingsCtrl.msgType).toEqual('success');
        });

        it('should set a cron query as changed', function () {
          expect(settingsCtrl.cronQueries.key1.changed).toBeFalsy();

          settingsCtrl.cronQueryChanged('key1');

          expect(settingsCtrl.cronQueries.key1.changed).toBeTruthy();
        });

        it('should cancel a cron query change', function () {
          settingsCtrl.cronQueries.key1.name = 'new cron query name';

          $httpBackend.expectGET('user/cron').respond(200, userCronQueries);

          settingsCtrl.cancelCronQueryChange('key1');

          $httpBackend.flush();

          expect(settingsCtrl.cronQueries.key1).toEqual(userCronQueries.key1);
        });

        it('should update a cron query', function () {
          settingsCtrl.cronQueries.key1.name = 'new cron query name';

          settingsCtrl.cronQueryChanged('key1');

          $httpBackend.expectPOST('user/cron/update',
             function (postData) {
               let jsonData = JSON.parse(postData);
               expect(jsonData.key).toEqual('key1');
               expect(jsonData.enabled).toBeTruthy();
               expect(jsonData.name).toEqual('new cron query name');
               expect(jsonData.query).toEqual('expression');
               expect(jsonData.action).toEqual('tag');
               expect(jsonData.tags).toEqual('taggy');
               return true;
             }
          ).respond(200, {text: 'SUCCESS!'});

          settingsCtrl.updateCronQuery('key1');

          $httpBackend.flush();

          expect(settingsCtrl.cronQueries.key1).toBeDefined();
          expect(settingsCtrl.cronQueries.key1).toEqual({
            enabled : true,
            name    : 'new cron query name',
            query   : 'expression',
            tags    : 'taggy',
            action  : 'tag',
            lastRun : 1485197671,
            lpValue : 1485197121,
            count   : 0,
            creator : 'moloch',
            changed : false,
            key     : 'key1'
          });
        });

        it('should not update a cron query if it hasn\'t changed', function () {
          settingsCtrl.updateCronQuery('key1');

          expect(settingsCtrl.cronQueries.key1).toEqual(userCronQueries.key1);
          expect(settingsCtrl.msg).toEqual('This cron query has not changed');
          expect(settingsCtrl.msgType).toEqual('warning');
        });

        it('should not update a cron query if it doesn\'t exist', function () {
          settingsCtrl.cronQueryChanged('key1');
          settingsCtrl.updateCronQuery('key2');

          expect(settingsCtrl.msg).toEqual('Could not find corresponding cron query');
          expect(settingsCtrl.msgType).toEqual('danger');
        });

        it('should delete a cron query', function () {
          $httpBackend.expectPOST('user/cron/delete',
             function (postData) {
               let jsonData = JSON.parse(postData);
               expect(jsonData.key).toEqual('key1');
               return true;
             }
          ).respond(200, { text: 'SUCCESS!' });

          settingsCtrl.deleteCronQuery('key1');

          $httpBackend.flush();

          expect(settingsCtrl.cronQueries.key1).not.toBeDefined();
          expect(settingsCtrl.msg).toEqual('SUCCESS!');
          expect(settingsCtrl.msgType).toEqual('success');
        });

      });

      /* password tab */
      describe('Password Settings ->', function () {

        it('should require the user\'s current password', function () {
          settingsCtrl.changePassword();

          expect(settingsCtrl.changePasswordError).toEqual(
             'You must enter your current password');
        });

        it('should require a new password', function () {
          settingsCtrl.currentPassword = 'supersecretpassword';

          settingsCtrl.changePassword();

          expect(settingsCtrl.changePasswordError).toEqual(
             'You must enter a new password');
        });

        it('should require a confirmation of the new password', function () {
          settingsCtrl.currentPassword  = 'supersecretpassword';
          settingsCtrl.newPassword      = 'newsupersecretpassword';

          settingsCtrl.changePassword();

          expect(settingsCtrl.changePasswordError).toEqual(
             'You must confirm your new password');
        });

        it('should require the new and confirmation passwords to match', function () {
          settingsCtrl.currentPassword    = 'supersecretpassword';
          settingsCtrl.newPassword        = 'newsupersecretpassword';
          settingsCtrl.confirmNewPassword = 'XnewsupersecretpasswordX';

          settingsCtrl.changePassword();

          expect(settingsCtrl.changePasswordError).toEqual(
             'Your passwords don\'t match');
        });

        it('should require the new and confirmation passwords to match', function () {
          settingsCtrl.currentPassword    = 'supersecretpassword';
          settingsCtrl.newPassword        = 'newsupersecretpassword';
          settingsCtrl.confirmNewPassword = 'XnewsupersecretpasswordX';

          settingsCtrl.changePassword();

          expect(settingsCtrl.changePasswordError).toEqual(
             'Your passwords don\'t match');
        });

        it('should change a user\'s password', function () {
          settingsCtrl.currentPassword    = 'supersecretpassword';
          settingsCtrl.newPassword        = 'newsupersecretpassword';
          settingsCtrl.confirmNewPassword = 'newsupersecretpassword';

          $httpBackend.expectPOST('user/password/change',
             function (postData) {
               let jsonData = JSON.parse(postData);
               expect(jsonData.currentPassword).toEqual('supersecretpassword');
               expect(jsonData.newPassword).toEqual('newsupersecretpassword');
               return true;
             }
          ).respond(200, { text: 'SUCCESS!' });

          settingsCtrl.changePassword();

          $httpBackend.flush();

          expect(settingsCtrl.currentPassword).toBeNull();
          expect(settingsCtrl.newPassword).toBeNull();
          expect(settingsCtrl.confirmNewPassword).toBeNull();
          expect(settingsCtrl.msg).toEqual('SUCCESS!');
          expect(settingsCtrl.msgType).toEqual('success');
        });

      });

    });


    /* admin user tests ---------------------------------------------------- */
    describe('Admin editing another user ->', function() {

      // Initialize and a mock scope
      beforeEach(inject(function(
         _$httpBackend_,
         $componentController,
         $rootScope,
         $compile,
         $interval,
         $location,
         FieldService,
         ConfigService,
         UserService) {

        $httpBackend = _$httpBackend_;

        $httpBackend.expectGET('user/current').respond(200, user);

        $httpBackend.expectGET('molochclusters').respond(200, {});

        $httpBackend.expectGET('fields?array=true').respond(200, fields);

        $httpBackend.expectGET('user/settings?userId=anotheruserid')
          .respond(200, user.settings);

        $httpBackend.expectGET('user/views?userId=anotheruserid')
          .respond(200, userViews);

        $httpBackend.expectGET('user/cron?userId=anotheruserid')
          .respond(200, userCronQueries);

        scope = $rootScope.$new();

        settingsCtrl = $componentController('molochSettings', {
          $interval     : $interval,
          $location     : $location,
          $routeParams  : { userId: 'anotheruserid' },
          UserService   : UserService,
          FieldService  : FieldService,
          ConfigService : ConfigService
        });

        // initialize settings component controller
        settingsCtrl.$onInit();
        $httpBackend.flush();
      }));

      afterEach(function() {
        $httpBackend.verifyNoOutstandingExpectation();
        $httpBackend.verifyNoOutstandingRequest();
      });

      it('should exist and have dependencies', function() {
        expect(settingsCtrl).toBeDefined();
        expect(settingsCtrl.$interval).toBeDefined();
        expect(settingsCtrl.$location).toBeDefined();
        expect(settingsCtrl.$routeParams).toBeDefined();
        expect(settingsCtrl.UserService).toBeDefined();
        expect(settingsCtrl.FieldService).toBeDefined();
        expect(settingsCtrl.ConfigService).toBeDefined();
      });

      it('should have user\'s settings', function() {
        expect(settingsCtrl.settings).toBeDefined();
        expect(settingsCtrl.settings).toEqual(user.settings);
      });

      it('should have user\'s views', function () {
        expect(settingsCtrl.views).toBeDefined();
        expect(settingsCtrl.views).toEqual(userViews);
      });

      it('should have user\'s cron queries', function () {
        expect(settingsCtrl.cronQueries).toBeDefined();
        expect(settingsCtrl.cronQueries).toEqual(userCronQueries);
      });

      /* general tab */
      describe('General Settings ->', function () {

        it('should update a user\'s settings', function () {
          let newSettings = {
            timezone      : 'gmt',
            detailFormat  : 'last',
            showTimestamps: 'last',
            sortColumn    : 'start',
            sortDirection : 'asc',
            spiGraph      : 'no',
            connSrcField  : 'a1',
            connDstField  : 'ip.dst:port',
            numPackets    : 'last'
          };

          expect(settingsCtrl.settings).toEqual(user.settings);

          settingsCtrl.settings = newSettings;

          $httpBackend.expectPOST('user/settings/update?userId=anotheruserid')
            .respond(200, { text: 'SUCCESS!' });

          settingsCtrl.update();

          $httpBackend.flush();

          expect(settingsCtrl.msg).toEqual('SUCCESS!');
          expect(settingsCtrl.msgType).toEqual('success');
          expect(settingsCtrl.settings).toEqual(newSettings);
        });

        it('should be able to update the time', function () {
          let date = new Date();

          $httpBackend.expectPOST('user/settings/update?userId=anotheruserid')
            .respond(200, { text: 'SUCCESS!' });

          settingsCtrl.updateTime();

          $httpBackend.flush();

          expect(settingsCtrl.dateFormat).toEqual('yyyy/MM/dd HH:mm:ss');

          // change the timezone and update time
          $httpBackend.expectPOST('user/settings/update?userId=anotheruserid')
            .respond(200, { text: 'SUCCESS!' });

          settingsCtrl.settings.timezone = 'gmt';

          date = new Date();

          settingsCtrl.updateTime();

          $httpBackend.flush();

          expect(settingsCtrl.msg).toEqual('SUCCESS!');
          expect(settingsCtrl.msgType).toEqual('success');
          expect(settingsCtrl.dateFormat).toEqual('yyyy/MM/dd HH:mm:ss\'Z\'');
        });

      });

      /* view tab */
      describe('View Settings ->', function () {

        it('should create a view', function () {
          // should show error if no view name
          settingsCtrl.createView();

          expect(settingsCtrl.viewFormError).toEqual('No view name specified.');

          // should show error if no view expression
          settingsCtrl.newViewName = 'new view name';

          settingsCtrl.createView();

          expect(settingsCtrl.viewFormError).toEqual('No view expression specified.');

          // should execute creation if view name and expression exist
          settingsCtrl.newViewExpression = 'new view expression';

          $httpBackend.expectPOST('user/views/create?userId=anotheruserid',
             function (postData) {
               let jsonData = JSON.parse(postData);
               expect(jsonData.viewName).toEqual('new view name');
               expect(jsonData.expression).toEqual('new view expression');
               return true;
             }
          ).respond(200, { text: 'SUCCESS!' });

          settingsCtrl.createView();

          $httpBackend.flush();

          userViews['new view name'] = {
            name      : 'new view name',
            expression: 'new view expression'
          };

          expect(settingsCtrl.views).toEqual(userViews);
          expect(settingsCtrl.newViewName).toBeNull();
          expect(settingsCtrl.newViewExpression).toBeNull();
          expect(settingsCtrl.msg).toEqual('SUCCESS!');
          expect(settingsCtrl.msgType).toEqual('success');
        });

        it('should cancel a view change', function () {
          settingsCtrl.views.viewy.name = 'new viewy name';

          $httpBackend.expectGET('user/views?userId=anotheruserid')
            .respond(200, userViews);

          settingsCtrl.cancelViewChange('viewy');

          $httpBackend.flush();

          expect(settingsCtrl.views.viewy).toEqual(userViews.viewy);
        });

        it('should update a view', function () {
          settingsCtrl.views.viewy.name = 'new viewy name';

          settingsCtrl.viewChanged('viewy');

          $httpBackend.expectPOST('user/views/update?userId=anotheruserid',
             function (postData) {
               let jsonData = JSON.parse(postData);
               expect(jsonData.key).toEqual('viewy');
               expect(jsonData.name).toEqual('new viewy name');
               expect(jsonData.expression).toEqual('expression');
               return true;
             }
          ).respond(200, {
            text  : 'SUCCESS!',
            views : {
              'new viewy name': {
                name      : 'new viewy name',
                expression: 'expression'
              }
            }
          });

          settingsCtrl.updateView('viewy');

          $httpBackend.flush();

          expect(settingsCtrl.views.viewy).not.toBeDefined();
          expect(settingsCtrl.views['new viewy name']).toEqual({
            name: 'new viewy name', expression: 'expression'
          });
        });

        it('should delete a view', function () {
          $httpBackend.expectPOST('user/views/delete?userId=anotheruserid',
             function (postData) {
               let jsonData = JSON.parse(postData);
               expect(jsonData.view).toEqual('viewy');
               return true;
             }
          ).respond(200, {text: 'SUCCESS!'});

          settingsCtrl.deleteView('viewy');

          $httpBackend.flush();

          expect(settingsCtrl.views.viewy).not.toBeDefined();
          expect(settingsCtrl.msg).toEqual('SUCCESS!');
          expect(settingsCtrl.msgType).toEqual('success');
        });

      });

      /* cron query tab */
      describe('Cron Query Settings ->', function () {

        it('should create a cron query', function () {
          settingsCtrl.newCronQueryName       = 'new cron query name';
          settingsCtrl.newCronQueryExpression = 'new cron query expression';
          settingsCtrl.newCronQueryTags       = 'tag1, tag2';

          $httpBackend.expectPOST('user/cron/create?userId=anotheruserid',
             function (postData) {
               let jsonData = JSON.parse(postData);
               expect(jsonData.enabled).toBeTruthy();
               expect(jsonData.name).toEqual('new cron query name');
               expect(jsonData.query).toEqual('new cron query expression');
               expect(jsonData.action).toEqual('tag');
               expect(jsonData.tags).toEqual('tag1, tag2');
               expect(jsonData.since).toEqual('0');
               return true;
             }
          ).respond(200, {
            key : 'newcronkey',
            text: 'SUCCESS!'
          });

          settingsCtrl.createCronQuery();

          $httpBackend.flush();

          userCronQueries.newcronkey = {
            enabled : true,
            name    : 'new cron query name',
            query   : 'new cron query expression',
            action  : 'tag',
            tags    : 'tag1, tag2',
            since   : '0',
            count   : 0
          };

          expect(settingsCtrl.cronQueries).toEqual(userCronQueries);
          expect(settingsCtrl.newCronQueryTags).toBeNull();
          expect(settingsCtrl.newCronQueryName).toBeNull();
          expect(settingsCtrl.newCronQueryExpression).toBeNull();
          expect(settingsCtrl.msg).toEqual('SUCCESS!');
          expect(settingsCtrl.msgType).toEqual('success');
        });

        it('should cancel a cron query change', function () {
          settingsCtrl.cronQueries.key1.name = 'new cron query name';

          $httpBackend.expectGET('user/cron?userId=anotheruserid')
            .respond(200, userCronQueries);

          settingsCtrl.cancelCronQueryChange('key1');

          $httpBackend.flush();

          expect(settingsCtrl.cronQueries.key1).toEqual(userCronQueries.key1);
        });

        it('should update a cron query', function () {
          settingsCtrl.cronQueries.key1.name = 'new cron query name';

          settingsCtrl.cronQueryChanged('key1');

          $httpBackend.expectPOST('user/cron/update?userId=anotheruserid',
             function (postData) {
               let jsonData = JSON.parse(postData);
               expect(jsonData.key).toEqual('key1');
               expect(jsonData.enabled).toBeTruthy();
               expect(jsonData.name).toEqual('new cron query name');
               expect(jsonData.query).toEqual('expression');
               expect(jsonData.action).toEqual('tag');
               expect(jsonData.tags).toEqual('taggy');
               // expect(jsonData.since).toEqual('0');
               return true;
             }
          ).respond(200, {text: 'SUCCESS!'});

          settingsCtrl.updateCronQuery('key1');

          $httpBackend.flush();

          expect(settingsCtrl.cronQueries.key1).toBeDefined();
          expect(settingsCtrl.cronQueries.key1).toEqual({
            enabled : true,
            name    : 'new cron query name',
            query   : 'expression',
            tags    : 'taggy',
            action  : 'tag',
            lastRun : 1485197671,
            lpValue : 1485197121,
            count   : 0,
            creator : 'moloch',
            changed : false,
            key     : 'key1'
          });
        });

        it('should delete a cron query', function () {
          $httpBackend.expectPOST('user/cron/delete?userId=anotheruserid',
             function (postData) {
               let jsonData = JSON.parse(postData);
               expect(jsonData.key).toEqual('key1');
               return true;
             }
          ).respond(200, {text: 'SUCCESS!'});

          settingsCtrl.deleteCronQuery('key1');

          $httpBackend.flush();

          expect(settingsCtrl.cronQueries.key1).not.toBeDefined();
          expect(settingsCtrl.msg).toEqual('SUCCESS!');
          expect(settingsCtrl.msgType).toEqual('success');
        });

      });

      /* password tab */
      describe('Password Settings ->', function () {

        it('should change a user\'s password', function () {
          settingsCtrl.newPassword        = 'newsupersecretpassword';
          settingsCtrl.confirmNewPassword = 'newsupersecretpassword';

          $httpBackend.expectPOST('user/password/change?userId=anotheruserid',
             function (postData) {
               let jsonData = JSON.parse(postData);
               expect(jsonData.currentPassword).not.toBeDefined('supersecretpassword');
               expect(jsonData.newPassword).toEqual('newsupersecretpassword');
               return true;
             }
          ).respond(200, {text: 'SUCCESS!'});

          settingsCtrl.changePassword();

          $httpBackend.flush();

          expect(settingsCtrl.currentPassword).toBeNull();
          expect(settingsCtrl.newPassword).toBeNull();
          expect(settingsCtrl.confirmNewPassword).toBeNull();
          expect(settingsCtrl.msg).toEqual('SUCCESS!');
          expect(settingsCtrl.msgType).toEqual('success');
        });

      });

    });

    /* admin user tests ---------------------------------------------------- */
    describe('Unauthorized user editing another user ->', function() {

      let unauthorizedUser = {
        userId            : 'unauthorizeduser',
        enabled           : true,
        webEnabled        : true,
        emailSearch       : false,
        createEnabled     : false,
        removeEnabled     : false,
        headerAuthEnabled : false,
        settings          : {
          timezone        : 'local',
          detailFormat    : 'hex',
          showTimestamps  : 'no',
          sortColumn      : 'stop',
          sortDirection   : 'desc',
          spiGraph        : 'no',
          connSrcField    : 'p1',
          connDstField    : 'ip.dst',
          numPackets      : '50'
        }
      };

      // Initialize and a mock scope
      beforeEach(inject(function (
        _$httpBackend_,
        $componentController,
        $rootScope,
        $compile,
        $interval,
        $location,
        FieldService,
        ConfigService,
        UserService) {

        $httpBackend = _$httpBackend_;

        $httpBackend.expectGET('user/current').respond(200, unauthorizedUser);

        $httpBackend.expectGET('molochclusters').respond(200, {});

        $httpBackend.expectGET('fields?array=true').respond(200, fields);

        $httpBackend.expectGET('user/views').respond(200, userViews);

        $httpBackend.expectGET('user/cron').respond(200, userCronQueries);

        scope = $rootScope.$new();

        settingsCtrl = $componentController('molochSettings', {
          $interval     : $interval,
          $location     : $location,
          $routeParams  : {userId: 'anotheruserid'},
          UserService   : UserService,
          FieldService  : FieldService,
          ConfigService : ConfigService
        });

        // watch for calls in controller
        spyOn(settingsCtrl.$location, 'search').and.callThrough();

        // initialize settings component controller
        settingsCtrl.$onInit();
        $httpBackend.flush();
      }));

      afterEach(function () {
        $httpBackend.verifyNoOutstandingExpectation();
        $httpBackend.verifyNoOutstandingRequest();
      });

      it('should exist and have dependencies', function () {
        expect(settingsCtrl).toBeDefined();
        expect(settingsCtrl.$interval).toBeDefined();
        expect(settingsCtrl.$location).toBeDefined();
        expect(settingsCtrl.$routeParams).toBeDefined();
        expect(settingsCtrl.UserService).toBeDefined();
        expect(settingsCtrl.FieldService).toBeDefined();
        expect(settingsCtrl.ConfigService).toBeDefined();
      });

      it('should have removed the userId from the url', function() {
        // expect(settingsCtrl.$location.userId).not.toBeDefined();
        expect(settingsCtrl.$location.search).toHaveBeenCalled();
        expect(settingsCtrl.$location.search).toHaveBeenCalledWith('userId', null);
      });

      it('should have user\'s settings', function () {
        expect(settingsCtrl.settings).toBeDefined();
        expect(settingsCtrl.settings).toEqual(unauthorizedUser.settings);
      });

      it('should have user\'s views', function () {
        expect(settingsCtrl.views).toBeDefined();
        expect(settingsCtrl.views).toEqual(userViews);
      });

      it('should have user\'s cron queries', function () {
        expect(settingsCtrl.cronQueries).toBeDefined();
        expect(settingsCtrl.cronQueries).toEqual(userCronQueries);
      });

    });

  });

})();
