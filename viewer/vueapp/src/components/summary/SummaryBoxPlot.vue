<!--
Copyright Andy Wick
SPDX-License-Identifier: Apache-2.0

Box plot: one box per top value of a field, drawn from the spread of a
numeric metric over the value's sessions. The box is the middle half of the
values (25th to 75th percentile), the line across it the median, the whiskers
run out to the smallest and largest value and the dot is the mean. Which
values carry long sessions or big transfers, and which only a few odd ones,
reads off it at a glance. The metric axis can be log scaled, which byte counts
usually need. Shared by the dashboard box plot widget and SPI Graph's Box plot.
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
  // [{ item, sessions, box: { count, min, q1, median, q3, max, mean } }]
  // (box.count 0 when none of the value's sessions has the metric)
  data: { type: Array, required: true },
  svgId: { type: String, required: true },
  fieldConfig: { type: Object, required: true },
  metricLabel: { type: String, default: '' },
  formatValue: { type: Function, default: (v) => String(v) },
  // log scale the metric axis (symlog, so a 0 still has a place)
  log: { type: Boolean, default: false },
  // 'vertical' boxes stand on the metric axis, 'horizontal' ones lie along it
  orientation: { type: String, default: 'vertical' },
  width: { type: Number, default: 500 },
  height: { type: Number, default: 350 },
  colorScheme: { type: String, default: 'rainbow' }
});

const emit = defineEmits(['show-tooltip']);

const chartContainer = ref(null);
let d3 = null;

// the band axis' margin fits its (cut) labels; the metric axis gets the rest
const PAD = 12;
const TICK_GAP = 10; // between the metric axis and its tick labels
const TITLE_SPACE = 18; // the metric axis' title, beside its tick labels
const MAX_BOX = 48;
const MAX_LABEL = 24;
const shortLabel = (v) => {
  const s = String(v);
  return s.length > MAX_LABEL ? `${s.slice(0, MAX_LABEL - 1)}…` : s;
};

const showTooltip = (d, evt) => {
  const b = d.box || {};
  const rows = b.count
    ? [
      { label: t('sessions.summary.box.max'), value: props.formatValue(b.max) },
      { label: t('sessions.summary.box.q3'), value: props.formatValue(b.q3) },
      { label: t('sessions.summary.box.median'), value: props.formatValue(b.median) },
      { label: t('sessions.summary.box.q1'), value: props.formatValue(b.q1) },
      { label: t('sessions.summary.box.min'), value: props.formatValue(b.min) },
      { label: t('sessions.summary.box.mean'), value: props.formatValue(b.mean) }
    ]
    : [{ label: props.metricLabel, value: t('sessions.summary.box.none') }];
  // sessions without the metric (or with several values of it) make the
  // number of values differ from the session count
  if (b.count && b.count !== d.sessions) {
    rows.push({ label: t('sessions.summary.box.count'), value: Number(b.count).toLocaleString() });
  }
  emit('show-tooltip', {
    data: { item: d.item, sessions: d.sessions },
    rows,
    position: { x: evt.clientX + 1, y: evt.clientY + 1 },
    fieldConfig: props.fieldConfig
  });
};

// a log axis ticks at the powers of ten it spans (symlog's own ticks are
// linear), every 2nd, 3rd... of them when there is only room for some,
// counted down from the largest so the top of the axis keeps its tick
const logTicks = (max, room) => {
  const powers = [];
  for (let p = 0; Math.pow(10, p) <= max; p++) { powers.push(Math.pow(10, p)); }
  const every = Math.max(1, Math.ceil(powers.length / Math.max(1, room - 1)));
  return [0, ...powers.filter((v, i) => (powers.length - 1 - i) % every === 0)];
};

