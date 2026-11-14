<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0

Tag cloud: a field's top values as words, each sized by its metric. Laid out
as wrapped rows in an SVG (so the PNG export works like the other charts),
largest values first. Hovering a word shows the shared chart tooltip and
clicking it adds the value to the search.
-->
<template>
  <div class="chart-wrapper">
    <svg
      :id="svgId"
      class="tagcloud"
      :width="width"
      :height="height"
      :viewBox="`0 0 ${width} ${height}`">
      <text
        v-for="w in words"
        :key="w.key"
        :x="w.x"
        :y="w.y"
        :font-size="w.size"
        :fill="w.color"
        text-anchor="middle"
        class="tagcloud-word"
        @mouseover="onHover(w, $event)"
        @mouseleave="emit('hide-tooltip')"
        @click="onClick(w)">
        {{ w.label }}
      </text>
    </svg>
  </div>
</template>

<script setup>
import { ref, computed, watch, onMounted } from 'vue';
import { useStore } from 'vuex';
import { buildExpression } from '@common/vueFilters.js';
import { colorRange } from './widgets/chartColors';

const props = defineProps({
  data: { type: Array, required: true },
  svgId: { type: String, required: true },
  fieldConfig: { type: Object, required: true },
  width: { type: Number, default: 400 },
  height: { type: Number, default: 300 },
  metricType: { type: String, default: 'sessions' },
  metricAgg: { type: String, default: 'sum' },
  colorScheme: { type: String, default: 'rainbow' }
});

const emit = defineEmits(['show-tooltip', 'hide-tooltip']);
const store = useStore();

const MIN_SIZE = 11;
const MAX_SIZE = 44;
const GAP_X = 14;
const LINE_PAD = 6;
// an average glyph is ~0.58em wide in the ui font; good enough for wrapping
const CHAR_W = 0.58;
// a value longer than this is cut with an ellipsis (the tooltip has it whole)
const MAX_LABEL = 40;

let d3 = null;
const palette = ref([]);
const loadPalette = async () => {
  if (!d3) { d3 = await import('d3'); }
  palette.value = colorRange(d3, props.colorScheme, props.data.length);
};
onMounted(loadPalette);
watch([() => props.colorScheme, () => props.data.length], loadPalette);

// font size scales with the square root of the value, so a handful of huge
// values don't shrink everything else to the minimum
const sized = computed(() => {
  const rows = (props.data || []).filter(d => d && d.item !== undefined);
  const vals = rows.map(d => Math.sqrt(Math.max(0, d.value || 0)));
  const lo = Math.min(...vals, Infinity);
  const hi = Math.max(...vals, -Infinity);
  return rows.map((d, i) => {
    const t = hi > lo ? (vals[i] - lo) / (hi - lo) : 1;
    const full = String(d.item);
    const label = full.length > MAX_LABEL ? `${full.slice(0, MAX_LABEL - 1)}…` : full;
    return { data: d, label, size: Math.round(MIN_SIZE + t * (MAX_SIZE - MIN_SIZE)), index: i };
  });
});

// greedy row wrap: words in value order, each row centered, rows centered
// vertically; the sizes shrink together when the rows would overflow the box
const words = computed(() => {
  const colors = palette.value;
  const fit = (shrink) => {
    const rows = [];
    let row = [];
    let rowW = 0;
    let rowH = 0;
    for (const w of sized.value) {
      // a word wider than the box shrinks to fit it (it can't wrap)
      const maxSize = (props.width - 8) / Math.max(1, w.label.length * CHAR_W);
      const size = Math.max(8, Math.min(w.size * shrink, maxSize));
      const wW = w.label.length * size * CHAR_W;
      if (row.length && rowW + GAP_X + wW > props.width - 8) {
        rows.push({ words: row, width: rowW, height: rowH });
        row = []; rowW = 0; rowH = 0;
      }
      row.push({ ...w, size, w: wW });
      rowW += (row.length > 1 ? GAP_X : 0) + wW;
      rowH = Math.max(rowH, size);
    }
    if (row.length) { rows.push({ words: row, width: rowW, height: rowH }); }
    const used = rows.reduce((sum, r) => sum + r.height + LINE_PAD, 0);
    return { rows, used };
  };
  let scale = 1;
  let layout = fit(scale);
  // a roomy box (the SPI Graph page) grows the words into it, the largest to
  // a quarter of its height at most; a tight one shrinks them until the rows fit
  const maxScale = Math.max(1, Math.min(4, (props.height / 4) / MAX_SIZE));
  while (scale < maxScale) {
    const next = Math.min(maxScale, scale + 0.1);
    const bigger = fit(next);
    if (bigger.used > props.height) { break; }
    scale = next;
    layout = bigger;
  }
  while (layout.used > props.height && scale > 0.3) {
    scale -= 0.1;
    layout = fit(scale);
  }
  const out = [];
  let y = Math.max(0, (props.height - layout.used) / 2);
  for (const r of layout.rows) {
    y += r.height;
    let x = (props.width - r.width) / 2;
    for (const w of r.words) {
      out.push({
        key: w.index,
        label: w.label,
        size: w.size,
        x: x + w.w / 2,
        y,
        color: colors[w.index % Math.max(1, colors.length)] || 'currentColor',
        data: w.data
      });
      x += w.w + GAP_X;
    }
    y += LINE_PAD;
  }
  return out;
});

const total = computed(() => (props.data || []).reduce((sum, d) => sum + (d.value || 0), 0));

const onHover = (w, evt) => {
  emit('show-tooltip', {
    data: w.data,
    position: { x: evt.clientX + 1, y: evt.clientY + 1 },
    percentage: total.value > 0 ? (w.data.value / total.value) * 100 : null,
    fieldConfig: props.fieldConfig,
    metricType: props.metricType,
    metricAgg: props.metricAgg
  });
};

// the whole value (the label may be cut); a range bucket (bucketInterval)
// has no single value to search for
const onClick = (w) => {
  if (!props.fieldConfig?.exp || w.data.range) { return; }
  store.commit('addToExpression', { expression: buildExpression(props.fieldConfig.exp, String(w.data.item), '==') });
};
</script>

<style scoped>
.chart-wrapper {
  position: relative;
  width: 100%;
  height: 100%;
  flex: 1;
  overflow: hidden;
}
.tagcloud {
  display: block;
  font-family: inherit;
}
.tagcloud-word {
  cursor: pointer;
  font-weight: 600;
}
.tagcloud-word:hover {
  opacity: 0.7;
}
</style>
