<template>

  <div class="pt-2"
    ref="vizContainer"
    :id="'vizContainer' + id">

    <!-- map content -->
    <div :class="{'expanded':mapExpanded}">

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

      <div v-show="showMap"
        class="inline-map">
        <div v-if="mapData">
          <div class="moloch-map-container">

            <!-- map -->
            <div class="moloch-map"
              :id="'molochMap' + id">
            </div> <!-- /map -->

            <!-- map buttons -->
            <button type="button"
              v-if="primary"
              class="btn btn-xs btn-default btn-close-map btn-fw"
              @click="toggleMap()"
              v-b-tooltip.hover
              title="Close map">
              <span class="fa fa-close">
              </span>
            </button>
            <button type="button"
              class="btn btn-xs btn-default btn-fw"
              :class="{'btn-expand-map':primary,'btn-close-map':!primary}"
              @click="toggleMapSize()"
              title="Expand/Collapse Map">
              <span class="fa"
                :class="{'fa-expand':!mapExpanded,'fa-compress':mapExpanded}">
              </span>
            </button>
            <div v-if="primary"
              class="btn-group-vertical src-dst-btns btn-fw">
              <button type="button"
                class="btn btn-xs btn-default"
                :class="{'active':src}"
                @click="toggleSrcDst('src')"
                v-b-tooltip.hover
                title="Toggle source countries">
                <strong>S</strong>
              </button>
              <button type="button"
                class="btn btn-xs btn-default"
                :class="{'active':dst}"
                @click="toggleSrcDst('dst')"
                v-b-tooltip.hover
                title="Toggle destination countries">
                <strong>D</strong>
              </button>
            </div> <!-- /map buttons -->

            <!-- map legend -->
            <div class="map-legend"
              v-if="mapExpanded && legend.length">
              <strong>Top 10</strong>&nbsp;
              <span v-for="(item, key) in legend"
                :key="key"
                class="legend-item"
                :style="{'background-color':item.color}">
                {{ item.name }}
                ({{ item.value | commaString }})
              </span>
            </div> <!-- map legend -->

          </div>
        </div>
      </div>

    </div> <!-- /map content -->

    <!-- graph content -->
    <div>

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
          :id="'plotArea' + id">
        </div>
      </div><!-- /graph -->

    </div> <!-- /graph content -->

  </div>

</template>

<script>
// map imports
import '../../../../public/jquery-jvectormap-1.2.2.min.js';
import '../../../../public/jquery-jvectormap-world-en.js';

// graph imports
import '../../../../public/flot-0.7/jquery.flot';
import '../../../../public/flot-0.7/jquery.flot.selection';
import '../../../../public/flot-0.7/jquery.flot.navigate';
import '../../../../public/flot-0.7/jquery.flot.resize';
import '../../../../public/flot-0.7/jquery.flot.stack';

// color vars
let foregroundColor;
let primaryColor;
let srcColor;
let dstColor;
let highlightColor;
let waterColor;
let landColorDark;
let landColorLight;

let timeout;
let basePath;

