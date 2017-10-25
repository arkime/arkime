(function() {

  'use strict';

  describe('Expression Typeahead Component ->', function() {

    // load modules
    beforeEach(angular.mock.module('moloch'));
    beforeEach(angular.mock.module('directives.search'));
    beforeEach(angular.mock.module('moloch.util'));

    let scope, rootScope, typeahead, templateAsHtml, $httpBackend;

    // Initialize and a mock scope
    beforeEach(inject(function(
      _$httpBackend_,
      $componentController,
      $rootScope,
      $location,
      $timeout,
      $compile,
      FieldService) {

      $httpBackend = _$httpBackend_;

      $httpBackend.expectGET('fields').respond({});

      rootScope = $rootScope;
      scope     = $rootScope.$new();

      scope.expression = { value: '' };

      let htmlString = '<expression-typeahead query="expression"></expression-typeahead>';

      let element   = angular.element(htmlString);
      let template  = $compile(element)(scope);

      scope.$digest();

      templateAsHtml = template.html();

      typeahead = $componentController('expressionTypeahead',
        {
          $scope      : scope,
          $timeout    : $timeout,
          $location   : $location,
          $rootScope  : $rootScope,
          $routeParams: {},
          FieldService: FieldService
        },
        { query: scope.expression });

      // typeahead.$onInit();

      $httpBackend.flush();
    }));

    afterEach(function() {
      $httpBackend.verifyNoOutstandingExpectation();
      $httpBackend.verifyNoOutstandingRequest();
    });

    it('should exist and have dependencies', function() {
      expect(typeahead).toBeDefined();
      expect(typeahead.$routeParams).toBeDefined();
      expect(typeahead.FieldService).toBeDefined();
    });

    it('should render html', function() {
      expect(templateAsHtml).toBeDefined();
    });

    it('should start with smart default', function() {
      expect(typeahead.query.value).toEqual('');
    });

    it('should be able to clear query', function() {
      rootScope.expression = 'asdf';

      typeahead.clear();
      expect(rootScope.expression).toBe(null);
    });

    it('should be able to add field to blank query', function() {
      rootScope.expression = 'co';
      typeahead.caretPos = 2;

      typeahead.changeExpression();
      typeahead.addToQuery({exp:'country'});

      expect(rootScope.expression).toEqual('country ');
    });

    it('should be able to add operator to end of query', function() {
      typeahead.$onInit();
      typeahead.$timeout.flush();

      rootScope.expression = 'country =';
      typeahead.caretPos = 9;

      typeahead.changeExpression();
      typeahead.addToQuery('==');

      expect(rootScope.expression).toEqual('country == ');
    });

    it('should be able to add value to end of query', function() {
      typeahead.$onInit();
      typeahead.$timeout.flush();

      rootScope.expression = 'country == U';
      typeahead.caretPos = 12;

      typeahead.changeExpression();
      typeahead.addToQuery({exp:'USA'});

      expect(rootScope.expression).toEqual('country == USA ');
    });

    it('should be able to change field in query', function() {
      rootScope.expression = 'country == USA';
      typeahead.caretPos = 7;

      typeahead.changeExpression();
      typeahead.addToQuery({exp:'country.src'});

      expect(rootScope.expression).toEqual('country.src == USA ');
    });

    it('should be able to change query and preserve spaces', function() {
      rootScope.expression = 'asn.dst == "value with spaces in it"';
      typeahead.caretPos = 7;

      typeahead.changeExpression();
      typeahead.addToQuery({exp:'asn.dst'});

      expect(rootScope.expression).toEqual('asn.dst == "value with spaces in it" ');
    });

    it('should be able to change operation in query', function() {
      typeahead.$onInit();
      typeahead.$timeout.flush();

      rootScope.expression = 'country = USA';
      typeahead.caretPos = 9;

      typeahead.changeExpression();
      typeahead.addToQuery('!=');

      expect(rootScope.expression).toEqual('country != USA ');
    });

    it('should be able to change value in query', function() {
      typeahead.$onInit();
      typeahead.$timeout.flush();

      rootScope.expression = 'country != U';
      typeahead.caretPos = 11;

      typeahead.changeExpression();
      typeahead.addToQuery({exp:'AUS'});

      expect(rootScope.expression).toEqual('country != AUS ');
    });

    it('should be able to add a value with a "-" in it', function() {
      let expression = 'rootId == "rootId-1234567890"';
      rootScope.$broadcast('add:to:typeahead', { expression: expression });

      expect(rootScope.expression).toEqual(expression);
    });

    it('should be able to add EXISTS! to end of query', function() {
      typeahead.$onInit();
      typeahead.$timeout.flush();

      rootScope.expression = 'country == EXIS';
      typeahead.caretPos = 12;

      typeahead.changeExpression();
      typeahead.addToQuery('EXISTS!');

      expect(rootScope.expression).toEqual('country == EXISTS! ');
    });

    it('EXISTS! should not split up just because "!" is a special character', function() {
      typeahead.$onInit();
      typeahead.$timeout.flush();

      rootScope.expression = 'protocols == EXISTS! && tls';
      typeahead.caretPos = 27;

      typeahead.changeExpression();
      typeahead.addToQuery('tls.sessionid');

      expect(rootScope.expression).toEqual('protocols == EXISTS! && tls.sessionid ');
    });

  });

})();
