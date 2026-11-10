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

              <!-- every setting of the graph, the ones without a control
                   here included -->
              <v-btn
                variant="outlined"
                size="small"
                density="comfortable"
                class="ms-1 px-2"
                min-width="0"
                :disabled="!currentGraph"
                :aria-label="$t('spigraph.settings.title')"
                @click="openSettings">
                <v-icon icon="mdi-cog" />
                <v-tooltip activator="parent">
                  {{ $t('spigraph.settings.tip') }}
                </v-tooltip>
              </v-btn>

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

              <!-- timeline layout (timeline and area): one timeline per value,
                   or the values stacked in one (as counts, or as shares); an
                   area's values can also be overlaid -->
              <div
                class="arkime-input-group"
                v-if="spiGraphType === 'default' || spiGraphType === 'area'">
                <span class="arkime-input-label">
                  {{ $t('spigraph.timelineMode') }}:
                </span>
                <select
                  class="arkime-input-control"
                  :value="timelineMode"
                  @change="changeTimelineMode($event.target.value)">
                  <option
                    v-for="m in layoutModes"
                    :key="m"
                    :value="m">
                    {{ $t(`spigraph.timelineMode-${m}`) }}
                  </option>
                </select>
              </div> <!-- /timeline layout -->

              <!-- metric: what the values are ranked, sized or colored by
                   (the graphs of several fields only count sessions) -->
              <div
                class="arkime-input-group"
                v-if="hasMetricSelect">
                <span class="arkime-input-label">
                  {{ $t('sessions.summary.widget.metric') }}:
                </span>
                <select
                  class="arkime-input-control"
                  :value="metricSelectValue"
                  :disabled="sessionsOnly"
                  @change="changeMetric($event.target.value)">
                  <option
                    v-for="opt in intensityOptions"
                    :key="opt.value"
                    :value="opt.value">
                    {{ opt.label }}
                  </option>
                </select>
              </div> <!-- /metric -->

              <!-- combo and scatter: a timeline metric per slot (the first
                   ranks the values, like the heatmap's intensity) -->
              <template v-if="isMetricsChartType">
                <div
                  v-for="(slot, i) in metricSlotDefs"
                  :key="slot.key"
                  class="arkime-input-group">
                  <span class="arkime-input-label">
                    {{ $t(`sessions.summary.widget.metricSlot-${slot.key}`) }}:
                  </span>
                  <select
                    class="arkime-input-control"
                    :value="slotKeys[i]"
                    @change="changeSlotMetric(i, $event.target.value)">
                    <option
                      v-if="slot.optional"
                      value="">
                      {{ $t('sessions.summary.widget.metricSlotNone') }}
                    </option>
                    <option
                      v-for="opt in intensityOptions"
                      :key="opt.value"
                      :value="opt.value">
                      {{ opt.label }}
                    </option>
                  </select>
                </div>
              </template> <!-- /combo and scatter metrics -->

              <!-- page info -->
              <div
                class="records-display align-self-center"
                v-if="isHistoType && !widgetGraph">
                <strong
                  class="text-theme-accent"
                  v-if="!error && recordsFiltered !== undefined">
                  {{ $t('common.showingAllTip', { count: commaString(recordsFiltered), total: commaString(recordsTotal) }) }}
                </strong>
              </div> <!-- /page info -->

              <!-- export button-->
              <v-btn
                v-if="!isHistoType && spiGraphType !== 'sankey' && !isConnectionsType && !widgetGraph"
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
                v-if="['bar', 'pie', 'waffle', 'table', 'treemap', 'sankey', 'matrix', 'mosaic'].includes(spiGraphType)"
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
        :metric-override="percentileKey || ownMetric"
        @fetch-map-data="cancelAndLoad(true)"
        @spanning-change="cancelAndLoad(true)" />
    </div> <!-- /main visualization -->

    <div
      class="spigraph-content"
      v-if="!isConnectionsType">
      <!-- the graphs the dashboard's widget draws (a metric, an aggregation,
           an order...) -->
      <spigraph-widget-view
        v-if="widgetGraph"
        :graph="widgetGraph"
        :reload-nonce="widgetNonce" />

      <!-- heatmap graph type -->
      <div v-else-if="spiGraphType === 'heatmap'">
        <arkime-heatmap
          v-if="items && items.length && fieldObj && graphData"
          :items="items"
          :graph="graphData"
          :field-obj="fieldObj"
          :metric="shownMetric"
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
        v-else-if="(spiGraphType === 'default' && timelineMode !== 'separate') || spiGraphType === 'area'"
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
            :graph-type="shownMetric"
            :series-defs="stackedSeries"
            :series-type="spiGraphType === 'area' ? 'area' : (timelineSeriesType === 'lines' || timelineMode === 'overlaid' || percentileKey) ? 'lines' : 'bars'"
            :y-scale="timelineMode === 'percent' && !percentileKey ? 'percent' : timelineLog ? 'log' : 'linear'"
            :thresholds="graphThresholds"
            :stacked="timelineMode !== 'overlaid' && !percentileKey"
            fit-height
            :timeline-data-filters="timelineDataFilters"
            :timezone="timezone"
            @update-time-range="onStackedTimeRange" />
        </div>
      </div> <!-- /stacked timeline -->

      <!-- combo and scatter: the top values by several timeline metrics -->
      <spigraph-metrics-chart
        v-else-if="isMetricsChartType"
        :type="spiGraphType"
        :items="fieldObj ? items : []"
        :field-obj="fieldObj || {}"
        :metrics="slotDefs"
        :log="scatterLog" /> <!-- /combo and scatter -->

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
          :split-length="barSplitLength"
          :thresholds="graphThresholds"
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
                  <sup>({{ metricText(item[shownMetric]) }})</sup>
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
                  :timeline-data-filters="timelineDataFilters"
                  :metric-override="percentileKey || ownMetric" />
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
        v-if="!error && !loading && !items.length && !widgetGraph"
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

    <!-- the graph's settings: the dashboard's widget editor, so the page can
         set whatever a widget can -->
    <summary-widget-edit-modal
      :show="showSettings"
      :widget="currentGraph"
      graph-only
      page-graph
      @close="showSettings = false"
      @save="applySettings">
      <template #extra>
        <div
          v-if="isHistoType"
          class="d-flex flex-wrap gap-3 mb-3">
          <v-select
            v-if="!widgetGraph"
            v-model="settingsDraft.sortBy"
            :items="sortByItems"
            :label="$t('spigraph.sortBy')"
            density="compact"
            variant="outlined"
            hide-details
            style="min-width: 160px" />
          <v-select
            v-model="settingsDraft.refresh"
            :items="refreshItems"
            :label="$t('spigraph.refreshEvery')"
            density="compact"
            variant="outlined"
            hide-details
            style="min-width: 160px" />
        </div>
      </template>
    </summary-widget-edit-modal>

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
import { metricSeries } from '../visualizations/metrics.js';
import ArkimeCollapsible from '../utils/CollapsibleWrapper.vue';
import PageLayout from '../utils/PageLayout.vue';
import ArkimePie from './Hierarchy.vue';
import ArkimeHeatmap from './Heatmap.vue';
import ArkimeConnectionsGraph from '../connections/ConnectionsGraph.vue';
import SpigraphConfigDropdown from './SpigraphConfigDropdown.vue';
import SpigraphMetricsChart from './SpigraphMetricsChart.vue';
import SpigraphWidgetView from './SpigraphWidgetView.vue';
import SummaryWidgetEditModal from '../summary/SummaryWidgetEditModal.vue';
import { graphFromSpigraphQuery, spigraphQueryFromGraph, graphFromWidget, graphDrawnAsWidget, graphIsHierarchyFed } from '../summary/dashboardConfig';
import { SPLIT_LENGTHS, METRIC_SLOTS, PERCENTILE_AGGS, slotMetrics, fieldCountLimits } from '../summary/widgets/viewModes';
import { widgetThresholds } from '../summary/widgets/widgetData';
import { commaString, humanReadableBytes } from '@common/vueFilters.js';
import { resolveMessage } from '@common/resolveI18nMessage';
// import utils
import Utils from '../utils/utils';

