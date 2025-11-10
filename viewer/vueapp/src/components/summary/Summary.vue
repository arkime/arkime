<template>
  <div>
    <!-- Loading overlay -->
    <div
      v-if="loading"
      class="text-center p-5">
      <span class="fa fa-spinner fa-spin fa-3x" />
    </div>

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

      <!-- Popup area -->
      <div
        class="summary-popup"
        v-if="popupInfo">
        <popup
          :popup-info="popupInfo"
          :field-list="popupFieldList"
          @close-info="closePopup" />
      </div> <!-- /popup area -->

      <!-- Charts Grid -->
      <div class="charts-container">
        <!-- Top IP Addresses -->
        <SummaryWidget
          :title="$t('sessions.summary.topIPAddresses')"
          :has-data="hasIPData"
          :no-data-message="$t('sessions.summary.noIPData')"
          enable-view-mode
          :view-mode="ipsViewMode"
          :available-modes="['pie', 'bar', 'table']"
          :data="currentIPData"
          :columns="ipColumns"
          :field-config="currentIPFieldConfig"
          :field-name="ipFieldName"
          :field-exp="ipFieldExp"
          svg-id="ipChartSvg"
          color-scheme="schemeCategory10"
          @change-mode="ipsViewMode = $event"
          @show-popup="handlePopup"
          @export="handleExport({
            data: currentIPData,
            svgId: 'ipChartSvg',
            filename: 'ip-addresses',
            itemLabel: 'IP Address',
            mode: ipsViewMode
          })">
          <template #controls>
            <div class="chart-controls mb-2">
              <button
                :class="['btn btn-sm', ipType === 'all' ? 'btn-primary' : 'btn-outline-secondary']"
                @click="ipType = 'all'">
                {{ $t('sessions.summary.allIPs') }}
              </button>
              <button
                :class="['btn btn-sm ms-2', ipType === 'src' ? 'btn-primary' : 'btn-outline-secondary']"
                @click="ipType = 'src'">
                {{ $t('sessions.summary.sourceIPs') }}
              </button>
              <button
                :class="['btn btn-sm ms-2', ipType === 'dst' ? 'btn-primary' : 'btn-outline-secondary']"
                @click="ipType = 'dst'">
                {{ $t('sessions.summary.destinationIPs') }}
              </button>
              <button
                :class="['btn btn-sm ms-2', ipType === 'dstport' ? 'btn-primary' : 'btn-outline-secondary']"
                @click="ipType = 'dstport'">
                {{ $t('sessions.summary.destinationPortIPs') }}
              </button>
            </div>
          </template>
        </SummaryWidget>

        <!-- Top Protocols -->
        <SummaryWidget
          :title="$t('sessions.summary.topProtocols')"
          :has-data="hasProtocolData"
          :no-data-message="$t('sessions.summary.noProtocolData')"
          enable-view-mode
          :view-mode="protocolsViewMode"
          :available-modes="['pie', 'bar', 'table']"
          :data="summary.protocols"
          :columns="protocolColumns"
          :field-config="{ friendlyName: 'Protocol', exp: 'protocol', dbField: 'protocol' }"
          field-name="Protocol"
          field-exp="protocol"
          svg-id="protocolChartSvg"
          color-scheme="schemeCategory10"
          label-font-size="12px"
          :label-radius="40"
          @change-mode="protocolsViewMode = $event"
          @show-popup="handlePopup"
          @export="handleExport({
            dataKey: 'protocols',
            svgId: 'protocolChartSvg',
            filename: 'protocols',
            itemLabel: 'Protocol',
            mode: protocolsViewMode
          })" />

        <!-- Top Tags -->
        <SummaryWidget
          :title="$t('sessions.summary.topTags')"
          :has-data="hasTagData"
          :no-data-message="$t('sessions.summary.noTagData')"
          enable-view-mode
          :view-mode="tagsViewMode"
          :available-modes="['pie', 'bar', 'table']"
          :data="summary.tags"
          :columns="tagColumns"
          :field-config="{ friendlyName: 'Tags', exp: 'tags', dbField: 'tags' }"
          field-name="Tags"
          field-exp="tags"
          svg-id="tagsChartSvg"
          color-scheme="schemePaired"
          label-font-size="10px"
          :label-radius="50"
          @change-mode="tagsViewMode = $event"
          @show-popup="handlePopup"
          @export="handleExport({
            dataKey: 'tags',
            svgId: 'tagsChartSvg',
            filename: 'tags',
            itemLabel: 'Tag',
            mode: tagsViewMode
          })" />

        <!-- DNS Query Hosts -->
        <SummaryWidget
          :title="$t('sessions.summary.topDNSQueries')"
          :has-data="hasDNSData"
          :no-data-message="$t('sessions.summary.noDNSData')"
          enable-view-mode
          :view-mode="dnsViewMode"
          :available-modes="['pie', 'bar', 'table']"
          :data="summary.dnsQueryHost"
          :columns="dnsColumns"
          :field-config="FIELD_CONFIGS.DNS_HOST"
          field-name="DNS Query"
          field-exp="host.dns"
          svg-id="dnsChartSvg"
          color-scheme="schemeCategory10"
          @change-mode="dnsViewMode = $event"
          @show-popup="handlePopup"
          @export="handleExport({
            dataKey: 'dnsQueryHost',
            svgId: 'dnsChartSvg',
            filename: 'dns-queries',
            itemLabel: 'DNS Query',
            mode: dnsViewMode
          })" />

        <!-- HTTP Hosts -->
        <SummaryWidget
          :title="$t('sessions.summary.topHTTPHosts')"
          :has-data="hasHTTPData"
          :no-data-message="$t('sessions.summary.noHTTPData')"
          enable-view-mode
          :view-mode="httpViewMode"
          :available-modes="['pie', 'bar', 'table']"
          :data="summary.httpHost"
          :columns="httpColumns"
          :field-config="FIELD_CONFIGS.HTTP_HOST"
          field-name="HTTP Host"
          field-exp="host.http"
          svg-id="httpChartSvg"
          color-scheme="schemeCategory10"
          @change-mode="httpViewMode = $event"
          @show-popup="handlePopup"
          @export="handleExport({
            dataKey: 'httpHost',
            svgId: 'httpChartSvg',
            filename: 'http-hosts',
            itemLabel: 'HTTP Host',
            mode: httpViewMode
          })" />

        <!-- Top Ports -->
        <SummaryWidget
          :title="$t('sessions.summary.topPorts')"
          :has-data="hasPortData"
          :no-data-message="$t('sessions.summary.noPortData')"
          enable-view-mode
          :view-mode="portsViewMode"
          :available-modes="['pie', 'bar', 'table']"
          :data="currentPortData"
          :columns="portColumns"
          :field-config="{ friendlyName: portFieldName, exp: 'port.dst', dbField: 'port.dst' }"
          :field-name="portFieldName"
          field-exp="port.dst"
          svg-id="portsChartSvg"
          color-scheme="schemeSet2"
          @change-mode="portsViewMode = $event"
          @show-popup="handlePopup"
          @export="handleExport({
            data: currentPortData,
            svgId: 'portsChartSvg',
            filename: 'ports',
            itemLabel: 'Port',
            mode: portsViewMode
          })">
          <template #controls>
            <div class="chart-controls mb-2">
              <button
                :class="['btn btn-sm', portType === 'tcp' ? 'btn-primary' : 'btn-outline-secondary']"
                @click="portType = 'tcp'">
                {{ $t('sessions.summary.tcpPorts') }}
              </button>
              <button
                :class="['btn btn-sm ms-2', portType === 'udp' ? 'btn-primary' : 'btn-outline-secondary']"
                @click="portType = 'udp'">
                {{ $t('sessions.summary.udpPorts') }}
              </button>
            </div>
          </template>
        </SummaryWidget>
      </div> <!-- /charts-container -->
    </div>
  </div>
