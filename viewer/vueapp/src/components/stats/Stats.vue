<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>

  <div class="stats-content">
    <ArkimeCollapsible>
      <span class="fixed-header">
        <!-- stats sub navbar -->
        <BRow gutter-x="1" align-h="start" class="stats-form p-1">

          <div v-if="tabIndex === 7">&nbsp;</div>

          <BCol cols="auto" v-if="tabIndex !== 7">
            <BInputGroup size="sm">
              <BInputGroupText class="input-group-text-fw">
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
              </BInputGroupText>
              <input type="text"
                class="form-control"
                v-model="searchTerm"
                v-focus="focusInput"
                @blur="onOffFocus"
                @input="debounceSearchInput"
                @keydown.stop.prevent.enter="debounceSearchInput"
                placeholder="Begin typing to search for items below (can use regex like .*foo)"
              />
              <BButton
                @click="clear"
                variant="outline-secondary"
                :disabled="!searchTerm"
                class="btn-clear-input">
                <span class="fa fa-close"></span>
              </BButton>
            </BInputGroup>
          </BCol>

          <!-- graph type select -->
          <BCol cols="auto" v-if="tabIndex === 0">
            <BInputGroup size="sm">
              <BInputGroupText>
                Graph Type
              </BInputGroupText>
              <select
                class="form-control"
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
                <option value="deltaDupDroppedPerSec">Duplicate Dropped/Sec</option>
                <option value="deltaTotalDroppedPerSec">Total Dropped/Sec</option>
                <option value="deltaSessionBytesPerSec">ES Session Bytes/Sec</option>
                <option value="sessionSizePerSec">ES Session Size/Sec</option>
                <option value="deltaWrittenBytesPerSec">Written Bytes/Sec</option>
                <option value="deltaUnwrittenBytesPerSec">Unwritten Bytes/Sec</option>
              </select>
            </BInputGroup>
          </BCol> <!-- /graph type select -->

          <!-- graph interval select -->
          <BCol cols="auto" v-if="tabIndex === 0">
            <BInputGroup size="sm">
              <BInputGroupText>
                Graph Interval
              </BInputGroupText>
              <select
                class="form-control"
                v-model="graphInterval"
                v-on:change="graphIntervalChange">
                <option value="5">Seconds</option>
                <option value="60">Minutes</option>
                <option value="600">10 Minutes</option>
              </select>
            </BInputGroup>
          </BCol> <!-- /graph interval select -->

          <!-- graph hide select -->
          <BCol cols="auto" v-if="tabIndex === 0 || tabIndex === 1">
            <BInputGroup size="sm">
              <BInputGroupText>Hide</BInputGroupText>
              <select class="form-control input-sm"
                v-model="graphHide"
                v-on:change="graphHideChange">
                <option value="none">None</option>
                <option value="old">Out of date</option>
                <option value="nosession">No sessions</option>
                <option value="both">Both</option>
              </select>
            </BInputGroup>
          </BCol> <!-- /graph hide select -->

          <!-- graph sort select -->
          <BCol cols="auto" v-if="tabIndex === 0">
            <BInputGroup size="sm">
              <BInputGroupText>Sort</BInputGroupText>
              <select class="form-control input-sm"
                v-model="graphSort">
                <option value="asc">Ascending</option>
                <option value="desc">Descending</option>
              </select>
            </BInputGroup>
          </BCol> <!-- /graph hide select -->

          <!-- page size select -->
          <BCol cols="auto" v-if="tabIndex === 4">
            <BInputGroup size="sm">
              <BInputGroupText>Page Size</BInputGroupText>
              <select
                class="form-control "
                v-model="pageSize"
                v-on:change="pageSizeChange">
                <option value="100">100 per page</option>
                <option value="200">200 per page</option>
                <option value="500">500 per page</option>
                <option value="1000">1,000 per page</option>
                <option value="5000">5,000 per page</option>
                <option value="10000">10,000 per page (careful)</option>
              </select>
            </BInputGroup>
          </BCol><!-- /page size select -->

          <!-- table data interval select -->
          <BCol cols="auto" v-if="tabIndex !== 0 && tabIndex !== 7">
            <BInputGroup size="sm">
              <BInputGroupText>Refresh Data Every</BInputGroupText>
              <select
                class="form-control"
                v-model="dataInterval"
                v-on:change="dataIntervalChange">
                <option value="5000">5 seconds</option>
                <option value="15000">15 seconds</option>
                <option value="30000">30 seconds</option>
                <option value="60000">1 minute</option>
                <option value="600000">10 minutes</option>
                <option value="0">None</option>
              </select>
            </BInputGroup>
          </BCol> <!-- /table data interval select -->

          <!-- shards show select -->
          <BCol cols="auto" v-if="tabIndex === 5">
            <BInputGroup size="sm">
              <BInputGroupText>Show</BInputGroupText>
              <select
                class="form-control"
                v-model="shardsShow"
                v-on:change="shardsShowChange">
                <option value="all">All</option>
                <option value="UNASSIGNED">Unassigned</option>
                <option value="RELOCATING">Relocating</option>
                <option value="INITIALIZING">Initializing</option>
                <option value="notstarted">Not Started</option>
              </select>
            </BInputGroup>
          </BCol> <!-- /graph hide select -->

          <!-- recovery show select -->
          <BCol cols="auto" v-if="tabIndex === 6">
            <BInputGroup size="sm">
              <BInputGroupText>Show</BInputGroupText>
              <select
                class="form-control"
                v-model="recoveryShow"
                v-on:change="recoveryShowChange">
                <option value="all">All</option>
                <option value="notdone">Active</option>
              </select>
            </BInputGroup>
          </BCol> <!-- /graph hide select -->

          <!-- refresh button -->
          <BCol cols="auto" v-if="tabIndex !== 0 && tabIndex !== 7">
            <BButton
              size="sm"
              variant="theme-tertiary"
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
            </BButton>
          </BCol> <!-- /refresh button -->

          <BCol>
            <!-- confirm button -->
            <transition name="buttons">
              <button v-if="confirmMessage"
                type="button"
                class="btn btn-sm btn-danger ms-2"
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
                class="btn btn-sm btn-warning ms-2"
                @click="cancelConfirm">
                <span class="fa fa-ban">
                </span>&nbsp;
                Cancel
              </button>
            </transition> <!-- /cancel confirm button -->
          </BCol>

          <!-- error (from child component) -->
          <div v-if="childError"
            role="alert"
            class="alert alert-sm alert-danger alert-dismissible fade show ms-2">
            {{ childError }}
            <button type="button"
              class="btn-close"
              @click="childError = ''">
              <span>&times;</span>
            </button>
          </div> <!-- /error (from child component) -->

          <!-- shrink index -->
          <div v-if="shrinkIndex"
            class="ms-4 form-inline">
            <strong>
              Shrink {{ shrinkIndex.index }}
            </strong>
            <!-- new # shards -->
            <div class="input-group input-group-sm ms-2">
              <span class="input-group-text">
                # Shards
              </span>
              <select v-model="shrinkFactor"
                class="form-control"
                style="-webkit-appearance:none;">
                <option v-for="factor in shrinkFactors"
                  :key="factor"
                  :value="factor">
                  {{ factor }}
                </option>
              </select>
            </div> <!-- /new # shards -->
            <!-- temporary node -->
            <div v-if="nodes && temporaryNode"
              class="input-group input-group-sm ms-2">
              <span class="input-group-text">
                Temporary Node
              </span>
              <select v-model="temporaryNode"
                class="form-control"
                style="-webkit-appearance:none;">
                <option v-for="node in nodes"
                  :key="node.name"
                  :value="node.name">
                  {{ node.name }}
                </option>
              </select>
            </div> <!-- /new shards input -->
            <!-- ok button -->
            <button class="btn btn-sm btn-success pull-right ms-2"
              @click="executeShrink(shrinkIndex)"
              type="button">
              <span class="fa fa-check"></span>
            </button> <!-- /ok button -->
            <!-- cancel button -->
            <button class="btn btn-sm btn-warning pull-right ms-2"
              @click="cancelShrink"
              type="button">
              <span class="fa fa-ban"></span>
            </button> <!-- /cancel button -->
          </div>
          <span v-if="shrinkIndex && shrinkError"
            class="text-danger ms-2">
            {{ shrinkError }}
          </span> <!-- /shrink index -->

          <!-- select cluster(s) -->
          <BCol cols="auto" v-if="multiviewer">
            <Clusters
              @updateCluster="updateCluster"
              :select-one="clusterParamOverride && tabIndex > 1"
            />
          </BCol> <!-- /select cluster(s) -->

        </BRow> <!-- /stats sub navbar -->
      </span>
    </ArkimeCollapsible>

    <!-- stats content -->
    <div class="stats-tabs">
      <b-tabs lazy @update:model-value="tabIndexChange($event)">
        <b-tab title="Capture Graphs" :active="tabIndex === 0">
          <capture-graphs
            :refreshData="refreshData"
            :searchTerm="searchTerm"
            :graph-type="statsType"
            :graph-interval="graphInterval"
            :graph-hide="graphHide"
            :graph-sort="graphSort"
            :cluster="cluster"
            :user="user">
          </capture-graphs>
        </b-tab>
        <b-tab title="Capture Stats" :active="tabIndex === 1">
          <capture-stats
            :graph-hide="graphHide"
            :refreshData="refreshData"
            :searchTerm="searchTerm"
            :data-interval="dataInterval"
            :cluster="cluster"
            :user="user">
          </capture-stats>
        </b-tab>
        <b-tab title="ES Nodes" :active="tabIndex === 2">
          <es-nodes
            :refreshData="refreshData"
            :searchTerm="searchTerm"
            :data-interval="dataInterval"
            :cluster="cluster">
          </es-nodes>
        </b-tab>
        <b-tab title="ES Indices" :active="tabIndex === 3">
          <es-indices
            :refreshData="refreshData"
            :data-interval="dataInterval"
            @errored="onError"
            @confirm="confirm"
            @shrink="shrink"
            :searchTerm="searchTerm"
            :issueConfirmation="issueConfirmation"
            :user="user"
            :cluster="cluster">
          </es-indices>
        </b-tab>
        <b-tab title="ES Tasks" :active="tabIndex === 4">
          <es-tasks
            :data-interval="dataInterval"
            :refreshData="refreshData"
            :searchTerm="searchTerm"
            :pageSize="pageSize"
            :user="user"
            @errored="onError"
            :cluster="cluster">
          </es-tasks>
        </b-tab>
        <b-tab title="ES Shards" :active="tabIndex === 5">
          <es-shards
            :shards-show="shardsShow"
            :refreshData="refreshData"
            :searchTerm="searchTerm"
            :data-interval="dataInterval"
            :cluster="cluster">
          </es-shards>
        </b-tab>
        <b-tab title="ES Recovery" :active="tabIndex === 6">
          <es-recovery
            :recovery-show="recoveryShow"
            :data-interval="dataInterval"
            :refreshData="refreshData"
            :searchTerm="searchTerm"
            :user="user"
            :cluster="cluster">
          </es-recovery>
        </b-tab>
        <b-tab title="ES Admin" :active="tabIndex === 7" v-if="user.esAdminUser">
          <es-admin
            :data-interval="dataInterval"
            :refreshData="refreshData"
            :user="user"
            :cluster="cluster">
          </es-admin>
        </b-tab>
      </b-tabs>
    </div> <!-- /stats content -->

  </div>

