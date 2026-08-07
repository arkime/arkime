<template>
  <WidgetCard
    :title="title"
    :loading="loading"
    :error="error"
    :has-data="cardHasData"
    :show-export="showExport"
    :export-label="exportLabel"
    :empty-text="emptyText"
    :no-data-message="noDataMessage"
    :info-items="infoItems"
    @edit="$emit('edit')"
    @export="$emit('export', svgId)"
    @remove="$emit('remove-field')"
    @retry="$emit('retry-field', field)">
    <div
      ref="chartContainerRef"
      class="chart-content"
      :class="{ 'chart-content--scroll': viewMode === 'table' }">
      <!-- Pie Chart -->
      <SummaryPieChart
        v-if="viewMode === 'pie'"
        :data="data"
        :svg-id="svgId"
        :field-config="fieldConfig"
        :width="chartWidth"
        :height="chartHeight"
        :metric-type="metricType"
        :color-scheme="colorScheme"
        @show-tooltip="$emit('show-tooltip', $event)" />

      <!-- Bar Chart -->
      <SummaryBarChart
        v-else-if="viewMode === 'bar'"
        :data="data"
        :svg-id="svgId"
        :field-config="fieldConfig"
        :width="chartWidth"
        :height="chartHeight"
        :metric-type="metricType"
        :color-scheme="colorScheme"
        @show-tooltip="$emit('show-tooltip', $event)" />

      <!-- Table -->
      <SummaryTable
        v-else
        :data="data"
        :columns="columns"
        :field-config="fieldConfig" />
    </div>
  </WidgetCard>
</template>

<script setup>
import { computed, ref, watch, onMounted, onBeforeUnmount, nextTick } from 'vue';
import { useI18n } from 'vue-i18n';
import WidgetCard from './widgets/WidgetCard.vue';
import SummaryPieChart from './SummaryPieChart.vue';
import SummaryBarChart from './SummaryBarChart.vue';
import SummaryTable from './SummaryTable.vue';
import FieldService from '../search/FieldService';
import Utils from '../utils/utils';
import { metricIsBytes } from './widgets/widgetData';

const { t } = useI18n();

// Chart dimension constants
const MIN_CHART_SIZE = 400;
const RESIZE_DEBOUNCE_MS = 500;

// Debounce helper for resize events
const debounce = (fn, delay) => {
  let timeoutId;
  return (...args) => {
    clearTimeout(timeoutId);
    timeoutId = setTimeout(() => fn(...args), delay);
  };
};

// Generate unique SVG ID for this widget instance
const svgId = `chart-${Utils.createRandomString()}`;

const props = defineProps({
  title: {
    type: String,
    required: true
  },
  noDataMessage: {
    type: String,
    default: 'sessions.summary.noDataAvailable'
  },
  loading: {
    type: Boolean,
    default: false
  },
  error: {
    type: String,
    default: null
  },
  showExport: {
    type: Boolean,
    default: true
  },
  viewMode: {
    type: String,
    default: 'pie',
    validator: (value) => ['pie', 'bar', 'table'].includes(value)
  },
  metricType: {
    type: String,
    default: 'sessions'
  },
  // Data and visualization props
  data: {
    type: Array,
    default: () => []
  },
  field: {
    type: String,
    required: true
  },
  colorScheme: {
    type: String,
    default: 'rainbow'
  },
  infoItems: {
    type: Array,
    default: () => []
  }
});

defineEmits(['export', 'show-tooltip', 'remove-field', 'retry-field', 'edit']);

// Fetch field configuration from FieldService
const fieldConfig = computed(() => {
  return FieldService.getField(props.field, true);
});

// Check if field configuration is valid
const hasValidField = computed(() => {
  return fieldConfig.value !== null && fieldConfig.value !== undefined;
});

// Whether the card should render chart content (valid field + has rows)
const cardHasData = computed(() => {
  return hasValidField.value && Array.isArray(props.data) && props.data.length > 0;
});

