<template>

  <!-- session detail -->
  <div>

    <!-- detail loading -->
    <div v-if="loading"
      class="mt-1 mb-1 large">
      <span class="fa fa-spinner fa-spin"></span>&nbsp;
      Loading session detail...
    </div> <!-- /detail loading -->

    <!-- detail error -->
    <div v-if="error"
      class="text-danger mt-1 mb-1 large">
      <span class="fa fa-exclamation-triangle"></span>&nbsp;
      {{ error }}
    </div> <!-- /detail error -->

    <!-- detail -->
    <div class="detail-container"
      ref="detailContainer">
    </div> <!-- /detail -->

    <!-- packet options -->
    <div v-show="!loading"
      class="packet-options mr-1 ml-1">
      <form class="form-inline mb-2 pt-2">
        <fieldset :disabled="loading || loadingPackets || errorPackets || renderingPackets">
          <div class="form-group">
            <div class="input-group input-group-sm mb-1 mr-1">
              <span class="input-group-prepend cursor-help"
                v-b-tooltip
                title="Number of Packets">
                <span class="input-group-text">
                  Packets
                </span>
              </span>
              <select class="form-control"
                style="-webkit-appearance:none;"
                v-model="params.packets"
                @change="getPackets()">
                <option value="50">50</option>
                <option value="200">200</option>
                <option value="500">500</option>
                <option value="1000">1,000</option>
                <option value="2000">2,000</option>
              </select>
            </div>
            <b-form-group class="mr-1 mb-1">
              <b-form-radio-group
                size="sm"
                buttons
                v-model="params.base"
                @input="getPackets">
                <b-radio value="natural"
                  class="btn-radio">
                  natural
                </b-radio>
                <b-radio value="ascii"
                  class="btn-radio">
                  ascii
                </b-radio>
                <b-radio value="utf8"
                  class="btn-radio">
                  utf8
                </b-radio>
                <b-radio value="hex"
                  class="btn-radio">
                  hex
                </b-radio>
              </b-form-radio-group>
            </b-form-group>
            <button type="button"
              class="btn btn-secondary btn-checkbox btn-sm mr-1 mb-1"
              :disabled="params.base !== 'hex'"
              :class="{'active':params.line && params.base === 'hex'}"
              @click="toggleLineNumbers">
              <span class="fa fa-list-ol">
              </span>&nbsp;
              Line Numbers
            </button>
            <button type="button"
              class="btn btn-secondary btn-checkbox btn-sm mr-1 mb-1"
              :class="{'active':params.gzip}"
              @click="toggleCompression">
              <span class="fa fa-file-archive-o">
              </span>&nbsp;
              Uncompress
            </button>
            <button type="button"
              class="btn btn-secondary btn-checkbox btn-sm mr-1 mb-1"
              :class="{'active':params.image}"
              @click="toggleImages">
              <span class="fa fa-file-image-o">
              </span>&nbsp;
              Show Image &amp; Files
            </button>
            <button type="button"
              class="btn btn-secondary btn-checkbox btn-sm mr-1 mb-1"
              :class="{'active':params.ts}"
              @click="toggleTimestamps">
              <span class="fa fa-clock-o">
              </span>&nbsp;
              Show Timestamps
            </button>
            <!-- decodings -->
            <div class="btn-group mr-1 mb-1"
              role="group">
              <button v-for="(value, key) in decodings"
                :key="key"
                type="button"
                class="btn btn-secondary btn-checkbox btn-sm"
                v-b-tooltip.hover
                :title="value.name + 'Decoding'"
                :class="{'active':params.decode[key]}"
                @click="toggleDecoding(key)">
                {{ value.name }}
              </button>
            </div> <!-- /decodings -->
            <!-- cyberchef -->
            <b-dropdown right
              size="sm"
              class="pull-right mb-1"
              variant="theme-primary"
              text="CyberChef">
              <b-dropdown-item target="_blank"
                :href="cyberChefSrcUrl">
                <span class="fa fa-fw fa-file-o"></span>&nbsp;
                Open src packets with CyberChef
              </b-dropdown-item>
              <b-dropdown-item target="_blank"
                :href="cyberChefDstUrl">
                <span class="fa fa-fw fa-file-o"></span>&nbsp;
                Open dst packets with CyberChef
              </b-dropdown-item>
            </b-dropdown> <!-- cyberchef -->
          </div>
        </fieldset>
      </form>
      <!-- decoding form -->
      <div v-if="decodingForm">
        <form class="form-inline well well-sm mt-1">
          <span v-for="field in decodings[decodingForm].fields"
            :key="field.name"
            v-if="!field.disabled">
            <div class="form-group mr-1 mt-1">
              <div class="input-group input-group-sm">
                <span class="input-group-prepend">
                  <span class="input-group-text">
                    {{ field.name }}
                  </span>
                </span>
                <input v-model="field.value"
                  class="form-control"
                  type="field.type"
                />
              </div>
            </div>
          </span>
          <div class="btn-group btn-group-sm pull-right mt-1 mb-1">
            <button type="button"
              class="btn btn-warning"
              title="cancel"
              v-b-tooltip.hover
              @click="closeDecodingForm(false)">
              <span class="fa fa-ban">
              </span>
            </button>
            <button type="button"
              class="btn btn-theme-primary"
              title="apply"
              v-b-tooltip.hover
              @click="applyDecoding(decodingForm)">
              <span class="fa fa-check">
              </span>
            </button>
          </div>
        </form>
        <div class="help-block">
          <span class="fa fa-info-circle">
          </span>&nbsp;
          {{ decodings[decodingForm].title }}
        </div>
      </div> <!-- /decoding form -->
    </div> <!-- /packet options -->

    <!-- packets loading -->
    <div v-if="!loading && loadingPackets"
      class="mt-4 mb-4 large">
      <span class="fa fa-spinner fa-spin">
      </span>&nbsp;
      Loading session packets&nbsp;
      <button type="button"
        @click="cancelPacketLoad()"
        class="btn btn-warning btn-xs">
        <span class="fa fa-ban">
        </span>&nbsp;
        cancel
      </button>
    </div> <!-- /packets loading -->

    <!-- packets rendering -->
    <div v-if="!loading && renderingPackets"
      class="mt-4 mb-4 large">
      <span class="fa fa-spinner fa-spin">
      </span>&nbsp;
      Rendering session packets
    </div> <!-- /packets rendering -->

    <!-- packets error -->
    <div v-if="!error && errorPackets"
      class="mt-4 mb-4 large">
      <span class="text-danger">
        <span class="fa fa-exclamation-triangle">
        </span>&nbsp;
        {{ errorPackets }}&nbsp;
      </span>
      <button type="button"
        @click="getPackets()"
        class="btn btn-success btn-xs">
        <span class="fa fa-refresh">
        </span>&nbsp;
        retry
      </button>
    </div> <!-- /packets error -->

    <!-- packets -->
    <div v-if="!loadingPackets && !errorPackets"
      class="inner packet-container mr-1 ml-1"
      v-html="packetHtml"
      ref="packetContainer"
      :class="{'show-ts':params.ts === true}">
    </div> <!-- packets -->

    <!-- packet options -->
    <div v-show="!loading && !loadingPackets && !errorPackets"
      class="mr-1 ml-1">
      <form class="form-inline mb-2 pt-2">
        <fieldset :disabled="loading || loadingPackets || errorPackets || renderingPackets">
          <div class="form-group">
            <div class="input-group input-group-sm mb-1 mr-1">
              <span class="input-group-prepend cursor-help"
                v-b-tooltip
                title="Number of Packets">
                <span class="input-group-text">
                  Packets
                </span>
              </span>
              <select class="form-control"
                style="-webkit-appearance:none;"
                v-model="params.packets"
                @change="getPackets()">
                <option value="50">50</option>
                <option value="200">200</option>
                <option value="500">500</option>
                <option value="1000">1,000</option>
                <option value="2000">2,000</option>
              </select>
            </div>
            <b-form-group class="mr-1 mb-1">
              <b-form-radio-group
                size="sm"
                buttons
                v-model="params.base"
                @input="getPackets">
                <b-radio value="natural"
                  class="btn-radio">
                  natural
                </b-radio>
                <b-radio value="ascii"
                  class="btn-radio">
                  ascii
                </b-radio>
                <b-radio value="utf8"
                  class="btn-radio">
                  utf8
                </b-radio>
                <b-radio value="hex"
                  class="btn-radio">
                  hex
                </b-radio>
              </b-form-radio-group>
            </b-form-group>
            <button type="button"
              class="btn btn-secondary btn-checkbox btn-sm mr-1 mb-1"
              :disabled="params.base !== 'hex'"
              :class="{'active':params.line && params.base === 'hex'}"
              @click="toggleLineNumbers">
              <span class="fa fa-list-ol">
              </span>&nbsp;
              Line Numbers
            </button>
            <button type="button"
              class="btn btn-secondary btn-checkbox btn-sm mr-1 mb-1"
              :class="{'active':params.gzip}"
              @click="toggleCompression">
              <span class="fa fa-file-archive-o">
              </span>&nbsp;
              Uncompress
            </button>
            <button type="button"
              class="btn btn-secondary btn-checkbox btn-sm mr-1 mb-1"
              :class="{'active':params.image}"
              @click="toggleImages">
              <span class="fa fa-file-image-o">
              </span>&nbsp;
              Show Image &amp; Files
            </button>
            <button type="button"
              class="btn btn-secondary btn-checkbox btn-sm mr-1 mb-1"
              :class="{'active':params.ts}"
              @click="toggleTimestamps">
              <span class="fa fa-clock-o">
              </span>&nbsp;
              Show Timestamps
            </button>
            <!-- decodings -->
            <div class="btn-group mr-1 mb-1"
              role="group">
              <button v-for="(value, key) in decodings"
                :key="key"
                type="button"
                class="btn btn-secondary btn-checkbox btn-sm"
                v-b-tooltip.hover
                :title="value.name + 'Decoding'"
                :class="{'active':params.decode[key]}"
                @click="toggleDecoding(key)">
                {{ value.name }}
              </button>
            </div> <!-- /decodings -->
            <!-- cyberchef -->
            <b-dropdown right
              size="sm"
              class="pull-right mb-1"
              variant="theme-primary"
              text="CyberChef">
              <b-dropdown-item target="_blank"
                :href="cyberChefSrcUrl">
                <span class="fa fa-fw fa-file-o"></span>&nbsp;
                Open src packets with CyberChef
              </b-dropdown-item>
              <b-dropdown-item target="_blank"
                :href="cyberChefDstUrl">
                <span class="fa fa-fw fa-file-o"></span>&nbsp;
                Open dst packets with CyberChef
              </b-dropdown-item>
            </b-dropdown> <!-- cyberchef -->
          </div>
        </fieldset>
      </form>
      <!-- decoding form -->
      <div v-if="decodingForm">
        <form class="form-inline well well-sm mt-1">
          <span v-for="field in decodings[decodingForm].fields"
            :key="field.name"
            v-if="!field.disabled">
            <div class="form-group mr-1 mt-1">
              <div class="input-group input-group-sm">
                <span class="input-group-prepend">
                  <span class="input-group-text">
                    {{ field.name }}
                  </span>
                </span>
                <input v-model="field.value"
                  class="form-control"
                  type="field.type"
                />
              </div>
            </div>
          </span>
          <div class="btn-group btn-group-sm pull-right mt-1 mb-1">
            <button type="button"
              class="btn btn-warning"
              title="cancel"
              v-b-tooltip.hover
              @click="closeDecodingForm(false)">
              <span class="fa fa-ban">
              </span>
            </button>
            <button type="button"
              class="btn btn-theme-primary"
              title="apply"
              v-b-tooltip.hover
              @click="applyDecoding(decodingForm)">
              <span class="fa fa-check">
              </span>
            </button>
          </div>
        </form>
        <div class="help-block">
          <span class="fa fa-info-circle">
          </span>&nbsp;
          {{ decodings[decodingForm].title }}
        </div>
      </div> <!-- /decoding form -->
    </div> <!-- /packet options -->

  </div> <!-- /session detail -->

