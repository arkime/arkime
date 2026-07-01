<template>
  <div>
    <!-- Error message -->
    <arkime-error
      v-if="error"
      :message="error"
      class="mt-5 mb-5" />

    <!-- Skeleton stats (shown while loading, before first chunk) -->
    <div
      v-if="loading && !summary && !error"
      class="summary-content m-2">
      <!-- Skeleton Field Widgets -->
      <div class="charts-container charts-grid">
        <div
          v-for="w in widgets"
          :key="w.id"
          class="widget-wrapper"
          :style="spanStyle(w)">
          <SummaryWidget
            :title="w.title || FieldService.getField(w.field, true)?.friendlyName || w.field"
            :data="[]"
            :loading="true"
            :field="w.field" />
        </div>
      </div>
    </div>

    <!-- Summary content (shows progressively as data streams in) -->
    <div
      v-if="!error && summary"
      class="summary-content m-2">
      <!-- Shared tooltip for all charts -->
      <SummaryChartTooltip
        :visible="tooltipVisible"
        :data="tooltipData"
        :field-config="tooltipFieldConfig"
        :position="tooltipPosition"
        :percentage="tooltipPercentage"
        :metric-type="tooltipMetricType"
        @close="hideTooltip" />

      <!-- empty state when no widgets configured -->
      <div
        v-if="!summary.fields?.length"
        class="empty-dashboard text-medium-emphasis">
        <v-icon
          icon="mdi-view-dashboard-outline"
          size="x-large"
          class="mb-2" />
        <p class="mb-0">
          {{ $t('sessions.summary.noWidgets') }}
        </p>
      </div>

      <!-- Charts Grid -->
      <div
        v-else
        ref="widgetContainer"
        class="charts-container charts-grid">
        <!-- Widgets rendered via v-for -->
        <div
          v-for="w in summary.fields"
          :key="w.id"
          :data-widget-id="w.id"
          class="widget-wrapper"
          :class="{ 'widget-flash': w.id === flashWidgetId }"
          :style="spanStyle(w)">
          <!-- reorder handle lives in the card header (.widget-drag) -->
          <!-- drag-to-resize handles: right edge (width), bottom edge (height),
               corner (both); snap to the 4-col / 160px-row grid -->
          <span
            class="widget-resize widget-resize--e"
            :title="$t('sessions.summary.dragToResize')"
            @mousedown.stop.prevent="startResize(w, $event, 'x')" />
          <span
            class="widget-resize widget-resize--s"
            :title="$t('sessions.summary.dragToResize')"
            @mousedown.stop.prevent="startResize(w, $event, 'y')" />
          <span
            class="widget-resize widget-resize--se"
            :title="$t('sessions.summary.dragToResize')"
            @mousedown.stop.prevent="startResize(w, $event, 'both')" />
          <!-- multi-field (2-3) nested pie/treemap -->
          <HierarchyWidget
            v-if="isMultiField(w) && (w.viewMode === 'pie' || w.viewMode === 'treemap')"
            :widget="w"
            :reload-nonce="reloadNonce"
            :color-scheme="colorScheme"
            :info-items="widgetInfo(w)"
            @show-tooltip="showTooltip"
            @edit="openEdit(w.id)"
            @remove="removeWidgetLocal(w)" />
          <!-- table: 1-3 fields side by side, each with its metric column(s) -->
          <MultiFieldTableWidget
            v-else-if="w.viewMode === 'table'"
            :widget="w"
            :reload-nonce="reloadNonce"
            :info-items="widgetInfo(w)"
            @edit="openEdit(w.id)"
            @remove="removeWidgetLocal(w)" />
          <SummaryWidget
            v-else-if="isStreamMode(w.viewMode)"
            :title="w.title || FieldService.getField(w.field, true)?.friendlyName || w.field"
            :data="w.data"
            :loading="w.loading"
            :error="w.error"
            :view-mode="w.viewMode"
            :metric-type="w.metricType"
            :field="w.field"
            :color-scheme="colorScheme"
            :info-items="widgetInfo(w)"
            @show-tooltip="showTooltip"
            @export="handleWidgetExport(w, $event)"
            @edit="openEdit(w.id)"
            @remove-field="removeWidgetLocal(w)"
            @retry-field="retryWidget(w.id)" />
          <HeatmapWidget
            v-else-if="w.viewMode === 'heatmap'"
            :widget="w"
            :reload-nonce="reloadNonce"
            :color-scheme="colorScheme"
            :info-items="widgetInfo(w)"
            @edit="openEdit(w.id)"
            @remove="removeWidgetLocal(w)" />
          <TreemapWidget
            v-else-if="w.viewMode === 'treemap'"
            :widget="w"
            :reload-nonce="reloadNonce"
            :color-scheme="colorScheme"
            :info-items="widgetInfo(w)"
            @show-tooltip="showTooltip"
            @edit="openEdit(w.id)"
            @remove="removeWidgetLocal(w)" />
          <IntersectionWidget
            v-else-if="w.viewMode === 'intersection'"
            :widget="w"
            :reload-nonce="reloadNonce"
            :info-items="widgetInfo(w)"
            @edit="openEdit(w.id)"
            @remove="removeWidgetLocal(w)" />
          <!-- session-wide widgets: fed by the host's global stats chunk -->
          <TimelineWidget
            v-else-if="w.viewMode === 'timeline'"
            :widget="w"
            :reload-nonce="reloadNonce"
            :timeline-data-filters="timelineDataFilters"
            :info-items="widgetInfo(w)"
            @time-range="onTimeRange"
            @edit="openEdit(w.id)"
            @remove="removeWidgetLocal(w)" />
          <MapWidget
            v-else-if="w.viewMode === 'map'"
            :widget="w"
            :reload-nonce="reloadNonce"
            :info-items="widgetInfo(w)"
            @edit="openEdit(w.id)"
            @remove="removeWidgetLocal(w)" />
          <StatsWidget
            v-else-if="w.viewMode === 'stats'"
            :stats="summary"
            :title="w.title"
            :info-items="widgetInfo(w)"
            @edit="openEdit(w.id)"
            @remove="removeWidgetLocal(w)" />
          <TimeWidget
            v-else-if="w.viewMode === 'time'"
            :stats="summary"
            :title="w.title"
            :info-items="widgetInfo(w)"
            @edit="openEdit(w.id)"
            @remove="removeWidgetLocal(w)" />
        </div>
      </div> <!-- /charts-container -->
    </div>

    <!-- Per-widget edit modal -->
    <SummaryWidgetEditModal
      :show="editModalShow"
      :widget="editingWidget"
      @close="onEditClose"
      @save="onEditSave" />
  </div>