let refreshInterval;
let respondedAt; // the time that the last data load successfully responded
let pendingPromise; // save a pending promise to be able to cancel it
let d3; // lazy loaded for the stacked timeline's colors

// the url params a graph sets: showing another graph clears the ones it leaves out
const GRAPH_PARAMS = ['subFields', 'timelineMode', 'seriesType', 'barMode', 'barOrientation', 'splitLength', 'pieMode', 'pieCount', 'other', 'tableMode',
  'lineMetric', 'yMetric', 'sizeMetric', 'yScale', 'srcField', 'dstField', 'graphType', 'metricAgg', 'metrics', 'sortMetric', 'order', 'orderBy',
  'bucketInterval', 'warn', 'crit', 'connectionsLength', 'treeRoot', 'treeDepth', 'view'];

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
    SpigraphConfigDropdown,
    SpigraphMetricsChart,
    SpigraphWidgetView,
    SummaryWidgetEditModal
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
      restoringGraph: false, // a saved graph is being shown: it loads once its url is in place
      tableResults: [],
      fieldTypeaheadList: [],
      baseFieldObj: {},
      // Subnav option lists -- extracted from inline literals so the
      // template stays readable and v-for collapses the repeated
      // <option> blocks into one.
      maxElementsOptions: [5, 10, 15, 20, 30, 50, 100, 200, 500],
      refreshOptions: [0, 5, 10, 15, 30, 45, 60],
      snackbar: { show: false, text: '', color: 'success', link: '' },
      graphTypeOptions: ['default', 'area', 'timeofday', 'heatmap', 'bar', 'combo', 'scatter', 'pie', 'tagcloud', 'waffle', 'table', 'treemap', 'sankey', 'matrix', 'mosaic', 'connections', 'tree'],
      timelineModes: ['separate', 'stacked', 'percent', 'overlaid'],
      showSettings: false,
      settingsDraft: { sortBy: 'graph', refresh: 0 }, // the dialog's page settings, until applied
      widgetNonce: 0, // bumped by a search, so the widget drawn as the page searches again
      areaModes: ['stacked', 'percent', 'overlaid'],
      // the types that can draw a percentile of their metric (?metricAgg=p95)
      percentileTypes: ['default', 'heatmap'],
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
    // the page's metric: one of the timeline's is the store's (shared with
    // the Sessions timeline), any other a graph names stays in the url
    graphType: function () {
      const named = this.$route.query.graphType;
      return this.namedMetrics.includes(named) && !this.isTimelineMetric(named) ? named : this.$store.state.graphType;
    },
    ownMetric: function () {
      return this.graphType && !this.isTimelineMetric(this.graphType) ? this.graphType : '';
    },
    // what a sort by the graph ranks the top values by: the metric drawn,
    // which is the session count for all but the timeline types
    rankKey: function () {
      if (!this.isHistoType) { return 'sessionsHisto'; }
      return this.percentileKey || this.$route.query.graphType || this.graphType || 'sessionsHisto';
    },
    // heatmap intensity choices: sessions + each timeline metric
    intensityOptions: function () {
      const opts = [{ value: 'sessionsHisto', label: this.$t('spigraph.sessions') }];
      for (const f of this.timelineDataFilters) {
        opts.push({ value: f.dbField + 'Histo', label: f.friendlyName });
      }
      // a metric the graph names (a saved graph, a widget's link, the
      // settings) that isn't one of the user's timeline metrics
      for (const key of this.namedMetrics) {
        const field = opts.some(o => o.value === key) ? undefined : FieldService.getField(key.slice(0, -5));
        if (field?.dbField) { opts.push({ value: key, label: field.friendlyName }); }
      }
      return opts;
    },
    // the <dbField>Histo metrics in the url
    namedMetrics: function () {
      const q = this.$route.query;
      return [q.graphType, q.lineMetric, q.yMetric, q.sizeMetric].filter(k => typeof k === 'string' && /^[^:]+Histo$/.test(k) && k !== 'sessionsHisto');
    },
    // the page's metric when the server only sums it for the asking (it sums
    // the user's timeline metrics for every search)
    extraMetric: function () {
      const key = (this.spiGraphType === 'heatmap' || this.spiGraphType === 'timeofday') ? this.intensity : this.baseMetric;
      if (!this.isHistoType || this.isTimelineMetric(key)) { return undefined; }
      return FieldService.getField(key.slice(0, -5))?.exp;
    },
    intensity: function () {
      // graphType may name a metric whose timeline filter was since removed
      if (this.graphType && this.intensityOptions.some(o => o.value === this.graphType)) {
        return this.graphType;
      }
      return 'sessionsHisto';
    },
    // the timeline's metric (the heatmap's intensity), a <dbField>Histo key
    baseMetric: function () {
      return this.spiGraphType === 'heatmap' ? this.intensity : (this.graphType || 'sessionsHisto');
    },
    // the percentile drawn of it in place of its sum, a multi viewer has none
    percentileAgg: function () {
      const agg = this.$route.query.metricAgg;
      return !this.$constants.MULTIVIEWER && this.percentileTypes.includes(this.spiGraphType) && PERCENTILE_AGGS.includes(agg) ? agg : undefined;
    },
    // its series, <dbField>:p<N>Histo ('' for a sum, or the session count)
    percentileKey: function () {
      return this.percentileAgg && this.baseMetric !== 'sessionsHisto' ? `${this.baseMetric.slice(0, -5)}:${this.percentileAgg}Histo` : '';
    },
    shownMetric: function () {
      return this.percentileKey || this.baseMetric;
    },
    intensityLabel: function () {
      return this.intensityOptions.find(o => o.value === this.intensity)?.label || '';
    },
    timezone: function () {
      return this.user.settings.timezone || 'local';
    },
    // the types drawn from the per value histograms (timeline, time of day,
    // heatmap): they sort, refresh and count like the timeline does
    // (the area, combo and scatter draw the same items)
    isHistoType: function () {
      return ['default', 'area', 'heatmap', 'timeofday', 'combo', 'scatter'].includes(this.spiGraphType);
    },
    // the layouts of the type's timeline: separate, stacked or percent, and
    // for an area stacked (its default), percent or overlaid
    layoutModes: function () {
      return this.spiGraphType === 'area' ? this.areaModes : this.timelineModes;
    },
    // timeline: one per value (separate), or one stacked timeline of the
    // values, as counts (stacked) or shares of each bucket (percent)
    timelineMode: function () {
      const mode = this.$route.query.timelineMode;
      return this.layoutModes.includes(mode) ? mode : this.layoutModes[0];
    },
    // combo and scatter: each slot's timeline metric, a <dbField>Histo key;
    // the first is the page's graph type (it ranks the values), the others
    // are url params of their own, a combo's line and a scatter's y defaulting
    // to bytes (or the first metric there is) and its bubble size to none
    isMetricsChartType: function () {
      return this.spiGraphType === 'combo' || this.spiGraphType === 'scatter';
    },
    metricSlotDefs: function () {
      return METRIC_SLOTS[this.spiGraphType] || [];
    },
    slotParams: function () {
      return this.spiGraphType === 'combo' ? ['lineMetric'] : ['yMetric', 'sizeMetric'];
    },
    slotKeys: function () {
      if (!this.isMetricsChartType) { return []; }
      const valid = (k) => this.intensityOptions.some(o => o.value === k);
      const fallback = valid('network.bytesHisto') ? 'network.bytesHisto' : (this.intensityOptions[1] || this.intensityOptions[0]).value;
      return this.metricSlotDefs.map((slot, i) => {
        if (i === 0) { return this.intensity; }
        const k = this.$route.query[this.slotParams[i - 1]];
        if (slot.optional) { return valid(k) ? k : ''; }
        return valid(k) ? k : fallback;
      });
    },
    // [{ key, label, format }] per slot that is set
    slotDefs: function () {
      return this.slotKeys.map((key) => {
        if (!key) { return undefined; }
        const label = this.intensityOptions.find(o => o.value === key)?.label || key;
        const format = /bytes/i.test(key) ? (v) => humanReadableBytes(v || 0) : (v) => commaString(v || 0);
        return { key, label, format };
      });
    },
    scatterLog: function () {
      return this.spiGraphType === 'scatter' && this.$route.query.yScale === 'log';
    },
    // the stacked timeline: the page's graph for the bounds, one series per
    // value of its metric (the heatmap does the same fold)
    stackedGraph: function () {
      if (!this.graphData) { return null; }
      const key = this.percentileKey || this.graphType || 'sessionsHisto';
      const data = { xmin: this.graphData.xmin, xmax: this.graphData.xmax, interval: this.graphData.interval };
      // packets and bytes are a src and a dst series, stacked as their sum
      this.items.forEach((item, i) => { data[`s${i}`] = metricSeries(item.graph, key); });
      return data;
    },
    stackedSeries: function () {
      const colors = this.stackedColors;
      return this.items.map((item, i) => ({ label: String(item.name), key: `s${i}`, color: colors[i % (colors.length || 1)] || '#888' }));
    },
    query: function () {
      let sort = 'name';
      if (!this.$route.query.sort || this.$route.query.sort === 'graph') {
        sort = this.rankKey;
      }
      // a percentile is asked of the metric's field
      const percentileField = this.percentileKey ? FieldService.getField(this.baseMetric.slice(0, -5))?.exp : undefined;
      return {
        sort,
        metric: percentileField || this.extraMetric,
        metricAgg: percentileField ? this.percentileAgg : undefined,
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
    // pie: show what falls outside the top values as "Other"
    showOther: function () {
      return this.$route.query.other === 'true';
    },
    // table layout: value combinations (intersection), or each field's own (unique)
    tableMode: function () {
      return this.$route.query.tableMode === 'unique' ? 'unique' : 'intersection';
    },
    // bar: how many of the second field's values each bar is split by
    barSplitLength: function () {
      const n = parseInt(this.$route.query.splitLength, 10);
      return SPLIT_LENGTHS.includes(n) ? n : 5;
    },
    // stacked timeline: bars or lines (the graph's own, else the timeline's
    // last pick) and a log y axis
    timelineSeriesType: function () {
      const type = this.$route.query.seriesType;
      return type === 'lines' || type === 'bars' ? type : (this.$store.state.seriesType === 'lines' ? 'lines' : 'bars');
    },
    timelineLog: function () {
      return this.$route.query.yScale === 'log';
    },
    // the warning and critical levels a bar or stacked timeline draws lines at
    graphThresholds: function () {
      return widgetThresholds(this.currentGraph);
    },
    // the graph, when the dashboard's widget draws it: its settings are the
    // summary endpoint's, or a combo's or scatter's metric isn't one of the
    // timeline metrics the page's own chart has sums of
    widgetGraph: function () {
      return this.widgetDraws(this.currentGraph) ? this.currentGraph : null;
    },
    // the types with one metric the toolbar picks (a combo's and scatter's
    // have their own, a table's are in the settings)
    hasMetricSelect: function () {
      return ['default', 'area', 'timeofday', 'heatmap', 'bar', 'pie', 'tagcloud', 'waffle', 'treemap'].includes(this.spiGraphType);
    },
    // the types sized by the url's metric (the others draw the store's)
    isSizedType: function () {
      return ['bar', 'pie', 'tagcloud', 'waffle', 'treemap'].includes(this.spiGraphType);
    },
    // a split or nested pie, a split waffle and a nested treemap only count sessions
    sessionsOnly: function () {
      return this.isSizedType && !!this.currentGraph && graphIsHierarchyFed(this.currentGraph);
    },
    metricSelectValue: function () {
      if (!this.isSizedType) { return this.spiGraphType === 'heatmap' || this.spiGraphType === 'timeofday' ? this.intensity : this.baseMetric; }
      const key = this.$route.query.graphType;
      return !this.sessionsOnly && this.intensityOptions.some(o => o.value === key) ? key : 'sessionsHisto';
    },
    sortByItems: function () {
      return ['name', 'graph'].map(value => ({ title: this.$t(`spigraph.sortBy-${value}`), value }));
    },
    refreshItems: function () {
      return this.refreshOptions.map(value => ({ title: `${value} ${this.$t('common.seconds')}`, value }));
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
        graphType: (this.spiGraphType === 'heatmap' || this.spiGraphType === 'timeofday' || this.isMetricsChartType) ? this.intensity : (this.spiGraphType === 'default' || this.spiGraphType === 'area') ? this.graphType : this.$route.query.graphType,
        // a combo's or scatter's other metrics as the page draws them
        ...(this.spiGraphType === 'combo' ? { lineMetric: this.slotKeys[1] } : {}),
        ...(this.spiGraphType === 'scatter' ? { yMetric: this.slotKeys[1], sizeMetric: this.slotKeys[2] || undefined } : {}),
        // a timeline draws the store's bars or lines unless the url names one
        seriesType: this.timelineSeriesType,
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
      // sort=graph (or none) ranks by the metric drawn, as the query does;
      // the server has no 'graph' sort
      const sort = (newVal.sort && newVal.sort !== 'graph') ? newVal.sort : this.rankKey;
      if (sort !== this.query.sort) {
        this.query.sort = sort;
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
    // the data has to be loaded again when leaving connections, which owns its
    // data, and when going to or from time of day, whose grid asks for hour
    // buckets folded into a week (interval=hourOfWeek) rather than the timeline
    spiGraphType: function (newVal, oldVal) {
      const isConnections = t => t === 'connections' || t === 'tree';
      if (isConnections(newVal) || this.restoringGraph) { return; }
      if (isConnections(oldVal) || (newVal === 'timeofday') !== (oldVal === 'timeofday')) {
        this.cancelAndLoad(true);
      }
    },
    // back to one of the page's own charts: it needs the page's data
    widgetGraph: function (newVal, oldVal) {
      if (!newVal && oldVal && !this.isConnectionsType && !this.restoringGraph) { this.cancelAndLoad(true); }
    },
    // the server only sums a metric outside the timeline's for the asking
    extraMetric: function (newVal) {
      if (newVal && !this.restoringGraph && !this.widgetGraph) { this.cancelAndLoad(true); }
    },
    // watch graph type and update sort
    graphType: function (newVal, oldVal) {
      if (newVal && this.sortBy === 'graph') {
        this.query.sort = this.rankKey;
      }
    },
    // the server only has the percentile it was asked for: a new one (another
    // aggregation or metric, a type that draws one, back/forward) is a new
    // fetch; a saved graph loads once its url is in place
    percentileKey: function (newVal, oldVal) {
      if (newVal !== oldVal && !this.restoringGraph && this.percentileTypes.includes(this.spiGraphType)) { this.cancelAndLoad(true); }
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
    if (this.$route.query.graphType) { this.setTimelineMetric(this.$route.query.graphType); }

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
      // a graph drawn by its dashboard widget searches again with the page;
      // any other load comes with a change of the graph, which it fetches for
      if (this.widgetGraph) { this.widgetNonce++; }
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
    loadGraph: function (saved) {
      // a shared graph is another user's data: normalize it before use
      return this.showGraph(graphFromWidget(saved.data), { graphId: saved.id });
    },
    /* Shows a graph: its type, fields, options, search and view take over the
       url's. Given the graph the page shows now (the settings dialog's Apply),
       only what differs from it is written, so what the url holds that a graph
       can't (separate timelines, more than 10 series) stays as it is */
    showGraph: async function (graph, extra = {}, from = null) {
      const q = spigraphQueryFromGraph(graph);
      if (!q) { return false; }
      const was = from ? (spigraphQueryFromGraph(from) || {}) : null;
      const query = { ...this.$route.query };
      for (const p of new Set([...GRAPH_PARAMS, ...Object.keys(q), ...Object.keys(was || {})])) {
        if (!was || q[p] !== was[p]) { query[p] = q[p]; }
      }
      if (!was) { query.expression = graph.expression || undefined; }
      const field = FieldService.getField(q.exp, true);
      if (field) {
        this.fieldTypeahead = field.friendlyName;
        this.baseField = field.exp;
      }
      this.$store.commit('setExpression', graph.expression);
      // the ranking (a heatmap's intensity) falls back to the store's metric
      // when the url names none, so a sessions graph must not keep the last one
      if (!was || q.graphType !== was.graphType) { this.setTimelineMetric(q.graphType || 'sessionsHisto'); }
      // the type watcher would load against the url before the push below
      // puts the graph's field and size in it: this load is the one below
      this.restoringGraph = true;
      try {
        this.spiGraphType = q.spiGraphType;
        // the fetch reads its field, size and view from the url
        await this.$router.push({ query: { ...query, ...extra } });
      } finally {
        this.restoringGraph = false;
      }
      // only the timeline types refresh, and only they have its control
      if (!this.isHistoType && this.refresh) { this.changeRefreshInterval(0); }
      if (q.spiGraphType !== 'connections' && q.spiGraphType !== 'tree') {
        this.cancelAndLoad(true);
      }
      return true;
    },
    openSettings: function () {
      this.settingsDraft = { sortBy: this.sortBy, refresh: this.refresh };
      this.showSettings = true;
    },
    /* The settings dialog's Apply: the graph it made takes over the url, under
       the page's own search and view */
    applySettings: async function (widget) {
      this.showSettings = false;
      const page = this.currentGraph;
      const edited = { ...widget, expression: this.$store.state.expression || '', view: this.$route.query.view || '' };
      // the dialog keeps a type's first fields; SPI Graph draws any number,
      // so the page's others stay when the dialog left its own alone
      const max = fieldCountLimits(edited.viewMode)[1];
      if (page && page.viewMode === edited.viewMode && page.fields.length > max && page.fields.slice(0, max).join() === (edited.fields || []).join()) {
        edited.fields = page.fields;
      }
      const { refresh } = this.settingsDraft;
      let { sortBy } = this.settingsDraft;
      // the widget sorts a combo or scatter it draws by its own Order by, so
      // the page's sort by name becomes that and the page's is cleared
      if (this.widgetDraws(graphFromWidget(edited)) && (edited.viewMode === 'combo' || edited.viewMode === 'scatter')) {
        if (sortBy === 'name' && edited.orderBy !== 'key') { edited.orderBy = 'key'; edited.order = 'asc'; }
        sortBy = 'graph';
      }
      const graph = graphFromWidget(edited);
      // checked first, so a graph that can't be drawn changes nothing
      if (!spigraphQueryFromGraph(graph)) {
        this.snackbar = { show: true, text: this.$t('spigraph.settings.cantDraw'), color: 'error', link: '' };
        return;
      }
      const extra = {};
      if (sortBy !== this.sortBy) {
        this.sortBy = sortBy;
        extra.sort = sortBy;
      }
      await this.showGraph(graph, extra, page);
      if (this.isHistoType && refresh !== this.refresh) { this.changeRefreshInterval(refresh); }
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
    // a combo's or scatter's metric: the first is the graph type (it ranks the
    // values), the others url params of their own
    changeSlotMetric: async function (i, key) {
      if (i === 0) {
        // the ranking changes, so the top values are fetched again once the
        // url (which the fetch's sort reads first) names the new metric
        if (!this.setTimelineMetric(key) || this.$route.query.graphType) {
          await this.$router.replace({ query: { ...this.$route.query, graphType: key === 'sessionsHisto' ? undefined : key } });
        }
        this.cancelAndLoad(true);
        return;
      }
      this.$router.push({ query: { ...this.$route.query, [this.slotParams[i - 1]]: key || undefined } });
    },
    changeBarMode: function (barMode) {
      this.$router.push({
        query: {
          ...this.$route.query,
          barMode
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
    changeTableMode: function (tableMode) {
      this.$router.push({
        query: {
          ...this.$route.query,
          tableMode: tableMode === 'unique' ? 'unique' : undefined
        }
      });
    },
    widgetDraws: function (graph) {
      if (!graph) { return false; }
      if (graphDrawnAsWidget(graph)) { return true; }
      const summed = (exp) => !exp || exp === 'sessions' || this.timelineDataFilters.some(f => f.exp === exp);
      return (graph.viewMode === 'combo' || graph.viewMode === 'scatter') && !slotMetrics(graph).every(summed);
    },
    isTimelineMetric: function (key) {
      return key === 'sessionsHisto' || this.timelineDataFilters.some(f => f && f.dbField + 'Histo' === key);
    },
    /* Makes a metric the timeline's (the store's, which the Sessions timeline
       shares) when it is one of the timeline's own; false when it isn't */
    setTimelineMetric: function (key) {
      if (!this.isTimelineMetric(key)) { return false; }
      this.$store.commit('updateGraphType', key);
      return true;
    },
    changeIntensity: function (value) {
      // recolors + re-sorts client-side (all metrics are already fetched); the
      // backend top-N ranking only switches to this metric on the next fetch
      // a graphType in the url (a loaded graph, a link) would otherwise keep
      // ranking by the old metric and be saved as the graph's intensity
      if (!this.setTimelineMetric(value) || this.$route.query.graphType) {
        this.$router.replace({ query: { ...this.$route.query, graphType: value === 'sessionsHisto' ? undefined : value } });
      }
    },
    // the toolbar's metric: a bar's, pie's, tag cloud's, waffle's or treemap's
    // is the url's (the dashboard's widget then draws it), the others' the
    // timeline's
    changeMetric: function (value) {
      if (!this.isSizedType) { this.changeIntensity(value); return; }
      this.setTimelineMetric(value);
      this.$router.push({ query: { ...this.$route.query, graphType: value === 'sessionsHisto' ? undefined : value, metricAgg: value === 'sessionsHisto' ? undefined : this.$route.query.metricAgg } });
    },
    // a value's total: a percentile is rarely whole
    metricText: function (v) {
      return commaString(Number.isFinite(v) && !Number.isInteger(v) ? Math.round(v * 100) / 100 : v);
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
      this.spiGraphType = spiGraphType;
      if (this.spiGraphType === 'pie' || this.spiGraphType === 'bar' || this.spiGraphType === 'tagcloud' || this.spiGraphType === 'waffle' ||
        this.spiGraphType === 'table' || this.spiGraphType === 'treemap' || this.spiGraphType === 'sankey' || this.spiGraphType === 'matrix' || this.spiGraphType === 'mosaic') {
        if (!this.$route.query.size) {
          this.query.size = 5; // set default size to 5
        }
        this.sortBy = 'graph'; // set default sort to count (graph)
        this.query.sort = this.rankKey;
        this.refresh = 0;
        this.changeRefreshInterval(0);
      } else if (this.spiGraphType === 'heatmap' || this.spiGraphType === 'timeofday' || this.isMetricsChartType) {
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

      // so does a graph drawn by its dashboard widget
      if (this.widgetGraph) {
        this.loading = false;
        this.error = '';
        respondedAt = Date.now();
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
/* the value's (hidden until hover) menu arrow stays on its line: the field's
   98% max width of this shrink-to-fit item would wrap it onto a second one,
   and the swatch would center against both */
.spigraph-page .timeline-stacked__item :deep(.field) {
  max-width: none;
  white-space: nowrap;
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
