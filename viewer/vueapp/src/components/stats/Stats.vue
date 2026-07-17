<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <page-layout class="stats-content">
    <template #chrome>
      <ArkimeCollapsible>
        <div class="page-toolbar">
          <!-- stats sub navbar -->
          <v-row class="g-1 stats-form p-1 align-center justify-start page-subnav">
            <v-col
              cols="auto"
              class="flex-grow-1"
              v-if="tabIndex !== 7">
              <div class="arkime-input-group arkime-input-group--fluid">
                <span class="arkime-input-label arkime-input-label-fw">
                  <v-icon
                    icon="mdi-loading"
                    class="mdi-spin text-theme-accent"
                    v-if="loadingData" />
                  <v-icon
                    icon="mdi-magnify"
                    v-else-if="!shiftKeyHold" />
                  <span
                    v-else-if="shiftKeyHold"
                    class="query-shortcut">
                    Q
                  </span>
                </span>
                <input
                  type="text"
                  class="arkime-input-control"
                  v-model="searchTerm"
                  v-focus="focusInput"
                  @blur="onOffFocus"
                  @input="debounceSearchInput"
                  @keydown.stop.prevent.enter="debounceSearchInput"
                  :placeholder="$t('stats.filterPlaceholder')">
                <v-btn
                  v-if="searchTerm"
                  variant="outlined"
                  size="x-small"
                  density="comfortable"
                  icon
                  :disabled="!searchTerm"
                  @click="clear">
                  <v-icon icon="mdi-close" />
                </v-btn>
              </div>
            </v-col>

            <!-- graph type select -->
            <v-col
              cols="auto"
              v-if="tabIndex === 0">
              <div class="arkime-input-group">
                <span class="arkime-input-label">
                  {{ $t('stats.graphType') }}
                </span>
                <select
                  class="arkime-input-control"
                  v-model="statsType"
                  @change="statsTypeChange">
                  <option
                    value="deltaPacketsPerSec"
                    v-i18n-value="'stats.cstats.'" />
                  <option
                    value="deltaBytesPerSec"
                    v-i18n-value="'stats.cstats.'" />
                  <option
                    value="deltaBitsPerSec"
                    v-i18n-value="'stats.cstats.'" />
                  <option
                    value="deltaSessionsPerSec"
                    v-i18n-value="'stats.cstats.'" />
                  <option
                    value="deltaDroppedPerSec"
                    v-i18n-value="'stats.cstats.'" />
                  <option
                    value="monitoring"
                    v-i18n-value="'stats.cstats.'" />
                  <option
                    value="tcpSessions"
                    v-i18n-value="'stats.cstats.'" />
                  <option
                    value="udpSessions"
                    v-i18n-value="'stats.cstats.'" />
                  <option
                    value="icmpSessions"
                    v-i18n-value="'stats.cstats.'" />
                  <option
                    value="sctpSessions"
                    v-i18n-value="'stats.cstats.'" />
                  <option
                    value="espSessions"
                    v-i18n-value="'stats.cstats.'" />
                  <option
                    value="usedSpaceM"
                    v-i18n-value="'stats.cstats.'" />
                  <option
                    value="freeSpaceM"
                    v-i18n-value="'stats.cstats.'" />
                  <option
                    value="freeSpaceP"
                    v-i18n-value="'stats.cstats.'" />
                  <option
                    value="memory"
                    v-i18n-value="'stats.cstats.'" />
                  <option
                    value="memoryP"
                    v-i18n-value="'stats.cstats.'" />
                  <option
                    value="cpu"
                    v-i18n-value="'stats.cstats.'" />
                  <option
                    value="diskQueue"
                    v-i18n-value="'stats.cstats.'" />
                  <option
                    value="esQueue"
                    v-i18n-value="'stats.cstats.'" />
                  <option
                    value="deltaESDroppedPerSec"
                    v-i18n-value="'stats.cstats.'" />
                  <option
                    value="esHealthMS"
                    v-i18n-value="'stats.cstats.'" />
                  <option
                    value="packetQueue"
                    v-i18n-value="'stats.cstats.'" />
                  <option
                    value="closeQueue"
                    v-i18n-value="'stats.cstats.'" />
                  <option
                    value="needSave"
                    v-i18n-value="'stats.cstats.'" />
                  <option
                    value="frags"
                    v-i18n-value="'stats.cstats.'" />
                  <option
                    value="deltaFragsDroppedPerSec"
                    v-i18n-value="'stats.cstats.'" />
                  <option
                    value="deltaOverloadDroppedPerSec"
                    v-i18n-value="'stats.cstats.'" />
                  <option
                    value="deltaDupDroppedPerSec"
                    v-i18n-value="'stats.cstats.'" />
                  <option
                    value="deltaTotalDroppedPerSec"
                    v-i18n-value="'stats.cstats.'" />
                  <option
                    value="deltaSessionBytesPerSec"
                    v-i18n-value="'stats.cstats.'" />
                  <option
                    value="sessionSizePerSec"
                    v-i18n-value="'stats.cstats.'" />
                  <option
                    value="deltaWrittenBytesPerSec"
                    v-i18n-value="'stats.cstats.'" />
                  <option
                    value="deltaUnwrittenBytesPerSec"
                    v-i18n-value="'stats.cstats.'" />
                </select>
              </div>
            </v-col> <!-- /graph type select -->

            <!-- graph interval select -->
            <v-col
              cols="auto"
              v-if="tabIndex === 0">
              <div class="arkime-input-group">
                <span class="arkime-input-label">
                  {{ $t('stats.graphInterval') }}
                </span>
                <select
                  class="arkime-input-control"
                  v-model="graphInterval"
                  @change="graphIntervalChange">
                  <option value="5">
                    {{ $t('common.secondCount', 5) }}
                  </option>
                  <option value="60">
                    {{ $t('common.minuteCount', 1) }}
                  </option>
                  <option value="600">
                    {{ $t('common.minuteCount', 10) }}
                  </option>
                </select>
              </div>
            </v-col> <!-- /graph interval select -->

            <!-- graph hide select -->
            <v-col
              cols="auto"
              v-if="tabIndex === 0 || tabIndex === 1">
              <div class="arkime-input-group">
                <span class="arkime-input-label">{{ $t('stats.graphHide') }}</span>
                <select
                  class="arkime-input-control"
                  v-model="graphHide"
                  @change="graphHideChange">
                  <option
                    value="none"
                    v-i18n-value="'stats.graphHide-'" />
                  <option
                    value="old"
                    v-i18n-value="'stats.graphHide-'" />
                  <option
                    value="nosession"
                    v-i18n-value="'stats.graphHide-'" />
                  <option
                    value="both"
                    v-i18n-value="'stats.graphHide-'" />
                </select>
              </div>
            </v-col> <!-- /graph hide select -->

            <!-- graph sort select -->
            <v-col
              cols="auto"
              v-if="tabIndex === 0">
              <div class="arkime-input-group">
                <span class="arkime-input-label">{{ $t('stats.graphSort') }}</span>
                <select
                  class="arkime-input-control"
                  v-model="graphSort">
                  <option
                    value="asc"
                    v-i18n-value="'stats.graphSort-'" />
                  <option
                    value="desc"
                    v-i18n-value="'stats.graphSort-'" />
                </select>
              </div>
            </v-col> <!-- /graph sort select -->

            <!-- page size select -->
            <v-col
              cols="auto"
              v-if="tabIndex === 4">
              <div class="arkime-input-group">
                <span class="arkime-input-label">{{ $t('stats.pageSize') }}</span>
                <select
                  class="arkime-input-control"
                  v-model="pageSize"
                  @change="pageSizeChange">
                  <option value="100">
                    {{ $t('common.perPage', 100) }}
                  </option>
                  <option value="200">
                    {{ $t('common.perPage', 200) }}
                  </option>
                  <option value="500">
                    {{ $t('common.perPage', 500) }}
                  </option>
                  <option value="1000">
                    {{ $t('common.perPage', {count: "1,000"}) }}
                  </option>
                  <option value="5000">
                    {{ $t('common.perPage', {count: "5,000"}) }}
                  </option>
                  <option value="10000">
                    {{ $t('common.perPage', {count: "10,000"}) }}
                  </option>
                </select>
              </div>
            </v-col><!-- /page size select -->

            <!-- table data interval select -->
            <v-col
              cols="auto"
              v-if="tabIndex !== 0 && tabIndex !== 7">
              <div class="arkime-input-group">
                <span class="arkime-input-label">{{ $t('stats.refreshEvery') }}</span>
                <select
                  class="arkime-input-control"
                  v-model="dataInterval"
                  @change="dataIntervalChange">
                  <option value="5000">
                    {{ $t('common.secondCount', 5) }}
                  </option>
                  <option value="15000">
                    {{ $t('common.secondCount', 15) }}
                  </option>
                  <option value="30000">
                    {{ $t('common.secondCount', 30) }}
                  </option>
                  <option value="60000">
                    {{ $t('common.minuteCount', 1) }}
                  </option>
                  <option value="600000">
                    {{ $t('common.minuteCount', 10) }}
                  </option>
                  <option value="0">
                    {{ $t('common.never') }}
                  </option>
                </select>
              </div>
            </v-col> <!-- /table data interval select -->

            <!-- shards show select -->
            <v-col
              cols="auto"
              v-if="tabIndex === 5">
              <div class="arkime-input-group">
                <span class="arkime-input-label">{{ $t('stats.shardsShow') }}</span>
                <select
                  class="arkime-input-control"
                  v-model="shardsShow"
                  @change="shardsShowChange">
                  <option
                    value="all"
                    v-i18n-value="'stats.shardsShow-'" />
                  <option
                    value="UNASSIGNED"
                    v-i18n-value="'stats.shardsShow-'" />
                  <option
                    value="RELOCATING"
                    v-i18n-value="'stats.shardsShow-'" />
                  <option
                    value="INITIALIZING"
                    v-i18n-value="'stats.shardsShow-'" />
                  <option
                    value="notstarted"
                    v-i18n-value="'stats.shardsShow-'" />
                </select>
              </div>
            </v-col> <!-- /shards show select -->

            <!-- recovery show select -->
            <v-col
              cols="auto"
              v-if="tabIndex === 6">
              <div class="arkime-input-group">
                <span class="arkime-input-label">{{ $t('stats.recoveryShow') }}</span>
                <select
                  class="arkime-input-control"
                  v-model="recoveryShow"
                  @change="recoveryShowChange">
                  <option
                    value="all"
                    v-i18n-value="'stats.recoveryShow-'" />
                  <option
                    value="notdone"
                    v-i18n-value="'stats.recoveryShow-'" />
                </select>
              </div>
            </v-col> <!-- /recovery show select -->

            <!-- refresh button -->
            <v-col
              cols="auto"
              v-if="tabIndex !== 0 && tabIndex !== 7">
              <v-btn
                variant="flat"
                size="large"
                density="comfortable"
                :style="tertiaryBtnStyle"
                @click="loadData">
                <span v-if="!shiftKeyHold">
                  {{ $t('common.refresh') }}
                </span>
                <span
                  v-else
                  class="enter-icon">
                  <v-icon
                    icon="mdi-arrow-left"
                    size="small" />
                  <div class="enter-arm" />
                </span>
              </v-btn>
            </v-col> <!-- /refresh button -->

            <v-col cols="auto">
              <!-- confirm button -->
              <transition name="buttons">
                <v-btn
                  v-if="confirmMessage"
                  color="error"
                  variant="flat"
                  size="small"
                  density="comfortable"
                  class="ms-2"
                  @click="confirmed">
                  <v-icon
                    icon="mdi-check"
                    class="me-1" />
                  {{ confirmMessage }}
                </v-btn>
              </transition> <!-- /confirm button -->

              <!-- cancel confirm button -->
              <transition name="buttons">
                <v-btn
                  v-if="confirmMessage"
                  color="warning"
                  variant="flat"
                  size="small"
                  density="comfortable"
                  class="ms-2"
                  @click="cancelConfirm">
                  <v-icon
                    icon="mdi-cancel"
                    class="me-1" />
                  {{ $t('common.cancel') }}
                </v-btn>
              </transition> <!-- /cancel confirm button -->
            </v-col>

            <!-- error (from child component) -->
            <v-alert
              v-if="childError"
              type="error"
              density="compact"
              variant="tonal"
              closable
              class="ms-2"
              @click:close="childError = ''">
              {{ childError }}
            </v-alert> <!-- /error (from child component) -->

            <!-- shrink index -->
            <div
              v-if="shrinkIndex"
              class="ms-4 d-flex align-center">
              <strong>
                {{ $t('stats.shrink') }}  {{ shrinkIndex.index }}
              </strong>
              <!-- new # shards -->
              <div class="arkime-input-group ms-2">
                <span class="arkime-input-label">
                  {{ $t('stats.numShards') }}
                </span>
                <select
                  v-model="shrinkFactor"
                  class="arkime-input-control"
                  style="-webkit-appearance:none;">
                  <option
                    v-for="factor in shrinkFactors"
                    :key="factor"
                    :value="factor">
                    {{ factor }}
                  </option>
                </select>
              </div> <!-- /new # shards -->
              <!-- temporary node -->
              <div
                v-if="nodes && temporaryNode"
                class="arkime-input-group ms-2">
                <span class="arkime-input-label">
                  {{ $t('stats.temporaryNode') }}
                </span>
                <select
                  v-model="temporaryNode"
                  class="arkime-input-control"
                  style="-webkit-appearance:none;">
                  <option
                    v-for="node in nodes"
                    :key="node.name"
                    :value="node.name">
                    {{ node.name }}
                  </option>
                </select>
              </div> <!-- /new shards input -->
              <!-- ok button -->
              <v-btn
                color="success"
                variant="flat"
                size="small"
                density="comfortable"
                icon
                class="float-right ms-2"
                :aria-label="$t('common.apply')"
                @click="executeShrink(shrinkIndex)">
                <v-icon icon="mdi-check" />
              </v-btn> <!-- /ok button -->
              <!-- cancel button -->
              <v-btn
                color="warning"
                variant="flat"
                size="small"
                density="comfortable"
                icon
                class="float-right ms-2"
                :aria-label="$t('common.cancel')"
                @click="cancelShrink">
                <v-icon icon="mdi-cancel" />
              </v-btn> <!-- /cancel button -->
            </div>
            <span
              v-if="shrinkIndex && shrinkError"
              class="text-danger ms-2">
              {{ shrinkError }}
            </span> <!-- /shrink index -->

            <!-- select cluster(s) -->
            <v-col
              cols="auto"
              v-if="multiviewer">
              <Clusters
                @update-cluster="updateCluster"
                :select-one="clusterParamOverride && tabIndex > 1" />
            </v-col> <!-- /select cluster(s) -->

            <!-- spacer on esAdmin so the sub-navbar keeps the same height
               as on other tabs (all real controls are v-if-hidden when
               tabIndex === 7); without this the row collapses and leaves
               a gap above the tab strip. -->
            <v-col
              v-if="tabIndex === 7"
              cols="auto">
            &nbsp;
            </v-col>
          </v-row> <!-- /stats sub navbar -->
        </div>
      </ArkimeCollapsible>

      <!-- tab strip: chrome row outside the collapsible so the tabs stay
           visible when the toolbar is collapsed -->
      <div class="stats-tabs">
        <div class="stats-tab-bar">
          <v-btn-toggle
            :model-value="tabIndex"
            @update:model-value="tabIndexChange($event)"
            density="compact"
            variant="text"
            color="primary"
            mandatory
            class="stats-tab-strip">
            <v-btn :value="0">
              <v-icon
                start
                icon="mdi-chart-areaspline" />
              {{ $t('stats.nav.captureGraphs') }}
            </v-btn>
            <v-btn :value="1">
              <v-icon
                start
                icon="mdi-speedometer" />
              {{ $t('stats.nav.captureStats') }}
            </v-btn>
            <span class="stats-tab-divider" />
            <v-btn :value="2">
              <v-icon
                start
                icon="mdi-server" />
              {{ $t('stats.nav.esNodes') }}
            </v-btn>
            <v-btn :value="3">
              <v-icon
                start
                icon="mdi-database" />
              {{ $t('stats.nav.esIndices') }}
            </v-btn>
            <v-btn :value="4">
              <v-icon
                start
                icon="mdi-format-list-checks" />
              {{ $t('stats.nav.esTasks') }}
            </v-btn>
            <v-btn :value="5">
              <v-icon
                start
                icon="mdi-sitemap" />
              {{ $t('stats.nav.esShards') }}
            </v-btn>
            <v-btn :value="6">
              <v-icon
                start
                icon="mdi-lifebuoy" />
              {{ $t('stats.nav.esRecovery') }}
            </v-btn>
            <v-btn
              v-if="user.esAdminUser"
              :value="7">
              <v-icon
                start
                icon="mdi-cog-outline" />
              {{ $t('stats.nav.esAdmin') }}
            </v-btn>
          </v-btn-toggle>
        </div>
      </div>
    </template>

    <!-- stats content -->
    <div class="stats-tabs">
      <!-- Lazy-mount each tab pane via v-if so child components only initialize
           when their tab is active. -->
      <div class="stats-tab-content">
        <capture-graphs
          v-if="tabIndex === 0"
          :refresh-data="refreshData"
          :search-term="searchTerm"
          :graph-type="statsType"
          :graph-interval="graphInterval"
          :graph-hide="graphHide"
          :graph-sort="graphSort"
          :cluster="cluster"
          :user="user" />
        <capture-stats
          v-else-if="tabIndex === 1"
          :graph-hide="graphHide"
          :refresh-data="refreshData"
          :search-term="searchTerm"
          :data-interval="dataInterval"
          :cluster="cluster"
          :user="user" />
        <es-nodes
          v-else-if="tabIndex === 2"
          :refresh-data="refreshData"
          :search-term="searchTerm"
          :data-interval="dataInterval"
          :cluster="cluster" />
        <es-indices
          v-else-if="tabIndex === 3"
          :refresh-data="refreshData"
          :data-interval="dataInterval"
          @errored="onError"
          @confirm="confirm"
          @shrink="shrink"
          :search-term="searchTerm"
          :issue-confirmation="issueConfirmation"
          :user="user"
          :cluster="cluster" />
        <es-tasks
          v-else-if="tabIndex === 4"
          :data-interval="dataInterval"
          :refresh-data="refreshData"
          :search-term="searchTerm"
          :page-size="pageSize"
          :user="user"
          @errored="onError"
          :cluster="cluster" />
        <es-shards
          v-else-if="tabIndex === 5"
          :shards-show="shardsShow"
          :refresh-data="refreshData"
          :search-term="searchTerm"
          :data-interval="dataInterval"
          :cluster="cluster" />
        <es-recovery
          v-else-if="tabIndex === 6"
          :recovery-show="recoveryShow"
          :data-interval="dataInterval"
          :refresh-data="refreshData"
          :search-term="searchTerm"
          :user="user"
          :cluster="cluster" />
        <es-admin
          v-else-if="tabIndex === 7 && user.esAdminUser"
          :data-interval="dataInterval"
          :refresh-data="refreshData"
          :user="user"
          :cluster="cluster" />
      </div>
    </div> <!-- /stats content -->
  </page-layout>
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
import PageLayout from '../utils/PageLayout.vue';
import Utils from '../utils/utils';
import Focus from '@common/Focus.vue';
import Clusters from '../utils/Clusters.vue';
import StatsService from './StatsService.js';
import { resolveMessage } from '@common/resolveI18nMessage';

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
    PageLayout,
    Clusters
  },
  directives: { Focus },
  data: function () {
    return {
      tabIndex: parseInt(this.$route.query.statsTab, 10) || 0,
      statsType: this.$route.query.type || 'deltaPacketsPerSec',
      graphInterval: parseInt(this.$route.query.gtime, 10) || 5,
      graphHide: this.$route.query.hide || 'none',
      graphSort: this.$route.query.sort || 'asc',
      recoveryShow: this.$route.query.recoveryShow || 'notdone',
      shardsShow: this.$route.query.shardsShow || 'notstarted',
      dataInterval: parseInt(this.$route.query.refreshInterval, 10) || 15000,
      pageSize: parseInt(this.$route.query.size, 10) || 500,
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
      multiviewer: this.$constants.MULTIVIEWER,
      // Arkime theme-color v-btn styles. Vuetify's :color doesn't
      // resolve CSS variables; inline :style keeps them theme-adaptive.
      tertiaryBtnStyle: {
        backgroundColor: 'rgb(var(--v-theme-tertiary))',
        color: 'rgb(var(--v-theme-button-fg))'
      },
      secondaryBtnStyle: {
        backgroundColor: 'rgb(var(--v-theme-secondary))',
        color: 'rgb(var(--v-theme-button-fg))'
      }
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
      if (queryParams.gtime) {
        this.graphInterval = parseInt(queryParams.gtime, 10);
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
        this.dataInterval = parseInt(queryParams.refreshInterval, 10);
      }
      if (queryParams.cluster) {
        this.cluster = queryParams.cluster;
      }
      if (queryParams.size) {
        this.pageSize = parseInt(queryParams.size, 10);
      }
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
        this.nodes = response.data;
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
        if (!response.success) {
          this.shrinkError = resolveMessage(response, this.$t);
          return;
        }
        this.cancelShrink();
      } catch (error) {
        this.shrinkError = resolveMessage(error, this.$t);
      }
    }
  }
};
</script>