</template>

<script setup>
// external dependencies
import { ref, inject, onMounted, onBeforeUnmount, computed, watch, nextTick } from 'vue';
import { themedColor } from '@common/themes/themedColor.js';
import { useRoute, useRouter } from 'vue-router';
import { useStore } from 'vuex';
import { useI18n } from 'vue-i18n';
import Sortable from 'sortablejs';
// internal dependencies
import setReqHeaders from '@common/setReqHeaders';
import ArkimeError from '../utils/Error.vue';
import SummaryWidget from './SummaryWidget.vue';
import SummaryWidgetEditModal from './SummaryWidgetEditModal.vue';
import HeatmapWidget from './widgets/HeatmapWidget.vue';
import TreemapWidget from './widgets/TreemapWidget.vue';
import IntersectionWidget from './widgets/IntersectionWidget.vue';
import HierarchyWidget from './widgets/HierarchyWidget.vue';
import MultiFieldTableWidget from './widgets/MultiFieldTableWidget.vue';
import TimelineWidget from './widgets/TimelineWidget.vue';
import MapWidget from './widgets/MapWidget.vue';
import StatsWidget from './widgets/StatsWidget.vue';
import TimeWidget from './widgets/TimeWidget.vue';
import SummaryChartTooltip from './SummaryChartTooltip.vue';
import { isStreamMode, isFieldMode, isGeoFieldMode, hasMetric, hasAgg, allowsMultiField } from './widgets/viewModes';
import { widgetLocalExpression, widgetFields } from './widgets/widgetData';
import FieldService from '../search/FieldService';
import ConfigService from '../utils/ConfigService';
import Utils from '../utils/utils';
import { commaString, humanReadableBytes } from '@common/vueFilters.js';

// Streaming response helpers

// Decode Uint8Array to string
const textDecoder = new TextDecoder('utf-8');
const decoder = (arr) => textDecoder.decode(arr);

// Parse JSON chunk, returning null for empty/invalid chunks
const parseChunk = (chunk) => {
  if (chunk.length < 2) return null;
  try {
    return JSON.parse(chunk);
  } catch (err) {
    console.error('Error parsing chunk:', chunk, err);
    return null;
  }
};

// Access route and store
const route = useRoute();
const router = useRouter();
const store = useStore();
const { t } = useI18n();

// Define props
const props = defineProps({
  // widget definitions (source of truth lives in the parent)
  widgets: {
    type: Array,
    default: () => []
  },
  // dashboard-wide chart color palette
  colorScheme: {
    type: String,
    default: 'rainbow'
  }
});

// Define emits
const emit = defineEmits(['widget-config-changed', 'streaming-state', 'canceled-state']);

// Save a pending promise to be able to cancel it
let pendingPromise;

// Sortable instance for drag-and-drop reordering
let sortableInstance = null;

// Computed properties
const user = computed(() => store.state.user);

// Timeline-data-filter field objects for the timeline widget's series selector
// (the user's configured timeline metrics, resolved to field objects)
const timelineDataFilters = computed(() => (user.value?.settings?.timelineDataFilters || [])
  .map(f => FieldService.getField(f)).filter(Boolean));

// A timeline widget's drag-select updates the dashboard time range (mirrors the
// old pinned timeline): commit the new range and push it onto the URL to reload.
const onTimeRange = (times) => {
  if (!times?.startTime || !times?.stopTime) { return; }
  store.commit('setTimeRange', 0); // custom range
  store.commit('setTime', times);
  if (route.query.date !== undefined ||
      route.query.startTime !== times.startTime || route.query.stopTime !== times.stopTime) {
    router.push({
      query: { ...route.query, date: undefined, startTime: times.startTime, stopTime: times.stopTime }
    });
  }
};

// Per-widget grid span: 1-4 columns (grid is 4 cols wide) / 1-8 rows. Rows are a
// fine 160px unit, so a chart (default 3 rows) is 480px while short widgets
// (stats/time) can be a single ~160px row.
const spanStyle = (w) => ({
  gridColumn: `span ${Math.min(4, Math.max(1, w?.width || 2))}`,
  gridRow: `span ${Math.min(8, Math.max(1, w?.height || 3))}`
});

// True when a widget has 2-3 fields and a view mode that nests/compares them
// (pie/treemap → HierarchyWidget; table → MultiFieldTableWidget).
const isMultiField = (w) => allowsMultiField(w?.viewMode) && widgetFields(w).length > 1;

