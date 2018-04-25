<template>

  <div>

    <!-- map open button -->
    <div class="map-btn"
      v-if="!showMap && primary"
      @click="toggleMap()"
      v-b-tooltip.hover
      title="View map"
      placement="left">
      <span class="fa fa-fw fa-globe">
      </span>
    </div> <!-- /map open button -->

    <!-- graph content -->
    <div class="graph-content pt-2">

      <!-- graph controls -->
      <div class="session-graph-btn-container"
        v-if="primary">
        <!-- zoom in/out -->
        <div class="btn-group btn-group-xs">
          <label class="btn btn-default"
            @click="zoomOut()"
            v-b-tooltip.hover
            title="Zoom out">
            <span class="fa fa-search-minus">
            </span>
          </label>
          <label class="btn btn-default"
            @click="zoomIn()"
            v-b-tooltip.hover
            title="Zoom in">
            <span class="fa fa-search-plus">
            </span>
          </label>
        </div> <!-- /zoom in/out -->
        <!-- pan left/right -->
        <div class="btn-group btn-group-xs ml-1">
          <label class="btn btn-default"
            @click="panLeft()"
            v-b-tooltip.hover
            title="Pan left">
            <span class="fa fa-chevron-left">
            </span>
          </label>
          <label class="btn btn-default"
            @click="panRight()"
            v-b-tooltip.hover
            title="Pan right">
            <span class="fa fa-chevron-right">
            </span>
          </label>
        </div> <!-- /pan left/right -->
        <!-- graph type -->
        <div class="btn-group btn-group-xs btn-group-radios ml-1">
          <b-form-radio-group
            size="sm"
            buttons
            v-model="graphType"
            @input="changeGraphType">
            <b-radio value="lpHisto"
              class="btn-radio">
              Session
            </b-radio>
            <b-radio value="paHisto"
              class="btn-radio">
              Packets
            </b-radio>
            <b-radio value="dbHisto"
              class="btn-radio">
              Databytes
            </b-radio>
          </b-form-radio-group>
        </div> <!-- graph type -->
        <!-- series type -->
        <div class="btn-group btn-group-xs btn-group-radios ml-1">
          <b-form-radio-group
            size="sm"
            buttons
            v-model="seriesType"
            @input="changeSeriesType">
            <b-radio value="lines"
              class="btn-radio">
              Lines
            </b-radio>
            <b-radio value="bars"
              class="btn-radio">
              Bars
            </b-radio>
          </b-form-radio-group>
        </div> <!-- series type -->
      </div> <!-- /graph controls -->

      <!-- graph -->
      <div v-if="graphData"
        :class="{'inline-graph':showMap,'whole-graph':!showMap}">
        <div class="plot-area"
          ref="plotArea"
          style="width:100%;height:150px;">
        </div>
      </div><!-- /graph -->

    </div> <!-- /graph content -->

  </div>

</template>

<script>
import '../../../../public/flot-0.7/jquery.flot';
import '../../../../public/flot-0.7/jquery.flot.selection';
import '../../../../public/flot-0.7/jquery.flot.navigate';
import '../../../../public/flot-0.7/jquery.flot.resize';
import '../../../../public/flot-0.7/jquery.flot.stack';

let basePath;
let plotArea;
let plot;
let timeout;

