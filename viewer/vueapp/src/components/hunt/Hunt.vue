<template>

  <div class="packet-search-page">

    <!-- search navbar -->
    <moloch-search
      v-if="user.settings"
      :start="sessionsQuery.start"
      :timezone="user.settings.timezone"
      :hide-actions="true"
      :hide-interval="true"
      @changeSearch="loadSessions">
    </moloch-search> <!-- /search navbar -->

    <div>&nbsp;</div>

    <!-- loading overlay -->
    <moloch-loading
      v-if="loading && !pageError">
    </moloch-loading> <!-- /loading overlay -->

    <!-- page error -->
    <moloch-error
      v-if="pageError"
      :message="pageError"
      class="mt-5 mb-5">
    </moloch-error> <!-- /page error -->

    <!-- packet search jobs content -->
    <div v-if="!pageError"
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
        <div v-if="createFormOpened"
          class="card">
          <form class="card-body">
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
                    :checked="jobSearchType === 'asciicase'"
                    @click="setJobSearchType('asciicase')"
                    type="radio"
                    id="asciicase"
                    value="asciicase"
                    name="packetSearchTextType"
                  />
                  <label class="form-check-label"
                    for="asciicase">
                    ascii (case sensitive)
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
                <!-- <div class="form-check form-check-inline">
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
                </div> -->
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
                    @click="setJobType('reassembled')"
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
            <div class="row">
              <div class="col-12 mt-1">
                <div v-if="createFormError"
                  class="pull-left alert alert-danger alert-sm">
                  <span class="fa fa-exclamation-triangle">
                  </span>&nbsp;
                  {{ createFormError }}
                </div>
                <!-- create search job button -->
                <button type="button"
                  @click="createJob"
                  class="pull-right btn btn-theme-tertiary pull-right ml-1">
                  <span class="fa fa-plus fa-fw">
                  </span>&nbsp;
                  Create
                </button> <!-- /create search job button -->
                <!-- cancel create search job button -->
                <button type="button"
                  @click="cancelCreateForm"
                  class="pull-right btn btn-warning pull-right">
                  <span class="fa fa-ban fa-fw">
                  </span>&nbsp;
                  Cancel
                </button> <!-- /cancel create search job button -->
              </div>
            </div>
          </form>
        </div>
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
            v-model="query.searchTerm"
            @input="debounceSearch"
            placeholder="Search packet search jobs"
            class="form-control"
          />
        </div>
      </div> <!-- /search packet search jobs -->

      <table class="table table-sm table-striped">
        <thead>
          <tr>
            <th width="40px">&nbsp;</th>
            <th class="cursor-pointer"
              @click="columnClick('status')">
              Status
              <span v-show="query.sortField === 'status' && !query.desc" class="fa fa-sort-asc"></span>
              <span v-show="query.sortField === 'status' && query.desc" class="fa fa-sort-desc"></span>
              <span v-show="query.sortField !== 'status'" class="fa fa-sort"></span>
            </th>
            <th class="cursor-pointer"
              @click="columnClick('name')">
              Name
              <span v-show="query.sortField === 'name' && !query.desc" class="fa fa-sort-asc"></span>
              <span v-show="query.sortField === 'name' && query.desc" class="fa fa-sort-desc"></span>
              <span v-show="query.sortField !== 'name'" class="fa fa-sort"></span>
            </th>
            <th class="cursor-pointer"
              @click="columnClick('userId')">
              User
              <span v-show="query.sortField === 'userId' && !query.desc" class="fa fa-sort-asc"></span>
              <span v-show="query.sortField === 'userId' && query.desc" class="fa fa-sort-desc"></span>
              <span v-show="query.sortField !== 'userId'" class="fa fa-sort"></span>
            </th>
            <th>
              Search text
            </th>
            <th class="cursor-pointer"
              @click="columnClick('created')">
              Created
              <span v-show="query.sortField === 'created' && !query.desc" class="fa fa-sort-asc"></span>
              <span v-show="query.sortField === 'created' && query.desc" class="fa fa-sort-desc"></span>
              <span v-show="query.sortField !== 'created'" class="fa fa-sort"></span>
            </th>
            <th width="120px">&nbsp;</th>
          </tr>
        </thead>
        <tbody>
          <!-- packet search jobs -->
          <template v-if="results.length"
            v-for="job in results">
            <tr :key="`${job.id}-row`">
              <td>
                <toggle-btn
                  v-if="user.userId === job.userId || user.createEnabled"
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
                  {{ ((job.searchedSessions / job.totalSessions) * 100) | round(1) }}%
                </span>
                <b-tooltip :target="`jobmatches${job.id}`">
                  Found {{ job.matchedSessions }} out of {{ job.searchedSessions | commaString }} sessions searched.
                  <div v-if="job.status !== 'finished'">
                    Still need to search {{ (job.totalSessions - job.searchedSessions) | commaString }} sessions.
                  </div>
                </b-tooltip>
                <span v-if="job.errors && job.errors.length"
                  class="badge badge-danger cursor-help">
                  <span class="fa fa-exclamation-triangle"
                    v-b-tooltip.hover
                    title="Errors were encountered while running this hunt job. Open the job to view the error details.">
                  </span>
                </span>
              </td>
              <td>
                {{ job.name }}
              </td>
              <td>
                {{ job.userId }}
              </td>
              <td>
                <span v-if="user.userId === job.userId || user.createEnabled">
                  {{ job.search }} ({{ job.searchType }})
                </span>
              </td>
              <td>
                {{ job.created | timezoneDateString(user.settings.timezone, 'YYYY/MM/DD HH:mm:ss z') }}
              </td>
              <td>
                <small v-if="job.error"
                  class="text-danger">
                  {{ job.error }}
                </small>
                <span v-if="user.userId === job.userId || user.createEnabled">
                  <button
                    @click="removeJob(job)"
                    type="button"
                    v-b-tooltip.hover
                    title="Remove this job from history"
                    class="ml-1 pull-right btn btn-sm btn-danger">
                    <span class="fa fa-trash-o fa-fw">
                    </span>
                  </button>
                  <button type="button"
                    v-if="job.matchedSessions"
                    :id="`openresults${job.id}`"
                    class="ml-1 pull-right btn btn-sm btn-theme-primary">
                    <span class="fa fa-folder-open fa-fw">
                    </span>
                  </button>
                  <b-tooltip v-if="job.matchedSessions"
                    :target="`openresults${job.id}`">
                    <span v-if="job.status === 'finished'">
                      Open results in a new Sessions tab
                    </span>
                    <span v-else>
                      Open <strong>partial</strong> results in a new Sessions tab
                    </span>
                  </b-tooltip>
                  <button v-if="job.status === 'running'"
                    @click="pauseJob(job)"
                    type="button"
                    v-b-tooltip.hover
                    title="Pause this job"
                    class="pull-right btn btn-sm btn-warning">
                    <span class="fa fa-pause fa-fw">
                    </span>
                  </button>
                  <button v-if="job.status === 'paused'"
                    @click="playJob(job)"
                    type="button"
                    v-b-tooltip.hover
                    title="Play this job"
                    class="pull-right btn btn-sm btn-theme-secondary">
                    <span class="fa fa-play fa-fw">
                    </span>
                  </button>
                </span>
              </td>
            </tr>
            <tr :key="`${job.id}-detail`"
              v-if="job.expanded">
              <td colspan="7">
                <div class="row">
                  <div class="col-12">
                    This hunt is
                    <strong>{{ job.status }}</strong>
                  </div>
                </div>
                <div v-if="job.lastUpdated"
                  class="row">
                  <div class="col-12">
                    This hunt was last updated at:
                    <strong>
                      {{ job.lastUpdated | timezoneDateString(user.settings.timezone, 'YYYY/MM/DD HH:mm:ss z') }}
                    </strong>
                  </div>
                </div>
                <div class="row">
                  <div class="col-12">
                    Examining
                    <strong v-if="job.size > 0">{{ job.size }}</strong>
                    <strong v-else>all</strong>
                    <strong>{{ job.type }}</strong>
                    <strong v-if="job.src">source</strong>
                    <span v-if="job.src && job.dst">
                      and
                    </span>
                    <strong v-if="job.dst">destination</strong>
                    packets per session
                  </div>
                </div>
                <div class="row">
                  <div class="col-12">
                    <label>Found</label>
                    <strong>{{ job.matchedSessions }}</strong> of
                    <strong>{{ job.searchedSessions }}</strong>
                    searched sessions out of
                    <strong>{{ job.totalSessions }}</strong>
                    total sessions to search
                  </div>
                </div>
                <div v-if="job.errors"
                  v-for="(error, index) in job.errors"
                  :key="index"
                  class="row text-danger">
                  <div class="col-12">
                    <span class="fa fa-exclamation-triangle">
                    </span>&nbsp;
                    {{ error.value }}
                  </div>
                </div>
              </td>
            </tr>
          </template> <!-- /packet search jobs -->
        </tbody>
      </table>

      <!-- no results -->
      <div v-if="!loading && !results.length"
        class="ml-1 mr-1">
        <div class="mb-5 info-area vertical-horizontal-center">
          <div>
            <span class="fa fa-3x text-muted-more fa-folder-open">
            </span>&nbsp;
            <span v-if="!query.searchTerm">
              There are currently no packet search jobs.
              <br>
              Fill out the form above to create one.
            </span>
            <span v-else>
              There are no packet search jobs that match your search.
            </span>
          </div>
        </div>
      </div> <!-- /no results -->

    </div> <!-- /packet search jobs content -->

  </div>

