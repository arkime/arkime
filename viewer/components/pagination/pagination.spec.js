(function() {

  'use strict';

  describe('Pagination Directive ->', function() {

    // load the module
    beforeEach(module('directives.pagination'));

    var scope, pagination, $httpBackend;

    // Initialize and a mock scope
    beforeEach(inject(function($componentController, $rootScope) {
      scope = $rootScope.$new();

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
