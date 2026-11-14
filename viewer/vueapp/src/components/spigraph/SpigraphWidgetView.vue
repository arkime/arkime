<!--
Copyright Andy Wick
SPDX-License-Identifier: Apache-2.0

A graph drawn by its dashboard widget, as SPI Graph's page: the graphs whose
settings are the summary endpoint's (a metric the values are sized by, an
aggregation, Top/Bottom order, a bucket interval, a table's metric columns),
which SPI Graph's own charts of session counts can't show. The widget searches
with the page's search, view and time, and reloads with the page.
-->
<template>
  <div class="spigraph-widget-view">
    <!-- the graph's other fields, in the sub navbar like the page's own -->
    <teleport
      v-if="maxSubFields > 0"
      defer
      to="#spigraph-subfield-anchor">
      <div class="d-flex flex-row align-center">
        <div
          v-if="subFields.length < maxSubFields"
          class="arkime-input-group subfield-group">
          <span class="arkime-input-label">
            {{ $t('spigraph.addAnotherField') }}:
          </span>
          <ArkimeFieldTypeahead
            :fields="fields"
            page="SpigraphSubfield"
            @field-selected="addSubField" />
        </div>
        <div class="d-inline ms-1">
          <DragList
            :list="subFields"
            @reorder="setSubFields"
            @remove="(i) => setSubFields(subFields.filter((f, n) => n !== i))" />
        </div>
      </div>
    </teleport>

    <MultiFieldTableWidget
      v-if="graph.viewMode === 'table'"
      :widget="widget"
      :reload-nonce="reloadNonce" />
    <TreemapWidget
      v-else-if="graph.viewMode === 'treemap'"
      :widget="widget"
      :reload-nonce="reloadNonce"
      @show-tooltip="showTooltip" />
    <MetricsChartWidget
      v-else-if="graph.viewMode === 'combo' || graph.viewMode === 'scatter'"
      :widget="widget"
      :reload-nonce="reloadNonce"
      @show-tooltip="showTooltip"
      @hide-tooltip="tooltip = null" />
    <SummaryWidget
      v-else
      title=""
      :data="rows"
      :loading="loading"
      :error="error"
      :view-mode="graph.viewMode"
      :metric-type="graph.metricType"
      :metric-agg="graph.metricAgg"
      :split-field="graph.splitField"
      :bar-mode="graph.barMode"
      :orientation="graph.orientation"
      :thresholds="thresholds"
      :other="graph.other ? other : null"
      :field="graph.field"
      @show-tooltip="showTooltip"
      @hide-tooltip="tooltip = null"
      @export="exportPNG"
      @retry-field="fetchData" />

    <SummaryChartTooltip
      :visible="!!tooltip"
      :data="tooltip?.data"
      :field-config="tooltip?.fieldConfig"
      :position="tooltip?.position"
      :percentage="tooltip?.percentage ?? null"
      :metric-type="tooltip?.metricType || 'sessions'"
      :metric-agg="tooltip?.metricAgg || 'sum'"
      :split="tooltip?.split || null"
      :rows="tooltip?.rows || null"
      @close="tooltip = null" />
  </div>
</template>

<script setup>
import { ref, computed, provide, onMounted, onBeforeUnmount } from 'vue';
import { useRoute, useRouter } from 'vue-router';
import { useStore } from 'vuex';
import SummaryWidget from '../summary/SummaryWidget.vue';
import SummaryChartTooltip from '../summary/SummaryChartTooltip.vue';
import MultiFieldTableWidget from '../summary/widgets/MultiFieldTableWidget.vue';
import TreemapWidget from '../summary/widgets/TreemapWidget.vue';
import MetricsChartWidget from '../summary/widgets/MetricsChartWidget.vue';
import ArkimeFieldTypeahead from '../utils/FieldTypeahead.vue';
import DragList from '../utils/DragList.vue';
import FieldService from '../search/FieldService';
import { useSpigraphWidget } from '../summary/widgets/useSpigraphWidget';
import { fetchSummaryWidget, widgetThresholds } from '../summary/widgets/widgetData';
import { isStreamMode } from '../summary/widgets/viewModes';

const props = defineProps({
  // the page's graph (saved-graph data)
  graph: { type: Object, required: true },
  // bumped by the page's search (and its refresh), the widgets' too
  reloadNonce: { type: Number, default: 0 }
});

provide('widgetBare', true);

const route = useRoute();
const router = useRouter();
const store = useStore();

// the page's search and view are the url's, which every fetch already sends:
// the graph's are the search box's text as typed, not yet searched
const widget = computed(() => ({ ...props.graph, expression: '', view: '' }));

// a bar, pie, tag cloud or waffle: the dashboard feeds these from its page
// stream, here the one drawn is fetched on its own
const stream = computed(() => isStreamMode(props.graph.viewMode));
const rows = ref([]);
const other = ref(null);
const { loading, error, fetchData } = useSpigraphWidget(
  () => (stream.value ? widget.value : {}),
  () => props.reloadNonce,
  (res) => { rows.value = res.data; other.value = res.other; },
  (r, s, w, opts) => (stream.value ? fetchSummaryWidget(r, s, w, opts) : Promise.resolve({ data: [], other: null }))
);
const thresholds = computed(() => widgetThresholds(props.graph));

// the fields after the first: a bar's split field, a table's other columns
const fields = computed(() => FieldService.addIpDstPortField(store.state.fieldsArr));
const maxSubFields = computed(() => (props.graph.viewMode === 'bar' ? 1 : props.graph.viewMode === 'table' ? 6 : props.graph.viewMode === 'pie' || props.graph.viewMode === 'waffle' || props.graph.viewMode === 'treemap' ? 1 : 0));
const subFields = computed(() => String(route.query.subFields || '').split(',').filter(Boolean)
  .map(exp => FieldService.getField(exp, true)).filter(Boolean));
const setSubFields = (list) => {
  const query = { ...route.query, subFields: list.map(f => f.exp).join(',') || undefined };
  // a table of several fields stays each field's own top values
  if (props.graph.viewMode === 'table') { query.tableMode = list.length ? 'unique' : undefined; }
  router.push({ query });
};
const addSubField = (field) => {
  if (!field?.exp || subFields.value.some(f => f.exp === field.exp)) { return; }
  setSubFields([...subFields.value, field]);
};

const tooltip = ref(null);
const showTooltip = (evt) => { tooltip.value = evt; };
const closeTooltip = (e) => {
  if (tooltip.value && !e.target.closest('.summary-tooltip')) { tooltip.value = null; }
};
onMounted(() => document.addEventListener('click', closeTooltip));
onBeforeUnmount(() => document.removeEventListener('click', closeTooltip));

let saveSvgAsPng;
const exportPNG = async (svgId) => {
  if (!saveSvgAsPng) { saveSvgAsPng = await import('save-svg-as-png'); }
  const svg = document.getElementById(svgId);
  if (!svg) { return; }
  const style = getComputedStyle(document.body);
  saveSvgAsPng.saveSvgAsPng(svg, 'spigraph.png', {
    backgroundColor: style.backgroundColor,
    modifyCss: (selector, properties) => `${selector}{${selector.includes('text') ? `fill: ${style.color}; ` : ''}${properties}}`
  });
};
</script>

<style scoped>
.spigraph-widget-view {
  height: calc(100vh - 220px);
  min-height: 320px;
  padding: 0.5rem;
  display: flex;
  flex-direction: column;
}
.spigraph-widget-view > :deep(.widget-card) {
  flex: 1;
}
</style>