</template>

<script setup>
// external dependencies
import { ref, onMounted, onBeforeUnmount, watch, nextTick, computed } from 'vue';
import { useRoute } from 'vue-router';
import { useStore } from 'vuex';
// internal dependencies
import SessionsService from '../sessions/SessionsService';
import Popup from '../spigraph/Popup.vue';
import ArkimeSessionField from '../sessions/SessionField.vue';
import SummaryWidget from './SummaryWidget.vue';
import { commaString, humanReadableBytes, readableTime, timezoneDateString } from '@common/vueFilters.js';

// Access route and store
const route = useRoute();
const store = useStore();

// Computed properties
const user = computed(() => store.state.user);
const timezone = computed(() => user.value?.settings?.timezone || 'local');
const showMs = computed(() => user.value?.settings?.ms === true);

// Check if data exists for each section
const hasIPData = computed(() => {
  if (!summary.value) return false;
  const data = ipType.value === 'src' ? summary.value.uniqueSrcIp
    : ipType.value === 'dst' ? summary.value.uniqueDstIp
      : ipType.value === 'dstport' ? summary.value.uniqueDstIpPort
        : summary.value.uniqueIp;
  return data && data.length > 0;
});

const hasProtocolData = computed(() => {
  return summary.value?.protocols && summary.value.protocols.length > 0;
});

const hasTagData = computed(() => {
  return summary.value?.tags && summary.value.tags.length > 0;
});

const hasDNSData = computed(() => {
  return summary.value?.dnsQueryHost && summary.value.dnsQueryHost.length > 0;
});

const hasHTTPData = computed(() => {
  return summary.value?.httpHost && summary.value.httpHost.length > 0;
});

const hasPortData = computed(() => {
  return (summary.value?.uniqueTcpDstPorts && summary.value.uniqueTcpDstPorts.length > 0) ||
         (summary.value?.uniqueUdpDstPorts && summary.value.uniqueUdpDstPorts.length > 0);
});

