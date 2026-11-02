<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <page-layout class="spigraph-page">
    <template #chrome>
      <ArkimeCollapsible>
        <div class="page-toolbar">
          <!-- search navbar -->
          <arkime-search
            :num-matching-sessions="filtered"
            @change-search="onSearch" /> <!-- /search navbar -->

          <!-- spigraph sub navbar -->
          <div class="spigraph-form mx-1">
            <div class="d-flex flex-wrap align-center gap-1 page-subnav">
              <!-- main graph type select -->
              <div class="arkime-input-group">
                <span class="arkime-input-label">
                  {{ $t('spigraph.graphType') }}:
                </span>
                <select
                  class="arkime-input-control"
                  :value="spiGraphType"
                  @change="changeSpiGraphType($event.target.value)">
                  <option
                    v-for="t in sortedGraphTypeOptions"
                    :key="t"
                    :value="t">
                    {{ $t(`spigraph.graphType-${t}`) }}
                  </option>
                </select>
              </div> <!-- /main graph type select -->

              <!-- saved graphs: save, load, share, add to a dashboard; right
                   after the type so it is in the same place for every graph -->
              <spigraph-config-dropdown
                :current-graph="currentGraph"
                :loaded-graph-id="$route.query.graphId"
                @load="loadGraph"
                @saved-as="onGraphSavedAs"
                @message="onGraphMessage" />

              <!-- field select -->
              <div
                class="arkime-input-group"
                v-if="!isConnectionsType && fields && fields.length && fieldTypeahead">
                <span class="arkime-input-label">
                  {{ $t('spigraph.field') }}:
                </span>
                <arkime-field-typeahead
                  :fields="fields"
                  query-param="exp"
                  :initial-value="fieldTypeahead"
                  @field-selected="changeField"
                  page="Spigraph" />
              </div> <!-- /field select -->

              <!-- maxElements select -->
              <div
                class="arkime-input-group"
                v-if="!isConnectionsType">
                <span
                  id="maxElements"
                  class="arkime-input-label cursor-help">
                  {{ $t('spigraph.maxElements') }}:
                </span>
                <v-tooltip activator="#maxElements">
                  {{ $t('spigraph.maxElementsTip') }}
                </v-tooltip>
                <select
                  class="arkime-input-control"
                  :value="query.size"
                  @change="changeMaxElements(Number($event.target.value))">
                  <option
                    v-for="opt in maxElementsOptions"
                    :key="opt"
                    :value="opt">
                    {{ opt }}
                  </option>
                </select>
              </div> <!-- /maxElements select -->

              <!-- connections controls anchor -->
              <div
                v-if="isConnectionsType"
                id="connections-controls-anchor"
                class="connections-controls-anchor d-flex flex-wrap align-center gap-1" />

              <!-- bar layout (bar only): how a second field's values are drawn -->
              <div
                class="arkime-input-group"
                v-if="spiGraphType === 'bar'">
                <span class="arkime-input-label">
                  {{ $t('spigraph.barMode') }}:
                </span>
                <select
                  class="arkime-input-control"
                  :value="barMode"
                  @change="changeBarMode($event.target.value)">
                  <option value="stacked">
                    {{ $t('spigraph.barMode-stacked') }}
                  </option>
                  <option value="grouped">
                    {{ $t('spigraph.barMode-grouped') }}
                  </option>
                </select>
              </div> <!-- /bar layout -->

              <!-- bar orientation (bar only): standing up, or lying along the axis -->
              <div
                class="arkime-input-group"
                v-if="spiGraphType === 'bar'">
                <span class="arkime-input-label">
                  {{ $t('sessions.summary.widget.orientation') }}:
                </span>
                <select
                  class="arkime-input-control"
                  :value="barOrientation"
                  @change="changeBarOrientation($event.target.value)">
                  <option value="vertical">
                    {{ $t('sessions.summary.widget.orientation-vertical') }}
                  </option>
                  <option value="horizontal">
                    {{ $t('sessions.summary.widget.orientation-horizontal') }}
                  </option>
                </select>
              </div> <!-- /bar orientation -->

              <!-- pie layout (pie only): nested rings, or one pie per value
                   of the first field (split) -->
              <div
                class="arkime-input-group"
                v-if="spiGraphType === 'pie'">
                <span class="arkime-input-label">
                  {{ $t('spigraph.pieMode') }}:
                </span>
                <select
                  class="arkime-input-control"
                  :value="pieMode"
                  @change="changePieMode($event.target.value)">
                  <option value="nested">
                    {{ $t('spigraph.pieMode-nested') }}
                  </option>
                  <option value="split">
                    {{ $t('spigraph.pieMode-split') }}
                  </option>
                </select>
              </div> <!-- /pie layout -->

              <!-- split pie only: how many pies (the first field's top values) -->
              <div
                class="arkime-input-group"
                v-if="spiGraphType === 'pie' && pieMode === 'split'">
                <span class="arkime-input-label">
                  {{ $t('sessions.summary.widget.splitPies') }}:
                </span>
                <select
                  class="arkime-input-control"
                  :value="pieCount"
                  @change="changePieCount($event.target.value)">
                  <option
                    v-for="n in pieCounts"
                    :key="n"
                    :value="n">
                    {{ n }}
                  </option>
                </select>
              </div> <!-- /pie count -->

              <!-- pie only: a gray slice for what falls outside the top values -->
              <label
                v-if="spiGraphType === 'pie'"
                class="arkime-input-group cursor-pointer">
                <span class="arkime-input-label">
                  <input
                    type="checkbox"
                    class="arkime-check-input me-2"
                    :checked="showOther"
                    @change="changeShowOther($event.target.checked)">
                  {{ $t('spigraph.showOther') }}
                </span>
                <v-tooltip activator="parent">
                  {{ $t('sessions.summary.widget.showOtherTip') }}
                </v-tooltip>
              </label> <!-- /pie other -->

              <!-- table layout (table only): the fields' value combinations
                   (intersection), or each field's own top values (unique) -->
              <div
                class="arkime-input-group"
                v-if="spiGraphType === 'table'">
                <span class="arkime-input-label">
                  {{ $t('spigraph.tableMode') }}:
                </span>
                <select
                  class="arkime-input-control"
                  :value="tableMode"
                  @change="changeTableMode($event.target.value)">
                  <option value="intersection">
                    {{ $t('spigraph.tableMode-intersection') }}
                  </option>
                  <option value="unique">
                    {{ $t('spigraph.tableMode-unique') }}
                  </option>
                </select>
              </div> <!-- /table layout -->

              <!-- timeline layout (timeline only): one timeline per value, or
                   the values stacked in one (as counts, or as shares) -->
              <div
                class="arkime-input-group"
                v-if="spiGraphType === 'default'">
                <span class="arkime-input-label">
                  {{ $t('spigraph.timelineMode') }}:
                </span>
                <select
                  class="arkime-input-control"
                  :value="timelineMode"
                  @change="changeTimelineMode($event.target.value)">
                  <option
                    v-for="m in timelineModes"
                    :key="m"
                    :value="m">
                    {{ $t(`spigraph.timelineMode-${m}`) }}
                  </option>
                </select>
              </div> <!-- /timeline layout -->

              <!-- intensity select (heatmap and time of day) -->
              <div
                class="arkime-input-group"
                v-if="spiGraphType === 'heatmap' || spiGraphType === 'timeofday'">
                <span class="arkime-input-label">
                  {{ $t('spigraph.intensity') }}:
                </span>
                <select
                  class="arkime-input-control"
                  :value="intensity"
                  @change="changeIntensity($event.target.value)">
                  <option
                    v-for="opt in intensityOptions"
                    :key="opt.value"
                    :value="opt.value">
                    {{ opt.label }}
                  </option>
                </select>
              </div> <!-- /intensity select -->

              <!-- sort select (not shown for the pie graph) -->
              <div
                class="arkime-input-group"
                v-if="isHistoType">
                <span class="arkime-input-label">
                  {{ $t('spigraph.sortBy') }}:
                </span>
                <select
                  class="arkime-input-control"
                  :value="sortBy"
                  @change="changeSortBy($event.target.value)">
                  <option value="name">
                    {{ $t('spigraph.sortBy-name') }}
                  </option>
                  <option value="graph">
                    {{ $t('spigraph.sortBy-graph') }}
                  </option>
                </select>
              </div> <!-- /sort select -->

              <!-- refresh input (not shown for pie) -->
              <div
                class="arkime-input-group"
                v-if="isHistoType">
                <span class="arkime-input-label">
                  {{ $t('spigraph.refreshEvery') }}:
                </span>
                <select
                  class="arkime-input-control"
                  :value="refresh"
                  @change="changeRefreshInterval(Number($event.target.value))">
                  <option
                    v-for="opt in refreshOptions"
                    :key="opt"
                    :value="opt">
                    {{ opt }}
                  </option>
                </select>
                <span class="arkime-input-label">
                  {{ $t('common.seconds') }}
                </span>
              </div> <!-- /refresh input-->

              <!-- page info -->
              <div
                class="records-display align-self-center"
                v-if="isHistoType">
                <strong
                  class="text-theme-accent"
                  v-if="!error && recordsFiltered !== undefined">
                  {{ $t('common.showingAllTip', { count: commaString(recordsFiltered), total: commaString(recordsTotal) }) }}
                </strong>
              </div> <!-- /page info -->

              <!-- export button-->
              <v-btn
                v-if="!isHistoType && spiGraphType !== 'sankey' && !isConnectionsType"
                variant="outlined"
                size="small"
                density="comfortable"
                class="ms-1"
                prepend-icon="mdi-download"
                @click.stop.prevent="exportCSV">
                {{ $t('spigraph.exportCSV') }}
                <v-tooltip activator="parent">
                  {{ $t('spigraph.exportCSVSPIGraphTip') }}
                </v-tooltip>
              </v-btn> <!-- /export button-->

              <!-- hierarchy "add another field" anchor (own full-width row); a
                   tag cloud draws one field, so it has none -->
              <div
                v-if="['bar', 'pie', 'table', 'treemap', 'sankey', 'matrix'].includes(spiGraphType)"
                id="spigraph-subfield-anchor"
                class="spigraph-subfield-anchor d-flex flex-wrap align-center gap-1" />
            </div>
          </div>
        </div>
      </ArkimeCollapsible>
    </template>

    <!-- main visualization (timeline/map): always inline, scrolls with content -->
    <div v-if="spiGraphType === 'default' && mapData && graphData && fieldObj && showToolBars">
      <arkime-visualizations
        id="primary"
        :graph-data="graphData"
        :map-data="mapData"
        :primary="true"
        :timeline-data-filters="timelineDataFilters"
        @fetch-map-data="cancelAndLoad(true)"
        @spanning-change="cancelAndLoad(true)" />
    </div> <!-- /main visualization -->

    <div
      class="spigraph-content"
      v-if="!isConnectionsType">
      <!-- heatmap graph type -->
      <div v-if="spiGraphType === 'heatmap'">
        <arkime-heatmap
          v-if="items && items.length && fieldObj && graphData"
          :items="items"
          :graph="graphData"
          :field-obj="fieldObj"
          :metric="intensity"
          :timeline-data-filters="timelineDataFilters"
          :sort-by="sortBy" />
      </div> <!-- /heatmap graph type -->

      <!-- time of day graph type: the search as a week of hours, then one
           small week per top value of the field -->
      <div
        v-else-if="spiGraphType === 'timeofday'"
        class="timeofday-page">
        <div
          v-if="graphData && fieldObj"
          class="timeofday-primary">
          <time-of-day-chart
            :graph="graphData"
            :metric="intensity"
            :metric-label="intensityLabel"
            :timezone="timezone" />
        </div>
        <div
          v-if="fieldObj"
          class="timeofday-grid">
          <div
            v-for="item in items"
            :key="item.name"
            class="timeofday-cell">
            <div class="spi-bucket">
              <strong>
                <arkime-session-field
                  :field="fieldObj"
                  :value="item.name"
                  :expr="fieldObj.exp"
                  :parse="true"
                  :pull-left="true"
                  :session-btn="true" />
              </strong>
              <sup>({{ commaString(item[intensity]) }})</sup>
            </div>
            <div class="timeofday-chart">
              <time-of-day-chart
                :graph="item.graph || {}"
                :metric="intensity"
                :metric-label="intensityLabel"
                :timezone="timezone"
                compact />
            </div>
          </div>
        </div>
      </div> <!-- /time of day graph type -->

      <!-- stacked timeline: the field's top values as one stacked timeline,
           as counts or as each value's share of the bucket -->
      <div
        v-else-if="spiGraphType === 'default' && timelineMode !== 'separate'"
        class="timeline-stacked">
        <div
          v-if="stackedSeries.length"
          class="timeline-stacked__legend">
          <span
            v-for="s in stackedSeries"
            :key="s.key"
            class="timeline-stacked__item">
            <span
              class="timeline-stacked__swatch"
              :style="{ background: s.color }" />
            <arkime-session-field
              :field="fieldObj"
              :value="s.label"
              :expr="fieldObj.exp"
              :parse="true"
              :pull-left="true"
              :session-btn="true" />
          </span>
        </div>
        <div class="timeline-stacked__plot">
          <timeline-graph
            v-if="stackedGraph && stackedSeries.length"
            :graph-data="stackedGraph"
            :graph-type="graphType || 'sessionsHisto'"
            :series-defs="stackedSeries"
            :series-type="$store.state.seriesType === 'lines' ? 'lines' : 'bars'"
            :y-scale="timelineMode === 'percent' ? 'percent' : 'linear'"
            stacked
            fit-height
            :timeline-data-filters="timelineDataFilters"
            :timezone="timezone"
            @update-time-range="onStackedTimeRange" />
        </div>
      </div> <!-- /stacked timeline -->

      <!-- pie graph type -->
      <div v-else-if="spiGraphType !== 'default'">
        <arkime-pie
          v-if="items && items.length"
          :base-field="baseField"
          :graph-data="items"
          :fields="fields"
          :query="query"
          :spi-graph-type="spiGraphType"
          :bar-mode="barMode"
          :bar-orientation="barOrientation"
          :pie-mode="pieMode"
          :pie-count="pieCount"
          :show-other="showOther"
          :table-mode="tableMode"
          @toggle-load="toggleLoad"
          @toggle-error="toggleError"
          @fetched-results="fetchedResults" />
      </div> <!-- /pie graph type -->

      <!-- default graph type -->
      <div v-else>
        <!-- values -->
        <template v-if="fieldObj">
          <div
            v-for="(item, index) in items"
            :key="item.name"
            class="spi-graph-item ps-1 pe-1 pt-1">
            <!-- field value -->
            <v-row>
              <v-col
                cols="12"
                md="12">
                <div class="spi-bucket">
                  <strong>
                    <arkime-session-field
                      :field="fieldObj"
                      :value="item.name"
                      :expr="fieldObj.exp"
                      :parse="true"
                      :pull-left="true"
                      :session-btn="true" />
                  </strong>
                  <sup>({{ commaString(item[graphType]) }})</sup>
                </div>
              </v-col>
            </v-row> <!-- /field value -->
            <!-- field visualization -->
            <v-row>
              <v-col
                cols="12"
                md="12">
                <arkime-visualizations
                  :id="(index + 1).toString()"
                  :graph-data="item.graph"
                  :map-data="item.map"
                  :primary="false"
                  :timeline-data-filters="timelineDataFilters" />
              </v-col>
            </v-row> <!-- /field visualization -->
          </div>
        </template> <!-- /values -->
      </div> <!-- /default graph type -->

      <!-- loading overlay -->
      <arkime-loading
        :can-cancel="true"
        v-if="loading && !error"
        @cancel="cancelAndLoad" /> <!-- /loading overlay -->

      <!-- page error -->
      <arkime-error
        v-if="error"
        :message="error"
        class="mt-5 mb-5" /> <!-- /page error -->

      <!-- no results -->
      <arkime-no-results
        v-if="!error && !loading && !items.length"
        class="mt-5 mb-5"
        :records-total="recordsTotal"
        :view="query.view" /> <!-- /no results -->
    </div>

    <!-- connections network graph type (self-contained: owns its own
         /api/connections fetch, controls, and overlay buttons) -->
    <arkime-connections-graph
      v-else
      ref="connectionsGraph"
      :layout="spiGraphType === 'tree' ? 'tree' : 'force'" /> <!-- /connections graph type -->

    <v-snackbar
      v-model="snackbar.show"
      :color="snackbar.color"
      :timeout="5000">
      {{ snackbar.text }}
      <template
        v-if="snackbar.link"
        #actions>
        <v-btn
          variant="text"
          :to="snackbar.link">
          {{ $t('sessions.summary.addToDashboard.open') }}
        </v-btn>
      </template>
    </v-snackbar>

    <!-- overlay anchor: connections floating buttons teleport here -->
    <template
      v-if="isConnectionsType"
      #overlay>
      <div id="connections-overlay-anchor" />
    </template>
  </page-layout>
</template>

<script>
// import services
import SpigraphService from './SpigraphService';
import FieldService from '../search/FieldService';
import ConfigService from '../utils/ConfigService';
// import internal
import ArkimeError from '../utils/Error.vue';
import ArkimeSearch from '../search/Search.vue';
import ArkimeLoading from '../utils/Loading.vue';
import ArkimeNoResults from '../utils/NoResults.vue';
import ArkimeFieldTypeahead from '../utils/FieldTypeahead.vue';
import ArkimeVisualizations from '../visualizations/Visualizations.vue';
import TimelineGraph from '../visualizations/TimelineGraph.vue';
import TimeOfDayChart from '../visualizations/TimeOfDayChart.vue';
import { colorRange } from '../summary/widgets/chartColors';
import { requestTimezone } from '../summary/widgets/widgetData';
import ArkimeCollapsible from '../utils/CollapsibleWrapper.vue';
import PageLayout from '../utils/PageLayout.vue';
import ArkimePie from './Hierarchy.vue';
import ArkimeHeatmap from './Heatmap.vue';
import ArkimeConnectionsGraph from '../connections/ConnectionsGraph.vue';
import SpigraphConfigDropdown from './SpigraphConfigDropdown.vue';
import { graphFromSpigraphQuery, spigraphQueryFromGraph, graphFromWidget } from '../summary/dashboardConfig';
import { SPLIT_LENGTHS } from '../summary/widgets/viewModes';
import { commaString } from '@common/vueFilters.js';
import { resolveMessage } from '@common/resolveI18nMessage';
// import utils
import Utils from '../utils/utils';

let refreshInterval;
let respondedAt; // the time that the last data load successfully responded
let pendingPromise; // save a pending promise to be able to cancel it
let d3; // lazy loaded for the stacked timeline's colors

export default {
  name: 'Spigraph',
  components: {
    ArkimeError,
    ArkimeSearch,
    ArkimeLoading,
    ArkimeNoResults,
    ArkimeFieldTypeahead,
    ArkimeVisualizations,
    TimelineGraph,
    TimeOfDayChart,
    ArkimeCollapsible,
    PageLayout,
    ArkimePie,
    ArkimeHeatmap,
    ArkimeConnectionsGraph,
    SpigraphConfigDropdown
  },
  data: function () {
    return {
      error: '',
      loading: true,
      filtered: 0,
      graphData: undefined,
      mapData: undefined,
      refresh: 0,
      recordsTotal: 0,
      recordsFiltered: 0,
      items: [],
      showDropdown: false,
      fieldTypeahead: 'node',
      baseField: this.$route.query.exp || this.$route.query.field || this.$store.state.user.settings.spiGraph || 'node',
      sortBy: this.$route.query.sort || 'graph',
      spiGraphType: this.$route.query.spiGraphType || 'default',
      tableResults: [],
      fieldTypeaheadList: [],
      baseFieldObj: {},
      // Subnav option lists -- extracted from inline literals so the
      // template stays readable and v-for collapses the repeated
      // <option> blocks into one.
      maxElementsOptions: [5, 10, 15, 20, 30, 50, 100, 200, 500],
      refreshOptions: [0, 5, 10, 15, 30, 45, 60],
      snackbar: { show: false, text: '', color: 'success', link: '' },
      graphTypeOptions: ['default', 'timeofday', 'heatmap', 'bar', 'pie', 'tagcloud', 'table', 'treemap', 'sankey', 'matrix', 'connections', 'tree'],
      timelineModes: ['separate', 'stacked', 'percent'],
      stackedColors: [] // one per top value, for the stacked timeline
    };
  },
  computed: {
    user: function () {
      return this.$store.state.user;
    },
    timelineDataFilters: function () {
      const filters = this.user.settings.timelineDataFilters;
      return filters.map(i => FieldService.getField(i));
    },
    // the graph types in alphabetical order of their translated labels
    sortedGraphTypeOptions: function () {
      const label = (t) => this.$t(`spigraph.graphType-${t}`);
      return [...this.graphTypeOptions].sort((a, b) => label(a).localeCompare(label(b)));
    },
    graphType: function () {
      return this.$store.state.graphType;
    },
    // heatmap intensity choices: sessions + each timeline metric
    intensityOptions: function () {
      const opts = [{ value: 'sessionsHisto', label: this.$t('spigraph.sessions') }];
      for (const f of this.timelineDataFilters) {
        opts.push({ value: f.dbField + 'Histo', label: f.friendlyName });
      }
      return opts;
    },
    intensity: function () {
      // graphType may name a metric whose timeline filter was since removed
      if (this.graphType && this.intensityOptions.some(o => o.value === this.graphType)) {
        return this.graphType;
      }
      return 'sessionsHisto';
    },
    intensityLabel: function () {
      return this.intensityOptions.find(o => o.value === this.intensity)?.label || '';
    },
    timezone: function () {
      return this.user.settings.timezone || 'local';
    },
    // the types drawn from the per value histograms (timeline, time of day,
    // heatmap): they sort, refresh and count like the timeline does
    isHistoType: function () {
      return this.spiGraphType === 'default' || this.spiGraphType === 'heatmap' || this.spiGraphType === 'timeofday';
    },
    // timeline: one per value (separate), or one stacked timeline of the
    // values, as counts (stacked) or shares of each bucket (percent)
    timelineMode: function () {
      const mode = this.$route.query.timelineMode;
      return this.timelineModes.includes(mode) ? mode : 'separate';
    },
    // the stacked timeline: the page's graph for the bounds, one series per
    // value of its metric (the heatmap does the same fold)
    stackedGraph: function () {
      if (!this.graphData) { return null; }
      const key = this.graphType || 'sessionsHisto';
      const data = { xmin: this.graphData.xmin, xmax: this.graphData.xmax, interval: this.graphData.interval };
      this.items.forEach((item, i) => { data[`s${i}`] = item.graph?.[key] || []; });
      return data;
    },
    stackedSeries: function () {
      const colors = this.stackedColors;
      return this.items.map((item, i) => ({ label: String(item.name), key: `s${i}`, color: colors[i % (colors.length || 1)] || '#888' }));
    },
    query: function () {
      let sort = 'name';
      if (!this.$route.query.sort || this.$route.query.sort === 'graph') {
        sort = this.$route.query.graphType || this.$store.state.graphType || 'sessionsHisto';
      }
      return {
        sort,
        date: this.$store.state.timeRange,
        exp: this.$route.query.exp || this.$route.query.field || this.user.settings.spiGraph || 'node',
        size: this.$route.query.size || 20,
        startTime: this.$store.state.time.startTime,
        stopTime: this.$store.state.time.stopTime,
        bounding: this.$route.query.bounding || 'last',
        // a time of day grid is the viewer's fold of hour buckets into a week
        // in the user's timezone (interval=hourOfWeek)
        interval: this.spiGraphType === 'timeofday' ? 'hourOfWeek' : (this.$route.query.interval || 'auto'),
        timezone: this.spiGraphType === 'timeofday' ? requestTimezone(this.user.settings.timezone) : undefined,
        spanning: this.$route.query.spanning === 'true' ? true : undefined,
        view: this.$route.query.view || undefined,
        expression: this.$store.state.expression || undefined,
        cluster: this.$route.query.cluster || undefined
      };
    },
    fieldObj: function () {
      return FieldService.getField(this.query.exp);
    },
    showToolBars: function () {
      return this.$store.state.showToolBars;
    },
    fields: function () {
      return FieldService.addIpDstPortField(this.$store.state.fieldsArr);
    },
    // bar graph segment layout, kept in the url like the other options
    barMode: function () {
      return this.$route.query.barMode === 'grouped' ? 'grouped' : 'stacked';
    },
    barOrientation: function () {
      return this.$route.query.barOrientation === 'horizontal' ? 'horizontal' : 'vertical';
    },
    // pie layout: nested rings, or split into one pie per first field value
    pieMode: function () {
      return this.$route.query.pieMode === 'split' ? 'split' : 'nested';
    },
    // split pie: how many pies, on the dashboard's split length scale
    pieCount: function () {
      const n = parseInt(this.$route.query.pieCount, 10);
      return SPLIT_LENGTHS.includes(n) ? n : 5;
    },
    pieCounts: function () {
      return SPLIT_LENGTHS;
    },
    // pie: show what falls outside the top values as "Other"
    showOther: function () {
      return this.$route.query.other === 'true';
    },
    // table layout: value combinations (intersection), or each field's own (unique)
    tableMode: function () {
      return this.$route.query.tableMode === 'unique' ? 'unique' : 'intersection';
    },
    // the connections graph draws both the force layout and the tree
    isConnectionsType: function () {
      return this.spiGraphType === 'connections' || this.spiGraphType === 'tree';
    },
    // what the page shows as saved-graph data.
    // The field, size and type come from the page state, since the url only
    // carries them once the user has changed them from their defaults
    currentGraph: function () {
      const settings = this.$store.state.user?.settings || {};
      return graphFromSpigraphQuery({
        ...this.$route.query,
        spiGraphType: this.spiGraphType,
        exp: this.query.exp,
        size: this.query.size,
        // the heatmap and time of day draw the store's intensity and the
        // timeline the store's graph type, which the url may not carry
        graphType: (this.spiGraphType === 'heatmap' || this.spiGraphType === 'timeofday') ? this.intensity : this.spiGraphType === 'default' ? this.graphType : this.$route.query.graphType,
        // connections falls back to the user's own default fields
        srcField: this.$route.query.srcField || settings.connSrcField,
        dstField: this.$route.query.dstField || settings.connDstField
      }, {
        expression: this.$store.state.expression,
        fieldsArr: this.$store.state.fieldsArr
      });
    }
  },
  watch: {
    '$route.query' (newVal, oldVal) {
      if (newVal.size !== this.query.size) {
        this.query.size = newVal.size || 20;
      }
      if (newVal.sort !== this.query.sort) {
        this.query.sort = newVal.sort || 'graph';
      }
      if (newVal.spiGraphType !== this.spiGraphType) {
        this.spiGraphType = newVal.spiGraphType || 'default';
        if (this.isConnectionsType) {
          // connections owns its data; stop spigraph refresh
          this.refresh = 0;
          this.changeRefreshInterval(0);
        }
      }
    },
    // watch graph type and update sort
    graphType: function (newVal, oldVal) {
      if (newVal && this.sortBy === 'graph') {
        this.query.sort = newVal;
      }
    },
    // one color per value for the stacked timeline (d3 is lazy loaded)
    'items.length': {
      immediate: true,
      handler: async function (n) {
        if (!n) { this.stackedColors = []; return; }
        if (!d3) { d3 = await import('d3'); }
        this.stackedColors = colorRange(d3, 'rainbow', n);
      }
    }
  },
  mounted: function () {
    // a link (a saved graph, a dashboard widget's full page) names its metric
    if (this.$route.query.graphType) { this.$store.commit('updateGraphType', this.$route.query.graphType); }

    const field = FieldService.getField(this.query.exp, true);
    if (field) {
      this.fieldTypeahead = field.friendlyName;
      this.baseField = field.exp;
      this.query.exp = field.exp;
    }

    if (this.isConnectionsType) {
      // the connections child runs its own /api/connections load on mount
      this.loading = false;
    } else if (field) {
      this.cancelAndLoad(true);
      this.changeRefreshInterval(0);
    }
  },
  methods: {
    commaString,
    /* Search bar change: connections reloads via its child, others here. */
    onSearch: function () {
      if (this.isConnectionsType) {
        this.$refs.connectionsGraph?.cancelAndLoad(true);
        return;
      }
      this.cancelAndLoad(true);
    },
    /* exposed page functions ---------------------------------------------- */
    /**
     * Cancels the pending session query (if it's still pending) and runs a new
     * query if requested
     * @param {bool} runNewQuery  Whether to run a new spigraph query after
     *                            canceling the request
     */
    cancelAndLoad: function (runNewQuery) {
      const clientCancel = () => {
        if (pendingPromise) {
          pendingPromise.controller.abort('You canceled the search');
          pendingPromise = null;
        }

        if (!runNewQuery) {
          this.loading = false;
          if (!this.items.length) {
            // show a page error if there is no data on the page
            this.error = 'You canceled the search';
          }
          return;
        }

        this.loadData();
      };

      if (pendingPromise) {
        ConfigService.cancelEsTask(pendingPromise.cancelId).then((response) => {
          clientCancel();
        }).catch((error) => {
          clientCancel();
        });
      } else if (runNewQuery) {
        this.loadData();
      }
    },
    changeMaxElements: function (size) {
      this.query.size = size;
      this.$router.push({
        query: {
          ...this.$route.query,
          size
        }
      });
    },
    /* Shows a saved graph: its type, fields, options, search and view take
       over (having no search or view included). Its id stays in the url so
       later changes can be saved back to it */
    loadGraph: async function (saved) {
      // a shared graph is another user's data: normalize it before use
      const graph = graphFromWidget(saved.data);
      const q = spigraphQueryFromGraph(graph);
      if (!q) { return; }
      const field = FieldService.getField(q.exp, true);
      if (field) {
        this.fieldTypeahead = field.friendlyName;
        this.baseField = field.exp;
      }
      this.$store.commit('setExpression', graph.expression);
      // the ranking (a heatmap's intensity) falls back to the store's metric
      // when the url names none, so a sessions graph must not keep the last one
      this.$store.commit('updateGraphType', q.graphType || 'sessionsHisto');
      this.spiGraphType = q.spiGraphType;
      // the fetch reads its field, size and view from the url
      await this.$router.push({
        query: {
          ...this.$route.query,
          subFields: undefined,
          timelineMode: undefined,
          barMode: undefined,
          barOrientation: undefined,
          pieMode: undefined,
          pieCount: undefined,
          other: undefined,
          tableMode: undefined,
          srcField: undefined,
          dstField: undefined,
          graphType: undefined,
          connectionsLength: undefined,
          treeRoot: undefined,
          treeDepth: undefined,
          view: undefined,
          ...q,
          graphId: saved.id,
          expression: graph.expression || undefined
        }
      });
      if (q.spiGraphType !== 'connections' && q.spiGraphType !== 'tree') {
        this.cancelAndLoad(true);
      }
    },
    /* The graph just saved as new is now the loaded one (no reload needed) */
    onGraphSavedAs: function (id) {
      this.$router.replace({ query: { ...this.$route.query, graphId: id } });
    },
    onGraphMessage: function ({ msg, type, link }) {
      this.snackbar = { show: true, text: msg, color: type === 'danger' ? 'error' : 'success', link: link || '' };
    },
    changeTimelineMode: function (timelineMode) {
      this.$router.push({
        query: {
          ...this.$route.query,
          timelineMode: timelineMode === 'separate' ? undefined : timelineMode
        }
      });
    },
    // a drag on the stacked timeline narrows the search's time range
    onStackedTimeRange: function (times) {
      if (!times?.startTime || !times?.stopTime) { return; }
      this.$store.commit('setTimeRange', 0);
      this.$store.commit('setTime', times);
      this.$router.push({ query: { ...this.$route.query, date: undefined, startTime: times.startTime, stopTime: times.stopTime } });
    },
    changeBarMode: function (barMode) {
      this.$router.push({
        query: {
          ...this.$route.query,
          barMode
        }
      });
    },
    changeBarOrientation: function (orientation) {
      this.$router.push({
        query: {
          ...this.$route.query,
          barOrientation: orientation === 'horizontal' ? 'horizontal' : undefined
        }
      });
    },
    changePieMode: function (pieMode) {
      this.$router.push({
        query: {
          ...this.$route.query,
          pieMode: pieMode === 'split' ? 'split' : undefined,
          pieCount: pieMode === 'split' ? this.$route.query.pieCount : undefined
        }
      });
    },
    changePieCount: function (pieCount) {
      this.$router.push({
        query: {
          ...this.$route.query,
          pieCount
        }
      });
    },
    changeTableMode: function (tableMode) {
      this.$router.push({
        query: {
          ...this.$route.query,
          tableMode: tableMode === 'unique' ? 'unique' : undefined
        }
      });
    },
    changeShowOther: function (show) {
      this.$router.push({
        query: {
          ...this.$route.query,
          other: show ? 'true' : undefined
        }
      });
    },
    changeIntensity: function (value) {
      // recolors + re-sorts client-side (all metrics are already fetched); the
      // backend top-N ranking only switches to this metric on the next fetch
      this.$store.commit('updateGraphType', value);
      // a graphType in the url (a loaded graph, a link) would otherwise keep
      // ranking by the old metric and be saved as the graph's intensity
      if (this.$route.query.graphType) {
        this.$router.replace({ query: { ...this.$route.query, graphType: value === 'sessionsHisto' ? undefined : value } });
      }
    },
    changeSortBy: function (sortBy) {
      this.sortBy = sortBy;
      if (this.sortBy === 'graph') {
        this.query.sort = this.graphType;
      } else {
        this.query.sort = this.sortBy;
      }
      this.$router.push({
        query: {
          ...this.$route.query,
          sort: this.sortBy
        }
      });
    },
    changeRefreshInterval: function (interval) {
      this.refresh = interval;
      if (refreshInterval) { clearInterval(refreshInterval); }

      if (this.refresh && this.refresh > 0) {
        this.$store.commit('setIssueSearch', true);
        refreshInterval = setInterval(() => {
          if (respondedAt && Date.now() - respondedAt >= parseInt(this.refresh * 1000)) {
            this.$store.commit('setIssueSearch', true);
          }
        }, 500);
      }
    },
    changeSpiGraphType: function (spiGraphType) {
      const leavingConnections = this.isConnectionsType;
      this.spiGraphType = spiGraphType;
      if (this.spiGraphType === 'pie' || this.spiGraphType === 'bar' || this.spiGraphType === 'tagcloud' ||
        this.spiGraphType === 'table' || this.spiGraphType === 'treemap' || this.spiGraphType === 'sankey' || this.spiGraphType === 'matrix') {
        if (!this.$route.query.size) {
          this.query.size = 5; // set default size to 5
        }
        this.sortBy = 'graph'; // set default sort to count (graph)
        this.query.sort = this.graphType;
        this.refresh = 0;
        this.changeRefreshInterval(0);
      } else if (this.spiGraphType === 'heatmap' || this.spiGraphType === 'timeofday') {
        if (!this.graphType) { this.$store.commit('updateGraphType', 'sessionsHisto'); }
      } else if (this.isConnectionsType) {
        // connections owns its data; stop spigraph refresh
        this.refresh = 0;
        this.changeRefreshInterval(0);
      }
      this.$router.push({
        query: {
          ...this.$route.query,
          size: this.query.size,
          sort: this.sortBy,
          spiGraphType: this.spiGraphType,
          // the tree's own params go with it
          treeRoot: this.spiGraphType === 'tree' ? this.$route.query.treeRoot : undefined,
          treeDepth: this.spiGraphType === 'tree' ? this.$route.query.treeDepth : undefined
        }
      });
      if (leavingConnections && !this.isConnectionsType) {
        // load spigraph data skipped while connections was active
        this.cancelAndLoad(true);
      }
    },
    fetchedResults: function (tableResults, fieldTypeaheadList, baseFieldObj) {
      this.fieldTypeaheadList = fieldTypeaheadList;
      this.tableResults = tableResults;
      this.baseFieldObj = baseFieldObj;
    },
    exportCSV () {
      let fieldList = [this.baseFieldObj.friendlyName];
      fieldList = fieldList.concat(this.fieldTypeaheadList.map(f => f.friendlyName));

      let csv = '';
      if (this.tableResults.length) {
        const keys = ['name', 'size'];

        // add the headers
        fieldList.forEach((field) => {
          keys.forEach((key) => {
            csv += `${field} ${key === 'name' ? 'value' : 'count'},`;
          });
        });

        csv = csv.slice(0, -1); // remove trailing comma
        csv += '\n'; // newline for next row

        // add the values
        this.tableResults.forEach((row) => {
          if (row.parents) {
            csv += row.parents.map(p => {
              return keys.map(key => p[key]).join(',');
            }).join(',');
            if (row.parents.length) {
              csv += ',';
            }
          }
          csv += keys.map(key => row[key]).join(',') + '\n';
        });
      }

      const blob = new Blob([csv], { type: 'text/csv' });
      const url = window.URL.createObjectURL(blob);
      const a = document.createElement('a');
      a.href = url;
      a.download = 'spigraph.csv';
      a.click();
      window.URL.revokeObjectURL(url);
    },
    /* event functions ----------------------------------------------------- */
    changeField: function (field) {
      this.fieldTypeahead = field.friendlyName;
      this.query.exp = field.exp;
      this.baseField = field.exp;
      this.$router.push({
        query: {
          ...this.$route.query,
          exp: this.query.exp
        }
      });
      this.cancelAndLoad(true);
    },
    toggleLoad: function (loading) {
      this.loading = loading;
    },
    toggleError: function (message) {
      this.error = message;
    },
    /* helper functions ---------------------------------------------------- */
    async loadData () {
      respondedAt = undefined;

      // connections loads via its own child
      if (this.isConnectionsType) {
        this.loading = false;
        return;
      }

      if (!Utils.checkClusterSelection(this.query.cluster, this.$store.state.esCluster.availableCluster.active, this).valid) {
        this.items = [];
        pendingPromise = null;
        this.recordsTotal = 0;
        this.recordsFiltered = 0;
        this.mapData = undefined;
        this.graphData = undefined;
        return;
      }

      this.loading = true;
      this.error = false;

      // set whether map is open on the sessions page
      if (localStorage.getItem('spigraph-open-map') === 'true') {
        this.query.map = true;
      }

      // create unique cancel id to make cancel req for corresponding es task
      const cancelId = Utils.createRandomString();
      this.query.cancelId = cancelId;

      try {
        const { controller, fetcher } = await SpigraphService.get(this.query);
        pendingPromise = { controller, cancelId };

        const response = await fetcher;
        if (response.error) {
          throw new Error(response.error);
        }

        pendingPromise = null;
        respondedAt = Date.now();
        this.error = '';
        this.loading = false;
        this.items = response.items;
        this.mapData = response.map;
        this.graphData = response.graph;
        this.recordsTotal = response.recordsTotal;
        this.recordsFiltered = response.recordsFiltered;
      } catch (error) {
        pendingPromise = null;
        respondedAt = undefined;
        this.loading = false;
        this.error = resolveMessage(error, this.$t);
      }
    }
  },
  beforeUnmount () {
    if (pendingPromise) {
      pendingPromise.controller.abort('Closing the SPIGraph page canceled the search');
      pendingPromise = null;
    }
  }
};
</script>

<style scoped>
.spigraph-page .spigraph-form {
  z-index: 4;
  background-color: rgb(var(--v-theme-quaternary-lightest));
}
.spigraph-page .spigraph-form .records-display  {
  font-size: 0.85rem;
  font-weight: 400;
}

/* "add another field" on its own row: full-width forces a flex-wrap break */
.spigraph-page .spigraph-subfield-anchor {
  flex: 1 0 100%;
}

/* field typeahead */
.spigraph-page .spigraph-form .field-typeahead {
  max-height: 300px;
  overflow-y: auto;
}

/* spigraph content styles ------------------- */
.spigraph-page .spigraph-content {
  padding-top: 10px;
}

.spigraph-page .spi-graph-item .spi-bucket sup {
  margin-left: -8px;
}

/* main graph/map */
.spigraph-page .spigraph-content .well {
  -webkit-box-shadow: 0 4px 16px -2px black;
     -moz-box-shadow: 0 4px 16px -2px black;
          box-shadow: 0 4px 16px -2px black;
}

/* stripes */
.spigraph-page .spigraph-content .spi-graph-item:nth-child(odd) {
  background-color: rgb(var(--v-theme-quaternary-lightest));
}

/* the stacked timeline: a legend strip, then a plot that takes the height */
.spigraph-page .timeline-stacked {
  display: flex;
  flex-direction: column;
  height: 460px;
  padding: 0 0.5rem;
}
.spigraph-page .timeline-stacked__legend {
  display: flex;
  flex-wrap: wrap;
  gap: 0.25rem 1rem;
  font-size: 0.85rem;
  margin-bottom: 0.25rem;
}
.spigraph-page .timeline-stacked__item {
  display: inline-flex;
  align-items: center;
}
.spigraph-page .timeline-stacked__swatch {
  display: inline-block;
  width: 10px;
  height: 10px;
  border-radius: 2px;
  margin-right: 0.35rem;
}
.spigraph-page .timeline-stacked__plot {
  flex: 1;
  min-height: 0;
}

/* time of day: the search's week, then a grid of small weeks per value */
.spigraph-page .timeofday-primary {
  height: 260px;
  padding: 0 0.5rem;
}
.spigraph-page .timeofday-grid {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(300px, 1fr));
  gap: 0.5rem;
  padding: 0.5rem;
}
.spigraph-page .timeofday-cell {
  display: flex;
  flex-direction: column;
  padding: 0.25rem;
  background-color: rgb(var(--v-theme-quaternary-lightest));
  border-radius: 4px;
}
.spigraph-page .timeofday-chart {
  height: 170px;
}
</style>
