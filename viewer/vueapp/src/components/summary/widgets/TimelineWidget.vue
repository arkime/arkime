<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0

Field widget: the search timeline. Self-fetches its own graph (global search AND
the widget's local filter) so it can plot any numeric field's metric and apply a
per-widget View/expression. The plotted series follows the widget's metric:
'sessions' -> sessionsHisto, else the field's <dbField>Histo. With a split
field it draws one series per top value of that field instead (stacked, or
overlaid lines), from each value's own graph. Bars or lines, on a linear or log
y axis, per the widget. Drag-select emits a new time range to
the host.
-->
<template>
  <WidgetCard
    :title="displayTitle"
    :loading="loading"
    :error="error"
    :has-data="hasData"
    :info-items="infoItems"
    :link="link"
    @edit="$emit('edit')"
    @remove="$emit('remove')"
    @retry="fetchData">
    <div class="timeline-widget">
      <div
        v-if="seriesDefs"
        class="timeline-widget__legend">
        <span
          v-for="s in seriesDefs"
          :key="s.key"
          class="timeline-widget__legend-item"
          :title="s.label">
          <span
            class="timeline-widget__swatch"
            :style="{ background: s.color }" />
          {{ s.label }}
        </span>
      </div>
      <div class="timeline-widget__plot">
        <TimelineGraph
          :graph-data="graphData"
          :graph-type="graphType"
          :series-defs="seriesDefs"
          :series-type="seriesType"
          :stacked="!!seriesDefs && widget.barMode !== 'grouped'"
          :y-scale="widget.yScale || 'linear'"
          fit-height
          :timeline-data-filters="timelineDataFilters"
          :timezone="timezone"
          @update-time-range="$emit('time-range', $event)" />
      </div>
    </div>
  </WidgetCard>
</template>

<script setup>
import { ref, computed, watch } from 'vue';
import { useStore } from 'vuex';
import { useI18n } from 'vue-i18n';
import WidgetCard from './WidgetCard.vue';
import TimelineGraph from '../../visualizations/TimelineGraph.vue';
import FieldService from '../../search/FieldService';
import { useSpigraphWidget } from './useSpigraphWidget';
import { metricHistoKey, fetchGraph, fetchSplitGraph } from './widgetData';
import { colorRange } from './chartColors';
import { metricSeries } from '../../visualizations/metrics.js';

const { t } = useI18n();
const store = useStore();

const props = defineProps({
  widget: { type: Object, required: true },
  reloadNonce: { type: Number, default: 0 },
  timelineDataFilters: { type: Array, default: () => [] },
  colorScheme: { type: String, default: 'rainbow' },
  infoItems: { type: Array, default: () => [] },
  link: { type: Object, default: null }
});

defineEmits(['edit', 'remove', 'time-range']);

const graphData = ref(null);
// [{ label, key, color }] when split by a field, else null (the single series)
const seriesDefs = ref(null);
const split = computed(() => !!props.widget.splitField);
// an overlaid (grouped) split is always lines, bars would hide each other
const seriesType = computed(() => ((split.value && props.widget.barMode === 'grouped') || props.widget.seriesType === 'lines' ? 'lines' : 'bars'));

// series to plot follows the widget's metric: sessions -> sessionsHisto, else
// the numeric field's <dbField>Histo (the graph fetch requests it via `metric`)
const graphType = computed(() => metricHistoKey({ metricType: props.widget.metricType }));

let d3 = null;
const seriesColors = async (n) => {
  if (!d3) { d3 = await import('d3'); }
  return colorRange(d3, props.colorScheme, n);
};

// results are numbered so one that resolves late (the split path waits for
// its colors) can't overwrite a newer one
let resultSeq = 0;

// a split graph: the top-level graph for the bounds, one <key> series per value
const applySplit = async (res, seq) => {
  const items = res.items || [];
  const colors = await seriesColors(items.length);
  if (seq !== resultSeq) { return; }
  // no top-level graph (no bounds to draw against): nothing to show
  if (!res.graph) { seriesDefs.value = null; graphData.value = null; return; }
  const data = { xmin: res.graph.xmin, xmax: res.graph.xmax, interval: res.graph.interval };
  const defs = items.map((item, i) => {
    const key = `s${i}`;
    data[key] = metricSeries(item.graph, graphType.value); // packets/bytes: src + dst summed
    return { label: String(item.name), key, color: colors[i % colors.length] };
  });
  seriesDefs.value = defs;
  graphData.value = data;
};

// self-fetch the graph; re-runs on metric/expression/view/split/time change
// (the useSpigraphWidget dependency key) so a change updates without a refresh
const { loading, error, fetchData } = useSpigraphWidget(
  () => props.widget,
  () => props.reloadNonce,
  (res) => {
    const seq = ++resultSeq;
    if (split.value) { applySplit(res, seq); return; }
    seriesDefs.value = null;
    graphData.value = res.graph || null;
  },
  (route, s, widget, opts) => (widget.splitField ? fetchSplitGraph(route, s, widget, opts) : fetchGraph(route, s, widget, opts))
);

// a palette change recolors the series without a refetch
watch(() => props.colorScheme, async () => {
  if (!seriesDefs.value) { return; }
  const colors = await seriesColors(seriesDefs.value.length);
  seriesDefs.value = seriesDefs.value.map((d, i) => ({ ...d, color: colors[i % colors.length] }));
});

const hasData = computed(() => !!graphData.value && (!split.value || (seriesDefs.value?.length || 0) > 0));
const splitName = computed(() => (props.widget.splitField
  ? (FieldService.getField(props.widget.splitField, true)?.friendlyName || props.widget.splitField)
  : ''));
const displayTitle = computed(() => props.widget.title ||
  (split.value ? `${t('sessions.summary.timelineView')} / ${splitName.value}` : t('sessions.summary.timelineView')));
const timezone = computed(() => store.state.user?.settings?.timezone || 'local');
</script>

<style scoped>
.timeline-widget {
  display: flex;
  flex-direction: column;
  flex: 1;
  min-height: 0;
}
/* fill the card: the plot grows/shrinks with the widget's configured row span
   (TimelineGraph reads this height via fit-height) */
.timeline-widget__plot {
  flex: 1;
  min-height: 0;
}
.timeline-widget__legend {
  display: flex;
  flex-wrap: wrap;
  gap: 0.2rem 0.8rem;
  font-size: 0.72rem;
  padding: 0 0.25rem 0.25rem;
  max-height: 3.2rem;
  overflow: hidden;
}
.timeline-widget__legend-item {
  display: inline-flex;
  align-items: center;
  gap: 0.3rem;
  max-width: 18rem;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}
.timeline-widget__swatch {
  display: inline-block;
  width: 0.7rem;
  height: 0.7rem;
  border-radius: 2px;
  flex-shrink: 0;
}
</style>