// Computed properties for IP chart data and fields
const currentIPData = computed(() => {
  if (!summary.value) return [];
  switch (ipType.value) {
  case 'src':
    return summary.value.uniqueSrcIp || [];
  case 'dst':
    return summary.value.uniqueDstIp || [];
  case 'dstport':
    return summary.value.uniqueDstIpPort || [];
  default:
    return summary.value.uniqueIp || [];
  }
});

const ipFieldName = computed(() => {
  switch (ipType.value) {
  case 'src':
    return 'Source IP';
  case 'dst':
    return 'Destination IP';
  default:
    return 'IP Address';
  }
});

const ipFieldExp = computed(() => {
  switch (ipType.value) {
  case 'src':
    return 'ip.src';
  case 'dst':
    return 'ip.dst';
  default:
    return 'ip';
  }
});

const currentIPFieldConfig = computed(() => {
  return {
    friendlyName: ipFieldName.value,
    exp: ipFieldExp.value,
    dbField: ipFieldExp.value
  };
});

// Computed properties for Ports chart data
const currentPortData = computed(() => {
  if (!summary.value) return [];
  return portType.value === 'tcp'
    ? (summary.value.uniqueTcpDstPorts || [])
    : (summary.value.uniqueUdpDstPorts || []);
});

const portFieldName = computed(() => {
  return portType.value === 'tcp' ? 'TCP Port' : 'UDP Port';
});

// Reactive state
const summary = ref(null);
const loading = ref(true);
const error = ref('');
const ipType = ref('all');
const portType = ref('tcp');
const popupInfo = ref(null);
const popupFieldList = ref([]);

// View mode state for each section
const protocolsViewMode = ref('pie');
const tagsViewMode = ref('pie');
const dnsViewMode = ref('table');
const httpViewMode = ref('table');
const ipsViewMode = ref('bar');
const portsViewMode = ref('bar');

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

// Table column definitions
const dnsColumns = [
  {
    key: 'item',
    header: 'Domain',
    align: 'left',
    useSessionField: true,
    expr: 'host.dns'
  },
  {
    key: 'sessions',
    header: 'Queries',
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

const httpColumns = [
  {
    key: 'item',
    header: 'Host',
    align: 'left',
    useSessionField: true,
    expr: 'host.http'
  },
  {
    key: 'sessions',
    header: 'Requests',
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

const protocolColumns = [
  {
    key: 'item',
    header: 'Protocol',
    align: 'left'
  },
  {
    key: 'sessions',
    header: 'Sessions',
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

const tagColumns = [
  {
    key: 'item',
    header: 'Tag',
    align: 'left'
  },
  {
    key: 'sessions',
    header: 'Sessions',
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

const ipColumns = [
  {
    key: 'item',
    header: 'IP Address',
    align: 'left'
  },
  {
    key: 'sessions',
    header: 'Sessions',
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

const portColumns = [
  {
    key: 'item',
    header: 'Port',
    align: 'left'
  },
  {
    key: 'sessions',
    header: 'Sessions',
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

// Methods
const generateSummary = async () => {
  loading.value = true;
  error.value = '';

  try {
    const response = await SessionsService.generateSummary(route.query);
    summary.value = response;

    // Load D3 for export functionality
    if (!d3) {
      d3 = await import('d3');
    }

    loading.value = false;
  } catch (err) {
    console.error('Error generating summary:', err);
    error.value = err.text || String(err);
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

const showPopup = (data, fieldName, fieldExp) => {
  popupFieldList.value = [{
    friendlyName: fieldName,
    exp: fieldExp,
    dbField: fieldExp
  }];

  // Create a simple popup structure
  popupInfo.value = {
    data: {
      name: data.item || String(data.item),
      size: data.sessions,
      srcips: 0,
      dstips: 0
    },
    depth: 1,
    parent: null
  };
};

// Handle popup events from chart components
const handlePopup = (popupEvent) => {
  showPopup(popupEvent.data, popupEvent.fieldName, popupEvent.fieldExp);
};

const closePopup = () => {
  popupInfo.value = null;
};

const closePopupOnEsc = (e) => {
  if (e.key === 'Escape') {
    closePopup();
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
      csv += `${escapeCSV(item.item)},${item.sessions},${item.bytes}\n`;
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

// Unified export handler that adapts based on view mode
const handleExport = (section) => {
  const { dataKey, data, svgId, filename, itemLabel, mode } = section;

  if (mode === 'table') {
    // Export as CSV - use direct data if provided, otherwise lookup by key
    const headers = [itemLabel, 'Sessions', 'Bytes'];
    exportTableCSV(data || dataKey, headers, `arkime-summary-${filename}.csv`);
  } else {
    // Export as PNG (pie or bar chart)
    exportChart(svgId, filename);
  }
};

// Watch for route query changes to regenerate summary when search changes
watch(() => route.query, () => {
  generateSummary();
}, { deep: true });

// On mount
onMounted(() => {
  generateSummary();
  window.addEventListener('keyup', closePopupOnEsc);
});

// On unmount
onBeforeUnmount(() => {
  window.removeEventListener('keyup', closePopupOnEsc);
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
  grid-template-columns: repeat(auto-fit, minmax(500px, 1fr));
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
