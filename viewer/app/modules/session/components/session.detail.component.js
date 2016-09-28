(function() {

  'use strict';

  var options = require('html!../templates/session.detail.options.html');

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
      this.$scope         = $scope;
      this.$sce           = $sce;
      this.$location      = $location;
      this.SessionService = SessionService;
      this.ConfigService  = ConfigService;
      this.FieldService   = FieldService;
    }

    /* Callback when component is mounted and ready */
    $onInit() {
      this.$scope.loading = true;
      this.$scope.error   = false;
      this.$scope.params  = {
        base  : 'hex',
        line  : false,
        image : false,
        gzip  : false,
        ts    : false,
        decode: {}
      };

      if (localStorage) { // display browser saved options
        if (localStorage['moloch-ts']) {
          this.$scope.params.ts = (localStorage['moloch-ts'] === 'true');
        }
        if (localStorage['moloch-base']) {
          this.$scope.params.base = localStorage['moloch-base'];
        }
        if (localStorage['moloch-line']) {
          this.$scope.params.line = (localStorage['moloch-line'] === 'true');
        }
        if (localStorage['moloch-gzip']) {
          this.$scope.params.gzip = (localStorage['moloch-gzip'] === 'true');
        }
        if (localStorage['moloch-image']) {
          this.$scope.params.image = (localStorage['moloch-image'] === 'true');
        }
      }

      this.getDetailData(this.$scope.params);

      this.ConfigService.getMolochClickables()
        .then((response) => {
          this.$scope.molochClickables = response;
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
    /**
     *
     */
    getDetailData() {
      if (localStorage) { // update browser saved options
        localStorage['moloch-ts']     = this.$scope.params.ts;
        localStorage['moloch-base']   = this.$scope.params.base;
        localStorage['moloch-line']   = this.$scope.params.line;
        localStorage['moloch-gzip']   = this.$scope.params.gzip;
        localStorage['moloch-image']  = this.$scope.params.image;
      }

      this.SessionService.getDetail(this.$scope.sessionId,
        this.$scope.sessionNode, this.$scope.params)
        .then((response) => {
          this.$scope.loading     = false;
          this.$scope.detailHtml  = this.$sce.trustAsHtml(response.data);
          this.$scope.watchClickableValues();
        })
        .catch((error) => {
          this.$scope.loading = false;
          this.$scope.error   = error;
        });
    }

  }

  SessionDetailController.$inject = ['$scope','$sce','$location',
    'SessionService','ConfigService','FieldService'];


  angular.module('moloch')
    .directive('sessionDetail', ['$timeout', '$filter', '$compile',
    function($timeout, $filter, $compile) {
      return {
        template    : require('html!../templates/session.detail.html'),
        controller  : SessionDetailController,
        controllerAs: '$ctrl',
        scope       : { sessionId: '@', sessionNode: '@' },
        link        : function SessionDetailLink(scope, element, attrs, ctrl) {

          function molochExprClick(event) {
            event.preventDefault();

            // if user is selecting text, don't add expression to query
            if (window.getSelection().toString() !== '') { return; }

            var target = event.target;

            var field  = target.getAttribute('molochfield');

            // if click target doesn't have a molochfield attribute,
            while (!field) { // it may be in the parent or parent's parent
              target = target.parentNode;
              field = target.getAttribute('molochfield');
            }

            var val = target.getAttribute('molochvalue');

            var expr = target.getAttribute('molochexpr');
            if (!expr) { expr = '=='; }

            var fullExpression = `${field} ${expr} ${val}`;

            scope.$emit('add:to:search', { expression: fullExpression });

            // close open dropdown menu
            var menu = $(target.parentNode.querySelector('.dropdown-menu'));
            if (menu.length === 0) {
              menu = $(target.parentNode.parentNode.querySelector('.dropdown-menu'));
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
                molochvalue='${value}'>
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

          function showMoreItems(event) {
            $(event.target).hide();
            $(event.target).prev().show();
          }

          // function radioClick(event) {
          //   // get the value of the selected radio btn
          //   var val = event.target.getAttribute('value');
          //   if (val) { ctrl.getDetailData({ base:val }); }
          // }



          // TODO: CLEAN THIS UP!
          function safeStr(str) {
            if (str === undefined) { return ''; }

            if (Array.isArray(str)) { return str.map(safeStr); }

            return str.replace(/&/g,'&amp;').replace(/</g,'&lt;')
              .replace(/>/g,'&gt;').replace(/\"/g,'&quot;')
              .replace(/\'/g, '&#39;').replace(/\//g, '&#47;');
          }

          function getClickInfo(target) {
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

          function getClickValue(target) {
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

            var info = getClickInfo(target);
            var text = getClickValue(target);
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
            var urlParams = ctrl.$location.search();
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

            for (var key in scope.molochClickables) {
              var rc = scope.molochClickables[key];
              if ((!rc.category || info.category.indexOf(rc.category) === -1) &&
                  (!rc.fields || rc.fields.indexOf(info.field) === -1)) {
                continue;
              }

              var result = scope.molochClickables[key].url
                .replace('%EXPRESSION%', encodeURIComponent(urlParams.expression))
                .replace('%DATE%', dateparams)
                .replace('%ISOSTART%', isostart.toISOString())
                .replace('%ISOSTOP%', isostop.toISOString())
                .replace('%FIELD%', info.field)
                .replace('%TEXT%', text)
                .replace('%UCTEXT%', text.toUpperCase())
                .replace('%HOST%', host)
                .replace('%URL%', encodeURIComponent('http:' + url));

              var nameDisplay = '<b>' + (scope.molochClickables[key].name || key) + '</b> %URL%';
              if (rc.category === 'host') {
                nameDisplay = '<b>' + (scope.molochClickables[key].name || key) + '</b> %HOST%';
              }

              var name = (scope.molochClickables[key].nameDisplay || nameDisplay)
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

            buildMenu(items, text, target);
          }

          /**
           * Determine clickable values in html from server
           * Add click events to add values to the search query
           */
          var clickableValues, clickableTimes, molochMenus, showMore, radios;

          scope.watchClickableValues = function() {
            $timeout(function() { // wait until session detail is rendered
              // display packet options
              var optionsEl = element.find('.packet-options');
              var content = $compile(options)(scope);
              optionsEl.replaceWith(content);

              var i, len, time;

              clickableValues = element[0].querySelectorAll('.moloch-clickable[molochfield]');
              for (i = 0, len = clickableValues.length; i < len; ++i) {
                clickableValues[i].addEventListener('click', molochExprClick);
              }

              clickableTimes = element[0].querySelectorAll('.format-seconds');
              for (i = 0, len = clickableTimes.length; i < len; ++i) {
                var timeEl = clickableTimes[i];
                time = $filter('date')(timeEl.innerHTML * 1000, 'yyyy/MM/dd HH:mm:ss');
                timeEl.innerHTML = time;

                if (timeEl.getAttribute('molochvalue')) {
                  timeEl.setAttribute('molochvalue', "\"" + time + "\"");
                }

                if (timeEl.getAttribute('molochstart') ||
                    timeEl.getAttribute('molochstop')) {
                  timeEl.addEventListener('click', timeClick);
                }
              }

              molochMenus = element[0].querySelectorAll('[molochmenu]');
              for (i = 0, len = molochMenus.length; i < len; ++i) {
                molochMenus[i].addEventListener('click', contextMenuClick);
              }

              showMore = element[0].querySelectorAll('.showMoreItems');
              for (i = 0, len = showMore.length; i < len; ++i) {
                showMore[i].addEventListener('click', showMoreItems);
              }

              // radios = element[0].querySelectorAll('input[type=radio]');
              // for (i = 0, len = radios.length; i < len; ++i) {
              //   radios[i].addEventListener('click', radioClick);
              // }
            });
          };

          scope.$on('$destroy', function() {
            // remove event listeners to prevent memory leaks
            var i, len;

            if (clickableTimes) {
              for (i = 0, len = clickableTimes.length; i < len; ++i) {
                clickableTimes[i].removeEventListener('click', timeClick);
              }
            }

            if (clickableValues) {
              for (i = 0, len = clickableValues.length; i < len; ++i) {
                clickableValues[i].removeEventListener('click', molochExprClick);
              }
            }

            if (molochMenus) {
              for (i = 0, len = molochMenus.length; i < len; ++i) {
                molochMenus[i].removeEventListener('click', contextMenuClick);
              }
            }

            if (menuItems) {
              for (i = 0, len = menuItems.length; i < len; ++i) {
                menuItems[i].removeEventListener('click', molochExprClick);
              }
            }

            // if (radios) {
            //   for (i = 0, len = radios.length; i < len; ++i) {
            //     radios[i].removeEventListener('click', radioClick);
            //   }
            // }
          });

        },
      };
    }]);

})();
