<template>

  <div class="ml-1 mr-1">

    <moloch-loading v-if="loading && !error">
    </moloch-loading>

    <moloch-error v-if="error"
      :message="error">
    </moloch-error>

    <div v-show="!error">

      <div class="input-group input-group-sm node-search pull-right mt-1">
        <div class="input-group-prepend">
          <span class="input-group-text">
            <span class="fa fa-search"></span>
          </span>
        </div>
        <input type="text"
          class="form-control"
          v-model="query.filter"
          @keyup="searchForNodes()"
          placeholder="Begin typing to search for nodes by name">
      </div>

      <moloch-paging v-if="stats"
        class="mt-1"
        :records-total="stats.recordsTotal"
        :records-filtered="stats.recordsFiltered"
        v-on:changePaging="changePaging">
      </moloch-paging>

      <table class="table table-sm text-right small">
        <thead>
          <tr>
            <th v-for="column of columns"
              :key="column.name"
              class="cursor-pointer"
              :class="{'text-left':!column.doStats}"
              @click="columnClick(column.sort)">
              {{ column.name }}
              <span v-if="column.sort !== undefined">
                <span v-show="query.sortField === column.sort && !query.desc" class="fa fa-sort-asc"></span>
                <span v-show="query.sortField === column.sort && query.desc" class="fa fa-sort-desc"></span>
                <span v-show="query.sortField !== column.sort" class="fa fa-sort"></span>
              </span>
            </th>
          </tr>
        </thead>
        <tbody v-if="stats">
          <template v-if="averageValues && totalValues && stats.data.length > 9">
            <tr class="bold average-row">
              <td>&nbsp;</td>
              <td class="text-left">Average</td>
              <td>&nbsp;</td>
              <td>{{ averageValues.monitoring | round(0) | commaString }}</td>
              <td>{{ averageValues.freeSpaceM*1000000 | humanReadableBytes }} ({{ averageValues.freeSpaceP | round(1) }}%)</td>
              <td>{{ averageValues.cpu/100.0 | round(1) }}%</td>
              <td>{{ averageValues.memory | humanReadableBytes }} ({{ averageValues.memoryP | round(1) }}%)</td>
              <td>{{ averageValues.packetQueue | round(0) | commaString }}</td>
              <td>{{ averageValues.deltaPacketsPerSec | round(0) | commaString }}</td>
              <td>{{ averageValues.deltaBytesPerSec | humanReadableBytes }}</td>
              <td>{{ averageValues.deltaSessionsPerSec | round(0) | commaString }}</td>
              <td>{{ averageValues.deltaDroppedPerSec | round(0) | commaString }}</td>
              <td>{{ averageValues.deltaOverloadDroppedPerSec | round(0) | commaString }}</td>
              <td>{{ averageValues.deltaESDroppedPerSec | round(0) | commaString }}</td>
            </tr>
            <tr class="border-bottom-bold bold total-row">
              <td>&nbsp;</td>
              <td class="text-left">Total</td>
              <td>&nbsp;</td>
              <td>{{ totalValues.monitoring | round(0) | commaString }}</td>
              <td>{{ totalValues.freeSpaceM*1000000 | humanReadableBytes }} ({{ totalValues.freeSpaceP | round(1) }}%)</td>
              <td>{{ totalValues.cpu/100.0 | round(1) }}%</td>
              <td>{{ totalValues.memory | humanReadableBytes }} ({{ totalValues.memoryP | round(1) }}%)</td>
              <td>{{ totalValues.packetQueue | round(0) | commaString }}</td>
              <td>{{ totalValues.deltaPacketsPerSec | round(0) | commaString }}</td>
              <td>{{ totalValues.deltaBytesPerSec | humanReadableBytes }}</td>
              <td>{{ totalValues.deltaSessionsPerSec | round(0) | commaString }}</td>
              <td>{{ totalValues.deltaDroppedPerSec | round(0) | commaString }}</td>
              <td>{{ totalValues.deltaOverloadDroppedPerSec | round(0) | commaString }}</td>
              <td>{{ totalValues.deltaESDroppedPerSec | round(0) | commaString }}</td>
            </tr>
          </template>
          <template v-for="stat of stats.data">
            <tr :key="stat.id + 'data'">
              <td>
                <toggle-btn class="mr-2"
                  :opened="expandedNodeStats[stat.id.replace(/[.:]/g, '\\$&')]"
                  @toggle="toggleStatDetail(stat)">
                </toggle-btn>
              </td>
              <td class="text-left">{{ stat.id }}</td>
              <td>{{ stat.currentTime | timezoneDateString(user.settings.timezone, 'YYYY/MM/DD HH:mm:ss z') }}</td>
              <td>{{ stat.monitoring | round(0) | commaString }}</td>
              <td>{{ stat.freeSpaceM*1000000 | humanReadableBytes }} ({{ stat.freeSpaceP | round(1) }}%)</td>
              <td>{{ stat.cpu/100.0 | round(1) }}%</td>
              <td>{{ stat.memory | humanReadableBytes }} ({{ stat.memoryP | round(1) }}%)</td>
              <td>{{ stat.packetQueue | round(0) | commaString }}</td>
              <td>{{ stat.deltaPacketsPerSec | round(0) | commaString }}</td>
              <td>{{ stat.deltaBytesPerSec | humanReadableBytes }}</td>
              <td>{{ stat.deltaSessionsPerSec | round(0) | commaString }}</td>
              <td>{{ stat.deltaDroppedPerSec | round(0) | commaString }}</td>
              <td>{{ stat.deltaOverloadDroppedPerSec | round(0) | commaString }}</td>
              <td>{{ stat.deltaESDroppedPerSec | round(0) | commaString }}</td>
            </tr>
            <tr :key="stat.id + 'graph'"
              :id="'statsGraphRow-' + stat.id"
              style="display:none;">
              <td :colspan="columns.length">
                <div :id="'statsGraph-' + stat.id"
                  style="width: 1440px;">
                </div>
              </td>
            </tr>
          </template>
          <tr v-if="stats.data && !stats.data.length">
            <td :colspan="columns.length"
              class="text-danger text-center">
              <span class="fa fa-warning">
              </span>&nbsp;
              No results match your search
            </td>
          </tr>
        </tbody>
        <tfoot v-if="stats && averageValues && totalValues && stats.data.length > 1">
          <tr class="border-top-bold bold average-row">
            <td>&nbsp;</td>
            <td class="text-left">Average</td>
            <td>&nbsp;</td>
            <td>{{ averageValues.monitoring | round(0) | commaString }}</td>
            <td>{{ averageValues.freeSpaceM*1000000 | humanReadableBytes }} ({{ averageValues.freeSpaceP | round(1) }}%)</td>
            <td>{{ averageValues.cpu/100.0 | round(1) }}%</td>
            <td>{{ averageValues.memory | humanReadableBytes }} ({{ averageValues.memoryP | round(1) }}%)</td>
            <td>{{ averageValues.packetQueue | round(0) | commaString }}</td>
            <td>{{ averageValues.deltaPacketsPerSec | round(0) | commaString }}</td>
            <td>{{ averageValues.deltaBytesPerSec | humanReadableBytes }}</td>
            <td>{{ averageValues.deltaSessionsPerSec | round(0) | commaString }}</td>
            <td>{{ averageValues.deltaDroppedPerSec | round(0) | commaString }}</td>
            <td>{{ averageValues.deltaOverloadDroppedPerSec | round(0) | commaString }}</td>
            <td>{{ averageValues.deltaESDroppedPerSec | round(0) | commaString }}</td>
          </tr>
          <tr class="bold total-row">
            <td>&nbsp;</td>
            <td class="text-left">Total</td>
            <td>&nbsp;</td>
            <td>{{ totalValues.monitoring | round(0) | commaString }}</td>
            <td>{{ totalValues.freeSpaceM*1000000 | humanReadableBytes }} ({{ totalValues.freeSpaceP | round(1) }}%)</td>
            <td>{{ totalValues.cpu/100.0 | round(1) }}%</td>
            <td>{{ totalValues.memory | humanReadableBytes }} ({{ totalValues.memoryP | round(1) }}%)</td>
            <td>{{ totalValues.packetQueue | round(0) | commaString }}</td>
            <td>{{ totalValues.deltaPacketsPerSec | round(0) | commaString }}</td>
            <td>{{ totalValues.deltaBytesPerSec | humanReadableBytes }}</td>
            <td>{{ totalValues.deltaSessionsPerSec | round(0) | commaString }}</td>
            <td>{{ totalValues.deltaDroppedPerSec | round(0) | commaString }}</td>
            <td>{{ totalValues.deltaOverloadDroppedPerSec | round(0) | commaString }}</td>
            <td>{{ totalValues.deltaESDroppedPerSec | round(0) | commaString }}</td>
          </tr>
        </tfoot>
      </table>

    </div>

  </div>

