<template>
  <div>
    <!-- Loading overlay -->
    <arkime-loading
      :can-cancel="true"
      v-if="loading && !error"
      @cancel="cancelAndLoad(false)" />

    <!-- Error message -->
    <div
      v-if="error"
      class="alert alert-danger m-3">
      {{ error }}
    </div>

    <!-- Summary content -->
    <div
      v-if="!loading && !error && summary"
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
        class="charts-container"
        :class="gridLayoutClass">
        <!-- Widgets rendered via v-for -->
        <SummaryWidget
          v-for="widget in widgetConfigs"
          :key="widget.field"
          :title="widget.title || FieldService.getField(widget.field, true)?.friendlyName || widget.field"
          :data="widget.data"
          :view-mode="widget.viewMode.value"
          :metric-type="widget.metricType.value"
          :field="widget.field"
          @change-mode="widget.viewMode.value = $event"
          @change-metric="widget.metricType.value = $event"
          @show-tooltip="showTooltip"
          @export="handleWidgetExport(widget, $event)" />
      </div> <!-- /charts-container -->
    </div>
  </div>
</template>

<script setup>
// external dependencies
import { ref, onMounted, onBeforeUnmount, computed } from 'vue';
import { useRoute } from 'vue-router';
import { useStore } from 'vuex';
import { useI18n } from 'vue-i18n';
// internal dependencies
import SessionsService from '../sessions/SessionsService';
import SummaryWidget from './SummaryWidget.vue';
import SummaryChartTooltip from './SummaryChartTooltip.vue';
import ArkimeLoading from '../utils/Loading.vue';
import ConfigService from '../utils/ConfigService';
import FieldService from '../search/FieldService';
import Utils from '../utils/utils';
import { commaString, humanReadableBytes, readableTime, timezoneDateString } from '@common/vueFilters.js';

// Access route and store
const route = useRoute();
const store = useStore();
const { t } = useI18n();

// Define emits
const emit = defineEmits(['update-visualizations']);

// Save a pending promise to be able to cancel it
let pendingPromise;

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
  return summary.value.fields.map(fieldObj => ({
    data: fieldObj.data || [],
    viewMode: ref(fieldObj.viewMode),     // viewMode from API
    metricType: ref(fieldObj.metricType), // metricType from API
    field: fieldObj.field,                // field expression from API
    title: fieldObj.title,                // title from API (may be undefined)
    description: fieldObj.description     // description from API (may be undefined)
  }));
});

// Methods
const generateSummary = async () => {
  loading.value = true;
  error.value = '';

  try {
    // Build query params from route and store (like Sessions.vue does)
    const queryParams = {
      ...route.query,
      date: store.state.timeRange,
      startTime: store.state.time.startTime,
      stopTime: store.state.time.stopTime,
      facets: 1
    };

    // Map summaryLength to length for the API
    if (queryParams.summaryLength) {
      queryParams.length = queryParams.summaryLength;
      delete queryParams.summaryLength;
    }

    // Create unique cancel id to make cancel req for corresponding es task
    const cancelId = Utils.createRandomString();
    queryParams.cancelId = cancelId;

    const { controller, fetcher } = SessionsService.generateSummary(queryParams);
    pendingPromise = { controller, cancelId };

    const response = await fetcher;
    summary.value = response;

    // Emit map/graph data to parent component for visualizations
    emit('update-visualizations', {
      mapData: response.map,
      graphData: response.graph
    });

    pendingPromise = null;

    loading.value = false;
  } catch (err) {
    pendingPromise = null;
    console.error('Error generating summary:', err);
    error.value = err.text || String(err);
    loading.value = false;
  }
};

/**
 * Cancels pending summary request and optionally loads new data
 * @param {boolean} runNewQuery Whether to run a new summary query after canceling
 */
const cancelAndLoad = (runNewQuery) => {
  const clientCancel = () => {
    if (pendingPromise) {
      pendingPromise.controller.abort(t('sessions.summary.canceledSearch'));
      pendingPromise = null;
    }

    if (!runNewQuery) {
      loading.value = false;
      if (!summary.value) {
        // show a page error if there is no data on the page
        error.value = t('sessions.summary.canceledSearch');
      }
      return;
    }

    generateSummary();
  };

  if (pendingPromise) {
    ConfigService.cancelEsTask(pendingPromise.cancelId).finally(() => {
      clientCancel();
    });
  } else if (runNewQuery) {
    generateSummary();
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
});

// Expose methods to parent component
defineExpose({
  reloadSummary: generateSummary
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
  margin-top: 1rem;
  display: grid;
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
</style>