</template>

<script>
import Vue from 'vue';
import qs from 'qs';
import sanitizeHtml from 'sanitize-html';
import UserService from '../users/UserService';
import ConfigService from '../utils/ConfigService';
import SessionsService from './SessionsService';
import FieldService from '../search/FieldService';
import MolochTagSessions from '../sessions/Tags';
import MolochDeleteSessions from '../sessions/Delete';
import MolochScrubPcap from '../sessions/Scrub';
import MolochSendSessions from '../sessions/Send';
import MolochExportPcap from '../sessions/ExportPcap';
import MolochToast from '../utils/Toast';

const defaultUserSettings = {
  detailFormat: 'last',
  numPackets: 'last',
  showTimestamps: 'last'
};

export default {
  name: 'MolochSessionDetail',
  props: [ 'session' ],
  data: function () {
    return {
      error: '',
      loading: true,
      errorPackets: '',
      loadingPackets: false,
      userSettings: {},
      packetPromise: undefined,
      detailHtml: undefined,
      molochclusters: {},
      packetHtml: undefined,
      renderingPackets: false,
      decodingForm: false,
      decodings: {},
      params: {
        base: 'hex',
        line: false,
        image: false,
        gzip: false,
        ts: false,
        decode: {},
        packets: 200
      }
    };
  },
  computed: {
    cyberChefSrcUrl: function () {
      return `${this.session.node}/session/${this.session.id}/cyberchef?type=src&recipe=[{"op":"From Hex","args":["None"]}]`;
    },
    cyberChefDstUrl: function () {
      return `${this.session.node}/session/${this.session.id}/cyberchef?type=dst&recipe=[{"op":"From Hex","args":["None"]}]`;
    }
  },
  created: function () {
    this.getUserSettings();
    this.getDetailData();
  },
  methods: {
    /* exposed functions --------------------------------------------------- */
    /* Cancels the packet loading request */
    cancelPacketLoad: function () {
      if (this.packetPromise) {
        this.packetPromise.source.cancel();
        this.packetPromise = null;
        this.loadingPackets = false;
        this.errorPackets = 'Request canceled';
      }
    },
    toggleLineNumbers: function () {
      // can only have line numbers in hex mode
      if (!this.params.base === 'hex') { return; }
      this.params.line = !this.params.line;
      this.getPackets();
    },
    toggleCompression: function () {
      this.params.gzip = !this.params.gzip;
      this.getPackets();
    },
    toggleImages: function () {
      this.params.image = !this.params.image;
      this.getPackets();
    },
    toggleTimestamps: function () {
      this.params.ts = !this.params.ts;
      if (localStorage && this.userSettings.showTimestamps === 'last') {
        // update browser saved ts if the user settings is set to last
        localStorage['moloch-ts'] = this.params.ts;
      }
    },
    /**
     * Toggles a decoding on or off
     * If a decoding needs more input, shows form
     * @param {string} key Identifier of the decoding to toggle
     */
    toggleDecoding: function (key) {
      let decoding = this.decodings[key];

      decoding.active = !decoding.active;

      if (decoding.fields && decoding.active) {
        this.decodingForm = key;
      } else {
        this.decodingForm = false;
        this.applyDecoding(key);
      }
    },
    /**
     * Closes the form for additional decoding input
     * @param {bool} active The active state of the decoding
     */
    closeDecodingForm: function (active) {
      if (this.decodingForm) {
        this.decodings[this.decodingForm].active = active;
      }

      this.decodingForm = false;
    },
    /**
     * Sets the decode param, issues query, and closes form if necessary
     * @param {key} key Identifier of the decoding to apply
     */
    applyDecoding: function (key) {
      this.params.decode[key] = {};
      let decoding = this.decodings[key];

      if (decoding.active) {
        if (decoding.fields) {
          for (let i = 0, len = decoding.fields.length; i < len; ++i) {
            let field = decoding.fields[i];
            this.params.decode[key][field.key] = field.value;
          }
        }
      } else {
        this.params.decode[key] = null;
        delete this.params.decode[key];
      }

      this.getPackets();
      this.closeDecodingForm(decoding.active);

      // update local storage
      localStorage['moloch-decodings'] = JSON.stringify(this.params.decode);
    },
    /* helper functions ---------------------------------------------------- */
    /**
     * Gets the session detail from the server
     * @param {string} message An optional message to display to the user
     */
    getDetailData: function (message) {
      this.loading = true;

      let p1 = FieldService.get();
      let p2 = ConfigService.getMolochClusters();
      let p3 = SessionsService.getDetail(this.session.id, this.session.node);

      Promise.all([p1, p2, p3])
        .then((responses) => {
          this.loading = false;

          new Vue({
            // template string here
            template: responses[2].data,
            // makes $parent work
            parent: this,
            // any props the component should receive.
            // reference to data in the template will *not* have access the the current
            // components scope, as you create a new component
            propsData: {
              session: this.session,
              fields: responses[0],
              molochclusters: responses[1]
            },
            props: [ 'session', 'fields', 'molochclusters' ],
            data: function () {
              return {
                form: undefined,
                cluster: undefined,
                message: undefined,
                messageType: undefined
              };
            },
            computed: {
              expression: {
                get: function () {
                  return this.$store.state.expression;
                },
                set: function (newValue) {
                  this.$store.commit('setExpression', newValue);
                }
              },
              startTime: {
                get: function () {
                  return this.$store.state.time.startTime;
                },
                set: function (newValue) {
                  this.$store.commit('setTime', { startTime: newValue });
                }
              }
            },
            methods: {
              actionFormDone: function (message, success, reload) {
                if (message) {
                  this.message = message;
                  this.messageType = success ? 'success' : 'warning';
                }
                this.form = undefined;
              },
              messageDone: function () {
                this.message = undefined;
                this.messageType = undefined;
              },
              addTags: function () {
                this.form = 'add:tags';
              },
              removeTags: function () {
                this.form = 'remove:tags';
              },
              exportPCAP: function () {
                this.form = 'export:pcap';
              },
              scrubPCAP: function () {
                this.form = 'scrub:pcap';
              },
              deleteSession: function () {
                this.form = 'delete:session';
              },
              sendSession: function (cluster) {
                this.form = 'send:session';
                this.cluster = cluster;
              },
              openPermalink: function () {
                let params = {
                  expression: `id == ${this.session.id}`,
                  startTime: Math.floor(this.session.firstPacket / 1000),
                  stopTime: Math.floor(this.session.lastPacket / 1000),
                  openAll: 1
                };

                let url = `sessions?${qs.stringify(params)}`;

                window.location = url;
              },
              /**
               * Adds a rootId expression and applies a new start time
               * @param {string} rootId The root id of the session
               * @param {int} startTime The start time of the session
               */
              allSessions: function (rootId, startTime) {
                startTime = Math.floor(startTime / 1000);

                let fullExpression = `rootId == ${rootId}`;

                this.expression = fullExpression;

                if (this.$route.query.startTime) {
                  if (this.$route.query.startTime < startTime) {
                    startTime = this.$route.query.startTime;
                  }
                }

                this.startTime = startTime;
              },
              /**
               * Opens a new browser tab containing all the unique values for a given field
               * @param {string} fieldID  The field id to display unique values for
               * @param {int} counts      Whether to display the unique values with counts (1 or 0)
               */
              exportUnique: function (fieldID, counts) {
                SessionsService.exportUniqueValues(fieldID, counts, this.$route.query);
              },
              /**
               * Opens the spi graph page in a new browser tab
               * @param {string} fieldID The field id (dbField) to display spi graph data for
               */
              openSpiGraph: function (fieldID) {
                SessionsService.openSpiGraph(fieldID, this.$route.query);
              },
              /**
               * Shows more items in a list of values
               * @param {object} e The click event
               */
              showMoreItems: function (e) {
                e.target.style.display = 'none';
                e.target.previousSibling.style.display = 'inline-block';
              },
              /**
               * Hides more items in a list of values
               * @param {object} e The click event
               */
              showFewerItems: function (e) {
                e.target.parentElement.style.display = 'none';
                e.target.parentElement.nextElementSibling.style.display = 'inline-block';
              }
            },
            components: {
              MolochTagSessions,
              MolochDeleteSessions,
              MolochScrubPcap,
              MolochSendSessions,
              MolochExportPcap,
              MolochToast
            }
          }).$mount(this.$refs.detailContainer);
        })
        .catch((error) => {
          this.loading = false;
          this.error = error;
        });
    },
    getMolochClusters: function () {
      ConfigService.getMolochClusters()
        .then((clusters) => {
          this.molochclusters = clusters;
        });
    },
    getUserSettings: function () {
      UserService.getSettings()
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
    },
    /* sets some of the session detail query parameters based on user settings */
    setUserParams: function () {
      if (localStorage && this.userSettings) { // display user saved options
        if (this.userSettings.detailFormat === 'last' && localStorage['moloch-base']) {
          this.params.base = localStorage['moloch-base'];
        } else if (this.userSettings.detailFormat) {
          this.params.base = this.userSettings.detailFormat;
        }

        if (this.userSettings.numPackets === 'last') {
          this.params.packets = localStorage['moloch-packets'] || 200;
        } else {
          this.params.packets = this.userSettings.numPackets || 200;
        }

        if (this.userSettings.showTimestamps === 'last' && localStorage['moloch-ts']) {
          this.params.ts = localStorage['moloch-ts'] === 'true';
        } else if (this.userSettings.showTimestamps) {
          this.params.ts = this.userSettings.showTimestamps === 'on';
        }
      }
    },
    /**
     * retrieves other decodings for packet data
     * IMPORTANT: kicks of packet request
     */
    getDecodings: function () {
      SessionsService.getDecodings()
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
    },
    /* sets some of the session detail query parameters based on browser saved options */
    setBrowserParams: function () {
      if (localStorage) { // display browser saved options
        if (localStorage['moloch-line']) {
          this.params.line = JSON.parse(localStorage['moloch-line']);
        }
        if (localStorage['moloch-gzip']) {
          this.params.gzip = JSON.parse(localStorage['moloch-gzip']);
        }
        if (localStorage['moloch-image']) {
          this.params.image = JSON.parse(localStorage['moloch-image']);
        }
        if (localStorage['moloch-decodings']) {
          this.params.decode = JSON.parse(localStorage['moloch-decodings']);
          for (let key in this.decodings) {
            if (this.decodings.hasOwnProperty(key)) {
              if (this.params.decode[key]) {
                this.decodings[key].active = true;
                for (let field in this.params.decode[key]) {
                  for (let i = 0, len = this.decodings[key].fields.length; i < len; ++i) {
                    if (this.decodings[key].fields[i].key === field) {
                      this.decodings[key].fields[i].value = this.params.decode[key][field];
                    }
                  }
                }
              }
            }
          }
        }
      }
    },
    /* Gets the packets for the session from the server */
    getPackets: function () {
      // already loading, don't load again!
      if (this.loadingPackets) { return; }

      this.loadingPackets = true;
      this.errorPackets = false;

      if (localStorage) { // update browser saved options
        if (this.userSettings.detailFormat === 'last') {
          localStorage['moloch-base'] = this.params.base;
        }
        if (this.userSettings.numPackets === 'last') {
          localStorage['moloch-packets'] = this.params.packets || 200;
        }
        localStorage['moloch-line'] = this.params.line;
        localStorage['moloch-gzip'] = this.params.gzip;
        localStorage['moloch-image'] = this.params.image;
      }

      this.packetPromise = SessionsService.getPackets(
        this.session.id,
        this.session.node,
        this.params
      );

      this.packetPromise.promise
        .then((response) => {
          this.loadingPackets = false;
          this.renderingPackets = true;
          this.packetPromise = undefined;

          // remove all un-whitelisted tokens from the html
          this.packetHtml = sanitizeHtml(response, {
            allowedTags: [ 'h3', 'h4', 'h5', 'h6', 'a', 'b', 'i', 'strong', 'em', 'div', 'pre', 'span', 'br', 'img' ],
            allowedClasses: {
              'div': [ 'row', 'col-md-6', 'offset-md-6', 'sessionsrc', 'sessiondst', 'session-detail-ts', 'alert', 'alert-danger' ],
              'span': [ 'pull-right', 'dstcol', 'srccol', 'fa', 'fa-info-circle', 'fa-lg', 'fa-exclamation-triangle', 'sessionln', 'src-col-tip', 'dst-col-tip' ],
              'em': [ 'ts-value' ],
              'h5': [ 'text-theme-quaternary' ],
              'a': [ 'imagetag', 'file' ]
            },
            allowedAttributes: {
              'div': [ 'value' ],
              'img': [ 'src' ],
              'a': [ 'target', 'href' ]
            }
          });

          setTimeout(() => { // wait until session packets are rendered
            // tooltips for src/dst byte images
            let tss = this.$refs.packetContainer.getElementsByClassName('session-detail-ts');
            for (let i = 0; i < tss.length; ++i) {
              let timeEl = tss[i];
              let value = timeEl.getAttribute('value');
              timeEl = timeEl.getElementsByClassName('ts-value');
              if (!isNaN(value)) { // only parse value if it's a number (ms from 1970)
                let time = this.$options.filters.timezoneDateString(
                  Math.floor(value / 1000),
                  this.userSettings.timezone,
                  'YYYY/MM/DD HH:mm:ss.sss z'
                );
                timeEl[0].innerHTML = time;
              }
            }

            // tooltips for linked images
            let imgs = this.$refs.packetContainer.getElementsByClassName('imagetag');
            for (let i = 0; i < imgs.length; ++i) {
              let img = imgs[i];
              let href = img.getAttribute('href');
              href = href.replace('body', 'bodypng');

              let tooltip = document.createElement('span');
              tooltip.className = 'img-tip';
              tooltip.innerHTML = `File Bytes:
                <br>
                <img src="${href}">
              `;

              img.appendChild(tooltip);
            }

            this.renderingPackets = false;
          });
        })
        .catch((error) => {
          this.loadingPackets = false;
          this.errorPackets = error;
          this.packetPromise = undefined;
        });
    }
  },
  beforeDestroy: function () {
    if (this.packetPromise) {
      this.cancelPacketLoad();
    }
  }
};
</script>

