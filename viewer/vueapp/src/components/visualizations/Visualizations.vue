<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div :class="{'sticky-viz':stickyViz && primary, 'hide-viz':hideViz && primary, 'disabled-msg':disabledAggregations}">
    <div
      class="pt-2 ps-2 pe-2 viz-container"
      :id="'vizContainer' + id"
      :class="{'map-visible':showMap,'map-invisible':!showMap}">
      <div v-show="!hideViz">
        <div
          class="row"
          v-if="disabledAggregations">
          <div class="col text-center">
            <div class="alert alert-sm alert-info container">
              <strong>
                {{ $t('vis.hideViz', { days: turnOffGraphDays }) }}
              </strong>
              <span class="fa fa-info-circle fa-lg ms-1 me-1 cursor-help">
                <v-tooltip activator="parent">
                  {{ $t('vis.hideVizTip') }}
                </v-tooltip>
              </span>
              <br>
              {{ $t('vis.hideVizMore') }}
            </div>
          </div>
        </div>

        <template v-else>
          <!-- map content -->
          <div :class="{'expanded':mapExpanded}">
            <!-- map open button -->
            <div
              class="map-btn"
              v-show="!showMap && primary"
              @click="toggleMap">
              <span class="fa fa-fw fa-globe" />
              <v-tooltip activator="parent">
                {{ $t('common.openMap') }}
              </v-tooltip>
            </div> <!-- /map open button -->

            <div class="inline-map">
              <div v-if="mapData">
                <div class="map-container">
                  <!-- map -->
                  <world-map
                    ref="worldMap"
                    class="map"
                    :map-data="mapData"
                    :src="src"
                    :dst="dst"
                    :xff-geo="xffGeo"
                    @region-click="onMapRegionClick"
                    @update-legend="onUpdateLegend" /> <!-- /map -->

                  <!-- map buttons -->
                  <button
                    type="button"
                    v-if="primary"
                    :aria-label="$t('common.close')"
                    class="btn btn-xs btn-default btn-close-map btn-fw"
                    @click="toggleMap">
                    <span class="fa fa-close" />
                  </button>
                  <button
                    type="button"
                    :aria-label="$t('vis.toggleMapSize')"
                    class="btn btn-xs btn-default btn-fw btn-z-index-2"
                    :class="{'btn-expand-map':primary,'btn-close-map':!primary}"
                    @click="toggleMapSize">
                    <span
                      class="fa"
                      :class="{'fa-expand':!mapExpanded,'fa-compress':mapExpanded}" />
                  </button>
                  <div
                    v-if="primary"
                    class="btn-group-vertical src-dst-btns btn-fw">
                    <button
                      type="button"
                      class="btn btn-xs btn-default"
                      :class="{'active':src}"
                      @click="toggleSrcDstXff('src')">
                      <strong>S</strong>
                      <v-tooltip activator="parent">
                        {{ $t('vis.toggleSrcCountry') }}
                      </v-tooltip>
                    </button>
                    <button
                      type="button"
                      class="btn btn-xs btn-default"
                      :class="{'active':dst}"
                      @click="toggleSrcDstXff('dst')">
                      <strong>D</strong>
                      <v-tooltip activator="parent">
                        {{ $t('vis.toggleDstCountry') }}
                      </v-tooltip>
                    </button>
                  </div>
                  <button
                    v-if="primary"
                    type="button"
                    class="btn btn-xs btn-default btn-fw xff-btn"
                    @click="toggleSrcDstXff('xffGeo')"
                    :class="{'active':xffGeo}"
                    title="Toggle XFF Countries">
                    <small>XFF</small>
                  </button> <!-- /map buttons -->

                  <!-- map legend -->
                  <div
                    class="map-legend"
                    v-if="mapExpanded && legend.length">
                    <strong>Top 10</strong>&nbsp;
                    <span
                      v-for="(item, key) in legend"
                      :key="key"
                      class="legend-item"
                      :style="{'background-color':item.color}">
                      {{ item.name }}
                      ({{ commaString(item.value) }})
                    </span>
                  </div> <!-- map legend -->
                </div>
              </div>
            </div>
          </div> <!-- /map content -->

          <!-- graph content -->
          <div>
            <!-- graph controls -->
            <div
              class="session-graph-btn-container"
              v-if="primary">
              <!-- zoom in/out -->
              <div class="btn-group btn-group-xs zoom-buttons">
                <label
                  class="btn btn-default"
                  @click="zoomOut">
                  <span class="fa fa-search-minus" />
                </label>
                <label
                  class="btn btn-default"
                  @click="zoomIn">
                  <span class="fa fa-search-plus" />
                </label>
              </div> <!-- /zoom in/out -->
              <!-- pan left/right -->
              <div class="btn-group btn-group-xs ms-1 pan-buttons">
                <label
                  class="btn btn-default"
                  @click="panLeft">
                  <span class="fa fa-chevron-left" />
                </label>
                <b-dropdown
                  size="sm"
                  variant="default"
                  class="pan-dropdown">
                  <template #button-content>
                    {{ plotPan * 100 + '%' }}
                  </template>
                  <b-dropdown-item @click="plotPanChange(0.05)">
                    5%
                  </b-dropdown-item>
                  <b-dropdown-item @click="plotPanChange(0.1)">
                    10%
                  </b-dropdown-item>
                  <b-dropdown-item @click="plotPanChange(0.2)">
                    20%
                  </b-dropdown-item>
                  <b-dropdown-item @click="plotPanChange(0.5)">
                    50%
                  </b-dropdown-item>
                  <b-dropdown-item @click="plotPanChange(1)">
                    100%
                  </b-dropdown-item>
                </b-dropdown>
                <label
                  class="btn btn-default"
                  @click="panRight">
                  <span class="fa fa-chevron-right" />
                </label>
              </div> <!-- /pan left/right -->
              <!-- graph type -->
              <div
                class="btn-group btn-group-xs btn-group-radios ms-1"
                style="margin-top: 3px;">
                <b-form-radio-group
                  size="sm"
                  class="buttons-with-boxes"
                  :model-value="graphType"
                  @update:model-value="changeGraphType">
                  <b-form-radio
                    value="sessionsHisto"
                    key="sessionsHisto">
                    {{ $t('common.sessions') }}
                  </b-form-radio>
                  <b-form-radio
                    v-for="filter in timelineDataFilters"
                    :value="filter.dbField + 'Histo'"
                    :key="filter.dbField">
                    {{ filter.friendlyName }}
                  </b-form-radio>
                </b-form-radio-group>
              </div> <!-- graph type -->
              <!-- series type -->
              <div class="btn-group btn-group-xs btn-group-radios ms-1">
                <b-form-radio-group
                  size="sm"
                  class="buttons-with-boxes"
                  style="margin-top: 2px;"
                  :model-value="seriesType"
                  @update:model-value="changeSeriesType">
                  <b-form-radio value="lines">
                    {{ $t('vis.graphLines') }}
                  </b-form-radio>
                  <b-form-radio value="bars">
                    {{ $t('vis.graphBars') }}
                  </b-form-radio>
                </b-form-radio-group>
              </div> <!-- series type -->
              <!-- cap times -->
              <div
                class="btn-group btn-group-xs btn-group-checkboxes ms-1"
                style="margin-top: 4px;">
                <b-form-checkbox
                  size="sm"
                  class="buttons-with-boxes"
                  :model-value="showCapStartTimes"
                  @update:model-value="toggleCapStartTimes">
                  {{ $t('vis.capRestarts') }}
                </b-form-checkbox> <!-- /cap times -->
                <v-tooltip
                  activator="parent"
                  location="bottom">
                  {{ $t('vis.capRestartsTip') }}
                </v-tooltip>
              </div>
              <!-- spanning -->
              <div
                class="btn-group btn-group-xs btn-group-checkboxes ms-1"
                style="margin-top: 4px;">
                <b-form-checkbox
                  size="sm"
                  class="buttons-with-boxes"
                  :model-value="spanning"
                  :disabled="timeBounding === 'database'"
                  @update:model-value="toggleSpanning">
                  {{ $t('search.timeBounding-spanning') }}
                </b-form-checkbox>
                <v-tooltip
                  activator="parent"
                  location="bottom">
                  {{ $t('search.spanningTip') }}
                </v-tooltip>
              </div> <!-- /spanning -->
            </div> <!-- /graph controls -->

            <!-- graph -->
            <div
              v-if="graphData"
              class="plot-container">
              <timeline-graph
                ref="timelineGraph"
                :graph-data="graphData"
                :graph-type="graphType"
                :series-type="seriesType"
                :timeline-data-filters="timelineDataFilters"
                :show-cap-start-times="showCapStartTimes"
                :cap-start-times="capStartTimes"
                :timezone="timezone"
                @update-time-range="updateStopStartTime" />
            </div> <!-- /graph -->
          </div> <!-- /graph content -->
        </template>
      </div>
    </div>
  </div>
