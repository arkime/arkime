<template>
  <div>
    <ArkimeCollapsible>
      <span class="fixed-header">
        <!-- search navbar -->
        <arkime-search
          :start="0"
          @change-search="generateSummary"
          @recalc-collapse="$emit('recalc-collapse')" />
      </span>
    </ArkimeCollapsible>

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
      <!-- General Statistics -->
      <div class="summary-stats mb-4">
        <h3 class="mb-3">
          {{ $t('sessions.summary.captureStatistics') }}
        </h3>
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
          <div class="stat-card">
            <div class="stat-label">
              {{ $t('sessions.summary.duration') }}
            </div>
            <div class="stat-value">
              {{ formatDuration(summary.firstPacket, summary.lastPacket) }}
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
        <div class="chart-section">
          <div class="d-flex justify-content-between align-items-center mb-2">
            <h4 class="chart-title mb-0">
              {{ $t('sessions.summary.topIPAddresses') }}
            </h4>
            <button
              class="btn btn-sm btn-theme-tertiary"
              @click="exportChart('ipChartSvg', 'ip-addresses')">
              <span class="fa fa-download" />
            </button>
          </div>
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
          <div
            ref="ipChart"
            class="chart" />
        </div>

        <!-- Top Protocols -->
        <div class="chart-section">
          <div class="d-flex justify-content-between align-items-center mb-2">
            <h4 class="chart-title mb-0">
              {{ $t('sessions.summary.topProtocols') }}
            </h4>
            <button
              class="btn btn-sm btn-theme-tertiary"
              @click="exportChart('protocolChartSvg', 'protocols')">
              <span class="fa fa-download" />
            </button>
          </div>
          <div
            ref="protocolChart"
            class="chart chart-pie" />
        </div>

        <!-- Top Tags -->
        <div
          class="chart-section"
          v-if="summary.tags && summary.tags.length">
          <div class="d-flex justify-content-between align-items-center mb-2">
            <h4 class="chart-title mb-0">
              {{ $t('sessions.summary.topTags') }}
            </h4>
            <button
              class="btn btn-sm btn-theme-tertiary"
              @click="exportChart('tagsChartSvg', 'tags')">
              <span class="fa fa-download" />
            </button>
          </div>
          <div
            ref="tagsChart"
            class="chart chart-pie" />
        </div>

        <!-- DNS Query Hosts -->
        <div
          class="chart-section"
          v-if="summary.dnsQueryHost && summary.dnsQueryHost.length">
          <div class="d-flex justify-content-between align-items-center mb-2">
            <h4 class="chart-title mb-0">
              {{ $t('sessions.summary.topDNSQueries') }}
            </h4>
            <button
              class="btn btn-sm btn-theme-tertiary"
              @click="exportDNSCSV">
              <span class="fa fa-download" />
            </button>
          </div>
          <table class="table table-sm table-striped">
            <thead>
              <tr>
                <th>{{ $t('sessions.summary.domain') }}</th>
                <th class="text-end">
                  {{ $t('sessions.summary.queries') }}
                </th>
                <th class="text-end">
                  {{ $t('sessions.summary.bytes') }}
                </th>
              </tr>
            </thead>
            <tbody>
              <tr
                v-for="(item, index) in summary.dnsQueryHost"
                :key="index">
                <td>
                  <arkime-session-field
                    :field="FIELD_CONFIGS.DNS_HOST"
                    :value="item.item"
                    expr="host.dns"
                    :parse="true"
                    :session-btn="true"
                    :pull-left="true" />
                </td>
                <td class="text-end">
                  {{ formatNumber(item.sessions) }}
                </td>
                <td class="text-end">
                  {{ formatBytes(item.bytes) }}
                </td>
              </tr>
            </tbody>
          </table>
        </div>

        <!-- HTTP Hosts -->
        <div
          class="chart-section"
          v-if="summary.httpHost && summary.httpHost.length">
          <div class="d-flex justify-content-between align-items-center mb-2">
            <h4 class="chart-title mb-0">
              {{ $t('sessions.summary.topHTTPHosts') }}
            </h4>
            <button
              class="btn btn-sm btn-theme-tertiary"
              @click="exportHTTPCSV">
              <span class="fa fa-download" />
            </button>
          </div>
          <table class="table table-sm table-striped">
            <thead>
              <tr>
                <th>{{ $t('sessions.summary.host') }}</th>
                <th class="text-end">
                  {{ $t('sessions.summary.requests') }}
                </th>
                <th class="text-end">
                  {{ $t('sessions.summary.bytes') }}
                </th>
              </tr>
            </thead>
            <tbody>
              <tr
                v-for="(item, index) in summary.httpHost"
                :key="index">
                <td>
                  <arkime-session-field
                    :field="FIELD_CONFIGS.HTTP_HOST"
                    :value="item.item"
                    expr="host.http"
                    :parse="true"
                    :session-btn="true"
                    :pull-left="true" />
                </td>
                <td class="text-end">
                  {{ formatNumber(item.sessions) }}
                </td>
                <td class="text-end">
                  {{ formatBytes(item.bytes) }}
                </td>
              </tr>
            </tbody>
          </table>
        </div>

        <!-- Top Ports -->
        <div
          class="chart-section"
          v-if="summary.uniqueTcpDstPorts || summary.uniqueUdpDstPorts">
          <div class="d-flex justify-content-between align-items-center mb-2">
            <h4 class="chart-title mb-0">
              {{ $t('sessions.summary.topPorts') }}
            </h4>
            <button
              class="btn btn-sm btn-theme-tertiary"
              @click="exportChart('portsChartSvg', 'ports')">
              <span class="fa fa-download" />
            </button>
          </div>
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
          <div
            ref="portsChart"
            class="chart" />
        </div>
      </div> <!-- /charts-container -->
    </div>
  </div>
