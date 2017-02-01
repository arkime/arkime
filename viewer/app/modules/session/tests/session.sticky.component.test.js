(function() {

  'use strict';

  let sessions = [{
    pa2     :0,
    p1      :10000,
    no      :'node',
    pa1     :1,
    p2      :2948,
    pr      :17,
    lp      :0,
    fp      :0,
    a1      :16843009,
    a2      :33686018,
    pa      :1,
    db1     :437,
    db2     :0,
    by      :445,
    by2     :0,
    by1     :445,
    db      :437,
    index   :'sessions-',
    id      :'sessionid',
    expanded: true
  },{
    pa2     :0,
    p1      :10000,
    no      :'node',
    pa1     :1,
    p2      :2948,
    pr      :17,
    lp      :0,
    fp      :0,
    a1      :16843009,
    a2      :33686018,
    pa      :1,
    db1     :437,
    db2     :0,
    by      :445,
    by2     :0,
    by1     :445,
    db      :437,
    index   :'sessions-',
    id      :'sessionid2',
    expanded: true
  }];

  describe('Session Sticky Component ->', function() {

    // load the module and enable debug info (to access isolateScope)
    beforeEach(function() {
      angular.mock.module('moloch', function (_$compileProvider_) {
        _$compileProvider_.debugInfoEnabled(true);
      });
    });

    let scope, element, template, isolateScope, templateAsHtml, clones;
    let $location, $timeout;

    // Initialize and a mock scope
    beforeEach(inject(function(
       $compile,
       _$location_,
       _$timeout_,
       $rootScope) {

      $location = _$location_;
      $timeout  = _$timeout_;

      scope   = $rootScope.$new();

      clones  = JSON.parse(JSON.stringify(sessions));

      scope.sessions  = sessions;

      let htmlString  = '<sticky-sessions sessions="sessions"></sticky-sessions>';

      element       = angular.element(htmlString);
      template      = $compile(element)(scope);
      isolateScope  = template.isolateScope();

      scope.$digest();

      templateAsHtml  = template.html();

      spyOn($location, 'hash').and.callThrough();
    }));

    afterEach(function() {
      // cleanup
      scope.sessions = sessions = clones;
    });

    it('should render html with session info', function() {
      expect(templateAsHtml).toBeDefined();
      expect(scope.sessions).toBeDefined();
    });

    it('should scroll to a session', function() {
      let id = 'sessionid';

      isolateScope.scrollTo({ preventDefault:function() {} }, id);

      expect($location.hash).toHaveBeenCalled();
      expect($location.hash).toHaveBeenCalledWith();


      expect($location.hash).toHaveBeenCalled();
      expect($location.hash).toHaveBeenCalledWith('session' + id);

      expect($location.hash).toHaveBeenCalled();
      expect($location.hash).toHaveBeenCalledWith('session' + id);

      expect($location.hash.calls.count()).toBe(4);
    });

    it('should close an expanded session detail', function() {
      let session = sessions[0];

      expect(session.expanded).toBeTruthy();

      isolateScope.closeSessionDetail(session);

      expect(session.expanded).toBeFalsy();
      expect(isolateScope.sessions.length).toEqual(1);
    });

    it('should close sticky session panel automatically', function() {
      let session   = sessions[0];
      let session2  = sessions[1];

      isolateScope.state.open = true;

      expect(isolateScope.state.open).toBeTruthy();
      expect(session.expanded).toBeTruthy();
      expect(session2.expanded).toBeTruthy();

      isolateScope.closeSessionDetail(session);

      expect(session.expanded).toBeFalsy();
      expect(isolateScope.sessions.length).toEqual(1);

      isolateScope.closeSessionDetail(session2);

      expect(session2.expanded).toBeFalsy();
      expect(isolateScope.sessions.length).toEqual(0);

      expect(isolateScope.state.open).toBeFalsy();
    });

    it('should close all expanded session details', function() {
      isolateScope.state.open = true;

      expect(isolateScope.state.open).toBeTruthy();
      expect(sessions[0].expanded).toBeTruthy();
      expect(sessions[1].expanded).toBeTruthy();

      isolateScope.closeAll();

      expect(sessions[0].expanded).toBeFalsy();
      expect(sessions[1].expanded).toBeFalsy();
      expect(isolateScope.sessions.length).toEqual(0);
      expect(isolateScope.state.open).toBeFalsy();
    });

    it('should watch for changes in the sessions array', function() {
      isolateScope.state.open = true;

      expect(isolateScope.state.open).toBeTruthy();

      scope.sessions.push({ id:'sessionid3', expanded:true, fp:0, lp:0 });

      scope.$digest();

      $timeout.flush(100);
      expect(element.hasClass('bounce')).toBeTruthy();

      $timeout.flush(1000);
      expect(element.hasClass('bounce')).toBeFalsy();

      scope.sessions = [];

      scope.$digest();

      expect(isolateScope.state.open).toBeFalsy();
    });

  });

})();
