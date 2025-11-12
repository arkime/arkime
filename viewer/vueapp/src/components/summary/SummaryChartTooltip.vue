<template>
  <Teleport to="body">
    <div
      v-if="visible && data"
      class="summary-tooltip"
      :style="tooltipStyle">
      <div class="p-1">
        <!-- Header with session field dropdown and close button -->
        <div class="d-flex justify-content-between align-items-center no-wrap">
          <arkime-session-field
            :field="fieldConfig"
            :value="data.item"
            :expr="fieldConfig.exp"
            :parse="true"
            :session-btn="true"
            class="flex-grow-1 mb-1" />
          <button
            class="btn-close btn-xs"
            @click="$emit('close')" />
        </div>

        <!-- Statistics in compact grid format -->
        <div class="d-flex gap-1">
          <div class="stat-card p-1">
            <div class="stat-label">
              Sessions
            </div>
            <div class="stat-value">
              {{ formatNumber(data.sessions) }}
            </div>
          </div>
          <div class="stat-card p-1">
            <div class="stat-label">
              Packets
            </div>
            <div class="stat-value">
              {{ formatNumber(data.packets) }}
            </div>
          </div>
          <div class="stat-card p-1">
            <div class="stat-label">
              Bytes
            </div>
            <div class="stat-value">
              {{ formatBytes(data.bytes) }}
            </div>
          </div>
          <div
            v-if="percentage !== null"
            class="stat-card p-1">
            <div class="stat-label">
              Sessions %
            </div>
            <div class="stat-value">
              {{ percentage.toFixed(1) }}%
            </div>
          </div>
        </div>
      </div>
    </div>
  </Teleport>
</template>

<script setup>
import { computed } from 'vue';
import { commaString, humanReadableBytes } from '@common/vueFilters.js';
import ArkimeSessionField from '../sessions/SessionField.vue';

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
    required: true
  },
  position: {
    type: Object,
    default: () => ({ x: 0, y: 0 })
  },
  percentage: {
    type: Number,
    default: null
  }
});

defineEmits(['close']);

const tooltipStyle = computed(() => {
  return {
    left: `${props.position.x}px`,
    top: `${props.position.y}px`
  };
});

const formatNumber = (num) => {
  return commaString(num || 0);
};

const formatBytes = (bytes) => {
  return humanReadableBytes(bytes || 0);
};
</script>

<style scoped>
.summary-tooltip {
  position: fixed;
  z-index: 9999;
  background: var(--color-quaternary-lightest);
  border: 1px solid var(--color-gray);
  border-radius: 4px;
  box-shadow: 0 4px 12px rgba(0, 0, 0, 0.3);
  padding: 0;
  min-width: 150px;
  max-width: 250px;
  pointer-events: auto;
}

.stat-card {
  background: var(--color-quaternary);
  border-radius: 4px;
  text-align: center;
}

.stat-label {
  font-size: 10px;
  color: var(--color-white);
  margin-bottom: 2px;
  font-weight: 500;
  text-transform: uppercase;
}

.stat-value {
  font-size: 13px;
  font-weight: 600;
  word-break: break-word;
}
</style>
