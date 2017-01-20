(function() {

  'use strict';

  describe('Utilities ->', function() {

    // load the module
    beforeEach(angular.mock.module('moloch.util'));

    let $filter, $timeout, scope;

    beforeEach(inject(function($rootScope, _$filter_, _$timeout_) {
        scope     = $rootScope.$new();
        $filter   = _$filter_;
        $timeout  = _$timeout_;
    }));


    describe('Comma String Filter ->', function() {

      it('should add commas to an integer and return a string', function () {
        let int = 1;
        let result = $filter('commaString')(int);
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
        let protocolInt = 1;
        let result = $filter('protocol')(protocolInt);
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
        let ip = 3232235777;
        let result  = $filter('extractIPString')(ip);
        expect(result).toEqual('192.168.1.1');
      });

    });


    describe('Extract IPv6 String Filter ->', function() {

      it('should return the appropriate ipv6 string', function() {
        let ipv6 = 'ff0200000000000000000001ff8295b5';
        let result  = $filter('extractIPv6String')(ipv6);
        expect(result).toEqual('ff02:0:0:0:0:1:ff82:95b5');
      });

    });


    describe('Readble Time Filter ->', function() {

      it('should return the appropriate readable time string', function() {
        let ms = 3600000;
        let result = $filter('readableTime')(ms);
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


    describe('Field Filter ->', function() {

      it('should filter an array of fields by the search term', function() {
        let items = [
          { friendlyName: 'item name' },
          { friendlyName: 'different' },
          { friendlyName: 'another' }
        ];

        let searchTerm = 'item';
        let result = $filter('fieldFilter')(items, searchTerm);

        expect(result).toEqual([items[0]]);

        searchTerm = 'diff';
        result = $filter('fieldFilter')(items, searchTerm);

        expect(result).toEqual([items[1]]);
      });

      it('should return empty array if not items match the search term', function() {
        let items = [
          { friendlyName: 'item name' },
          { friendlyName: 'different' },
          { friendlyName: 'another' }
        ];

        let searchTerm = 'asdf';
        let result = $filter('fieldFilter')(items, searchTerm);

        expect(result).toEqual([]);
      });

    });


    describe('Capitalize Filter ->', function() {

      it('should return the string with the first letter capitalized', function() {
        let str     = 'not capitalized string';
        let result  = $filter('capitalize')(str);
        expect(result).toEqual('Not capitalized string');
      });

    });


    describe('Lowercase Filter ->', function() {

      it('should return the string with the all letters lowercased', function() {
        let str     = 'CAPITALIZED string HERE!';
        let result  = $filter('lowercase')(str);
        expect(result).toEqual('capitalized string here!');
      });

    });


    describe('Min Filter ->', function() {

      it('should return the minimum number in an array of numbers', function() {
        let array   = [ 932, 652, 754, 1, 73, 4, 652];
        let result  = $filter('min')(array);
        expect(result).toEqual(1);
      });

      it('should return the input if the input is not an array', function() {
        let notArray  = 'x';
        let result    = $filter('min')(notArray);
        expect(result).toEqual(notArray);

        notArray  = 4;
        result    = $filter('min')(notArray);
        expect(result).toEqual(notArray);
      });

    });


    describe('Timezone Date Filter ->', function() {

      it('should return an integer date in the requested timezone', function() {
        let time   = 18000;
        let result = $filter('timezone-date')(time, 'local');
        expect(result).toEqual(18000000);

        result = $filter('timezone-date')(time, 'gmt');
        expect(result).toEqual(36000000);
      });

    });


    describe('Convert To Number Directive ->', function() {

      let element, template, templateAsHtml;

      beforeEach(inject(function($compile) {
        scope.model = 2;

        let htmlString = '<select ng-model="model" convert-to-number>'+
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

      let element, template, templateAsHtml, compile;

      beforeEach(inject(function($compile) {
        compile = $compile;
      }));

      it('should render html with ng-model', function() {
        scope.model = '2016/09/12 03:36:48';

        let htmlString = '<input ng-model="model" epoch-date />';

        element   = angular.element(htmlString);
        template  = compile(element)(scope);

        scope.$digest();

        templateAsHtml = template.html();

        expect(element.val()).toEqual('1473665808000');
        expect(templateAsHtml).toBeDefined();
      });

      it('should apply changes to ng-model', function() {
        scope.model = '2016/09/12 15:36:48';

        let htmlString = '<input ng-model="model" epoch-date />';

        element   = angular.element(htmlString);
        template  = compile(element)(scope);

        scope.$digest();

        templateAsHtml = template.html();

        scope.model = '2015/03/14 03:14:15';
        scope.$digest();
        expect(element.val()).toEqual('1426317255000');
      });

      it('should apply gmt to time', function() {
        scope.model = '2016/09/12 03:36:48';

        let htmlString = `<input ng-model="model" epoch-date="gmt" />`;

        element   = angular.element(htmlString);
        template  = compile(element)(scope);

        scope.$digest();

        templateAsHtml = template.html();

        expect(element.val()).toEqual('1473680208000');
        expect(element.attr('epoch-date')).toEqual('gmt');
        expect(templateAsHtml).toBeDefined();
      });

    });


    describe('Caret Position Directive ->', function() {

      let element, template, templateAsHtml;

      beforeEach(inject(function($compile) {
        scope.model     = '';
        scope.position  = 0;

        let htmlString  = '<input ng-model="model" caret-pos="position" />';

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
        let text = 'text';

        element.val(text);

        element.click();
        scope.$digest();

        expect(scope.position).not.toBeGreaterThan(text.length);
        expect(scope.position).toEqual(text.length);
      });

    });


    describe('Focus Input Directive ->', function() {

      let element, template, templateAsHtml;

      beforeEach(inject(function($compile) {
        scope.focusMe  = true;

        let htmlString  = '<input focus-input="focusMe" />';

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

      let element, template, templateAsHtml;

      beforeEach(inject(function($compile) {
        scope.func = function() {};

        let htmlString  = '<input enter-click="func()" />';

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
        let e     = jQuery.Event('keypress');
        e.which   = 13;
        e.keyCode = 13;

        element.trigger(e);

        expect(scope.func).toHaveBeenCalled();
      });

    });

  });

})();
