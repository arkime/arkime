(function() {

  'use strict';
  
  let userSettings = {
    timezone      : 'local',
    detailFormat  : 'last',
    showTimestamps: 'last',
    sortColumn    : 'start',
    sortDirection : 'asc',
    spiGraph      : 'no',
    connSrcField  : 'a1',
    connDstField  : 'ip.dst:port',
    numPackets    : 'last'
  };

  describe('Settings Component ->', function() {

    // load modules
    beforeEach(angular.mock.module('moloch'));

    let scope, rootScope, timeout, settingsCtrl, templateAsHtml, $httpBackend;

    // Initialize and a mock scope
    beforeEach(inject(function(
       _$httpBackend_,
       $componentController,
       $rootScope,
       $compile,
       $timeout,
       $interval,
       FieldService,
       ConfigService,
       UserService) {

      timeout = $timeout;

      $httpBackend = _$httpBackend_;

      $httpBackend.expectGET('user/settings').respond(200, userSettings);

      $httpBackend.expectGET('user/views').respond(200, {});

      $httpBackend.expectGET('user/cron').respond(200, {});

      $httpBackend.expectGET('molochclusters').respond(200, {});

      $httpBackend.expectGET('fields?array=true').respond(200, []);

      rootScope = $rootScope;

      scope = $rootScope.$new();
      let htmlString = '<moloch-settings></moloch-settings>';

      let element   = angular.element(htmlString);
      let template  = $compile(element)(scope);

      scope.$digest();

      templateAsHtml = template.html();

      settingsCtrl = $componentController('molochSettings', {
        $interval     : $interval,
        UserService   : UserService,
        FieldService  : FieldService,
        ConfigService : ConfigService
      }, {});

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
      expect(settingsCtrl.UserService).toBeDefined();
      expect(settingsCtrl.FieldService).toBeDefined();
      expect(settingsCtrl.ConfigService).toBeDefined();
    });

    it('should render html', function() {
      expect(templateAsHtml).toBeDefined();
    });

    it('should be able to switch settings tabs', function() {
      settingsCtrl.openView('general');
      expect(settingsCtrl.view).toEqual('general');

      settingsCtrl.openView('cron');
      expect(settingsCtrl.view).toEqual('cron');
    });

    it('should be able to dismiss message', function() {
      settingsCtrl.msg = 'super awesome message';
      settingsCtrl.msgType = 'danger';

      settingsCtrl.messageDone();

      expect(settingsCtrl.msg).toBeNull();
      expect(settingsCtrl.msgType).toBeNull();
    });


    describe('General Settings ->', function() {

      it('should update a user\'s settings', function() {
        $httpBackend.expectPOST('user/settings/update').respond(200, {
          text: 'SUCCESS!'
        });

        settingsCtrl.settings = userSettings;

        settingsCtrl.update();

        $httpBackend.flush();

        expect(settingsCtrl.msg).toEqual('SUCCESS!');
        expect(settingsCtrl.msgType).toEqual('success');
        expect(settingsCtrl.settings).toEqual(userSettings);
      });

    });

  });

})();
