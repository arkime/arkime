<template>
  <Teleport to="body">
    <div
      v-if="visible && data"
      class="summary-tooltip"
      :style="tooltipStyle">
      <div class="tooltip-content">
        <!-- Header with session field dropdown and close button -->
        <arkime-session-field
          :parse="true"
          :session-btn="true"
          :value="data.item"
          :field="fieldConfig"
          :expr="fieldConfig.exp" />
        <v-btn
          variant="text"
          size="x-small"
          density="comfortable"
          icon
          class="float-right mt-1"
          :aria-label="$t('common.close')"
          @click="$emit('close')">
          <v-icon icon="mdi-close" />
        </v-btn>

        <!-- Statistics in vertical format -->
        <div class="stats-container">
          <div class="stat-row">
            <span class="stat-label">{{ $t('sessions.summary.sessions') }}:</span>
            <span class="stat-value">{{ formatNumber(data.sessions) }}</span>
          </div>
          <!-- The widget's selected metric (when it isn't the session count) -->
          <div
            v-if="showMetricRow"
            class="stat-row">
            <span class="stat-label">{{ metricLabel }}:</span>
            <span class="stat-value">{{ formattedMetric }}</span>
          </div>
          <div
            v-if="percentage !== null"
            class="stat-row">
            <span class="stat-label">{{ $t('sessions.summary.percentOfTotal') }}:</span>
            <span class="stat-value">{{ percentage.toFixed(1) }}%</span>
          </div>
        </div>
      </div>
    </div>
  </Teleport>
</template>

<script setup>
import { computed } from 'vue';
import { commaString } from '@common/vueFilters.js';
import ArkimeSessionField from '../sessions/SessionField.vue';
import FieldService from '../search/FieldService';
import { formatMetricValue } from './widgets/widgetData';

const props = defineProps({
  visible: {
    type: Boolean,
    default: false
  },
  data: {
    type: Object,
    default: null
  },
  fieldConfig: {
    type: Object,
    default: null
  },
  position: {
    type: Object,
    default: () => ({ x: 0, y: 0 })
  },
  percentage: {
    type: Number,
    default: null
  },
  metricType: {
    type: String,
    default: 'sessions'
  }
});

defineEmits(['close']);

// The selected metric's field config (null when the metric is the session count)
const metricField = computed(() => {
  if (!props.metricType || props.metricType === 'sessions') { return null; }
  return FieldService.getField(props.metricType, true);
});

const metricLabel = computed(() => metricField.value?.friendlyName || props.metricType);

const showMetricRow = computed(() => metricField.value != null && props.data?.value != null);

const formattedMetric = computed(() => formatMetricValue(props.metricType, props.data?.value));

const tooltipStyle = computed(() => {
  // Position tooltip to the top-right of the pointer
  const offset = 1; // pixels away from pointer
  return {
    left: `${props.position.x + offset}px`,
    top: `${props.position.y - offset}px`,
    transform: 'translate(0, -100%)' // Move up by its own height
  };
});

const formatNumber = (num) => {
  return commaString(num || 0);
};
</script>

<style scoped>
.summary-tooltip {
  position: fixed;
  /* below Vuetify's overlay range (~2000+) so the session-field dropdown menu
     inside the popover renders above it, not behind */
  z-index: 1900;
  background: rgb(var(--v-theme-quaternary-lightest));
  border: 1px solid rgb(var(--v-theme-neutral));
  border-radius: 3px;
  box-shadow: 0 2px 8px rgba(0, 0, 0, 0.3);
  padding: 0;
  white-space: nowrap;
}

.tooltip-content {
  padding: 6px 8px;
}

.stats-container {
  margin-top: 4px;
  display: flex;
  flex-direction: column;
  gap: 2px;
}

.stat-row {
  display: flex;
  justify-content: space-between;
  align-items: center;
  gap: 8px;
  white-space: nowrap;
}

.stat-label {
  font-size: 11px;
  font-weight: 500;
  white-space: nowrap;
}

.stat-value {
  font-size: 11px;
  font-weight: 600;
  white-space: nowrap;
  text-align: right;
}

</style>
