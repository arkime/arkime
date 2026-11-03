<!--
Copyright Andy Wick
SPDX-License-Identifier: Apache-2.0
Mosaic: one column per value of the first field, as wide as its share of the
sessions, split top to bottom into the second field's values, each as tall as
its share of the column, so a cell's area is its share of the whole. Drawn
from the hierarchy endpoint's two level tree fetched with "Other", so a column
adds up to its sessions. A second-field value keeps one color in every column;
what falls outside the kept values is a gray "Other" cell (see mosaicLayout).
Hovering a cell shows the shared chart tooltip (the column value with its
field actions, the cell value as its split).
-->
<template>
  <div class="chart-wrapper">
    <svg
      :id="svgId"
      class="mosaic"
      :width="width"
      :height="height">
      <g
        v-for="cell in cells"
        :key="cell.k"
        class="mosaic-cell"
        @mouseover="onHover(cell, $event)"
        @mouseleave="emit('hide-tooltip')">
        <rect
          :x="cell.x"
          :y="cell.y"
          :width="cell.w"
          :height="cell.h"
          rx="2"
          :style="{ fill: cell.fill }" />
        <text
          v-if="cell.label"
          :x="cell.x + 4"
          :y="cell.y + 13"
          class="mosaic-label">
          {{ cell.label }}
        </text>
        <text
          v-if="cell.pct"
          :x="cell.x + 4"
          :y="cell.y + 26"
          class="mosaic-value">
          {{ cell.pct }}
        </text>
      </g>
      <!-- each column's value and share, under it -->
      <text
        v-for="col in colLabels"
        :key="'c' + col.index"
        :x="col.x"
        :y="height - 4"
        text-anchor="middle"
        class="mosaic-col-label">
        {{ col.label }}
        <title>{{ col.name }}</title>
      </text>
    </svg>
  </div>
</template>

<script setup>
import { ref, computed, watch } from 'vue';
import { useI18n } from 'vue-i18n';
import { colorRange, OTHER_COLOR } from './widgets/chartColors';
import { mosaicLayout, fitLabel, sharePercent } from '../utils/hierarchyLayout';

const props = defineProps({
  // the hierarchy: { children: [{ name, size | sizeValue, other?, children: [{ name, size, other? }] }] }
  data: { type: Object, required: true },
  svgId: { type: String, required: true },
  // the first field (columns) and the second (the cells in each column)
  fieldConfig: { type: Object, required: true },
  colFieldConfig: { type: Object, default: null },
  width: { type: Number, default: 400 },
  height: { type: Number, default: 300 },
  colorScheme: { type: String, default: 'rainbow' },
  // the most second-field values given their own color
  maxSegments: { type: Number, default: 10 }
});
const emit = defineEmits(['show-tooltip', 'hide-tooltip']);
const { t } = useI18n();

const CHAR_PX = 6; // ~width of a 10px label character
const COL_LABEL_H = 18;

const d3 = ref(null);
// without d3 (its chunk failed to load) the cells keep the accent color
import('d3').then((lib) => { d3.value = lib; }).catch(() => {});

const nameOf = (node) => (node.other ? t('sessions.summary.other') : String(node.name ?? ''));

const layout = computed(() => mosaicLayout(props.data, props.width, Math.max(0, props.height - COL_LABEL_H), { maxSegments: props.maxSegments }));

// a second field colors by its kept values; with one field each column has its own color
const palette = ref(null);
watch([d3, layout, () => props.colorScheme], () => {
  const m = layout.value;
  if (!d3.value || !m) { palette.value = null; return; }
  const keys = m.segments.length ? m.segments : m.columns.filter(c => !c.other).map(c => c.name);
  palette.value = d3.value.scaleOrdinal(colorRange(d3.value, props.colorScheme, keys.length)).domain(keys);
}, { immediate: true });

const cells = computed(() => (layout.value?.cells || []).map((c, i) => {
  const pct = sharePercent(c.share);
  const label = c.h >= 16 ? fitLabel(nameOf(c), c.w - 8, CHAR_PX) : '';
  return {
    ...c,
    k: `${c.col}-${i}`,
    fill: c.other ? OTHER_COLOR : (palette.value ? palette.value(c.name) : 'rgb(var(--v-theme-foreground-accent))'),
    label,
    // a second field's cells name their share of the column, below the name
    pct: label && props.colFieldConfig && c.h >= 30 && c.w >= pct.length * CHAR_PX + 8 ? pct : ''
  };
}));

const colLabels = computed(() => (layout.value?.columns || []).map((c, i) => {
  // the value and its share where both fit whole, else the value (cut to fit)
  const text = `${nameOf(c)} ${sharePercent(c.share)}`;
  return { index: i, name: nameOf(c), x: c.x + c.w / 2, label: text.length * CHAR_PX <= c.w - 2 ? text : fitLabel(nameOf(c), c.w - 2, CHAR_PX) };
}));

const onHover = (cell, evt) => {
  const col = layout.value.columns[cell.col];
  emit('show-tooltip', {
    data: { item: nameOf(col), other: col.other, sessions: cell.value, value: cell.value },
    position: { x: evt.clientX + 1, y: evt.clientY + 1 },
    fieldConfig: props.fieldConfig,
    metricType: 'sessions',
    // the cell's area: its share of the column times the column's share
    percentage: cell.area * 100,
    split: props.colFieldConfig ? { label: props.colFieldConfig.friendlyName, value: nameOf(cell) } : null
  });
};
</script>

<style scoped>
.chart-wrapper {
  width: 100%;
  height: 100%;
  overflow: hidden;
}
.mosaic-cell {
  cursor: pointer;
}
.mosaic-cell:hover rect {
  opacity: 0.85;
}
/* white text with a dark halo stays readable on any fill */
.mosaic-label,
.mosaic-value {
  fill: #fff;
  paint-order: stroke;
  stroke: rgba(0, 0, 0, 0.55);
  stroke-width: 2px;
  stroke-linejoin: round;
  pointer-events: none;
}
.mosaic-label {
  font-size: 10px;
  font-weight: 600;
}
.mosaic-value {
  font-size: 10px;
}
.mosaic-col-label {
  fill: rgb(var(--v-theme-foreground));
  font-size: 10px;
}
</style>
