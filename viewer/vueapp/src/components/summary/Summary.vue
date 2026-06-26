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
      <div class="stats-container mb-3 placeholder-glow">
        <!-- Main Statistics Skeleton -->
        <div class="summary-stats">
          <h4 class="mb-2">
            {{ $t('sessions.summary.captureStatistics') }}
          </h4>
          <div class="stats-grid">
            <div
              v-for="n in 5"
              :key="n"
              class="stat-card">
              <div class="stat-label">
                <span class="placeholder col-8" />
              </div>
              <div class="stat-value">
                <span class="placeholder col-6" />
              </div>
            </div>
          </div>
        </div>

        <!-- Time Statistics Skeleton -->
        <div class="time-stats">
          <h4 class="mb-2">
            {{ $t('sessions.summary.timeInformation') }}
          </h4>
          <div class="time-grid">
            <div
              v-for="n in 4"
              :key="n"
              class="time-item">
              <span class="placeholder col-4" />
              <span class="placeholder col-6" />
            </div>
          </div>
        </div>
      </div>

      <!-- Skeleton Field Widgets -->
      <div
        class="charts-container charts-grid"
        :style="gridStyle">
        <div
          v-for="w in widgets"
          :key="w.id"
          class="widget-wrapper"
          :class="spanClass(w)">
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
      <!-- Statistics Container -->
      <div class="stats-container mb-3">
        <!-- Main Statistics -->
        <div class="summary-stats">
          <h4 class="mb-2">
            {{ $t('sessions.summary.captureStatistics') }}
          </h4>
          <div class="stats-grid">
            <div class="stat-card">
              <div class="stat-label">
                {{ $t('sessions.summary.sessions') }}
              </div>
              <div class="stat-value">
                {{ formatNumber(summary.sessions) }}
              </div>
            </div>
            <div class="stat-card">
              <div class="stat-label">
                {{ $t('sessions.summary.totalPackets') }}
              </div>
              <div class="stat-value">
                {{ formatNumber(summary.packets) }}
              </div>
            </div>
            <div class="stat-card">
              <div class="stat-label">
                {{ $t('sessions.summary.dataBytes') }}
              </div>
              <div class="stat-value">
                {{ formatBytes(summary.dataBytes) }}
              </div>
            </div>
            <div class="stat-card">
              <div class="stat-label">
                {{ $t('sessions.summary.totalBytes') }}
              </div>
              <div class="stat-value">
                {{ formatBytes(summary.bytes) }}
              </div>
            </div>
            <div class="stat-card">
              <div class="stat-label">
                {{ $t('sessions.summary.downloadBytes') }}
              </div>
              <div class="stat-value">
                {{ formatBytes(summary.downloadBytes) }}
              </div>
            </div>
          </div>
        </div>

        <!-- Time Statistics -->
        <div class="time-stats">
          <h4 class="mb-2">
            {{ $t('sessions.summary.timeInformation') }}
          </h4>
          <div class="time-grid">
            <div class="time-item">
              <span class="time-label">{{ $t('sessions.summary.duration') }}:</span>
              <span class="time-value">{{ formatDuration(summary.firstPacket, summary.lastPacket) }}</span>
            </div>
            <div class="time-item">
              <span class="time-label">{{ $t('sessions.summary.firstPacket') }}:</span>
              <span class="time-value">{{ formatTimestamp(summary.firstPacket) }}</span>
            </div>
            <div class="time-item">
              <span class="time-label">{{ $t('sessions.summary.lastPacket') }}:</span>
              <span class="time-value">{{ formatTimestamp(summary.lastPacket) }}</span>
            </div>
            <div class="time-item">
              <span class="time-label">{{ $t('sessions.summary.currentTime') }}:</span>
              <span class="time-value">{{ getCurrentTime() }}</span>
            </div>
          </div>
        </div>
      </div>

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
        class="charts-container charts-grid"
        :style="gridStyle">
        <!-- Widgets rendered via v-for -->
        <div
          v-for="w in summary.fields"
          :key="w.id"
          class="widget-wrapper"
          :class="spanClass(w)">
          <span
            class="widget-handle"
            :title="$t('sessions.summary.dragToReorder')">
            <v-icon icon="mdi-view-grid" />
          </span>
          <SummaryWidget
            :title="w.title || FieldService.getField(w.field, true)?.friendlyName || w.field"
            :data="w.data"
            :loading="w.loading"
            :error="w.error"
            :view-mode="w.viewMode"
            :metric-type="w.metricType"
            :field="w.field"
            @show-tooltip="showTooltip"
            @export="handleWidgetExport(w, $event)"
            @edit="openEdit(w.id)"
            @remove-field="removeWidgetLocal(w)"
            @retry-field="retryWidget(w.id)" />
        </div>
      </div> <!-- /charts-container -->
    </div>

    <!-- Per-widget edit modal -->
    <SummaryWidgetEditModal
      :show="editModalShow"
      :widget="editingWidget"
      @close="editModalShow = false"
      @save="onEditSave" />
  </div>
</template>

