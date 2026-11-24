<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0

Combo chart: one bar per top value of a field for one metric, and a line
through the values for a second metric on its own axis on the right, so two
metrics of different scales (sessions and bytes, say) read on one chart.
Shared by the dashboard combo widget and SPI Graph's Combo type.
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
import { themedColor } from '@common/themes/themedColor.js';
import { colorRange } from './widgets/chartColors';

const props = defineProps({
  // [{ item, sessions, bar, line }] in rank order
  data: { type: Array, required: true },
  svgId: { type: String, required: true },
  fieldConfig: { type: Object, required: true },
  barLabel: { type: String, default: '' },
  lineLabel: { type: String, default: '' },
  // value -> display string, per axis (bytes read as bytes)
  formatBar: { type: Function, default: (v) => String(v) },
  formatLine: { type: Function, default: (v) => String(v) },
  width: { type: Number, default: 500 },
  height: { type: Number, default: 350 },
  colorScheme: { type: String, default: 'rainbow' }
});

const emit = defineEmits(['show-tooltip', 'hide-tooltip']);

const chartContainer = ref(null);
let d3 = null;

const MARGIN = { top: 26, right: 52, bottom: 110, left: 52 };
const MAX_BAR_WIDTH = 120;
const MAX_LABEL = 28;
const shortLabel = (v) => {
  const s = String(v);
  return s.length > MAX_LABEL ? `${s.slice(0, MAX_LABEL - 1)}…` : s;
};

// an axis's ticks in the metric's own format (bytes as KB/MB, counts as 1.2k)
const tickFormat = (format) => (v) => format(v);

const showTooltip = (d, evt) => {
  emit('show-tooltip', {
    data: { item: d.item, sessions: d.sessions },
    rows: [
      { label: props.barLabel, value: props.formatBar(d.bar) },
      { label: props.lineLabel, value: props.formatLine(d.line) }
    ],
    position: { x: evt.clientX + 1, y: evt.clientY + 1 },
    fieldConfig: props.fieldConfig
  });
};

// the two series' swatches across the top, drawn in the SVG so a PNG keeps them
const drawLegend = (root, barColor, lineColor) => {
  const legend = root.append('g')
    .attr('class', 'chart-legend')
    .attr('transform', `translate(${MARGIN.left},10)`);
  const bar = legend.append('g');
  bar.append('rect').attr('y', -5).attr('width', 10).attr('height', 10).attr('rx', 2).attr('fill', barColor);
  const barText = bar.append('text').attr('x', 14).attr('dy', '0.35em').attr('fill', 'currentColor')
    .style('font-size', '11px').text(props.barLabel);
  const x = 14 + (barText.node().getComputedTextLength() || props.barLabel.length * 6) + 16;
  const line = legend.append('g').attr('transform', `translate(${x},0)`);
  line.append('line').attr('x1', 0).attr('x2', 14).attr('stroke', lineColor).attr('stroke-width', 2);
  line.append('circle').attr('cx', 7).attr('r', 3).attr('fill', lineColor);
  line.append('text').attr('x', 18).attr('dy', '0.35em').attr('fill', 'currentColor')
    .style('font-size', '11px').text(props.lineLabel);
};

const renderChart = async () => {
  if (!chartContainer.value || !props.data?.length) { return; }
  if (!d3) { d3 = await import('d3'); }

  const container = d3.select(chartContainer.value);
  container.selectAll('*').remove();

  const svgWidth = props.width;
  const svgHeight = props.height;
  const height = Math.max(40, svgHeight - MARGIN.top - MARGIN.bottom);
  const available = Math.max(40, svgWidth - MARGIN.left - MARGIN.right);
  // few bars don't stretch: each is capped, the chart left aligned
  const plotWidth = available / props.data.length * 0.9 > MAX_BAR_WIDTH
    ? props.data.length * MAX_BAR_WIDTH / 0.9
    : available;

  // the bars take the palette's first color, the line the text color, which
  // reads on any palette in either theme
  const barColor = colorRange(d3, props.colorScheme, 6)[0];
  const lineColor = themedColor('foreground', '#333');

  const root = container.append('svg')
    .attr('id', props.svgId)
    .attr('width', svgWidth)
    .attr('height', svgHeight);
  drawLegend(root, barColor, lineColor);

  const svg = root.append('g').attr('transform', `translate(${MARGIN.left},${MARGIN.top})`);

  const x = d3.scaleBand().range([0, plotWidth]).domain(props.data.map(d => d.item)).padding(0.15);
  const yBar = d3.scaleLinear().domain([0, d3.max(props.data, d => d.bar) || 1]).nice().range([height, 0]);
  const yLine = d3.scaleLinear().domain([0, d3.max(props.data, d => d.line) || 1]).nice().range([height, 0]);
  const ticks = Math.max(2, Math.floor(height / 40));

  svg.append('g')
    .attr('class', 'grid')
    .call(d3.axisLeft(yBar).ticks(ticks).tickSize(-plotWidth).tickFormat(''))
    .call(g => g.selectAll('line').attr('stroke', 'currentColor').attr('stroke-opacity', 0.1))
    .call(g => g.select('.domain').remove());

  svg.selectAll('.bar')
    .data(props.data)
    .enter()
    .append('rect')
    .attr('class', 'bar')
    .attr('x', d => x(d.item))
    .attr('width', x.bandwidth())
    .attr('y', d => yBar(d.bar || 0))
    .attr('height', d => height - yBar(d.bar || 0))
    .attr('fill', barColor)
    .style('cursor', 'pointer')
    .on('mouseover', function (e, d) { d3.select(this).style('opacity', 0.7); showTooltip(d, e); })
    .on('mouseleave', function () { d3.select(this).style('opacity', 1); });

  // the line through the bars' centers, a dot on each (hover shows the same tooltip)
  const cx = (d) => x(d.item) + x.bandwidth() / 2;
  svg.append('path')
    .datum(props.data)
    .attr('fill', 'none')
    .attr('stroke', lineColor)
    .attr('stroke-width', 2)
    .attr('d', d3.line().x(cx).y(d => yLine(d.line || 0)));
  svg.selectAll('.dot')
    .data(props.data)
    .enter()
    .append('circle')
    .attr('class', 'dot')
    .attr('cx', cx)
    .attr('cy', d => yLine(d.line || 0))
    .attr('r', 4)
    .attr('fill', lineColor)
    .attr('stroke', 'rgb(var(--v-theme-background))')
    .attr('stroke-width', 1)
    .style('cursor', 'pointer')
    .on('mouseover', (e, d) => showTooltip(d, e));

  svg.append('g')
    .call(d3.axisLeft(yBar).ticks(ticks).tickFormat(tickFormat(props.formatBar)))
    .call(g => g.selectAll('text').attr('fill', barColor));
  svg.append('g')
    .attr('transform', `translate(${plotWidth},0)`)
    .call(d3.axisRight(yLine).ticks(ticks).tickFormat(tickFormat(props.formatLine)))
    .call(g => g.selectAll('text').attr('fill', lineColor));

  const labels = svg.append('g')
    .attr('transform', `translate(0,${height})`)
    .call(d3.axisBottom(x).tickFormat(shortLabel));
  labels.selectAll('text')
    .attr('transform', 'rotate(-45)')
    .style('text-anchor', 'end')
    .style('font-size', '10px');
  labels.selectAll('.tick').append('title').text(d => String(d));
};

watch([() => props.data, () => props.width, () => props.height, () => props.colorScheme, () => props.barLabel, () => props.lineLabel], renderChart, { deep: true });
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
  overflow-x: auto;
  overflow-y: hidden;
}
</style>
