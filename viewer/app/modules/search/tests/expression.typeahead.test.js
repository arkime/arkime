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
      typeahead.query.value = 'asdf';
      expect(typeahead.query.value).toEqual('asdf');

      typeahead.clear();
      expect(typeahead.query.value).toBe(null);
    });

    it('should be able to add field to blank query', function() {
      typeahead.query.value = 'co';
      typeahead.caretPos = 2;

      typeahead.changeExpression();
      typeahead.addToQuery({exp:'country'});

      expect(typeahead.query.value).toEqual('country ');
    });

    it('should be able to add operator to end of query', function() {
      typeahead.$onInit();
      typeahead.$timeout.flush();

      typeahead.query.value = 'country =';
      typeahead.caretPos = 9;

      typeahead.changeExpression();
      typeahead.addToQuery('==');

      expect(typeahead.query.value).toEqual('country == ');
    });

    it('should be able to add value to end of query', function() {
      typeahead.$onInit();
      typeahead.$timeout.flush();

      typeahead.query.value = 'country == U';
      typeahead.caretPos = 12;

      typeahead.changeExpression();
      typeahead.addToQuery({exp:'USA'});

      expect(typeahead.query.value).toEqual('country == USA ');
    });

    it('should be able to change field in query', function() {
      typeahead.query.value = 'country == USA';
      typeahead.caretPos = 7;

      typeahead.changeExpression();
      typeahead.addToQuery({exp:'country.src'});

      expect(typeahead.query.value).toEqual('country.src == USA ');
    });

    it('should be able to change query and preserve spaces', function() {
      typeahead.query.value = 'asn.dst == "value with spaces in it"';
      typeahead.caretPos = 7;

      typeahead.changeExpression();
      typeahead.addToQuery({exp:'asn.dst'});

      expect(typeahead.query.value).toEqual('asn.dst == "value with spaces in it" ');
    });

    it('should be able to change operation in query', function() {
      typeahead.$onInit();
      typeahead.$timeout.flush();

      typeahead.query.value = 'country = USA';
      typeahead.caretPos = 9;

      typeahead.changeExpression();
      typeahead.addToQuery('!=');

      expect(typeahead.query.value).toEqual('country != USA ');
    });

    it('should be able to change value in query', function() {
      typeahead.$onInit();
      typeahead.$timeout.flush();

      typeahead.query.value = 'country != U';
      typeahead.caretPos = 11;

      typeahead.changeExpression();
      typeahead.addToQuery({exp:'AUS'});

      expect(typeahead.query.value).toEqual('country != AUS ');
    });

    it('should be able to add a value with a "-" in it', function() {
      let expression = 'rootId == "rootId-1234567890"';
      rootScope.$broadcast('add:to:typeahead', { expression: expression });

      expect(typeahead.query.value).toEqual(expression);
    });

    it('should be able to add EXISTS! to end of query', function() {
      typeahead.$onInit();
      typeahead.$timeout.flush();

      typeahead.query.value = 'country == EXIS';
      typeahead.caretPos = 12;

      typeahead.changeExpression();
      typeahead.addToQuery('EXISTS!');

      expect(typeahead.query.value).toEqual('country == EXISTS! ');
    });

  });

})();
