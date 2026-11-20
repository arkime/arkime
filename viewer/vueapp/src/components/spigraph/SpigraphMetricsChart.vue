<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0

SPI Graph's Combo and Scatter types: the field's top values (the page's
/api/spigraph items) charted by several of the timeline metrics at once, each
a per value sum (<dbField>Histo). Sizes the shared dashboard chart to the
page and shows the dashboard's tooltip, with the field's actions, on hover.
-->
<template>
  <div
    ref="container"
    class="spigraph-metrics-chart">
    <template v-if="width > 0 && data.length">
      <SummaryComboChart
        v-if="type === 'combo'"
        :data="comboData"
        svg-id="spigraph-combo"
        :field-config="fieldObj"
        :bar-label="metrics[0].label"
        :line-label="metrics[1].label"
        :format-bar="metrics[0].format"
        :format-line="metrics[1].format"
        :width="width"
        :height="height"
        @show-tooltip="showTooltip" />
      <SummaryScatterChart
        v-else
        :data="scatterData"
        svg-id="spigraph-scatter"
        :field-config="fieldObj"
        :x-label="metrics[0].label"
        :y-label="metrics[1].label"
        :size-label="metrics[2]?.label || ''"
        :format-x="metrics[0].format"
        :format-y="metrics[1].format"
        :format-size="metrics[2]?.format || metrics[0].format"
        :log="log"
        :width="width"
        :height="height"
        @show-tooltip="showTooltip" />
    </template>
    <SummaryChartTooltip
      :visible="!!tooltip"
      :data="tooltip?.data"
      :field-config="tooltip?.fieldConfig"
      :position="tooltip?.position"
      :rows="tooltip?.rows"
      @close="tooltip = null" />
  </div>
</template>

<script setup>
import { ref, computed, onMounted, onBeforeUnmount } from 'vue';
import SummaryComboChart from '../summary/SummaryComboChart.vue';
import SummaryScatterChart from '../summary/SummaryScatterChart.vue';
import SummaryChartTooltip from '../summary/SummaryChartTooltip.vue';

const props = defineProps({
  // 'combo' | 'scatter'
  type: { type: String, required: true },
  // the /api/spigraph items: { name, sessionsHisto, <dbField>Histo, ... }
  items: { type: Array, default: () => [] },
  fieldObj: { type: Object, required: true },
  // [{ key (a Histo key), label, format }] in slot order; a scatter's third
  // (the bubble size) may be missing
  metrics: { type: Array, required: true },
  log: { type: Boolean, default: false }
});

const container = ref(null);
const width = ref(0);
const height = ref(0);
const tooltip = ref(null);

const data = computed(() => props.items || []);
const val = (item, m) => (m ? Number(item[m.key]) || 0 : undefined);
const comboData = computed(() => data.value.map(item => ({
  item: item.name, sessions: item.sessionsHisto, bar: val(item, props.metrics[0]), line: val(item, props.metrics[1])
})));
const scatterData = computed(() => data.value.map(item => ({
  item: item.name, sessions: item.sessionsHisto, x: val(item, props.metrics[0]), y: val(item, props.metrics[1]), size: val(item, props.metrics[2])
})));

const showTooltip = (evt) => { tooltip.value = evt; };
// a click outside the tooltip closes it (its field menu needs clicks inside)
const onDocClick = (e) => {
  if (tooltip.value && !e.target.closest('.summary-tooltip')) { tooltip.value = null; }
};

let ro;
const measure = () => {
  if (!container.value) { return; }
  width.value = container.value.clientWidth;
  height.value = container.value.clientHeight;
};
onMounted(() => {
  measure();
  if (typeof ResizeObserver !== 'undefined') {
    ro = new ResizeObserver(measure);
    ro.observe(container.value);
  }
  document.addEventListener('click', onDocClick);
});
onBeforeUnmount(() => {
  if (ro) { ro.disconnect(); }
  document.removeEventListener('click', onDocClick);
});
</script>

<style scoped>
.spigraph-metrics-chart {
  position: relative;
  height: 520px;
  padding: 0 0.5rem;
  display: flex;
}
</style>
