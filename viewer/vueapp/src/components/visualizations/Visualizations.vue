<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div :class="{'sticky-viz':stickyViz && primary, 'hide-viz':hideViz && primary, 'disabled-msg':disabledAggregations}">
    <div
      class="pt-2 ps-2 pe-2 viz-container"
      :id="'vizContainer' + id"
      :class="{'map-visible':showMap,'map-invisible':!showMap,'map-expanded':mapExpanded}">
      <div v-show="!hideViz">
        <div
          v-if="disabledAggregations"
          class="text-center">
          <v-alert
            type="info"
            :icon="false"
            density="compact"
            variant="tonal"
            class="d-inline-block text-start">
            <template #title>
              <span>
                {{ $t('vis.hideViz', { days: turnOffGraphDays }) }}
              </span>
              <v-icon
                id="hideVizInfo"
                icon="mdi-information"
                size="small"
                class="ms-1 cursor-help" />
              <v-tooltip activator="#hideVizInfo">
                {{ $t('vis.hideVizTip') }}
              </v-tooltip>
            </template>
            {{ $t('vis.hideVizMore') }}
          </v-alert>
        </div>

        <template v-else>
          <!-- map content -->
          <div :class="{'expanded':mapExpanded}">
            <!-- map open button -->
            <div
              class="map-btn"
              v-show="!showMap && primary"
              @click="toggleMap">
              <v-icon icon="mdi-earth" />
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
                  <v-btn
                    v-if="primary"
                    :aria-label="$t('common.close')"
                    variant="outlined"
                    size="small"
                    density="comfortable"
                    icon
                    class="btn-close-map"
                    @click="toggleMap">
                    <v-icon icon="mdi-close" />
                  </v-btn>
                  <v-btn
                    :aria-label="$t('vis.toggleMapSize')"
                    variant="outlined"
                    size="small"
                    density="comfortable"
                    icon
                    :class="['btn-z-index-2', primary ? 'btn-expand-map' : 'btn-close-map']"
                    @click="toggleMapSize">
                    <v-icon :icon="mapExpanded ? 'mdi-arrow-collapse' : 'mdi-arrow-expand'" />
                  </v-btn>
                  <div
                    v-if="primary"
                    class="src-dst-btns d-flex flex-column">
                    <v-btn
                      :variant="src ? 'flat' : 'outlined'"
                      :color="src ? 'primary' : undefined"
                      size="small"
                      density="comfortable"
                      icon
                      @click="toggleSrcDstXff('src')">
                      <strong>S</strong>
                      <v-tooltip activator="parent">
                        {{ $t('vis.toggleSrcCountry') }}
                      </v-tooltip>
                    </v-btn>
                    <v-btn
                      :variant="dst ? 'flat' : 'outlined'"
                      :color="dst ? 'primary' : undefined"
                      size="small"
                      density="comfortable"
                      icon
                      @click="toggleSrcDstXff('dst')">
                      <strong>D</strong>
                      <v-tooltip activator="parent">
                        {{ $t('vis.toggleDstCountry') }}
                      </v-tooltip>
                    </v-btn>
                  </div>
                  <v-btn
                    v-if="primary"
                    :variant="xffGeo ? 'flat' : 'outlined'"
                    :color="xffGeo ? 'primary' : undefined"
                    size="small"
                    density="comfortable"
                    icon
                    class="xff-btn"
                    @click="toggleSrcDstXff('xffGeo')"
                    title="Toggle XFF Countries">
                    <small>XFF</small>
                  </v-btn> <!-- /map buttons -->

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
          <div class="graph-content">
            <!-- graph -->
            <div
              v-if="graphData"
              class="plot-container">
              <!-- graph controls overlay (slides down on hover) -->
              <div
                class="session-graph-btn-container"
                v-if="primary">
                <!-- zoom in/out -->
                <div class="zoom-buttons">
                  <v-btn
                    size="small"
                    variant="text"
                    density="comfortable"
                    icon
                    @click="zoomOut">
                    <v-icon icon="mdi-magnify-minus" />
                  </v-btn>
                  <v-btn
                    size="small"
                    variant="text"
                    density="comfortable"
                    icon
                    @click="zoomIn">
                    <v-icon icon="mdi-magnify-plus" />
                  </v-btn>
                </div> <!-- /zoom in/out -->
                <!-- pan left/right -->
                <div class="pan-buttons">
                  <v-btn
                    size="small"
                    variant="text"
                    density="comfortable"
                    icon
                    @click="panLeft">
                    <v-icon icon="mdi-chevron-left" />
                  </v-btn>
                  <v-menu>
                    <template #activator="{ props: activatorProps }">
                      <v-btn
                        v-bind="activatorProps"
                        size="small"
                        variant="text"
                        density="comfortable">
                        {{ plotPan * 100 + '%' }}
                        <v-icon
                          icon="mdi-menu-down"
                          class="ms-1" />
                      </v-btn>
                    </template>
                    <v-list density="compact">
                      <v-list-item @click="plotPanChange(0.05)">
                        5%
                      </v-list-item>
                      <v-list-item @click="plotPanChange(0.1)">
                        10%
                      </v-list-item>
                      <v-list-item @click="plotPanChange(0.2)">
                        20%
                      </v-list-item>
                      <v-list-item @click="plotPanChange(0.5)">
                        50%
                      </v-list-item>
                      <v-list-item @click="plotPanChange(1)">
                        100%
                      </v-list-item>
                    </v-list>
                  </v-menu>
                  <v-btn
                    size="small"
                    variant="text"
                    density="comfortable"
                    icon
                    @click="panRight">
                    <v-icon icon="mdi-chevron-right" />
                  </v-btn>
                </div> <!-- /pan left/right -->
                <!-- graph type -->
                <v-radio-group
                  inline
                  density="compact"
                  hide-details
                  class="ms-1"
                  :model-value="graphType"
                  @update:model-value="changeGraphType">
                  <v-radio
                    value="sessionsHisto"
                    key="sessionsHisto"
                    :label="$t('common.sessions')" />
                  <v-radio
                    v-for="filter in timelineDataFilters"
                    :value="filter.dbField + 'Histo'"
                    :key="filter.dbField"
                    :label="filter.friendlyName" />
                </v-radio-group> <!-- /graph type -->
                <!-- series type -->
                <v-radio-group
                  inline
                  density="compact"
                  hide-details
                  class="ms-1"
                  :model-value="seriesType"
                  @update:model-value="changeSeriesType">
                  <v-radio
                    value="lines"
                    :label="$t('vis.graphLines')" />
                  <v-radio
                    value="bars"
                    :label="$t('vis.graphBars')" />
                </v-radio-group> <!-- /series type -->
                <!-- y axis scale -->
                <v-radio-group
                  inline
                  density="compact"
                  hide-details
                  class="ms-1"
                  :model-value="yScale"
                  @update:model-value="changeYScale">
                  <v-radio
                    value="linear"
                    :label="$t('vis.graphLinear')" />
                  <v-radio
                    value="log"
                    :label="$t('vis.graphLog')" />
                </v-radio-group> <!-- /y axis scale -->
                <!-- cap times -->
                <div class="ms-1">
                  <v-checkbox
                    density="compact"
                    hide-details
                    :model-value="showCapStartTimes"
                    :label="$t('vis.capRestarts')"
                    @update:model-value="toggleCapStartTimes" />
                  <v-tooltip
                    activator="parent"
                    location="bottom">
                    {{ $t('vis.capRestartsTip') }}
                  </v-tooltip>
                </div> <!-- /cap times -->
                <!-- spanning -->
                <div class="ms-1">
                  <v-checkbox
                    density="compact"
                    hide-details
                    :model-value="spanning"
                    :disabled="timeBounding === 'database'"
                    :label="$t('search.timeBounding-spanning')"
                    @update:model-value="toggleSpanning" />
                  <v-tooltip
                    activator="parent"
                    location="bottom">
                    {{ $t('search.spanningTip') }}
                  </v-tooltip>
                </div> <!-- /spanning -->
              </div> <!-- /graph controls overlay -->
              <timeline-graph
                ref="timelineGraph"
                :graph-data="graphData"
                :graph-type="graphType"
                :series-type="seriesType"
                :y-scale="yScale"
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
      // yScale persists in localStorage per-app (sessions/spiview/spigraph/
      // hunt/connections each get their own key) -- hydrated in created().
      yScale: 'linear',
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

    if (localStorage && localStorage[`${basePath}-y-scale`] === 'log') {
      this.yScale = 'log';
    }

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
    changeYScale (newValue) {
      this.yScale = newValue;
      localStorage[`${basePath}-y-scale`] = newValue;
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
  /* 186 = TimelineGraph host (180) + wrapper top padding (6); the two
     panels line up flush. Left margin gives breathing room between
     the timeline and map. */
  height: 186px;
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
  color: rgb(var(--v-theme-neutral-lighter));
  padding: 3px;
  font-size: 8pt;
  background-color: rgb(var(--v-theme-black));
  border-radius: 4px;
  border: 1px solid rgb(var(--v-theme-black));
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
  border-color: rgb(var(--v-theme-black)) transparent transparent transparent;
}

