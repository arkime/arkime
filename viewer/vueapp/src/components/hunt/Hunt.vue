<template>

  <div class="packet-search-page">

    <!-- search navbar -->
    <moloch-search
      :start="query.start"
      :timezone="settings.timezone"
      @changeSearch="loadSessions">
    </moloch-search> <!-- /search navbar -->

    <div>&nbsp;</div>

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
      class="mt-5 ml-2 mr-2">

      <!-- create new packet search job -->
      <div class="mb-3">
        <br>
        <button type="button"
          v-if="!createFormOpened"
          @click="createFormOpened = true"
          class="btn btn-theme-tertiary btn-lg btn-block">
          Create a new packet search job
        </button>
        <form v-if="createFormOpened"
          class="mt-2">
          <div class="row">
            <div class="col-12">
              <div class="alert alert-info">
                <span class="fa fa-info-circle fa-fw">
                </span>&nbsp;
                This packet search job will search
                <strong>
                  {{ sessions.recordsFiltered | commaString }}
                </strong>
                sessions.
                <em v-if="sessions.recordsFiltered > 10000">
                  That's a lot of sessions, this job will take a while.
                  <strong>
                    Proceed with caution.
                  </strong>
                </em>
                <br>
                <span class="fa fa-exclamation-triangle fa-fw">
                </span>&nbsp;
                Make sure your search above contains only the sessions that
                you want in your packet search!
              </div>
            </div>
          </div>
          <div class="row">
            <div class="form-group col-lg-6 col-md-12">
              <!-- packet search job name -->
              <div class="input-group input-group-sm">
                <span class="input-group-prepend"
                  v-b-tooltip.hover
                  title="Give your packet search job a short, unique name">
                  <span class="input-group-text">
                    Name
                  </span>
                </span>
                <input type="text"
                  v-model="jobName"
                  placeholder="Name your packet search job"
                  class="form-control"
                  maxlength="40"
                />
              </div> <!-- /packet search job name -->
            </div>
            <!-- packet search size -->
            <div class="form-group col-lg-6 col-md-12">
              <div class="input-group input-group-sm">
                <span class="input-group-prepend">
                  <span class="input-group-text">
                    Max number of packets to examine per session
                  </span>
                </span>
                <select class="form-control"
                  v-model="jobSize"
                  style="-webkit-appearance: none;">
                  <option value="50">50 packets</option>
                  <option value="500">500 packets</option>
                  <option value="5000">5000 packets</option>
                  <option value="-1">All packets</option>
                </select>
              </div>
            </div> <!-- /packet search size -->
          </div>
          <div class="row">
            <!-- packet search text & text type -->
            <div class="form-group col-lg-6 col-md-12">
              <div class="input-group input-group-sm">
                <span class="input-group-prepend cursor-help"
                  v-b-tooltip.hover
                  title="Search for this text in packets">
                  <span class="input-group-text">
                    <span class="fa fa-search">
                    </span>
                  </span>
                </span>
                <input type="text"
                  v-model="jobSearch"
                  placeholder="Search packets for"
                  class="form-control"
                />
              </div>
              <div class="form-check form-check-inline">
                <input class="form-check-input"
                  :checked="jobSearchType === 'ascii'"
                  @click="setJobSearchType('ascii')"
                  type="radio"
                  id="ascii"
                  value="ascii"
                  name="packetSearchTextType"
                />
                <label class="form-check-label"
                  for="ascii">
                  ascii
                </label>
              </div>
              <div class="form-check form-check-inline">
                <input class="form-check-input"
                  :checked="jobSearchType === 'hex'"
                  @click="setJobSearchType('hex')"
                  type="radio"
                  id="hex"
                  value="hex"
                  name="packetSearchTextType"
                />
                <label class="form-check-label"
                  for="hex">
                  hex
                </label>
              </div>
              <div class="form-check form-check-inline">
                <input class="form-check-input"
                  :checked="jobSearchType === 'wildcard'"
                  @click="setJobSearchType('wildcard')"
                  type="radio"
                  id="wildcard"
                  value="wildcard"
                  name="packetSearchTextType"
                />
                <label class="form-check-label"
                  for="wildcard">
                  wildcard
                </label>
              </div>
              <div class="form-check form-check-inline">
                <input class="form-check-input"
                  :checked="jobSearchType === 'regex'"
                  @click="setJobSearchType('regex')"
                  type="radio"
                  id="regex"
                  value="regex"
                  name="packetSearchTextType"
                />
                <label class="form-check-label"
                  for="regex">
                  regex
                </label>
              </div>
            </div> <!-- /packet search text & text type -->
            <!-- packet search direction -->
            <div class="form-group col-lg-3 col-md-12">
              <div class="form-check">
                <input class="form-check-input"
                  :checked="jobSrc"
                  @click="jobSrc = !jobSrc"
                  type="checkbox"
                  id="src"
                  value="src"
                />
                <label class="form-check-label"
                  for="src">
                  search src packets
                </label>
              </div>
              <div class="form-check">
                <input class="form-check-input"
                  :checked="jobDst"
                  @click="jobDst = !jobDst"
                  type="checkbox"
                  id="dst"
                  value="dst"
                />
                <label class="form-check-label"
                  for="dst">
                  search dst packets
                </label>
              </div>
            </div> <!-- /packet search direction -->
            <!-- packet search type -->
            <div class="form-group col-lg-3 col-md-12">
              <div class="form-check">
                <input class="form-check-input"
                  :checked="jobType === 'raw'"
                  @click="setJobType('raw')"
                  type="radio"
                  id="raw"
                  value="raw"
                  name="packetSearchType"
                />
                <label class="form-check-label"
                  for="raw">
                  search raw packets
                </label>
              </div>
              <div class="form-check">
                <input class="form-check-input"
                  :checked="jobType === 'reassembled'"
                  @click="setJobType('raw')"
                  type="radio"
                  id="reassembled"
                  value="reassembled"
                  name="packetSearchType"
                />
                <label class="form-check-label"
                  for="reassembled">
                  search reassembled packets
                </label>
              </div>
            </div> <!-- /packet search type -->
          </div>
          <div class="row mb-4">
            <div class="col-12 mt-1">
              <!-- create search job button -->
              <button type="button"
                @click="createJob"
                class="pull-right btn btn-theme-tertiary pull-right ml-1">
                <span class="fa fa-plus">
                </span>&nbsp;
                Create
              </button> <!-- /create search job button -->
              <!-- cancel create search job button -->
              <button type="button"
                @click="createFormOpened = false"
                class="pull-right btn btn-warning pull-right">
                <span class="fa fa-ban">
                </span>&nbsp;
                Cancel
              </button> <!-- /cancel create search job button -->
            </div>
          </div>
        </form>
      </div> <!-- /create new packet search job -->

      <!-- search packet search jobs -->
      <div class="form-group">
        <div class="input-group input-group-sm">
          <span class="input-group-prepend cursor-help">
            <span class="input-group-text">
              <span class="fa fa-search">
              </span>
            </span>
          </span>
          <input type="text"
            placeholder="Search packet search jobs"
            class="form-control"
          />
        </div>
      </div> <!-- /search packet search jobs -->

      <table class="table table-sm table-striped">
        <thead>
          <tr>
            <th width="40px">&nbsp;</th>
            <th class="cursor-pointer">
              Status
              <span class="fa fa-sort">
              </span>
            </th>
            <th class="cursor-pointer">
              Name
              <span class="fa fa-sort">
              </span>
            </th>
            <th class="cursor-pointer">
              User
              <span class="fa fa-sort">
              </span>
            </th>
            <th>
              Search text
            </th>
            <th class="cursor-pointer">
              Created
              <span class="fa fa-sort-desc">
              </span>
            </th>
            <th width="120px">&nbsp;</th>
          </tr>
        </thead>
        <tbody>
          <!-- packet search jobs -->
          <tr v-if="results.length"
            v-for="job in results"
            :key="job.id">
            <!-- TODO more info (direction & type) -->
            <td>
              <toggle-btn class="mt-1"
                :opened="job.expanded"
                @toggle="toggleJobDetail(job)">
              </toggle-btn>
            </td>
            <td>
              <span v-if="job.status === 'running'"
                v-b-tooltip.hover
                title="Running"
                class="fa fa-spinner fa-spin fa-fw cursor-help">
              </span>
              <span v-else-if="job.status === 'paused'"
                v-b-tooltip.hover
                title="Paused"
                class="fa fa-pause fa-fw cursor-help">
              </span>
              <span v-else-if="job.status === 'queued'"
                v-b-tooltip.hover
                title="Queued"
                class="fa fa-clock-o fa-fw cursor-help">
              </span>
              <span v-else-if="job.status === 'finished'"
                v-b-tooltip.hover
                title="Finished"
                class="fa fa-check fa-fw cursor-help">
              </span>
              &nbsp;
              <span class="badge badge-secondary cursor-help"
                :id="`jobmatches${job.id}`">
                {{ ((job.searched / job.numSessions) * 100) | round(1) }}%
              </span>
              <b-tooltip :target="`jobmatches${job.id}`">
                Found {{ job.matches }} out of {{ job.searched | commaString }} sessions searched.
                <div v-if="job.status !== 'finished'">
                  Still need to search {{ (job.numSessions - job.searched) | commaString }} sessions.
                </div>
              </b-tooltip>
            </td>
            <td>
              {{ job.name }}
            </td>
            <td>
              {{ job.userId }}
            </td>
            <td>
              {{ job.search }} ({{ job.searchType }})
            </td>
            <td>
              {{ job.created | timezoneDateString(settings.timezone, 'YYYY/MM/DD HH:mm:ss z') }}
            </td>
            <td>
              <button
                type="button"
                v-b-tooltip.hover
                title="Remove this job from history"
                class="ml-1 pull-right btn btn-sm btn-danger">
                <span class="fa fa-trash-o">
                </span>
              </button>
              <button type="button"
                v-if="job.matches"
                :id="`openresults${job.id}`"
                class="ml-1 pull-right btn btn-sm btn-theme-primary">
                <span class="fa fa-folder-open">
                </span>
              </button>
              <b-tooltip :target="`openresults${job.id}`">
                <span v-if="job.status === 'finished'">
                  Open results in a new Sessions tab
                </span>
                <span v-else>
                  Open <strong>partial</strong> results in a new Sessions tab
                </span>
              </b-tooltip>
              <button v-if="job.status === 'running'"
                type="button"
                v-b-tooltip.hover
                title="Pause this job"
                class="pull-right btn btn-sm btn-warning">
                <span class="fa fa-pause">
                </span>
              </button>
              <button v-if="job.status === 'paused'"
                type="button"
                v-b-tooltip.hover
                title="Play this job"
                class="pull-right btn btn-sm btn-theme-secondary">
                <span class="fa fa-play">
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
import SessionsService from '../sessions/SessionsService';
import ToggleBtn from '../utils/ToggleBtn';
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
      settings: {}, // user settings
      sessions: {},
      createFormOpened: false,
      // new job search default values
      jobName: '',
      jobSize: 50,
      jobSearch: '',
      jobSearchType: 'ascii',
      jobSrc: true,
      jobDst: true,
      jobType: 'raw'
    };
  },
  computed: {
    query: function () {
      return { // query defaults
        length: this.$route.query.length || 50, // page length
        start: 0, // first item index
        facets: 1,
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
    ToggleBtn,
    MolochSearch,
    MolochLoading,
    MolochError,
    MolochNoResults
  },
  mounted: function () {
    this.getUserSettings();
    this.loadSessions();
    this.loadData();
    // TODO interval to load data
  },
  methods: {
    createJob: function () {
      this.createFormOpened = false;

      // TODO validate inputs
      let newJob = {
        name: this.jobName,
        size: this.jobSize,
        search: this.jobSearch,
        searchType: this.jobSearchType,
        type: this.jobType,
        src: this.jobSrc,
        dst: this.jobDst
      };

      this.axios.post('hunt', { hunt: newJob })
        .then((response) => {
          console.log(response);
        }, (error) => {
          console.log(error);
        });
    },
    setJobSearchType: function (val) {
      this.jobSearchType = val;
    },
    setJobType: function (val) {
      this.jobType = val;
    },
    toggleJobDetail: function (job) {
      this.$set(job, 'expanded', !job.expanded);
    },
    loadData: function () {
      this.axios.get('hunt/list')
        .then((response) => {
          this.results = response.data.data;
          this.loading = false;
        }, (error) => {
          this.error = error;
        });
    },
    /* helper functions ---------------------------------------------------- */
    getUserSettings: function () {
      UserService.getCurrent()
        .then((response) => {
          this.settings = response.settings;
        }, (error) => {
          this.settings = { timezone: 'local' };
          this.error = error.text || error;
        });
    },
    loadSessions: function () {
      SessionsService.get(this.query)
        .then((response) => {
          this.sessions = response.data;
        })
        .catch((error) => {
          this.sessions = undefined;
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
