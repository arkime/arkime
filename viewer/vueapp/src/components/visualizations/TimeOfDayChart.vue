<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0

Time of day grid: a week of hours, Monday to Sunday down the side and the 24
hours across, each cell colored by how much of the metric fell in that hour of
that weekday over the whole time range. It folds a timeline's hour buckets by
weekday and hour in the user's timezone, so it needs no data of its own: the
same <metric>Histo series the timeline draws. The classic baselining view:
off hours traffic stands out as color where the grid should be dark.
Fills its container (a ResizeObserver resizes it), compact for a small
multiple (a time of day split by a field).
-->
<template>
  <div
    ref="root"
    class="tod-chart"
    :class="{ 'tod-chart--compact': compact }">
    <svg
      v-if="cellW > 0"
      :width="width"
      :height="height"
      @mouseleave="tooltip = null">
      <!-- hour labels across the top -->
      <text
        v-for="h in hourTicks"
        :key="'h' + h"
        :x="labelW + h * cellW + cellW / 2"
        :y="axisH - 4"
        text-anchor="middle"
        class="tod-tick">
        {{ pad(h) }}
      </text>
      <!-- weekday labels down the side -->
      <text
        v-for="(day, d) in dayLabels"
        :key="'d' + d"
        :x="labelW - 4"
        :y="axisH + d * cellH + cellH / 2"
        text-anchor="end"
        dominant-baseline="middle"
        class="tod-tick">
        {{ day }}
      </text>
      <!-- cells -->
      <rect
        v-for="cell in cells"
        :key="cell.k"
        :x="labelW + cell.h * cellW + 0.5"
        :y="axisH + cell.d * cellH + 0.5"
        :width="Math.max(1, cellW - 1)"
        :height="Math.max(1, cellH - 1)"
        :style="{ fill: cell.fill }"
        rx="1"
        @mousemove="onMove($event, cell)" />
    </svg>
    <div
      v-if="tooltip"
      class="tod-tooltip"
      :style="{ left: tooltip.x + 'px', top: tooltip.y + 'px' }">
      <div><strong>{{ tooltip.when }}</strong></div>
      <div>{{ metricLabel }}: {{ tooltip.value }}</div>
      <div
        v-if="tooltip.count > 1"
        class="tod-tooltip-note">
        {{ $t('vis.timeofdayHours', { count: tooltip.count }) }}
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref, computed, watch, onMounted, onBeforeUnmount } from 'vue';
import { useI18n } from 'vue-i18n';
import moment from 'moment-timezone';
import { metricHistoKeys } from './metrics.js';
import { heatmapInterpolator } from '../summary/widgets/chartColors';
import { commaString, humanReadableBytes, humanReadableNumber } from '@common/vueFilters.js';

const props = defineProps({
  // the timeline graph: { <metric>Histo: [[ms, value], ...], interval, ... }
  graph: { type: Object, required: true },
  // the <dbField>Histo key to fold (a composite metric sums its parts)
  metric: { type: String, default: 'sessionsHisto' },
  metricLabel: { type: String, default: '' },
  timezone: { type: String, default: 'local' },
  // an optional dashboard palette; a categorical one keeps the accent shade
  colorScheme: { type: String, default: '' },
  // a small multiple: labels every 6 hours, no fixed minimum cell size
  compact: { type: Boolean, default: false }
});

const { locale } = useI18n();

const root = ref(null);
const width = ref(0);
const height = ref(0);
const tooltip = ref(null);
const interp = ref(null);

const HOURS = 24;
const DAYS = 7;

const pad = (h) => String(h).padStart(2, '0');

const labelW = computed(() => (props.compact ? 26 : 34));
const axisH = computed(() => (props.compact ? 12 : 16));
const hourTicks = computed(() => {
  const step = props.compact ? 6 : 3;
  const out = [];
  for (let h = 0; h < HOURS; h += step) { out.push(h); }
  return out;
});
const cellW = computed(() => Math.max(0, (width.value - labelW.value) / HOURS));
const cellH = computed(() => Math.max(0, (height.value - axisH.value) / DAYS));

// Mon..Sun in the user's language (the 5th of January 1970 was a Monday); a
// tag Intl doesn't know (the x-pl pseudo-locale) falls back to English
const dayLabels = computed(() => {
  const opts = { weekday: props.compact ? 'narrow' : 'short', timeZone: 'UTC' };
  let fmt;
  try {
    fmt = new Intl.DateTimeFormat(locale.value, opts);
  } catch (err) {
    fmt = new Intl.DateTimeFormat('en', opts);
  }
  return Array.from({ length: DAYS }, (_, d) => fmt.format(new Date(Date.UTC(1970, 0, 5 + d))));
});