export default {
  name: 'MolochVisualizations',
  props: [ 'graphData', 'mapData', 'primary', 'open', 'timezone' ],
  data: function () {
    return {
      showMap: false,
      graph: undefined,
      graphOptions: {},
      graphType: this.$route.query.graphType || 'lpHisto',
      seriesType: this.$route.query.seriesType || 'bars'
    };
  },
  watch: {
    graphData: function (newVal, oldVal) {
      if (newVal && oldVal) {
        this.setupGraph(); // setup this.graph and this.graphOptions
        plot = $.plot(plotArea, this.graph, this.graphOptions);
      }
    }
  },
  created: function () {
    // TODO watch for open/close map
    // TODO watch for change series/graph type from siblings
    basePath = this.$route.path.split('/')[1];

    // set show map based on previous setting
    if (this.open &&
      localStorage && localStorage[`${basePath}-open-map`] &&
      localStorage[`${basePath}-open-map`] !== 'false') {
      this.showMap = true;
    }

    // setup the graph data and options
    this.setupGraph();
  },
  mounted: function () {
    // create flot graph
    plotArea = this.$refs.plotArea;
    plot = $.plot(plotArea, this.graph, this.graphOptions);

    // triggered when an area of the graph is selected
    $(plotArea).on('plotselected', (event, ranges) => {
      let result = {
        startTime: (ranges.xaxis.from / 1000).toFixed(),
        stopTime: (ranges.xaxis.to / 1000).toFixed()
      };

      if (result.startTime && result.stopTime) {
        this.$store.commit('setTime', result);
      }
    });

    let previousPoint;
    // triggered when hovering over the graph
    $(plotArea).on('plothover', (event, pos, item) => {
      if (item) {
        if (!previousPoint ||
          previousPoint.dataIndex !== item.dataIndex ||
          previousPoint.seriesIndex !== item.seriesIndex) {
          $(document.body).find('#tooltip').remove();

          previousPoint = {
            dataIndex: item.dataIndex,
            seriesIndex: item.seriesIndex
          };

          let type;
          if (this.graphType === 'dbHisto' || this.graphType === 'paHisto') {
            type = item.seriesIndex === 0 ? 'Src' : 'Dst';
          }

          let val = this.$options.filters.commaString(Math.round(item.series.data[item.dataIndex][1] * 100) / 100);
          let d = this.$options.filters.timezoneDateString(item.datapoint[0].toFixed(0) / 1000, this.timezone || 'local');

          let tooltipHTML = `<div id="tooltip" class="graph-tooltip">
                              <strong>${type || ''}</strong>
                              ${val} <strong>at</strong> ${d}
                            </div>`;

          $(tooltipHTML).css({
            top: item.pageY - 30,
            left: item.pageX - 8
          }).appendTo(document.body);
        }
      } else {
        $(document.body).find('#tooltip').remove();
        previousPoint = null;
      }
    });
  },
  methods: {
    /* exposed functions --------------------------------------------------- */
    /* MAP functions */
    toggleMap: function () {
      this.showMap = !this.showMap;

      if (this.primary && this.showMap) {
        // TODO display all maps
        if (localStorage) { localStorage[`${basePath}-open-map`] = true; }
      } else if (this.primary && !this.showMap) {
        // TODO hide all maps
        if (localStorage) { localStorage[`${basePath}-open-map`] = false; }
      }

      if (!this.showMap) {
        // TODO hide map
      }
    },
    /* GRAPH functions */
    changeGraphType: function () {
      this.setupGraph();

      plot.setData(this.graph);
      plot.setupGrid();
      plot.draw();

      if (this.primary) { // primary graph sets all graph's histo type
        // TODO notify sibling graphs to change graph type
      }
    },
    changeSeriesType: function () {
      this.setupGraph();

      plot = $.plot(plotArea, this.graph, this.graphOptions);

      this.$router.push({
        query: {
          ...this.$route.query,
          seriesType: this.seriesType
        }
      });

      if (this.primary) { // primary graph sets all graph's series type
        // TODO notify sibling graphs to change series type
      }
    },
    zoomOut: function () {
      plot.zoomOut();
      this.debounce(this.updateResults, plot, 400);
    },
    zoomIn: function () {
      plot.zoom();
      this.debounce(this.updateResults, plot, 400);
    },
    panLeft: function () {
      plot.pan({left: -100});
      this.debounce(this.updateResults, plot, 400);
    },
    panRight: function () {
      plot.pan({left: 100});
      this.debounce(this.updateResults, plot, 400);
    },
    /* helper functions ---------------------------------------------------- */
    debounce: function (func, funcParam, ms) {
      if (timeout) { clearTimeout(timeout); }

      timeout = setTimeout(() => {
        func(funcParam);
      }, ms);
    },
    updateResults: function (graph) {
      let xAxis = graph.getXAxes();

      let result = {
        startTime: (xAxis[0].min / 1000).toFixed(),
        stopTime: (xAxis[0].max / 1000).toFixed()
      };

      if (result.startTime && result.stopTime) {
        this.$store.commit('setTime', result);
      }
    },
    setupGraph: function () {
      let styles = window.getComputedStyle(document.body);
      let foregroundColor = styles.getPropertyValue('--color-foreground').trim();
      let primaryColor = styles.getPropertyValue('--color-primary').trim();
      let srcColor = styles.getPropertyValue('--color-src').trim() || '#CA0404';
      let dstColor = styles.getPropertyValue('--color-dst').trim() || '#0000FF';
      let highlightColor = styles.getPropertyValue('--color-gray-darker').trim();

      if (this.graphType === 'dbHisto') {
        this.graph = [
          { data: this.graphData.db1Histo, color: srcColor },
          { data: this.graphData.db2Histo, color: dstColor }
        ];
      } else if (this.graphType === 'paHisto') {
        this.graph = [
          { data: this.graphData.pa1Histo, color: srcColor },
          { data: this.graphData.pa2Histo, color: dstColor }
        ];
      } else {
        this.graph = [{ data: this.graphData[this.graphType], color: primaryColor }];
      }

      let showBars = this.seriesType === 'bars';

      for (let i = 0, len = this.graph.length; i < len; ++i) {
        this.graph[i].bars = { show: showBars };
      }

      this.graphOptions = { // flot graph options
        series: {
          stack: true,
          bars: {
            barWidth: (this.graphData.interval * 1000) / 1.7
          },
          lines: {
            fill: true
          }
        },
        selection: {
          mode: 'x',
          color: highlightColor
        },
        xaxis: {
          mode: 'time',
          label: 'Datetime',
          color: foregroundColor,
          min: this.graphData.xmin || null,
          max: this.graphData.xmax || null,
          tickFormatter: (v, axis) => {
            return this.$options.filters.timezoneDateString(
              Math.floor(v / 1000),
              this.timezone,
              'YYYY/MM/DD HH:mm:ss z'
            );
          }
        },
        yaxis: {
          min: 0,
          color: foregroundColor,
          zoomRange: false,
          autoscaleMargin: 0.2,
          tickFormatter: (v) => {
            if (this.graphType === 'dbHisto') {
              return this.$options.filters.humanReadableBytes(v);
            } else {
              return this.$options.filters.humanReadableNumber(v);
            }
          }
        },
        grid: {
          borderWidth: 0,
          color: foregroundColor,
          hoverable: true,
          clickable: true
        },
        zoom: {
          interactive: false,
          trigger: 'dblclick',
          amount: 2
        },
        pan: {
          interactive: false,
          cursor: 'move',
          frameRate: 20
        }
      };
    }
  },
  beforeDestroy: function () {
    $(plotArea).off('plothover');
    $(plotArea).off('plotselected');

    if (timeout) { clearTimeout(timeout); }
  }
};
</script>

