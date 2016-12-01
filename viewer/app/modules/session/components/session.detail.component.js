(function() {

  'use strict';

  var optionsHTML = require('html!../templates/session.detail.options.html');

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
    constructor($scope, $sce, SessionService, ConfigService, FieldService) {
      this.$scope         = $scope;
      this.$sce           = $sce;
      this.SessionService = SessionService;
      this.ConfigService  = ConfigService;
      this.FieldService   = FieldService;
    }

    /* Callback when component is mounted and ready */
    $onInit() {
      this.loading = true;
      this.error   = false;

      // default session detail parameters
      // add to $scope so session.detail.options can use it
      this.$scope.params = {
        base  : 'hex',
        line  : false,
        image : false,
        gzip  : false,
        ts    : false,
        decode: {}
      };

      if (localStorage) { // display browser saved options
        if (localStorage['moloch-base']) {
          this.$scope.params.base = localStorage['moloch-base'];
        }
        if (localStorage['moloch-ts']) {
          this.$scope.params.ts = JSON.parse(localStorage['moloch-ts']);
        }
        if (localStorage['moloch-line']) {
          this.$scope.params.line = JSON.parse(localStorage['moloch-line']);
        }
        if (localStorage['moloch-gzip']) {
          this.$scope.params.gzip = JSON.parse(localStorage['moloch-gzip']);
        }
        if (localStorage['moloch-image']) {
          this.$scope.params.image = JSON.parse(localStorage['moloch-image']);
        }
      }

      this.getDetailData(this.$scope.params);

      this.ConfigService.getMolochClickables()
        .then((response) => {
          this.$scope.molochClickables = response;
        });

      this.FieldService.get()
        .then((response) => {
          this.$scope.molochFields = response;
        });

      /* LISTEN! */
      this.$scope.$on('open:form:container', (event, args) => {
        this.$scope.displayFormContainer(args);
      });

      this.$scope.$on('close:form:container', (event, args) => {
        console.log(args);
        if (args.ids) { // find the sessions affected
          for (let i = 0, len = args.ids.length; i < len; ++i) {
            this.$scope.hideFormContainer();

            if (this.$scope.session.id === args.ids[i]) {
              if (args && args.reloadData) {
                this.getDetailData(this.$scope.params);
              }

              if (args && args.message) {
                this.$scope.displayFormContainer(args);
              }
            }
          }
        }
      });
    }


    /* exposed functions --------------------------------------------------- */
    /**
     * Gets the session detail from the server
     */
    getDetailData() {
      if (localStorage) { // update browser saved options
        localStorage['moloch-base']   = this.$scope.params.base;
        localStorage['moloch-line']   = this.$scope.params.line;
        localStorage['moloch-gzip']   = this.$scope.params.gzip;
        localStorage['moloch-image']  = this.$scope.params.image;
      }

      this.SessionService.getDetail(this.$scope.session.id,
        this.$scope.session.no, this.$scope.params)
        .then((response) => {
          this.loading = false;
          this.$scope.detailHtml = this.$sce.trustAsHtml(response.data);
          this.$scope.watchClickableValues();
        })
        .catch((error) => {
          this.loading = false;
          this.error   = error;
        });
    }

    /**
     * Toggles the view of timestamps
     */
    toggleTimeStamps() {
      if (localStorage) { // update browser saved ts
        localStorage['moloch-ts'] = this.$scope.params.ts;
      }
    }

    removeItem(value, field) {
      if (field === 'tags') {
        this.SessionService.removeTags([this.$scope.session.id], value)
          .then((response) => {
            this.getDetailData(); // refresh content
          })
          .catch((error) => {
            this.error = error;
          });
      }
    }

  }

  SessionDetailController.$inject = ['$scope','$sce',
    'SessionService','ConfigService','FieldService'];


  angular.module('moloch')
    .directive('sessionDetail', ['$timeout', '$filter', '$compile', '$location',
    function($timeout, $filter, $compile, $location) {
      return {
        template    : require('html!../templates/session.detail.html'),
        controller  : SessionDetailController,
        controllerAs: '$ctrl',
        scope       : { session: '=' },
        link        : function SessionDetailLink(scope, element, attrs, ctrl) {

          /* exposed functions --------------------------------------------- */
          var formHTMLs = {
            'add:tags'      : `<div class="margined-bottom-xlg">
                                <session-tag class="form-container"
                                sessions="[session]" add="true"></session-tag>
                              </div>`,
            'remove:tags'   : `<div class="margined-bottom-xlg">
                                <session-tag class="form-container"
                                sessions="[session]" add="false"></session-tag>
                              </div>`,
            'export:pcap'   : `<div class="margined-bottom-xlg">
                                <export-pcap class="form-container"
                                sessionid="session.id"></export-pcap>
                              </div>`,
            'scrub:pcap'    : `<div class="margined-bottom-xlg">
                                <scrub-pcap class="form-container"
                                sessionid="session.id"></scrub-pcap>
                              </div>`,
            'delete:session': `<div class="margined-bottom-xlg">
                                <session-delete class="form-container"
                                sessionid="session.id"></session-delete>
                              </div>`,
            'send:session'  : `<div class="margined-bottom-xlg">
                                <session-send class="form-container"
                                sessionid="session.id" cluster="cluster"></session-send>
                              </div>`
          };

          scope.displayFormContainer = function(args) {
            var formContainer = element.find('.form-container');
            var html = formHTMLs[args.form];

            // pass in the cluster for sending session
            if (args.cluster) { scope.cluster = args.cluster; }

            // display a message to the user (overrides form)
            if (args.message) {
              html = `<div class="alert alert-success form-container">
                      ${args.message}</div>`;
            }

            if (html) {
              var content = $compile(html)(scope);
              formContainer.replaceWith(content);
            }
          };

          scope.hideFormContainer = function() {
            element.find('.form-container').hide();
          };


          /* link utilities ------------------------------------------------ */
          function safeStr(str) {
            if (str === undefined) { return ''; }

            if (Array.isArray(str)) { return str.map(safeStr); }

            return str.replace(/&/g,'&amp;').replace(/</g,'&lt;')
              .replace(/>/g,'&gt;').replace(/\"/g,'&quot;')
              .replace(/\'/g, '&#39;').replace(/\//g, '&#47;');
          }

          function getClickInfo(target) {
            var molochfield = target.getAttribute('molochfield');
            if (!molochfield) { return { category:[] }; }

            var field = scope.molochFields[molochfield];
            if (!field) { return { category:[] }; }

            if (Array.isArray(field.category)) {
              return {field: molochfield, category:field.category, info: field};
            } else {
              return {field: molochfield, category:[field.category], info: field};
            }
          }

          function getClickValue(target) {
            var value = target.getAttribute('molochvalue');
            if (!value) { return ''; }

            return value;
          }


          /* click events -------------------------------------------------- */
          /**
           * Triggered when a moloch value is clicked
           * Must have attributes molochfield an molochvalue
           * @param {Object} e The click event
           */
          function molochExprClick(e) {
            e.preventDefault();

            // if user is selecting text, don't add expression to query
            if (window.getSelection().toString() !== '') { return; }

            var target = e.target;

            var field  = target.getAttribute('molochfield');

            // if click target doesn't have a molochfield attribute,
            while (target && !field) { // it may be in the parent or parent's parent
              target  = target.parentNode;
              field   = target.getAttribute('molochfield');
            }

            var val   = target.getAttribute('molochvalue');
            var expr  = target.getAttribute('molochexpr');
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

          var menuItems;
          function buildMenu(items, value, target) {
            var html = `<ul class="dropdown-menu moloch-menu">`;

            var field;

            for (var k in items) {
              var item = items[k];
              // all items should be of the same field
              if (!field) { field = item.info.field; }

              html += `<li class="cursor-pointer"
                molochfield="${field}"
                molochexpr="${item.exp}"
                molochvalue='${value}'>
                <a class="menu-item">${item.name}</a>
                </li>`;
            }

            if (field === 'tags') {
              // allows a user (with permission) to remove the value from the session
              html += `<li has-permission="removeEnabled" class="divider"></li>
                <li has-permission="removeEnabled">
                  <a ng-click="$ctrl.removeItem('${value}', '${field}')">
                    <span class="fa fa-trash-o"></span>&nbsp; remove ${value}
                  </a>
                </li>`;
            }

            html += '</ul>';

            // have to compile it so has-permission directive
            // and removeItem function work
            var content = $compile(html)(scope);

            $(target).after(content[0]);

            menuItems = target.parentNode.querySelectorAll('.menu-item');
            for (var i = 0, len = menuItems.length; i < len; ++i) {
              menuItems[i].addEventListener('click', molochExprClick);
            }
          }

          /**
           * Triggered when a time range value is clicked
           * Must have attribute molochstart or molochstop
           * @param {Object} e The click event
           */
          function timeClick(e) {
            e.preventDefault();

            var start = e.target.getAttribute('molochstart');
            var stop  = e.target.getAttribute('molochstop');

            var result = {};
            if (start) { result.start = start; }
            if (stop)  { result.stop  = stop; }

            if (result.start || result.stop) {
              scope.$emit('change:time', result);
            }
          }


          /**
           * Triggered when ellipsis to show more items is clicked
           * @param {Object} e The click event
           */
          function showMoreItems(e) {
            $(e.target).hide();
            $(e.target).prev().show();
          }

          /**
           * Triggered when a menu (next to a value) is clicked
           * @param {Object} e The click event
           */
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
            var urlParams = $location.search();
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
          var clickableValues, clickableTimes, molochMenus, showMore;
          var srccol, dstcol, imgs;

          scope.watchClickableValues = function() {
            $timeout(function() { // wait until session detail is rendered
              var i, len, time, value, timeEl;

              // display session actions dropdown
              var actionsEl       = element.find('.session-actions-menu');
              // var exists = actionsEl.find('')
              if (actionsEl.find('session-actions').length === 0) {
                var actionsContent  = $compile('<session-actions></session-actions>')(scope);
                actionsEl.append(actionsContent);
                actionsEl.dropdown();
              }

              // display packet option buttons
              var optionsEl   = element.find('.packet-options');
              var optContent  = $compile(optionsHTML)(scope);
              optionsEl.replaceWith(optContent);

              // display tag adding button by the tags
              var tagEl       = element.find('.session-tag-container');
              var tagContent  = $compile(`<div ng-click="displayFormContainer({form:'add:tags'})"
                                          uib-tooltip="Add a new tag to this session"
                                          class="btn btn-xs btn-blue margined-left-xlg margined-bottom margined-top">
                                          <span class="fa fa-plus-circle"></span>
                                        </div>`)(scope);
              tagEl.replaceWith(tagContent);

              // add click listener to add expression to search input
              clickableValues = element[0].querySelectorAll('.moloch-clickable[molochfield]');
              for (i = 0, len = clickableValues.length; i < len; ++i) {
                clickableValues[i].addEventListener('click', molochExprClick);
              }

              // add click listener for all menus (next to each value)
              molochMenus = element[0].querySelectorAll('[molochmenu]');
              for (i = 0, len = molochMenus.length; i < len; ++i) {
                molochMenus[i].addEventListener('click', contextMenuClick);
              }

              // add click listener to show more values in list
              showMore = element[0].querySelectorAll('.show-more-items');
              for (i = 0, len = showMore.length; i < len; ++i) {
                showMore[i].addEventListener('click', showMoreItems);
              }

              clickableTimes = element[0].querySelectorAll('.format-seconds');
              for (i = 0, len = clickableTimes.length; i < len; ++i) {
                timeEl  = clickableTimes[i];
                value   = timeEl.innerText;
                if (!isNaN(value)) { // only parse value if it's a number (s from 1970)
                  time = $filter('date')(timeEl.innerHTML * 1000, 'yyyy/MM/dd HH:mm:ss');
                  timeEl.innerHTML = time;
                }

                // if it has a value, set it to parsed time
                if (timeEl.getAttribute('molochvalue')) {
                  timeEl.setAttribute('molochvalue', "\"" + time + "\"");
                }

                // add click listener for time values
                // if it has molochstart or molochstop, add these to the
                // time range, not the search input
                if (timeEl.getAttribute('molochstart') ||
                    timeEl.getAttribute('molochstop')) {
                  timeEl.addEventListener('click', timeClick);
                }
              }

              // modify the packet timestamp values
              var tss = element[0].querySelectorAll('.session-detail-ts');
              for (i = 0, len = tss.length; i < len; ++i) {
                timeEl  = tss[i];
                value   = timeEl.getAttribute('ts');
                timeEl  = timeEl.querySelectorAll('.ts-value');
                if (!isNaN(value)) { // only parse value if it's a number (ms from 1970)
                  time = $filter('date')(value, 'yyyy/MM/dd HH:mm:ss.sss');
                  timeEl[0].innerHTML = time;
                }
              }

              srccol = element[0].querySelector('.srccol');
              if (srccol) {
                $(srccol).tooltip({ placement:'right', html:true });
              }

              dstcol = element[0].querySelector('.dstcol');
              if (dstcol) {
                $(dstcol).tooltip({ placement:'right', html:true });
              }

              imgs = element[0].querySelectorAll('.imagetag');
              for (i = 0, len = imgs.length; i < len; ++i) {
                var img = imgs[i];
                var href = img.getAttribute('href');
                href = href.replace('body', 'bodypng');
                $(img).tooltip({
                  placement :'top',
                  html      :true,
                  title     : `File Bytes:<br><img src="${href}">`
                });
              }
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

            if (showMore) {
              for (i = 0, len = showMore.length; i < len; ++i) {
                showMore[i].removeEventListener('click', showMoreItems);
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

            if (srccol) { $(srccol).tooltip('destroy'); }

            if (dstcol) { $(dstcol).tooltip('destroy'); }

            if (imgs) {
              for (i = 0, len = imgs.length; i < len; ++i) {
                $(imgs[i]).tooltip('destroy');
              }
            }
          });

        }
      };
    }]);

})();