// Drag-to-resize: a handle on the right edge changes width (1-4 cols), the bottom
// edge changes height (1-8 rows), the corner does both. Deltas snap to the grid
// (4 equal columns + 8px gap; 160px row unit). Persists on release.
const GRID_GAP = 8; // 0.5rem gap between cells (matches .charts-container gap)
const ROW_UNIT = 160; // grid-auto-rows
const startResize = (w, evt, axis) => {
  const wrapperEl = evt.currentTarget.closest('.widget-wrapper');
  if (!wrapperEl) { return; }
  const startRect = wrapperEl.getBoundingClientRect();
  const startCols = Math.min(4, Math.max(1, w.width || 2));
  const cellW = (startRect.width - (startCols - 1) * GRID_GAP) / startCols;

  const onMove = (e) => {
    // re-read position each frame so reflow (wrapping) doesn't skew the math
    const r = wrapperEl.getBoundingClientRect();
    if (axis !== 'y') {
      const cols = Math.round((e.clientX - r.left + GRID_GAP) / (cellW + GRID_GAP));
      w.width = Math.min(4, Math.max(1, cols));
    }
    if (axis !== 'x') {
      const rows = Math.round((e.clientY - r.top + GRID_GAP) / (ROW_UNIT + GRID_GAP));
      w.height = Math.min(8, Math.max(1, rows));
    }
  };
  const onUp = () => {
    window.removeEventListener('mousemove', onMove);
    window.removeEventListener('mouseup', onUp);
    document.body.style.userSelect = '';
    document.body.style.cursor = '';
    emitWidgetConfigChange(); // persist new spans
    window.dispatchEvent(new Event('resize')); // nudge chart/map reflow
  };

  document.body.style.userSelect = 'none';
  document.body.style.cursor = axis === 'x' ? 'ew-resize' : (axis === 'y' ? 'ns-resize' : 'nwse-resize');
  window.addEventListener('mousemove', onMove);
  window.addEventListener('mouseup', onUp);
};

// Reactive state
const summary = ref(null);
const loading = ref(true);
const error = ref('');
const widgetContainer = ref(null);
// scroll container provided by the page shell (PageLayout)
const pageScrollEl = inject('pageScrollEl', null);
const canceled = ref(false);
const streaming = ref(false);
// bumped on every (re)load so self-fetching view modes (e.g. heatmap) refetch
const reloadNonce = ref(0);
// briefly highlights a freshly-added widget so it's noticed after scrolling
const flashWidgetId = ref('');

// Per-widget edit modal state
const editModalShow = ref(false);
const editingWidget = ref(null);

// Shared tooltip state - one tooltip to rule them all
const tooltipVisible = ref(false);
const tooltipData = ref(null);
const tooltipPosition = ref({ x: 0, y: 0 });
const tooltipPercentage = ref(null);
const tooltipFieldConfig = ref(null);
const tooltipMetricType = ref('sessions');

// Save SVG as PNG will be loaded lazily (for export functionality)
let saveSvgAsPng;

// Build a render entry (widget definition + data state) from a definition.
// Only configured stream-mode widgets (bar/pie/table) start loading; an
// unconfigured widget (no field) shows its configure state, and self-fetch
// view modes (heatmap/treemap) manage their own loading.
const makeEntry = (def) => ({
  ...def,
  data: [],
  // only single-field stream widgets are fed by the host stream; multi-field
  // pie/table and self-fetch modes manage their own loading
  loading: !!def.field && isStreamMode(def.viewMode) && !isMultiField(def),
  error: null
});

// Map a widget definition to the request payload (only what the server needs).
// The View (if any) is folded into the local expression here; the server ANDs
// it with the global search.
const toRequestWidget = (w) => ({
  id: w.id,
  field: w.field,
  length: w.length,
  order: w.order,
  metricType: w.metricType,
  expression: widgetLocalExpression(w, store.state.views)
});

// Rows describing a widget's configuration that isn't visible on the card
// itself (metric, limit/order, local filter, saved view), shown in the header
// info tooltip.
const widgetInfo = (w) => {
  if (!w) { return []; }
  const rows = [];

  const exps = widgetFields(w);
  if (exps.length) {
    const names = exps.map(e => FieldService.getField(e, true)?.friendlyName || e).join(', ');
    rows.push({ label: t(exps.length > 1 ? 'sessions.summary.widget.fields' : 'sessions.summary.widget.field'), value: names });
  }

  // metric only applies to the metric-driven charts / tables
  if (hasMetric(w.viewMode)) {
    const metricName = (m) => (m === 'sessions'
      ? t('sessions.summary.sessions')
      : (FieldService.getField(m, true)?.friendlyName || m));
    const metricExps = (Array.isArray(w.metrics) && w.metrics.length) ? w.metrics : [w.metricType || 'sessions'];
    rows.push({ label: t('sessions.summary.widget.metric'), value: metricExps.map(metricName).join(', ') });
    if (metricExps.length > 1 && w.sortMetric) {
      rows.push({ label: t('sessions.summary.widget.sortBy'), value: metricName(w.sortMetric) });
    }
  }

  // Top/Bottom N only applies to the aggregating widgets
  if (hasAgg(w.viewMode)) {
    const orderLabel = w.order === 'asc' ? t('sessions.summary.bottom') : t('sessions.summary.top');
    rows.push({ label: t('sessions.summary.widget.limit'), value: `${orderLabel} ${w.length || 20}` });
  }

  if (w.view) {
    const v = (store.state.views || []).find(x => x.id === w.view || x.name === w.view);
    rows.push({ label: t('sessions.summary.widget.localView'), value: v?.name || w.view });
  }
  if (w.expression) {
    rows.push({ label: t('sessions.summary.widget.localFilter'), value: w.expression });
  }

  return rows;
};

// Cancel an in-progress summary stream
const cancelLoading = () => {
  if (!pendingPromise) return;

  // Cancel ES task server-side (fire and forget)
  ConfigService.cancelEsTask(pendingPromise.cancelId).catch(() => {});

  // Abort client-side fetch
  pendingPromise.controller.abort();
  pendingPromise = null;

  // Update state
  canceled.value = true;
  streaming.value = false;
  loading.value = false;

  // Mark still-loading widgets as cancelled
  if (summary.value?.fields) {
    for (let i = 0; i < summary.value.fields.length; i++) {
      if (summary.value.fields[i].loading) {
        summary.value.fields[i] = {
          ...summary.value.fields[i],
          loading: false,
          error: t('sessions.summary.canceledSearch')
        };
      }
    }
  }
};

