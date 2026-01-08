<template>
  <div>
    <!-- Error message -->
    <div
      v-if="error"
      class="alert alert-danger m-3">
      {{ error }}
    </div>

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
        class="charts-container"
        :class="gridLayoutClass">
        <div
          v-for="field in summaryFields"
          :key="field"
          class="widget-wrapper">
          <SummaryWidget
            :title="FieldService.getField(field, true)?.friendlyName || field"
            :data="[]"
            :loading="true"
            :field="field" />
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

      <!-- Charts Grid -->
      <div
        ref="widgetContainer"
        class="charts-container"
        :class="gridLayoutClass">
        <!-- Widgets rendered via v-for -->
        <div
          v-for="widget in widgetConfigs"
          :key="widget.field"
          class="widget-wrapper">
          <span
            class="widget-handle"
            :title="$t('sessions.summary.dragToReorder')">
            <span class="fa fa-th" />
          </span>
          <SummaryWidget
            :title="widget.title || FieldService.getField(widget.field, true)?.friendlyName || widget.field"
            :data="widget.data"
            :loading="widget.loading"
            :error="widget.error"
            :view-mode="widget.viewMode.value"
            :metric-type="widget.metricType.value"
            :field="widget.field"
            @change-mode="updateWidgetViewMode(widget, $event)"
            @change-metric="updateWidgetMetricType(widget, $event)"
            @show-tooltip="showTooltip"
            @export="handleWidgetExport(widget, $event)" />
        </div>
      </div> <!-- /charts-container -->
    </div>
  </div>
</template>

<script setup>
// external dependencies
import { ref, onMounted, onBeforeUnmount, computed, watch, nextTick } from 'vue';
import { useRoute } from 'vue-router';
import { useStore } from 'vuex';
import { useI18n } from 'vue-i18n';
import Sortable from 'sortablejs';
// internal dependencies
import setReqHeaders from '@common/setReqHeaders';
import SummaryWidget from './SummaryWidget.vue';
import SummaryChartTooltip from './SummaryChartTooltip.vue';
import FieldService from '../search/FieldService';
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
  summaryFields: {
    type: Array,
    default: () => []
  }
});

// Define emits
const emit = defineEmits(['update-visualizations', 'reorder-fields', 'widget-config-changed']);

// Save a pending promise to be able to cancel it
let pendingPromise;

// Sortable instance for drag-and-drop reordering
let sortableInstance = null;

// Computed properties
const user = computed(() => store.state.user);
const timezone = computed(() => user.value?.settings?.timezone || 'local');
const showMs = computed(() => user.value?.settings?.ms === true);

// Grid layout class based on results limit
const gridLayoutClass = computed(() => {
  const resultsLimit = parseInt(route.query.summaryLength) || 20;
  // When results > 20, use fewer columns (more data per chart)
  // When results <= 20, use more columns (less data per chart)
  return resultsLimit > 20 ? 'charts-grid-large-data' : 'charts-grid-small-data';
});

// Reactive state
const summary = ref(null);
const loading = ref(true);
const error = ref('');
const widgetContainer = ref(null);

// Shared tooltip state - one tooltip to rule them all
const tooltipVisible = ref(false);
const tooltipData = ref(null);
const tooltipPosition = ref({ x: 0, y: 0 });
const tooltipPercentage = ref(null);
const tooltipFieldConfig = ref(null);
const tooltipMetricType = ref('sessions');

// Save SVG as PNG will be loaded lazily (for export functionality)
let saveSvgAsPng;

// Widget configurations for v-for rendering
// Dynamically built from the API response fields array
const widgetConfigs = computed(() => {
  if (!summary.value?.fields) {
    return [];
  }

  // Map each field from the API response to a widget configuration
  // Fields start as loading placeholders until their data arrives
  return summary.value.fields.map(fieldObj => ({
    data: fieldObj.data || [],
    loading: fieldObj.loading ?? false,   // true until field data arrives
    error: fieldObj.error || null,        // error message if field aggregation failed
    viewMode: ref(fieldObj.viewMode ?? 'bar'),     // viewMode from API (default bar)
    metricType: ref(fieldObj.metricType ?? 'sessions'), // metricType from API (default sessions)
    field: fieldObj.field,                // field expression from API
    title: fieldObj.title,                // title from API (may be undefined)
    description: fieldObj.description     // description from API (may be undefined)
  }));
});

