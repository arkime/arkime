<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
-->
<template>
  <div class="chart-wrapper">
    <div
      ref="chartContainer"
      class="chart" />
  </div>
</template>

<script setup>
import { ref, computed, watch, onMounted } from 'vue';
import { useI18n } from 'vue-i18n';
import { colorRange } from './widgets/chartColors';
import { metricIsAdditive } from './widgets/widgetData';

const { t } = useI18n();

const props = defineProps({
  data: {
    type: Array,
    required: true
  },
  svgId: {
    type: String,
    required: true
  },
  fieldConfig: {
    type: Object,
    required: true
  },
  // the split (second) field's config, when bars are split
  splitFieldConfig: {
    type: Object,
    default: null
  },
  // 'stacked' | 'grouped' segment layout for split bars
  barMode: {
    type: String,
    default: 'stacked'
  },
  width: {
    type: Number,
    default: 500
  },
  height: {
    type: Number,
    default: 350
  },
  metricType: {
    type: String,
    default: 'sessions'
  },
  // sum/avg/min/max/cardinality: only sums (and counts) get an "other" remainder
  metricAgg: {
    type: String,
    default: 'sum'
  },
  colorScheme: {
    type: String,
    default: 'rainbow'
  }
});

const emit = defineEmits(['show-tooltip']);

const chartContainer = ref(null);
let d3 = null;

const OTHER_KEY = '\u0000other';
const MARGIN = { top: 12, right: 10, bottom: 120, left: 44 };
const MAX_BAR_WIDTH = 120; // Maximum width per bar to prevent overly wide bars
const LEGEND_HEIGHT = 26;

const isSplit = computed(() => !!props.splitFieldConfig && props.data.some(d => Array.isArray(d.split)));
// the remainder of a value not covered by its top-N sub-values; only additive
// aggregations (counts and sums) have one
const additive = computed(() => metricIsAdditive(props.metricType, props.metricAgg));

// each bar's segments: its sub-values (+ remainder) keyed for color lookup
const segmentsFor = (d) => {
  const segs = (d.split || []).map(s => ({ key: s.item, sessions: s.sessions || 0, value: s.value || 0 }));
  if (additive.value) {
    const rest = (d.value || 0) - segs.reduce((sum, s) => sum + s.value, 0);
    if (rest > 0) {
      const restSessions = (d.sessions || 0) - segs.reduce((sum, s) => sum + s.sessions, 0);
      segs.push({ key: OTHER_KEY, sessions: Math.max(0, restSessions), value: rest });
    }
  }
  return segs;
};

// sub-value keys in order of first appearance (bars are already ranked), with
// the remainder key last when any bar has one
const splitKeys = computed(() => {
  if (!isSplit.value) { return []; }
  const keys = [];
  const seen = new Set();
  for (const d of props.data) {
    for (const s of segmentsFor(d)) {
      if (s.key === OTHER_KEY) { continue; }
      if (!seen.has(s.key)) { seen.add(s.key); keys.push(s.key); }
    }
  }
  if (props.data.some(d => segmentsFor(d).some(s => s.key === OTHER_KEY))) { keys.push(OTHER_KEY); }
  return keys;
});

const keyLabel = (k) => (k === OTHER_KEY ? t('sessions.summary.other') : k);

// One row of swatches across the top, drawn in the SVG so PNG exports keep it.
// Entries that don't fit the width are left off.
const drawLegend = (root, keys, color, width) => {
  const legend = root.append('g')
    .attr('class', 'chart-legend')
    .attr('transform', `translate(4,${MARGIN.top + 1})`);
  let x = 0;
  for (const k of keys) {
    const label = String(keyLabel(k));
    const short = label.length > 24 ? `${label.slice(0, 23)}…` : label;
    const item = legend.append('g').attr('transform', `translate(${x},0)`);
    item.append('title').text(label);
    item.append('rect')
      .attr('y', -5)
      .attr('width', 10)
      .attr('height', 10)
      .attr('rx', 2)
      .attr('fill', color(k));
    const text = item.append('text')
      .attr('x', 14)
      .attr('dy', '0.35em')
      .attr('fill', 'currentColor')
      .style('font-size', '11px')
      .text(short);
    const itemWidth = 14 + (text.node().getComputedTextLength() || short.length * 6);
    if (x + itemWidth > width - 8) {
      item.remove();
      break;
    }
    x += itemWidth + 12;
  }
};

const showTooltip = (data, evt, split) => {
  emit('show-tooltip', {
    data,
    position: {
      x: evt.clientX + 1,
      y: evt.clientY + 1
    },
    fieldConfig: props.fieldConfig,
    metricType: props.metricType,
    metricAgg: props.metricAgg,
    split
  });
};

const createChartHoverHandlers = () => {
  return {
    mouseover: function (e, d) {
      d3.select(this).style('opacity', 0.7);
      if (d.segment) {
        const label = props.splitFieldConfig?.friendlyName || props.splitFieldConfig?.exp || '';
        showTooltip({ item: d.parent.item, sessions: d.segment.sessions, value: d.segment.value }, e,
          { label, value: keyLabel(d.segment.key) });
      } else {
        showTooltip(d, e);
      }
    },
    mouseleave: function () {
      d3.select(this).style('opacity', 1);
    }
  };
};

