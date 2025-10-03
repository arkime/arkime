<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>

  <div class="container-fluid">

    <arkime-loading v-if="initialLoading && !error">
    </arkime-loading>

    <arkime-error v-if="error"
      :message="error">
    </arkime-error>

    <div v-show="!error">

      <span id="captureStatsHelp"
        class="fa fa-lg fa-question-circle-o cursor-help mt-2 pull-right">
        <BTooltip target="captureStatsHelp">
          <span v-html="$t('stats.cstats.helpTipHtml')" />
        </BTooltip>
      </span>

      <arkime-paging v-if="stats"
        class="mt-2"
        :records-total="recordsTotal"
        :records-filtered="recordsFiltered"
        v-on:changePaging="changePaging"
        :length-default=200>
      </arkime-paging>

      <arkime-table
        id="captureStatsTable"
        :data="stats"
        :loadData="loadData"
        :columns="columns"
        :no-results="true"
        :show-avg-tot="true"
        :action-column="true"
        :info-row="true"
        :info-row-function="toggleStatDetailWrapper"
        :desc="query.desc"
        :sortField="query.sortField"
        :no-results-msg="$t( cluster ? 'stats.noResultsCluster' : 'stats.noResults' )"
        page="captureStats"
        table-animation="list"
        table-state-name="captureStatsCols"
        table-widths-state-name="captureStatsColWidths"
        table-classes="table-sm table-hover text-end small">
      </arkime-table>

    </div>

  </div>

</template>

<script>
import '../../cubismoverrides.css';
import Utils from '../utils/utils';
import ArkimeError from '../utils/Error.vue';
import ArkimeTable from '../utils/Table.vue';
import ArkimeLoading from '../utils/Loading.vue';
import ArkimePaging from '../utils/Pagination.vue';
import StatsService from './StatsService.js';
import { round, roundCommaString, timezoneDateString, humanReadableBytes, humanReadableBits, readableTime, readableTimeCompact } from '@common/vueFilters.js';

let oldD3, cubism; // lazy load old d3 and cubism

let reqPromise; // promise returned from setInterval for recurring requests
let respondedAt; // the time that the last data load successfully responded