<style>
.session-detail {
  display: block;
  margin-left: var(--px-md);
  margin-right: var(--px-md);
}

.session-detail h4.sessionDetailMeta {
  padding-top: 10px;
  border-top: 1px solid var(--color-gray);
}

.packet-options {
  border-top: var(--color-gray) 1px solid;
}

.packet-container .tooltip .tooltip-inner {
  max-width: 400px;
}

.packet-container .file {
  display: inline-block;
  margin-bottom: 20px;
}

/* image tooltips */
.packet-container .srccol:hover,
.packet-container .dstcol:hover,
.packet-container .imagetag:hover {
  position: relative;
}
.packet-container .srccol .src-col-tip,
.packet-container .dstcol .dst-col-tip,
.packet-container .imagetag .img-tip {
  display: none;
}
.packet-container .srccol:hover .src-col-tip,
.packet-container .dstcol:hover .dst-col-tip,
.packet-container .imagetag:hover .img-tip {
  font-size: 12px;
  display: block;
  position: absolute;
  margin: 10px;
  left: 0;
  top: 20px;
  padding: 4px 6px;
  color: white;
  text-align: center;
  background-color: black;
  border-radius: 5px;
  line-height: 1.2;
  z-index: 2;
}
.packet-container .srccol .src-col-tip:before,
.packet-container .dstcol .dst-col-tip:before,
.packet-container .imagetag .img-tip:before {
  content: '';
  display: block;
  width: 0;
  height: 0;
  position: absolute;
  border-left: 8px solid transparent;
  border-right: 8px solid transparent;
  border-bottom: 8px solid black;
  top: -7px;
  left: 8px;
}

