<template>

  <div class="stats-content">

    <!-- stats sub navbar -->
    <form class="stats-form">
      <div class="form-inline mr-1 ml-1 mt-1 mb-1">

        <!-- graph type select -->
        <div class="input-group input-group-sm"
          v-if="tabIndex === 0">
          <div class="input-group-prepend help-cursor"
            v-b-tooltip.hover
            title="The type of graph data to display">
            <span class="input-group-text">
              Graph Type
            </span>
          </div>
          <select class="form-control input-sm"
            v-model="statsType"
            v-on:change="statsTypeChange">
            <option value="deltaPacketsPerSec">Packets/Sec</option>
            <option value="deltaBytesPerSec">Bytes/Sec</option>
            <option value="deltaBitsPerSec">Bits/Sec</option>
            <option value="deltaSessionsPerSec">Sessions/Sec</option>
            <option value="deltaDroppedPerSec">Input Dropped/Sec</option>
            <option value="monitoring">Active Sessions</option>
            <option value="tcpSessions">Active TCP Sessions</option>
            <option value="udpSessions">Active UDP Sessions</option>
            <option value="icmpSessions">Active ICMP Sessions</option>
            <option value="sctpSessions">Active SCTP Sessions</option>
            <option value="espSessions">Active ESP Sessions</option>
            <option value="usedSpaceM">Used Space MB</option>
            <option value="freeSpaceM">Free Space MB</option>
            <option value="freeSpaceP">Free Space %</option>
            <option value="memory">Memory</option>
            <option value="memoryP">Memory %</option>
            <option value="cpu">CPU</option>
            <option value="diskQueue">Disk Queue</option>
            <option value="esQueue">ES Queue</option>
            <option value="deltaESDroppedPerSec">ES Dropped/Sec</option>
            <option value="esHealthMS">ES Health Response MS</option>
            <option value="packetQueue">Packet Queue</option>
            <option value="closeQueue">Closing Queue</option>
            <option value="needSave">Waiting Queue</option>
            <option value="frags">Active Fragments</option>
            <option value="deltaFragsDroppedPerSec">Fragments Dropped/Sec</option>
            <option value="deltaOverloadDroppedPerSec">Overload Dropped/Sec</option>
            <option value="deltaTotalDroppedPerSec">Total Dropped/Sec</option>
            <option value="deltaSessionBytesPerSec">ES Session Bytes/Sec</option>
            <option value="sessionSizePerSec">ES Session Size/Sec</option>
            <option value="deltaWrittenBytesPerSec">Written Bytes/Sec</option>
            <option value="deltaUnwrittenBytesPerSec">Unwritten Bytes/Sec</option>
          </select>
        </div> <!-- /graph type select -->

        <!-- graph interval select -->
        <div class="input-group input-group-sm ml-1"
          v-if="tabIndex === 0">
          <div class="input-group-prepend help-cursor"
            v-b-tooltip.hover
            title="Graph data bins and graph data refresh interval">
            <span class="input-group-text">
              Graph Interval
            </span>
          </div>
          <select class="form-control input-sm"
            v-model="graphInterval"
            v-on:change="graphIntervalChange">
            <option value="5">Seconds</option>
            <option value="60">Minutes</option>
            <option value="600">10 Minutes</option>
          </select>
        </div> <!-- /graph interval select -->

        <!-- graph hide select -->
        <div class="input-group input-group-sm ml-1"
          v-if="tabIndex === 0 || tabIndex === 1">
          <div class="input-group-prepend help-cursor"
            v-b-tooltip.hover
            title="Hide rows">
           <span class="input-group-text">
             Hide
           </span>
         </div>
          <select class="form-control input-sm"
            v-model="graphHide"
            v-on:change="graphHideChange">
            <option value="none">None</option>
            <option value="old">Out of date</option>
            <option value="nosession">No sessions</option>
            <option value="both">Both</option>
          </select>
        </div> <!-- /graph hide select -->

        <!-- graph sort select -->
        <div class="input-group input-group-sm ml-1"
          v-if="tabIndex === 0">
          <div class="input-group-prepend help-cursor"
            v-b-tooltip.hover
            title="Sort">
           <span class="input-group-text">
             Sort
           </span>
         </div>
          <select class="form-control input-sm"
            v-model="graphSort">
            <option value="asc">Ascending</option>
            <option value="desc">Descending</option>
          </select>
        </div> <!-- /graph hide select -->

        <!-- page size select -->
        <div class="input-group input-group-sm ml-1"
          v-if="tabIndex === 4">
          <div class="input-group-prepend">
            <span class="input-group-text">
              Page Size
            </span>
          </div>
          <select class="form-control input-sm"
            v-model="pageSize"
            v-on:change="pageSizeChange">
            <option value="100">100 per page</option>
            <option value="200">200 per page</option>
            <option value="500">500 per page</option>
            <option value="1000">1,000 per page</option>
            <option value="5000">5,000 per page</option>
            <option value="10000">10,000 per page (careful)</option>
          </select>
        </div> <!-- /page size select -->

        <!-- table data interval select -->
        <div class="input-group input-group-sm ml-1"
          v-if="tabIndex !== 0">
          <div class="input-group-prepend help-cursor"
            v-b-tooltip.hover
            title="Data refresh interval for Node and Elasticsearch stats">
            <span class="input-group-text">
              Refresh Data Every
            </span>
          </div>
          <select class="form-control input-sm"
            v-model="dataInterval"
            v-on:change="dataIntervalChange">
            <option value="5000">5 seconds</option>
            <option value="15000">15 seconds</option>
            <option value="30000">30 seconds</option>
            <option value="60000">1 minute</option>
            <option value="600000">10 minutes</option>
            <option value="0">None</option>
          </select>
        </div> <!-- /table data interval select -->

        <!-- shards show select -->
        <div class="input-group input-group-sm ml-1"
          v-if="tabIndex === 5">
          <div class="input-group-prepend help-cursor"
            v-b-tooltip.hover
            title="Hide shards">
           <span class="input-group-text">
             Show
           </span>
         </div>
          <select class="form-control input-sm"
            v-model="shardsShow"
            v-on:change="shardsShowChange">
            <option value="all">All</option>
            <option value="UNASSIGNED">Unassigned</option>
            <option value="RELOCATING">Relocating</option>
            <option value="INITIALIZING">Initializing</option>
            <option value="notstarted">Not Started</option>
          </select>
        </div> <!-- /graph hide select -->

        <!-- recovery show select -->
        <div class="input-group input-group-sm ml-1"
          v-if="tabIndex === 6">
          <div class="input-group-prepend help-cursor"
            v-b-tooltip.hover
            title="Hide rows">
           <span class="input-group-text">
             Show
           </span>
         </div>
          <select class="form-control input-sm"
            v-model="recoveryShow"
            v-on:change="recoveryShowChange">
            <option value="all">All</option>
            <option value="notdone">Not Done</option>
          </select>
        </div> <!-- /graph hide select -->

        <!-- refresh button -->
        <div class="input-group input-group-sm ml-1"
          v-if="tabIndex !== 0">
          <button type="button"
            class="btn btn-theme-tertiary btn-sm refresh-btn"
            @click="loadData">
            <span v-if="!shiftKeyHold">
              Refresh
            </span>
            <span v-else
              class="enter-icon">
              <span class="fa fa-long-arrow-left fa-lg">
              </span>
              <div class="enter-arm">
              </div>
            </span>
          </button>
        </div> <!-- /refresh button -->

        <!-- confirm button -->
        <transition name="buttons">
          <button v-if="confirmMessage"
            type="button"
            class="btn btn-sm btn-danger ml-2"
            @click="confirmed">
            <span class="fa fa-check">
            </span>&nbsp;
            {{ confirmMessage }}
          </button>
        </transition> <!-- /confirm button -->

        <!-- cancel confirm button -->
        <transition name="buttons">
          <button v-if="confirmMessage"
            type="button"
            class="btn btn-sm btn-warning ml-2"
            @click="cancelConfirm">
            <span class="fa fa-ban">
            </span>&nbsp;
            Cancel
          </button>
        </transition> <!-- /cancel confirm button -->

        <!-- error (from child component) -->
        <div v-if="childError"
          role="alert"
          class="alert alert-sm alert-danger alert-dismissible fade show ml-2">
          {{ childError }}
          <button type="button"
            class="close"
            @click="childError = ''">
            <span>&times;</span>
          </button>
        </div> <!-- /error (from child component) -->

      </div>
    </form> <!-- /stats sub navbar -->

    <!-- stats content -->
    <div class="stats-tabs">
      <div class="input-group input-group-sm pull-right mr-1 pt-1">
        <div class="input-group-prepend">
          <span class="input-group-text input-group-text-fw">
            <span v-if="loadingData"
              class="fa fa-spinner fa-spin text-theme-accent">
            </span>
            <span v-else-if="!shiftKeyHold"
              class="fa fa-search fa-fw">
            </span>
            <span v-else-if="shiftKeyHold"
              class="query-shortcut">
              Q
            </span>
          </span>
        </div>
        <input type="text"
          class="form-control"
          v-model="searchTerm"
          v-focus-input="focusInput"
          @blur="onOffFocus"
          @input="debounceSearchInput"
          @keyup.enter="debounceSearchInput"
          placeholder="Begin typing to search for items below"
        />
        <span class="input-group-append">
          <button type="button"
            @click="clear"
            :disabled="!searchTerm"
            class="btn btn-outline-secondary btn-clear-input">
            <span class="fa fa-close">
            </span>
          </button>
        </span>
      </div>
      <b-tabs v-model="tabIndex">
        <b-tab title="Capture Graphs"
          @click="tabIndexChange(0)">
          <capture-graphs v-if="user && tabIndex === 0"
            :refreshData="refreshData"
            :searchTerm="searchTerm"
            :graph-type="statsType"
            :graph-interval="graphInterval"
            :graph-hide="graphHide"
            :graph-sort="graphSort"
            :user="user">
          </capture-graphs>
        </b-tab>
        <b-tab title="Capture Stats"
          @click="tabIndexChange(1)">
          <capture-stats v-if="user && tabIndex === 1"
            :graph-hide="graphHide"
            :refreshData="refreshData"
            :searchTerm="searchTerm"
            :data-interval="dataInterval"
            :user="user">
          </capture-stats>
        </b-tab>
        <b-tab title="ES Nodes"
          @click="tabIndexChange(2)">
          <es-nodes v-if="user && tabIndex === 2"
            :refreshData="refreshData"
            :searchTerm="searchTerm"
            :data-interval="dataInterval">
          </es-nodes>
        </b-tab>
        <b-tab title="ES Indices"
          @click="tabIndexChange(3)"
          v-if="!multiviewer">
          <es-indices v-if="user && tabIndex === 3"
            :refreshData="refreshData"
            :data-interval="dataInterval"
            @errored="onError"
            @confirm="confirm"
            :searchTerm="searchTerm"
            :issueConfirmation="issueConfirmation"
            :user="user">
          </es-indices>
        </b-tab>
        <b-tab title="ES Tasks"
          @click="tabIndexChange(4)"
          v-if="!multiviewer">
          <es-tasks v-if="user && tabIndex === 4"
            :data-interval="dataInterval"
            :refreshData="refreshData"
            :searchTerm="searchTerm"
            :pageSize="pageSize"
            :user="user">
          </es-tasks>
        </b-tab>
        <b-tab title="ES Shards"
          @click="tabIndexChange(5)"
          v-if="!multiviewer">
          <es-shards v-if="user && tabIndex === 5"
            :shards-show="shardsShow"
            :refreshData="refreshData"
            :searchTerm="searchTerm"
            :data-interval="dataInterval">
          </es-shards>
        </b-tab>
        <b-tab title="ES Recovery"
          @click="tabIndexChange(6)"
          v-if="!multiviewer">
          <es-recovery v-if="user && tabIndex === 6"
            :recovery-show="recoveryShow"
            :data-interval="dataInterval"
            :refreshData="refreshData"
            :searchTerm="searchTerm"
            :user="user">
          </es-recovery>
        </b-tab>
      </b-tabs>
    </div> <!-- /stats content -->

  </div>

