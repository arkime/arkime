(function() {

  'use strict';

  describe('Utilities ->', function() {

    // load the module
    beforeEach(angular.mock.module('moloch.util'));

    var $filter, $timeout, scope;

    beforeEach(inject(function($rootScope, _$filter_, _$timeout_) {
        scope     = $rootScope.$new();
        $filter   = _$filter_;
        $timeout  = _$timeout_;
    }));


    describe('Comma String Filter ->', function() {

      it('should add commas to an integer and return a string', function () {
        var int = 1;
        var result = $filter('commaString')(int);
        expect(result).toEqual('1');

        int = 100;
        result = $filter('commaString')(int);
        expect(result).toEqual('100');

        int = 1000;
        result = $filter('commaString')(int);
        expect(result).toEqual('1,000');

        int = 10000;
        result = $filter('commaString')(int);
        expect(result).toEqual('10,000');

        int = 100000;
        result = $filter('commaString')(int);
        expect(result).toEqual('100,000');

        int = 1000000;
        result = $filter('commaString')(int);
        expect(result).toEqual('1,000,000');
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
        var ip = 3232235777;
        var result  = $filter('extractIPString')(ip);
        expect(result).toEqual('192.168.1.1');
      });

    });


    describe('Extract IPv6 String Filter ->', function() {

      it('should return the appropriate ipv6 string', function() {
        var ipv6 = 'ff0200000000000000000001ff8295b5';
        var result  = $filter('extractIPv6String')(ipv6);
        expect(result).toEqual('ff02:0:0:0:0:1:ff82:95b5');
      });

    });


    describe('Readble Time Filter ->', function() {

      it('should return the appropriate readable time string', function() {
        var ms = 3600000;
        var result  = $filter('readableTime')(ms);
        expect(result).toEqual('01:00:00');

        ms = ms * 24;
        result = $filter('readableTime')(ms);
        expect(result).toEqual('1 day ');

        ms = ms + 3600000;
        result = $filter('readableTime')(ms);
        expect(result).toEqual('1 day 01:00:00');

        ms = 0;
        result = $filter('readableTime')(ms);
        expect(result).toEqual('0');
      });

    });


    describe('Convert To Number Directive ->', function() {

      var element, template, templateAsHtml;

      beforeEach(inject(function($compile) {
        scope.model = 2;

        var htmlString = '<select ng-model="model" convert-to-number>'+
          '<option value="1">One</option>' +
          '<option value="2">Two</option>' +
          '<option value="3">Three</option>' +
          '<option value="4">Four</option>' +
          '<option value="5">Five</option>' +
          '</select>';

        element   = angular.element(htmlString);
        template  = $compile(element)(scope);

        scope.$digest();

        templateAsHtml = template.html();
      }));

      it('should render html with ng-model', function() {
        expect(element.val()).toEqual('2');
        expect(templateAsHtml).toBeDefined();
        expect(element.find('option:checked')[0].text).toEqual('Two');
      });

      it('should apply changes to ng-model', function() {
        scope.model = 5;
        scope.$digest();

        expect(element.val()).toEqual('5');
        expect(templateAsHtml).toBeDefined();
        expect(element.find('option:checked')[0].text).toEqual('Five');
      });

    });


    describe('Epoch Date Directive ->', function() {

      var element, template, templateAsHtml;

      beforeEach(inject(function($compile) {
        scope.model = '2016/09/12 15:36:48';

        var htmlString = '<input ng-model="model" epoch-date />';

        element   = angular.element(htmlString);
        template  = $compile(element)(scope);

        scope.$digest();

        templateAsHtml = template.html();
      }));

      it('should render html with ng-model', function() {
        expect(element.val()).toEqual('2016/09/12 15:36:48');
        expect(templateAsHtml).toBeDefined();
      });

      it('should apply changes to ng-model', function() {
        scope.model = '2015/03/14 03:14:15';
        scope.$digest();
        expect(element.val()).toEqual('2015/03/14 03:14:15');
      });

    });


    describe('Caret Position Directive ->', function() {

      var element, template, templateAsHtml;

      beforeEach(inject(function($compile) {
        scope.model     = '';
        scope.position  = 0;

        var htmlString  = '<input ng-model="model" caret-pos="position" />';

        element   = angular.element(htmlString);
        template  = $compile(element)(scope);

        scope.$digest();

        templateAsHtml = template.html();
      }));

      it('should render html with caret-pos', function() {
        expect(scope.position).toEqual(0);
        expect(templateAsHtml).toBeDefined();

        element.val('some text');

        element.click();
        scope.$digest();

        expect(scope.position).toEqual(9);
      });

      it('should not exceed text length', function() {
        var text = 'text';

        element.val(text);

        element.click();
        scope.$digest();

        expect(scope.position).not.toBeGreaterThan(text.length);
        expect(scope.position).toEqual(text.length);
      });

    });


    describe('Focus Input Directive ->', function() {

      var element, template, templateAsHtml;

      beforeEach(inject(function($compile) {
        scope.focusMe  = true;

        var htmlString  = '<input focus-input="focusMe" />';

        element   = angular.element(htmlString);
        template  = $compile(element)(scope);

        spyOn(element[0], 'focus');

        scope.$digest();

        templateAsHtml = template.html();
      }));

      it('should render html and have focus', function() {
        expect(scope.focusMe).toBeTruthy();
        expect(templateAsHtml).toBeDefined();
        expect(element[0].focus).toHaveBeenCalled();
      });

      it('should re-focus', function() {
        scope.focusMe = false;
        scope.$digest();

        scope.focusMe = true;
        scope.$digest();
        expect(element[0].focus).toHaveBeenCalled();
      });

    });


    describe('Enter Click Directive ->', function() {

      var element, template, templateAsHtml;

      beforeEach(inject(function($compile) {
        scope.func = function() {};

        var htmlString  = '<input enter-click="func()" />';

        element   = angular.element(htmlString);
        template  = $compile(element)(scope);

        spyOn(scope, 'func');

        scope.$digest();

        templateAsHtml = template.html();
      }));

      it('should render html', function() {
        expect(templateAsHtml).toBeDefined();
      });

      it('should call function when enter is pressed', function() {
        var e     = jQuery.Event('keypress');
        e.which   = 13;
        e.keyCode = 13;

        element.trigger(e);

        expect(scope.func).toHaveBeenCalled();
      });

    });

  });

})();
