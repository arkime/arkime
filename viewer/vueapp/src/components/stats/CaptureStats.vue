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
        class="mt-1 ml-2"
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
import d3 from '../../../../public/d3.min.js';
import cubism from '../../../../public/cubism.v1.js';
import '../../../../public/highlight.min.js';

import '../../cubismoverrides.css';
import MolochPaging from '../utils/Pagination';
import MolochError from '../utils/Error';
import MolochLoading from '../utils/Loading';
import MolochTable from '../utils/Table';

let reqPromise; // promise returned from setInterval for recurring requests
let respondedAt; // the time that the last data load succesfully responded

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
        { id: 'nodeName', name: 'Node', sort: 'nodeName', width: 120, default: true, doStats: false },
        { id: 'currentTime', name: 'Time', sort: 'currentTime', width: 200, default: true, doStats: false, dataFunction: (item) => { return this.$options.filters.timezoneDateString(item.currentTime * 1000, this.user.settings.timezone, false); } },
        { id: 'monitoring', name: 'Sessions', sort: 'monitoring', width: 100, default: true, doStats: true, dataFunction: (item) => { return this.$options.filters.roundCommaString(item.monitoring); } },
        { id: 'freeSpaceM', name: 'Free Space', sort: 'freeSpaceM', width: 120, default: true, doStats: true, dataFunction: (item) => { return this.$options.filters.humanReadableBytes(item.freeSpaceM * 1000000) + ' (' + this.$options.filters.round(item.freeSpaceP, 1) + '%)'; }, avgTotFunction: (item) => { return this.$options.filters.humanReadableBytes(item.freeSpaceM * 1000000); } },
        { id: 'cpu', name: 'CPU', sort: 'cpu', width: 80, default: true, doStats: true, dataFunction: (item) => { return this.$options.filters.round(item.cpu / 100.0, 1) + '%'; } },
        { id: 'memory', name: 'Memory', sort: 'memory', width: 120, default: true, doStats: true, dataFunction: (item) => { return this.$options.filters.humanReadableBytes(item.memory) + ' (' + this.$options.filters.round(item.memoryP, 1) + '%)'; }, avgTotFunction: (item) => { return this.$options.filters.humanReadableBytes(item.memory); } },
        { id: 'packetQueue', name: 'Packet Q', sort: 'packetQueue', width: 85, default: true, doStats: true, dataFunction: (item) => { return this.$options.filters.roundCommaString(item.packetQueue); } },
        { id: 'diskQueue', name: 'Disk Q', sort: 'diskQueue', width: 85, default: true, doStats: true, dataFunction: (item) => { return this.$options.filters.roundCommaString(item.diskQueue); } },
        { id: 'esQueue', name: 'ES Q', sort: 'esQueue', width: 85, default: true, doStats: true, dataFunction: (item) => { return this.$options.filters.roundCommaString(item.esQueue); } },
        { id: 'deltaPackets', name: 'Packet/s', sort: 'deltaPackets', width: 100, default: true, doStats: true, dataFunction: (item) => { return this.$options.filters.roundCommaString(item.deltaPackets); } },
        { id: 'deltaBytes', name: 'Bytes/s', sort: 'deltaBytes', width: 80, dataFunction: (item) => { return this.$options.filters.humanReadableBytes(item.deltaBytesPerSec); }, default: true, doStats: true },
        { id: 'deltaSessions', name: 'Sessions/s', sort: 'deltaSessions', width: 100, default: true, doStats: true, dataFunction: (item) => { return this.$options.filters.roundCommaString(item.deltaSessions); } },
        { id: 'deltaDropped', name: 'Packet Drops/s', sort: 'deltaDropped', width: 130, default: true, doStats: true, dataFunction: (item) => { return this.$options.filters.roundCommaString(item.deltaDropped); } },
        // all the rest of the available stats
        { id: 'deltaBitsPerSec', name: 'Bits/Sec', sort: 'deltaBitsPerSec', width: 100, doStats: true, dataFunction: (item) => { return this.$options.filters.roundCommaString(item.deltaBitsPerSec); } },
        { id: 'deltaWrittenBytesPerSec', name: 'Written Bytes/s', sort: 'deltaWrittenBytesPerSec', width: 100, doStats: true, dataFunction: (item) => { return this.$options.filters.humanReadableBytes(item.deltaWrittenBytesPerSec); } },
        { id: 'deltaUnwrittenBytesPerSec', name: 'Unwritten Bytes/s', sort: 'deltaUnwrittenBytesPerSec', width: 100, doStats: true, dataFunction: (item) => { return this.$options.filters.humanReadableBytes(item.deltaUnwrittenBytesPerSec); } },
        { id: 'tcpSessions', name: 'Active TCP Sessions', sort: 'tcpSessions', width: 100, doStats: true, dataFunction: (item) => { return this.$options.filters.roundCommaString(item.tcpSessions); } },
        { id: 'udpSessions', name: 'Active UDP Sessions', sort: 'udpSessions', width: 100, doStats: true, dataFunction: (item) => { return this.$options.filters.roundCommaString(item.udpSessions); } },
        { id: 'icmpSessions', name: 'Active ICMP Sessions', sort: 'icmpSessions', width: 100, doStats: true, dataFunction: (item) => { return this.$options.filters.roundCommaString(item.icmpSessions); } },
        { id: 'sctpSessions', name: 'Active SCTP Sessions', sort: 'sctpSessions', width: 100, doStats: true, dataFunction: (item) => { return this.$options.filters.roundCommaString(item.sctpSessions); } },
        { id: 'espSessions', name: 'Active ESP Sessions', sort: 'espSessions', width: 100, doStats: true, dataFunction: (item) => { return this.$options.filters.roundCommaString(item.espSessions); } },
        { id: 'usedSpaceM', name: 'Used Space', sort: 'usedSpaceM', width: 100, doStats: true, dataFunction: (item) => { return this.$options.filters.humanReadableBytes(item.usedSpaceM * 1000000); } },
        { id: 'esHealthMS', name: 'ES Health Response MS', sort: 'esHealthMS', width: 100, doStats: true, dataFunction: (item) => { return this.$options.filters.roundCommaString(item.esHealthMS); } },
        { id: 'closeQueue', name: 'Closing Q', sort: 'closeQueue', width: 100, doStats: true, dataFunction: (item) => { return this.$options.filters.roundCommaString(item.closeQueue); } },
        { id: 'needSave', name: 'Waiting Q', sort: 'needSave', width: 100, doStats: true, dataFunction: (item) => { return this.$options.filters.roundCommaString(item.needSave); } },
        { id: 'frags', name: 'Active Fragments', sort: 'frags', width: 100, doStats: true, dataFunction: (item) => { return this.$options.filters.roundCommaString(item.frags); } },
        { id: 'deltaFragsDroppedPerSec', name: 'Fragments Dropped/Sec', sort: 'deltaFragsDroppedPerSec', width: 100, doStats: true, dataFunction: (item) => { return this.$options.filters.roundCommaString(item.deltaFragsDroppedPerSec); } },
        { id: 'deltaTotalDroppedPerSec', name: 'Total Dropped/Sec', sort: 'deltaTotalDroppedPerSec', width: 100, doStats: true, dataFunction: (item) => { return this.$options.filters.roundCommaString(item.deltaTotalDroppedPerSec); } },
        { id: 'deltaSessionBytesPerSec', name: 'ES Session Bytes/Sec', sort: 'deltaSessionBytesPerSec', width: 100, doStats: true, dataFunction: (item) => { return this.$options.filters.humanReadableBytes(item.deltaSessionBytesPerSec); } },
        { id: 'deltaOverloadDropped', name: 'Overload Drops/s', sort: 'deltaOverloadDropped', width: 140, doStats: true, dataFunction: (item) => { return this.$options.filters.roundCommaString(item.deltaOverloadDropped); } },
        { id: 'deltaESDroppedPerSec', name: 'ES Drops/s', sort: 'deltaESDropped', width: 120, doStats: true, dataFunction: (item) => { return this.$options.filters.roundCommaString(item.deltaESDropped); } },
        { id: 'sessionSizePerSec', name: 'ES Session Size/Sec', sort: 'sessionSizePerSec', width: 100, doStats: true, dataFunction: (item) => { return this.$options.filters.roundCommaString(item.sessionSizePerSec); } },
        { id: 'retention', name: 'Retention', sort: 'retention', width: 100, doStats: true, dataFunction: (item) => { return this.$options.filters.readableTimeCompact(item.retention * 1000); } }
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
