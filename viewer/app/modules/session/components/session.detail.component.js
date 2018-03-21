(function() {

  'use strict';

  const defaultUserSettings = {
    detailFormat  : 'last',
    numPackets    : 'last',
    showTimestamps: 'last'
  };

  /**
   * @class SessionDetailController
   * @classdesc Interacts with session details
   */
  class SessionDetailController {

    /* setup --------------------------------------------------------------- */
    /**
     * Initialize global variables for this controller
     * @param $sce            Angular strict contextual escaping service
     * @param $scope          Angular application model object
     * @param $sanitize       Sanitizes an html string by stripping all potentially dangerous tokens
     * @param $routeParams    Retrieve the current set of route parameters
     * @param SessionService  Transacts sessions with the server
     * @param ConfigService   Transacts app configurations with the server
     * @param FieldService    Retrieves available fields from the server
     * @param UserService     Transacts users and user data with the server
     *
     * @ngInject
     */
    constructor($sce, $scope, $sanitize, $routeParams,
                SessionService, ConfigService, FieldService, UserService) {
      this.$sce           = $sce;
      this.$scope         = $scope;
      this.$sanitize      = $sanitize;
      this.$routeParams   = $routeParams;
      this.SessionService = SessionService;
      this.ConfigService  = ConfigService;
      this.FieldService   = FieldService;
      this.UserService    = UserService;
    }

    /* Callback when component is mounted and ready */
    $onInit() {
      this.loading = true;
      this.error   = false;

      // default session detail parameters
      // add to $scope so session.detail.options can use it
      this.$scope.params = {
        base    : 'hex',
        line    : false,
        image   : false,
        gzip    : false,
        ts      : false,
        decode  : {},
        packets : 200
      };

      this.UserService.getSettings()
        .then((response) => {
          this.userSettings = response;

          this.setUserParams();
          this.getDecodings(); // IMPORTANT: kicks of packet request
        })
        .catch((error) => {
          // can't get user, so use defaults
          this.userSettings = defaultUserSettings;
          this.getDecodings(); // IMPORTANT: kicks of packet request
        });

      this.getDetailData(); // get SPI data

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
        this.$scope.hideFormContainer();

        if (args) {
          if (args.reloadData)  {
            if (args.message) { this.getDetailData(args.message); }
            else { this.getDetailData(); }
          } else if (args.message) {
            this.$scope.displayMessage(args.message);
          }
        }
      });
    }

    /* sets some of the session detail query parameters based on user settings */
    setUserParams() {
      if (localStorage && this.userSettings) { // display user saved options
        if (this.userSettings.detailFormat === 'last' && localStorage['moloch-base']) {
          this.$scope.params.base = localStorage['moloch-base'];
        } else if (this.userSettings.detailFormat) {
          this.$scope.params.base = this.userSettings.detailFormat;
        }
        if (this.userSettings.numPackets === 'last' && localStorage['moloch-packets']) {
          this.$scope.params.packets = localStorage['moloch-packets'];
        } else if (this.userSettings.numPackets) {
          this.$scope.params.packets = this.userSettings.numPackets;
        }
        if (this.userSettings.showTimestamps === 'last' && localStorage['moloch-ts']) {
          this.$scope.params.ts = localStorage['moloch-ts'] === 'true';
        } else if (this.userSettings.showTimestamps) {
          this.$scope.params.ts = this.userSettings.showTimestamps === 'on';
        }
      }
    }

    /* sets some of the session detail query parameters based on browser saved
     * options */
    setBrowserParams() {
      if (localStorage) { // display browser saved options
        if (localStorage['moloch-line']) {
          this.$scope.params.line = JSON.parse(localStorage['moloch-line']);
        }
        if (localStorage['moloch-gzip']) {
          this.$scope.params.gzip = JSON.parse(localStorage['moloch-gzip']);
        }
        if (localStorage['moloch-image']) {
          this.$scope.params.image = JSON.parse(localStorage['moloch-image']);
        }
        if (localStorage['moloch-decodings']) {
          this.$scope.params.decode = JSON.parse(localStorage['moloch-decodings']);
          for (let key in this.decodings) {
            if (this.decodings.hasOwnProperty(key)) {
              if (this.$scope.params.decode[key]) {
                this.decodings[key].active = true;
                for (let field in this.$scope.params.decode[key]) {
                  for (let i = 0, len = this.decodings[key].fields.length; i < len; ++i) {
                    if (this.decodings[key].fields[i].key === field) {
                      this.decodings[key].fields[i].value = this.$scope.params.decode[key][field];
                    }
                  }
                }
              }
            }
          }
        }
      }
    }


    /* exposed functions --------------------------------------------------- */
    /* gets other decodings for packet data
     * IMPORTANT: kicks of packet request */
    getDecodings() {
      this.SessionService.getDecodings()
        .then((response) => {
          this.decodings = response;

          this.setBrowserParams();
          this.getPackets();
        })
        .catch(() => {
          this.setBrowserParams();
          // still get the packets
          this.getPackets();
        });
    }

    /**
     * Gets the session detail from the server
     * @param {string} message An optional message to display to the user
     */
    getDetailData(message) {
      this.loading = true;

      this.SessionService.getDetail(this.$scope.session.id, this.$scope.session.node)
        .then((response) => {
          this.loading = false;
          this.$scope.detailHtml = this.$sce.trustAsHtml(response.data);

          this.$scope.renderDetail();

          if (message) { this.$scope.displayMessage(message); }
        })
        .catch((error) => {
          this.loading = false;
          this.error   = error;
        });
    }

    /* Gets the packets for the session from the server */
    getPackets() {
      // already loading, don't load again!
      if (this.loadingPackets) { return; }

      this.loadingPackets = true;
      this.errorPackets   = false;

      if (localStorage) { // update browser saved options
        if (this.userSettings.detailFormat === 'last') {
          localStorage['moloch-base'] = this.$scope.params.base;
        }
        if (this.userSettings.numPackets === 'last') {
          localStorage['moloch-packets'] = this.$scope.params.packets || 200;
        }
        localStorage['moloch-line']   = this.$scope.params.line;
        localStorage['moloch-gzip']   = this.$scope.params.gzip;
        localStorage['moloch-image']  = this.$scope.params.image;
      }

      this.packetPromise = this.SessionService.getPackets(this.$scope.session.id,
         this.$scope.session.node, this.$scope.params);

      this.packetPromise.then((response) => {
        this.loadingPackets = false;
        this.packetPromise  = null;

        if (response && response.data) {
          this.$scope.packetHtml = this.$sce.trustAsHtml(response.data);
          // remove all un-whitelisted tokens from the html
          this.$scope.packetHtml = this.$sanitize(this.$scope.packetHtml);

          this.$scope.renderPackets();
        }
      })
      .catch((error) => {
        this.loadingPackets = false;
        this.errorPackets   = error;
        this.packetPromise  = null;
      });
    }

    /* Toggles the view of packet timestamps */
    toggleTimeStamps() {
      if (localStorage && this.userSettings.showTimestamps === 'last') {
        // update browser saved ts if the user settings is set to lastl
        localStorage['moloch-ts'] = this.$scope.params.ts;
      }
    }

    /**
     * Shows more items in a list of values
     * @param {object} e The click event
     */
    showMoreItems(e) {
      $(e.target).hide().prev().show();
    }

    /**
     * Hides more items in a list of values
     * @param {object} e The click event
     */
    showFewerItems(e) {
      $(e.target).parent().hide().next().show();
    }

    /**
     * Adds a rootId expression
     * @param {string} rootId The root id of the session
     * @param {int} startTime The start time of the session
     */
    allSessions(rootId, startTime) {
      let fullExpression = `rootId == \"${rootId}\"`;

      this.$scope.$emit('add:to:search', { expression: fullExpression });

      if (this.$routeParams.startTime) {
        if (this.$routeParams.startTime < startTime) {
          startTime = this.$routeParams.startTime;
        }
      }

      this.$scope.$emit('change:time', { start:startTime });
    }

    /* Cancels the packet loading request */
    cancelPacketLoad() {
      this.packetPromise.abort();
      this.packetPromise  = null;
      this.errorPackets   = 'Request canceled.';
    }

    /* other decodings */
    /**
     * Toggles a decoding on or off
     * If a decoding needs more input, shows form
     * @param {string} key Identifier of the decoding to toggle
     */
    toggleDecoding(key) {
      let decoding = this.decodings[key];

      decoding.active = !decoding.active;

      if (decoding.fields && decoding.active) {
        this.decodingForm = key;
      } else {
        this.decodingForm = false;
        this.applyDecoding(key);
      }
    }

    /**
     * Closes the form for additional decoding input
     * @param {bool} active The active state of the decoding
     */
    closeDecodingForm(active) {
      if (this.decodingForm) {
        this.decodings[this.decodingForm].active = active;
      }

      this.decodingForm = false;
    }

    /**
     * Sets the decode param, issues query, and closes form if necessary
     * @param {key} key Identifier of the decoding to apply
     */
    applyDecoding(key) {
      this.$scope.params.decode[key] = {};
      let decoding = this.decodings[key];

      if (decoding.active) {
        if (decoding.fields) {
          for (let i = 0, len = decoding.fields.length; i < len; ++i) {
            let field = decoding.fields[i];
            this.$scope.params.decode[key][field.key] = field.value;
          }
        }
      } else {
        this.$scope.params.decode[key] = null;
        delete this.$scope.params.decode[key];
      }

      this.getPackets();
      this.closeDecodingForm(decoding.active);

      // update local storage
      localStorage['moloch-decodings'] = JSON.stringify(this.$scope.params.decode);
    }

    /**
     * Opens a new browser tab containing all the unique values for a given field
     * @param {string} fieldID  The field id to display unique values for
     * @param {int} counts      Whether to display the unique values with counts (1 or 0)
     */
    exportUnique(fieldID, counts) {
      this.SessionService.exportUniqueValues(fieldID, counts);
    }

    /**
     * Opens the spi graph page in a new browser tab
     * @param {string} fieldID The field id (dbField) to display spi graph data for
     */
    openSpiGraph(fieldID) {
      this.SessionService.openSpiGraph(fieldID);
    }

  }

  SessionDetailController.$inject = ['$sce','$scope','$sanitize','$routeParams',
    'SessionService','ConfigService','FieldService','UserService'];


  angular.module('moloch')
    .directive('sessionDetail',
    ['$timeout','$filter','$compile','$routeParams','$location','$route',
    function($timeout, $filter, $compile, $routeParams, $location, $route) {
      return {
        template    : require('../templates/session.detail.html'),
        controller  : SessionDetailController,
        controllerAs: '$ctrl',
        scope       : { session: '=' },
        link        : function SessionDetailLink(scope, element, attrs, ctrl) {
          let timeout;

          /* exposed functions --------------------------------------------- */
          let formHTMLs = {
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
                                sessions="[session]"></export-pcap>
                              </div>`,
            'scrub:pcap'    : `<div class="margined-bottom-xlg">
                                <scrub-pcap class="form-container"
                                sessions="[session]"></scrub-pcap>
                              </div>`,
            'delete:session': `<div class="margined-bottom-xlg">
                                <session-delete class="form-container"
                                sessions="[session]"></session-delete>
                              </div>`,
            'send:session'  : `<div class="margined-bottom-xlg">
                                <session-send class="form-container"
                                sessions="[session]" cluster="cluster"></session-send>
                              </div>`
          };

          scope.displayFormContainer = function(args) {
            let formContainer = element.find('.form-container');
            let html = formHTMLs[args.form];

            // pass in the cluster for sending session
            if (args.cluster) { scope.cluster = args.cluster; }

            if (html) {
              let content = $compile(html)(scope);
              formContainer.replaceWith(content);
            }
          };

          scope.displayMessage = function(message) {
            timeout = $timeout(function() { // timeout to wait for detail to render
              // display a message to the user (overrides form)
              let formContainer = element.find('.form-container');
              let html = `<div class="form-container">
                            <toast message="'${message}'" type="'success'"></toast>
                          </div>`;

              let content = $compile(html)(scope);
              formContainer.replaceWith(content);
            });
          };

          scope.hideFormContainer = function() {
            element.find('.form-container').hide();
          };

          scope.openPermalink = function() {
            $location.path('sessions')
              .search('expression', `id=${scope.session.id}`)
              .search('startTime', scope.session.firstPacket)
              .search('stopTime', scope.session.lastPacket)
              .search('openAll', 1);
          };

          /**
           * Renders the session detail html
           * Then the session actions menu
           * Then the packet options
           */
          let srccol, dstcol, imgs;

          scope.renderDetail = function() {
            // compile and render the session detail
            let template = `<div class="detail-container" 
                              ng-class="{'show-ts':params.ts === true}">
                                ${scope.detailHtml}</div>`;
            let compiled = $compile(template)(scope);
            element.find('.detail-container').replaceWith(compiled);

            timeout = $timeout(function() { // wait until session detail is rendered
              let i, len, time, value, timeEl;

              // display session actions dropdown
              let actionsEl = element.find('.session-actions-menu');
              if (actionsEl.find('session-actions').length === 0) {
                let actionsContent  = $compile('<session-actions></session-actions>')(scope);
                actionsEl.append(actionsContent);
                actionsEl.dropdown();
              }
            });
          };
          
          scope.renderPackets = function() {
            // render session packets (don't compile!)
            // if there are lots of packets, rendering could take a while
            // so display a message (the user cannot cancel this action)
            scope.renderingPackets = true;

            let template = `<div class="inner">${scope.packetHtml}</div>`;
            element.find('.packet-container > .inner').replaceWith(template);

            timeout = $timeout(function() { // wait until session packets are rendered
              scope.renderingPackets = false;

              let i, len, time, value, timeEl;

              // modify the packet timestamp values
              let tss = element[0].querySelectorAll('.session-detail-ts');
              for (i = 0, len = tss.length; i < len; ++i) {
                timeEl  = tss[i];
                value   = timeEl.getAttribute('value');
                timeEl  = timeEl.querySelectorAll('.ts-value');
                if (!isNaN(value)) { // only parse value if it's a number (ms from 1970)
                  time = $filter('date')(value, 'yyyy/MM/dd HH:mm:ss.sss');
                  timeEl[0].innerHTML = time;
                }
              }

              // add tooltips to display source/destination byte visualization
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
                let img = imgs[i];
                let href = img.getAttribute('href');
                href = href.replace('body', 'bodypng');
                $(img).tooltip({
                  placement : 'right',
                  html      : true,
                  title     : `File Bytes:<br><img src="${href}">`
                });
              }
            });
          };


          // cleanup! cleanup! everybody, everywhere
          scope.$on('$destroy', function() {
            // remove listeners to prevent memory leaks
            if (srccol) { $(srccol).tooltip('destroy'); }

            if (dstcol) { $(dstcol).tooltip('destroy'); }

            if (imgs) {
              for (let i = 0, len = imgs.length; i < len; ++i) {
                $(imgs[i]).tooltip('destroy');
              }
            }

            // cancel server request for packets
            if (ctrl.packetPromise) { ctrl.cancelPacketLoad(); }

            if (timeout) { $timeout.cancel(timeout); }

            element.remove();
          });

        }
      };
    }]);

})();
