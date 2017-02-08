(function() {

  'use strict';

  describe('Toast Directive ->', function() {

    // load the module
    beforeEach(angular.mock.module('moloch'));
    beforeEach(angular.mock.module('directives.toast'));

    let scope, toast, templateAsHtml, $timeout;
    let message = 'Awesome Toast!';

    // Initialize and a mock scope
    beforeEach(inject(function(
       $componentController,
       _$timeout_,
       $rootScope,
       $compile) {

      $timeout = _$timeout_;

      scope = $rootScope.$new();

      let element   = angular.element(`<toast message="\'${message}\'"></toast>`);
      let template  = $compile(element)(scope);
      let done      = () => { message = undefined; };

      toast = $componentController('toast', {
        $timeout: $timeout
      }, {
        message: message,
        done   : done
      });

      spyOn(toast, '$onChanges').and.callThrough();

      scope.$digest();
      templateAsHtml = template.html();

      // initialize component controller
      toast.$onInit();
    }));

    it('should exist and have dependencies', function() {
      expect(toast).toBeDefined();
      expect(toast.$timeout).toBeDefined();
    });

    it('should have smart defaults', function() {
      expect(toast.type).toEqual('info');
    });

    it('should render html with message', function() {
      expect(toast.visible).toBeTruthy();
      expect(toast.message).toBeDefined();
      expect(templateAsHtml).toBeDefined();
      expect(templateAsHtml).toContain(toast.message);
    });

    it('should dismiss the toast', function() {
      toast.dismiss();

      expect(toast.visible).toBeFalsy();
      expect(toast.message).not.toBeDefined();
    });

    it('should dismiss the toast after 5 seconds', function() {
      $timeout.flush(5000);

      expect(toast.visible).toBeFalsy();
      expect(toast.message).not.toBeDefined();
    });

  });

})();
