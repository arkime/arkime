<template>
  <div class="chart-section">
    <!-- Header with title, view mode selector, and export button -->
    <div class="d-flex justify-content-end align-items-center mb-2">
      <h4 class="flex-grow-1">
        {{ title }}
      </h4>
      <div class="no-wrap">
        <!-- Consolidated Settings Dropdown -->
        <b-dropdown
          v-if="hasData"
          size="sm"
          variant="outline-secondary"
          class="d-inline-block"
          no-caret>
          <template #button-content>
            <span
              class="fa fa-gear"
              title="Settings" />
          </template>

          <!-- View Mode Options -->
          <template v-if="enableViewMode">
            <b-dropdown-item
              :active="viewMode === 'pie'"
              @click="$emit('change-mode', 'pie')">
              <span><span class="fa fa-pie-chart" /> {{ $t('sessions.summary.pieChart') }}</span>
            </b-dropdown-item>

            <b-dropdown-item
              :active="viewMode === 'bar'"
              @click="$emit('change-mode', 'bar')">
              <span><span class="fa fa-bar-chart" /> {{ $t('sessions.summary.barChart') }}</span>
            </b-dropdown-item>

            <b-dropdown-item
              :active="viewMode === 'table'"
              @click="$emit('change-mode', 'table')">
              <span><span class="fa fa-table" /> {{ $t('sessions.summary.tableView') }}</span>
            </b-dropdown-item>

            <!-- Metric Selector Options (only for charts, not table) -->
            <template v-if="viewMode !== 'table'">
              <b-dropdown-divider />

              <b-dropdown-item
                :active="metricType === 'sessions'"
                @click="$emit('change-metric', 'sessions')">
                <span>{{ $t('sessions.summary.sessions') }}</span>
              </b-dropdown-item>

              <b-dropdown-item
                :active="metricType === 'packets'"
                @click="$emit('change-metric', 'packets')">
                <span>{{ $t('sessions.summary.packets') }}</span>
              </b-dropdown-item>

              <b-dropdown-item
                :active="metricType === 'bytes'"
                @click="$emit('change-metric', 'bytes')">
                <span>{{ $t('sessions.summary.bytes') }}</span>
              </b-dropdown-item>
            </template>
          </template>

          <!-- Export Option -->
          <template v-if="showExport">
            <b-dropdown-divider v-if="enableViewMode" />

            <b-dropdown-item @click="$emit('export', svgId)">
              <span class="fa fa-download" /> {{ viewMode === 'table' ? $t('sessions.summary.downloadCSV') : $t('sessions.summary.downloadPNG') }}
            </b-dropdown-item>
          </template>
        </b-dropdown>
      </div>
    </div>

    <!-- Content, error, or empty state -->
    <div
      v-if="!hasValidField"
      class="empty-state">
      <span class="fa fa-exclamation-triangle fa-4x mb-3 text-danger" />
      <p class="empty-state-text text-danger">
        Invalid field: {{ field }}
      </p>
    </div>
    <div
      v-else-if="hasData"
      ref="chartContainerRef"
      class="chart-content">
      <!-- Pie Chart -->
      <SummaryPieChart
        v-if="viewMode === 'pie'"
        :data="data"
        :svg-id="svgId"
        :field-config="fieldConfig"
        :width="chartWidth"
        :height="chartHeight"
        :metric-type="metricType"
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
        @show-tooltip="$emit('show-tooltip', $event)" />

      <!-- Table -->
      <SummaryTable
        v-else
        :data="data"
        :columns="columns"
        :field-config="fieldConfig" />
    </div>
    <div
      v-else
      class="empty-state">
      <span class="fa fa-folder-open fa-4x mb-3 text-muted" />
      <p class="empty-state-text text-muted">
        {{ $t(noDataMessage) }}
      </p>
    </div>
  </div>
</template>

<script setup>
import { computed, ref, onMounted, onBeforeUnmount, nextTick } from 'vue';
import SummaryPieChart from './SummaryPieChart.vue';
import SummaryBarChart from './SummaryBarChart.vue';
import SummaryTable from './SummaryTable.vue';
import FieldService from '../search/FieldService';
import Utils from '../utils/utils';

// Chart dimension constants
const MIN_CHART_SIZE = 400;
const RESIZE_DEBOUNCE_MS = 100;

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
  showExport: {
    type: Boolean,
    default: true
  },
  enableViewMode: {
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
    default: 'sessions',
    validator: (value) => ['sessions', 'packets', 'bytes'].includes(value)
  },
  // Data and visualization props
  data: {
    type: Array,
    default: () => []
  },
  field: {
    type: String,
    required: true
  }
});

// Fetch field configuration from FieldService
const fieldConfig = computed(() => {
  return FieldService.getField(props.field, true);
});

// Check if field configuration is valid
const hasValidField = computed(() => {
  return fieldConfig.value !== null && fieldConfig.value !== undefined;
});

// Computed hasData - check if data array has items
const hasData = computed(() => {
  return props.data && Array.isArray(props.data) && props.data.length > 0;
});

// Generate table columns from fieldConfig
const columns = computed(() => [
  {
    key: 'item',
    header: fieldConfig.value?.friendlyName || props.field,
    align: 'left',
    useSessionField: true,
    ...(fieldConfig.value?.exp && { expr: fieldConfig.value.exp })
  },
  { key: 'sessions', header: 'Sessions', align: 'end', format: 'number' },
  { key: 'packets', header: 'Packets', align: 'end', format: 'number' },
  { key: 'bytes', header: 'Bytes', align: 'end', format: 'bytes' }
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

// Setup ResizeObserver on mount
onMounted(() => {
  nextTick(() => {
    if (chartContainerRef.value) {
      resizeObserver = new ResizeObserver(handleResize);
      resizeObserver.observe(chartContainerRef.value);
      // Initialize with current size
      containerWidth.value = chartContainerRef.value.clientWidth || MIN_CHART_SIZE;
      containerHeight.value = chartContainerRef.value.clientHeight || MIN_CHART_SIZE;
    }
  });
});

// Cleanup on unmount
onBeforeUnmount(() => {
  if (resizeObserver) {
    resizeObserver.disconnect();
    resizeObserver = null;
  }
});

defineEmits(['export', 'change-mode', 'change-metric', 'show-tooltip']);
</script>

<style scoped>
.chart-section {
  background: var(--color-quaternary-lightest);
  padding: 1rem;
  border-radius: 8px;
  box-shadow: 0 2px 4px rgba(0,0,0,0.1);
  overflow-x: hidden; /* Prevent widget from expanding page */
  display: flex;
  flex-direction: column;
  height: 100%;
  min-height: 450px;
}

.chart-content {
  flex: 1;
  display: flex;
  flex-direction: column;
  min-height: 400px;
  overflow: hidden; /* Prevent chart content from expanding container - fixes ResizeObserver loop */
}

.empty-state {
  flex: 1;
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