<style>
/* The tab strip lives inside its own sub-navbar bar -- a tinted
   horizontal band fixed at the top of the page that contains the
   pill-style v-btn-toggle. Matches the visual rhythm of stats-form
   (the search/cluster/refresh row above) but uses a different theme
   tint so the two rows read as separate strata. */
.stats-tabs .stats-tab-bar {
  padding: 6px 12px;
  background-color: rgb(var(--v-theme-secondary-lightest));
  border-bottom: 1px solid rgb(var(--v-theme-neutral-light));
  box-shadow: 0 1px 4px rgba(0, 0, 0, 0.06);
}
/* v-btn-toggle at density="compact" has a baked-in height (~24px) that
   clips taller children -- pin it to the pill height and let it grow
   so the active pill isn't cropped. */
.stats-tabs .stats-tab-strip {
  background-color: transparent !important;
  border: 0 !important;
  gap: 2px;
  height: auto !important;
  min-height: 34px !important;
  overflow: visible !important;
}
/* Strip the button-group chrome (no shared border) so the tabs read
   as a nav strip rather than a segmented control. Inactive tabs are
   plain text; the active tab gets a soft tonal-primary pill. */
.stats-tabs .stats-tab-strip .v-btn {
  text-transform: none !important;
  letter-spacing: 0 !important;
  font-size: 0.9rem !important;
  font-weight: 500 !important;
  padding: 0 14px !important;
  height: 34px !important;
  min-width: 0 !important;
  border-radius: 17px !important;
  border: 0 !important;
  color: rgb(var(--v-theme-foreground)) !important;
  opacity: 0.78;
  transform: translateY(3px);
}
.stats-tabs .stats-tab-strip .v-btn:hover {
  background-color: rgb(var(--v-theme-background)) !important;
  opacity: 1;
}
.stats-tabs .stats-tab-strip .v-btn--active {
  background-color: rgb(var(--v-theme-primary)) !important;
  color: rgb(var(--v-theme-button-fg)) !important;
  opacity: 1;
  font-weight: 700 !important;
  box-shadow: 0 1px 3px rgba(0, 0, 0, 0.18);
}
.stats-tabs .stats-tab-strip .v-btn--active:hover {
  background-color: rgb(var(--v-theme-primary)) !important;
  filter: brightness(1.08);
}
.stats-tabs .stats-tab-strip .v-btn .v-btn__content {
  transform: translateY(-1px);
}
.stats-tabs .stats-tab-strip .v-btn .v-icon {
  font-size: 15px;
  margin-inline-end: 6px;
}
/* visual separator between the Capture group and the ES group. */
.stats-tabs .stats-tab-strip .stats-tab-divider {
  align-self: center;
  width: 1px;
  height: 20px;
  margin: 0 8px;
  background-color: rgb(var(--v-theme-neutral-light));
}

/* shrink the column header font on stats tables one notch so that the
   long headers (Sessions/s, Packet Q, Free Space, etc.) fit without
   needing per-column width tweaks */
.stats-content .arkime-table > thead > tr > th {
  font-size: 0.875rem;
}
</style>

<style scoped>

/* apply theme colors to subnavbar */
.stats-form {
  z-index : 6;
  background-color: rgb(var(--v-theme-quaternary-lightest));
}

/* remove browser styles on select box (mostly for border-radius) */
select {
  -webkit-appearance: none;
}

</style>
