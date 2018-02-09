<template>

  <div>

    <div class="row mt-1">
      <div class="col-md-6">
        <!-- TODO hook up paging -->
        <b-pagination size="sm"
          :total-rows="100"
          v-model="currentPage"
          :per-page="10">
        </b-pagination>
      </div>
      <div class="col-md-6">
        <div class="input-group input-group-sm">
          <div class="input-group-prepend">
            <span class="input-group-text">
              <span class="fa fa-search"></span>
            </span>
          </div>
          <input type="text"
            class="form-control pull-right"
            v-model="query.filter"
            placeholder="Begin typing to search for nodes by name">
        </div>
      </div>
    </div>

    <div role="tablist">

      <b-card no-body
        class="mb-2">
        <b-card-header header-tag="header"
          class="p-2 cursor-pointer"
          role="tab">
          <div v-b-toggle.graphContent>
            Graphs
            <span class="when-opened pull-right fa fa-minus"></span>
            <span class="when-closed pull-right fa fa-plus"></span>
          </div>
        </b-card-header>
        <b-collapse id="graphContent"
          v-model="showGraphs"
          role="tabpanel">
          <div id="statsGraph" style="width:1440px;"></div>
        </b-collapse>
      </b-card>

      <b-card no-body
        class="mb-1">
        <b-card-header header-tag="header"
          class="p-2 cursor-pointer"
          role="tab">
          <div v-b-toggle.nodeStatsContent>
            Node Stats
            <span class="when-opened pull-right fa fa-minus"></span>
            <span class="when-closed pull-right fa fa-plus"></span>
          </div>
        </b-card-header>
        <b-collapse id="nodeStatsContent"
          v-model="showNodeStats"
          role="tabpanel">
          <b-card-body>
            <table class="table table-sm text-right small">
              <thead>
                <tr>
                  <th v-for="column of columns"
                    :key="column.name"
                    class="cursor-pointer">
                    {{ column.name }}
                  </th>
                </tr>
              </thead>
              <tbody v-if="stats">
                <template v-if="averageValues && totalValues && stats.data.length > 9">
                  <tr class="bold">
                    <td>&nbsp;</td>
                    <td>Total</td>
                    <td>&nbsp;</td>
                    <td>{{ totalValues.monitoring | commaString }}</td>
                    <td>{{ totalValues.freeSpaceM*1000000 | humanReadable }} ({{ totalValues.freeSpaceP }}%)</td>
                    <td>{{ totalValues.cpu/100.0 }}%</td>
                    <td>{{ totalValues.memory | humanReadable }} ({{ totalValues.memoryP }}%)</td>
                    <td>{{ totalValues.packetQueue | commaString }}</td>
                    <td>{{ totalValues.deltaPacketsPerSec | commaString }}</td>
                    <td>{{ totalValues.deltaBytesPerSec | humanReadable }}</td>
                    <td>{{ totalValues.deltaSessionsPerSec | commaString }}</td>
                    <td>{{ totalValues.deltaDroppedPerSec | commaString }}</td>
                    <td>{{ totalValues.deltaOverloadDroppedPerSec | commaString }}</td>
                    <td>{{ totalValues.deltaESDroppedPerSec | commaString }}</td>
                  </tr>
                  <tr class="bold">
                    <td>&nbsp;</td>
                    <td>Average</td>
                    <td>&nbsp;</td>
                    <td>{{ averageValues.monitoring | commaString }}</td>
                    <td>{{ averageValues.freeSpaceM*1000000 | humanReadable }} ({{ averageValues.freeSpaceP }}%)</td>
                    <td>{{ averageValues.cpu/100.0 }}%</td>
                    <td>{{ averageValues.memory | humanReadable }} ({{ averageValues.memoryP }}%)</td>
                    <td>{{ averageValues.packetQueue | commaString }}</td>
                    <td>{{ averageValues.deltaPacketsPerSec | commaString }}</td>
                    <td>{{ averageValues.deltaBytesPerSec | humanReadable }}</td>
                    <td>{{ averageValues.deltaSessionsPerSec | commaString }}</td>
                    <td>{{ averageValues.deltaDroppedPerSec | commaString }}</td>
                    <td>{{ averageValues.deltaOverloadDroppedPerSec | commaString }}</td>
                    <td>{{ averageValues.deltaESDroppedPerSec | commaString }}</td>
                  </tr>
                </template>
                <template v-for="stat of stats.data">
                  <tr :key="stat.id + 'data'">
                    <td>
                      <toggle-btn v-on:toggle="toggleStatDetail(stat)">
                      </toggle-btn>
                    </td>
                    <td>{{ stat.id }}</td>
                    <td>{{ stat.currentTime | timezoneDateString(user.settings.timezone, 'YYYY/MM/DD HH:mm:ss z') }}</td>
                    <td class="nowrap">{{ stat.monitoring | commaString }}</td>
                    <td>{{ stat.freeSpaceM*1000000 | humanReadable }} ({{ stat.freeSpaceP }}%)</td>
                    <td>{{ stat.cpu/100.0 }}%</td>
                    <td>{{ stat.memory | humanReadable }} ({{ stat.memoryP }}%)</td>
                    <td>{{ stat.packetQueue | commaString }}</td>
                    <td>{{ stat.deltaPacketsPerSec | commaString }}</td>
                    <td>{{ stat.deltaBytesPerSec | humanReadable }}</td>
                    <td>{{ stat.deltaSessionsPerSec | commaString }}</td>
                    <td>{{ stat.deltaDroppedPerSec | commaString }}</td>
                    <td>{{ stat.deltaOverloadDroppedPerSec | commaString }}</td>
                    <td>{{ stat.deltaESDroppedPerSec | commaString }}</td>
                  </tr>
                  <tr :key="stat.id + 'graph'">
                    <td :colspan="columns.length">
                      <div :id="'statsGraph-' + stat.id" style="width: 1440px;">
                      </div>
                    </td>
                  </tr>
                </template>
              </tbody>
              <tfoot v-if="stats && averageValues && totalValues && stats.data.length > 1">
                <tr class="border-top-bold bold">
                  <td>&nbsp;</td>
                  <td>Average</td>
                  <td>&nbsp;</td>
                  <td>{{ averageValues.monitoring | commaString }}</td>
                  <td>{{ averageValues.freeSpaceM*1000000 | humanReadable }} ({{ averageValues.freeSpaceP }}%)</td>
                  <td>{{ averageValues.cpu/100.0 }}%</td>
                  <td>{{ averageValues.memory | humanReadable }} ({{ averageValues.memoryP }}%)</td>
                  <td>{{ averageValues.packetQueue | commaString }}</td>
                  <td>{{ averageValues.deltaPacketsPerSec | commaString }}</td>
                  <td>{{ averageValues.deltaBytesPerSec | humanReadable }}</td>
                  <td>{{ averageValues.deltaSessionsPerSec | commaString }}</td>
                  <td>{{ averageValues.deltaDroppedPerSec | commaString }}</td>
                  <td>{{ averageValues.deltaOverloadDroppedPerSec | commaString }}</td>
                  <td>{{ averageValues.deltaESDroppedPerSec | commaString }}</td>
                </tr>
                <tr class="bold">
                  <td>&nbsp;</td>
                  <td>Total</td>
                  <td>&nbsp;</td>
                  <td>{{ totalValues.monitoring | commaString }}</td>
                  <td>{{ totalValues.freeSpaceM*1000000 | humanReadable }} ({{ totalValues.freeSpaceP }}%)</td>
                  <td>{{ totalValues.cpu/100.0 }}%</td>
                  <td>{{ totalValues.memory | humanReadable }} ({{ totalValues.memoryP }}%)</td>
                  <td>{{ totalValues.packetQueue | commaString }}</td>
                  <td>{{ totalValues.deltaPacketsPerSec | commaString }}</td>
                  <td>{{ totalValues.deltaBytesPerSec | humanReadable }}</td>
                  <td>{{ totalValues.deltaSessionsPerSec | commaString }}</td>
                  <td>{{ totalValues.deltaDroppedPerSec | commaString }}</td>
                  <td>{{ totalValues.deltaOverloadDroppedPerSec | commaString }}</td>
                  <td>{{ totalValues.deltaESDroppedPerSec | commaString }}</td>
                </tr>
              </tfoot>
            </table>
          </b-card-body>
        </b-collapse>
      </b-card>

    </div>

  </div>