// Build the common request body from current route/store state. `statsless`
// skips the server's Phase-1 stats/map/graph for incremental single-widget
// fetches (add/edit/retry) — the dashboard stats don't change for those.
const buildRequestBody = (widgetDefs, cancelId, statsless = false) => {
  const body = {
    cancelId,
    widgets: widgetDefs.map(toRequestWidget)
  };

  if (statsless) { body.noStats = true; }

  const routeParams = ['view', 'bounding', 'interval', 'expression', 'cluster'];
  for (const param of routeParams) {
    if (route.query[param]) {
      body[param] = route.query[param];
    }
  }

  if (route.query.spanning === 'true') {
    body.spanning = true;
  }

  if (route.query.start) { body.start = route.query.start; }

  if (parseInt(store.state.timeRange, 10) === -1) {
    body.date = store.state.timeRange;
  } else {
    body.startTime = store.state.time.startTime;
    body.stopTime = store.state.time.stopTime;
  }

  // The dashboard's visualizations fetch their own data — timeline & map
  // self-fetch spigraph, stats/time use the top-level counts — so the global
  // stats fetch no longer needs the graph (facets) or map aggregations. Still
  // bypass the sessions-page large-dataset toggle so the stat counts aggregate.
  store.commit('setDisabledAggregations', false);

  return body;
};

// Merge a streamed field chunk (keyed by widget id) into the matching entry
const mergeFieldChunk = (chunk) => {
  if (!summary.value?.fields) return;
  const index = summary.value.fields.findIndex(f => f.id === chunk.id);
  if (index === -1) return;
  summary.value.fields[index] = {
    ...summary.value.fields[index],
    data: chunk.data || [],
    loading: false,
    error: chunk.error || null
  };
};

// Fetch a subset of widgets and merge them into the existing summary. These are
// incremental (add/edit/retry) so the dashboard stats are already loaded — skip
// recomputing them server-side.
const fetchWidgets = async (widgetDefs) => {
  if (!widgetDefs.length) return;
  const cancelId = Utils.createRandomString();
  const body = buildRequestBody(widgetDefs, cancelId, true);

  const controller = new AbortController();

  try {
    const fetchResponse = await fetch('api/sessions/summary', {
      method: 'POST',
      headers: setReqHeaders({ 'Content-Type': 'application/json' }),
      body: JSON.stringify(body),
      signal: controller.signal
    });

    if (!fetchResponse.ok) {
      throw new Error(fetchResponse.statusText);
    }

    const reader = fetchResponse.body.getReader();
    let buffer = '';

    while (true) {
      const { done, value } = await reader.read();
      if (done) break;

      buffer += decoder(value);

      let pos;
      while ((pos = buffer.indexOf('\n')) > -1) {
        let line = buffer.slice(0, pos);
        buffer = buffer.slice(pos + 1);

        // Drop the leading '[' (first line) or ',' (subsequent lines)
        line = line.slice(1);

        const chunk = parseChunk(line);
        if (chunk && chunk.id) {
          // Only process widget chunks — ignore the stats chunk
          mergeFieldChunk(chunk);
        }
      }
    }
  } catch (err) {
    if (err.name === 'AbortError') return;
    // Mark retried widgets as errored
    if (summary.value?.fields) {
      for (const def of widgetDefs) {
        const index = summary.value.fields.findIndex(f => f.id === def.id);
        if (index !== -1 && summary.value.fields[index].loading) {
          summary.value.fields[index] = {
            ...summary.value.fields[index],
            loading: false,
            error: err.message || String(err)
          };
        }
      }
    }
  }
};

// Retry a single failed widget
const retryWidget = (id) => {
  if (!summary.value?.fields) return;

  const index = summary.value.fields.findIndex(f => f.id === id);
  if (index === -1) return;

  // Reset to loading state (keep the definition)
  summary.value.fields[index] = { ...summary.value.fields[index], data: [], loading: true, error: null };

  fetchWidgets([summary.value.fields[index]]);
};

// Retry all failed widgets at once
const retryAllFailed = () => {
  if (!summary.value?.fields) return;

  const errored = summary.value.fields.filter(f => f.error);
  if (!errored.length) return;

  // Reset all errored widgets to loading
  for (const w of errored) {
    const index = summary.value.fields.findIndex(f => f.id === w.id);
    if (index !== -1) {
      summary.value.fields[index] = { ...summary.value.fields[index], data: [], loading: true, error: null };
    }
  }

  // Clear canceled state since user is retrying
  canceled.value = false;

  fetchWidgets(errored.map(w => summary.value.fields.find(f => f.id === w.id)));
};

