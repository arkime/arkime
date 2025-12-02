<template>
  <Teleport to="body">
    <div
      v-if="visible && data"
      class="summary-tooltip"
      :style="tooltipStyle">
      <div class="tooltip-content">
        <!-- Header with session field dropdown and close button -->
        <div class="d-flex justify-content-between align-items-top">
          <arkime-session-field
            :field="fieldConfig"
            :value="data.item"
            :expr="fieldConfig.exp"
            :parse="true"
            :session-btn="true"
            class="flex-grow-1"
            style="white-space: break-spaces;" />
          <button
            class="btn-close btn-xs"
            @click="$emit('close')" />
        </div>

        <!-- Statistics in vertical format -->
        <div class="stats-container">
          <div class="stat-row">
            <span class="stat-label">{{ $t('sessions.summary.sessions') }}:</span>
            <span class="stat-value">{{ formatNumber(data.sessions) }}</span>
          </div>
          <div class="stat-row">
            <span class="stat-label">{{ $t('sessions.summary.packets') }}:</span>
            <span class="stat-value">{{ formatNumber(data.packets) }}</span>
          </div>
          <div class="stat-row">
            <span class="stat-label">{{ $t('sessions.summary.bytes') }}:</span>
            <span class="stat-value">{{ formatBytes(data.bytes) }}</span>
          </div>
          <div
            v-if="percentage !== null"
            class="stat-row">
            <span class="stat-label">{{ percentageLabel }}:</span>
            <span class="stat-value">{{ percentage.toFixed(1) }}%</span>
          </div>
        </div>
      </div>
    </div>
  </Teleport>
</template>

<script setup>
import { computed } from 'vue';
import { useI18n } from 'vue-i18n';
import { commaString, humanReadableBytes } from '@common/vueFilters.js';
import ArkimeSessionField from '../sessions/SessionField.vue';

const { t } = useI18n();

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
    default: 'sessions',
    validator: (value) => ['sessions', 'packets', 'bytes'].includes(value)
  }
});

defineEmits(['close']);

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

const formatBytes = (bytes) => {
  return humanReadableBytes(bytes || 0);
};

const percentageLabel = computed(() => {
  const key = `sessions.summary.${props.metricType}Percent`;
  return t(key);
});
</script>

<style scoped>
.summary-tooltip {
  position: fixed;
  z-index: 9999;
  background: var(--color-quaternary-lightest);
  border: 1px solid var(--color-gray);
  border-radius: 3px;
  box-shadow: 0 2px 8px rgba(0, 0, 0, 0.3);
  padding: 0;
  pointer-events: auto;
  white-space: nowrap;
  max-width: 250px;
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

.btn-close.btn-xs {
  font-size: 10px;
  padding: 2px 3px;
  margin-left: 6px;
}
</style>