const renderChart = async () => {
  if (!chartContainer.value) return;
  if (!props.data || props.data.length === 0) return;

  // Load D3 if not already loaded
  if (!d3) {
    d3 = await import('d3');
  }

  const container = d3.select(chartContainer.value);
  container.selectAll('*').remove();

  // Use props.width as the SVG width - never exceed it to prevent resize loops
  const svgWidth = props.width;
  const svgHeight = props.height;
  const legendHeight = isSplit.value ? LEGEND_HEIGHT : 0;
  const height = svgHeight - MARGIN.top - MARGIN.bottom - legendHeight;
  const dataCount = props.data.length;

  // Calculate the inner width available for bars
  const availableInnerWidth = svgWidth - MARGIN.left - MARGIN.right;

  // Calculate natural bar width if we use full available space
  const naturalBarWidth = availableInnerWidth / dataCount * 0.9; // 0.9 accounts for padding

  // Determine actual inner width to use (may be less than available if bars would be too wide)
  let chartInnerWidth;
  if (naturalBarWidth > MAX_BAR_WIDTH) {
    // Too few bars - cap bar width, use less space (left-aligned)
    chartInnerWidth = dataCount * MAX_BAR_WIDTH / 0.9;
  } else {
    // Use full available width (bars will be natural size or clamped to min by scrolling)
    chartInnerWidth = availableInnerWidth;
  }

  const root = container.append('svg')
    .attr('id', props.svgId)
    .attr('width', svgWidth)
    .attr('height', svgHeight);
  const svg = root.append('g')
    .attr('transform', `translate(${MARGIN.left},${MARGIN.top + legendHeight})`);

  const x = d3.scaleBand()
    .range([0, chartInnerWidth])
    .domain(props.data.map(d => d.item))
    .padding(0.1);

  const handlers = createChartHoverHandlers();

  if (isSplit.value) {
    const keys = splitKeys.value;
    // averages, extremes and unique counts don't add up, so they never stack
    const grouped = props.barMode === 'grouped' || !additive.value;
    const bars = props.data.map(d => ({ parent: d, segments: segmentsFor(d) }));

    // stacked bars reach the value's total; grouped bars the tallest segment
    const yMax = grouped
      ? d3.max(bars, b => d3.max(b.segments, s => s.value) || 0)
      : d3.max(bars, b => d3.sum(b.segments, s => s.value));
    const y = d3.scaleLinear().domain([0, yMax || 0]).range([height, 0]);

    const palette = d3.scaleOrdinal(colorRange(d3, props.colorScheme, keys.length));
    const keyIndex = new Map(keys.map((k, i) => [k, i]));
    const color = (key) => palette(keyIndex.get(key) ?? 0);
    drawLegend(root, keys, color, svgWidth);

    const groups = svg.selectAll('.bar-group')
      .data(bars)
      .enter()
      .append('g')
      .attr('class', 'bar-group')
      .attr('transform', b => `translate(${x(b.parent.item)},0)`);

    if (grouped) {
      groups.each(function (b) {
        const xs = d3.scaleBand().domain(b.segments.map(s => s.key)).range([0, x.bandwidth()]).padding(0.05);
        d3.select(this).selectAll('rect')
          .data(b.segments.map(segment => ({ parent: b.parent, segment })))
          .enter()
          .append('rect')
          .attr('class', 'bar')
          .attr('x', d => xs(d.segment.key))
          .attr('width', xs.bandwidth())
          .attr('y', d => y(d.segment.value))
          .attr('height', d => height - y(d.segment.value))
          .attr('fill', d => color(d.segment.key))
          .style('cursor', 'pointer')
          .on('mouseover', handlers.mouseover)
          .on('mouseleave', handlers.mouseleave);
      });
    } else {
      groups.each(function (b) {
        let y0 = 0;
        const stacked = b.segments.map(segment => {
          const d = { parent: b.parent, segment, y0, y1: y0 + segment.value };
          y0 += segment.value;
          return d;
        });
        d3.select(this).selectAll('rect')
          .data(stacked)
          .enter()
          .append('rect')
          .attr('class', 'bar')
          .attr('x', 0)
          .attr('width', x.bandwidth())
          .attr('y', d => y(d.y1))
          .attr('height', d => Math.max(0, y(d.y0) - y(d.y1)))
          .attr('fill', d => color(d.segment.key))
          .style('cursor', 'pointer')
          .on('mouseover', handlers.mouseover)
          .on('mouseleave', handlers.mouseleave);
      });
    }

    svg.append('g').call(d3.axisLeft(y));
  } else {
    const y = d3.scaleLinear()
      .domain([0, d3.max(props.data, d => d.value)])
      .range([height, 0]);

    // Dashboard palette (shared with the pie/treemap charts)
    const colors = d3.scaleOrdinal(colorRange(d3, props.colorScheme, props.data.length));

    // Draw bars
    svg.selectAll('.bar')
      .data(props.data)
      .enter()
      .append('rect')
      .attr('class', 'bar')
      .attr('x', d => x(d.item))
      .attr('width', x.bandwidth())
      .attr('y', d => y(d.value))
      .attr('height', d => height - y(d.value))
      .attr('fill', (d, i) => colors(i))
      .style('cursor', 'pointer')
      .on('mouseover', handlers.mouseover)
      .on('mouseleave', handlers.mouseleave);

    // Y axis
    svg.append('g')
      .call(d3.axisLeft(y));
  }

  // X axis
  svg.append('g')
    .attr('transform', `translate(0,${height})`)
    .call(d3.axisBottom(x))
    .selectAll('text')
    .attr('transform', 'rotate(-45)')
    .style('text-anchor', 'end')
    .style('font-size', '10px');
};

// Watch for data, metric type, or dimension changes
watch([() => props.data, () => props.metricType, () => props.metricAgg, () => props.width, () => props.height, () => props.colorScheme, () => props.barMode, () => props.splitFieldConfig], () => {
  renderChart();
}, { deep: true });

// Initial render
onMounted(() => {
  renderChart();
});
</script>

<style scoped>
.chart-wrapper {
  position: relative;
  width: 100%;
  flex: 1;
}

.chart {
  min-height: 200px;
  height: 100%;
  overflow-x: auto;
  overflow-y: hidden;
}
</style>