// Message shown in the card's empty state for unconfigured/invalid fields
const emptyText = computed(() => {
  if (hasValidField.value) { return ''; } // fall back to the default no-data message
  return props.field
    ? `${t('sessions.summary.invalidField')}: ${props.field}`
    : t('sessions.summary.configureWidget');
});

// Export menu label depends on the view mode (CSV for tables, PNG for charts)
const exportLabel = computed(() => {
  return props.viewMode === 'table'
    ? t('sessions.summary.downloadCSV')
    : t('sessions.summary.downloadPNG');
});

// The selected metric's column label + formatting (mirrors the chart tooltip)
const metricLabel = computed(() => {
  const m = props.metricType || 'sessions';
  if (m === 'sessions') { return t('sessions.summary.sessions'); }
  return FieldService.getField(m, true)?.friendlyName || m;
});

// Table columns: the grouping field + the selected metric (single metric for now;
// a future tableColumns[] enables multi-metric). Metric value is data[].value.
const columns = computed(() => [
  {
    key: 'item',
    header: fieldConfig.value?.friendlyName || props.field,
    align: 'left',
    useSessionField: true,
    ...(fieldConfig.value?.exp && { expr: fieldConfig.value.exp })
  },
  { key: 'value', header: metricLabel.value, align: 'end', format: metricIsBytes(props.metricType) ? 'bytes' : 'number' }
]);

// ResizeObserver for dynamic chart sizing
const chartContainerRef = ref(null);
const containerWidth = ref(MIN_CHART_SIZE);
const containerHeight = ref(MIN_CHART_SIZE);
let resizeObserver = null;

// Computed chart dimensions with minimum constraints
const chartWidth = computed(() => Math.max(MIN_CHART_SIZE, containerWidth.value));
const chartHeight = computed(() => Math.max(MIN_CHART_SIZE, containerHeight.value));

// Handle container resize
const handleResize = debounce((entries) => {
  const entry = entries[0];
  if (entry) {
    containerWidth.value = entry.contentRect.width;
    containerHeight.value = entry.contentRect.height;
  }
}, RESIZE_DEBOUNCE_MS);

// Cleanup ResizeObserver
const cleanupResizeObserver = () => {
  if (resizeObserver) {
    resizeObserver.disconnect();
    resizeObserver = null;
  }
};

// Setup ResizeObserver when chart container becomes available
const setupResizeObserver = () => {
  nextTick(() => {
    if (chartContainerRef.value) {
      // Clean up any existing observer before creating new one
      cleanupResizeObserver();
      resizeObserver = new ResizeObserver(handleResize);
      resizeObserver.observe(chartContainerRef.value);
      // Initialize with current size
      containerWidth.value = chartContainerRef.value.clientWidth || MIN_CHART_SIZE;
      containerHeight.value = chartContainerRef.value.clientHeight || MIN_CHART_SIZE;
    }
  });
};

// Manage ResizeObserver lifecycle as the chart content appears/disappears
watch(() => props.loading, (isLoading) => {
  if (isLoading) {
    cleanupResizeObserver();
  } else {
    setupResizeObserver();
  }
});
watch(cardHasData, (has) => {
  if (has) { setupResizeObserver(); } else { cleanupResizeObserver(); }
});

// Setup on mount if not loading
onMounted(() => {
  if (!props.loading) {
    setupResizeObserver();
  }
});

// Cleanup on unmount
onBeforeUnmount(cleanupResizeObserver);
</script>

<style scoped>
.chart-content {
  flex: 1;
  display: flex;
  flex-direction: column;
  min-height: 0;       /* allow the body to shrink within its grid cell */
  overflow: hidden;    /* charts fit their container, never scroll */
}

/* Only table widgets scroll internally (e.g. Top 50) */
.chart-content--scroll {
  overflow-y: auto;
  overflow-x: hidden;
}
</style>
