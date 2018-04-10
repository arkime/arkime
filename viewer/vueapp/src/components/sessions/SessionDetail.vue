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
    <div class="detail-container">
    </div> <!-- /detail -->

    <hr v-if="!loading && !error">

  </div> <!-- /session detail -->

</template>

<script>
import UserService from '../UserService';
import SessionsService from './SessionsService';

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
    /* helper functions ---------------------------------------------------- */
    /**
     * Gets the session detail from the server
     * @param {string} message An optional message to display to the user
     */
    getDetailData: function (message) {
      this.loading = true;

      SessionsService.getDetail(this.session.id, this.session.node)
        .then((response) => {
          this.loading = false;
          // TODO
          console.log(response);
          // this.detailHtml = this.$sce.trustAsHtml(response.data);
          // this.$scope.renderDetail();
          // if (message) { this.displayMessage(message); }
        })
        .catch((error) => {
          this.loading = false;
          this.error = error;
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
          console.log(response);
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
