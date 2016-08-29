(function() {

  'use strict';

  describe('Pagination Directive ->', function() {

    // load the module
    beforeEach(module('directives.pagination'));

    // load templates
    beforeEach(module('templates'));

    var scope, pagination, templateAsHtml;

    // Initialize and a mock scope
    beforeEach(inject(function($componentController, $rootScope, $compile) {
      scope = $rootScope.$new();

      scope.query = {
        length: 10,
        start : 1
      };
      scope.sessions = {
        recordsTotal    : 9999,
        recordsFiltered : 1000
      };
      scope.currentPage = 1;

      var htmlString = '<moloch-pagination length="query.length" '+
        'records-total="sessions.recordsTotal" ' +
        'records-filtered="sessions.recordsFiltered" ' +
        'current-page="currentPage" ' +
        'start="query.start"></moloch-pagination>';

      var element = angular.element(htmlString);
      var template = $compile(element)(scope);

      scope.$digest();

      templateAsHtml = template.html();

      pagination = $componentController('molochPagination', {
        $scope: scope
      });

      // spy on emit event
      spyOn(scope, '$emit').and.callThrough();

      // initialize pagination component controller
      pagination.$onInit();
    }));

    it('should exist and have dependencies', function() {
      expect(pagination).toBeDefined();
      expect(pagination.$scope).toBeDefined();
    });

    it('should render html with input values', function() {
      expect(scope.query.length).toEqual(10);
      expect(scope.query.start).toEqual(1);
      expect(scope.sessions.recordsTotal).toEqual(9999);
      expect(scope.sessions.recordsFiltered).toEqual(1000);
      expect(templateAsHtml).toBeDefined();
      expect(templateAsHtml).toContain('1,000');
      expect(templateAsHtml).toContain('9,999');
    });

    it('should have smart defaults', function() {
      expect(pagination.start).toEqual(0);
      expect(pagination.length).toEqual(50);
      expect(pagination.currentPage).toEqual(1);
    });

    it('should emit a "change:pagination" event', function() {
      pagination.length       = 10;
      pagination.currentPage  = 2;

      pagination.change();

      var start = (pagination.currentPage - 1) * pagination.length;

      expect(pagination.start).not.toEqual(0);
      expect(pagination.start).toEqual(start);
      expect(scope.$emit).toHaveBeenCalled();
      expect(scope.$emit).toHaveBeenCalledWith('change:pagination', {
        start       : start,
        length      : pagination.length,
        currentPage : pagination.currentPage
      });
    });

  });

})();
