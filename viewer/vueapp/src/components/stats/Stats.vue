<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div class="stats-content">
    <ArkimeCollapsible>
      <span class="fixed-header">
        <!-- stats sub navbar -->
        <BRow
          gutter-x="1"
          align-h="start"
          class="stats-form p-1"
        >

          <BCol
            cols="auto"
            v-if="tabIndex !== 7"
          >
            <BInputGroup size="sm">
              <BInputGroupText class="input-group-text-fw">
                <span
                  v-if="loadingData"
                  class="fa fa-spinner fa-spin text-theme-accent"
                />
                <span
                  v-else-if="!shiftKeyHold"
                  class="fa fa-search fa-fw"
                />
                <span
                  v-else-if="shiftKeyHold"
                  class="query-shortcut"
                >
                  Q
                </span>
              </BInputGroupText>
              <input
                type="text"
                class="form-control"
                v-model="searchTerm"
                v-focus="focusInput"
                @blur="onOffFocus"
                @input="debounceSearchInput"
                @keydown.stop.prevent.enter="debounceSearchInput"
                :placeholder="$t('stats.filterPlaceholder')"
              >
              <BButton
                @click="clear"
                variant="outline-secondary"
                :disabled="!searchTerm"
                class="btn-clear-input"
              >
                <span class="fa fa-close" />
              </BButton>
            </BInputGroup>
          </BCol>

          <!-- graph type select -->
          <BCol
            cols="auto"
            v-if="tabIndex === 0"
          >
            <BInputGroup size="sm">
              <BInputGroupText>
                {{ $t('stats.graphType') }}
              </BInputGroupText>
              <select
                class="form-control"
                v-model="statsType"
                @change="statsTypeChange"
              >
                <option
                  value="deltaPacketsPerSec"
                  v-i18n-value="'stats.cstats.'"
                />
                <option
                  value="deltaBytesPerSec"
                  v-i18n-value="'stats.cstats.'"
                />
                <option
                  value="deltaBitsPerSec"
                  v-i18n-value="'stats.cstats.'"
                />
                <option
                  value="deltaSessionsPerSec"
                  v-i18n-value="'stats.cstats.'"
                />
                <option
                  value="deltaDroppedPerSec"
                  v-i18n-value="'stats.cstats.'"
                />
                <option
                  value="monitoring"
                  v-i18n-value="'stats.cstats.'"
                />
                <option
                  value="tcpSessions"
                  v-i18n-value="'stats.cstats.'"
                />
                <option
                  value="udpSessions"
                  v-i18n-value="'stats.cstats.'"
                />
                <option
                  value="icmpSessions"
                  v-i18n-value="'stats.cstats.'"
                />
                <option
                  value="sctpSessions"
                  v-i18n-value="'stats.cstats.'"
                />
                <option
                  value="espSessions"
                  v-i18n-value="'stats.cstats.'"
                />
                <option
                  value="usedSpaceM"
                  v-i18n-value="'stats.cstats.'"
                />
                <option
                  value="freeSpaceM"
                  v-i18n-value="'stats.cstats.'"
                />
                <option
                  value="freeSpaceP"
                  v-i18n-value="'stats.cstats.'"
                />
                <option
                  value="memory"
                  v-i18n-value="'stats.cstats.'"
                />
                <option
                  value="memoryP"
                  v-i18n-value="'stats.cstats.'"
                />
                <option
                  value="cpu"
                  v-i18n-value="'stats.cstats.'"
                />
                <option
                  value="diskQueue"
                  v-i18n-value="'stats.cstats.'"
                />
                <option
                  value="esQueue"
                  v-i18n-value="'stats.cstats.'"
                />
                <option
                  value="deltaESDroppedPerSec"
                  v-i18n-value="'stats.cstats.'"
                />
                <option
                  value="esHealthMS"
                  v-i18n-value="'stats.cstats.'"
                />
                <option
                  value="packetQueue"
                  v-i18n-value="'stats.cstats.'"
                />
                <option
                  value="closeQueue"
                  v-i18n-value="'stats.cstats.'"
                />
                <option
                  value="needSave"
                  v-i18n-value="'stats.cstats.'"
                />
                <option
                  value="frags"
                  v-i18n-value="'stats.cstats.'"
                />
                <option
                  value="deltaFragsDroppedPerSec"
                  v-i18n-value="'stats.cstats.'"
                />
                <option
                  value="deltaOverloadDroppedPerSec"
                  v-i18n-value="'stats.cstats.'"
                />
                <option
                  value="deltaDupDroppedPerSec"
                  v-i18n-value="'stats.cstats.'"
                />
                <option
                  value="deltaTotalDroppedPerSec"
                  v-i18n-value="'stats.cstats.'"
                />
                <option
                  value="deltaSessionBytesPerSec"
                  v-i18n-value="'stats.cstats.'"
                />
                <option
                  value="sessionSizePerSec"
                  v-i18n-value="'stats.cstats.'"
                />
                <option
                  value="deltaWrittenBytesPerSec"
                  v-i18n-value="'stats.cstats.'"
                />
                <option
                  value="deltaUnwrittenBytesPerSec"
                  v-i18n-value="'stats.cstats.'"
                />
              </select>
            </BInputGroup>
          </BCol> <!-- /graph type select -->

          <!-- graph interval select -->
          <BCol
            cols="auto"
            v-if="tabIndex === 0"
          >
            <BInputGroup size="sm">
              <BInputGroupText>
                {{ $t('stats.graphInterval') }}
              </BInputGroupText>
              <select
                class="form-control"
                v-model="graphInterval"
                @change="graphIntervalChange"
              >
                <option value="5">{{ $t('common.secondCount', 5) }}</option>
                <option value="60">{{ $t('common.minuteCount', 1) }}</option>
                <option value="600">{{ $t('common.minuteCount', 10) }}</option>
              </select>
            </BInputGroup>
          </BCol> <!-- /graph interval select -->

          <!-- graph hide select -->
          <BCol
            cols="auto"
            v-if="tabIndex === 0 || tabIndex === 1"
          >
            <BInputGroup size="sm">
              <BInputGroupText>{{ $t('stats.graphHide') }}</BInputGroupText>
              <select
                class="form-control input-sm"
                v-model="graphHide"
                @change="graphHideChange"
              >
                <option
                  value="none"
                  v-i18n-value="'stats.graphHide-'"
                />
                <option
                  value="old"
                  v-i18n-value="'stats.graphHide-'"
                />
                <option
                  value="nosession"
                  v-i18n-value="'stats.graphHide-'"
                />
                <option
                  value="both"
                  v-i18n-value="'stats.graphHide-'"
                />
              </select>
            </BInputGroup>
          </BCol> <!-- /graph hide select -->

          <!-- graph sort select -->
          <BCol
            cols="auto"
            v-if="tabIndex === 0"
          >
            <BInputGroup size="sm">
              <BInputGroupText>{{ $t('stats.graphSort') }}</BInputGroupText>
              <select
                class="form-control input-sm"
                v-model="graphSort"
              >
                <option
                  value="asc"
                  v-i18n-value="'stats.graphSort-'"
                />
                <option
                  value="desc"
                  v-i18n-value="'stats.graphSort-'"
                />
              </select>
            </BInputGroup>
          </BCol> <!-- /graph hide select -->

          <!-- page size select -->
          <BCol
            cols="auto"
            v-if="tabIndex === 4"
          >
            <BInputGroup size="sm">
              <BInputGroupText>{{ $t('stats.pageSize') }}</BInputGroupText>
              <select
                class="form-control "
                v-model="pageSize"
                @change="pageSizeChange"
              >
                <option value="100">{{ $t('common.perPage', 100) }}</option>
                <option value="200">{{ $t('common.perPage', 200) }}</option>
                <option value="500">{{ $t('common.perPage', 500) }}</option>
                <option value="1000">{{ $t('common.perPage', {count: "1,000"}) }}</option>
                <option value="5000">{{ $t('common.perPage', {count: "5,000"}) }}</option>
                <option value="10000">{{ $t('common.perPage', {count: "10,000"}) }}</option>
              </select>
            </BInputGroup>
          </BCol><!-- /page size select -->

          <!-- table data interval select -->
          <BCol
            cols="auto"
            v-if="tabIndex !== 0 && tabIndex !== 7"
          >
            <BInputGroup size="sm">
              <BInputGroupText>{{ $t('stats.refreshEvery') }}</BInputGroupText>
              <select
                class="form-control"
                v-model="dataInterval"
                @change="dataIntervalChange"
              >
                <option value="5000">{{ $t('common.secondCount', 5) }}</option>
                <option value="15000">{{ $t('common.secondCount', 15) }}</option>
                <option value="30000">{{ $t('common.secondCount', 30) }}</option>
                <option value="60000">{{ $t('common.minuteCount', 1) }}</option>
                <option value="600000">{{ $t('common.minuteCount', 10) }}</option>
                <option value="0">{{ $t('common.never') }}</option>
              </select>
            </BInputGroup>
          </BCol> <!-- /table data interval select -->

          <!-- shards show select -->
          <BCol
            cols="auto"
            v-if="tabIndex === 5"
          >
            <BInputGroup size="sm">
              <BInputGroupText>{{ $t('stats.shardsShow') }}</BInputGroupText>
              <select
                class="form-control"
                v-model="shardsShow"
                @change="shardsShowChange"
              >
                <option
                  value="all"
                  v-i18n-value="'stats.shardsShow-'"
                />
                <option
                  value="UNASSIGNED"
                  v-i18n-value="'stats.shardsShow-'"
                />
                <option
                  value="RELOCATING"
                  v-i18n-value="'stats.shardsShow-'"
                />
                <option
                  value="INITIALIZING"
                  v-i18n-value="'stats.shardsShow-'"
                />
                <option
                  value="notstarted"
                  v-i18n-value="'stats.shardsShow-'"
                />
              </select>
            </BInputGroup>
          </BCol> <!-- /graph hide select -->

          <!-- recovery show select -->
          <BCol
            cols="auto"
            v-if="tabIndex === 6"
          >
            <BInputGroup size="sm">
              <BInputGroupText>{{ $t('stats.recoveryShow') }}</BInputGroupText>
              <select
                class="form-control"
                v-model="recoveryShow"
                @change="recoveryShowChange"
              >
                <option
                  value="all"
                  v-i18n-value="'stats.recoveryShow-'"
                />
                <option
                  value="notdone"
                  v-i18n-value="'stats.recoveryShow-'"
                />
              </select>
            </BInputGroup>
          </BCol> <!-- /graph hide select -->

          <!-- refresh button -->
          <BCol
            cols="auto"
            v-if="tabIndex !== 0 && tabIndex !== 7"
          >
            <BButton
              size="sm"
              variant="theme-tertiary"
              @click="loadData"
            >
              <span v-if="!shiftKeyHold">
                {{ $t('common.refresh') }}
              </span>
              <span
                v-else
                class="enter-icon"
              >
                <span class="fa fa-long-arrow-left fa-lg" />
                <div class="enter-arm" />
              </span>
            </BButton>
          </BCol> <!-- /refresh button -->

          <BCol>
            <!-- confirm button -->
            <transition name="buttons">
              <button
                v-if="confirmMessage"
                type="button"
                class="btn btn-sm btn-danger ms-2"
                @click="confirmed"
              >
                <span class="fa fa-check" />&nbsp;
                {{ confirmMessage }}
              </button>
            </transition> <!-- /confirm button -->

            <!-- cancel confirm button -->
            <transition name="buttons">
              <button
                v-if="confirmMessage"
                type="button"
                class="btn btn-sm btn-warning ms-2"
                @click="cancelConfirm"
              >
                <span class="fa fa-ban" />&nbsp;
                {{ $t('common.cancel') }}
              </button>
            </transition> <!-- /cancel confirm button -->
          </BCol>

          <!-- error (from child component) -->
          <div
            v-if="childError"
            role="alert"
            class="alert alert-sm alert-danger alert-dismissible fade show ms-2"
          >
            {{ childError }}
            <button
              type="button"
              class="btn-close"
              @click="childError = ''"
            >
              <span>&times;</span>
            </button>
          </div> <!-- /error (from child component) -->

          <!-- shrink index -->
          <div
            v-if="shrinkIndex"
            class="ms-4 form-inline"
          >
            <strong>
              {{ $t('stats.shrink') }}  {{ shrinkIndex.index }}
            </strong>
            <!-- new # shards -->
            <div class="input-group input-group-sm ms-2">
              <span class="input-group-text">
                {{ $t('stats.numShards') }}
              </span>
              <select
                v-model="shrinkFactor"
                class="form-control"
                style="-webkit-appearance:none;"
              >
                <option
                  v-for="factor in shrinkFactors"
                  :key="factor"
                  :value="factor"
                >
                  {{ factor }}
                </option>
              </select>
            </div> <!-- /new # shards -->
            <!-- temporary node -->
            <div
              v-if="nodes && temporaryNode"
              class="input-group input-group-sm ms-2"
            >
              <span class="input-group-text">
                {{ $t('stats.temporaryNode') }}
              </span>
              <select
                v-model="temporaryNode"
                class="form-control"
                style="-webkit-appearance:none;"
              >
                <option
                  v-for="node in nodes"
                  :key="node.name"
                  :value="node.name"
                >
                  {{ node.name }}
                </option>
              </select>
            </div> <!-- /new shards input -->
            <!-- ok button -->
            <button
              class="btn btn-sm btn-success pull-right ms-2"
              @click="executeShrink(shrinkIndex)"
              type="button"
            >
              <span class="fa fa-check" />
            </button> <!-- /ok button -->
            <!-- cancel button -->
            <button
              class="btn btn-sm btn-warning pull-right ms-2"
              @click="cancelShrink"
              type="button"
            >
              <span class="fa fa-ban" />
            </button> <!-- /cancel button -->
          </div>
          <span
            v-if="shrinkIndex && shrinkError"
            class="text-danger ms-2"
          >
            {{ shrinkError }}
          </span> <!-- /shrink index -->

          <!-- select cluster(s) -->
          <BCol
            cols="auto"
            v-if="multiviewer"
          >
            <Clusters
              @update-cluster="updateCluster"
              :select-one="clusterParamOverride && tabIndex > 1"
            />
          </BCol> <!-- /select cluster(s) -->

          <!-- need this on non-multivierwer esAdmin tab to keep the layout consistent (empty navbar)-->
          <div v-else-if="tabIndex === 7">
            <button
              class="btn btn-sm btn-theme-secondary"
              style="visibility: hidden;"
            >placeholder</button>
          </div>

        </BRow> <!-- /stats sub navbar -->
      </span>
    </ArkimeCollapsible>

    <!-- stats content -->
    <div class="stats-tabs">
      <b-tabs
        lazy
        :model-value="tabIndex"
        @update:index="tabIndexChange($event)"
      >
        <b-tab
          :title="$t('stats.nav.captureGraphs')"
          :active="tabIndex === 0"
        >
          <capture-graphs
            :refresh-data="refreshData"
            :search-term="searchTerm"
            :graph-type="statsType"
            :graph-interval="graphInterval"
            :graph-hide="graphHide"
            :graph-sort="graphSort"
            :cluster="cluster"
            :user="user"
          />
        </b-tab>
        <b-tab
          :title="$t('stats.nav.captureStats')"
          :active="tabIndex === 1"
        >
          <capture-stats
            :graph-hide="graphHide"
            :refresh-data="refreshData"
            :search-term="searchTerm"
            :data-interval="dataInterval"
            :cluster="cluster"
            :user="user"
          />
        </b-tab>
        <b-tab
          :title="$t('stats.nav.esNodes')"
          :active="tabIndex === 2"
        >
          <es-nodes
            :refresh-data="refreshData"
            :search-term="searchTerm"
            :data-interval="dataInterval"
            :cluster="cluster"
          />
        </b-tab>
        <b-tab
          :title="$t('stats.nav.esIndices')"
          :active="tabIndex === 3"
        >
          <es-indices
            :refresh-data="refreshData"
            :data-interval="dataInterval"
            @errored="onError"
            @confirm="confirm"
            @shrink="shrink"
            :search-term="searchTerm"
            :issue-confirmation="issueConfirmation"
            :user="user"
            :cluster="cluster"
          />
        </b-tab>
        <b-tab
          :title="$t('stats.nav.esTasks')"
          :active="tabIndex === 4"
        >
          <es-tasks
            :data-interval="dataInterval"
            :refresh-data="refreshData"
            :search-term="searchTerm"
            :page-size="pageSize"
            :user="user"
            @errored="onError"
            :cluster="cluster"
          />
        </b-tab>
        <b-tab
          :title="$t('stats.nav.esShards')"
          :active="tabIndex === 5"
        >
          <es-shards
            :shards-show="shardsShow"
            :refresh-data="refreshData"
            :search-term="searchTerm"
            :data-interval="dataInterval"
            :cluster="cluster"
          />
        </b-tab>
        <b-tab
          :title="$t('stats.nav.esRecovery')"
          :active="tabIndex === 6"
        >
          <es-recovery
            :recovery-show="recoveryShow"
            :data-interval="dataInterval"
            :refresh-data="refreshData"
            :search-term="searchTerm"
            :user="user"
            :cluster="cluster"
          />
        </b-tab>
        <b-tab
          :title="$t('stats.nav.esAdmin')"
          :active="tabIndex === 7"
          v-if="user.esAdminUser"
        >
          <es-admin
            :data-interval="dataInterval"
            :refresh-data="refreshData"
            :user="user"
            :cluster="cluster"
          />
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
import Focus from '@common/Focus.vue';
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