// Methods
const generateSummary = async () => {
  // signal self-fetching view modes (e.g. heatmap) to refetch on every reload
  reloadNonce.value++;

  // Note: an empty dashboard still requests (widgets: []) so the capture-stats
  // band shows real numbers, consistent with a dashboard of only self-fetch widgets.
  canceled.value = false;
  streaming.value = true;

  // Abort any previous pending request
  if (pendingPromise) {
    pendingPromise.controller.abort();
    pendingPromise = null;
  }

  loading.value = true;
  error.value = '';
  summary.value = null; // Reset for progressive display

  try {
    // Create unique cancel id to make cancel req for corresponding es task
    const cancelId = Utils.createRandomString();

    // Stream only single-field stream-mode widgets; heatmap/treemap/intersection
    // and multi-field pie/table self-fetch. An empty list still returns the stats chunk.
    const fieldDefs = props.widgets.filter(w => w.field && isStreamMode(w.viewMode) && !isMultiField(w));
    const body = buildRequestBody(fieldDefs, cancelId);

    // Create abort controller for request cancellation
    const controller = new AbortController();
    pendingPromise = { controller, cancelId };

    // Make direct fetch call
    const fetchResponse = await fetch('api/sessions/summary', {
      method: 'POST',
      headers: setReqHeaders({ 'Content-Type': 'application/json' }),
      body: JSON.stringify(body),
      signal: controller.signal
    });

    if (!fetchResponse.ok) {
      throw new Error(fetchResponse.statusText);
    }

    // Extract and commit response time to store (matches fetchWrapper pattern)
    const responseTime = fetchResponse.headers.get('x-arkime-response-time');
    if (responseTime) {
      store.commit('setResponseTime', responseTime);
    }

    // Stream the response using newline-delimited JSON
    const reader = fetchResponse.body.getReader();
    let buffer = '';

    // Handle each parsed chunk
    const handleChunk = (chunk) => {
      // Check for query errors (can come as first chunk)
      if (chunk.bsqErr) {
        throw new Error(chunk.bsqErr);
      }

      // First chunk has summary stats (sessions, packets, bytes, etc.) + map/graph
      // which now feed the stats/time/timeline/map widgets directly.
      if (chunk.sessions !== undefined) {
        // Initialize render entries for all configured widgets (loading state)
        chunk.fields = props.widgets.map(makeEntry);
        summary.value = chunk;

        // Hide loading overlay once we have summary stats (widgets show their own loading)
        loading.value = false;
      } else if (chunk.id) {
        // Widget chunks have an id - find and update the placeholder
        mergeFieldChunk(chunk);
      }
      // Empty object {} signals stream end (ignore)
    };

    // Process the stream
    // Format: [{first}\n,{second}\n,{third}\n...{}]
    while (true) {
      const { done, value } = await reader.read();

      if (done) {
        // Process any remaining buffer (shouldn't normally have anything)
        break;
      }

      buffer += decoder(value);

      // Process complete lines (newline-delimited JSON)
      let pos;
      while ((pos = buffer.indexOf('\n')) > -1) {
        let line = buffer.slice(0, pos);
        buffer = buffer.slice(pos + 1);

        // Drop the leading '[' (first line) or ',' (subsequent lines)
        line = line.slice(1);

        const chunk = parseChunk(line);
        if (chunk) handleChunk(chunk);
      }
    }

    pendingPromise = null;
    streaming.value = false;
  } catch (err) {
    if (err.name === 'AbortError') {
      // Request was cancelled, don't show as error
      streaming.value = false;
      return;
    }
    pendingPromise = null;
    streaming.value = false;
    console.error('Error generating summary:', err);
    error.value = err.text || err.message || String(err);
    loading.value = false;
  }
};

// Shared tooltip methods
const showTooltip = (evt) => {
  tooltipData.value = evt.data;
  tooltipPosition.value = evt.position;
  tooltipPercentage.value = evt.percentage !== undefined ? evt.percentage : null;
  tooltipFieldConfig.value = evt.fieldConfig;
  tooltipMetricType.value = evt.metricType || 'sessions';
  tooltipVisible.value = true;
};

const hideTooltip = () => {
  tooltipVisible.value = false;
  tooltipData.value = null;
  tooltipPercentage.value = null;
  tooltipFieldConfig.value = null;
  tooltipMetricType.value = 'sessions';
};

// Close tooltip on outside click
const handleClickOutside = (e) => {
  if (tooltipVisible.value && !e.target.closest('.summary-tooltip')) {
    hideTooltip();
  }
};

// Export individual chart as PNG
const exportChart = async (svgId, filename) => {
  try {
    if (!saveSvgAsPng) {
      const saveSvgAsPngModule = await import('save-svg-as-png');
      saveSvgAsPng = saveSvgAsPngModule;
    }

    // Get computed colors from CSS variables
    const foregroundColor = themedColor('foreground', '#000000');
    const backgroundColor = themedColor('background', '#ffffff');

    // Find the SVG element
    const svgElement = document.getElementById(svgId);
    if (!svgElement) {
      return;
    }

    // Export the chart as PNG
    await saveSvgAsPng.saveSvgAsPng(
      svgElement,
      `arkime-summary-${filename}.png`,
      {
        backgroundColor,
        modifyCss: (selector, properties) => {
          if (selector.includes('text')) {
            properties = `fill: ${foregroundColor}; ${properties}`;
          }
          return selector + '{' + properties + '}';
        }
      }
    );
  } catch (err) {
    error.value = 'Failed to export chart as PNG';
  }
};

// Chart (bar/pie) export → PNG. Table widgets self-fetch and own their CSV export.
const handleWidgetExport = (widget, svgId) => {
  exportChart(svgId, widget.field);
};

// Temporarily add count labels to an SVG for export, returns a cleanup function
const addExportCountLabels = (svg, data, metricType, foregroundColor) => {
  const NS = 'http://www.w3.org/2000/svg';
  const added = [];

  const metricIsBytes = /bytes/i.test(metricType || '');
  const formatValue = (val) => {
    if (metricIsBytes) return humanReadableBytes(val || 0);
    return commaString(val || 0);
  };

  // Bar chart: add count text above each bar
  const bars = svg.querySelectorAll('rect.bar');
  if (bars.length > 0) {
    const g = bars[0].parentNode;
    bars.forEach((bar, i) => {
      if (i >= data.length) return;
      const x = parseFloat(bar.getAttribute('x'));
      const y = parseFloat(bar.getAttribute('y'));
      const width = parseFloat(bar.getAttribute('width'));
      const value = data[i]?.value || 0;

      const text = document.createElementNS(NS, 'text');
      text.setAttribute('x', String(x + width / 2));
      text.setAttribute('y', String(y - 5));
      text.setAttribute('text-anchor', 'middle');
      text.setAttribute('font-size', '10px');
      text.setAttribute('fill', foregroundColor);
      text.textContent = formatValue(value);
      g.appendChild(text);
      added.push(text);
    });
    return () => added.forEach(el => el.remove());
  }

  // Pie chart: add count text below each slice label
  const arcs = svg.querySelectorAll('g.arc');
  if (arcs.length > 0) {
    arcs.forEach((arc, i) => {
      if (i >= data.length) return;
      const existingText = arc.querySelector('text');
      if (!existingText || !existingText.textContent) return; // Skip tiny slices with no label
      const value = data[i]?.value || 0;
      const transform = existingText.getAttribute('transform');

      const countText = document.createElementNS(NS, 'text');
      countText.setAttribute('transform', transform);
      countText.setAttribute('text-anchor', 'middle');
      countText.setAttribute('font-size', '10px');
      countText.setAttribute('fill', foregroundColor);
      countText.setAttribute('dy', '1.2em');
      countText.textContent = formatValue(value);
      arc.appendChild(countText);
      added.push(countText);
    });
  }

  return () => added.forEach(el => el.remove());
};

