<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <Teleport to="body">
    <div
      v-if="visible && data"
      class="summary-tooltip"
      :style="tooltipStyle">
      <div class="tooltip-content">
        <!-- Header with session field dropdown and close button. "Other" is no
             value of the field and a range bucket ('0-9') no single value, so
             neither gets field actions -->
        <strong v-if="data.other || data.range">{{ data.item }}</strong>
        <arkime-session-field
          v-else
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
          <!-- the split (second field) value for a bar segment -->
          <div
            v-if="split"
            class="stat-row">
            <span class="stat-label">{{ split.label }}:</span>
            <span class="stat-value">{{ split.value }}</span>
          </div>
          <div class="stat-row">
            <span class="stat-label">{{ $t('sessions.summary.sessions') }}:</span>
            <!-- a count that isn't known (a matrix pair outside its row's top values) says why -->
            <span class="stat-value">{{ data.sessions == null && data.note ? data.note : formatNumber(data.sessions) }}</span>
          </div>
          <!-- a chart of several metrics (combo, scatter) names each one -->
          <div
            v-for="(row, i) in metricRows"
            :key="i"
            class="stat-row">
            <span class="stat-label">{{ row.label }}:</span>
            <span class="stat-value">{{ row.value }}</span>
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
        <!-- a dashboard control of this field takes the value as a pick -->
        <v-btn
          v-if="pickVariable && !alreadyPicked"
          size="small"
          variant="tonal"
          color="primary"
          prepend-icon="mdi-filter-plus-outline"
          class="mt-2"
          @click="pickValue">
          {{ $t('sessions.summary.controlPick') }}
        </v-btn>
      </div>
    </div>
  </Teleport>
</template>

<script setup>
import { computed } from 'vue';
import { commaString } from '@common/vueFilters.js';
import ArkimeSessionField from '../sessions/SessionField.vue';
import FieldService from '../search/FieldService';
import { useI18n } from 'vue-i18n';
import { useRoute, useRouter } from 'vue-router';
import { useStore } from 'vuex';
import { formatMetricValue, metricLabel as metricLabelFor } from './widgets/widgetData';
import { VARIABLE_PARAM_PREFIX, MAX_VARIABLE_VALUES } from './widgets/dashboardVariables';

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
  },
  metricAgg: {
    type: String,
    default: 'sum'
  },
  // { label, value } of a bar segment's split field
  split: {
    type: Object,
    default: null
  },
  // [{ label, value }] of each metric a combo or scatter charts (formatted)
  rows: {
    type: Array,
    default: null
  }
});

const { t } = useI18n();

const emit = defineEmits(['close']);

const route = useRoute();
const router = useRouter();
const store = useStore();

// the dashboard's list or text control of the value's field ([name, variable]),
// so a value seen in a chart can filter the dashboard; none off the dashboard
// (SPI Graph), for "Other" or a range bucket
const pickVariable = computed(() => {
  if (route.name !== 'Arkime' || !props.data || props.data.other || props.data.range || !props.fieldConfig?.exp) { return null; }
  return Object.entries(store.state.dashboardVariables || {})
    .find(([, v]) => v.field === props.fieldConfig.exp && (v.kind === 'list' || v.kind === 'text')) || null;
});
const alreadyPicked = computed(() => !!pickVariable.value?.[1].values.includes(String(props.data?.item)));
// a list control adds the value to its picks, a text one takes it as its text
const pickValue = () => {
  const [varName, v] = pickVariable.value;
  const value = String(props.data.item);
  const values = v.kind === 'text' ? [value] : [...v.values, value].slice(0, MAX_VARIABLE_VALUES);
  router.replace({ query: { ...route.query, [`${VARIABLE_PARAM_PREFIX}${varName}`]: values } });
  emit('close');
};

// The selected metric's field config (null when the metric is the session count)
const metricField = computed(() => {
  if (!props.metricType || props.metricType === 'sessions') { return null; }
  return FieldService.getField(props.metricType, true);
});

const metricLabel = computed(() => metricLabelFor(props.metricType, props.metricAgg, t));

const showMetricRow = computed(() => !props.rows && metricField.value != null && props.data?.value != null);
// the session count already has its own row
const metricRows = computed(() => (props.rows || []).filter(r => r.label && r.label !== t('sessions.summary.sessions')));

const formattedMetric = computed(() => formatMetricValue(props.metricType, props.data?.value, props.metricAgg));

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