const renderChart = async () => {
  if (!chartContainer.value || !props.data?.length) { return; }
  if (!d3) { d3 = await import('d3'); }

  const container = d3.select(chartContainer.value);
  container.selectAll('*').remove();

  const svgWidth = props.width;
  const svgHeight = props.height;
  const horizontal = props.orientation === 'horizontal';
  const boxes = props.data.filter(d => d.box?.count);

  const root = container.append('svg')
    .attr('id', props.svgId)
    .attr('width', svgWidth)
    .attr('height', svgHeight);

  // the widest of some labels as drawn (10px), for the margins they sit in
  const probe = root.append('text').style('font-size', '10px');
  const widestOf = (labels) => labels.reduce((w, label) => {
    probe.text(label);
    return Math.max(w, probe.node().getComputedTextLength() || label.length * 6);
  }, 0);

  // the metric axis: from 0 unless the values all sit well above it
  let lo = boxes.length ? d3.min(boxes, d => d.box.min) : 0;
  const hi = (boxes.length ? d3.max(boxes, d => d.box.max) : 0) || 1;
  if (props.log || (lo >= 0 && lo < hi * 0.3)) { lo = Math.min(0, lo); }
  const value = props.log ? d3.scaleSymlog().constant(1) : d3.scaleLinear();
  value.domain([lo, props.log ? hi * 2 : hi]);
  if (!props.log) { value.nice(); }

  // vertical: the value labels slant down under the boxes and the metric's
  // tick labels and title fit on the left; horizontal: the value labels run
  // down the left, each beside its box, and the metric's ticks are spaced
  // by their width
  const widest = widestOf(props.data.map(d => shortLabel(d.item)));
  const margin = horizontal
    ? { top: PAD, right: PAD + 8, bottom: 44, left: Math.min(Math.ceil(widest) + 12, svgWidth / 2) }
    : { top: PAD, right: PAD, bottom: Math.min(Math.ceil(widest * 0.72) + 16, svgHeight * 0.45), left: 0 };
  const valueSpan = horizontal ? svgWidth - margin.left - margin.right : svgHeight - margin.top - margin.bottom;
  const tickSpacing = horizontal ? Math.max(80, widestOf([props.formatValue(value.domain()[1])]) + 24) : 40;
  const tickRoom = Math.max(2, Math.floor(valueSpan / tickSpacing));
  const ticks = props.log ? logTicks(value.domain()[1], tickRoom) : value.ticks(tickRoom);
  const tickWidest = widestOf(ticks.map(props.formatValue));
  probe.remove();
  if (horizontal) {
    margin.right = Math.max(margin.right, Math.ceil(tickWidest / 2) + 4);
  } else {
    margin.left = Math.min(Math.ceil(tickWidest) + TICK_GAP + TITLE_SPACE, svgWidth / 3);
  }
  const plotW = Math.max(40, svgWidth - margin.left - margin.right);
  const plotH = Math.max(40, svgHeight - margin.top - margin.bottom);
  const bandLength = horizontal ? plotH : plotW;
  const valueLength = horizontal ? plotW : plotH;
  value.range(horizontal ? [0, valueLength] : [valueLength, 0]);

  // no wider than MAX_BOX each, the band axis is cut short when they are few
  const naturalBand = bandLength / props.data.length;
  const bandSpan = naturalBand * 0.6 > MAX_BOX ? props.data.length * MAX_BOX / 0.6 : bandLength;
  const band = d3.scaleBand()
    .domain(props.data.map((d, i) => i))
    .range([0, bandSpan])
    .padding(0.4);
  const boxW = band.bandwidth();
  const colors = d3.scaleOrdinal(colorRange(d3, props.colorScheme, props.data.length));

  const svg = root.append('g').attr('transform', `translate(${margin.left},${margin.top})`);

  // the metric axis and a faint grid behind the boxes
  const axis = (a) => a.tickValues(ticks);
  if (horizontal) {
    svg.append('g')
      .attr('transform', `translate(0,${plotH})`)
      .call(axis(d3.axisBottom(value).tickSize(-plotH)).tickFormat(() => ''))
      .call(g => g.selectAll('line').attr('stroke', 'currentColor').attr('stroke-opacity', 0.1))
      .call(g => g.select('.domain').remove());
  } else {
    svg.append('g')
      .call(axis(d3.axisLeft(value).tickSize(-plotW)).tickFormat(() => ''))
      .call(g => g.selectAll('line').attr('stroke', 'currentColor').attr('stroke-opacity', 0.1))
      .call(g => g.select('.domain').remove());
  }

  // position along the band axis (p) and the metric axis (v) as x/y
  const xy = (p, v) => (horizontal ? [v, p] : [p, v]);
  const line = (sel, p1, v1, p2, v2) => {
    const [x1, y1] = xy(p1, v1);
    const [x2, y2] = xy(p2, v2);
    return sel.append('line').attr('x1', x1).attr('y1', y1).attr('x2', x2).attr('y2', y2);
  };

  const items = svg.selectAll('.box-item')
    .data(props.data.map((d, i) => ({ ...d, i })))
    .enter()
    .append('g')
    .attr('class', 'box-item');

  items.each(function (d) {
    const g = d3.select(this);
    const p0 = band(d.i);
    const mid = p0 + boxW / 2;
    // the whole band takes the hover, so a narrow or empty box is easy to hit
    const [hx, hy] = xy(p0 - band.step() * band.paddingInner() / 2, 0);
    g.append('rect')
      .attr('class', 'box-hover')
      .attr('x', hx)
      .attr('y', hy)
      .attr('width', horizontal ? valueLength : band.step())
      .attr('height', horizontal ? band.step() : valueLength)
      .attr('fill', 'currentColor')
      .attr('fill-opacity', 0);
    const b = d.box;
    if (!b?.count) { return; }
    const color = colors(d.i);
    const v = (n) => value(n);
    const capW = boxW / 2;
    // whiskers out to the extremes, each with a cap
    line(g, mid, v(b.min), mid, v(b.q1)).attr('stroke', color).attr('stroke-width', 1);
    line(g, mid, v(b.q3), mid, v(b.max)).attr('stroke', color).attr('stroke-width', 1);
    line(g, mid - capW / 2, v(b.min), mid + capW / 2, v(b.min)).attr('stroke', color).attr('stroke-width', 1.5);
    line(g, mid - capW / 2, v(b.max), mid + capW / 2, v(b.max)).attr('stroke', color).attr('stroke-width', 1.5);
    // the middle half of the values, at least a hairline when they are all one
    const lowEnd = Math.min(v(b.q1), v(b.q3));
    const span = Math.max(1, Math.abs(v(b.q3) - v(b.q1)));
    const [bx, by] = xy(p0, lowEnd);
    g.append('rect')
      .attr('class', 'box-body')
      .attr('x', bx)
      .attr('y', by)
      .attr('width', horizontal ? span : boxW)
      .attr('height', horizontal ? boxW : span)
      .attr('fill', color)
      .attr('fill-opacity', 0.35)
      .attr('stroke', color)
      .attr('stroke-width', 1.5);
    line(g, p0, v(b.median), p0 + boxW, v(b.median)).attr('stroke', color).attr('stroke-width', 2.5);
    const [mx, my] = xy(mid, v(b.mean));
    g.append('circle')
      .attr('cx', mx)
      .attr('cy', my)
      .attr('r', Math.max(2, Math.min(4, boxW / 6)))
      .attr('fill', 'none')
      .attr('stroke', 'currentColor')
      .attr('stroke-width', 1.5);
  });

  items.style('cursor', 'pointer')
    .on('mouseover', function (e, d) {
      d3.select(this).select('.box-body').attr('fill-opacity', 0.6);
      d3.select(this).select('.box-hover').attr('fill-opacity', 0.05);
      showTooltip(d, e);
    })
    .on('mouseleave', function () {
      d3.select(this).select('.box-body').attr('fill-opacity', 0.35);
      d3.select(this).select('.box-hover').attr('fill-opacity', 0);
    });

  // the band axis: each value's name (cut short, whole on hover)
  const labelOf = (i) => shortLabel(props.data[i]?.item);
  const fullOf = (i) => String(props.data[i]?.item);
  if (horizontal) {
    const labels = svg.append('g').call(d3.axisLeft(band).tickFormat(labelOf));
    labels.selectAll('text').style('font-size', '10px');
    labels.selectAll('.tick').append('title').text(fullOf);
    svg.append('g')
      .attr('transform', `translate(0,${plotH})`)
      .call(axis(d3.axisBottom(value)).tickFormat(props.formatValue));
    svg.append('text')
      .attr('x', plotW / 2)
      .attr('y', plotH + 36)
      .attr('text-anchor', 'middle')
      .attr('fill', 'currentColor')
      .style('font-size', '11px')
      .text(props.metricLabel);
  } else {
    const labels = svg.append('g')
      .attr('transform', `translate(0,${plotH})`)
      .call(d3.axisBottom(band).tickFormat(labelOf));
    labels.selectAll('text')
      .attr('transform', 'rotate(-40)')
      .style('text-anchor', 'end')
      .style('font-size', '10px');
    labels.selectAll('.tick').append('title').text(fullOf);
    svg.append('g')
      .call(axis(d3.axisLeft(value)).tickFormat(props.formatValue));
    svg.append('text')
      .attr('transform', 'rotate(-90)')
      .attr('x', -plotH / 2)
      .attr('y', -margin.left + 12)
      .attr('text-anchor', 'middle')
      .attr('fill', 'currentColor')
      .style('font-size', '11px')
      .text(props.metricLabel);
  }
};

watch([() => props.data, () => props.width, () => props.height, () => props.colorScheme, () => props.log, () => props.orientation, () => props.metricLabel], renderChart, { deep: true });
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
