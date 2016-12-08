(function() {

  'use strict';

  let session = {
    pa2   : 0,
    p1    : 10000,
    no    : 'demo',
    pa1   : 1,
    p2    : 2948,
    pr    : 17,
    lp    : 1055289981,
    fp    : 1055289978,
    a1    : 16843009,
    a2    : 33686018,
    pa    : 1,
    db1   : 437,
    db2   : 0,
    by    : 445,
    by2   : 0,
    by1   : 445,
    db    : 437,
    index : 'sessions-',
    id    : '--RzB4CrWwqlMZIHRa0CzjUYn'
  };

  let col = {
    dbField     : 'fp',
    exp         : 'starttime',
    friendlyName: 'Start Time',
    group       : 'general',
    help        : 'Session Start Time',
    type        : 'seconds'
  };

  describe('Session Field Directive ->', function() {

    // load the module
    beforeEach(angular.mock.module('moloch'));

    let scope, sessionField, templateAsHtml;

    // Initialize and a mock scope
    beforeEach(inject(function($componentController, $rootScope, $compile) {
      scope = $rootScope.$new();

      let htmlString = '<session-field column="col" session="session"></session-field>';

      let element   = angular.element(htmlString);
      let template  = $compile(element)(scope);

      scope.$digest();

      templateAsHtml = template.html();

      sessionField = $componentController('sessionField', {}, {
        expr    : 'starttime',
        value   : session.fp,
        session : session,
        column  : col,
        parse   : true
      });
    }));

    it('should render html with session data', function() {
      expect(sessionField).toBeDefined();
      expect(templateAsHtml).toBeDefined();
      expect(sessionField.expr).toBeDefined();
      expect(sessionField.parse).toBeDefined();
      expect(sessionField.value).toBeDefined();
      expect(sessionField.column).toBeDefined();
      expect(sessionField.session).toBeDefined();
    });

  });

})();
