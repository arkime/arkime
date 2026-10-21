<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0

Metric widget: one big number — the chosen metric (session count, or a field's
sum/avg/min/max/unique count) over the whole result set — or one number per
top-N value when a split field is chosen. Self-fetches /api/sessions/summary
(global search AND the widget's local filter).
-->
<template>
  <WidgetCard
    :title="title"
    :loading="loading"
    :error="error"
    :has-data="hasData"
    :info-items="infoItems"
    @edit="$emit('edit')"
    @remove="$emit('remove')"
    @retry="fetchData">
    <div
      class="metric-grid"
      :class="{ 'metric-grid--single': !fieldObj }">
      <div
        v-for="(row, i) in rows"
        :key="i"
        class="metric-card">
        <div
          v-if="fieldObj"
          class="metric-label">
          <arkime-session-field
            :field="fieldObj"
            :value="row.item"
            :expr="fieldObj.exp"
            :parse="true"
            :session-btn="true"
            :pull-left="true" />
        </div>
        <div
          v-else
          class="metric-label">
          {{ metricLabel }}
        </div>
        <div
          class="metric-value"
          :title="String(row.value)">
          {{ format(row.value) }}
        </div>
        <div
          v-if="fieldObj"
          class="metric-sublabel">
          {{ metricLabel }}
        </div>
      </div>
    </div>
  </WidgetCard>
</template>

<script setup>
import { ref, computed } from 'vue';
import { useI18n } from 'vue-i18n';
import WidgetCard from './WidgetCard.vue';
import ArkimeSessionField from '../../sessions/SessionField.vue';
import FieldService from '../../search/FieldService';
import { useSpigraphWidget } from './useSpigraphWidget';
import { fetchMetric, formatMetricValue, metricLabel as metricLabelFor } from './widgetData';

const { t } = useI18n();

const props = defineProps({
  widget: { type: Object, required: true },
  reloadNonce: { type: Number, default: 0 },
  infoItems: { type: Array, default: () => [] }
});

defineEmits(['edit', 'remove']);

const rows = ref([]); // [{ item, sessions, value, metricValues }]

const { loading, error, fetchData } = useSpigraphWidget(
  () => props.widget,
  () => props.reloadNonce,
  (res) => { rows.value = Array.isArray(res) ? res : []; },
  fetchMetric
);

const fieldObj = computed(() => (props.widget.field ? FieldService.getField(props.widget.field, true) : null));
const hasData = computed(() => rows.value.length > 0);

const metricType = computed(() => props.widget.metricType || 'sessions');
const metricAgg = computed(() => props.widget.metricAgg || 'sum');

// "Sessions", "Bytes" or "<Agg> <Field>" (eg "Max Bytes"), same as every other widget
const metricLabel = computed(() => metricLabelFor(metricType.value, metricAgg.value, t));

const title = computed(() => props.widget.title ||
  (fieldObj.value ? `${metricLabel.value} / ${fieldObj.value.friendlyName}` : metricLabel.value));

const format = (v) => formatMetricValue(metricType.value, v, metricAgg.value);
</script>

<style scoped>
.metric-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(120px, 1fr));
  gap: 0.4rem;
  align-content: center;
  flex: 1;
  overflow: auto;
}
.metric-grid--single {
  grid-template-columns: 1fr;
}
.metric-card {
  padding: 0.35rem 0.5rem;
  border-radius: 6px;
  background: rgb(var(--v-theme-quaternary-lightest));
  text-align: center;
  min-width: 0;
}
.metric-label {
  font-size: 0.8rem;
  opacity: 0.85;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}
.metric-value {
  font-size: 2rem;
  font-weight: 700;
  line-height: 1.15;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}
.metric-grid--single .metric-value {
  font-size: 3rem;
}
.metric-sublabel {
  font-size: 0.7rem;
  opacity: 0.7;
}
</style>