// Export all chart widgets as a single PNG image
const exportAllPNG = async () => {
  try {
    if (!saveSvgAsPng) {
      const saveSvgAsPngModule = await import('save-svg-as-png');
      saveSvgAsPng = saveSvgAsPngModule;
    }

    if (!widgetContainer.value) return;

    // Get computed colors from CSS variables
    const foregroundColor = themedColor('foreground', '#000000');
    const backgroundColor = themedColor('background', '#ffffff');

    // Collect SVGs paired with their widget labels
    const wrappers = widgetContainer.value.querySelectorAll('.widget-wrapper');
    const entries = [];
    const cleanups = [];

    for (let i = 0; i < wrappers.length; i++) {
      const config = summary.value?.fields?.[i];
      if (!config || config.viewMode === 'table') continue; // Skip table widgets

      const svg = wrappers[i].querySelector('svg');
      if (!svg) continue; // Skip loading, error, or empty widgets
      const label = config?.title || FieldService.getField(config?.field, true)?.friendlyName || config?.field || '';
      const metricType = config?.metricType || 'sessions';
      const data = config?.data || [];

      // Temporarily add count labels to the live SVG
      const cleanup = addExportCountLabels(svg, data, metricType, foregroundColor);
      cleanups.push(cleanup);

      // Convert SVG to PNG data URI
      const uri = await saveSvgAsPng.svgAsPngUri(svg, {
        backgroundColor,
        modifyCss: (selector, properties) => {
          if (selector.includes('text')) {
            properties = `fill: ${foregroundColor}; ${properties}`;
          }
          return selector + '{' + properties + '}';
        }
      });

      entries.push({ uri, label });
    }

    // Remove all temporary count labels
    cleanups.forEach(fn => fn());

    if (!entries.length) return;

    // Load all images and measure dimensions
    const images = await Promise.all(entries.map(entry => new Promise((resolve, reject) => {
      const img = new Image();
      img.onload = () => resolve({ img, label: entry.label });
      img.onerror = reject;
      img.src = entry.uri;
    })));

    // Layout: stack vertically with a label row above each chart
    const labelHeight = 32;
    const padding = 16;
    const gap = 24;

    const canvasWidth = Math.max(...images.map(i => i.img.width)) + padding * 2;
    let canvasHeight = padding;
    for (let i = 0; i < images.length; i++) {
      canvasHeight += labelHeight + images[i].img.height;
      if (i < images.length - 1) canvasHeight += gap;
    }
    canvasHeight += padding;

    // Draw onto canvas
    const canvas = document.createElement('canvas');
    canvas.width = canvasWidth;
    canvas.height = canvasHeight;
    const ctx = canvas.getContext('2d');

    // Background
    ctx.fillStyle = backgroundColor;
    ctx.fillRect(0, 0, canvasWidth, canvasHeight);

    // Draw each chart with its label
    ctx.fillStyle = foregroundColor;
    ctx.font = 'bold 16px sans-serif';
    ctx.textBaseline = 'middle';

    let y = padding;
    for (const { img, label } of images) {
      ctx.fillStyle = foregroundColor;
      ctx.fillText(label, padding, y + labelHeight / 2);
      y += labelHeight;
      ctx.drawImage(img, padding, y);
      y += img.height + gap;
    }

    // Download
    canvas.toBlob((blob) => {
      const url = URL.createObjectURL(blob);
      const a = document.createElement('a');
      a.href = url;
      a.download = 'arkime-summary-export.png';
      a.click();
      URL.revokeObjectURL(url);
    });
  } catch (err) {
    error.value = 'Failed to export summary as PNG';
  }
};

// Initialize drag-and-drop reordering with Sortable.js
// NOTE: We use forceFallback + custom scrollFn because:
// 1. forceFallback is needed for consistent cross-browser drag behavior
// 2. SortableJS's scrollSpeed option only works with native HTML5 drag-and-drop,
//    not in fallback mode, so we implement custom scroll speed logic
const initializeDragDrop = () => {
  if (!widgetContainer.value) return;

  const scrollSensitivity = 200;
  const minScrollSpeed = 50;
  const maxScrollSpeed = 300;

  // the page shell's scroll container is the scroller (not the document)
  const scrollTarget = () => pageScrollEl?.value || document.documentElement;

  sortableInstance = Sortable.create(widgetContainer.value, {
    animation: 100,
    handle: '.widget-drag',
    draggable: '.widget-wrapper',
    ghostClass: 'widget-ghost',
    chosenClass: 'widget-chosen',
    scroll: scrollTarget(),
    scrollSensitivity,
    bubbleScroll: true,
    forceFallback: true,
    fallbackOnBody: true,
    scrollFn: (offsetX, offsetY, originalEvent) => {
      // Custom scroll function for dynamic speed based on cursor proximity to edge
      // offsetY is negative when near top, positive when near bottom
      if (offsetY !== 0 && originalEvent) {
        const el = scrollTarget();
        const rect = el.getBoundingClientRect();
        const cursorY = originalEvent.clientY;

        // Calculate distance from the scroller edge being scrolled toward
        const distanceFromEdge = offsetY < 0 ? cursorY - rect.top : rect.bottom - cursorY;

        // Calculate speed: closer to edge = faster scroll
        const ratio = 1 - (distanceFromEdge / scrollSensitivity);
        const speed = minScrollSpeed + (ratio * (maxScrollSpeed - minScrollSpeed));

        // Apply scroll in the appropriate direction
        el.scrollTop += offsetY > 0 ? speed : -speed;
      }
    },
    onSort: (evt) => {
      const oldIndex = evt.oldIndex;
      const newIndex = evt.newIndex;
      // Reorder the local data to match the new DOM order
      if (summary.value?.fields && oldIndex !== newIndex) {
        const w = summary.value.fields.splice(oldIndex, 1)[0];
        summary.value.fields.splice(newIndex, 0, w);
        // Emit the new full widget order to parent to persist
        emitWidgetConfigChange();
      }
    }
  });
};

