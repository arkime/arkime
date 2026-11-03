<!--
Copyright Andy Wick
SPDX-License-Identifier: Apache-2.0
Waffle: a 10x10 grid of squares, each 1% of the whole, colored by the field's
top values in order (and a gray "Other" for what falls outside them, when
asked for), with a legend of each value's share beside it where it fits. A
split waffle draws one small waffle per split value (groups), each its own
100%, a value keeping one color in all of them; its legend names the values.
Hovering a square or a legend row shows the shared chart tooltip and
highlights that value's squares (in every waffle).
-->
<template>
  <div class="chart-wrapper">
    <svg
      :id="svgId"
      :width="width"
      :height="height"
      @mouseleave="hover = null">
      <g
        v-for="(panel, p) in drawn"
        :key="'p' + p">
        <text
          v-if="panel.title"
          :x="panel.titleX"
          :y="panel.titleY"
          text-anchor="middle"
          class="waffle-title">
          {{ panel.title }}
          <title>{{ panel.name }}</title>
        </text>
        <rect
          v-for="(c, i) in panel.cells"
          :key="'s' + i"
          :x="c.x"
          :y="c.y"
          :width="panel.size"
          :height="panel.size"
          :rx="Math.min(3, panel.size / 5)"
          class="waffle-cell"
          :style="{ fill: fillOf(panel.parts[c.index]), opacity: hover === null || hover === keyOf(panel.parts[c.index]) ? 1 : 0.3 }"
          @mouseover="onHover(p, c.index, $event)" />
      </g>
      <g
        v-for="row in legend"
        :key="'l' + row.key"
        class="waffle-legend-row"
        @mouseover="row.part && onLegendHover(row, $event)">
        <rect
          v-if="row.part"
          :x="legendX"
          :y="row.y"
          width="10"
          height="10"
          rx="2"
          :style="{ fill: fillOf(row.part) }" />
        <text
          :x="legendX + 15"
          :y="row.y + 9"
          class="waffle-legend-text">
          {{ row.label }}
          <title>{{ row.name }}</title>
        </text>
      </g>
    </svg>
  </div>
</template>

<script setup>
import { ref, computed, watch } from 'vue';
import { useI18n } from 'vue-i18n';
import { colorRange, OTHER_COLOR } from './widgets/chartColors';
import { metricIsAdditive } from './widgets/widgetData';
import { apportionCells, waffleGrid, splitWaffleLayout } from '../utils/waffleLayout';
import { fitLabel, sharePercent } from '../utils/hierarchyLayout';

const props = defineProps({
  // the top values: [{ item, sessions, value }]
  data: { type: Array, default: () => [] },
  // the sessions outside the top N ({ sessions, value }), a last gray value
  other: { type: Object, default: null },
  // a split waffle: one waffle per split value, [{ name, items (as data), other (as other) }]
  groups: { type: Array, default: null },
  svgId: { type: String, required: true },
  fieldConfig: { type: Object, required: true },
  // the split field (its value is a split waffle's tooltip split)
  splitFieldConfig: { type: Object, default: null },
  width: { type: Number, default: 400 },
  height: { type: Number, default: 300 },
  metricType: { type: String, default: 'sessions' },
  metricAgg: { type: String, default: 'sum' },
  colorScheme: { type: String, default: 'rainbow' }
});
const emit = defineEmits(['show-tooltip']);
const { t } = useI18n();

const CHAR_PX = 6; // ~width of an 11px legend character
const ROW_H = 16;
const LEGEND_GAP = 12;
const TITLE_H = 16;

// a value's key: its name, or Other (one per waffle, the same key in all)
const OTHER_KEY = '\u0000other';
const keyOf = (part) => (part.other ? OTHER_KEY : String(part.item ?? ''));

// the hovered value's key (null: none)
const hover = ref(null);

// a waffle's values: the top N, then "Other" when it holds anything
const partsOf = (items, other) => {
  const topValues = (items || []).filter(d => d.value > 0);
  return other?.value > 0
    ? [...topValues, { item: t('sessions.summary.other'), other: true, sessions: other.sessions, value: other.value }]
    : topValues;
};
const panels = computed(() => (props.groups
  ? props.groups.map(g => ({ name: String(g.name ?? ''), parts: partsOf(g.items, g.other) })).filter(g => g.parts.length)
  : [{ name: '', parts: partsOf(props.data, props.other) }]));
const split = computed(() => !!props.groups);

// every value across the waffles, the biggest first and Other last: the
// palette order and the legend
const values = computed(() => {
  if (!split.value) { return panels.value[0]?.parts || []; }
  const totals = new Map();
  let other = null;
  for (const panel of panels.value) {
    for (const part of panel.parts) {
      if (part.other) { other = other || part; continue; }
      const key = keyOf(part);
      const prev = totals.get(key);
      totals.set(key, { part, total: (prev?.total || 0) + part.value });
    }
  }
  const sorted = [...totals.values()].sort((a, b) => b.total - a.total);
  return [...sorted.map(v => v.part), ...(other ? [other] : [])];
});