</template>

<script>
import EsShards from './EsShards.vue';
import EsNodes from './EsNodes.vue';
import EsTasks from './EsTasks.vue';
import EsRecovery from './EsRecovery.vue';
import EsIndices from './EsIndices.vue';
import EsAdmin from './EsAdmin.vue';
import CaptureGraphs from './CaptureGraphs.vue';
import CaptureStats from './CaptureStats.vue';
import ArkimeCollapsible from '../utils/CollapsibleWrapper.vue';
import Utils from '../utils/utils';
import Focus from '@real_common/Focus.vue';
import Clusters from '../utils/Clusters.vue';
import StatsService from './StatsService.js';

let searchInputTimeout;

export default {
  name: 'Stats',
  components: {
    CaptureGraphs,
    CaptureStats,
    EsShards,
    EsNodes,
    EsIndices,
    EsTasks,
    EsRecovery,
    EsAdmin,
    ArkimeCollapsible,
    Clusters
  },
  directives: { Focus },
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
      cluster: this.$route.query.cluster || undefined,
      refreshData: false,
      childError: '',
      confirmMessage: '',
      itemToConfirm: undefined,
      issueConfirmation: undefined,
      searchTerm: undefined,
      shrinkIndex: undefined,
      shrinkFactor: undefined,
      shrinkFactors: undefined,
      temporaryNode: undefined,
      nodes: undefined,
      shrinkError: undefined,
      clusterParamOverride: true,
      multiviewer: this.$constants.MULTIVIEWER
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
    $route: 'updateParams',
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
      // override the query params for the cluster selected
      // if the user is on any of the ES tabs, they can select only one cluster
      // so even if the route query is set, we want to override it with only one cluster
      // this flag is toggled so that the children components can override the route queries
      // the cluster dropdown component watches for selectOne and updates the cluster prop to override the route queries
      // but the route queries stay the same so that the user can navigate back to the previous tab without losing their selection
      this.tabIndex = newTabIndex;
      this.clusterParamOverride = false;
      setTimeout(() => { this.clusterParamOverride = true; });
      this.$router.push({ query: { ...this.$route.query, statsTab: newTabIndex } });
    },
    // overrides the cluster route query
    updateCluster: function ({ cluster }) {
      this.cluster = cluster;
    },
    updateParams: function () {
      const queryParams = this.$route.query;

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
      if (queryParams.cluster) {
        this.cluster = queryParams.cluster;
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
    debounceSearchInput: function () {
      if (searchInputTimeout) { clearTimeout(searchInputTimeout); }
      // debounce the input so it only issues a request after keyups cease for 400ms
      searchInputTimeout = setTimeout(() => {
        searchInputTimeout = null;
        this.loadData();
      }, 800);
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
    },
    async shrink (index) {
      this.shrinkIndex = index;
      this.shrinkFactors = Utils.findFactors(parseInt(index.pri));
      this.shrinkFactors.length === 1
        ? this.shrinkFactor = this.shrinkFactors[0]
        : this.shrinkFactor = this.shrinkFactors[1];

      if (!Utils.checkClusterSelection(this.cluster, this.$store.state.esCluster.availableCluster.active, this).valid) {
        return;
      }

      try {
        const response = await StatsService.getDataNodes({ cluster: this.cluster });
        this.nodes = response.data.data;
        this.temporaryNode = this.nodes[0].name;
      } catch {
        this.shrinkError = 'Error fetching data nodes';
      }
    },
    cancelShrink: function () {
      this.shrinkIndex = undefined;
      this.shrinkFactor = undefined;
      this.shrinkFactors = undefined;
      this.temporaryNode = undefined;
      this.shrinkError = undefined;
    },
    async executeShrink (index) {
      if (!Utils.checkClusterSelection(this.cluster, this.$store.state.esCluster.availableCluster.active, this).valid) {
        return;
      }

      try {
        const response = await StatsService.shrinkIndex(index.index, {
          target: this.temporaryNode,
          numShards: this.shrinkFactor
        }, { cluster: this.cluster });
        if (!response.data.success) {
          this.shrinkError = response.data.text;
          return;
        }
        this.cancelShrink();
      } catch (error) {
        this.shrinkError = error.text || error;
      }
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

/* apply theme colors to subnavbar */
.stats-form {
  z-index : 6;
  background-color: var(--color-quaternary-lightest);
}

/* remove browser styles on select box (mostly for border-radius) */
select {
  -webkit-appearance: none;
}

.stats-tabs .input-group {
  max-width: 333px;
  position: fixed;
  right: 0;
  z-index: 5;
  margin-top: 10px;
}
</style>
