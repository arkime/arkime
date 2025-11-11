<template>
  <div class="chart-section">
    <!-- Header with title, view mode selector, and export button -->
    <div class="d-flex justify-content-end align-items-center mb-2">
      <!-- <div class="d-flex align-items-center"> -->
      <h4 class="flex-grow-1">
        {{ title }}
      </h4>
      <div class="no-wrap">
        <!-- View Mode Dropdown -->
        <b-dropdown
          v-if="enableViewMode && hasData"
          size="sm"
          variant="outline-secondary"
          class="me-2 d-inline-block"
          no-caret>
          <template #button-content>
            <span
              :class="currentModeIcon"
              :title="currentModeLabel" />
          </template>

          <b-dropdown-item
            v-if="availableModes.includes('pie')"
            @click="$emit('change-mode', 'pie')">
            <span class="fa fa-pie-chart" /> {{ $t('sessions.summary.pieChart') }}
          </b-dropdown-item>

          <b-dropdown-item
            v-if="availableModes.includes('bar')"
            @click="$emit('change-mode', 'bar')">
            <span class="fa fa-bar-chart" /> {{ $t('sessions.summary.barChart') }}
          </b-dropdown-item>

          <b-dropdown-item
            v-if="availableModes.includes('table')"
            @click="$emit('change-mode', 'table')">
            <span class="fa fa-table" /> {{ $t('sessions.summary.tableView') }}
          </b-dropdown-item>
        </b-dropdown>

        <!-- Metric Selector Dropdown (only for charts, not table) -->
        <b-dropdown
          v-if="enableViewMode && hasData && viewMode !== 'table'"
          size="sm"
          variant="outline-secondary"
          class="me-2 d-inline-block"
          :text="currentMetricLabel">
          <b-dropdown-item
            :active="metricType === 'sessions'"
            @click="$emit('change-metric', 'sessions')">
            <span
              class="fa fa-check me-1"
              v-if="metricType === 'sessions'" />
            {{ $t('sessions.summary.sessions') }}
          </b-dropdown-item>

          <b-dropdown-item
            :active="metricType === 'packets'"
            @click="$emit('change-metric', 'packets')">
            <span
              class="fa fa-check me-1"
              v-if="metricType === 'packets'" />
            {{ $t('sessions.summary.packets') }}
          </b-dropdown-item>

          <b-dropdown-item
            :active="metricType === 'bytes'"
            @click="$emit('change-metric', 'bytes')">
            <span
              class="fa fa-check me-1"
              v-if="metricType === 'bytes'" />
            {{ $t('sessions.summary.bytes') }}
          </b-dropdown-item>
        </b-dropdown>

        <button
          v-if="showExport && hasData"
          class="btn btn-sm btn-theme-tertiary d-inline-block"
          :title="exportButtonLabel"
          @click="$emit('export')">
          <span class="fa fa-download" />
        </button>
      </div> <!-- /chart-controls -->
    </div>

    <!-- Optional controls slot -->
    <slot
      v-if="hasData"
      name="controls" />

    <!-- Content or empty state -->
    <div v-if="hasData">
      <!-- Pie Chart -->
      <SummaryPieChart
        v-if="viewMode === 'pie'"
        :data="data"
        :svg-id="svgId"
        :color-scheme="colorScheme"
        :field-name="fieldName"
        :field-exp="fieldExp"
        :label-font-size="labelFontSize"
        :label-radius="labelRadius"
        :width="chartWidth"
        :height="chartHeight"
        :metric-type="metricType"
        @show-tooltip="$emit('show-tooltip', $event)" />

      <!-- Bar Chart -->
      <SummaryBarChart
        v-else-if="viewMode === 'bar'"
        :data="data"
        :svg-id="svgId"
        :color-scheme="colorScheme"
        :field-name="fieldName"
        :field-exp="fieldExp"
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
        {{ noDataMessage }}
      </p>
    </div>
  </div>
</template>

<script setup>
import { computed } from 'vue';
import { useI18n } from 'vue-i18n';
import SummaryPieChart from './SummaryPieChart.vue';
import SummaryBarChart from './SummaryBarChart.vue';
import SummaryTable from './SummaryTable.vue';

const { t } = useI18n();

const props = defineProps({
  title: {
    type: String,
    required: true
  },
  hasData: {
    type: Boolean,
    required: true
  },
  noDataMessage: {
    type: String,
    required: true
  },
  showExport: {
    type: Boolean,
    default: true
  },
  enableViewMode: {
    type: Boolean,
    default: false
  },
  viewMode: {
    type: String,
    default: 'pie',
    validator: (value) => ['pie', 'bar', 'table'].includes(value)
  },
  availableModes: {
    type: Array,
    default: () => ['pie', 'bar', 'table']
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
  columns: {
    type: Array,
    default: () => []
  },
  fieldConfig: {
    type: Object,
    default: null
  },
  fieldName: {
    type: String,
    default: ''
  },
  fieldExp: {
    type: String,
    default: ''
  },
  svgId: {
    type: String,
    default: 'chartSvg'
  },
  colorScheme: {
    type: String,
    default: 'schemeCategory10'
  },
  labelFontSize: {
    type: String,
    default: '12px'
  },
  labelRadius: {
    type: Number,
    default: 40
  },
  width: {
    type: Number,
    default: 400
  },
  height: {
    type: Number,
    default: 400
  }
});

// Computed chart dimensions - larger for > 20 items
const chartWidth = computed(() => {
  return props.data.length > 20 ? 600 : props.width;
});

const chartHeight = computed(() => {
  return props.data.length > 20 ? 600 : props.height;
});

defineEmits(['export', 'change-mode', 'change-metric', 'show-tooltip']);

const currentModeLabel = computed(() => {
  switch (props.viewMode) {
  case 'pie':
    return t('sessions.summary.pieChart');
  case 'bar':
    return t('sessions.summary.barChart');
  case 'table':
    return t('sessions.summary.tableView');
  default:
    return '';
  }
});

const currentModeIcon = computed(() => {
  switch (props.viewMode) {
  case 'pie':
    return 'fa fa-pie-chart';
  case 'bar':
    return 'fa fa-bar-chart';
  case 'table':
    return 'fa fa-table';
  default:
    return '';
  }
});

const currentMetricLabel = computed(() => {
  switch (props.metricType) {
  case 'sessions':
    return t('sessions.summary.sessions');
  case 'packets':
    return t('sessions.summary.packets');
  case 'bytes':
    return t('sessions.summary.bytes');
  default:
    return t('sessions.summary.sessions');
  }
});

const exportButtonLabel = computed(() => {
  return props.viewMode === 'table'
    ? t('sessions.summary.downloadCSV')
    : t('sessions.summary.downloadPNG');
});
</script>

<style scoped>
.chart-section {
  background: var(--color-quaternary-lightest);
  padding: 1rem;
  border-radius: 8px;
  box-shadow: 0 2px 4px rgba(0,0,0,0.1);
  overflow-x: hidden; /* Prevent widget from expanding page */
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
