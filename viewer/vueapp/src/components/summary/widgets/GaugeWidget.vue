<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0

Gauge widget: one metric over the whole result set (session count, or a
field's sum/avg/min/max/unique count) drawn as an arc against a maximum, with
the arc colored by the widget's warning / critical thresholds. Self-fetches
/api/sessions/summary (global search AND the widget's local filter) the same
way the metric widget does.
-->
<template>
  <WidgetCard
    :title="title"
    :loading="loading"
    :error="error"
    :has-data="hasData"
    :info-items="infoItems"
    :link="link"
    @edit="$emit('edit')"
    @remove="$emit('remove')"
    @retry="fetchData">
    <div class="gauge-widget">
      <svg
        class="gauge-svg"
        viewBox="0 0 200 120"
        preserveAspectRatio="xMidYMid meet"
        role="img"
        :aria-label="`${title}: ${formatted}`">
        <!-- the track -->
        <path
          :d="arcPath(1)"
          class="gauge-track"
          fill="none"
          stroke-linecap="round"
          stroke-width="14" />
        <!-- the value -->
        <path
          v-if="fraction > 0"
          :d="arcPath(fraction)"
          :stroke="color"
          fill="none"
          stroke-linecap="round"
          stroke-width="14" />
        <!-- threshold ticks -->
        <line
          v-for="tick in ticks"
          :key="tick.key"
          :x1="tick.x1"
          :y1="tick.y1"
          :x2="tick.x2"
          :y2="tick.y2"
          :stroke="tick.color"
          stroke-width="2" />
        <text
          x="100"
          y="96"
          text-anchor="middle"
          class="gauge-value"
          :fill="color">
          {{ formatted }}
        </text>
        <text
          x="100"
          y="114"
          text-anchor="middle"
          class="gauge-label">
          {{ metricLabel }}<template v-if="max"> / {{ formatMax }}</template>
        </text>
      </svg>
    </div>
  </WidgetCard>
</template>

<script setup>
import { ref, computed } from 'vue';
import { useI18n } from 'vue-i18n';
import WidgetCard from './WidgetCard.vue';
import { useSpigraphWidget } from './useSpigraphWidget';
import { fetchMetric, formatMetricValue, metricLabel as metricLabelFor } from './widgetData';

const { t } = useI18n();

const props = defineProps({
  widget: { type: Object, required: true },
  reloadNonce: { type: Number, default: 0 },
  infoItems: { type: Array, default: () => [] },
  link: { type: Object, default: null }
});

defineEmits(['edit', 'remove']);

const rows = ref([]); // [{ item: '', sessions, value, metricValues }]

// the gauge is always the whole result set: no field, whatever the widget carries
const { loading, error, fetchData } = useSpigraphWidget(
  () => ({ ...props.widget, field: '', fields: [] }),
  () => props.reloadNonce,
  (res) => { rows.value = Array.isArray(res) ? res : []; },
  fetchMetric
);

const hasData = computed(() => rows.value.length > 0);
const value = computed(() => rows.value[0]?.value || 0);

const metricType = computed(() => props.widget.metricType || 'sessions');
const metricAgg = computed(() => props.widget.metricAgg || 'sum');
const metricLabel = computed(() => metricLabelFor(metricType.value, metricAgg.value, t));
const title = computed(() => props.widget.title || `${t('sessions.summary.gaugeView')}: ${metricLabel.value}`);

const format = (v) => formatMetricValue(metricType.value, v, metricAgg.value);
const formatted = computed(() => format(value.value));

// the arc's full value: the configured maximum, else the value itself (a full arc)
const max = computed(() => (props.widget.gaugeMax > 0 ? props.widget.gaugeMax : 0));
const formatMax = computed(() => format(max.value));
const fraction = computed(() => {
  if (!max.value) { return value.value > 0 ? 1 : 0; }
  return Math.min(1, Math.max(0, value.value / max.value));
});

// green below the warning threshold, amber from it, red from the critical one
const GREEN = '#43a047';
const AMBER = '#fb8c00';
const RED = '#e53935';
const color = computed(() => {
  const { gaugeWarn, gaugeCrit } = props.widget;
  if (gaugeCrit > 0 && value.value >= gaugeCrit) { return RED; }
  if (gaugeWarn > 0 && value.value >= gaugeWarn) { return AMBER; }
  return GREEN;
});

// a half circle from the left (180°) to the right (0°), centered at (100,100)
const CX = 100;
const CY = 100;
const R = 80;
const point = (frac) => {
  const angle = Math.PI - frac * Math.PI;
  return [CX + R * Math.cos(angle), CY - R * Math.sin(angle)];
};
const arcPath = (frac) => {
  const f = Math.min(1, Math.max(0.001, frac));
  const [x0, y0] = point(0);
  const [x1, y1] = point(f);
  return `M ${x0} ${y0} A ${R} ${R} 0 0 1 ${x1} ${y1}`;
};

// threshold marks on the track, when a maximum gives them a position
const ticks = computed(() => {
  if (!max.value) { return []; }
  const marks = [];
  for (const [key, val, c] of [['warn', props.widget.gaugeWarn, AMBER], ['crit', props.widget.gaugeCrit, RED]]) {
    if (!(val > 0) || val > max.value) { continue; }
    const frac = val / max.value;
    const angle = Math.PI - frac * Math.PI;
    const inner = R - 11;
    const outer = R + 11;
    marks.push({
      key,
      color: c,
      x1: CX + inner * Math.cos(angle),
      y1: CY - inner * Math.sin(angle),
      x2: CX + outer * Math.cos(angle),
      y2: CY - outer * Math.sin(angle)
    });
  }
  return marks;
});
</script>

<style scoped>
.gauge-widget {
  flex: 1;
  min-height: 0;
  display: flex;
  align-items: center;
  justify-content: center;
}
.gauge-svg {
  width: 100%;
  height: 100%;
  max-height: 100%;
}
.gauge-track {
  stroke: rgb(var(--v-theme-quaternary-lightest));
}
.gauge-value {
  font-size: 26px;
  font-weight: 700;
}
.gauge-label {
  font-size: 10px;
  fill: rgb(var(--v-theme-foreground));
  opacity: 0.75;
}
</style>
