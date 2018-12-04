<template>

  <div class="container-fluid">

    <moloch-loading v-if="initialLoading && !error">
    </moloch-loading>

    <moloch-error v-if="error"
      :message="error">
    </moloch-error>

    <div v-show="!error">

      <span v-b-tooltip.hover.left
        class="fa fa-lg fa-question-circle-o cursor-help mt-2 pull-right"
        title="HINT: These graphs are 1440 pixels wide. Expand your browser window to at least 1500 pixels wide for best viewing.">
      </span>

      <moloch-paging v-if="stats"
        class="mt-1"
        :records-total="recordsTotal"
        :records-filtered="recordsFiltered"
        v-on:changePaging="changePaging"
        length-default=100>
      </moloch-paging>

      <moloch-table
        id="captureStatsTable"
        :data="stats"
        :loadData="loadData"
        :columns="columns"
        :no-results="true"
        :show-avg-tot="true"
        :action-column="true"
        :info-row="true"
        :info-row-function="toggleStatDetail"
        :desc="query.desc"
        :sortField="query.sortField"
        table-animation="list"
        table-classes="table-sm text-right small"
        table-state-name="captureStatsCols"
        table-widths-state-name="captureStatsColWidths">
      </moloch-table>

    </div>

  </div>

</template>

<script>
import Vue from 'vue';
import d3 from '../../../../public/d3.min.js';
import cubism from '../../../../public/cubism.v1.js';
import '../../../../public/highlight.min.js';

import '../../cubismoverrides.css';
import ToggleBtn from '../utils/ToggleBtn';
import MolochPaging from '../utils/Pagination';
import MolochError from '../utils/Error';
import MolochLoading from '../utils/Loading';
import MolochTable from '../utils/Table';

let reqPromise; // promise returned from setInterval for recurring requests
let respondedAt; // the time that the last data load succesfully responded

function roundCommaString (val) {
  let result = Vue.options.filters.commaString(Vue.options.filters.round(val, 0));
  return result;
};