</template>

<script>
import d3 from '../../../../public/d3.min.js';
import cubism from '../../../../public/cubism.v1.js';
import '../../../../public/highlight.min.js';

import '../../cubismoverrides.css';
import ToggleBtn from '../utils/ToggleBtn';
import MolochPaging from '../utils/Pagination';
import MolochError from '../utils/Error';
import MolochLoading from '../utils/Loading';

let reqPromise; // promise returned from setInterval for recurring requests
let searchInputTimeout; // timeout to debounce the search input

export default {
  name: 'NodeStats',
  props: [ 'user', 'graphType', 'graphInterval', 'graphHide', 'dataInterval' ],
  components: { ToggleBtn, MolochPaging, MolochError, MolochLoading },
  data: function () {
    return {
      error: '',
      loading: true,
      stats: null,
      totalValues: null,
      averageValues: null,
      showNodeStats: true,
      expandedNodeStats: {},
      query: {
        length: parseInt(this.$route.query.length) || 50,
        start: 0,
        filter: null,
        sortField: 'nodeName',
        desc: true,
        hide: this.graphHide || 'none'
      },
      columns: [ // node stats table columns
        { name: '', doStats: false },
        { name: 'Node', sort: 'nodeName', doStats: false },
        { name: 'Time', sort: 'currentTime', doStats: true },
        { name: 'Sessions', sort: 'monitoring', doStats: true },
        { name: 'Free Space', sort: 'freeSpaceM', doStats: true },
        { name: 'CPU', sort: 'cpu', doStats: true },
        { name: 'Memory', sort: 'memory', doStats: true },
        { name: 'Packet Q', sort: 'packetQueue', doStats: true },
        { name: 'Packet/s', sort: 'deltaPackets', field: 'deltaPacketsPerSec', doStats: true },
        { name: 'Bytes/s', sort: 'deltaBytes', field: 'deltaBytesPerSec', doStats: true },
        { name: 'Sessions/s', sort: 'deltaSessions', field: 'deltaSessionsPerSec', doStats: true },
        { name: 'Packet Drops/s', sort: 'deltaDropped', field: 'deltaDroppedPerSec', doStats: true },
        { name: 'Overload Drops/s', sort: 'deltaOverloadDropped', field: 'deltaOverloadDroppedPerSec', doStats: true },
        { name: 'ES Drops/s', sort: 'deltaESDropped', field: 'deltaESDroppedPerSec', doStats: true }
      ]
    };
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
    }
  },
  methods: {
    /* exposed page functions ------------------------------------ */
    changePaging (pagingValues) {
      this.query.length = pagingValues.length;
      this.query.start = pagingValues.start;

      this.loadData();
    },
    searchForNodes () {
      if (searchInputTimeout) { clearTimeout(searchInputTimeout); }
      // debounce the input so it only issues a request after keyups cease for 400ms
      searchInputTimeout = setTimeout(() => {
        searchInputTimeout = null;
        this.loadData();
      }, 400);
    },
    columnClick (name) {
      this.query.sortField = name;
      this.query.desc = !this.query.desc;
      this.loadData();
    },
    /* helper functions ------------------------------------------ */
    setRequestInterval: function () {
      reqPromise = setInterval(() => {
        this.loadData();
      }, parseInt(this.dataInterval, 10));
    },
    loadData: function () {
      this.$http.get('stats.json', { params: this.query })
        .then((response) => {
          this.error = '';
          this.loading = false;
          this.stats = response.data;

          this.totalValues = {};
          this.averageValues = {};

          let columnNames = this.columns.map((item) => {
            return item.field || item.sort;
          });
          columnNames.push('memoryP');
          columnNames.push('freeSpaceP');

          if (!this.stats.data) { return; }

          for (let i = 3; i < columnNames.length; i++) {
            let columnName = columnNames[i];

            this.totalValues[columnName] = 0;
            for (let s = 0; s < this.stats.data.length; s++) {
              this.totalValues[columnName] += this.stats.data[s][columnName];
            }
            this.averageValues[columnName] = this.totalValues[columnName] / this.stats.data.length;
          }
        }, (error) => {
          this.loading = false;
          this.error = error;
        });
    },
    toggleStatDetail: function (stat) {
      var self = this;
      let id = stat.id.replace(/[.:]/g, '\\$&');

      this.expandedNodeStats[id] = !this.expandedNodeStats[id];

      document.getElementById('statsGraphRow-' + id).style.display =
        this.expandedNodeStats[id] ? 'table-row' : 'none';

      let wrap = document.getElementById('statsGraph-' + id);
      while (wrap.firstChild) {
        wrap.removeChild(wrap.firstChild);
      }

      if (!this.expandedNodeStats[id]) { return; }

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

      var headerNames = this.columns.map(function (item) { return item.name; });
      var dataSrcs = this.columns.map(function (item) { return item.sort; });
      var metrics = [];
      for (var i = 3; i < headerNames.length; i++) {
        if (headerNames[i].match('/s')) {
          metrics.push(dmetric(headerNames[i].replace('/s', '/m'), dataSrcs[i].replace('PerSec', '')));
        } else {
          metrics.push(dmetric(headerNames[i], dataSrcs[i]));
        }
      }

      d3.select('#statsGraph-' + id).call(function (div) {
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
.collapsed > .when-opened,
:not(.collapsed) > .when-closed {
  display: none;
}

.node-search {
  max-width: 50%;
}

td {
  white-space: nowrap;
}
tr.bold {
  font-weight: bold;
}
table.table tr.border-bottom-bold > td {
  border-bottom: 2px solid #dee2e6;
}
table.table tr.border-top-bold > td {
  border-top: 2px solid #dee2e6;
}

#graphContent, #nodeStatsContent {
  overflow-x: auto;
}
</style>