</template>

<script>
import EsShards from './EsShards';
import EsNodes from './EsNodes';
import EsTasks from './EsTasks';
import EsRecovery from './EsRecovery';
import EsIndices from './EsIndices';
import CaptureGraphs from './CaptureGraphs';
import CaptureStats from './CaptureStats';
import FocusInput from '../utils/FocusInput';

let searchInputTimeout;

export default {
  name: 'Stats',
  components: {
    CaptureGraphs, CaptureStats, EsShards, EsNodes, EsIndices, EsTasks, EsRecovery
  },
  directives: { FocusInput },
  data: function () {
    return {
      tabIndex: parseInt(this.$route.query.statsTab, 10) || 0,
      statsType: this.$route.query.type || 'deltaPacketsPerSec',
      graphInterval: this.$route.query.gtime || '5',
      graphHide: this.$route.query.hide || 'none',
      graphSort: this.$route.query.sort || 'asc',
      recoveryShow: this.$route.query.recoveryShow || 'notdone',
      shardsShow: this.$route.query.shardsShow || 'notstarted',
      dataInterval: this.$route.query.refreshInterval || '15000',
      pageSize: this.$route.query.size || '500',
      refreshData: false,
      childError: '',
      multiviewer: this.$constants.MOLOCH_MULTIVIEWER,
      confirmMessage: '',
      itemToConfirm: undefined,
      issueConfirmation: undefined,
      searchTerm: undefined
    };
  },
  computed: {
    user: function () {
      return this.$store.state.user;
    },
    issueSearch: function () {
      return this.$store.state.issueSearch;
    },
    shiftKeyHold: function () {
      return this.$store.state.shiftKeyHold;
    },
    focusInput: {
      get: function () {
        return this.$store.state.focusSearch;
      },
      set: function (newValue) {
        this.$store.commit('setFocusSearch', newValue);
      }
    },
    loadingData: function () {
      return this.$store.state.loadingData;
    }
  },
  watch: {
    // watch for the route to change, then update the view
    '$route': 'updateParams',
    issueSearch: function (newVal, oldVal) {
      if (newVal) { this.loadData(); }
    }
  },
  methods: {
    statsTypeChange: function () {
      this.$router.push({ query: { ...this.$route.query, type: this.statsType } });
    },
    graphIntervalChange: function () {
      this.$router.push({ query: { ...this.$route.query, gtime: this.graphInterval } });
    },
    graphHideChange: function () {
      this.$router.push({ query: { ...this.$route.query, hide: this.graphHide } });
    },
    shardsShowChange: function () {
      this.$router.push({ query: { ...this.$route.query, shardsShow: this.shardsShow } });
    },
    recoveryShowChange: function () {
      this.$router.push({ query: { ...this.$route.query, recoveryShow: this.recoveryShow } });
    },
    dataIntervalChange: function () {
      this.$router.push({ query: { ...this.$route.query, refreshInterval: this.dataInterval } });
    },
    pageSizeChange: function () {
      this.$router.push({ query: { ...this.$route.query, size: this.pageSize } });
    },
    tabIndexChange: function (newTabIndex) {
      this.$router.push({ query: { ...this.$route.query, statsTab: newTabIndex } });
    },
    updateParams: function () {
      let queryParams = this.$route.query;

      if (queryParams.statsTab) {
        this.tabIndex = parseInt(queryParams.statsTab, 10);
      }
      if (queryParams.type) {
        this.statsType = queryParams.type;
      }
      if (queryParams.graphInterval) {
        this.graphInterval = queryParams.gtime;
      }
      if (queryParams.graphHide) {
        this.graphHide = queryParams.graphHide;
      }
      if (queryParams.shardsShow) {
        this.shardsShow = queryParams.shardsShow;
      }
      if (queryParams.recoveryShow) {
        this.recoveryShow = queryParams.recoveryShow;
      }
      if (queryParams.graphSort) {
        this.graphSort = queryParams.graphSort;
      }
      if (queryParams.refreshInterval) {
        this.dataInterval = queryParams.refreshInterval;
      }
      this.pageSize = queryParams.size || 500;
    },
    clear: function () {
      this.searchTerm = undefined;
      this.loadData();
    },
    onOffFocus: function () {
      this.focusInput = false;
    },
    debounceSearchInput () {
      if (searchInputTimeout) { clearTimeout(searchInputTimeout); }
      // debounce the input so it only issues a request after keyups cease for 400ms
      searchInputTimeout = setTimeout(() => {
        searchInputTimeout = null;
        this.loadData();
      }, 400);
    },
    loadData: function () {
      this.refreshData = true;
      setTimeout(() => { this.refreshData = false; });
    },
    onError: function (message) {
      this.childError = message;
    },
    confirm: function (message, itemToConfirm) {
      this.confirmMessage = message;
      this.itemToConfirm = itemToConfirm;
    },
    cancelConfirm: function () {
      this.issueConfirmation = undefined;
      this.itemToConfirm = undefined;
      this.confirmMessage = '';
    },
    confirmed: function () {
      this.issueConfirmation = this.itemToConfirm;
      setTimeout(() => {
        this.issueConfirmation = undefined;
        this.itemToConfirm = undefined;
        this.confirmMessage = '';
      });
    }
  }
};
</script>

