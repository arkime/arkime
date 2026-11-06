<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0

Scatter (bubble) chart: one point per top value of a field, placed by two
metrics and, when a third is given, sized by it. Values that stand apart
from the rest (few sessions but many bytes, say) stand out on it. Both axes
can be log scaled, which network counts usually need. Shared by the
dashboard scatter widget and SPI Graph's Scatter type.
-->
<template>
  <div class="chart-wrapper">
    <div
      ref="chartContainer"
      class="chart" />
  </div>
</template>

<script setup>
import { ref, watch, onMounted } from 'vue';
import { useI18n } from 'vue-i18n';
import { colorRange } from './widgets/chartColors';

const { t } = useI18n();

const props = defineProps({
  // [{ item, sessions, x, y, size }] (size undefined without a size metric)
  data: { type: Array, required: true },
  svgId: { type: String, required: true },
  fieldConfig: { type: Object, required: true },
  xLabel: { type: String, default: '' },
  yLabel: { type: String, default: '' },
  sizeLabel: { type: String, default: '' },
  formatX: { type: Function, default: (v) => String(v) },
  formatY: { type: Function, default: (v) => String(v) },
  formatSize: { type: Function, default: (v) => String(v) },
  // log scale both axes (symlog, so a 0 still has a place)
  log: { type: Boolean, default: false },
  width: { type: Number, default: 500 },
  height: { type: Number, default: 350 },
  colorScheme: { type: String, default: 'rainbow' }
});

const emit = defineEmits(['show-tooltip', 'hide-tooltip']);

const chartContainer = ref(null);
let d3 = null;

const MARGIN = { top: 12, right: 20, bottom: 44, left: 64 };
const MIN_R = 4;
const MAX_R = 26;
// the biggest points get their value's name beside them
const LABELED = 5;
const MAX_LABEL = 20;
const shortLabel = (v) => {
  const s = String(v);
  return s.length > MAX_LABEL ? `${s.slice(0, MAX_LABEL - 1)}…` : s;
};

const showTooltip = (d, evt) => {
  const rows = [
    { label: props.xLabel, value: props.formatX(d.x) },
    { label: props.yLabel, value: props.formatY(d.y) }
  ];
  if (d.size !== undefined) { rows.push({ label: props.sizeLabel, value: props.formatSize(d.size) }); }
  emit('show-tooltip', {
    data: { item: d.item, sessions: d.sessions },
    rows,
    position: { x: evt.clientX + 1, y: evt.clientY + 1 },
    fieldConfig: props.fieldConfig
  });
};

// a log axis ticks at the powers of ten it spans (symlog's own ticks are linear)
const logTicks = (max) => {
  const out = [0];
  for (let p = 0; Math.pow(10, p) <= max; p++) { out.push(Math.pow(10, p)); }
  return out;
};