// Watch for summary data to load, then initialize drag-drop
watch(summary, (newVal) => {
  if (newVal?.fields?.length) {
    nextTick(() => {
      // Destroy existing instance to prevent duplicate handlers
      if (sortableInstance) {
        sortableInstance.destroy();
      }
      initializeDragDrop();
    });
  }
});

// Emit streaming/canceled state changes to parent
watch(streaming, (val) => { emit('streaming-state', val); });
watch(canceled, (val) => { emit('canceled-state', val); });

// On mount
onMounted(() => {
  document.addEventListener('click', handleClickOutside);
  generateSummary();
});

// On unmount
onBeforeUnmount(() => {
  document.removeEventListener('click', handleClickOutside);

  // Cancel any pending request on unmount
  if (pendingPromise) {
    pendingPromise.controller.abort();
    pendingPromise = null;
  }

  // Cleanup sortable instance
  if (sortableInstance) {
    sortableInstance.destroy();
    sortableInstance = null;
  }
});

/**
 * Open the edit modal for a widget
 */
const openEdit = (id) => {
  const w = summary.value?.fields?.find(f => f.id === id);
  if (w) {
    editingWidget.value = { ...w };
    editModalShow.value = true;
  }
};

/**
 * Close the edit modal. If a just-added field-bound widget was never configured
 * (no field), drop it so cancel doesn't leave a dangling "configure me" card.
 * Session-wide widgets (timeline/map/stats/time) are valid without a field.
 */
const onEditClose = () => {
  editModalShow.value = false;
  const id = editingWidget.value?.id;
  const entry = id && summary.value?.fields?.find(f => f.id === id);
  if (entry && (isFieldMode(entry.viewMode) || isGeoFieldMode(entry.viewMode)) && !entry.field) {
    removeWidgetLocal(entry);
  }
};

/**
 * Apply an edited widget; refetch only when data-affecting fields changed
 */
const onEditSave = (updated) => {
  editModalShow.value = false;
  if (!summary.value?.fields) return;
  const index = summary.value.fields.findIndex(f => f.id === updated.id);
  if (index === -1) return;

  const prev = summary.value.fields[index];
  // self-fetch view modes (heatmap/treemap) react to the replaced entry on their
  // own; stream-mode widgets refetch here when the aggregation changed — including
  // when switching back from a self-fetch mode (they have no streamed data yet)
  // self-fetch modes (heatmap/treemap/intersection + multi-field pie/table) react
  // to the replaced entry via their own watchers; only single-field stream widgets
  // refetch through the host here.
  const becameStream = isStreamMode(updated.viewMode) && !isStreamMode(prev.viewMode);
  const needsRefetch = isStreamMode(updated.viewMode) && !isMultiField(updated) && (
    becameStream ||
    prev.field !== updated.field ||
    prev.length !== updated.length ||
    prev.order !== updated.order ||
    (prev.metricType || 'sessions') !== (updated.metricType || 'sessions') ||
    (prev.expression || '') !== (updated.expression || '') ||
    (prev.view || '') !== (updated.view || ''));

  summary.value.fields[index] = { ...prev, ...updated };
  emitWidgetConfigChange();

  if (needsRefetch) {
    summary.value.fields[index] = { ...summary.value.fields[index], data: [], loading: true, error: null };
    fetchWidgets([summary.value.fields[index]]);
  }
};

/**
 * Remove a widget locally (from its own menu) and persist
 */
const removeWidgetLocal = (widget) => {
  if (!summary.value?.fields) return;
  const index = summary.value.fields.findIndex(f => f.id === widget.id);
  if (index !== -1) {
    summary.value.fields.splice(index, 1);
  }
  emitWidgetConfigChange();
};

/**
 * Emits the current widget configuration to parent (for persistence)
 */
const emitWidgetConfigChange = () => {
  emit('widget-config-changed', getWidgets());
};

/**
 * Gets the current widget definitions for saving
 */
const getWidgets = () => {
  return (summary.value?.fields || []).map(w => ({
    id: w.id,
    field: w.field,
    fields: Array.isArray(w.fields) && w.fields.length ? w.fields.slice(0, 3) : (w.field ? [w.field] : []),
    viewMode: w.viewMode,
    metricType: w.metricType,
    length: w.length,
    order: w.order,
    expression: w.expression || '',
    view: w.view || '',
    height: w.height || 3,
    width: w.width || 2,
    title: w.title || ''
  }));
};

// Scroll a (possibly off-screen) widget into view and briefly highlight it
const scrollToWidget = (id) => {
  nextTick(() => {
    const el = widgetContainer.value?.querySelector(`[data-widget-id="${id}"]`);
    if (el) { el.scrollIntoView({ behavior: 'smooth', block: 'center' }); }
    flashWidgetId.value = id;
    setTimeout(() => { if (flashWidgetId.value === id) { flashWidgetId.value = ''; } }, 1800);
  });
};

// Add a single widget without re-fetching everything (parent-initiated)
const addWidget = (def) => {
  if (!summary.value?.fields) {
    // Summary hasn't loaded yet, need a full reload
    generateSummary();
    return;
  }

  summary.value.fields.push(makeEntry(def));
  // configured single-field stream widgets fetch immediately; self-fetch modes
  // (heatmap/treemap/intersection + multi-field pie/table) fetch themselves and an
  // unconfigured widget waits for its editor
  if (def.field && isStreamMode(def.viewMode) && !isMultiField(def)) {
    fetchWidgets([def]);
  }
  // bring the new widget into view + flash it (it may be below the fold)
  scrollToWidget(def.id);
};

// Remove a single widget by id without re-fetching (parent-initiated)
const removeWidget = (id) => {
  if (!summary.value?.fields) return;
  const index = summary.value.fields.findIndex(f => f.id === id);
  if (index !== -1) {
    summary.value.fields.splice(index, 1);
  }
};

