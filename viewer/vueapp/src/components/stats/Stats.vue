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
            v-on:change="statsTypeChange()">
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
            <option value="freeSpaceM">Free Space MB</option>
            <option value="freeSpaceP">Free Space %</option>
            <option value="memory">Memory</option>
            <option value="memoryP">Memory %</option>
            <option value="cpu">CPU</option>
            <option value="diskQueue">Disk Queue</option>
            <option value="esQueue">ES Queue</option>
            <option value="deltaESDroppedPerSec">ES Dropped/Sec</option>
            <option value="packetQueue">Packet Queue</option>
            <option value="closeQueue">Closing Queue</option>
            <option value="needSave">Waiting Queue</option>
            <option value="frags">Active Fragments</option>
            <option value="deltaFragsDroppedPerSec">Fragments Dropped/Sec</option>
            <option value="deltaOverloadDroppedPerSec">Overload Dropped/Sec</option>
            <option value="deltaTotalDroppedPerSec">Total Dropped/Sec</option>
            <option value="deltaSessionBytesPerSec">ES Session Bytes/Sec</option>
            <option value="sessionSizePerSec">ES Session Size/Sec</option>
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
            v-on:change="graphIntervalChange()">
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
            v-on:change="graphHideChange()">
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
            v-on:change="dataIntervalChange()" >
            <option value="5000">5 seconds</option>
            <option value="15000">15 seconds</option>
            <option value="30000">30 seconds</option>
            <option value="60000">1 minute</option>
            <option value="600000">10 minutes</option>
            <option value="0">None</option>
          </select>
        </div> <!-- /table data interval select -->

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
    <div class="pt-5">
      <span v-if="tabIndex === 0"
        v-b-tooltip.hover
        class="fa fa-lg fa-question-circle-o cursor-help mt-2 stats-info"
        title="HINT: These graphs are 1440 pixels wide. Expand your browser window to at least 1500 pixels wide for best viewing.">
      </span>
      <b-tabs v-model="tabIndex">
        <b-tab title="Capture Graphs"
          @click="tabIndexChange()">
          <capture-graphs v-if="user && tabIndex === 0"
            :graph-type="statsType"
            :graph-interval="graphInterval"
            :graph-hide="graphHide"
            :graph-sort="graphSort"
            :user="user">
          </capture-graphs>
        </b-tab>
        <b-tab title="Capture Stats"
          @click="tabIndexChange()">
          <capture-stats v-if="user && tabIndex === 1"
            :graph-hide="graphHide"
            :data-interval="dataInterval"
            :user="user">
          </capture-stats>
        </b-tab>
        <b-tab title="ES Nodes"
          @click="tabIndexChange()">
          <es-nodes v-if="user && tabIndex === 2"
            :data-interval="dataInterval">
          </es-nodes>
        </b-tab>
        <b-tab title="ES Indices"
          @click="tabIndexChange()">
          <es-indices v-if="user && tabIndex === 3"
            :data-interval="dataInterval"
            @errored="onError">
          </es-indices>
        </b-tab>
        <b-tab title="ES Tasks"
          @click="tabIndexChange()">
          <es-tasks v-if="user && tabIndex === 4"
            :data-interval="dataInterval"
            :user="user">
          </es-tasks>
        </b-tab>
        <b-tab title="ES Shards"
          @click="tabIndexChange()">
          <es-shards v-if="user && tabIndex === 5"
            :data-interval="dataInterval">
          </es-shards>
        </b-tab>
      </b-tabs>
    </div> <!-- /stats content -->

  </div>

</template>

<script>
import EsShards from './EsShards';
import EsNodes from './EsNodes';
import EsTasks from './EsTasks';
import EsIndices from './EsIndices';
import CaptureGraphs from './CaptureGraphs';
import CaptureStats from './CaptureStats';
import UserService from '../users/UserService';
export default {
  name: 'Stats',
  data: function () {
    return {
      user: null,
      tabIndex: parseInt(this.$route.query.statsTab, 10) || 0,
      statsType: this.$route.query.type || 'deltaPacketsPerSec',
      graphInterval: this.$route.query.gtime || '5',
      graphHide: this.$route.query.hide || 'none',
      graphSort: this.$route.query.sort || 'asc',
      dataInterval: this.$route.query.refreshInterval || '5000',
      childError: ''
    };
  },
  components: {
    CaptureGraphs, CaptureStats, EsShards, EsNodes, EsIndices, EsTasks
  },
  created: function () {
    this.loadUser();
  },
  watch: {
    // watch for the route to change, then update the view
    '$route': 'updateParams'
  },
  methods: {
    loadUser: function () {
      UserService.getCurrent()
        .then((response) => {
          this.user = response;
        }, (error) => {
          this.user = { settings: { timezone: 'local' } };
        });
    },
    statsTypeChange: function () {
      this.$router.push({ query: { ...this.$route.query, type: this.statsType } });
    },
    graphIntervalChange: function () {
      this.$router.push({ query: { ...this.$route.query, gtime: this.graphInterval } });
    },
    graphHideChange: function () {
      this.$router.push({ query: { ...this.$route.query, hide: this.graphHide } });
    },
    dataIntervalChange: function () {
      this.$router.push({ query: { ...this.$route.query, refreshInterval: this.dataInterval } });
    },
    tabIndexChange: function () {
      this.$router.push({ query: { ...this.$route.query, statsTab: this.tabIndex } });
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
      if (queryParams.graphSort) {
        this.graphSort = queryParams.graphSort;
      }
      if (queryParams.refreshInterval) {
        this.dataInterval = queryParams.refreshInterval;
      }
    },
    onError: function (message) {
      this.childError = message;
    }
  }
};
</script>
<style scoped>
.stats-content {
  padding-top: 36px;
}

/* apply theme colors to subnavbar */
form.stats-form {
  position: fixed;
  left: 0;
  right: 0;
  z-index : 4;
  background-color: var(--color-quaternary-lightest);

  -webkit-box-shadow: var(--px-none) var(--px-none) var(--px-xxlg) -8px #333;
     -moz-box-shadow: var(--px-none) var(--px-none) var(--px-xxlg) -8px #333;
          box-shadow: var(--px-none) var(--px-none) var(--px-xxlg) -8px #333;
}

/* remove browser styles on select box (mostly for border-radius) */
select {
  -webkit-appearance: none;
}

.stats-info {
  position: absolute;
  right: 4px;
}
</style>