const renderChart = async () => {
  if (!chartContainer.value || !props.data?.length) { return; }
  if (!d3) { d3 = await import('d3'); }

  const container = d3.select(chartContainer.value);
  container.selectAll('*').remove();

  const svgWidth = props.width;
  const svgHeight = props.height;
  const width = Math.max(40, svgWidth - MARGIN.left - MARGIN.right);
  const height = Math.max(40, svgHeight - MARGIN.top - MARGIN.bottom);

  const xMax = d3.max(props.data, d => d.x) || 1;
  const yMax = d3.max(props.data, d => d.y) || 1;
  // room past the largest value, so its point and label aren't cut at the edge
  const scale = () => (props.log ? d3.scaleSymlog().constant(1) : d3.scaleLinear());
  const x = scale().domain([0, props.log ? xMax * 2 : xMax * 1.08]).range([0, width]);
  const y = scale().domain([0, props.log ? yMax * 2 : yMax * 1.08]).range([height, 0]);
  if (!props.log) { x.nice(); y.nice(); }
  const sized = props.data.some(d => d.size !== undefined);
  const r = sized
    ? d3.scaleSqrt().domain([0, d3.max(props.data, d => d.size) || 1]).range([MIN_R, MAX_R])
    : () => 6;
  const colors = d3.scaleOrdinal(colorRange(d3, props.colorScheme, props.data.length));

  const root = container.append('svg')
    .attr('id', props.svgId)
    .attr('width', svgWidth)
    .attr('height', svgHeight);
  const svg = root.append('g').attr('transform', `translate(${MARGIN.left},${MARGIN.top})`);

  const xTicks = props.log ? logTicks(x.domain()[1]) : Math.max(2, Math.floor(width / 80));
  const yTicks = props.log ? logTicks(y.domain()[1]) : Math.max(2, Math.floor(height / 40));
  const axis = (a, ticks, format) => (Array.isArray(ticks) ? a.tickValues(ticks) : a.ticks(ticks)).tickFormat(format);

  // a faint grid behind the points
  svg.append('g')
    .call(axis(d3.axisLeft(y).tickSize(-width), yTicks, () => ''))
    .call(g => g.selectAll('line').attr('stroke', 'currentColor').attr('stroke-opacity', 0.1))
    .call(g => g.select('.domain').remove());
  svg.append('g')
    .attr('transform', `translate(0,${height})`)
    .call(axis(d3.axisBottom(x).tickSize(-height), xTicks, () => ''))
    .call(g => g.selectAll('line').attr('stroke', 'currentColor').attr('stroke-opacity', 0.1))
    .call(g => g.select('.domain').remove());

  // largest first, so small points sit on top and stay hoverable
  const points = props.data.map((d, i) => ({ ...d, i })).sort((a, b) => (b.size || 0) - (a.size || 0));
  svg.selectAll('.point')
    .data(points)
    .enter()
    .append('circle')
    .attr('class', 'point')
    .attr('cx', d => x(d.x || 0))
    .attr('cy', d => y(d.y || 0))
    .attr('r', d => (sized ? r(d.size || 0) : r()))
    .attr('fill', d => colors(d.i))
    .attr('fill-opacity', 0.7)
    .attr('stroke', d => colors(d.i))
    .attr('stroke-width', 1)
    .style('cursor', 'pointer')
    .on('mouseover', function (e, d) { d3.select(this).attr('fill-opacity', 1); showTooltip(d, e); })
    .on('mouseleave', function () { d3.select(this).attr('fill-opacity', 0.7); });

  // name the top values (by rank, the first points in the data) so the chart
  // reads without hovering
  svg.selectAll('.point-label')
    .data(props.data.slice(0, LABELED))
    .enter()
    .append('text')
    .attr('class', 'point-label')
    .attr('x', d => x(d.x || 0) + (sized ? r(d.size || 0) : 6) + 3)
    .attr('y', d => y(d.y || 0))
    .attr('dy', '0.35em')
    .attr('fill', 'currentColor')
    .style('font-size', '10px')
    .style('pointer-events', 'none')
    .text(d => shortLabel(d.item));

  svg.append('g')
    .attr('transform', `translate(0,${height})`)
    .call(axis(d3.axisBottom(x), xTicks, props.formatX));
  svg.append('g')
    .call(axis(d3.axisLeft(y), yTicks, props.formatY));

  // axis titles
  svg.append('text')
    .attr('x', width / 2)
    .attr('y', height + 36)
    .attr('text-anchor', 'middle')
    .attr('fill', 'currentColor')
    .style('font-size', '11px')
    .text(sized ? t('sessions.summary.scatterSizedBy', { metric: props.xLabel, size: props.sizeLabel }) : props.xLabel);
  svg.append('text')
    .attr('transform', 'rotate(-90)')
    .attr('x', -height / 2)
    .attr('y', -52)
    .attr('text-anchor', 'middle')
    .attr('fill', 'currentColor')
    .style('font-size', '11px')
    .text(props.yLabel);
};

watch([() => props.data, () => props.width, () => props.height, () => props.colorScheme, () => props.log, () => props.xLabel, () => props.yLabel, () => props.sizeLabel], renderChart, { deep: true });
onMounted(renderChart);
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
  overflow: hidden;
}
</style>