// Expose methods to parent component
defineExpose({
  reloadSummary: generateSummary,
  addWidget,
  removeWidget,
  openEdit,
  getWidgets,
  exportAllPNG,
  cancelLoading,
  retryAllFailed
});
</script>

<style scoped>
.stats-container {
  display: flex;
  flex-direction: column;
  gap: 0.75rem;
}

@media (min-width: 1200px) {
  .stats-container {
    flex-direction: row;
    gap: 1rem;
  }
}

.summary-stats {
  flex: 1;
  padding: 0.75rem;
  border-radius: 6px;
  background: rgb(var(--v-theme-quaternary-lightest));
}

.summary-stats h4 {
  font-size: 1rem;
  margin: 0 0 0.5rem 0;
}

.stats-grid {
  display: grid;
  grid-template-columns: repeat(5, 1fr);
  gap: 0.4rem;
}

.stat-card {
  padding: 0.4rem;
  border-radius: 4px;
  text-align: center;
  border: 1px solid rgb(var(--v-theme-neutral));
  background: rgb(var(--v-theme-background));
}

.stat-label {
  font-size: 0.75rem;
  margin-bottom: 0.25rem;
}

.stat-value {
  font-size: 1.25rem;
  font-weight: bold;
}

.time-stats {
  flex-shrink: 0;
  padding: 0.5rem;
  border-radius: 6px;
  background: rgb(var(--v-theme-quaternary-lightest));
}

@media (min-width: 1200px) {
  .time-stats {
    width: 400px;
  }
}

.time-stats h4 {
  font-size: 0.875rem;
  margin: 0 0 0.35rem 0;
}

.time-grid {
  display: flex;
  flex-direction: column;
  gap: 0.25rem;
}

.time-item {
  display: flex;
  justify-content: space-between;
  align-items: baseline;
  padding: 0.2rem 0.4rem;
  border-radius: 3px;
  background: rgb(var(--v-theme-background));
  border: 1px solid rgb(var(--v-theme-neutral));
  font-size: 0.75rem;
  line-height: 1.3;
}

.time-label {
  font-weight: 500;
  margin-right: 0.5rem;
  white-space: nowrap;
}

.time-value {
  text-align: right;
  font-family: monospace;
}

.empty-dashboard {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  text-align: center;
  padding: 4rem 2rem;
}

.charts-container {
  gap: 0.5rem;
  margin-top: 0.75rem;
  display: grid;
}

/* Ensure grid items stretch to fill; height comes from the row span, not a floor */
.charts-container > * {
  min-width: 0;      /* Prevent grid blowout */
  min-height: 0;
}

/* Fixed 4-column grid; widgets span 1-4 cols and 1-8 rows via inline
   grid-column/-row. Rows are a fine 160px unit so short widgets (stats/time)
   can be a single row while charts span 3 (480px). */
.charts-grid {
  grid-template-columns: repeat(4, minmax(0, 1fr));
  grid-auto-rows: 160px;
}

/* Below 1200px the 4-col grid is too cramped for charts, so collapse to a single
   stacked column; at/above it, the full 4-col designed layout. The per-widget
   inline `grid-column: span N` must be overridden (!important) — otherwise a
   span > 1 makes the grid create implicit columns and widgets stop filling the
   width. Row spans (heights) are kept. */
@media (max-width: 1199.98px) {
  .charts-grid {
    grid-template-columns: 1fr;
  }
  .charts-grid > .widget-wrapper {
    grid-column: 1 / -1 !important;
  }
  /* single-column layout: no reorder/resize, so drop the drag + resize handles */
  .widget-resize,
  :deep(.widget-drag) {
    display: none;
  }
}

/* Widget wrapper for drag-and-drop */
.widget-wrapper {
  position: relative;
}

/* Briefly ring a freshly-added widget so it's noticed after scrolling */
.widget-wrapper.widget-flash {
  border-radius: 8px;
  animation: widgetFlash 1.8s ease-out;
}
@keyframes widgetFlash {
  0% { box-shadow: 0 0 0 3px rgb(var(--v-theme-foreground-accent)); }
  100% { box-shadow: 0 0 0 0 rgba(0, 0, 0, 0); }
}

/* reorder handle now lives inside the card header (.widget-drag in WidgetCard) */

/* Drag-to-resize handles (shown on hover) */
.widget-resize {
  position: absolute;
  z-index: 11;
  visibility: hidden;
}
.widget-wrapper:hover .widget-resize {
  visibility: visible;
}

/* Info icon + action buttons (edit/export/remove) reveal on hover like the drag
   handle. :focus-within keeps them reachable by keyboard. */
.widget-wrapper :deep(.widget-actions),
.widget-wrapper :deep(.widget-info-icon) {
  visibility: hidden;
}
.widget-wrapper:hover :deep(.widget-actions),
.widget-wrapper:hover :deep(.widget-info-icon),
.widget-wrapper:focus-within :deep(.widget-actions),
.widget-wrapper:focus-within :deep(.widget-info-icon) {
  visibility: visible;
}
.widget-resize--e {
  top: 0;
  right: -3px;
  width: 8px;
  height: 100%;
  cursor: ew-resize;
}
.widget-resize--s {
  left: 0;
  bottom: -3px;
  height: 8px;
  width: 100%;
  cursor: ns-resize;
}
.widget-resize--se {
  right: -3px;
  bottom: -3px;
  width: 18px;
  height: 18px;
  z-index: 12;
  cursor: nwse-resize;
}
/* visible L-grip for the corner handle */
.widget-resize--se::after {
  content: '';
  position: absolute;
  right: 3px;
  bottom: 3px;
  width: 9px;
  height: 9px;
  border-right: 2px solid rgb(var(--v-theme-neutral));
  border-bottom: 2px solid rgb(var(--v-theme-neutral));
  border-bottom-right-radius: 3px;
}

/* Visual feedback during drag */
.widget-ghost {
  opacity: 0.4;
}
.widget-chosen {
  box-shadow: 0 4px 12px rgba(0, 0, 0, 0.3);
}
</style>