<style>
table .btn-group.row-actions-btn > .btn-sm {
  padding: 1px 4px;
  font-size: 13px;
  line-height: 1.2;
}

/* fix the nav tabs to the top and scroll the content */
.stats-tabs .nav-tabs {
  position: fixed;
  left: 0;
  right: 0;
  z-index: 4;
  padding-top: 10px;
  background-color: var(--color-background, #FFFFFF);
}
.stats-tabs .tab-content {
  padding-top: 50px;
}
</style>

<style scoped>
.stats-content {
  padding-top: 36px;
}

/* apply theme colors to subnavbar */
form.stats-form {
  position: fixed;
  left: 0;
  right: 0;
  z-index : 5;
  background-color: var(--color-quaternary-lightest);

  -webkit-box-shadow: var(--px-none) var(--px-none) var(--px-xxlg) -8px #333;
     -moz-box-shadow: var(--px-none) var(--px-none) var(--px-xxlg) -8px #333;
          box-shadow: var(--px-none) var(--px-none) var(--px-xxlg) -8px #333;
}

/* remove browser styles on select box (mostly for border-radius) */
select {
  -webkit-appearance: none;
}

.stats-tabs {
  margin-top: 35px;
}
.stats-tabs .input-group {
  max-width: 333px;
  position: fixed;
  right: 0;
  z-index: 5;
  margin-top: 10px;
}

/* confirm button animations */
.buttons-enter-active {
  animation: bounce-in .5s;
}
.buttons-leave-active {
  transition: all .3s ease;
}
.buttons-enter, .buttons-leave-to {
  opacity: 0;
}
@keyframes bounce-in {
  0% {
    transform: scale(0);
  }
  50% {
    transform: scale(1.2);
  }
  100% {
    transform: scale(1);
  }
}
</style>
