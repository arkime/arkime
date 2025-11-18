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

            <b-dropdown-item @click="$emit('export')">
              <span class="fa fa-download" /> {{ viewMode === 'table' ? $t('sessions.summary.downloadCSV') : $t('sessions.summary.downloadPNG') }}
            </b-dropdown-item>
          </template>
        </b-dropdown>
      </div>
    </div>

    <!-- Content or empty state -->
    <div v-if="hasData">
      <!-- Pie Chart -->
      <SummaryPieChart
        v-if="viewMode === 'pie'"
        :data="data"
        :svg-id="svgId"
        :color-scheme="colorScheme"
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
        :color-scheme="colorScheme"
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
  columns: {
    type: Array,
    default: () => []
  },
  fieldConfig: {
    type: Object,
    default: null
  },
  svgId: {
    type: String,
    default: 'chartSvg'
  },
  colorScheme: {
    type: String,
    default: 'schemeCategory10'
  }
});

// Computed hasData - check if data array has items
const hasData = computed(() => {
  return props.data && Array.isArray(props.data) && props.data.length > 0;
});

// Computed chart dimensions - larger for > 20 items
const chartWidth = computed(() => {
  return props.data.length > 20 ? 600 : 400;
});

const chartHeight = computed(() => {
  return props.data.length > 20 ? 600 : 400;
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