<script setup>
// external dependencies
import { ref, inject, onMounted, onBeforeUnmount, computed, watch, nextTick } from 'vue';
import { themedColor } from '@common/themes/themedColor.js';
import { useRoute } from 'vue-router';
import { useStore } from 'vuex';
import { useI18n } from 'vue-i18n';
import Sortable from 'sortablejs';
// internal dependencies
import setReqHeaders from '@common/setReqHeaders';
import ArkimeError from '../utils/Error.vue';
import SummaryWidget from './SummaryWidget.vue';
import SummaryWidgetEditModal from './SummaryWidgetEditModal.vue';
import SummaryChartTooltip from './SummaryChartTooltip.vue';
import FieldService from '../search/FieldService';
import ConfigService from '../utils/ConfigService';
import Utils from '../utils/utils';
import { commaString, humanReadableBytes, readableTime, timezoneDateString } from '@common/vueFilters.js';

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
const store = useStore();
const { t } = useI18n();

// Define props
const props = defineProps({
  // widget definitions (source of truth lives in the parent)
  widgets: {
    type: Array,
    default: () => []
  },
  // number of grid columns (2 or 3)
  columnCount: {
    type: Number,
    default: 2
  }
});

// Define emits
const emit = defineEmits(['update-visualizations', 'widget-config-changed', 'streaming-state', 'canceled-state']);

// Save a pending promise to be able to cancel it
let pendingPromise;

// Sortable instance for drag-and-drop reordering
let sortableInstance = null;

// Computed properties
const user = computed(() => store.state.user);
const timezone = computed(() => user.value?.settings?.timezone || 'local');
const showMs = computed(() => user.value?.settings?.ms === true);

// Grid column count exposed to CSS
const gridStyle = computed(() => ({ '--summary-cols': props.columnCount || 2 }));

// Per-widget grid span classes (height/width presets)
const spanClass = (w) => ({
  'span-h-2': w?.height === 'double',
  'span-w-2': w?.width === 'double'
});

// Reactive state
const summary = ref(null);
const loading = ref(true);
const error = ref('');
const widgetContainer = ref(null);
// scroll container provided by the page shell (PageLayout)
const pageScrollEl = inject('pageScrollEl', null);
const canceled = ref(false);
const streaming = ref(false);

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

// Build a render entry (widget definition + data state) from a definition
const makeEntry = (def) => ({
  ...def,
  data: [],
  loading: true,
  error: null
});

// Map a widget definition to the request payload (only what the server needs)
const toRequestWidget = (w) => ({
  id: w.id,
  field: w.field,
  length: w.length,
  order: w.order,
  expression: w.expression || undefined
});

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

