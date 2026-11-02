<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0

Combo and scatter widgets: a field's top values, each charted by several
metrics at once (the widget's metric slots, see METRIC_SLOTS). The combo
draws one metric as bars and another as a line on its own axis; the scatter
places each value by two metrics and sizes it by an optional third.
Self-fetches /api/sessions/summary like the table, with every slot's metric
aggregated per value and the first one ranking the Top/Bottom N.
-->
<template>
  <WidgetCard
    :title="title"
    :loading="loading"
    :error="error"
    :has-data="hasData"
    :info-items="infoItems"
    :link="link"
    :graph="graphDef"
    @edit="$emit('edit')"
    @remove="$emit('remove')"
    @retry="fetchData">
    <div
      ref="container"
      class="metrics-chart-container">
      <template v-if="hasData && width > 0">
        <SummaryComboChart
          v-if="widget.viewMode === 'combo'"
          :data="comboData"
          :svg-id="svgId"
          :field-config="fieldObj"
          :bar-label="labels[0]"
          :line-label="labels[1]"
          :format-bar="formatters[0]"
          :format-line="formatters[1]"
          :width="width"
          :height="height"
          :color-scheme="colorScheme"
          @show-tooltip="$emit('show-tooltip', $event)" />
        <SummaryScatterChart
          v-else
          :data="scatterData"
          :svg-id="svgId"
          :field-config="fieldObj"
          :x-label="labels[0]"
          :y-label="labels[1]"
          :size-label="labels[2] || ''"
          :format-x="formatters[0]"
          :format-y="formatters[1]"
          :format-size="formatters[2] || formatters[0]"
          :log="widget.yScale === 'log'"
          :width="width"
          :height="height"
          :color-scheme="colorScheme"
          @show-tooltip="$emit('show-tooltip', $event)" />
      </template>
    </div>
  </WidgetCard>
</template>

<script setup>
import { ref, computed, nextTick, onBeforeUnmount } from 'vue';
import { useI18n } from 'vue-i18n';
import WidgetCard from './WidgetCard.vue';
import SummaryComboChart from '../SummaryComboChart.vue';
import SummaryScatterChart from '../SummaryScatterChart.vue';
import FieldService from '../../search/FieldService';
import { useSpigraphWidget } from './useSpigraphWidget';
import { fetchMetricsChart, formatMetricValue, metricLabel, metricRowValue } from './widgetData';
import { slotMetrics } from './viewModes';

const { t } = useI18n();

const props = defineProps({
  widget: { type: Object, required: true },
  reloadNonce: { type: Number, default: 0 },
  colorScheme: { type: String, default: 'rainbow' },
  infoItems: { type: Array, default: () => [] },
  link: { type: Object, default: null },
  graphDef: { type: Object, default: null }
});

defineEmits(['edit', 'remove', 'show-tooltip', 'hide-tooltip']);

const container = ref(null);
const width = ref(0);
const height = ref(0);
const rows = ref([]);

const { loading, error, fetchData } = useSpigraphWidget(
  () => props.widget,
  () => props.reloadNonce,
  (res) => { rows.value = res; setupObserver(); },
  fetchMetricsChart
);

const svgId = computed(() => `${props.widget.viewMode}-${props.widget.id}`);
const fieldObj = computed(() => FieldService.getField(props.widget.field, true));
// one metric per slot ('' for a scatter without a bubble size)
const metrics = computed(() => slotMetrics(props.widget));
const aggOf = (m) => (m === 'sessions' ? undefined : props.widget.metricAgg);
const labels = computed(() => metrics.value.map(m => (m ? metricLabel(m, aggOf(m), t) : '')));
const formatters = computed(() => metrics.value.map(m => (m ? (v) => formatMetricValue(m, v, aggOf(m)) : null)));
const value = (row, i) => metricRowValue(row, metrics.value[i]);

const comboData = computed(() => rows.value.map(row => ({ item: row.item, sessions: row.sessions, bar: value(row, 0), line: value(row, 1) })));
const scatterData = computed(() => rows.value.map(row => ({
  item: row.item,
  sessions: row.sessions,
  x: value(row, 0),
  y: value(row, 1),
  size: metrics.value[2] ? value(row, 2) : undefined
})));

const typeLabel = computed(() => t(props.widget.viewMode === 'combo' ? 'sessions.summary.comboView' : 'sessions.summary.scatterView'));
const title = computed(() => props.widget.title ||
  `${fieldObj.value?.friendlyName || props.widget.field} ${typeLabel.value}`);
const hasData = computed(() => rows.value.length > 0 && !!fieldObj.value);

const measure = () => {
  if (!container.value) { return; }
  width.value = container.value.clientWidth;
  height.value = container.value.clientHeight;
};
// the card only renders its content once the data is in, so the container
// to measure (and watch) exists after each result, not at mount
let ro;
const setupObserver = () => {
  nextTick(() => {
    if (!container.value) { return; }
    if (ro) { ro.disconnect(); }
    if (typeof ResizeObserver !== 'undefined') {
      ro = new ResizeObserver(measure);
      ro.observe(container.value);
    }
    measure();
  });
};
onBeforeUnmount(() => { if (ro) { ro.disconnect(); } });
</script>

<style scoped>
.metrics-chart-container {
  position: relative;
  flex: 1;
  width: 100%;
  min-height: 0;
  overflow: hidden;
}
</style>