/* make graph labels smaller */
.tickLabel {
  font-size: 11px;
  fill: rgb(var(--v-theme-foreground));
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

.map-btn {
  display: block;
  position: absolute;
  right: 0;
  z-index: 3;
  overflow: hidden;
  padding: 2px 8px 3px 8px;
  border-radius: 4px 0 0 4px;
  cursor: pointer;
  background-color: rgb(var(--v-theme-primary));
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
  gap: 4px;
}

.xff-btn {
  position: absolute;
  top: 106px;
  right: 2px;
  z-index: 3;
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

/* timeline controls toolbar — hidden above the chart by default,
   slides down to overlay the top of the timeline on hover. Trigger
   is the plot-container itself (not graph-content) so hovering the
   floated map next door doesn't fire it. */
.session-graph-btn-container {
  position: absolute;
  top: 0;
  left: 0;
  right: 0;
  z-index: 4;
  display: flex;
  flex-wrap: wrap;
  align-items: stretch;
  gap: 0;
  padding: 2px 4px;
  /* Theme-aware overlay: 60 % of the page background color from the
     active theme. Light themes get a translucent white overlay, dark
     themes get a translucent dark overlay — text contrast just works. */
  background: color-mix(in srgb, rgb(var(--v-theme-background)) 60%, transparent);
  backdrop-filter: blur(4px);
  -webkit-backdrop-filter: blur(4px);
  border-radius: 6px 6px 0 0;
  box-shadow:
    inset 0 1px 0 rgba(255, 255, 255, 0.15),
    0 2px 6px rgba(0, 0, 0, 0.18);
  transform: translateY(-100%);
  opacity: 0;
  transition: transform 220ms ease-out, opacity 180ms ease-out;
  pointer-events: none;
}
/* Each control group gets uniform horizontal padding + a vertical
   divider on its left so the groups read as discrete cells. Wider
   inter-group spacing for breathing room. */
.session-graph-btn-container > * {
  display: flex;
  align-items: center;
  gap: 2px;
  padding: 0 14px;
  min-height: 30px;
  margin: 0 !important;
}
.session-graph-btn-container > * + * {
  border-left: 1px solid color-mix(in srgb, rgb(var(--v-theme-foreground)) 18%, transparent);
}
/* v-btn theming inside the toolbar: snug icon buttons, neutral hover */
.session-graph-btn-container :deep(.v-btn) {
  min-width: 28px;
  letter-spacing: 0;
}
.plot-container:hover .session-graph-btn-container {
  transform: translateY(0);
  opacity: 1;
  pointer-events: auto;
}
/* Tighten Vuetify's default vertical chrome on the radio groups
   and checkboxes so they sit neatly inline with the v-btn groups. */
.session-graph-btn-container :deep(.v-radio-group),
.session-graph-btn-container :deep(.v-checkbox) {
  flex: 0 0 auto;
}
.session-graph-btn-container :deep(.v-input__control),
.session-graph-btn-container :deep(.v-selection-control-group) {
  display: flex;
  flex-direction: row;
  align-items: center;
}
.session-graph-btn-container :deep(.v-selection-control) {
  min-height: 28px;
  flex: 0 0 auto;
}
.session-graph-btn-container :deep(.v-label) {
  font-size: 12px;
  opacity: 0.9;
}

/* Own stacking context so map buttons inside (zoom controls at z:5,
   close-map at z:3) stay local and don't bubble up past the primary
   sticky viz at z:3 -- this was bleeding lower-bucket map buttons over
   the pinned top viz on Spigraph default view. */
.viz-container {
  position: relative;
  z-index: 0;
}

/* sticky ("pinned") visualizations --------------- */
/* The pinned viz is a normal-flow chrome row inside the page shell, so
   no fixed positioning and no reserved flow space. 195 = 8 viz-container
   padding-top + 186 chart/map (TimelineGraph host 180 + wrapper
   padding-top 6; map matches at .inline-map > .map). +1 keeps the
   overflow-hidden clip off antialiased chart edges. */
.sticky-viz .viz-container {
  height: 195px;
  overflow: hidden;
  background-color: rgb(var(--v-theme-background));
}

/* When this panel's map is expanded to fullscreen, lift the whole
   viz-container stacking context above page content. The expanded
   .map-container is position:fixed, but position:fixed escapes layout --
   NOT an ancestor stacking context. .viz-container sets z-index:0 above
   (to keep inline map buttons local), so the expanded map's z-index:5 is
   scoped inside this context at the page's z-index:0 level. Page UI with
   any positive z-index then paints above the map and intercepts its
   clicks ("I can click things underneath the expanded map"). Lifting the
   context only while expanded fixes the click-through without touching
   the intentional default-view button stacking. */
.viz-container.map-expanded {
  z-index: 10;
}

/* Disabled-aggregations variant: chart is replaced by a compact v-alert
   info card (~80px with title + body). */
.sticky-viz.disabled-msg .viz-container {
  height: 90px;
}

/* hide visualization styles ----------------- */
.hide-viz .viz-container {
  height: auto;
}
.hide-viz.sticky-viz .viz-container {
  overflow: visible;
  box-shadow: none !important;
  background-color: transparent;
}
</style>