export default {
  name: 'MolochVisualizations',
  props: {
    graphData: Object,
    mapData: Object,
    primary: Boolean,
    timezone: {
      type: String,
      default: 'local'
    },
    id: {
      type: String,
      default: 'primary'
    }
  },
  data: function () {
    return {
      // map vars
      map: undefined,
      mapEl: undefined,
      legend: [],
      mapExpanded: false,
      // graph vars
      plot: undefined,
      plotArea: undefined,
      graph: undefined,
      graphOptions: {}
    };
  },
  computed: {
    showMap: {
      get: function (value) {
        return this.$store.state.showMaps;
      },
      set: function (newValue) {
        if (this.primary) {
          this.$store.commit('toggleMaps', newValue);
          localStorage[`${basePath}-open-map`] = newValue;
        }
      }
    },
    src: {
      get: function (value) {
        return this.$store.state.mapSrc;
      },
      set: function (newValue) {
        if (this.primary) {
          this.$store.commit('toggleMapSrc', newValue);
        }
      }
    },
    dst: {
      get: function (value) {
        return this.$store.state.mapDst;
      },
      set: function (newValue) {
        if (this.primary) {
          this.$store.commit('toggleMapDst', newValue);
        }
      }
    },
    graphType: {
      get: function (value) {
        return this.$store.state.graphType;
      },
      set: function (newValue) {
        if (this.primary) {
          this.$store.commit('updateGraphType', newValue);
        }
      }
    },
    seriesType: {
      get: function (value) {
        return this.$store.state.seriesType;
      },
      set: function (newValue) {
        if (this.primary) {
          this.$store.commit('updateSeriesType', newValue);
        }
      }
    }
  },
  watch: {
    src: function (newVal, oldVal) {
      this.setupMapData(this.mapData);
    },
    dst: function (newVal, oldVal) {
      this.setupMapData(this.mapData);
    },
    graphType: function (newVal, oldVal) {
      this.setupGraphData();
      this.plot.setData(this.graph);
      this.plot.setupGrid();
      this.plot.draw();
    },
    seriesType: function (newVal, oldVal) {
      this.setupGraphData();
      this.plot = $.plot(this.plotArea, this.graph, this.graphOptions);
    },
    graphData: function (newVal, oldVal) {
      if (newVal && oldVal) {
        this.setupGraphData(); // setup this.graph and this.graphOptions
        this.plot = $.plot(this.plotArea, this.graph, this.graphOptions);
      }
    },
    mapData: function (newVal, oldVal) {
      if (newVal && oldVal) {
        this.setupMapData(); // setup this.mapData
      }
    }
  },
  created: function () {
    // set styles for graph and map
    const styles = window.getComputedStyle(document.body);

    foregroundColor = styles.getPropertyValue('--color-foreground').trim();
    primaryColor = styles.getPropertyValue('--color-primary').trim();
    srcColor = styles.getPropertyValue('--color-src').trim() || '#CA0404';
    dstColor = styles.getPropertyValue('--color-dst').trim() || '#0000FF';
    highlightColor = styles.getPropertyValue('--color-gray-darker').trim();
    waterColor = styles.getPropertyValue('--color-water').trim();
    landColorDark = styles.getPropertyValue('--color-land-dark').trim();
    landColorLight = styles.getPropertyValue('--color-land-light').trim();

    if (!landColorDark || !landColorLight) {
      landColorDark = styles.getPropertyValue('--color-primary-dark').trim();
      landColorLight = styles.getPropertyValue('--color-primary-lightest').trim();
    }
  },
  mounted: function () {
    basePath = this.$route.path.split('/')[1];

    let showMap = localStorage && localStorage[`${basePath}-open-map`] &&
      localStorage[`${basePath}-open-map`] !== 'false';

    if (this.primary) {
      this.showMap = showMap;
      this.$store.commit('toggleMaps', showMap);

      this.graphType = this.$route.query.graphType || 'lpHisto';
      this.$store.commit('updateGraphType', this.graphType);

      this.seriesType = this.$route.query.seriesType || 'bars';
      this.$store.commit('updateSeriesType', this.seriesType);
    }

    // create map
    this.displayMap();

    // create graph
    // setup the graph data and options
    this.setupGraphData();
    // create flot graph
    this.setupGraphElement();
  },
  methods: {
    /* exposed functions --------------------------------------------------- */
    /* exposed MAP functions */
    toggleMap: function () {
      if (this.primary) {
        this.showMap = !this.showMap;
      }
    },
    toggleMapSize: function () {
      this.mapExpanded = !this.mapExpanded;
      if (this.mapExpanded) {
        this.expandMapElement();
        $(document).on('mouseup', this.isOutsideClick);
      } else {
        this.shrinkMapElement();
        $(document).off('mouseup', this.isOutsideClick);
      }
    },
    toggleSrcDst: function (type) {
      if (this.primary) { // primary map sets all other map's src/dst
        this[type] = !this[type];
      }
    },
    /* exposed GRAPH functions */
    changeGraphType: function () {
      if (this.primary) { // primary graph sets all graph's histo type
        this.$store.commit('updateGraphType', this.graphType);
        this.$router.push({
          query: {
            ...this.$route.query,
            graphType: this.graphType
          }
        });
      }
    },
    changeSeriesType: function () {
      if (this.primary) { // primary graph sets all graph's series type
        this.$store.commit('updateSeriesType', this.seriesType);
        this.$router.push({
          query: {
            ...this.$route.query,
            seriesType: this.seriesType
          }
        });
      }
    },
    zoomOut: function () {
      this.plot.zoomOut();
      this.debounce(this.updateResults, this.plot, 400);
    },
    zoomIn: function () {
      this.plot.zoom();
      this.debounce(this.updateResults, this.plot, 400);
    },
    panLeft: function () {
      this.plot.pan({left: -100});
      this.debounce(this.updateResults, this.plot, 400);
    },
    panRight: function () {
      this.plot.pan({left: 100});
      this.debounce(this.updateResults, this.plot, 400);
    },
    /* helper functions ---------------------------------------------------- */
    debounce: function (func, funcParam, ms) {
      if (timeout) { clearTimeout(timeout); }

      timeout = setTimeout(() => {
        func(funcParam);
      }, ms);
    },
    /* helper GRAPH functions */
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
    setupGraphElement: function () {
      this.plotArea = $('#plotArea' + this.id);
      this.plot = $.plot(this.plotArea, this.graph, this.graphOptions);

      // triggered when an area of the graph is selected
      $(this.plotArea).on('plotselected', (event, ranges) => {
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
      $(this.plotArea).on('plothover', (event, pos, item) => {
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
    setupGraphData: function () {
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
    },
    /* helper MAP functions */
    onMapResize: function () {
      if (this.mapExpanded) {
        $(this.mapEl).css({
          position: 'fixed',
          right: '8px',
          'z-index': 5,
          top: '158px',
          width: $(window).width() * 0.95,
          height: $(window).height() - 175
        });
      }
    },
    expandMapElement: function () {
      // onMapResize function expandes the map
      $(this.mapEl).resize();
    },
    shrinkMapElement: function () {
      $(this.mapEl).css({
        position: 'relative',
        top: '0',
        right: '0',
        height: '150px',
        width: '100%',
        'z-index': 3,
        'margin-bottom': '-25px'
      });
    },
    isOutsideClick: function (e) {
      let element = $('#vizContainer' + this.id);
      if (!$(element).is(e.target) &&
        $(element).has(e.target).length === 0) {
        this.mapExpanded = false;
        this.shrinkMapElement();
      }
    },
    displayMap: function () {
      // create jvectormap
      this.setupMapElement();
      // setup map data
      this.setupMapData();
    },
    setupMapElement: function () {
      this.mapEl = $('#molochMap' + this.id);

      // watch for the window to resize to resize the expanded map
      window.addEventListener('resize', this.onMapResize, { passive: true });
      // watch for the map to resize to change its style
      $(this.mapEl).on('resize', this.onMapResize);

      $(this.mapEl).vectorMap({ // setup map
        map: 'world_en',
        backgroundColor: waterColor,
        hoverColor: 'black',
        hoverOpacity: 0.7,
        series: {
          regions: [{
            scale: [ landColorLight, landColorDark ],
            normalizeFunction: 'polynomial',
            attribute: 'fill'
          }]
        },
        onRegionLabelShow: (e, el, code) => {
          el.html(el.html() + ' (' + code + ') - ' +
            this.$options.filters.commaString(this.map.series.regions[0].values[code] || 0));
        },
        onRegionClick: (e, code) => {
          this.$store.commit('addToExpression', {
            expression: `country == ${code}`
          });
        }
      });

      // save reference to the map object to retrieve regions
      this.map = $(this.mapEl).children('.jvectormap-container').data('mapObject');
    },
    setupMapData: function () {
      this.map.series.regions[0].clear();
      delete this.map.series.regions[0].params.min;
      delete this.map.series.regions[0].params.max;

      if (this.src && this.dst) {
        if (!this.mapData.tot) {
          this.mapData.tot = {};
          let k;
          for (k in this.mapData.src) {
            this.mapData.tot[k] = this.mapData.src[k];
          }

          for (k in this.mapData.dst) {
            if (this.mapData.tot[k]) {
              this.mapData.tot[k] += this.mapData.dst[k];
            } else {
              this.mapData.tot[k] = this.mapData.dst[k];
            }
          }
        }
        this.map.series.regions[0].setValues(this.mapData.tot);
      } else if (this.src) {
        this.map.series.regions[0].setValues(this.mapData.src);
      } else if (this.dst) {
        this.map.series.regions[0].setValues(this.mapData.dst);
      }

      let region = this.map.series.regions[0];
      this.legend = [];
      for (var key in region.values) {
        if (region.values.hasOwnProperty(key) &&
           region.elements.hasOwnProperty(key)) {
          this.legend.push({
            name: key,
            value: region.values[key],
            color: region.elements[key].element.properties.fill
          });
        }
      }

      this.legend.sort((a, b) => {
        return b.value - a.value;
      });

      this.legend = this.legend.slice(0, 10); // get top 10
    }
  },
  beforeDestroy: function () {
    // turn of graph events
    $(this.plotArea).off('plothover');
    $(this.plotArea).off('plotselected');

    if (timeout) { clearTimeout(timeout); }

    // turn off map events
    window.removeEventListener('resize', this.onMapResize);
    $(document).off('mouseup', this.isOutsideClick);
    $(this.mapEl).off('resize', this.onMapResize);
    $(this.mapEl).remove();
  }
};
</script>

<style>
/* map styles ---------------------- */
.inline-map .moloch-map-container > .moloch-map {
  z-index: 3;
  height: 150px;
  width: 100%;
  margin-bottom: -25px;
}

.jvectormap-container {
  border: 1px solid var(--color-gray-light);
  border-radius: 4px;
  height: 100%;
  width: 100%;
}

/* zoom buttons */
.jvectormap-zoomin, .jvectormap-zoomout {
  position: absolute;
  left: 2px;
  width: 18px;
  height: 20px;
  cursor: pointer;
  line-height: 16px;
  text-align: center;
  border-radius: 3px;
  color: var(--color-gray-darker);
  border-color: var(--color-gray);
  background-color: var(--color-white);
  font-weight: bolder;
}
.jvectormap-zoomin:hover, .jvectormap-zoomout:hover {
  color: var(--color-gray-darker);
  border-color: var(--color-gray-dark);
  background-color: var(--color-gray-light);
}
.jvectormap-zoomin {
  top: 2px;
}
.jvectormap-zoomout {
  top: 24px;
}

/* labels added to body by jvectormap */
.jvectormap-label {
  z-index: 6;
  position: absolute;
  display: none;
  border: solid 1px var(--color-gray-light);
  background: var(--color-gray-darker);
  color: var(--color-white);
  font-size: smaller;
  padding: var(--px-none) var(--px-sm);
  border-radius: 3px;
}

/* legend (top 10) */
.moloch-map-container .map-legend {
  max-width: 94%;
  margin-left: 4px;
  font-size: .8rem;
  position: fixed;
  bottom: 22px;
  right: 16px;
  z-index: 5;
  padding: 0 0 2px 4px;
  border-radius: 4px;
  background-color: #fff;
}

.moloch-map-container .map-legend .legend-item {
  display: inline-block;
  margin-right: 4px;
  margin-top: 2px;
  border-radius: 2px;
  padding: 2px 4px;
  color: #fff;
  text-shadow: 1px 1px 0 #333,
    -1px -1px 0 #333,
    1px -1px 0 #333,
    -1px 1px 0 #333,
    1px 1px 3px rgba(0,0,0,0.65);
}

.moloch-map-container .map-legend strong {
  color: #000;
}

/* display a tooltip above the bar in the session graph */
.graph-tooltip {
  z-index: 4;
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
  top: 150px;
  right: 0;
  z-index: 3;
  margin-top: 6px;
  overflow: hidden;
  padding: 2px 8px 3px 8px;;
  border-radius: 4px 0 0 4px;
  cursor: pointer;
  background-color: var(--color-primary);
  color: #FFFFFF;
}

.btn-close-map {
  position: absolute;
  top: 2px;
  right: 2px;
  z-index: 3;
}

.btn-expand-map {
  position: absolute;
  top: 26px;
  right: 2px;
  z-index: 3;
}

.src-dst-btns {
  position: absolute;
  top: 54px;
  right: 2px;
  z-index: 3;
}

/* show the buttons on top of the map */
.expanded .src-dst-btns,
.expanded .btn-close-map,
.expanded .btn-expand-map {
  z-index : 6;
}

/* graph styles -------------------- */
.plot-area {
  width: 100%;
  height: 150px;
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
  z-index: 1; /* above timeline */
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
