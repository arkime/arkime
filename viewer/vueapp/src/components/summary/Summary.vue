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
          :key="widget.id"
          :title="widget.title"
          :data="widget.data"
          :no-data-message="widget.noDataMessage"
          :view-mode="widget.viewMode.value"
          :metric-type="widget.metricType.value"
          :columns="widget.columns"
          :field-config="widget.fieldConfig"
          :svg-id="widget.svgId"
          :color-scheme="widget.colorScheme"
          @change-mode="widget.viewMode.value = $event"
          @change-metric="widget.metricType.value = $event"
          @show-tooltip="showTooltip"
          @export="handleWidgetExport(widget)" />
      </div> <!-- /charts-container -->
    </div>
  </div>
</template>

<script setup>
// external dependencies
import { ref, onMounted, onBeforeUnmount, watch, nextTick, computed } from 'vue';
import { useRoute } from 'vue-router';
import { useStore } from 'vuex';
import { useI18n } from 'vue-i18n';
// internal dependencies
import SessionsService from '../sessions/SessionsService';
import SummaryWidget from './SummaryWidget.vue';
import SummaryChartTooltip from './SummaryChartTooltip.vue';
import ArkimeLoading from '../utils/Loading.vue';
import ConfigService from '../utils/ConfigService';
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

// Shared tooltip state
const tooltipVisible = ref(false);
const tooltipData = ref(null);
const tooltipPosition = ref({ x: 0, y: 0 });
const tooltipPercentage = ref(null);
const tooltipFieldConfig = ref(null);
const tooltipMetricType = ref('sessions');

// View mode state for each section
// TODO these will come from the user configuration
const protocolsViewMode = ref('pie');
const tagsViewMode = ref('pie');
const dnsViewMode = ref('table');
const httpViewMode = ref('table');
const ipsViewMode = ref('bar');
const portsViewMode = ref('bar');

// Metric type state for each section (sessions, packets, bytes)
// TODO these will come from the user configuration
const protocolsMetricType = ref('sessions');
const tagsMetricType = ref('sessions');
const dnsMetricType = ref('sessions');
const httpMetricType = ref('sessions');
const ipsMetricType = ref('sessions');
const portsMetricType = ref('sessions');

// Save SVG as PNG will be loaded lazily (for export functionality)
let saveSvgAsPng;
let d3;

// Chart dimension constants for export functionality
const CHART_CONSTANTS = {
  PIE_CHART: {
    WIDTH: 400,
    HEIGHT: 400
  },
  LEGEND: {
    WIDTH: 250,
    ITEM_HEIGHT: 25,
    BASE_PADDING: 50
  }
};

// Field configurations for session fields
const FIELD_CONFIGS = {
  DNS_HOST: {
    friendlyName: 'DNS Host',
    exp: 'host.dns',
    dbField: 'host.dns'
  },
  HTTP_HOST: {
    friendlyName: 'HTTP Host',
    exp: 'host.http',
    dbField: 'host.http'
  }
};

// Column factory function
const createColumns = (itemHeader, expr = null) => [
  {
    key: 'item',
    header: itemHeader,
    align: 'left',
    useSessionField: true,
    ...(expr && { expr })
  },
  {
    key: 'sessions',
    header: 'Sessions',
    align: 'end',
    format: 'number'
  },
  {
    key: 'packets',
    header: 'Packets',
    align: 'end',
    format: 'number'
  },
  {
    key: 'bytes',
    header: 'Bytes',
    align: 'end',
    format: 'bytes'
  }
];

// Table column definitions
const dnsColumns = createColumns('Domain', 'host.dns');
const httpColumns = createColumns('Host', 'host.http');
const protocolColumns = createColumns('Protocols', 'protocols');
const tagColumns = createColumns('Tag', 'tags');
const ipColumns = createColumns('IP Address');
const portColumns = createColumns('Port', 'port.dst');