// Build the common request body from current route/store state
const buildRequestBody = (widgetDefs, cancelId) => {
  const body = {
    cancelId,
    facets: 1,
    widgets: widgetDefs.map(toRequestWidget)
  };

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

  Utils.setFacetsQuery(body, 'sessions');
  Utils.setMapQuery(body);

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

// Fetch a subset of widgets and merge them into the existing summary
const fetchWidgets = async (widgetDefs) => {
  if (!widgetDefs.length) return;
  const cancelId = Utils.createRandomString();
  const body = buildRequestBody(widgetDefs, cancelId);

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
  // Wait for widgets to be provided by parent before generating summary
  if (!props.widgets?.length) {
    // No widgets configured: show an empty dashboard rather than spinning
    loading.value = false;
    error.value = '';
    summary.value = summary.value
      ? { ...summary.value, fields: [] }
      : { sessions: 0, packets: 0, bytes: 0, dataBytes: 0, downloadBytes: 0, firstPacket: 0, lastPacket: 0, fields: [] };
    return;
  }

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

    // Build request body using shared helper
    const body = buildRequestBody(props.widgets, cancelId);

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

      // First chunk has summary stats (sessions, packets, bytes, etc.)
      if (chunk.sessions !== undefined) {
        // Initialize render entries for all configured widgets (loading state)
        chunk.fields = props.widgets.map(makeEntry);
        summary.value = chunk;

        // Emit map/graph data immediately
        emit('update-visualizations', {
          mapData: chunk.map,
          graphData: chunk.graph
        });

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

const formatNumber = (num) => {
  return commaString(num || 0);
};

const formatBytes = (bytes) => {
  return humanReadableBytes(bytes || 0);
};

const formatDuration = (start, end) => {
  if (!start || !end) return 'N/A';
  return readableTime(end - start);
};

const formatTimestamp = (timestamp) => {
  if (!timestamp) return 'N/A';
  // Convert seconds to milliseconds if needed (Arkime stores timestamps in seconds)
  const ms = timestamp < 10000000000 ? timestamp * 1000 : timestamp;
  return timezoneDateString(ms, timezone.value, showMs.value);
};

const getCurrentTime = () => {
  return timezoneDateString(Date.now(), timezone.value, showMs.value);
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

// Helper function to escape CSV values
const escapeCSV = (value) => {
  let stringValue = String(value);
  // Neutralize CSV/spreadsheet formula injection. A cell that begins with one of
  // = + - @ TAB CR can execute as a formula when opened in Excel/Google Sheets.
  // Since values can come from captured traffic, prefix risky cells with a single quote.
  if (stringValue.length > 0 && '=+-@\t\r'.includes(stringValue[0])) {
    stringValue = `'${stringValue}`;
  }
  // Escape quotes by doubling them and wrap in quotes if contains comma, quote, or newline
  if (stringValue.includes('"') || stringValue.includes(',') || stringValue.includes('\n')) {
    return `"${stringValue.replace(/"/g, '""')}"`;
  }
  return stringValue;
};

// Export table as CSV
const exportTableCSV = (dataKeyOrData, headers, filename) => {
  try {
    // Support both direct data and lookup by key
    const data = typeof dataKeyOrData === 'string'
      ? summary.value?.[dataKeyOrData]
      : dataKeyOrData;

    if (!data?.length) {
      return;
    }

    let csv = headers.join(',') + '\n';

    data.forEach(item => {
      csv += `${escapeCSV(item.item)},${item.sessions},${item.packets},${item.bytes}\n`;
    });

    const blob = new Blob([csv], { type: 'text/csv' });
    const url = window.URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = filename;
    a.click();
    window.URL.revokeObjectURL(url);
  } catch (err) {
    error.value = 'Failed to export CSV file';
  }
};

// Simplified widget export handler
const handleWidgetExport = (widget, svgId) => {
  const filename = widget.field;
  const itemLabel = widget.title || FieldService.getField(widget.field, true)?.friendlyName || widget.field;

  if (widget.viewMode === 'table') {
    const headers = [itemLabel, 'Sessions', 'Packets', 'Bytes'];
    exportTableCSV(widget.data, headers, `arkime-summary-${filename}.csv`);
  } else {
    exportChart(svgId, filename);
  }
};

// Temporarily add count labels to an SVG for export, returns a cleanup function
const addExportCountLabels = (svg, data, metricType, foregroundColor) => {
  const NS = 'http://www.w3.org/2000/svg';
  const added = [];

  const formatValue = (val) => {
    if (metricType === 'bytes') return humanReadableBytes(val || 0);
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
      const value = data[i]?.[metricType] || 0;

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
      const value = data[i]?.[metricType] || 0;
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
    handle: '.widget-handle',
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
 * Apply an edited widget; refetch only when data-affecting fields changed
 */
const onEditSave = (updated) => {
  editModalShow.value = false;
  if (!summary.value?.fields) return;
  const index = summary.value.fields.findIndex(f => f.id === updated.id);
  if (index === -1) return;

  const prev = summary.value.fields[index];
  const needsRefetch = prev.field !== updated.field ||
    prev.length !== updated.length ||
    prev.order !== updated.order ||
    (prev.expression || '') !== (updated.expression || '');

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
    viewMode: w.viewMode,
    metricType: w.metricType,
    length: w.length,
    order: w.order,
    expression: w.expression || '',
    height: w.height || 'standard',
    width: w.width || 'standard',
    title: w.title || ''
  }));
};

// Add a single widget without re-fetching everything (parent-initiated)
const addWidget = (def) => {
  if (!summary.value?.fields) {
    // Summary hasn't loaded yet, need a full reload
    generateSummary();
    return;
  }

  summary.value.fields.push(makeEntry(def));
  fetchWidgets([def]);
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
  gap: 1rem;
  margin-top: 1.5rem; /* Extra margin to accommodate drag handles */
  display: grid;
}

/* Ensure grid items stretch to fill and have minimum height */
.charts-container > * {
  min-width: 0;      /* Prevent grid blowout */
  min-height: 450px; /* Minimum height for readability */
}

/* Explicit column-count grid driven by --summary-cols (2 or 3) */
.charts-grid {
  grid-template-columns: repeat(var(--summary-cols, 2), minmax(0, 1fr));
  grid-auto-rows: 480px;
}

/* Single column on narrow viewports regardless of column setting */
@media (max-width: 1200px) {
  .charts-grid {
    grid-template-columns: 1fr;
  }
  /* double-width spans are meaningless in a single column */
  .widget-wrapper.span-w-2 {
    grid-column: auto;
  }
}

/* Per-widget height/width spans */
.widget-wrapper.span-h-2 {
  grid-row: span 2;
}
.widget-wrapper.span-w-2 {
  grid-column: span 2;
}

/* Widget wrapper for drag-and-drop */
.widget-wrapper {
  position: relative;
}

/* Drag handle for reordering widgets */
.widget-handle {
  visibility: hidden;
  color: rgb(var(--v-theme-neutral));
  cursor: move;
  position: absolute;
  top: -20px;
  left: 0;
  z-index: 10;
  background: rgb(var(--v-theme-quaternary-lightest));
  padding: 2px 6px;
  border-radius: 4px 4px 0 0;
}
.widget-wrapper:hover .widget-handle {
  visibility: visible;
}

/* Visual feedback during drag */
.widget-ghost {
  opacity: 0.4;
}
.widget-chosen {
  box-shadow: 0 4px 12px rgba(0, 0, 0, 0.3);
}
</style>
