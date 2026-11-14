<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
Matrix: one field's top values down the side, another's across the top, each
cell colored (and labelled, where it fits) by the sessions the two values
share. Drawn from the hierarchy endpoint's two level tree, so a cell is the
second field's count under the first field's value. Hovering a cell shows the
shared chart tooltip (the row value with its field actions, the column value
as its split). Each row only has its own top values of the second field, so a
column a row doesn't have is zero when the row has no "Other" (it has no more
values) and unknown when it does; unknown cells are hatched. Cells keep a
minimum size, so a big matrix scrolls.
-->
<template>
  <div class="chart-wrapper">
    <svg
      :id="svgId"
      class="matrix"
      :width="svgW"
      :height="svgH"
      :viewBox="`0 0 ${svgW} ${svgH}`">
      <defs>
        <pattern
          :id="`${svgId}-unknown`"
          width="6"
          height="6"
          patternUnits="userSpaceOnUse"
          patternTransform="rotate(45)">
          <rect
            width="6"
            height="6"
            class="matrix-unknown-bg" />
          <line
            x1="0"
            y1="0"
            x2="0"
            y2="6"
            class="matrix-unknown-line" />
        </pattern>
      </defs>
      <!-- column labels, turned up so long values fit -->
      <text
        v-for="col in colLabels"
        :key="'c' + col.index"
        :transform="`translate(${col.x}, ${colLabelH - 4}) rotate(-45)`"
        class="matrix-label"
        text-anchor="start">
        {{ col.label }}
        <title>{{ col.name }}</title>
      </text>
      <!-- row labels -->
      <text
        v-for="row in rowLabels"
        :key="'r' + row.index"
        :x="labelW - 6"
        :y="row.y"
        dominant-baseline="middle"
        text-anchor="end"
        class="matrix-label">
        {{ row.label }}
        <title>{{ row.name }}</title>
      </text>
      <!-- cells -->
      <g
        v-for="cell in cells"
        :key="cell.k"
        class="matrix-cell"
        @mouseover="onHover(cell, $event)"
        @mouseleave="emit('hide-tooltip')">
        <rect
          :x="cell.x"
          :y="cell.y"
          :width="cell.w"
          :height="cell.h"
          rx="2"
          :style="{ fill: cell.value === null ? `url(#${svgId}-unknown)` : cell.fill }" />
        <text
          v-if="cell.text"
          :x="cell.x + cell.w / 2"
          :y="cell.y + cell.h / 2"
          dominant-baseline="middle"
          text-anchor="middle"
          class="matrix-value"
          :class="{ 'matrix-value--dark': cell.dark }">
          {{ cell.text }}
        </text>
      </g>
    </svg>
  </div>
</template>

<script setup>
import { ref, computed, watch } from 'vue';
import { useI18n } from 'vue-i18n';
import { commaString } from '@common/vueFilters.js';
import { heatmapInterpolator } from './widgets/chartColors';
import { fitLabel, nodeCount as count } from '../utils/hierarchyLayout';

const props = defineProps({
  // the hierarchy: { children: [{ name, size | sizeValue, children: [{ name, size }] }] }
  data: { type: Object, required: true },
  svgId: { type: String, required: true },
  // the first field (rows) and the second (columns)
  fieldConfig: { type: Object, required: true },
  colFieldConfig: { type: Object, default: null },
  width: { type: Number, default: 400 },
  height: { type: Number, default: 300 },
  colorScheme: { type: String, default: 'rainbow' },
  // the most columns drawn: the second field's values with the most sessions
  // across every row
  maxCols: { type: Number, default: 30 }
});
const emit = defineEmits(['show-tooltip', 'hide-tooltip']);
const { t } = useI18n();

const CHAR_PX = 6; // ~width of a 10px label character
const COL_LABEL_H = 70;
const MIN_CELL = 14;

const interp = ref(null);
watch(() => props.colorScheme, async (scheme) => { interp.value = await heatmapInterpolator(scheme); }, { immediate: true });

// the rows in the order fetched (the first field's top values); without a
// second field each row is one cell of its own count
const rows = computed(() => (props.data?.children || []).filter(r => !r.other));

const cols = computed(() => {
  if (!props.colFieldConfig) { return [{ name: props.fieldConfig?.friendlyName || '', total: 0 }]; }
  const totals = new Map();
  for (const r of rows.value) {
    for (const c of (r.children || [])) {
      if (c.other) { continue; }
      totals.set(c.name, (totals.get(c.name) || 0) + count(c));
    }
  }
  return [...totals.entries()]
    .map(([colName, total]) => ({ name: colName, total }))
    .sort((a, b) => b.total - a.total)
    .slice(0, props.maxCols);
});

