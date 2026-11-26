<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0

Field widget rendered in "treemap" view mode: the field's top-N values as nested
rectangles sized by session count or the widget's metric. Self-fetches the
values' totals from /api/sessions/summary (global search AND the widget's local
filter) and lays out a squarified treemap (d3) sized to the
card; instance-safe (no shared DOM ids). Hover shows the shared chart popover.
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
      class="treemap-container">
      <svg
        v-if="leaves.length"
        :width="size.w"
        :height="size.h">
        <g
          v-for="leaf in leaves"
          :key="leaf.key">
          <rect
            :x="leaf.x"
            :y="leaf.y"
            :width="leaf.w"
            :height="leaf.h"
            class="treemap-rect"
            :style="{ fill: leaf.fill }"
            @mouseover="onHover($event, leaf)" />
          <text
            v-if="leaf.showLabel"
            :x="leaf.x + 5"
            :y="leaf.y + 15"
            class="treemap-label">
            {{ leaf.name }}
          </text>
          <text
            v-if="leaf.showLabel"
            :x="leaf.x + 5"
            :y="leaf.y + 29"
            class="treemap-value">
            {{ leaf.valueLabel }}
          </text>
        </g>
      </svg>
    </div>
  </WidgetCard>
</template>

<script setup>
import { ref, computed, watch, onBeforeUnmount, nextTick } from 'vue';
import WidgetCard from './WidgetCard.vue';
import FieldService from '../../search/FieldService';
import { colorRange } from './chartColors';
import { useSpigraphWidget } from './useSpigraphWidget';
import { fetchSummaryWidget, formatMetricValue } from './widgetData';

const props = defineProps({
  widget: { type: Object, required: true },
  reloadNonce: { type: Number, default: 0 },
  colorScheme: { type: String, default: 'rainbow' },
  infoItems: { type: Array, default: () => [] },
  link: { type: Object, default: null },
  graphDef: { type: Object, default: null }
});
const emit = defineEmits(['edit', 'remove', 'show-tooltip']);

const items = ref([]);
const leaves = ref([]);
const size = ref({ w: 0, h: 0 });
const container = ref(null);
let ro;
let d3lib;

// a treemap draws each value's total, never its timeline: the summary
// endpoint's one terms aggregation, not SPI Graph's search per value with a
// graph each. Its parts add up and it has no order of its own, so it is
// always the top values by the metric's sum, whatever order or aggregation
// the widget still carries from another type.
const fetchTotals = async (route, store, widget, opts) =>
  (await fetchSummaryWidget(route, store, { ...widget, order: 'desc', orderBy: 'value', metricAgg: 'sum' }, opts)).data;

const { loading, error, fetchData } = useSpigraphWidget(
  () => props.widget,
  () => props.reloadNonce,
  (rows) => {
    // a row's value is its metric (the session count when it has none)
    items.value = rows.filter(r => r.sessions > 0).map(r => ({ name: r.item, count: r.sessions, value: r.value || 0 }));
    setupObserver();
  },
  fetchTotals
);

const fieldObj = computed(() => FieldService.getField(props.widget.field, true));
const title = computed(() => props.widget.title || fieldObj.value?.friendlyName || props.widget.field);

// metric drives rectangle size: session count by default, else the chosen
// metric's total per value
const itemValue = (i) => i.value;
const formatValue = (v) => formatMetricValue(props.widget.metricType, v);

// only positive-metric values can be sized/drawn; drives hasData so an all-zero
// metric shows the empty state, not a blank body
const drawItems = computed(() => items.value.filter(i => itemValue(i) > 0));
const hasData = computed(() => drawItems.value.length > 0);

const measure = () => {
  if (!container.value) { return; }
  size.value = { w: container.value.clientWidth, h: container.value.clientHeight };
};

const layout = async () => {
  if (!drawItems.value.length || !size.value.w || !size.value.h) { leaves.value = []; return; }
  if (!d3lib) { d3lib = await import('d3'); }
  const root = d3lib.hierarchy({ children: drawItems.value.map(i => ({ name: i.name, value: itemValue(i), src: i })) })
    .sum(d => d.value)
    .sort((a, b) => b.value - a.value);
  d3lib.treemap().size([size.value.w, size.value.h]).paddingInner(2).round(true)(root);
  // dashboard palette (shared with the bar/pie charts)
  const colors = d3lib.scaleOrdinal(colorRange(d3lib, props.colorScheme, drawItems.value.length));
  leaves.value = root.leaves().map((l, idx) => {
    const w = l.x1 - l.x0;
    const h = l.y1 - l.y0;
    return {
      key: l.data.name + '-' + idx,
      x: l.x0,
      y: l.y0,
      w,
      h,
      name: l.data.name,
      src: l.data.src,
      value: l.value,
      valueLabel: formatValue(l.value),
      fill: colors(l.data.name),
      showLabel: w > 44 && h > 24
    };
  });
};

// (re)attach the resize observer once the container is in the DOM (the slot
// only renders when there's data), then lay out
const setupObserver = () => {
  nextTick(() => {
    if (!container.value) { return; }
    if (ro) { ro.disconnect(); }
    measure();
    if (typeof ResizeObserver !== 'undefined') {
      ro = new ResizeObserver(measure);
      ro.observe(container.value);
    }
    layout();
  });
};

// hover shows the shared chart popover (same as the bar/pie widgets)
const onHover = (e, leaf) => {
  emit('show-tooltip', {
    data: {
      item: leaf.name,
      sessions: leaf.src.count,
      value: leaf.value
    },
    position: { x: e.clientX + 1, y: e.clientY + 1 },
    fieldConfig: fieldObj.value,
    metricType: props.widget.metricType || 'sessions'
  });
};

watch(size, layout);
watch(() => props.colorScheme, layout);

onBeforeUnmount(() => {
  if (ro) { ro.disconnect(); }
});
</script>

<style scoped>
.treemap-container {
  position: relative;
  width: 100%;
  height: 100%;
  min-height: 0;
  overflow: hidden;
}
.treemap-rect {
  stroke: rgb(var(--v-theme-background));
  stroke-width: 1;
}
/* white text with a dark halo stays readable on any fill */
.treemap-label,
.treemap-value {
  fill: #fff;
  paint-order: stroke;
  stroke: rgba(0, 0, 0, 0.55);
  stroke-width: 2px;
  stroke-linejoin: round;
  pointer-events: none;
}
.treemap-label {
  font-size: 11px;
  font-weight: 600;
}
.treemap-value {
  font-size: 10px;
  opacity: 0.92;
}
</style>
