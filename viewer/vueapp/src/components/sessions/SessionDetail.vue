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

    <hr v-if="!loading && !error">

  </div> <!-- /session detail -->

</template>

<script>
import Vue from 'vue';
import UserService from '../UserService';
import ConfigService from '../utils/ConfigService';
import SessionsService from './SessionsService';
import FieldService from '../search/FieldService';

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
  created: function () {
    console.log('session detail time!', this.session.id);
    this.getUserSettings();
    this.getDetailData();
    // TODO get moloch molochClickables
    // TODO get moloch fields
    // TODO listen for open/close form container
  },
  methods: {
    /* exposed functions --------------------------------------------------- */
    // TODO allSessions
    // TODO openPermalink
    // TODO session detail actions menu
    // TODO show more/fewer items
    // TODO exportUnique
    // TODO openSpiGraph
    // TODO displayFormContainer
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
            props: [ 'session', 'fields', 'molochclusters' ]
          }).$mount(this.$refs.detailContainer);
        }); // TODO catch
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

        if (this.userSettings.numPackets === 'last' && localStorage['moloch-packets']) {
          this.params.packets = localStorage['moloch-packets'];
        } else if (this.userSettings.numPackets) {
          this.params.packets = this.userSettings.numPackets;
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
                for (let field in this.$scope.params.decode[key]) {
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
          this.packetPromise = undefined;

          // TODO
          // console.log(response);
          // this.packetHtml = this.$sce.trustAsHtml(response.data);
          // remove all un-whitelisted tokens from the html
          // this.packetHtml = this.$sanitize(this.$scope.packetHtml);
          // this.renderPackets();
        })
        .catch((error) => {
          this.loadingPackets = false;
          this.errorPackets = error;
          this.packetPromise = undefined;
        });
    }
  }
};
</script>

<style>
.sessionDetail {
  display: block;
  margin-left: var(--px-md);
  margin-right: var(--px-md);
}

.sessionDetail hr {
  border-color: var(--color-gray);
}

.sessionDetail h4.sessionDetailMeta {
  padding-top: 10px;
  border-top: 1px solid var(--color-gray);
}

.sessionDetail .packet-options .btn.active {
  font-weight: bolder;
}

.sessionDetail .packet-container .tooltip .tooltip-inner {
  max-width: 400px;
}

.sessionDetail .packet-container .file {
  display: inline-block;
  margin-bottom: 20px;
}

/* src/dst packet text colors */
.sessionDetail .sessiondst {
  color: var(--color-dst, #0000FF) !important;
}

.sessionDetail .sessionsrc {
  color: var(--color-src, #CA0404) !important;
}

/* timestamps */
.sessionDetail .session-detail-ts {
  display: none;
  color: var(--color-foreground-accent);
  font-weight: bold;
  padding: 0 4px;
  margin-left: 2px;
  margin-right: 2px;
  border-bottom: 1px solid var(--color-gray);
}

.sessionDetail .session-detail-ts + pre {
  color: inherit;
  margin-top: -1px;
}

.sessionDetail .session-detail-ts + pre .sessionln {
  color: darkgreen;
}

.sessionDetail .sessionDetail.show-ts ~ .packet-container .session-detail-ts {
  display: block;
}

/* list values */
.sessionDetail dt {
  float: left;
  clear: left;
  width: 160px;
  text-align: right;
  margin-right: 6px;
  line-height: 1.7;
}

.sessionDetail dd {
  margin-left: 165px;
  margin-top: 10px;
}

/* more items link */
.sessionDetail .show-more-items {
  margin-left: 6px;
  margin-right: 6px;
  font-size: 12px;
  text-decoration: none;
}

/* top navigation links */
.sessionDetail .nav-pills {
  border-bottom: 1px solid var(--color-gray);
  padding-bottom: 4px;
  padding-top: 4px;
}

.sessionDetail .nav-pills li.nav-item a,
.sessionDetail .nav-pills div.nav-item button {
  color: var(--color-foreground);
  background-color: transparent;
  border: none;
}

.sessionDetail .nav-pills > li.nav-item > a:hover,
.sessionDetail .nav-pills > li.nav-item.open > a,
.sessionDetail .nav-pills > li.nav-item.open > a:hover,
.sessionDetail .nav-pills > div.nav-item > button:hover,
.sessionDetail .nav-pills > div.nav-item.open > button,
.sessionDetail .nav-pills > div.nav-item.open > button:hover  {
  background-color: var(--color-quaternary-lighter);
}

/* clickable labels */
.sessionDetail .clickable-label {
  margin-top: -2px;
}

.sessionDetail .clickable-label button.btn {
  height: 23px;
  background-color: transparent;
  font-size: 12px;
  line-height: 21px;
  padding: 1px 5px;
}

.sessionDetail .clickable-label button.btn:hover {
  color: #333;
  background-color: var(--color-gray);
  border-color: var(--color-gray);
}

.sessionDetail .clickable-label .dropdown-menu {
  max-height: 280px;
  overflow-y: auto;
}

.sessionDetail .clickable-label .dropdown-menu .dropdown-item {
  font-size: 12px;
}
</style>