</template>

<script setup>
// external dependencies
import { ref, onMounted, onBeforeUnmount, watch, nextTick } from 'vue';
import { useRoute } from 'vue-router';
// internal dependencies
import SessionsService from '../sessions/SessionsService';
import Popup from '../spigraph/Popup.vue';
import ArkimeSessionField from '../sessions/SessionField.vue';
import ArkimeSearch from '../search/Search.vue';
import ArkimeCollapsible from '../utils/CollapsibleWrapper.vue';
import { commaString, humanReadableBytes, readableTime } from '@common/vueFilters.js';

// Define emits
defineEmits(['recalc-collapse']);

// Access route
const route = useRoute();

// Reactive state
const summary = ref(null);
const loading = ref(true);
const error = ref('');
const ipType = ref('all');
const portType = ref('tcp');
const popupInfo = ref(null);
const popupFieldList = ref([]);

// Chart refs
const ipChart = ref(null);
const protocolChart = ref(null);
const tagsChart = ref(null);
const portsChart = ref(null);

// D3 will be loaded lazily
let d3;

// Save SVG as PNG will be loaded lazily
let saveSvgAsPng;

// Popup timer for debouncing
let popupTimer;

// Chart dimension constants
const CHART_CONSTANTS = {
  BAR_CHART: {
    MARGIN: { top: 20, right: 30, bottom: 120, left: 60 },
    WIDTH: 500,
    HEIGHT: 350
  },
  PIE_CHART: {
    WIDTH: 400,
    HEIGHT: 400,
    LABEL_MAX_LENGTH: 12,
    LABEL_TRUNCATE_LENGTH: 10
  },
  LEGEND: {
    WIDTH: 250,
    ITEM_HEIGHT: 25,
    BASE_PADDING: 50
  },
  POPUP_DELAY: 400
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

// Methods
const generateSummary = async () => {
  loading.value = true;
  error.value = '';

  try {
    const response = await SessionsService.generateSummary(route.query);
    summary.value = response;

    // Load D3 and render charts
    if (!d3) {
      d3 = await import('d3');
    }

    loading.value = false;

    // Wait for DOM to fully update before rendering charts
    await nextTick();
    await nextTick(); // Double nextTick to ensure refs are available
    renderCharts();
  } catch (err) {
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

const closePopup = () => {
  popupInfo.value = null;
  if (popupTimer) {
    clearTimeout(popupTimer);
  }
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
const exportTableCSV = (dataKey, headers, filename) => {
  try {
    const data = summary.value?.[dataKey];
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

// Export DNS table as CSV
const exportDNSCSV = () => {
  exportTableCSV('dnsQueryHost', ['Domain', 'Sessions', 'Bytes'], 'arkime-summary-dns-queries.csv');
};

// Export HTTP table as CSV
const exportHTTPCSV = () => {
  exportTableCSV('httpHost', ['Host', 'Sessions', 'Bytes'], 'arkime-summary-http-hosts.csv');
};

// Helper function to create consistent hover behavior for charts
const createChartHoverHandlers = (showPopupCallback) => {
  return {
    mouseover: function (e, d) {
      d3.select(this).style('opacity', 0.7);
      if (popupTimer) clearTimeout(popupTimer);
      popupTimer = setTimeout(() => {
        showPopupCallback(d);
      }, CHART_CONSTANTS.POPUP_DELAY);
    },
    mouseleave: function () {
      d3.select(this).style('opacity', 1);
      if (popupTimer) clearTimeout(popupTimer);
    }
  };
};

// Helper function to render pie charts (shared logic for protocols and tags)
const renderPieChart = (containerRef, data, svgId, colorScheme, fieldName, fieldExp, labelFontSize = '12px', labelRadius = 40) => {
  if (!containerRef || !data) return;

  const container = d3.select(containerRef);
  container.selectAll('*').remove();

  if (!data.length) return;

  const width = CHART_CONSTANTS.PIE_CHART.WIDTH;
  const height = CHART_CONSTANTS.PIE_CHART.HEIGHT;
  const radius = Math.min(width, height) / 2;

  const svg = container.append('svg')
    .attr('id', svgId)
    .attr('width', width)
    .attr('height', height)
    .append('g')
    .attr('transform', `translate(${width / 2},${height / 2})`);

  const color = d3.scaleOrdinal(colorScheme);

  const pie = d3.pie()
    .value(d => d.sessions)
    .sort(null);

  const arc = d3.arc()
    .innerRadius(0)
    .outerRadius(radius - 10);

  const labelArc = d3.arc()
    .innerRadius(radius - labelRadius)
    .outerRadius(radius - labelRadius);

  const arcs = svg.selectAll('.arc')
    .data(pie(data))
    .enter()
    .append('g')
    .attr('class', 'arc');

  const handlers = createChartHoverHandlers((d) => {
    showPopup(d.data, fieldName, fieldExp);
  });

  arcs.append('path')
    .attr('d', arc)
    .attr('fill', (d, i) => color(i))
    .attr('stroke', 'white')
    .style('stroke-width', '2px')
    .style('cursor', 'pointer')
    .on('mouseover', handlers.mouseover)
    .on('mouseleave', handlers.mouseleave);

  arcs.append('text')
    .attr('transform', d => `translate(${labelArc.centroid(d)})`)
    .attr('text-anchor', 'middle')
    .style('font-size', labelFontSize)
    .style('fill', 'white')
    .style('font-weight', labelFontSize === '12px' ? 'bold' : 'normal')
    .text(d => {
      const itemName = d.data.item;
      // Only truncate for tags (smaller font size)
      if (labelFontSize === '10px' && itemName.length > CHART_CONSTANTS.PIE_CHART.LABEL_MAX_LENGTH) {
        return itemName.substring(0, CHART_CONSTANTS.PIE_CHART.LABEL_TRUNCATE_LENGTH) + '...';
      }
      return itemName;
    });
};

const renderCharts = () => {
  if (!summary.value || !d3) return;

  renderIPChart();
  renderProtocolChart();
  if (summary.value.tags?.length) renderTagsChart();
  if (summary.value.uniqueTcpDstPorts || summary.value.uniqueUdpDstPorts) renderPortsChart();
};

const renderIPChart = () => {
  if (!ipChart.value || !summary.value) return;

  const container = d3.select(ipChart.value);
  container.selectAll('*').remove();

  let data;
  if (ipType.value === 'src') {
    data = summary.value.uniqueSrcIp || [];
  } else if (ipType.value === 'dst') {
    data = summary.value.uniqueDstIp || [];
  } else if (ipType.value === 'dstport') {
    data = summary.value.uniqueDstIpPort || [];
  } else {
    data = summary.value.uniqueIp || [];
  }

  if (!data.length) return;

  const margin = CHART_CONSTANTS.BAR_CHART.MARGIN;
  const width = CHART_CONSTANTS.BAR_CHART.WIDTH - margin.left - margin.right;
  const height = CHART_CONSTANTS.BAR_CHART.HEIGHT - margin.top - margin.bottom;

  const svg = container.append('svg')
    .attr('id', 'ipChartSvg')
    .attr('width', width + margin.left + margin.right)
    .attr('height', height + margin.top + margin.bottom)
    .append('g')
    .attr('transform', `translate(${margin.left},${margin.top})`);

  const x = d3.scaleBand()
    .range([0, width])
    .domain(data.map(d => d.item))
    .padding(0.2);

  const y = d3.scaleLinear()
    .domain([0, d3.max(data, d => d.sessions)])
    .range([height, 0]);

  const colors = d3.scaleOrdinal(d3.schemeCategory10);

  const handlers = createChartHoverHandlers((d) => {
    const fieldName = ipType.value === 'src' ? 'Source IP' : ipType.value === 'dst' ? 'Destination IP' : 'IP Address';
    const fieldExp = ipType.value === 'src' ? 'ip.src' : ipType.value === 'dst' ? 'ip.dst' : 'ip';
    showPopup(d, fieldName, fieldExp);
  });

  svg.selectAll('.bar')
    .data(data)
    .enter()
    .append('rect')
    .attr('class', 'bar')
    .attr('x', d => x(d.item))
    .attr('width', x.bandwidth())
    .attr('y', d => y(d.sessions))
    .attr('height', d => height - y(d.sessions))
    .attr('fill', (d, i) => colors(i))
    .style('cursor', 'pointer')
    .on('mouseover', handlers.mouseover)
    .on('mouseleave', handlers.mouseleave);

  svg.append('g')
    .attr('transform', `translate(0,${height})`)
    .call(d3.axisBottom(x))
    .selectAll('text')
    .attr('transform', 'rotate(-45)')
    .style('text-anchor', 'end')
    .style('font-size', '10px');

  svg.append('g')
    .call(d3.axisLeft(y));

  svg.selectAll('.label')
    .data(data)
    .enter()
    .append('text')
    .attr('class', 'label')
    .attr('x', d => x(d.item) + x.bandwidth() / 2)
    .attr('y', d => y(d.sessions) - 5)
    .attr('text-anchor', 'middle')
    .style('font-size', '11px')
    .text(d => d.sessions);
};

const renderProtocolChart = () => {
  if (!protocolChart.value || !summary.value?.protocols) return;
  renderPieChart(
    protocolChart.value,
    summary.value.protocols,
    'protocolChartSvg',
    d3.schemeCategory10,
    'Protocol',
    'protocol',
    '12px',
    40
  );
};

const renderTagsChart = () => {
  if (!tagsChart.value || !summary.value?.tags) return;
  renderPieChart(
    tagsChart.value,
    summary.value.tags,
    'tagsChartSvg',
    d3.schemePaired,
    'Tags',
    'tags',
    '10px',
    50
  );
};

const renderPortsChart = () => {
  if (!portsChart.value || !summary.value) return;

  const container = d3.select(portsChart.value);
  container.selectAll('*').remove();

  const data = portType.value === 'tcp'
    ? (summary.value.uniqueTcpDstPorts || [])
    : (summary.value.uniqueUdpDstPorts || []);

  if (!data.length) return;

  const margin = CHART_CONSTANTS.BAR_CHART.MARGIN;
  const width = CHART_CONSTANTS.BAR_CHART.WIDTH - margin.left - margin.right;
  const height = CHART_CONSTANTS.BAR_CHART.HEIGHT - margin.top - margin.bottom;

  const svg = container.append('svg')
    .attr('id', 'portsChartSvg')
    .attr('width', width + margin.left + margin.right)
    .attr('height', height + margin.top + margin.bottom)
    .append('g')
    .attr('transform', `translate(${margin.left},${margin.top})`);

  const x = d3.scaleBand()
    .range([0, width])
    .domain(data.map(d => d.item))
    .padding(0.2);

  const y = d3.scaleLinear()
    .domain([0, d3.max(data, d => d.sessions)])
    .range([height, 0]);

  const colors = d3.scaleOrdinal(d3.schemeSet2);

  const handlers = createChartHoverHandlers((d) => {
    const fieldName = portType.value === 'tcp' ? 'TCP Port' : 'UDP Port';
    const fieldExp = 'port.dst'; // Same for both TCP and UDP
    showPopup(d, fieldName, fieldExp);
  });

  svg.selectAll('.bar')
    .data(data)
    .enter()
    .append('rect')
    .attr('class', 'bar')
    .attr('x', d => x(d.item))
    .attr('width', x.bandwidth())
    .attr('y', d => y(d.sessions))
    .attr('height', d => height - y(d.sessions))
    .attr('fill', (d, i) => colors(i))
    .style('cursor', 'pointer')
    .on('mouseover', handlers.mouseover)
    .on('mouseleave', handlers.mouseleave);

  svg.append('g')
    .attr('transform', `translate(0,${height})`)
    .call(d3.axisBottom(x))
    .selectAll('text')
    .attr('transform', 'rotate(-45)')
    .style('text-anchor', 'end')
    .style('font-size', '10px');

  svg.append('g')
    .call(d3.axisLeft(y));

  svg.selectAll('.label')
    .data(data)
    .enter()
    .append('text')
    .attr('class', 'label')
    .attr('x', d => x(d.item) + x.bandwidth() / 2)
    .attr('y', d => y(d.sessions) - 5)
    .attr('text-anchor', 'middle')
    .style('font-size', '11px')
    .text(d => d.sessions);
};

// Watch for changes
watch(ipType, () => {
  if (d3 && summary.value) renderIPChart();
});

watch(portType, () => {
  if (d3 && summary.value) renderPortsChart();
});

// On mount
onMounted(() => {
  generateSummary();
  window.addEventListener('keyup', closePopupOnEsc);
});

// On unmount
onBeforeUnmount(() => {
  window.removeEventListener('keyup', closePopupOnEsc);
  if (popupTimer) {
    clearTimeout(popupTimer);
  }
});
</script>

<style scoped>
.summary-stats {
  padding: 1rem;
  border-radius: 8px;
  background: var(--color-quaternary-lightest);
}

.stats-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
  gap: .5rem;
}

.stat-card {
  padding: 0.2rem;
  border-radius: 6px;
  text-align: center;
  border: 1px solid var(--color-gray);
  background: var(--color-background);
}

.stat-label {
  font-size: 0.875rem;
}

.stat-value {
  font-size: 1.5rem;
  font-weight: bold;
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
</style>