</template>

<script>
import SessionsService from '../sessions/SessionsService';
import ToggleBtn from '../utils/ToggleBtn';
import MolochSearch from '../search/Search';
import MolochError from '../utils/Error';
import MolochLoading from '../utils/Loading';
import MolochNoResults from '../utils/NoResults';

let timeout;
let interval;

export default {
  name: 'PacketSearch',
  data: function () {
    return {
      pageError: '',
      loading: true,
      results: [], // page data
      sessions: {}, // sessions a new job applies to
      // new job search form
      createFormError: '',
      createFormOpened: false,
      // new job search default values
      jobName: '',
      jobSize: 50,
      jobSearch: '',
      jobSearchType: 'ascii',
      jobSrc: true,
      jobDst: true,
      jobType: 'raw',
      // packet search job search query
      query: {
        sortField: 'created',
        desc: false,
        searchTerm: ''
      }
    };
  },
  computed: {
    sessionsQuery: function () {
      return { // sessions query defaults
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
    },
    user: function () {
      return this.$store.state.user;
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
    this.loadData();
    setTimeout(() => {
      // wait for computed query
      this.loadSessions();
    });

    // interval to load jobs every 15 seconds
    interval = setInterval(() => {
      this.loadData();
    }, 15000);
  },
  methods: {
    cancelCreateForm: function () {
      this.createFormError = '';
      this.createFormOpened = false;
    },
    createJob: function () {
      this.createFormError = '';

      if (!this.sessions.recordsFiltered) {
        this.createFormError = 'This hunt applies to no sessions. Try searching for sessions first.';
        return;
      }
      if (!this.jobName) {
        this.createFormError = 'Job name required';
        return;
      }
      if (!this.jobSearch) {
        this.createFormError = 'Job search text required';
        return;
      }
      if (!this.jobSrc && !this.jobDst) {
        this.createFormError = 'The packet search job must search source or destination packets (or both)';
        return;
      }

      let newJob = {
        name: this.jobName,
        size: this.jobSize,
        search: this.jobSearch,
        searchType: this.jobSearchType,
        type: this.jobType,
        src: this.jobSrc,
        dst: this.jobDst,
        totalSessions: this.sessions.recordsFiltered,
        query: this.sessionsQuery
      };

      this.axios.post('hunt', { hunt: newJob })
        .then((response) => {
          this.createFormOpened = false;
          this.results.push(response.data.hunt);
        }, (error) => {
          this.createFormError = error.text || error;
        });
    },
    removeJob: function (job) {
      this.axios.delete(`hunt/${job.id}`)
        .then((response) => {
          for (let i = 0, len = this.results.length; i < len; ++i) {
            if (this.results[i].id === job.id) {
              this.results.splice(i, 1);
              return;
            }
          }
        }, (error) => {
          this.$set(job, 'error', error.text || error);
        });
    },
    pauseJob: function (job) {
      this.axios.put(`hunt/${job.id}/pause`)
        .then((response) => {
          this.$set(job, 'status', 'paused');
        }, (error) => {
          this.$set(job, 'error', error.text || error);
        });
    },
    playJob: function (job) { // TODO
      this.axios.put(`hunt/${job.id}/play`)
        .then((response) => {
          this.$set(job, 'status', 'queued');
        }, (error) => {
          this.$set(job, 'error', error.text || error);
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
    debounceSearch: function () {
      if (timeout) { clearTimeout(timeout); }
      timeout = setTimeout(() => {
        this.loadData();
      }, 400);
    },
    columnClick: function (name) {
      if (name === this.query.sortField) {
        // same sort field, so toggle order direction
        this.query.desc = !this.query.desc;
      } else { // new sort field, so set default order (desc)
        this.query.sortField = name;
        this.query.desc = true;
      }

      this.loadData();
    },
    /* helper functions ---------------------------------------------------- */
    loadData: function () {
      let expanded = [];
      if (this.results && this.results.length) {
        // save the expanded ones
        for (let result of this.results) {
          if (result.expanded) {
            expanded.push(result.id);
          }
        }
      }

      this.axios.get('hunt/list', { params: this.query })
        .then((response) => {
          this.loading = false;

          if (expanded.length) {
            // make sure expanded ones are still expanded
            for (let result of response.data.data) {
              if (expanded.indexOf(result.id) > -1) {
                result.expanded = true;
              }
            }
          }

          this.results = response.data.data;
        }, (error) => {
          this.pageError = error.text || error;
        });
    },
    loadSessions: function () {
      SessionsService.get(this.sessionsQuery)
        .then((response) => {
          this.sessions = response.data;
        })
        .catch((error) => {
          this.sessions = undefined;
        });
    }
  },
  beforeDestroy: function () {
    if (interval) { clearInterval(interval); }
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