// values[row][col]; null when the row has an "Other", so a column it doesn't
// list may still have sessions
const values = computed(() => rows.value.map((r) => {
  if (!props.colFieldConfig) { return [count(r)]; }
  const children = r.children || [];
  const missing = children.some(c => c.other) ? null : 0;
  const byName = new Map(children.filter(c => !c.other).map(c => [c.name, count(c)]));
  return cols.value.map(c => byName.get(c.name) ?? missing);
}));

const known = computed(() => values.value.flat().filter(v => v !== null));
const maxVal = computed(() => Math.max(0, ...known.value));

// the row label gutter takes what the longest value needs, up to a third
const labelW = computed(() => {
  const longest = Math.max(0, ...rows.value.map(r => String(r.name ?? '').length));
  return Math.min(props.width / 3, Math.max(40, longest * CHAR_PX + 10));
});
const colLabelH = computed(() => (props.colFieldConfig ? COL_LABEL_H : 4));
const cellW = computed(() => (cols.value.length ? Math.max(MIN_CELL, (props.width - labelW.value) / cols.value.length) : 0));
const cellH = computed(() => (rows.value.length ? Math.max(MIN_CELL, (props.height - colLabelH.value) / rows.value.length) : 0));
// the cells' minimum size can need more than the space given: the wrapper scrolls
const svgW = computed(() => Math.max(props.width, Math.ceil(labelW.value + cols.value.length * cellW.value + 1)));
const svgH = computed(() => Math.max(props.height, Math.ceil(colLabelH.value + rows.value.length * cellH.value + 1)));

const colLabels = computed(() => (props.colFieldConfig
  ? cols.value.map((c, i) => ({ index: i, name: c.name, x: labelW.value + i * cellW.value + cellW.value / 2 + 4, label: fitLabel(c.name, Math.min(COL_LABEL_H * 1.3, 120), CHAR_PX) }))
  : []));
const rowLabels = computed(() => rows.value.map((r, i) => ({ index: i, name: r.name, y: colLabelH.value + i * cellH.value + cellH.value / 2, label: fitLabel(r.name, labelW.value - 8, CHAR_PX) })));

const cellFill = (v) => {
  if (v <= 0) { return 'rgba(128,128,128,0.08)'; }
  const frac = maxVal.value > 0 ? v / maxVal.value : 0;
  if (interp.value) { return interp.value(0.12 + 0.88 * frac); }
  return `rgba(var(--v-theme-foreground-accent), ${0.15 + 0.85 * frac})`;
};

const cells = computed(() => {
  const out = [];
  values.value.forEach((rowVals, r) => {
    rowVals.forEach((v, c) => {
      const w = cellW.value - 1;
      const h = cellH.value - 1;
      const text = v === null ? '' : commaString(v);
      out.push({
        k: `${r}-${c}`,
        r,
        c,
        value: v,
        x: labelW.value + c * cellW.value + 0.5,
        y: colLabelH.value + r * cellH.value + 0.5,
        w,
        h,
        fill: cellFill(v),
        // a count where it fits; it reads dark on a light (low) cell
        text: v > 0 && w >= text.length * CHAR_PX + 4 && h >= 12 ? text : '',
        dark: maxVal.value > 0 && (v ?? 0) / maxVal.value < 0.45
      });
    });
  });
  return out;
});

const onHover = (cell, evt) => {
  const row = rows.value[cell.r];
  const total = known.value.reduce((a, b) => a + b, 0);
  emit('show-tooltip', {
    data: { item: row.name, sessions: cell.value, value: cell.value, note: cell.value === null ? t('sessions.summary.notInTopValues') : undefined },
    position: { x: evt.clientX + 1, y: evt.clientY + 1 },
    fieldConfig: props.fieldConfig,
    metricType: 'sessions',
    percentage: total > 0 && cell.value !== null ? (cell.value / total) * 100 : null,
    split: props.colFieldConfig ? { label: props.colFieldConfig.friendlyName, value: cols.value[cell.c].name } : null
  });
};
</script>

<style scoped>
.chart-wrapper {
  width: 100%;
  height: 100%;
  overflow: auto;
}
.matrix-label {
  fill: rgb(var(--v-theme-foreground));
  font-size: 10px;
}
.matrix-cell {
  cursor: pointer;
}
.matrix-value {
  fill: #fff;
  font-size: 10px;
  pointer-events: none;
}
.matrix-value--dark {
  fill: rgb(var(--v-theme-foreground));
}
.matrix-unknown-bg {
  fill: rgba(128, 128, 128, 0.08);
}
.matrix-unknown-line {
  stroke: rgba(128, 128, 128, 0.35);
  stroke-width: 1.5;
}
</style>
