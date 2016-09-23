(function() {

  'use strict';

  /**
   * @class SessionDetailController
   * @classdesc Interacts with session details
   */
  class SessionDetailController {

    /* setup --------------------------------------------------------------- */
    /**
     * Initialize global variables for this controller
     * @param $scope  Angular application model object
     * @param $sce    Angular strict contextual escaping service
     *
     * @ngInject
     */
    constructor($scope, $sce, $location, SessionService, ConfigService, FieldService) {
      this.$scope = $scope;
      this.$sce   = $sce;
      this.$scope.$location = $location;
      this.SessionService = SessionService;
      this.ConfigService  = ConfigService;
      this.FieldService   = FieldService;
    }

    /* Callback when component is mounted and ready */
    $onInit() {
      this.$scope.loading   = true;
      this.$scope.error     = false;

      this.SessionService.getDetail(this.$scope.sessionId, this.$scope.sessionNode)
        .then((response) => {
          this.$scope.loading     = false;
          this.$scope.detailHtml  = this.$sce.trustAsHtml(response.data);
          this.$scope.watchClickableValues();
        })
        .catch((error) => {
          this.$scope.loading = false;
          this.$scope.error   = error;
        });

      this.ConfigService.getMolochRightClick()
        .then((response) => {
          this.$scope.molochRightClick = response;
        })
        .catch((error) => {
          // TODO: moloch right click error
        });

      this.FieldService.get()
        .then((response) => {
          this.$scope.molochFields = response;
        })
        .catch((error) => {
          // TODO: moloch right click error
        });
    }


    /* exposed functions --------------------------------------------------- */


  }

  SessionDetailController.$inject = ['$scope','$sce','$location',
    'SessionService','ConfigService','FieldService'];


  angular.module('moloch')
    .directive('sessionDetail', ['$timeout', function($timeout) {
      return {
        template  : require('html!../templates/session.detail.html'),
        controller: SessionDetailController,
        scope     : { sessionId: '@', sessionNode: '@' },
        link      : function SessionDetailLink(scope, element, attrs, ctrl) {

          function molochExprClick(event) {
            event.preventDefault();

            // if user is selecting text, don't add expression to query
            if (window.getSelection().toString() !== '') { return; }

            var target = event.target;

            var field  = target.getAttribute('molochfield');
            console.log(target);

            // if click target doesn't have a molochfield attribute,
            while (!field) { // it may be in the parent or parent's parent
              target = target.parentNode;
              field = target.getAttribute('molochfield');
            }
            console.log(target);

            var val = target.getAttribute('molochvalue');

            var expr = target.getAttribute('molochexpr');
            if (!expr) { expr = '=='; }

            var fullExpression = `${field} ${expr} ${val}`;

            scope.$emit('add:to:search', { expression: fullExpression });

            // close open dropdown menu
            var menu = $(target.parentNode.querySelector('.dropdown-menu'));
            if (menu.length === 0) {
              menu = $(target.parentNode.parentNode.querySelector('.dropdown-menu'))
            }
            if (menu.length > 0 && menu.hasClass('moloch-menu')) {
              // hide it
              menu.removeClass('moloch-menu');
              return;
            }
          }

          function timeClick(event) {
            event.preventDefault();

            var start = event.target.getAttribute('molochstart');
            var stop  = event.target.getAttribute('molochstop');

            var result = {};
            if (start) { result.start = start; }
            if (stop)  { result.stop  = stop; }

            if (result.start || result.stop) {
              scope.$emit('change:time', result);
            }
          }

          var menuItems;
          function buildMenu(items, value, target) {
            var html = `<ul class="dropdown-menu moloch-menu">`;

            for (var k in items) {
              var item = items[k];
              html += `<li class="cursor-pointer"
                molochfield="${item.info.field}"
                molochexpr="${item.exp}"
                molochvalue="${value}">
                <a class="menu-item">${item.name}</a>
                </li>`;
            }

            html += '</ul>';

            target.insertAdjacentHTML('afterend', html);

            menuItems = target.parentNode.querySelectorAll('.menu-item');
            for (var i = 0, len = menuItems.length; i < len; ++i) {
              menuItems[i].addEventListener('click', molochExprClick);
            }
          }



          // TODO: CLEAN THIS UP!
          function safeStr(str) {
            if (str === undefined) { return ''; }

            if (Array.isArray(str)) { return str.map(safeStr); }

            return str.replace(/&/g,'&amp;').replace(/</g,'&lt;')
              .replace(/>/g,'&gt;').replace(/\"/g,'&quot;')
              .replace(/\'/g, '&#39;').replace(/\//g, '&#47;');
          }

          function rightClickInfo(target) {
            var molochfield = target.getAttribute('molochfield');
            if (!molochfield) {
              console.log('No molochfield', target);
              return { category:[] };
            }

            var field = scope.molochFields[molochfield];
            if (!field) {
              console.log('Unknown field', molochfield);
              return { category:[] };
            }

            if (Array.isArray(field.category)) {
              return {field: molochfield, category:field.category, info: field};
            } else {
              return {field: molochfield, category:[field.category], info: field};
            }
          }

          function rightClickValue(target) {
            var value = target.getAttribute('molochvalue');
            if (!value) {
              console.log('No value', target);
              return '';
            }

            return value;
          }

          function contextMenuClick(e) {
            var target = e.target;
            if (!e.target.getAttribute('molochfield')) {
              target = e.target.parentNode;
            }

            // if the menu already exists, don't recompute items in menu
            var existingMenu = $(target.parentNode.querySelector('.dropdown-menu'));
            if (existingMenu.length && existingMenu.hasClass('moloch-menu')) {
              // hide it
              existingMenu.removeClass('moloch-menu');
              return;
            } else if (existingMenu.length) { // show it
              existingMenu.addClass('moloch-menu');
              return;
            }

            var info = rightClickInfo(target);
            var text = rightClickValue(target);
            var url = text.indexOf('?') === -1 ? text :
              text.substring(0, text.indexOf('?'));
            var host = url;
            var pos = url.indexOf('//');
            if (pos >= 0) { host = url.substring(pos+2); }
            pos = host.indexOf('/');
            if (pos >= 0) { host = host.substring(0, pos); }
            pos = host.indexOf(':');
            if (pos >= 0) { host = host.substring(0, pos); }

            var items = {
              and: {name: '<b>and</b> ' + safeStr(url), exp: '==', info: info},
              andnot: {name: '<b>and not</b> ' + safeStr(url), exp: '!=', info: info}
            };

            // Extract the date/startTime/stopTime url params so we can use them
            // in a substitution
            var urlParams = scope.$location.search();
            var dateparams, isostart, isostop;
            if (urlParams.startTime && urlParams.stopTime) {
              dateparams = `startTime=${urlParams.startTime}&stopTime=${urlParams.stopTime}`;
              isostart = new Date(parseInt(urlParams.startTime) * 1000);
              isostop  = new Date(parseInt(urlParams.stopTime) * 1000);
            }
            else {
              dateparams = `date=${urlParams.date}`;
              isostart = new Date();
              isostop  = new Date();
              isostart.setHours(isostart.getHours() - parseInt(urlParams.date));
            }

            for (var key in scope.molochRightClick) {
              var rc = scope.molochRightClick[key];
              if ((!rc.category || info.category.indexOf(rc.category) === -1) &&
                  (!rc.fields || rc.fields.indexOf(info.field) === -1)) {
                continue;
              }

              var result = scope.molochRightClick[key].url
                .replace('%EXPRESSION%', encodeURIComponent(urlParams.expression))
                .replace('%DATE%', dateparams)
                .replace('%ISOSTART%', isostart.toISOString())
                .replace('%ISOSTOP%', isostop.toISOString())
                .replace('%FIELD%', info.field)
                .replace('%TEXT%', text)
                .replace('%UCTEXT%', text.toUpperCase())
                .replace('%HOST%', host)
                .replace('%URL%', encodeURIComponent('http:' + url));

              var nameDisplay = '<b>' + (scope.molochRightClick[key].name || key) + '</b> %URL%';
              if (rc.category === 'host') {
                nameDisplay = '<b>' + (scope.molochRightClick[key].name || key) + '</b> %HOST%';
              }

              var name = (scope.molochRightClick[key].nameDisplay || nameDisplay)
                .replace('%FIELD%', info.field)
                .replace('%TEXT%', text)
                .replace('%HOST%', host)
                .replace('%URL%', url);

              if (rc.regex) {
                if (!rc.cregex) {
                  rc.cregex = new RegExp(rc.regex);
                }
                var matches = text.match(rc.cregex);
                if (matches && matches[1]) {
                  result = result.replace('%REGEX%', matches[1]);
                } else {
                  continue;
                }
              }

              items[key] = {name: name, url: result};
            }

            console.log(items);
            buildMenu(items, text, target);
          }

          /**
           * Determine clickable values in html from server
           * Add click events to add values to the search query
           */
          var clickableValues, clickableStart, clickableStop, molochMenus;
          scope.watchClickableValues = function() {
            $timeout(function() { // wait until session detail is rendered
              var i, len;

              clickableValues = element[0].querySelectorAll('.clickable[molochfield]');
              for (i = 0, len = clickableValues.length; i < len; ++i) {
                clickableValues[i].addEventListener('click', molochExprClick);
              }

              clickableStart = element[0].querySelector('[molochstart]');
              clickableStart.addEventListener('click', timeClick);

              clickableStop = element[0].querySelector('[molochstop]');
              clickableStop.addEventListener('click', timeClick);

              molochMenus = element[0].querySelectorAll('[molochmenu]');
              for (i = 0, len = molochMenus.length; i < len; ++i) {
                molochMenus[i].addEventListener('click', contextMenuClick);
              }
            });
          };

          scope.$on('$destroy', function() {
            // remove event listeners to prevent memory leaks
            clickableStop.removeEventListener('click', timeClick);
            clickableStart.removeEventListener('click', timeClick);

            if (clickableValues) {
              for (var i = 0, len = clickableValues.length; i < len; ++i) {
                clickableValues[i].removeEventListener('click', molochExprClick);
              }
            }

            if (molochMenus) {
              for (var i = 0, len = molochMenus.length; i < len; ++i) {
                molochMenus[i].removeEventListener('click', contextMenuClick);
              }
            }

            if (menuItems) {
              for (var i = 0, len = menuItems.length; i < len; ++i) {
                menuItems[i].removeEventListener('click', molochExprClick);
              }
            }
          });

        },
      };
    }]);

})();
