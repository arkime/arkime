<!--
Copyright Andy Wick
SPDX-License-Identifier: Apache-2.0

Box plot widget: a field's top values, each drawn as the spread of a numeric
metric over its sessions (see SummaryBoxPlot). Self-fetches
/api/sessions/summary with box: true, ranked by session count or by an
aggregation of the metric. SPI Graph's Box plot draws this widget too.
-->
<template>
  <WidgetCard
    :title="title"
    :widget-id="widget.id"
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
      class="boxplot-container">
      <SummaryBoxPlot
        v-if="hasData && width > 0"
        :data="rows"
        :svg-id="svgId"
        :field-config="fieldObj"
        :metric-label="metricName"
        :format-value="formatValue"
        :log="widget.yScale === 'log'"
        :orientation="widget.orientation"
        :width="width"
        :height="height"
        :color-scheme="colorScheme"
        @show-tooltip="$emit('show-tooltip', $event)" />
    </div>
  </WidgetCard>
</template>

<script setup>
import { ref, computed } from 'vue';
import { useI18n } from 'vue-i18n';
import WidgetCard from './WidgetCard.vue';
import SummaryBoxPlot from '../SummaryBoxPlot.vue';
import FieldService from '../../search/FieldService';
import { useSpigraphWidget } from './useSpigraphWidget';
import { useContainerSize } from './useContainerSize';
import { fetchBoxPlot, formatMetricValue, metricLabel } from './widgetData';
import { boxMetric } from './viewModes';

const { t } = useI18n();

const props = defineProps({
  widget: { type: Object, required: true },
  reloadNonce: { type: Number, default: 0 },
  colorScheme: { type: String, default: 'rainbow' },
  infoItems: { type: Array, default: () => [] },
  link: { type: Object, default: null },
  graphDef: { type: Object, default: null }
});

defineEmits(['edit', 'remove', 'show-tooltip']);

const { container, width, height, observe } = useContainerSize();
const rows = ref([]);

const { loading, error, fetchData } = useSpigraphWidget(
  () => props.widget,
  () => props.reloadNonce,
  (res) => { rows.value = res; observe(); },
  fetchBoxPlot
);

const svgId = computed(() => `boxplot-${props.widget.id}`);
const fieldObj = computed(() => FieldService.getField(props.widget.field, true));
const metric = computed(() => boxMetric(props.widget));
const metricName = computed(() => metricLabel(metric.value, 'sum', t));
// the box's numbers are the metric's own values, an average or percentile
// of them rarely whole
const formatValue = (v) => formatMetricValue(metric.value, v, 'avg');

const title = computed(() => props.widget.title ||
  `${fieldObj.value?.friendlyName || props.widget.field} ${t('sessions.summary.boxplotView')}`);
const hasData = computed(() => rows.value.length > 0 && !!fieldObj.value);
</script>

<style scoped>
.boxplot-container {
  position: relative;
  flex: 1;
  width: 100%;
  min-height: 0;
  overflow: hidden;
}
</style>