<style>
/* display a tooltip above the bar in the session graph */
.graph-tooltip {
  z-index: 999;
  position: absolute;
  white-space: nowrap;
  color: var(--color-gray-lighter);
  padding: 3px;
  font-size: 8pt;
  background-color: var(--color-black);
  border-radius: 4px;
  border: 1px solid var(--color-black);
}
.graph-tooltip:after {
  content: '';
  display: block;
  width: 0;
  height: 0;
  position: absolute;
  left: 2px;
  top: 20px;
  border-style: solid;
  border-width: 8px 8px 0 8px;
  border-color: var(--color-black) transparent transparent transparent;
}

/* make graph labels smaller */
.tickLabels .tickLabel {
  font-size: smaller;
}
</style>

<style scoped>
.inline-map {
  width: 400px;
  float: right;
  position: relative;
}

/* fixed width buttons are the same width regardless of content */
.btn-fw {
  width: 26px;
}

.map-btn {
  display: block;
  position: absolute;
  top: 157px;
  right: 8px;
  z-index: 997;
  margin-top: 6px;
  overflow: hidden;
  padding: 2px 8px 3px 8px;;
  border-radius: 4px;
  cursor: pointer;
  background-color: var(--color-primary);
  color: #FFFFFF;
}

.btn-close-map {
  position: absolute;
  top     : 2px;
  right   : 2px;
  z-index : 997;
}

.btn-expand-map {
  position: absolute;
  top     : 26px;
  right   : 2px;
  z-index : 997;
}

.src-dst-btns {
  position: absolute;
  top     : 54px;
  right   : 2px;
  z-index : 997;
}

/* show the buttons on top of the map */
.expanded .src-dst-btns,
.expanded .btn-close-map,
.expanded .btn-expand-map {
  z-index : 999;
  position: fixed;
}

.expanded .btn-close-map {
  top   : 168px;
  right : 17px;
}
.expanded .btn-expand-map {
  top   : 192px;
  right : 17px;
}
.expanded .src-dst-btns {
  top   : 220px;
  right : 17px;
}

/* graph styles -------------------- */
.graph-content {
  overflow: hidden;
}

.inline-graph {
  position: relative;
  display: inline-block;
  width: calc(100% - 425px);
}
.whole-graph {
  position: relative;
  width: 99%;
}

/* center timeline buttons */
.session-graph-btn-container {
  position: absolute;
  left: 50%;
  white-space: nowrap;
}
.session-graph-btn-container > div {
  position: relative;
  left: -50%;
  z-index: 9; /* above timeline */
}

.session-graph-btn-container .btn-group-xs.btn-group-radios {
  margin-top: -7px;
}

.session-graph-btn-container .btn-group-xs label.btn-radio {
  padding: 1px 5px;
  font-size: 12px;
  line-height: 1.5;
}
</style>
