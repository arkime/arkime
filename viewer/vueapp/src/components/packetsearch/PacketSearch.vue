<template>

  <div class="packet-search-page">

    <!-- search navbar -->
    <moloch-search
      :start="query.start"
      :timezone="settings.timezone"
      @changeSearch="loadData">
    </moloch-search> <!-- /search navbar -->

    <div>
      packet search content goes here
    </div>

    <!-- loading overlay -->
    <moloch-loading
      v-if="loading && !error">
    </moloch-loading> <!-- /loading overlay -->

    <!-- page error -->
    <moloch-error
      v-if="error"
      :message="error"
      class="mt-5 mb-5">
    </moloch-error> <!-- /page error -->

    <!-- packet search jobs content -->
    <div v-if="!error"
      class="mt-5">

      <table class="table table-sm table-striped ml-1 mr-1">
        <thead>
          <tr>
            <th>Status</th>
            <th>Name</th>
            <th>Search text</th>
            <th>What</th>
            <th>Type</th>
            <th>Actions</th>
          </tr>
        </thead>
        <tbody>
          <!-- create new packet search job -->
          <tr>
            <td>&nbsp;</td>
            <td>
              <!-- packet search job name -->
              <input type="text"
                placeholder="Name your packet search job"
                class="mt-2 form-control form-control-sm"
              /> <!-- /packet search job name -->
            </td>
            <td>
              <!-- packet search job text -->
              <div class="mt-2 input-group input-group-sm">
                <span class="input-group-prepend cursor-help"
                  v-b-tooltip.hover
                  title="Search for this text in packets">
                  <span class="input-group-text">
                    <span class="fa fa-search"></span>
                  </span>
                </span>
                <input type="text"
                  style="min-width:300px;"
                  placeholder="Search packet for"
                  class="form-control"
                />
                <span class="input-group-append">
                  <!-- packet search text type -->
                  <button class="btn btn-outline-secondary"
                    type="button">
                    ascii
                  </button>
                  <button class="btn btn-outline-secondary"
                    type="button">
                    hex
                  </button>
                  <button class="btn btn-outline-secondary"
                    type="button">
                    wildcard
                  </button>
                  <button class="btn btn-outline-secondary"
                    type="button">
                    regex
                  </button> <!-- /packet search text type -->
                </span>
              </div> <!-- /packet search job text -->
            </td>
            <td>
              <!-- TODO checkbox styles -->
              <!-- packet search what -->
              <b-form-group class="mt-2">
                <b-form-checkbox-group
                  size="sm"
                  buttons>
                  <b-checkbox value="src"
                    v-b-tooltip.hover
                    title="search src packets"
                    class="btn-checkbox">
                    src
                  </b-checkbox>
                  <b-checkbox value="dst"
                    v-b-tooltip.hover
                    title="search dst packets"
                    class="btn-checkbox">
                    dst
                  </b-checkbox>
                </b-form-checkbox-group>
              </b-form-group> <!-- /packet search what -->
            </td>
            <td>
              <!-- packet search type -->
              <b-form-group class="mt-2">
                <b-form-checkbox-group
                  size="sm"
                  buttons>
                  <b-checkbox value="src"
                    v-b-tooltip.hover
                    title="search raw packets"
                    class="btn-checkbox">
                    raw
                  </b-checkbox>
                  <b-checkbox value="dst"
                    v-b-tooltip.hover
                    title="search reassembled/payload packets"
                    class="btn-checkbox">
                    reassembled
                  </b-checkbox>
                </b-form-checkbox-group>
              </b-form-group> <!-- /packet search type -->
            </td>
            <td>
              <!-- create search job button -->
              <button type="button"
                class="mt-2 pull-right btn btn-sm btn-theme-tertiary">
                <span class="fa fa-plus">
                </span>&nbsp;
                Create
              </button> <!-- /create search job button -->
            </td>
          </tr> <!-- /create new packet search job -->
          <!-- packet search jobs -->
          <tr v-if="results.length"
            v-for="result in results"
            :key="result.id">
            <td>
              <span v-if="result.status === 'running'"
                v-b-tooltip.hover
                title="Running"
                class="fa fa-spinner fa-spin">
              </span>
              <span v-else
                v-b-tooltip.hover
                title="Finished"
                class="fa fa-check">
              </span>
              <!-- TODO show number of matches and how many searched for alllllllll -->
              <!-- TODO paused, queued -->
            </td>
            <td>
              {{ result.name }} - {{ result.user }}
            </td>
            <td>
              {{ result.text }} ({{ result.textType }})
            </td>
            <td>
              {{ result.what }}
            </td>
            <td>
              {{ result.type }}
            </td>
            <td>
              <button v-if="result.status === 'running'"
                type="button"
                v-b-tooltip.hover
                title="cancel this packet search job"
                class="ml-1 pull-right btn btn-sm btn-warning">
                <span class="fa fa-ban">
                </span>
              </button>
              <button type="button"
                v-b-tooltip.hover
                title="go to results"
                class="pull-right btn btn-sm btn-theme-primary">
                <span class="fa fa-folder-open">
                </span>
              </button>
            </td>
          </tr> <!-- /packet search jobs -->
        </tbody>
      </table>

      <!-- no results -->
      <div v-if="!loading && !results.length"
        class="ml-1 mr-1">
        <div class="mb-5 info-area vertical-horizontal-center">
          <div>
            <span class="fa fa-3x text-muted-more fa-folder-open">
            </span>&nbsp;
            There are currently no packet search jobs.
            <br>
            Fill out the form above to create one.
          </div>
        </div>
      </div> <!-- /no results -->

    </div> <!-- /packet search jobs content -->

  </div>

