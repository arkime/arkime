(function() {

  'use strict';

  describe('Colheader Directive ->', function() {

    // load the module
    beforeEach(module('moloch'));

    // load templates
    beforeEach(module('templates'));

    var scope, colheader, templateAsHtml;

    // Initialize and a mock scope
    beforeEach(inject(function($componentController, $rootScope, $compile) {
      scope = $rootScope.$new();

      scope.query = {
        sortElement : 'fp',
        sortOrder   : 'asc'
      };

      var htmlString = '<colheader col-name="\'Start\'" col-id="\'fp\'" ' +
        'element="query.sortElement" ' +
        'order="query.sortOrder"></colheader>';

      var element   = angular.element(htmlString);
      var template  = $compile(element)(scope);

      scope.$digest();

      templateAsHtml = template.html();

      colheader = $componentController('colheader', {
        $scope: scope
      });

      // spy on emit event
      spyOn(scope, '$emit').and.callThrough();
    }));

    it('should exist and have dependencies', function() {
      expect(colheader).toBeDefined();
      expect(colheader.$scope).toBeDefined();
    });

    it('should render html with input values', function() {
      expect(scope.query.sortElement).toEqual('fp');
      expect(scope.query.sortOrder).toEqual('asc');
      expect(templateAsHtml).toBeDefined();
      expect(templateAsHtml).toContain('fp');
      expect(templateAsHtml).toContain('asc');
    });

    it('should emit a "change:sort" event and toggle sort order', function() {
      colheader.order   = scope.query.sortOrder;
      colheader.element = scope.query.sortElement;
      colheader.colId   = scope.query.sortElement;

      colheader.sort();

      expect(scope.$emit).toHaveBeenCalled();
      expect(scope.$emit).toHaveBeenCalledWith('change:sort', {
        sortOrder   : 'desc',
        sortElement : 'fp'
      });
    });

    it('should emit a "change:sort" event with updated element and order', function() {
      colheader.order   = 'desc';
      colheader.element = 'lp';

      colheader.sort();

      expect(scope.$emit).toHaveBeenCalled();
      expect(scope.$emit).toHaveBeenCalledWith('change:sort', {
        sortOrder   : colheader.order,
        sortElement : colheader.element
      });
    });

    it('should not emit a "change:sort" event if sort element is not set', function() {
      colheader.order   = 'desc';
      colheader.element = null;

      colheader.sort();

      expect(scope.$emit).not.toHaveBeenCalled();
    });

  });

})();