const d3 = ref(null);
// without d3 (its chunk failed to load) the squares keep the accent color
import('d3').then((lib) => { d3.value = lib; }).catch(() => {});
const palette = computed(() => {
  const keys = values.value.filter(v => !v.other).map(keyOf);
  return d3.value ? d3.value.scaleOrdinal(colorRange(d3.value, props.colorScheme, keys.length)).domain(keys) : null;
});
const fillOf = (part) => (part.other ? OTHER_COLOR : (palette.value ? palette.value(keyOf(part)) : 'rgb(var(--v-theme-foreground-accent))'));

// a legend beside the waffles when the widget is wide enough for one
const legendW = computed(() => (props.width >= 240 ? Math.min(220, props.width * (split.value ? 0.3 : 0.45)) : 0));
const areaW = computed(() => props.width - legendW.value - (legendW.value ? LEGEND_GAP : 0));

const drawn = computed(() => {
  const n = panels.value.length;
  const boxes = split.value
    ? splitWaffleLayout(n, areaW.value, props.height, TITLE_H)
    : [{ x: 0, y: 0, side: Math.min(props.height, areaW.value) }];
  // no box (a split in a box without height) draws no waffle
  if (boxes.length < n) { return []; }
  return panels.value.map((panel, i) => {
    const box = boxes[i];
    const grid = waffleGrid(apportionCells(panel.parts.map(p => p.value)), box.side, box.side, { gap: box.side > 150 ? 3 : (box.side > 60 ? 2 : 1) });
    const total = panel.parts.reduce((sum, p) => sum + p.value, 0);
    return {
      ...panel,
      total,
      size: grid?.size || 0,
      cells: (grid?.cells || []).map(c => ({ ...c, x: c.x + box.x, y: c.y + box.y })),
      title: split.value ? fitLabel(panel.name, box.cellW - 8, CHAR_PX) : '',
      titleX: box.titleX,
      titleY: box.titleY
    };
  });
});
const legendX = computed(() => (split.value ? areaW.value : Math.min(props.height, areaW.value)) + LEGEND_GAP);

// one row per value while they fit, the last row says how many more there
// are; a single waffle's rows carry each value's share
const legend = computed(() => {
  if (!legendW.value) { return []; }
  const all = values.value;
  const fit = Math.max(1, Math.floor(props.height / ROW_H));
  const shown = all.length > fit ? fit - 1 : all.length;
  const y0 = Math.max(0, (props.height - Math.min(all.length, fit) * ROW_H) / 2);
  const total = drawn.value[0]?.total || 0;
  const rows = all.slice(0, shown).map((part, i) => {
    const item = String(part.item ?? '');
    const pct = split.value || !total ? '' : ` ${sharePercent(part.value / total)}`;
    return { key: keyOf(part), part, name: item, y: y0 + i * ROW_H, label: `${fitLabel(item, legendW.value - 15 - pct.length * CHAR_PX, CHAR_PX)}${pct}` };
  });
  if (shown < all.length) {
    const more = t('sessions.summary.widget.moreValues', { count: all.length - shown });
    rows.push({ key: OTHER_KEY + 'more', part: null, name: more, y: y0 + shown * ROW_H, label: more });
  }
  return rows;
});

const showTooltip = (part, panel, evt) => {
  emit('show-tooltip', {
    data: part,
    position: { x: evt.clientX + 1, y: evt.clientY + 1 },
    // averages, extremes and unique counts have no share of a total
    percentage: panel && panel.total > 0 && metricIsAdditive(props.metricType, props.metricAgg) ? part.value / panel.total * 100 : null,
    fieldConfig: props.fieldConfig,
    metricType: props.metricType,
    metricAgg: props.metricAgg,
    split: panel && split.value && props.splitFieldConfig ? { label: props.splitFieldConfig.friendlyName, value: panel.name } : null
  });
};

const onHover = (p, index, evt) => {
  const panel = drawn.value[p];
  const part = panel.parts[index];
  hover.value = keyOf(part);
  showTooltip(part, panel, evt);
};

// a single waffle's legend row is its value there; a split one's is the
// value's total across the waffles
const onLegendHover = (row, evt) => {
  hover.value = row.key;
  if (split.value) {
    const sessions = drawn.value.reduce((sum, panel) => sum + (panel.parts.find(p => keyOf(p) === row.key)?.sessions || 0), 0);
    const value = drawn.value.reduce((sum, panel) => sum + (panel.parts.find(p => keyOf(p) === row.key)?.value || 0), 0);
    showTooltip({ ...row.part, sessions, value }, null, evt);
  } else {
    showTooltip(row.part, drawn.value[0], evt);
  }
};

// new values clear the highlight (the same values again, on a parent's re-render, don't)
watch(() => panels.value.map(p => `${p.name}:${p.parts.map(keyOf).join(',')}`).join('|'), () => { hover.value = null; });
</script>

<style scoped>
.chart-wrapper {
  position: relative;
  width: 100%;
  height: 100%;
  flex: 1;
  overflow: hidden;
}
.waffle-cell {
  cursor: pointer;
  transition: opacity 0.1s ease;
}
.waffle-legend-row {
  cursor: pointer;
}
.waffle-legend-text {
  fill: rgb(var(--v-theme-foreground));
  font-size: 11px;
}
.waffle-title {
  fill: rgb(var(--v-theme-foreground));
  font-size: 11px;
  font-weight: 600;
}
</style>
