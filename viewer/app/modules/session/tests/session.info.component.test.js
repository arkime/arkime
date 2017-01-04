(function() {

  'use strict';

  let session = {
    pa2   :0,
    p1    :10000,
    no    :'demo',
    pa1   :1,
    p2    :2948,
    pr    :17,
    lp    :0,
    fp    :0,
    a1    :16843009,
    a2    :33686018,
    pa    :1,
    db1   :437,
    db2   :0,
    by    :445,
    by2   :0,
    by1   :445,
    db    :437,
    index :'sessions-',
    id    :'--RzB4CrWwqlMZIHRa0CzjUYn'
  };

  let field = {
    dbField   : 'info',
    exp       : 'info',
    children  : [{
      dbField : 'us',
      exp     : 'http.uri',
      type    : 'textField'
    }]
  };

  describe('Session Info Directive ->', function() {

    // load the module
    beforeEach(angular.mock.module('moloch'));

    let scope, sessionInfo, templateAsHtml;

    // Initialize and a mock scope
    beforeEach(inject(function($componentController, $rootScope, $compile) {
      scope = $rootScope.$new();

      let htmlString  = '<session-info></session-graph>';
      let element     = angular.element(htmlString);
      let template    = $compile(element)(scope);

      scope.$digest();

      templateAsHtml = template.html();

      sessionInfo = $componentController('sessionInfo', null, {
        session:session, field:field
      });

      sessionInfo.$onInit();
    }));

    it('should render html with session data', function() {
      expect(sessionInfo).toBeDefined();
      expect(templateAsHtml).toBeDefined();
      expect(sessionInfo.field).toBeDefined();
      expect(sessionInfo.session).toBeDefined();
    });

    it('should be able to show/hide more information', function() {
      expect(sessionInfo.showAll).toBeFalsy();

      sessionInfo.toggleShowAll();
      expect(sessionInfo.showAll).toBeTruthy();

      sessionInfo.toggleShowAll();
      expect(sessionInfo.showAll).toBeFalsy();
    });

  });

})();