</template>

<script>
import $ from 'jQuery'; // TODO remove
import d3 from '../../../../public/d3.min.js';
import cubism from '../../../../public/cubism.v1.js';
import '../../../../public/highlight.min.js';

import '../../cubismoverrides.css';

import ToggleBtn from '../ToggleBtn';

export default {
  // TODO paging, search, sort
  name: 'NodeStats',
  props: ['user'],
  components: {
    ToggleBtn
  },
  data: function () {
    return {
      test: false, // TODO
      stats: null,
      totalValues: null,
      averageValues: null,
      currentPage: 1,
      loading: false,
      showGraphs: true,
      showNodeStats: true,
      expandedNodeStats: {},
      query: {
        filter: ''
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
  created: function () {
    this.loadData();
    // TODO interval for loading data
  },
  methods: {
    toggleTest: function () {
      this.test = !this.test;
      console.log('toggle test clicked', this.test);
    },
    loadData: function () {
      this.loading = true;
      this.error = null;

      // TODO pass in params
      this.$http.get('stats.json')
        .then((response) => {
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

          // TODO make the graph
          // if (this.stats.data && !initialized && this.graphsOpen) {
          if (this.stats.data && this.showGraphs) {
            // initialized = true; // only make the graph when page loads or tab switched to 0
            this.makeStatsGraph('deltaPacketsPerSec', 5);
            // TODO pass in this.graphType and this.graphInterval
            // if (this.graphInterval === '0') { // turn it on then off
            //   this.makeStatsGraph(this.graphType, 5);
            //   this.context.stop();
            // } else {
            //   this.makeStatsGraph(this.graphType, parseInt(this.graphInterval, 10));
            // }
          }
        }, (error) => {
          this.loading = false;
          this.error = error;
        });
    },
    makeStatsGraph: function (metricName, interval) {
      var self = this;
      if (self.context) { self.context.stop(); } // Stop old context

      self.context = cubism.cubism.context()
        .step(interval * 1000)
        .size(1440);

      var context = self.context;
      var nodes = self.stats.data.map(function (item) {
        return item.nodeName;
      });

      function metric (name) {
        return context.metric(function (startV, stopV, stepV, callback) {
          let config = {
            method: 'GET',
            url: 'dstats.json',
            params: {
              nodeName: name,
              start: startV / 1000,
              stop: stopV / 1000,
              step: stepV / 1000,
              interval: interval,
              name: metricName
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

      context.on('focus', function (i) {
        d3.selectAll('#statsGraph .value').style('right', i === null ? null : context.size() - i + 'px');
        let panel = $('#statsGraphPanel .panel-body');
        if (panel && panel.length) {
          let scrollPx = panel[0].scrollLeft;
          if (scrollPx) {
            scrollPx = scrollPx - 12;
            d3.selectAll('#statsGraph .rule').style('left', '-' + scrollPx + 'px');
          } else {
            d3.selectAll('#statsGraph .rule').style('left', '12px');
          }
        }
      });

      $('#statsGraph').empty();
      d3.select('#statsGraph').call(function (div) {
        var metrics = [];
        for (var i = 0, ilen = nodes.length; i < ilen; i++) {
          metrics.push(metric(nodes[i]));
        }

        if (div[0][0]) {
          let axis = context.axis();

          let timeStr;
          if (self.graphInterval >= 600) {
            timeStr = '%m/%d %H:%M:%S';
          } else {
            timeStr = '%H:%M:%S';
          }

          let timeFormat;
          if (self.user.settings.timezone === 'gmt') {
            timeFormat = d3.time.format(timeStr);
          } else {
            timeFormat = d3.time.format.utc(timeStr + 'Z');
          }

          div.append('div')
            .attr('class', 'axis')
            .attr('height', 28)
            .call(
              axis.orient('top')
                .tickFormat(timeFormat)
                .focusFormat(timeFormat)
            );

          div.selectAll('.horizon')
            .data(metrics)
            .enter().append('div')
            .attr('class', 'horizon')
            .call(context.horizon()); // TODO readd .colors(self.colors)

          div.append('div')
            .attr('class', 'rule')
            .call(context.rule());
        }
      });
    },
    toggleStatDetail (stat) {
      var self = this;
      let id = stat.id.replace(/[.:]/g, '\\$&');

      this.expandedNodeStats[id] = !this.expandedNodeStats[id];

      $(document.getElementById('statsGraph-' + id)).empty();

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
            .call(dcontext.horizon()); // TODO .colors(self.colors)

          div.append('div')
            .attr('class', 'rule')
            .call(dcontext.rule());
        }
      });

      dcontext.on('focus', function (i) {
        d3.selectAll('#statsGraph-' + id + ' .value').style('right', i === null ? null : dcontext.size() - i + 'px');
        let panel = $('#nodeStatsPanel .panel-body');
        if (panel && panel.length) {
          let scrollPx = panel[0].scrollLeft;
          if (scrollPx) {
            scrollPx = scrollPx - 16;
            d3.selectAll('#statsGraph-' + id + ' .rule').style('left', '-' + scrollPx + 'px');
          } else {
            d3.selectAll('#statsGraph-' + id + ' .rule').style('left', '16px');
          }
        }
      });
    }
  }
};
</script>

<style scoped>
.collapsed > .when-opened,
:not(.collapsed) > .when-closed {
  display: none;
}

td {
  white-space: nowrap;
}
tr.bold {
  font-weight: bold;
}
</style>
