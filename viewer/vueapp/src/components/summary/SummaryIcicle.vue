<!--
Copyright Andy Wick
SPDX-License-Identifier: Apache-2.0
Icicle (a flame graph of fields): one row per field, the first field's values
across the top and each value's values of the next field below it, each as
wide as its share of its parent (see icicleLayout). Drawn from the hierarchy
endpoint's tree; with "Other" fetched a row adds up to its parent. Colored by
the first field's value, fading a level at a time, with "Other" in gray.
Clicking a value with more below it zooms into it (it fills the width, its
path stays on top), clicking a row of that path zooms back out to it.
Hovering shows the shared chart tooltip (the value with its field actions,
its parent as its split), which stays up to use them until a click elsewhere.
-->
<template>
  <div class="chart-wrapper">
    <svg
      :id="svgId"
      class="icicle"
      :width="width"
      :height="height">
      <g
        v-for="cell in cells"
        :key="cell.key"
        class="icicle-cell"
        :class="cell.zoom ? `icicle-zoom-${cell.zoom}` : ''"
        @click="onClick(cell)"
        @mouseover="onHover(cell, $event)">
        <rect
          :x="cell.x"
          :y="cell.y"
          :width="Math.max(0, cell.w - 1)"
          :height="Math.max(0, cell.h - 1)"
          rx="2"
          :style="{ fill: cell.fill, fillOpacity: cell.opacity }" />
        <text
          v-if="cell.label"
          :x="cell.x + 4"
          :y="cell.y + Math.min(cell.h / 2, 14) + 3"
          class="icicle-label">
          {{ cell.label }}
        </text>
      </g>
    </svg>
  </div>
</template>

<script setup>
import { ref, computed, watch } from 'vue';
import { useI18n } from 'vue-i18n';
import { colorRange, OTHER_COLOR } from './widgets/chartColors';
import { icicleLayout, iciclePathKey, fitLabel, sharePercent } from '../utils/hierarchyLayout';

const props = defineProps({
  // the hierarchy: { children: [{ name, size | sizeValue, other?, children: [...] }] }
  data: { type: Object, required: true },
  svgId: { type: String, required: true },
  // each level's field, the first field first
  fieldConfigs: { type: Array, required: true },
  width: { type: Number, default: 400 },
  height: { type: Number, default: 300 },
  colorScheme: { type: String, default: 'rainbow' }
});
const emit = defineEmits(['show-tooltip', 'hide-tooltip']);
const { t } = useI18n();

const CHAR_PX = 6; // ~width of a 10px label character

const d3 = ref(null);
// without d3 (its chunk failed to load) the cells keep the accent color
import('d3').then((lib) => { d3.value = lib; }).catch(() => {});

const nameOf = (node) => (node.other ? t('sessions.summary.other') : String(node.name ?? ''));

// the path zoomed into, [] for the whole tree
const zoomPath = ref([]);
// another set of fields starts at the top, new data for the same ones keeps
// the zoom (as deep as it still matches)
watch(() => props.fieldConfigs.map(f => f?.exp).join(','), () => { zoomPath.value = []; });

const layout = computed(() => icicleLayout(props.data, props.width, props.height, { focus: zoomPath.value }));

// the first field's values each keep a color, whatever is zoomed into
const palette = computed(() => {
  const keys = (props.data?.children || []).filter(c => !c.other).map(c => String(c.name ?? ''));
  return d3.value ? d3.value.scaleOrdinal(colorRange(d3.value, props.colorScheme, keys.length)).domain(keys) : null;
});

const byKey = computed(() => new Map((layout.value?.cells || []).map(c => [c.key, c])));

const cells = computed(() => (layout.value?.cells || []).map((c) => {
  const cellName = nameOf(c);
  // the share where the name fits whole with it, else the name (cut to fit)
  const withPct = `${cellName} ${sharePercent(c.share)}`;
  const label = c.h < 12 ? '' : (withPct.length * CHAR_PX <= c.w - 8 ? withPct : fitLabel(cellName, c.w - 8, CHAR_PX));
  return {
    ...c,
    fill: c.underOther ? OTHER_COLOR : (palette.value ? palette.value(c.top) : 'rgb(var(--v-theme-foreground-accent))'),
    opacity: Math.max(0.45, 1 - 0.15 * (c.depth - 1)),
    label,
    // a crumb zooms back out, a value with more below it zooms in
    zoom: c.crumb ? 'out' : (c.more ? 'in' : '')
  };
}));

const onClick = (cell) => {
  if (!cell.zoom) { return; }
  emit('hide-tooltip');
  zoomPath.value = cell.crumb ? cell.path.slice(0, -1) : cell.path;
};

const onHover = (cell, evt) => {
  const parentCell = byKey.value.get(iciclePathKey(cell.path.slice(0, -1)));
  const parentField = props.fieldConfigs[cell.depth - 2];
  emit('show-tooltip', {
    data: { item: nameOf(cell), other: cell.other, sessions: cell.value, value: cell.value },
    position: { x: evt.clientX + 1, y: evt.clientY + 1 },
    fieldConfig: props.fieldConfigs[cell.depth - 1] || props.fieldConfigs[0],
    metricType: 'sessions',
    percentage: cell.share * 100,
    split: parentCell && parentField ? { label: parentField.friendlyName, value: nameOf(parentCell) } : null
  });
};
</script>

<style scoped>
.chart-wrapper {
  width: 100%;
  height: 100%;
  overflow: hidden;
}
.icicle-cell rect {
  stroke: rgb(var(--v-theme-background));
  stroke-width: 1;
}
.icicle-cell:hover rect {
  stroke: rgb(var(--v-theme-foreground));
}
.icicle-zoom-in {
  cursor: zoom-in;
}
.icicle-zoom-out {
  cursor: zoom-out;
}
/* white text with a dark halo stays readable on any fill */
.icicle-label {
  fill: #fff;
  font-size: 10px;
  font-weight: 600;
  paint-order: stroke;
  stroke: rgba(0, 0, 0, 0.55);
  stroke-width: 2px;
  stroke-linejoin: round;
  pointer-events: none;
}
</style>