export default {
  name: 'NodeStats',
  props: [
    'user',
    'graphType',
    'graphInterval',
    'graphHide',
    'dataInterval',
    'refreshData',
    'searchTerm'
  ],
  components: {
    ToggleBtn,
    MolochPaging,
    MolochError,
    MolochLoading,
    MolochTable
  },
  data: function () {
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
        length: parseInt(this.$route.query.length) || 100,
        start: 0,
        filter: this.searchTerm || undefined,
        sortField: 'nodeName',
        desc: true,
        hide: this.graphHide || 'none'
      },
      columns: [ // node stats table columns
        // default columns
        { id: 'node', name: 'Node', sort: 'nodeName', dataField: 'nodeName', width: 120, default: true, doStats: false },
        { id: 'time', name: 'Time', sort: 'currentTime', dataField: 'currentTime', width: 200, dataFunction: (val) => { return this.$options.filters.timezoneDateString(val, this.user.settings.timezone, 'YYYY/MM/DD HH:mm:ss z'); }, default: true, doStats: false },
        { id: 'sessions', name: 'Sessions', sort: 'monitoring', dataField: 'monitoring', width: 100, dataFunction: roundCommaString, default: true, doStats: true },
        { id: 'freeSpace', name: 'Free Space', sort: 'freeSpaceM', width: 120, dataFunction: (item) => { return this.$options.filters.humanReadableBytes(item.freeSpaceM * 1000000) + ' (' + this.$options.filters.round(item.freeSpaceP, 1) + '%)'; }, avgTotFunction: (val) => { return this.$options.filters.humanReadableBytes(val * 1000000); }, default: true, doStats: true },
        { id: 'cpu', name: 'CPU', sort: 'cpu', dataField: 'cpu', width: 80, dataFunction: (val) => { return this.$options.filters.round(val / 100.0, 1) + '%'; }, default: true, doStats: true },
        { id: 'memory', name: 'Memory', sort: 'memory', width: 120, dataFunction: (item) => { return this.$options.filters.humanReadableBytes(item.memory) + ' (' + this.$options.filters.round(item.memoryP, 1) + '%)'; }, avgTotFunction: (val) => { return this.$options.filters.humanReadableBytes(val); }, default: true, doStats: true },
        { id: 'packetQ', name: 'Packet Q', sort: 'packetQueue', dataField: 'packetQueue', width: 85, dataFunction: roundCommaString, default: true, doStats: true },
        { id: 'diskQ', name: 'Disk Q', sort: 'diskQueue', dataField: 'diskQueue', width: 85, dataFunction: roundCommaString, default: true, doStats: true },
        { id: 'esQueue', name: 'ES Q', sort: 'esQueue', dataField: 'esQueue', width: 85, dataFunction: roundCommaString, default: true, doStats: true },
        { id: 'deltaPackets', name: 'Packet/s', sort: 'deltaPackets', dataField: 'deltaPacketsPerSec', width: 100, dataFunction: roundCommaString, default: true, doStats: true },
        { id: 'deltaBytes', name: 'Bytes/s', sort: 'deltaBytes', dataField: 'deltaBytesPerSec', width: 80, dataFunction: (val) => { return this.$options.filters.humanReadableBytes(val); }, default: true, doStats: true },
        { id: 'deltaSessions', name: 'Sessions/s', sort: 'deltaSessions', dataField: 'deltaSessionsPerSec', width: 100, dataFunction: roundCommaString, default: true, doStats: true },
        { id: 'deltaDropped', name: 'Packet Drops/s', sort: 'deltaDropped', dataField: 'deltaDroppedPerSec', width: 130, dataFunction: roundCommaString, default: true, doStats: true },
        // all the rest of the available stats
        { id: 'deltaBitsPerSec', name: 'Bits/Sec', sort: 'deltaBitsPerSec', dataField: 'deltaBitsPerSec', width: 100, dataFunction: roundCommaString, doStats: true },
        { id: 'deltaWrittenBytes', name: 'Written Bytes/s', sort: 'deltaWrittenBytes', dataField: 'deltaWrittenBytesPerSec', width: 100, dataFunction: (val) => { return this.$options.filters.humanReadableBytes(val); }, doStats: true },
        { id: 'deltaUnwrittenBytes', name: 'Unwritten Bytes/s', sort: 'deltaUnwrittenBytes', dataField: 'deltaUnwrittenBytesPerSec', width: 100, dataFunction: (val) => { return this.$options.filters.humanReadableBytes(val); }, doStats: true },
        { id: 'tcpSessions', name: 'Active TCP Sessions', sort: 'tcpSessions', dataField: 'tcpSessions', width: 100, dataFunction: roundCommaString, doStats: true },
        { id: 'udpSessions', name: 'Active UDP Sessions', sort: 'udpSessions', dataField: 'udpSessions', width: 100, dataFunction: roundCommaString, doStats: true },
        { id: 'icmpSessions', name: 'Active ICMP Sessions', sort: 'icmpSessions', dataField: 'icmpSessions', width: 100, dataFunction: roundCommaString, doStats: true },
        { id: 'sctpSessions', name: 'Active SCTP Sessions', sort: 'sctpSessions', dataField: 'sctpSessions', width: 100, dataFunction: roundCommaString, doStats: true },
        { id: 'espSessions', name: 'Active ESP Sessions', sort: 'espSessions', dataField: 'espSessions', width: 100, dataFunction: roundCommaString, doStats: true },
        { id: 'usedSpaceM', name: 'Used Space', sort: 'usedSpaceM', dataField: 'usedSpaceM', width: 100, dataFunction: (val) => { return this.$options.filters.humanReadableBytes(val * 1000000); }, doStats: true },
        { id: 'esHealthMS', name: 'ES Health Response MS', sort: 'esHealthMS', dataField: 'esHealthMS', width: 100, dataFunction: roundCommaString, doStats: true },
        { id: 'closeQueue', name: 'Closing Q', sort: 'closeQueue', dataField: 'closeQueue', width: 100, dataFunction: roundCommaString, doStats: true },
        { id: 'needSave', name: 'Waiting Q', sort: 'needSave', dataField: 'needSave', width: 100, dataFunction: roundCommaString, doStats: true },
        { id: 'frags', name: 'Active Fragments', sort: 'frags', dataField: 'frags', width: 100, dataFunction: roundCommaString, doStats: true },
        { id: 'deltaFragsDroppedPerSec', name: 'Fragments Dropped/Sec', sort: 'deltaFragsDroppedPerSec', dataField: 'deltaFragsDroppedPerSec', width: 100, dataFunction: roundCommaString, doStats: true },
        { id: 'deltaTotalDroppedPerSec', name: 'Total Dropped/Sec', sort: 'deltaTotalDroppedPerSec', dataField: 'deltaTotalDroppedPerSec', width: 100, dataFunction: roundCommaString, doStats: true },
        { id: 'deltaSessionBytesPerSec', name: 'ES Session Bytes/Sec', sort: 'deltaSessionBytesPerSec', dataField: 'deltaSessionBytesPerSec', width: 100, dataFunction: (val) => { return this.$options.filters.humanReadableBytes(val); }, doStats: true },
        { id: 'deltaOverloadDropped', name: 'Overload Drops/s', sort: 'deltaOverloadDropped', dataField: 'deltaOverloadDroppedPerSec', width: 140, dataFunction: roundCommaString, doStats: true },
        { id: 'deltaESDropped', name: 'ES Drops/s', sort: 'deltaESDropped', dataField: 'deltaESDroppedPerSec', width: 120, dataFunction: roundCommaString, doStats: true },
        { id: 'sessionSizePerSec', name: 'ES Session Size/Sec', sort: 'sessionSizePerSec', dataField: 'sessionSizePerSec', width: 100, dataFunction: roundCommaString, doStats: true }
      ]
    };
  },
  computed: {
    colors: function () {
      // build colors array from css variables
      let styles = window.getComputedStyle(document.body);
      let primaryLighter = styles.getPropertyValue('--color-primary-light').trim();
      let primaryLight = styles.getPropertyValue('--color-primary').trim();
      let primary = styles.getPropertyValue('--color-primary-dark').trim();
      let primaryDark = styles.getPropertyValue('--color-primary-darker').trim();
      let secondaryLighter = styles.getPropertyValue('--color-tertiary-light').trim();
      let secondaryLight = styles.getPropertyValue('--color-tertiary').trim();
      let secondary = styles.getPropertyValue('--color-tertiary-dark').trim();
      let secondaryDark = styles.getPropertyValue('--color-tertiary-darker').trim();
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
    }
  },
  created: function () {
    this.loadData();
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
    columnClick (name) {
      this.query.sortField = name;
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
    loadData: function (sortField, desc) {
      this.loading = true;
      respondedAt = undefined;

      this.query.filter = this.searchTerm;

      if (desc !== undefined) { this.query.desc = desc; }
      if (sortField) { this.query.sortField = sortField; }

      this.$http.get('stats.json', { params: this.query })
        .then((response) => {
          respondedAt = Date.now();
          this.error = '';
          this.loading = false;
          this.initialLoading = false;
          this.stats = response.data.data;
          this.recordsTotal = response.data.recordsTotal;
          this.recordsFiltered = response.data.recordsFiltered;
        }, (error) => {
          respondedAt = undefined;
          this.loading = false;
          this.initialLoading = false;
          this.error = error.text || error;
        });
    },
    toggleStatDetail: function (stat) {
      if (!stat.opened) { return; }
      var self = this;
      let id = stat.id.replace(/[.:]/g, '\\$&');

      let wrap = document.getElementById('moreInfo-' + id);
      while (wrap.firstChild) {
        wrap.removeChild(wrap.firstChild);
      }
      $(wrap).css('width', '1440px');

      var dcontext = cubism.cubism.context()
        .serverDelay(0)
        .clientDelay(0)
        .step(60e3)
        .size(1440);

      function dmetric (name, mname) {
        return dcontext.metric(function (startV, stopV, stepV, callback) {
          let config = {
            method: 'GET',
            url: 'dstats.json',
            params: {
              nodeName: stat.id,
              start: startV / 1000,
              stop: stopV / 1000,
              step: stepV / 1000,
              interval: 60,
              name: mname
            }
          };
          self.$http(config)
            .then((response) => {
              return callback(null, response.data);
            }, (error) => {
              return callback(new Error('Unable to load data'));
            });
        }, name);
      }

      // TODO instead of just showing the default columns, show the ones currently in the table
      let columns = this.columns.filter((column) => {
        return column.default && column.id !== 'node' && column.id !== 'time';
      });
      let headerNames = columns.map(function (item) { return item.name; });
      let dataSrcs = columns.map(function (item) { return item.sort; });
      let metrics = [];
      for (let i = 0; i < headerNames.length; i++) {
        if (headerNames[i].match('/s')) {
          metrics.push(dmetric(headerNames[i].replace('/s', '/m'), dataSrcs[i].replace('PerSec', '')));
        } else {
          metrics.push(dmetric(headerNames[i], dataSrcs[i]));
        }
      }

      d3.select('#moreInfo-' + id).call(function (div) {
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
  beforeDestroy: function () {
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