const isBytes = computed(() => props.metric.toLowerCase().includes('bytes'));
const formatVal = (v) => (isBytes.value ? humanReadableBytes(v) : humanReadableNumber(v));

// the timezone setting is 'local', 'localtz' (the browser's zone by name) or
// 'gmt', the same three timezoneDateString reads; a moment in that zone
const inZone = (ts) => {
  if (props.timezone === 'gmt') { return moment.tz(ts, 'utc'); }
  if (props.timezone === 'localtz') { return moment.tz(ts, Intl.DateTimeFormat().resolvedOptions().timeZone); }
  return moment(ts);
};

// the week: [day][hour] -> { value, count } summed from every bucket of the
// metric's series, placed by the bucket's start in the user's timezone
const grid = computed(() => {
  const out = Array.from({ length: DAYS }, () => Array.from({ length: HOURS }, () => ({ value: 0, count: 0 })));
  for (const key of metricHistoKeys(props.metric)) {
    for (const [ts, val] of (props.graph?.[key] || [])) {
      const m = inZone(ts);
      const d = (m.day() + 6) % 7; // Monday first
      const cell = out[d][m.hour()];
      cell.value += val || 0;
      cell.count++;
    }
  }
  return out;
});

const maxVal = computed(() => {
  let max = 0;
  for (const row of grid.value) { for (const c of row) { if (c.value > max) { max = c.value; } } }
  return max;
});

const cellFill = (v) => {
  if (v <= 0) { return 'rgba(128,128,128,0.08)'; }
  const t = maxVal.value > 0 ? v / maxVal.value : 0;
  if (interp.value) { return interp.value(0.12 + 0.88 * t); }
  return `rgba(var(--v-theme-foreground-accent), ${0.15 + 0.85 * t})`;
};

const cells = computed(() => {
  const out = [];
  grid.value.forEach((row, d) => {
    row.forEach((c, h) => {
      out.push({ k: `${d}-${h}`, d, h, value: c.value, count: c.count, fill: cellFill(c.value) });
    });
  });
  return out;
});

const onMove = (e, cell) => {
  const rect = root.value?.getBoundingClientRect();
  if (!rect) { return; }
  const x = e.clientX - rect.left;
  tooltip.value = {
    // flip to the left of the cursor on the right half, so it stays in the box
    x: x > rect.width * 0.6 ? x - 150 : x + 12,
    y: e.clientY - rect.top + 12,
    when: `${dayLabels.value[cell.d]} ${pad(cell.h)}:00–${pad((cell.h + 1) % HOURS)}:00`,
    value: isBytes.value ? formatVal(cell.value) : commaString(Math.round(cell.value * 100) / 100),
    count: cell.count
  };
};

const measure = () => {
  if (!root.value) { return; }
  width.value = root.value.clientWidth;
  height.value = root.value.clientHeight;
};

let ro;
onMounted(() => {
  measure();
  if (typeof ResizeObserver !== 'undefined' && root.value) {
    ro = new ResizeObserver(measure);
    ro.observe(root.value);
  }
});
onBeforeUnmount(() => { if (ro) { ro.disconnect(); } });

watch(() => props.colorScheme, async (scheme) => { interp.value = await heatmapInterpolator(scheme); }, { immediate: true });
</script>

<style scoped>
.tod-chart {
  position: relative;
  width: 100%;
  height: 100%;
  min-height: 0;
  overflow: hidden;
}
.tod-chart svg {
  display: block;
}
.tod-tick {
  fill: rgb(var(--v-theme-foreground));
  opacity: 0.7;
  font-size: 10px;
  font-family: ui-monospace, "SF Mono", Menlo, Consolas, monospace;
}
.tod-chart--compact .tod-tick {
  font-size: 9px;
}
.tod-tooltip {
  position: absolute;
  z-index: 5;
  pointer-events: none;
  padding: 0.25rem 0.5rem;
  border-radius: 4px;
  font-size: 0.75rem;
  white-space: nowrap;
  background: rgb(var(--v-theme-background));
  color: rgb(var(--v-theme-foreground));
  border: 1px solid rgb(var(--v-theme-foreground-accent));
  box-shadow: 0 2px 6px rgba(0, 0, 0, 0.25);
}
.tod-tooltip-note {
  opacity: 0.7;
}
</style>