export default {
  name: 'NodeStats',
  props: [
    'user',
    'graphType',
    'graphInterval',
    'graphHide',
    'dataInterval',
    'refreshData',
    'searchTerm',
    'cluster'
  ],
  components: {
    ArkimePaging,
    ArkimeError,
    ArkimeLoading,
    ArkimeTable
  },
  data: function () {
    const $t = this.$t.bind(this);
    function intl(obj) {
      obj.name = $t('stats.cstats.' + obj.id);
      return obj;
    }
    return {
      error: '',
      initialLoading: true,
      stats: null,
      recordsTotal: undefined,
      recordsFiltered: undefined,
      totalValues: null,
      averageValues: null,
      showNodeStats: true,
      expandedNodeStats: {},
      query: {
        length: parseInt(this.$route.query.length) || 200,
        start: 0,
        filter: this.searchTerm || undefined,
        sortField: 'nodeName',
        desc: true,
        hide: this.graphHide || 'none',
        cluster: this.cluster || undefined
      },
      columns: [ // node stats table columns
        // default columns
        intl({ id: 'nodeName', classes: 'text-start', sort: 'nodeName', width: 120, default: true, doStats: false }),
        intl({ id: 'currentTime', sort: 'currentTime', width: 200, default: true, doStats: false, dataFunction: (item) => { return timezoneDateString(item.currentTime * 1000, this.user.settings.timezone, false); } }),
        intl({ id: 'monitoring', sort: 'monitoring', width: 100, default: true, doStats: true, dataFunction: (item) => { return roundCommaString(item.monitoring); } }),
        intl({ id: 'freeSpaceM', sort: 'freeSpaceM', width: 120, default: true, doStats: true, dataFunction: (item) => { return humanReadableBytes(item.freeSpaceM * 1000000) + ' (' + round(item.freeSpaceP, 1) + '%)'; }, avgTotFunction: (item) => { return humanReadableBytes(item.freeSpaceM * 1000000); } }),
        intl({ id: 'cpu', sort: 'cpu', width: 80, default: true, doStats: true, dataFunction: (item) => { return round(item.cpu / 100.0, 1) + '%'; } }),
        intl({ id: 'memory', sort: 'memory', width: 120, default: true, doStats: true, dataFunction: (item) => { return humanReadableBytes(item.memory) + ' (' + round(item.memoryP, 1) + '%)'; }, avgTotFunction: (item) => { return humanReadableBytes(item.memory); } }),
        intl({ id: 'packetQueue', sort: 'packetQueue', width: 95, default: true, doStats: true, dataFunction: (item) => { return roundCommaString(item.packetQueue); } }),
        intl({ id: 'diskQueue', sort: 'diskQueue', width: 85, default: true, doStats: true, dataFunction: (item) => { return roundCommaString(item.diskQueue); } }),
        intl({ id: 'esQueue', sort: 'esQueue', width: 75, default: true, doStats: true, dataFunction: (item) => { return roundCommaString(item.esQueue); } }),
        // deltaPackets, deltaSessions, deltaDropped use an id that doesn't match sort to not break saved columns
        intl({ id: 'deltaPackets', sort: 'deltaPacketsPerSec', width: 100, default: true, doStats: true, dataFunction: (item) => { return roundCommaString(item.deltaPacketsPerSec); } }),
        intl({ id: 'deltaBytesPerSec', sort: 'deltaBytesPerSec', width: 80, dataFunction: (item) => { return humanReadableBytes(item.deltaBytesPerSec); }, default: true, doStats: true }),
        intl({ id: 'deltaSessions', sort: 'deltaSessionsPerSec', width: 100, default: true, doStats: true, dataFunction: (item) => { return roundCommaString(item.deltaSessionsPerSec); } }),
        intl({ id: 'deltaDropped', sort: 'deltaDroppedPerSec', width: 130, default: true, doStats: true, dataFunction: (item) => { return roundCommaString(item.deltaDroppedPerSec); } }),
        // all the rest of the available stats
        intl({ id: 'deltaBitsPerSec', sort: 'deltaBitsPerSec', width: 100, doStats: true, dataFunction: (item) => { return humanReadableBits(item.deltaBitsPerSec); } }),
        intl({ id: 'deltaWrittenBytesPerSec', sort: 'deltaWrittenBytesPerSec', width: 100, doStats: true, dataFunction: (item) => { return humanReadableBytes(item.deltaWrittenBytesPerSec); } }),
        intl({ id: 'deltaUnwrittenBytesPerSec', sort: 'deltaUnwrittenBytesPerSec', width: 100, doStats: true, dataFunction: (item) => { return humanReadableBytes(item.deltaUnwrittenBytesPerSec); } }),
        intl({ id: 'tcpSessions', sort: 'tcpSessions', width: 100, doStats: true, dataFunction: (item) => { return roundCommaString(item.tcpSessions); } }),
        intl({ id: 'udpSessions', sort: 'udpSessions', width: 100, doStats: true, dataFunction: (item) => { return roundCommaString(item.udpSessions); } }),
        intl({ id: 'icmpSessions', sort: 'icmpSessions', width: 100, doStats: true, dataFunction: (item) => { return roundCommaString(item.icmpSessions); } }),
        intl({ id: 'sctpSessions', sort: 'sctpSessions', width: 100, doStats: true, dataFunction: (item) => { return roundCommaString(item.sctpSessions); } }),
        intl({ id: 'espSessions', sort: 'espSessions', width: 100, doStats: true, dataFunction: (item) => { return roundCommaString(item.espSessions); } }),
        intl({ id: 'usedSpaceM', sort: 'usedSpaceM', width: 100, doStats: true, dataFunction: (item) => { return humanReadableBytes(item.usedSpaceM * 1000000); } }),
        intl({ id: 'esHealthMS', sort: 'esHealthMS', width: 100, doStats: true, dataFunction: (item) => { return roundCommaString(item.esHealthMS); } }),
        intl({ id: 'closeQueue', sort: 'closeQueue', width: 100, doStats: true, dataFunction: (item) => { return roundCommaString(item.closeQueue); } }),
        intl({ id: 'needSave', sort: 'needSave', width: 100, doStats: true, dataFunction: (item) => { return roundCommaString(item.needSave); } }),
        intl({ id: 'frags', sort: 'frags', width: 100, doStats: true, dataFunction: (item) => { return roundCommaString(item.frags); } }),
        intl({ id: 'deltaFragsDroppedPerSec', sort: 'deltaFragsDroppedPerSec', width: 100, doStats: true, dataFunction: (item) => { return roundCommaString(item.deltaFragsDroppedPerSec); } }),
        intl({ id: 'deltaTotalDroppedPerSec', sort: 'deltaTotalDroppedPerSec', width: 100, doStats: true, dataFunction: (item) => { return roundCommaString(item.deltaTotalDroppedPerSec); } }),
        intl({ id: 'deltaSessionBytesPerSec', sort: 'deltaSessionBytesPerSec', width: 100, doStats: true, dataFunction: (item) => { return humanReadableBytes(item.deltaSessionBytesPerSec); } }),
        intl({ id: 'deltaOverloadDropped', sort: 'deltaOverloadDropped', width: 140, doStats: true, dataFunction: (item) => { return roundCommaString(item.deltaOverloadDropped); } }),
        intl({ id: 'deltaDupDroppedPerSec', sort: 'deltaDupDroppedPerSec', width: 120, doStats: true, dataFunction: (item) => { return roundCommaString(item.deltaDupDroppedPerSec); } }),
        intl({ id: 'deltaESDroppedPerSec', sort: 'deltaESDroppedPerSec', width: 120, doStats: true, dataFunction: (item) => { return roundCommaString(item.deltaESDroppedPerSec); } }),
        intl({ id: 'sessionSizePerSec', sort: 'sessionSizePerSec', width: 100, doStats: true, dataFunction: (item) => { return roundCommaString(item.sessionSizePerSec); } }),
        intl({ id: 'retention', sort: 'retention', width: 100, doStats: true, dataFunction: (item) => { return readableTimeCompact(item.retention * 1000); } }),
        intl({ id: 'startTime', sort: 'startTime', width: 200, doStats: false, dataFunction: (item) => { return timezoneDateString(item.startTime * 1000, this.user.settings.timezone, false); } }),
        intl({ id: 'runningTime', sort: 'runningTime', width: 200, doStats: false, dataFunction: (item) => { return readableTime(item.runningTime * 1000); } }),
        intl({ id: 'ver', sort: 'ver', width: 140, doStats: false })
      ]
    };
  },
  computed: {
    colors: function () {
      // build colors array from css variables
      const styles = window.getComputedStyle(document.body);
      const primaryLighter = styles.getPropertyValue('--color-primary-light').trim();
      const primaryLight = styles.getPropertyValue('--color-primary').trim();
      const primary = styles.getPropertyValue('--color-primary-dark').trim();
      const primaryDark = styles.getPropertyValue('--color-primary-darker').trim();
      const secondaryLighter = styles.getPropertyValue('--color-tertiary-light').trim();
      const secondaryLight = styles.getPropertyValue('--color-tertiary').trim();
      const secondary = styles.getPropertyValue('--color-tertiary-dark').trim();
      const secondaryDark = styles.getPropertyValue('--color-tertiary-darker').trim();
      return [primaryDark, primary, primaryLight, primaryLighter, secondaryLighter, secondaryLight, secondary, secondaryDark];
    },
    loading: {
      get: function () {
        return this.$store.state.loadingData;
      },
      set: function (newValue) {
        this.$store.commit('setLoadingData', newValue);
      }
    }
  },
  watch: {
    graphType: function () {
      this.loadData();
    },
    graphInterval: function () {
      this.loadData();
    },
    graphHide: function () {
      this.query.hide = this.graphHide;
      this.loadData();
    },
    dataInterval: function () {
      if (reqPromise) { // cancel the interval and reset it if necessary
        clearInterval(reqPromise);
        reqPromise = null;

        if (this.dataInterval === '0') { return; }

        this.setRequestInterval();
      } else if (this.dataInterval !== '0') {
        this.loadData();
        this.setRequestInterval();
      }
    },
    refreshData: function () {
      if (this.refreshData) {
        this.loadData();
      }
    },
    cluster: function () {
      this.query.cluster = this.cluster;
      this.loadData();
    }
  },
  created: function () {
    // don't need to load data (table component does it)
    // set a recurring server req if necessary
    if (this.dataInterval !== '0') {
      this.setRequestInterval();
    }

    // watch for the user to leave or return to the page
    // Don't load graph data if the user is not focused on the page!
    // if data is loaded in an inactive (background) tab,
    // the user will experience gaps in their cubism graph data
    // cubism uses setTimeout to delay requests
    // inactive tabs' timeouts are clamped and can fire late;
    // cubism requires little error in the timing of requests
    // for more info, view the "reasons for delays longer than specified" section of:
    // https://developer.mozilla.org/en-US/docs/Web/API/WindowTimers/setTimeout#Inactive_tabs
    if (document.addEventListener) {
      document.addEventListener('visibilitychange', this.handleVisibilityChange);
    }
  },
  methods: {
    /* exposed page functions ------------------------------------ */
    changePaging (pagingValues) {
      this.query.length = pagingValues.length;
      this.query.start = pagingValues.start;

      this.loadData();
    },
    columnClick (colName) {
      this.query.sortField = colName;
      this.query.desc = !this.query.desc;
      this.loadData();
    },
    /* helper functions ---------------------------------------------------- */
    setRequestInterval: function () {
      reqPromise = setInterval(() => {
        if (respondedAt && Date.now() - respondedAt >= parseInt(this.dataInterval)) {
          this.loadData();
        }
      }, 500);
    },
    async loadData (sortField, desc) {
      if (!Utils.checkClusterSelection(this.query.cluster, this.$store.state.esCluster.availableCluster.active, this).valid) {
        return;
      }

      this.loading = true;
      respondedAt = undefined;

      this.query.filter = this.searchTerm;

      if (desc !== undefined) { this.query.desc = desc; }
      if (sortField) { this.query.sortField = sortField; }

      try {
        const response = await StatsService.getStats(this.query);
        respondedAt = Date.now();
        this.error = '';
        this.loading = false;
        this.initialLoading = false;
        this.stats = response.data;
        this.recordsTotal = response.recordsTotal;
        this.recordsFiltered = response.recordsFiltered;
      } catch (error) {
        respondedAt = undefined;
        this.loading = false;
        this.initialLoading = false;
        this.error = error.text || error;
      }
    },
    toggleStatDetailWrapper: async function (stat) {
      try {
        await StatsService.loadTimeSeriesLibraries();
        oldD3 = window.d3;
        cubism = window.cubism;
        this.toggleStatDetail(stat);
      } catch (error) {
        console.error('Error loading time series libraries:', error);
        this.error = 'Error loading time series libraries. Please try again later.';
      }
    },
    toggleStatDetail: function (stat) {
      if (!stat.opened) { return; }
      const self = this;
      const id = stat.id.replace(/[\\.:]/g, '\\$&');

      const wrap = document.getElementById('moreInfo-' + id);
      while (wrap.firstChild) {
        wrap.removeChild(wrap.firstChild);
      }
      $(wrap).css('width', '1440px');

      const dcontext = cubism.context()
        .serverDelay(0)
        .clientDelay(0)
        .step(60e3)
        .size(1440);

      function dmetric (headerName, mname) {
        return dcontext.metric(async (startV, stopV, stepV, callback) => {
          try {
            const response = await StatsService.getDStats({
              name: mname,
              interval: 60,
              nodeName: stat.id,
              stop: stopV / 1000,
              step: stepV / 1000,
              start: startV / 1000
            });
            return callback(null, response);
          } catch (error) {
            console.error('Error loading data for metric:', headerName, error);
            return callback(new Error('Unable to load data'));
          }
        }, headerName);
      }

      // TODO instead of just showing the default columns, show the ones currently in the table
      const columns = this.columns.filter((column) => {
        return column.default && column.doStats;
      });
      const headerNames = columns.map(function (item) { return item.name; });
      const dataSrcs = columns.map(function (item) { return item.sort; });
      const metrics = [];
      for (let i = 0; i < headerNames.length; i++) {
        if (headerNames[i].match('/s')) {
          metrics.push(dmetric(headerNames[i].replace('/s', '/m'), dataSrcs[i].replace('PerSec', '')));
        } else {
          metrics.push(dmetric(headerNames[i], dataSrcs[i]));
        }
      }

      oldD3.select('#moreInfo-' + id).call(function (div) {
        if (div[0][0]) {
          div.append('div')
            .attr('class', 'axis')
            .call(dcontext.axis().orient('top'));

          div.selectAll('.horizon')
            .data(metrics)
            .enter().append('div')
            .attr('class', 'horizon')
            .call(dcontext.horizon().colors(self.colors));

          div.append('div')
            .attr('class', 'rule')
            .call(dcontext.rule());
        }
      });
    }
  },
  beforeUnmount () {
    if (reqPromise) {
      clearInterval(reqPromise);
      reqPromise = null;
    }

    if (document.removeEventListener) {
      document.removeEventListener('visibilitychange', this.handleVisibilityChange);
    }
  }
};
</script>

<style scoped>
.node-search {
  max-width: 50%;
}
</style>