// Methods
const generateSummary = async () => {
  // Wait for fields to be loaded from parent before generating summary
  if (!props.summaryFields?.length) {
    // No summary fields provided: ensure we are not stuck in a loading state
    loading.value = false;
    summary.value = null;
    error.value = 'No summary fields were provided to generate a summary.';
    return;
  }

  loading.value = true;
  error.value = '';
  summary.value = null; // Reset for progressive display

  try {
    // Create unique cancel id to make cancel req for corresponding es task
    const cancelId = Utils.createRandomString();

    // Build request body with fields and other params
    const body = {
      cancelId,
      facets: 1, // default to requesting facets for timeline graph (setFacetsQuery can override to 0)
      fields: props.summaryFields.join(',')
    };

    // Copy relevant params from route query
    const routeParams = ['view', 'bounding', 'interval', 'expression', 'cluster'];
    for (const param of routeParams) {
      if (route.query[param]) {
        body[param] = route.query[param];
      }
    }

    // Handle spanning param as boolean
    if (route.query.spanning === 'true') {
      body.spanning = true;
    }

    // Handle pagination params
    if (route.query.start) { body.start = route.query.start; }
    if (route.query.summaryLength) {
      body.length = route.query.summaryLength;
    } else if (route.query.length) {
      body.length = route.query.length;
    }

    // Handle time params - send stopTime and startTime unless date is all time (-1)
    if (parseInt(store.state.timeRange, 10) === -1) {
      body.date = store.state.timeRange;
    } else {
      body.startTime = store.state.time.startTime;
      body.stopTime = store.state.time.stopTime;
    }

    // Handle facets - check if visualizations are hidden
    Utils.setFacetsQuery(body, 'sessions');
    Utils.setMapQuery(body);

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
        // Initialize placeholder fields for all requested fields (they show loading state)
        chunk.fields = props.summaryFields.map(field => ({
          field,
          loading: true
        }));
        summary.value = chunk;

        // Emit map/graph data immediately
        emit('update-visualizations', {
          mapData: chunk.map,
          graphData: chunk.graph
        });

        // Hide loading overlay once we have summary stats (field cards show their own loading)
        loading.value = false;
      } else if (chunk.field) {
        // Field chunks have field property - find and update the placeholder
        if (summary.value?.fields) {
          const index = summary.value.fields.findIndex(f => f.field === chunk.field);
          if (index !== -1) {
            // Replace placeholder with actual data
            summary.value.fields[index] = chunk;
          }
        }
      }
      // Empty object {} signals stream end (ignore)
    };

    // Process the stream
    // Format: [{first}\n,{second}\n,{third}\n...{}]
    let isFirstLine = true;

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

        // First line starts with '[', subsequent lines start with ','
        if (isFirstLine) {
          line = line.slice(1); // Remove leading '['
          isFirstLine = false;
        } else {
          line = line.slice(1); // Remove leading ','
        }

        const chunk = parseChunk(line);
        if (chunk) handleChunk(chunk);
      }
    }

    pendingPromise = null;
  } catch (err) {
    if (err.name === 'AbortError') {
      // Request was cancelled, don't show as error
      return;
    }
    pendingPromise = null;
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
    const computedStyle = getComputedStyle(document.documentElement);
    const foregroundColor = computedStyle.getPropertyValue('--color-foreground').trim() || '#000000';
    const backgroundColor = computedStyle.getPropertyValue('--color-background').trim() || '#ffffff';

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
  const stringValue = String(value);
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

  if (widget.viewMode.value === 'table') {
    const headers = [itemLabel, 'Sessions', 'Packets', 'Bytes'];
    exportTableCSV(widget.data, headers, `arkime-summary-${filename}.csv`);
  } else {
    exportChart(svgId, filename);
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

  sortableInstance = Sortable.create(widgetContainer.value, {
    animation: 100,
    handle: '.widget-handle',
    draggable: '.widget-wrapper',
    ghostClass: 'widget-ghost',
    chosenClass: 'widget-chosen',
    scroll: document.documentElement,
    scrollSensitivity,
    bubbleScroll: true,
    forceFallback: true,
    fallbackOnBody: true,
    scrollFn: (offsetX, offsetY, originalEvent) => {
      // Custom scroll function for dynamic speed based on cursor proximity to edge
      // offsetY is negative when near top, positive when near bottom
      if (offsetY !== 0 && originalEvent) {
        const viewportHeight = window.innerHeight;
        const cursorY = originalEvent.clientY;

        // Calculate distance from the edge being scrolled toward
        const distanceFromEdge = offsetY < 0 ? cursorY : viewportHeight - cursorY;

        // Calculate speed: closer to edge = faster scroll
        const ratio = 1 - (distanceFromEdge / scrollSensitivity);
        const speed = minScrollSpeed + (ratio * (maxScrollSpeed - minScrollSpeed));

        // Apply scroll in the appropriate direction
        document.documentElement.scrollTop += offsetY > 0 ? speed : -speed;
      }
    },
    onSort: (evt) => {
      const oldIndex = evt.oldIndex;
      const newIndex = evt.newIndex;
      // Reorder the local data to match the new DOM order
      if (summary.value?.fields && oldIndex !== newIndex) {
        const field = summary.value.fields.splice(oldIndex, 1)[0];
        summary.value.fields.splice(newIndex, 0, field);
        // Emit to parent to persist the new order
        emit('reorder-fields', { oldIndex, newIndex });
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
    pendingPromise.controller.abort(t('sessions.summary.closingCancelsSearchErr'));
    pendingPromise = null;
  }

  // Cleanup sortable instance
  if (sortableInstance) {
    sortableInstance.destroy();
    sortableInstance = null;
  }
});

/**
 * Updates a widget's view mode and emits change event
 */
const updateWidgetViewMode = (widget, newMode) => {
  widget.viewMode.value = newMode;
  emitWidgetConfigChange();
};

/**
 * Updates a widget's metric type and emits change event
 */
const updateWidgetMetricType = (widget, newMetric) => {
  widget.metricType.value = newMetric;
  emitWidgetConfigChange();
};

/**
 * Emits the current widget configuration to parent
 */
const emitWidgetConfigChange = () => {
  emit('widget-config-changed', getWidgetConfigs());
};

/**
 * Gets the current widget configurations for saving
 * @returns {Array} Array of field configurations with viewMode and metricType
 */
const getWidgetConfigs = () => {
  return widgetConfigs.value.map(w => ({
    field: w.field,
    viewMode: w.viewMode.value,
    metricType: w.metricType.value
  }));
};

// Expose methods to parent component
defineExpose({
  reloadSummary: generateSummary,
  getWidgetConfigs
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
  background: var(--color-quaternary-lightest);
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
  border: 1px solid var(--color-gray);
  background: var(--color-background);
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
  background: var(--color-quaternary-lightest);
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
  background: var(--color-background);
  border: 1px solid var(--color-gray);
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

/* When results > 20 (large data): 1-2 columns max */
.charts-grid-large-data {
  /* Default: 1 column */
  grid-template-columns: 1fr;
}

@media (min-width: 2500px) {
  .charts-grid-large-data {
    /* Very large viewport: 2 columns */
    grid-template-columns: repeat(2, 1fr);
  }
}

/* When results <= 20 (small data): 2-3 columns max */
.charts-grid-small-data {
  /* Default: 2 columns */
  grid-template-columns: repeat(2, 1fr);
}

@media (min-width: 2500px) {
  .charts-grid-small-data {
    /* Very large viewport: 3 columns */
    grid-template-columns: repeat(3, 1fr);
  }
}

/* Widget wrapper for drag-and-drop */
.widget-wrapper {
  position: relative;
}

/* Drag handle for reordering widgets */
.widget-handle {
  visibility: hidden;
  color: var(--color-gray);
  cursor: move;
  position: absolute;
  top: -20px;
  left: 0;
  z-index: 10;
  background: var(--color-quaternary-lightest);
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
