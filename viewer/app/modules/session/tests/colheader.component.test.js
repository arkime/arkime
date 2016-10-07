(function() {

  'use strict';

  describe('Colheader Directive ->', function() {

    // load the module
    beforeEach(angular.mock.module('moloch'));

    var scope, colheader, templateAsHtml;

    // Initialize and a mock scope
    beforeEach(inject(function($componentController, $rootScope, $compile) {
      scope = $rootScope.$new();

      var htmlString = '<colheader col-name="\'Start\'" col-id="\'fp\'" ' +
        'sorts="sorts"></colheader>';

      var element   = angular.element(htmlString);
      var template  = $compile(element)(scope);

      scope.$digest();

      templateAsHtml = template.html();

      colheader = $componentController('colheader',
        { $scope:scope },
        { sorts:[], colName:'Start', colId:'fp' });

      // spy on emit event
      spyOn(scope, '$emit').and.callThrough();
    }));

    it('should exist and have dependencies', function() {
      expect(colheader).toBeDefined();
      expect(colheader.$scope).toBeDefined();
    });

    it('should render html with input values', function() {
      expect(templateAsHtml).toBeDefined();
      expect(templateAsHtml).toContain('Start');
      expect(templateAsHtml).toContain('fp');
      expect(colheader.sorts).toEqual([]);
      expect(colheader.colName).toEqual('Start');
      expect(colheader.colId).toEqual('fp');
    });

    it('should emit a "change:sort" event', function() {
      colheader.colId = 'lp'; // change column

      var event = { type:'click', shiftKey:false }; // simulate click event

      colheader.sortBy(event);

      var sorts = [{ element:'lp', order:'asc' }];

      expect(scope.$emit).toHaveBeenCalled();
      expect(scope.$emit).toHaveBeenCalledWith('change:sort',
        { sorts: sorts });
    });

    it('should not sort an unsortable column', function() {
      colheader.sorts = null; // no sorts = unsortable column
      colheader.colId = 'lp';

      var event = { type:'click', shiftKey:false }; // simulate click event

      colheader.sortBy(event);

      expect(scope.$emit).not.toHaveBeenCalled();
    });

    it('should emit a "change:sort" event and toggle order on click', function() {
      // already sorted by lp column
      colheader.sorts = [{ element:'lp', order:'asc' }];
      colheader.colId = 'lp'; // same column

      var event = { type:'click', shiftKey:false }; // simulate click event

      colheader.sortBy(event);

      var sorts = [{ element:'lp', order:'desc' }];

      expect(scope.$emit).toHaveBeenCalled();
      expect(scope.$emit).toHaveBeenCalledWith('change:sort',
        { sorts: sorts });
    });

    it('should emit a "change:sort" event with updated sort object on click', function() {
      colheader.sorts = [{ element:'lp', order:'asc' }]; // old sort order
      colheader.colId = 'fp'; // different column

      var event = { type:'click', shiftKey:false }; // simulate click event

      colheader.sortBy(event);

      var sorts = [{ element:'fp', order:'asc' }];

      expect(scope.$emit).toHaveBeenCalled();
      expect(scope.$emit).toHaveBeenCalledWith('change:sort',
        { sorts: sorts });
    });

    it('should emit a "change:sort" event and add new sort object on click', function() {
      colheader.sorts = [{ element:'lp', order:'asc' }]; // old sort order
      colheader.colId = 'fp'; // different column

      var event = { type:'click', shiftKey:true }; // simulate shift click event

      colheader.sortBy(event);

      var sorts = [
        { element:'lp', order:'asc' },
        { element:'fp', order:'asc' }
      ];

      expect(scope.$emit).toHaveBeenCalled();
      expect(scope.$emit).toHaveBeenCalledWith('change:sort',
        { sorts: sorts });
    });

    it('should emit a "change:sort" event and toggle order on shift click', function() {
      // asc -> desc -> removed
      colheader.sorts = [{ element:'lp', order:'asc' }]; // old sort order
      colheader.colId = 'lp'; // same column

      var event = { type:'click', shiftKey:true }; // simulate shift click event

      colheader.sortBy(event);

      var sorts = [{ element:'lp', order:'desc' }];

      expect(scope.$emit).toHaveBeenCalled();
      expect(scope.$emit).toHaveBeenCalledWith('change:sort',
        { sorts: sorts });

      colheader.sortBy(event);

      expect(scope.$emit).toHaveBeenCalled();
      expect(scope.$emit).toHaveBeenCalledWith('change:sort',
        { sorts: [] });
    });

  });

})();