/* timestamps */
.packet-container .session-detail-ts {
  display: none;
  color: var(--color-foreground-accent);
  font-weight: bold;
  padding: 0 4px;
  border-bottom: 1px solid var(--color-gray);
}
.packet-container.show-ts .session-detail-ts {
  display: block !important;
}

/* packets */
.packet-container pre {
  display: block;
  padding: 3px;
  font-size: 12px;
  word-break: break-all;
  word-wrap: break-word;
  border-radius: 0 0 4px 4px;
  margin-top: -1px;
  color: inherit;
  overflow-y: auto;
  white-space: pre-wrap;
}
.packet-container pre .sessionln {
  color: darkgreen;
}
/* src/dst packet text colors */
.packet-container .sessiondst {
  color: var(--color-dst, #0000FF) !important;
}
.packet-container .sessionsrc {
  color: var(--color-src, #CA0404) !important;
}

/* list values */
.session-detail dt {
  float: left;
  clear: left;
  width: 160px;
  text-align: right;
  margin-right: 6px;
  line-height: 1.7;
  font-weight: 600;
}

.session-detail dd {
  margin-left: 165px;
}

/* more items link */
.session-detail .show-more-items {
  margin-left: 6px;
  margin-right: 6px;
  font-size: 12px;
  text-decoration: none;
}

/* top navigation links */
.session-detail .nav-pills {
  border-bottom: 1px solid var(--color-gray);
  padding-bottom: 4px;
  padding-top: 4px;
}

.session-detail .nav-pills li.nav-item a,
.session-detail .nav-pills div.nav-item button {
  color: var(--color-foreground);
  background-color: transparent;
  border: none;
}

.session-detail .nav-pills > li.nav-item > a:hover,
.session-detail .nav-pills > li.nav-item.open > a,
.session-detail .nav-pills > li.nav-item.open > a:hover,
.session-detail .nav-pills > div.nav-item > button:hover,
.session-detail .nav-pills > div.nav-item.open > button,
.session-detail .nav-pills > div.nav-item.open > button:hover  {
  background-color: var(--color-quaternary-lighter);
}

/* clickable labels */
.session-detail .clickable-label {
  margin-top: -2px;
}

.session-detail .clickable-label button.btn {
  height: 23px;
  background-color: transparent;
  font-size: 12px;
  font-weight: 600;
  line-height: 21px;
  padding: 0 5px 1px 5px;
}

.session-detail .clickable-label button.btn:hover {
  color: #333;
  background-color: var(--color-gray);
  border-color: var(--color-gray);
}

.session-detail .clickable-label .dropdown-menu {
  max-height: 280px;
  overflow-y: auto;
}

.session-detail .clickable-label .dropdown-menu .dropdown-item {
  font-size: 12px;
}
</style>
