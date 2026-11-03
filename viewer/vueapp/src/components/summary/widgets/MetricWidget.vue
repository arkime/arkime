<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0

Metric widget: one big number — the chosen metric (session count, or a field's
sum/avg/min/max/unique count) over the whole result set — or one number per
top-N value when a split field is chosen. Self-fetches /api/sessions/summary
(global search AND the widget's local filter).
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
    <div
      class="metric-grid"
      :class="{ 'metric-grid--single': !fieldObj }">
      <div
        v-for="(row, i) in rows"
        :key="i"
        class="metric-card">
        <div
          v-if="fieldObj && !row.range"
          class="metric-label">
          <arkime-session-field
            :field="fieldObj"
            :value="row.item"
            :expr="fieldObj.exp"
            :parse="true"
            :session-btn="true"
            :pull-left="true" />
        </div>
        <!-- a range bucket ('0-9') is no single value to search for -->
        <div
          v-else-if="fieldObj"
          class="metric-label">
          {{ row.item }}
        </div>
        <div
          v-else
          class="metric-label">
          {{ metricLabel }}
        </div>
        <div
          class="metric-value"
          :style="valueColor(row.value)"
          :title="String(row.value)">
          {{ format(row.value) }}
        </div>
        <!-- the change from the same window a day or a week earlier -->
        <div
          v-if="!fieldObj && previous !== null"
          class="metric-compare"
          :title="`${compareLabel}: ${String(previous)}`">
          <span :class="changeClass(row.value)">{{ change(row.value) }}</span>
          · {{ compareLabel }} {{ format(previous) }}
        </div>
        <div
          v-if="fieldObj"
          class="metric-sublabel">
          {{ metricLabel }}
        </div>
        <!-- the metric over the time range, bucket by bucket, under the number -->
        <svg
          v-else-if="sparkPath"
          class="metric-spark"
          viewBox="0 0 100 24"
          preserveAspectRatio="none">
          <path
            :d="sparkPath + ' L100,24 L0,24 Z'"
            class="metric-spark__fill" />
          <path
            :d="sparkPath"
            class="metric-spark__line" />
        </svg>
      </div>
    </div>
  </WidgetCard>
</template>

<script setup>
import { ref, computed, watch, onBeforeUnmount } from 'vue';
import { useI18n } from 'vue-i18n';
import { useRoute } from 'vue-router';
import { useStore } from 'vuex';
import WidgetCard from './WidgetCard.vue';
import ArkimeSessionField from '../../sessions/SessionField.vue';
import FieldService from '../../search/FieldService';
import { useSpigraphWidget } from './useSpigraphWidget';
import { fetchMetric, fetchGraph, metricHistoKey, formatMetricValue, metricLabel as metricLabelFor, compareOffset, thresholdColor } from './widgetData';
import { metricSeries } from '../../visualizations/metrics.js';

const { t } = useI18n();
const route = useRoute();
const store = useStore();

const props = defineProps({
  widget: { type: Object, required: true },
  reloadNonce: { type: Number, default: 0 },
  infoItems: { type: Array, default: () => [] },
  link: { type: Object, default: null }
});

defineEmits(['edit', 'remove']);

const rows = ref([]); // [{ item, sessions, value, metricValues }]
// the single number over the same window a day or a week earlier, null when
// not compared (a split metric, an all time search)
const previous = ref(null);

const { loading, error, fetchData } = useSpigraphWidget(
  () => props.widget,
  () => props.reloadNonce,
  (res) => {
    rows.value = Array.isArray(res.rows) ? res.rows : [];
    previous.value = res.previous ?? null;
    loadSpark();
  },
  async (r, s, widget, opts) => {
    const shift = widget.field ? 0 : compareOffset(widget, s);
    const [now, prev] = await Promise.all([
      fetchMetric(r, s, widget, opts),
      shift ? fetchMetric(r, s, widget, { ...opts, timeShift: shift }) : null
    ]);
    return { rows: now, previous: prev ? (prev[0]?.value ?? 0) : null };
  }
);

const compareLabel = computed(() => (props.widget.compare ? t(`sessions.summary.widget.compare-${props.widget.compare}`) : ''));
// +38% / -12% from the earlier window (an arrow says which way); with nothing
// in the earlier window there is no percentage, so the whole value is the rise
const change = (value) => {
  const prev = previous.value;
  if (!prev) { return value ? `▲ ${format(value)}` : '= 0%'; }
  const pct = (value - prev) / prev * 100;
  if (Math.abs(pct) < 0.05) { return '= 0%'; }
  return `${pct > 0 ? '▲' : '▼'} ${Math.abs(pct) >= 10 ? Math.round(Math.abs(pct)) : Math.abs(pct).toFixed(1)}%`;
};
const changeClass = (value) => (value > (previous.value || 0) ? 'metric-compare--up' : value < (previous.value || 0) ? 'metric-compare--down' : '');
// amber or red at the widget's warning and critical levels
const valueColor = (value) => {
  const color = thresholdColor(props.widget, value);
  return color ? { color } : {};
};

// the trend line: the metric's timeline (the same <dbField>Histo series the
// timeline widget draws) fetched beside the number, for a summed metric over
// the whole result set
const spark = ref(null); // [[ms, value], ...] summed over the metric's histo keys
let sparkController;
const wantsSpark = computed(() => props.widget.sparkline && !props.widget.field &&
  ((props.widget.metricType || 'sessions') === 'sessions' || (props.widget.metricAgg || 'sum') === 'sum'));
const loadSpark = async () => {
  if (sparkController) { sparkController.abort(); }
  if (!wantsSpark.value) { spark.value = null; return; }
  sparkController = new AbortController();
  try {
    const res = await fetchGraph(route, store, props.widget, { signal: sparkController.signal });
    // buckets without sessions aren't returned: drop to zero across a gap
    // rather than drawing a line over it
    const pts = metricSeries(res.graph, metricHistoKey(props.widget));
    const step = (res.graph?.interval || 0) * 1000;
    const filled = [];
    for (const p of pts) {
      const prev = filled[filled.length - 1];
      if (step > 0 && prev && p[0] - prev[0] > step) {
        filled.push([prev[0] + step, 0]);
        if (p[0] - prev[0] > 2 * step) { filled.push([p[0] - step, 0]); }
      }
      filled.push(p);
    }
    spark.value = filled;
  } catch (err) {
    if (err.name !== 'AbortError') { spark.value = null; } // the number still shows
  }
};
watch(wantsSpark, loadSpark);
onBeforeUnmount(() => { if (sparkController) { sparkController.abort(); } });

// the line in a 100 x 24 box: x by time, y by value, so it scales with the card
const sparkPath = computed(() => {
  const pts = spark.value;
  if (!pts || pts.length < 2) { return ''; }
  const max = Math.max(...pts.map(p => p[1]), 0) || 1;
  const t0 = pts[0][0];
  const span = (pts[pts.length - 1][0] - t0) || 1;
  return pts.map(([ts, v], i) => `${i ? 'L' : 'M'}${((ts - t0) / span * 100).toFixed(2)},${(24 - (v / max) * 22).toFixed(2)}`).join(' ');
});

const fieldObj = computed(() => (props.widget.field ? FieldService.getField(props.widget.field, true) : null));
const hasData = computed(() => rows.value.length > 0);

const metricType = computed(() => props.widget.metricType || 'sessions');
const metricAgg = computed(() => props.widget.metricAgg || 'sum');

// "Sessions", "Bytes" or "<Agg> <Field>" (eg "Max Bytes"), same as every other widget
const metricLabel = computed(() => metricLabelFor(metricType.value, metricAgg.value, t));

const title = computed(() => props.widget.title ||
  (fieldObj.value ? `${metricLabel.value} / ${fieldObj.value.friendlyName}` : metricLabel.value));

const format = (v) => formatMetricValue(metricType.value, v, metricAgg.value);
</script>

<style scoped>
.metric-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(120px, 1fr));
  gap: 0.4rem;
  align-content: center;
  flex: 1;
  overflow: auto;
}
.metric-grid--single {
  grid-template-columns: 1fr;
}
.metric-card {
  padding: 0.35rem 0.5rem;
  border-radius: 6px;
  background: rgb(var(--v-theme-quaternary-lightest));
  text-align: center;
  min-width: 0;
}
.metric-label {
  font-size: 0.8rem;
  opacity: 0.85;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}
.metric-value {
  font-size: 2rem;
  font-weight: 700;
  line-height: 1.15;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}
.metric-grid--single .metric-value {
  font-size: 3rem;
}
.metric-compare {
  font-size: 0.75rem;
  opacity: 0.85;
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}
.metric-compare--up,
.metric-compare--down {
  font-weight: 600;
}
.metric-sublabel {
  font-size: 0.7rem;
  opacity: 0.7;
}
.metric-spark {
  display: block;
  width: 100%;
  height: 28px;
  margin-top: 0.25rem;
}
.metric-spark__line {
  fill: none;
  stroke: rgb(var(--v-theme-foreground-accent));
  stroke-width: 1.5;
  vector-effect: non-scaling-stroke;
}
.metric-spark__fill {
  fill: rgb(var(--v-theme-foreground-accent));
  opacity: 0.15;
}
</style>
