(function() {

  'use strict';

  describe('Utilities ->', function() {

    // load the module
    beforeEach(angular.mock.module('moloch.util'));

    var $filter, scope, templateAsHtml;

    beforeEach(inject(function($rootScope, _$filter_) {
        scope   = $rootScope.$new();
        $filter = _$filter_;
    }));

    describe('Convert To Number ->', function() {

      var element, template;

      // Initialize and a mock scope
      beforeEach(inject(function($compile) {
        scope.model = 2;

        var htmlString = '<select ng-model="model" convert-to-number>'+
          '<option value="1">One</option>' +
          '<option value="2">Two</option>' +
          '<option value="3">Three</option>' +
          '<option value="4">Four</option>' +
          '<option value="5">Five</option>' +
          '</select>';

        element = angular.element(htmlString);
        template = $compile(element)(scope);

        scope.$digest();

        templateAsHtml = template.html();
      }));

      it('should render html with ng-model', function() {
        expect(scope.model).toEqual(2);
        expect(templateAsHtml).toBeDefined();
        expect(element.find('option:checked')[0].text).toEqual('Two');
      });

      it('should apply changes to ng-model', function() {
        scope.model = 5;
        scope.$digest();

        expect(scope.model).toEqual(5);
        expect(templateAsHtml).toBeDefined();
        expect(element.find('option:checked')[0].text).toEqual('Five');
      });

    });

    describe('Protocol Filter ->', function() {

      it('should return the appropriate protocol strings', function () {
        var protocolInt = 1;
        var result = $filter('protocol')(protocolInt);
        expect(result).toEqual('icmp');

        protocolInt = 6;
        result = $filter('protocol')(protocolInt);
        expect(result).toEqual('tcp');

        protocolInt = 17;
        result = $filter('protocol')(protocolInt);
        expect(result).toEqual('udp');

        protocolInt = 47;
        result = $filter('protocol')(protocolInt);
        expect(result).toEqual('gre');

        protocolInt = 58;
        result = $filter('protocol')(protocolInt);
        expect(result).toEqual('icmp6');

        protocolInt = 'default';
        result = $filter('protocol')(protocolInt);
        expect(result).toEqual('default');
      });

    });

    describe('Extract IP String Filter ->', function() {

      it('should return the appropriate ip string', function() {
        var session = { a1:3232235777 };
        var result  = $filter('extractIPString')(session);
        expect(result).toEqual('192.168.1.1');
      });

      it('should return the appropriate ipv6 string', function() {
        var session = { 'tipv61-term':'ff0200000000000000000001ff8295b5' };
        var result  = $filter('extractIPString')(session);
        expect(result).toEqual('ff02:0:0:0:0:1:ff82:95b5');
      });

    });

  });

})();