</template>

<script>
// imports
import { commaString } from '@common/vueFilters.js';
import StatsService from '../stats/StatsService';
import TimelineGraph from './TimelineGraph.vue';
import WorldMap from './WorldMap.vue';

let timeout;
let basePath;

export default {
  name: 'ArkimeVisualizations',
  components: { TimelineGraph, WorldMap },
  emits: ['fetchMapData', 'spanningChange'],
  props: {
    graphData: {
      type: Object,
      default: () => { return {}; }
    },
    mapData: {
      type: Object,
      default: () => { return {}; }
    },
    primary: Boolean,
    id: {
      type: String,
      default: 'primary'
    },
    timelineDataFilters: {
      type: Array,
      required: true
    }
  },
  data: function () {
    return {
      // map vars (rendering lives in WorldMap; parent owns the open/close
      // toggle, expanded state, and the legend it emits)
      legend: [],
      mapExpanded: false,
      // graph vars (rendering lives in TimelineGraph; parent only owns pan
      // step + showMap toggle now that Flot is gone)
      plotPan: 0.1,
      showMap: undefined,
      turnOffGraphDays: this.$constants.TURN_OFF_GRAPH_DAYS
    };
  },
  computed: {
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
    xffGeo: {
      get: function (value) {
        return this.$store.state.xffGeo;
      },
      set: function (newValue) {
        if (this.primary) {
          this.$store.commit('toggleMapXffGeo', newValue);
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
    },
    timezone: function () {
      return this.$store.state.user.settings.timezone;
    },
    showCapStartTimes: {
      get: function () {
        return this.$store.state.showCapStartTimes;
      },
      set: function (newValue) {
        if (this.primary) {
          this.$store.commit('setShowCapStartTimes', newValue);
        }
      }
    },
    capStartTimes: function () {
      return this.$store.state.capStartTimes;
    },
    stickyViz: function () {
      return this.$store.state.stickyViz;
    },
    hideViz: function () {
      return this.$store.state.hideViz;
    },
    disabledAggregations: function () {
      return this.$store.state.disabledAggregations;
    },
    spanning: function () {
      return this.$route.query.spanning === 'true';
    },
    timeBounding: function () {
      return this.$route.query.bounding || 'last';
    }
  },
  watch: {
    // graphType/seriesType/graphData/capStartTimes are watched by
    // TimelineGraph directly. WorldMap watches mapData/src/dst/xffGeo
    // directly. Only cross-component state stays here.
    '$store.state.showMaps': function (newVal, oldVal) {
      if (this.id !== 'primary') {
        const id = parseInt(this.id);
        setTimeout(() => { // show/hide maps one at a time
          this.showMap = newVal;
        }, id * 100);
      }
    }
  },
  created: function () {
    basePath = this.$route.path.split('/')[1];

    const showMap = localStorage && localStorage[`${basePath}-open-map`] &&
      localStorage[`${basePath}-open-map`] !== 'false';

    this.showCapStartTimes = localStorage && localStorage[`${basePath}-cap-times`] &&
      localStorage[`${basePath}-cap-times`] !== 'false';

    this.showMap = showMap;

    if (this.primary) {
      this.$store.commit('toggleMaps', showMap);

      this.graphType = this.getDefaultGraphType();
      this.$store.commit('updateGraphType', this.graphType);

      this.seriesType = this.$route.query.seriesType || 'bars';
      this.$store.commit('updateSeriesType', this.seriesType);

      StatsService.getCapRestartTimes(basePath);
    }
  },
  methods: {
    commaString,
    getDefaultGraphType: function () {
      const storedFilters = this.$store.state.user.settings.timelineDataFilters;
      const routeFilter = this.$route.query.graphType;

      // filter is included in route and is enabled in settings
      if (routeFilter && storedFilters.includes(routeFilter.slice(0, -5))) {
        return routeFilter;
      }

      // user has selected a filter and it hasn't been removed from settings page
      if (this.graphType && storedFilters.includes(this.graphType.slice(0, -5))) {
        return this.graphType;
      }

      return 'sessionsHisto';
    },
    /* exposed functions --------------------------------------------------- */
    /* exposed MAP functions */
    toggleMap: function () {
      if (this.primary) {
        this.showMap = !this.showMap;
        this.$store.commit('toggleMaps', this.showMap);
        localStorage[`${basePath}-open-map`] = this.showMap;
        // let the parent know that there's no map data but the
        // map has been toggled open, so go fetch data
        if (this.showMap && !Object.keys(this.mapData).length) {
          sessionStorage['force-aggregations'] = true; // NOTE this runs a new query, so force aggregations again
          this.$emit('fetchMapData');
        }
      }
    },
    toggleMapSize: function () {
      this.mapExpanded = !this.mapExpanded;
      // The .expanded class on the parent div drives the fullscreen
      // CSS layout for WorldMap; the component re-fits via its own
      // ResizeObserver.
      if (this.mapExpanded) {
        $(document).on('mouseup', this.isOutsideClick);
      } else {
        $(document).off('mouseup', this.isOutsideClick);
      }
    },
    toggleSrcDstXff: function (type) {
      if (this.primary) { // primary map sets all other map's src/dst/xff
        this[type] = !this[type];
      }
    },
    /* exposed GRAPH functions */
    changeGraphType: function (newGraphType) {
      if (this.primary) { // primary graph sets all graph's histo type
        this.graphType = newGraphType;
        this.$store.commit('updateGraphType', this.graphType);
      }
    },
    changeSeriesType: function (newSeriesType) {
      if (this.primary) { // primary graph sets all graph's series type
        this.seriesType = newSeriesType;
        this.$store.commit('updateSeriesType', this.seriesType);
      }
    },
    zoomOut: function () {
      this.$refs.timelineGraph?.zoomOut();
      this.debounce(this.updateResults, 600);
    },
    zoomIn: function () {
      this.$refs.timelineGraph?.zoomIn();
      this.debounce(this.updateResults, 600);
    },
    panLeft: function () {
      this.$refs.timelineGraph?.panLeft(this.plotPan);
      this.debounce(this.updateResults, 600);
    },
    panRight: function () {
      this.$refs.timelineGraph?.panRight(this.plotPan);
      this.debounce(this.updateResults, 600);
    },
    plotPanChange: function (value) {
      this.plotPan = value;
    },
    toggleCapStartTimes (newValue) {
      this.showCapStartTimes = newValue;
      localStorage[`${basePath}-cap-times`] = this.showCapStartTimes;
      // TimelineGraph re-renders via its own showCapStartTimes prop watcher
      // once the cap-restart times are refreshed in the store.
      StatsService.getCapRestartTimes(basePath);
    },
    toggleSpanning (newValue) {
      this.$router.push({
        query: {
          ...this.$route.query,
          spanning: newValue ? 'true' : undefined
        }
      }).then(() => {
        this.$emit('spanningChange');
      });
    },
    /* helper functions ---------------------------------------------------- */
    debounce: function (func, ms) {
      if (timeout) { clearTimeout(timeout); }

      timeout = setTimeout(() => { func(); }, ms);
    },
    /* helper GRAPH functions */
    updateResults: function () {
      const range = this.$refs.timelineGraph?.getXRange();
      if (!range) return;
      this.updateStopStartTime(range);
    },
    updateStopStartTime: function (times) {
      if (times.startTime && times.stopTime) {
        this.$store.commit('setTimeRange', 0); // set time range to custom
        this.$store.commit('setTime', times); // set start/stop time

        if (this.$route.query.date !== undefined ||
            this.$route.query.startTime !== times.startTime || this.$route.query.stopTime !== times.stopTime) {
          this.$router.push({ // issue a search with the new time params
            query: {
              ...this.$route.query,
              date: undefined,
              stopTime: times.stopTime,
              startTime: times.startTime
            }
          });
        }
      }
    },
    /* helper MAP functions */
    isOutsideClick: function (e) {
      const element = $('#vizContainer' + this.id);
      if (!$(element).is(e.target) &&
        $(element).has(e.target).length === 0) {
        this.mapExpanded = false;
      }
    },
    onMapRegionClick: function (code) {
      this.$store.commit('addToExpression', {
        expression: `country == ${code}`
      });
    },
    onUpdateLegend: function (legend) {
      this.legend = legend;
    }
  },
  beforeUnmount () {
    if (timeout) { clearTimeout(timeout); }
    $(document).off('mouseup', this.isOutsideClick);
  }
};
</script>

<style>
/* map styles ---------------------- */
.inline-map .map-container > .map {
  z-index: 3;
  /* Match the timeline graph wrapper height so the two panels line up
     visually. Left margin gives breathing room between the timeline
     and the map. */
  height: 200px;
  width: calc(100% - 8px);
  margin-left: 8px;
}

/* legend (top 10) */
.map-container .map-legend {
  max-width: 94%;
  margin-left: 4px;
  font-size: .8rem;
  position: fixed;
  bottom: 22px;
  right: 16px;
  z-index: 6;
  padding: 0 0 2px 4px;
  border-radius: 4px;
  background-color: #fff;
}

.map-container .map-legend .legend-item {
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

.map-container .map-legend strong {
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
.tickLabel {
  font-size: 11px;
  fill: var(--color-foreground);
}

/* position the pan dropdown between the pan buttons */
.pan-dropdown > button {
  height: 22px;
  line-height: 1;
  font-size: small;
  border-radius: 0;
  margin-left: -1px;
}
.pan-buttons > label {
  margin-top: 2.4px;
  height: 22px !important;
}

.zoom-buttons {
  margin-top: 2px;
}
</style>

<style scoped>
.inline-map {
  width: 24%;
  float: right;
  position: relative;
}

.map-invisible .inline-map {
  visibility: hidden;
  width: 5px !important;
}

/* fixed width buttons are the same width regardless of content */
.btn-fw {
  width: 26px;
}

.map-btn {
  display: block;
  position: absolute;
  right: 0;
  z-index: 3;
  overflow: hidden;
  padding: 2px 8px 3px 8px;
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

/* make sure button stays under sticky viz */
.btn-z-index-2 {
  z-index: 2;
}

.btn-expand-map {
  position: absolute;
  top: 26px;
  right: 2px;
  z-index: 3;
}

.src-dst-btns {
  position: absolute;
  top: 50px;
  right: 2px;
  z-index: 3;
}

.xff-btn {
  position: absolute;
  top: 95px;
  right: 2px;
  z-index: 3;
  padding: 0;
}

.expanded .map-container {
  position: fixed;
  z-index: 5;
  right: 9px;
  top: 160px;
  /* Replaces jvectormap's onMapResize JS that used to size the expanded
     map (95% of window width, window height minus the headers). */
  width: 95vw;
  height: calc(100vh - 175px);
}
.expanded .map-container > .map {
  height: 100%;
  margin-bottom: 0;
}

/* show the buttons on top of the map */
.expanded .src-dst-btns,
.expanded .xff-btn,
.expanded .btn-close-map,
.expanded .btn-expand-map {
  z-index: 6;
}

/* graph styles -------------------- */
.map-visible .plot-container {
  position: relative;
  display: inline-block;
  width: 76%;
}
.map-invisible .plot-container {
  position: relative;
  width: 99%;
}

/* center timeline buttons */
.session-graph-btn-container {
  position: absolute;
  left: 50%;
  white-space: nowrap;
  z-index: 1;
  /* Pushed down so the buttons sit cleanly inside the timeline graph
     panel's top padding instead of overlaying the chart bars. */
  margin-top: 6px;
}

.session-graph-btn-container > div {
  position: relative;
  left: -50%;
}

/* center timeline buttons on timeline graph if the map is collapsed */
.map-visible .session-graph-btn-container {
  left: 12%;
}
.map-visible .session-graph-btn-container > div {
  left: 0;
}

/* sticky visualizations styles --------------- */
.sticky-viz {
  padding-bottom: 178px;
}

.sticky-viz .viz-container {
  left: 0;
  right: 0;
  z-index: 3;
  height: 183px;
  position: fixed;
  overflow: hidden;
  box-shadow: 0 0 16px -2px black;
  background-color: var(--color-background, white);
}

.sticky-viz.disabled-msg {
  padding-bottom: 48px;
}
.sticky-viz.disabled-msg .viz-container {
  height: 62px;
}

/* viz options button styles ----------------- */
.viz-options-btn-container {
  z-index: 5;
  position: relative;
}
.viz-options-btn {
  right: 8px;
  display: block;
  position: fixed;
  margin-top: -35px;
}

/* hide visualization styles ----------------- */
.hide-viz .viz-container {
  height: auto;
}
.hide-viz.sticky-viz {
  padding-bottom: 0px;
}
.hide-viz.sticky-viz .viz-container {
  z-index: 4;
  overflow: visible;
  box-shadow: none !important;
  background-color: transparent;
}
</style>