</template>

<script>
import UserService from '../users/UserService';
import MolochSearch from '../search/Search';
import MolochError from '../utils/Error';
import MolochLoading from '../utils/Loading';
import MolochNoResults from '../utils/NoResults';

export default {
  name: 'PacketSearch',
  data: function () {
    return {
      error: '',
      loading: true,
      results: [], // page data
      settings: {} // user settings
    };
  },
  computed: {
    query: function () {
      return { // query defaults
        length: this.$route.query.length || 50, // page length
        start: 0, // first item index
        date: this.$store.state.timeRange,
        startTime: this.$store.state.time.startTime,
        stopTime: this.$store.state.time.stopTime,
        bounding: this.$route.query.bounding || 'last',
        interval: this.$route.query.interval || 'auto',
        expression: this.$store.state.expression || undefined
      };
    }
  },
  components: {
    MolochSearch,
    MolochLoading,
    MolochError,
    MolochNoResults
  },
  mounted: function () {
    this.getUserSettings();
    this.loadData();
  },
  methods: {
    loadData: function () {
      setTimeout(() => {
        this.loading = false;

        this.results = [
          {
            id: 0,
            status: 'running',
            name: 'packet search 1',
            user: 'andy',
            text: 'super secret',
            textType: 'ascii',
            what: 'src',
            type: 'raw',
            matches: 0,
            searched: 666
          },
          {
            id: 1,
            status: 'queued',
            name: 'packet search 2',
            user: 'andy',
            text: 'super*secret',
            textType: 'wildcard',
            what: 'dst',
            type: 'raw',
            matches: 0,
            searched: 0
          },
          {
            id: 2,
            status: 'paused',
            name: 'packety searchy jobby A',
            user: 'elyse',
            text: '/fu/g',
            textType: 'regex',
            what: 'both',
            type: 'reassembled',
            matches: 2,
            searched: 42395
          },
          {
            id: 3,
            status: 'finished',
            name: 'packety searchy jobby B',
            user: 'elyse',
            text: '737570657220736563726574',
            textType: 'hex',
            what: 'both',
            type: 'reassembled',
            matches: 50,
            searched: 845393475
          }
        ];
      }, 2000);
    },
    /* helper functions ---------------------------------------------------- */
    getUserSettings: function () {
      UserService.getCurrent()
        .then((response) => {
          this.settings = response.settings;
        }, (error) => {
          this.settings = { timezone: 'local' };
          this.error = error;
        });
    }
  }
};
</script>

<style scoped>
/* packet search page, navbar, and content styles */
.packet-search-page {
  margin-top: 36px;
}

.info-area {
  font-size: var(--px-xxlg);
}
</style>