// Widget configurations for v-for rendering
const widgetConfigs = computed(() => [
  {
    id: 'ip-addresses',
    title: t('sessions.summary.topIPAddresses'),
    data: summary.value?.uniqueIp || [],
    noDataMessage: t('sessions.summary.noIPData'),
    viewMode: ipsViewMode,
    metricType: ipsMetricType,
    columns: ipColumns,
    fieldConfig: { friendlyName: 'IP Address', exp: 'ip', dbField: 'ip' },
    svgId: 'ipChartSvg',
    colorScheme: 'schemeCategory10',
  },
  {
    id: 'protocols',
    title: t('sessions.summary.topProtocols'),
    data: summary.value?.protocols || [],
    noDataMessage: t('sessions.summary.noProtocolData'),
    viewMode: protocolsViewMode,
    metricType: protocolsMetricType,
    columns: protocolColumns,
    fieldConfig: { friendlyName: 'Protocols', exp: 'protocols', dbField: 'protocols' },
    svgId: 'protocolChartSvg',
    colorScheme: 'schemeCategory10',
    exportDataKey: 'protocols'
  },
  {
    id: 'tags',
    title: t('sessions.summary.topTags'),
    data: summary.value?.tags || [],
    noDataMessage: t('sessions.summary.noTagData'),
    viewMode: tagsViewMode,
    metricType: tagsMetricType,
    columns: tagColumns,
    fieldConfig: { friendlyName: 'Tags', exp: 'tags', dbField: 'tags' },
    svgId: 'tagsChartSvg',
    colorScheme: 'schemePaired',
    exportDataKey: 'tags'
  },
  {
    id: 'dns-queries',
    title: t('sessions.summary.topDNSQueries'),
    data: summary.value?.dnsQueryHost || [],
    noDataMessage: t('sessions.summary.noDNSData'),
    viewMode: dnsViewMode,
    metricType: dnsMetricType,
    columns: dnsColumns,
    fieldConfig: FIELD_CONFIGS.DNS_HOST,
    svgId: 'dnsChartSvg',
    colorScheme: 'schemeCategory10',
    exportDataKey: 'dnsQueryHost'
  },
  {
    id: 'http-hosts',
    title: t('sessions.summary.topHTTPHosts'),
    data: summary.value?.httpHost || [],
    noDataMessage: t('sessions.summary.noHTTPData'),
    viewMode: httpViewMode,
    metricType: httpMetricType,
    columns: httpColumns,
    fieldConfig: FIELD_CONFIGS.HTTP_HOST,
    svgId: 'httpChartSvg',
    colorScheme: 'schemeCategory10',
    exportDataKey: 'httpHost'
  },
  {
    id: 'ports',
    title: t('sessions.summary.topPorts'),
    data: summary.value?.uniqueTcpDstPorts || [],
    noDataMessage: t('sessions.summary.noPortData'),
    viewMode: portsViewMode,
    metricType: portsMetricType,
    columns: portColumns,
    fieldConfig: { friendlyName: 'TCP Port', exp: 'port.dst', dbField: 'port.dst' },
    svgId: 'portsChartSvg',
    colorScheme: 'schemeSet2',
  }
]);

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

    // Load D3 for export functionality
    if (!d3) {
      d3 = await import('d3');
    }

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
    ConfigService.cancelEsTask(pendingPromise.cancelId).then((response) => {
      clientCancel();
    }).catch(() => {
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

    // Check if this is a pie chart that needs a legend
    const isPieChart = svgId === 'protocolChartSvg' || svgId === 'tagsChartSvg';

    if (isPieChart) {
      // Get the data and color scheme for this chart
      const data = svgId === 'protocolChartSvg'
        ? summary.value.protocols
        : summary.value.tags;
      const colorScheme = svgId === 'protocolChartSvg'
        ? d3.schemeCategory10
        : d3.schemePaired;

      // Clone the SVG
      const svgClone = svgElement.cloneNode(true);

      // Get original dimensions
      const originalWidth = CHART_CONSTANTS.PIE_CHART.WIDTH;
      const originalHeight = CHART_CONSTANTS.PIE_CHART.HEIGHT;
      const legendWidth = CHART_CONSTANTS.LEGEND.WIDTH;
      const newWidth = originalWidth + legendWidth;

      // Calculate height based on legend items
      const legendHeight = CHART_CONSTANTS.LEGEND.BASE_PADDING + (data.length * CHART_CONSTANTS.LEGEND.ITEM_HEIGHT);
      const newHeight = Math.max(originalHeight, legendHeight);

      // Create a wrapper SVG with legend
      const wrapperSvg = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
      wrapperSvg.setAttribute('width', newWidth);
      wrapperSvg.setAttribute('height', newHeight);
      wrapperSvg.setAttribute('xmlns', 'http://www.w3.org/2000/svg');

      // Add background
      const bgRect = document.createElementNS('http://www.w3.org/2000/svg', 'rect');
      bgRect.setAttribute('width', '100%');
      bgRect.setAttribute('height', '100%');
      bgRect.setAttribute('fill', backgroundColor);
      wrapperSvg.appendChild(bgRect);

      // Add the original chart
      const chartGroup = document.createElementNS('http://www.w3.org/2000/svg', 'g');
      while (svgClone.firstChild) {
        chartGroup.appendChild(svgClone.firstChild);
      }
      wrapperSvg.appendChild(chartGroup);

      // Add legend
      const legendGroup = document.createElementNS('http://www.w3.org/2000/svg', 'g');
      legendGroup.setAttribute('transform', `translate(${originalWidth + 20}, 20)`);

      // Legend title
      const legendTitle = document.createElementNS('http://www.w3.org/2000/svg', 'text');
      legendTitle.setAttribute('x', '0');
      legendTitle.setAttribute('y', '0');
      legendTitle.setAttribute('font-size', '14');
      legendTitle.setAttribute('font-weight', 'bold');
      legendTitle.setAttribute('fill', foregroundColor);
      legendTitle.textContent = 'Legend';
      legendGroup.appendChild(legendTitle);

      // Add legend items
      data.forEach((item, i) => {
        const yPos = CHART_CONSTANTS.LEGEND.ITEM_HEIGHT + (i * CHART_CONSTANTS.LEGEND.ITEM_HEIGHT);

        // Color box
        const rect = document.createElementNS('http://www.w3.org/2000/svg', 'rect');
        rect.setAttribute('x', '0');
        rect.setAttribute('y', yPos);
        rect.setAttribute('width', '15');
        rect.setAttribute('height', '15');
        rect.setAttribute('fill', colorScheme[i % colorScheme.length]);
        rect.setAttribute('stroke', 'white');
        rect.setAttribute('stroke-width', '1');
        legendGroup.appendChild(rect);

        // Label text
        const text = document.createElementNS('http://www.w3.org/2000/svg', 'text');
        text.setAttribute('x', '20');
        text.setAttribute('y', yPos + 12);
        text.setAttribute('font-size', '12');
        text.setAttribute('fill', foregroundColor);
        text.textContent = `${item.item}: ${formatNumber(item.sessions)}`;
        legendGroup.appendChild(text);
      });

      wrapperSvg.appendChild(legendGroup);

      // Add to document temporarily
      document.body.appendChild(wrapperSvg);

      // Export the wrapper SVG
      await saveSvgAsPng.saveSvgAsPng(
        wrapperSvg,
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

      // Cleanup
      document.body.removeChild(wrapperSvg);
    } else {
      // Export non-pie charts as-is
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
    }
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
const handleWidgetExport = (widget) => {
  const exportData = widget.exportDataKey
    ? summary.value?.[widget.exportDataKey]
    : widget.data;

  const filename = widget.id;
  const itemLabel = widget.fieldConfig.friendlyName;

  if (widget.viewMode.value === 'table') {
    const headers = [itemLabel, 'Sessions', 'Packets', 'Bytes'];
    exportTableCSV(exportData, headers, `arkime-summary-${filename}.csv`);
  } else {
    exportChart(widget.svgId, filename);
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

.chart-section {
  background: var(--color-quaternary-lightest);
  padding: 1rem;
  border-radius: 8px;
  box-shadow: 0 2px 4px rgba(0,0,0,0.1);
}

.chart {
  min-height: 300px;
  display: flex;
  justify-content: center;
  align-items: center;
}

.chart-pie {
  min-height: 400px;
}

.table th {
  position: sticky;
  top: 0;
}

.summary-popup {
  position: fixed;
  right: 10px;
  top: 125px;
  z-index: 1000;
  max-width: 300px;
}

.empty-state {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  min-height: 300px;
  padding: 2rem;
}

.empty-state-text {
  font-size: 1.1rem;
  margin: 0;
}
</style>
