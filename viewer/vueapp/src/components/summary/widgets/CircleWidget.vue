<!--
Copyright Andy Wick
SPDX-License-Identifier: Apache-2.0

Connection circle widget: the src → dst sample from /api/connections with the
hosts on a ring and each pair an arc across it (laid out by connectionCircle.js,
shared with the SPI Graph circle type). Hovering a host leaves only its arcs
and peers lit; labels are dropped when the ring is too crowded for them. Arcs
are colored by the widget's colorField (protocols unless one is picked) in the
dashboard's palette.
-->
<template>
  <WidgetCard
    :title="title"
    :loading="loading"
    :error="error"
    :has-data="hasData"
    :info-items="infoItems"
    :link="link"
    :graph="graphDef"
    @edit="$emit('edit')"
    @remove="$emit('remove')"
    @retry="fetchData">
    <div
      ref="container"
      class="circle-container">
      <svg
        v-show="hasData"
        ref="svgEl" />
      <div
        v-if="hasData && legend.length"
        class="circle-legend"
        @mouseleave="focusColor()">
        <div
          v-for="p in legend"
          :key="p.name"
          class="d-flex align-center ga-1"
          @mouseenter="focusColor(p.name)">
          <span
            class="circle-swatch"
            :style="{ background: p.color }" />
          {{ p.name || $t('sessions.summary.other') }}
        </div>
      </div>
    </div>
  </WidgetCard>
</template>

<script setup>
import { ref, computed, watch, onBeforeUnmount, nextTick } from 'vue';
import WidgetCard from './WidgetCard.vue';
import FieldService from '../../search/FieldService';
import { useSpigraphWidget } from './useSpigraphWidget';
import { fetchConnections, widgetFields, circleColorDbField } from './widgetData';
import { colorRange, OTHER_COLOR } from './chartColors';
import { buildConnectionCircle, circlePoint, circleLinkPath, circleLabel, circleTouches, circlePeers, circleLegend } from './connectionCircle';

const props = defineProps({
  widget: { type: Object, required: true },
  reloadNonce: { type: Number, default: 0 },
  colorScheme: { type: String, default: 'rainbow' },
  infoItems: { type: Array, default: () => [] },
  link: { type: Object, default: null },
  graphDef: { type: Object, default: null }
});

const emit = defineEmits(['edit', 'remove', 'show-tooltip']);

const container = ref(null);
const svgEl = ref(null);
const graph = ref(null); // { nodes, links } from /api/connections
let d3lib;
let ro;
const legend = ref([]); // { name, color } per arc color
let circle; // the ring, laid out once per sample
let linkSel; // the drawn arcs, for the legend's hover

// legend hover: leaves only one color's arcs lit (undefined lights all)
const focusColor = (value) => {
  linkSel?.classed('circle-link--off', l => value !== undefined && l.colorValue !== value);
};

const { loading, error, fetchData } = useSpigraphWidget(
  () => props.widget,
  () => props.reloadNonce,
  (res) => { graph.value = res?.nodes ? res : null; circle = undefined; setupObserver(); },
  fetchConnections
);

const fieldExps = computed(() => widgetFields(props.widget));
const fieldObjs = computed(() => fieldExps.value.map(exp => FieldService.getField(exp, true)));
const title = computed(() => props.widget.title ||
  fieldObjs.value.map((f, i) => f?.friendlyName || fieldExps.value[i]).join(' → '));
const hasData = computed(() => !!graph.value?.nodes?.length);

// hover → shared popover; a destination resolves to the second field
const onHover = (e, n) => {
  const node = n.data.node;
  emit('show-tooltip', {
    data: { item: node.id, sessions: node.sessions, value: node.sessions },
    position: { x: e.clientX + 1, y: e.clientY + 1 },
    fieldConfig: (node.type === 2 ? fieldObjs.value[1] : fieldObjs.value[0]) || fieldObjs.value[0],
    metricType: 'sessions'
  });
};
// an arc names both ends and shows the pair's own session count
const onArcHover = (e, l) => {
  emit('show-tooltip', {
    data: { item: l.source.data.id, sessions: l.value, value: l.value },
    position: { x: e.clientX + 1, y: e.clientY + 1 },
    fieldConfig: fieldObjs.value[0],
    split: { label: fieldObjs.value[1]?.friendlyName || fieldExps.value[1], value: l.target.data.id },
    metricType: 'sessions'
  });
};

const render = async () => {
  if (!container.value || !svgEl.value || !hasData.value) { return; }
  if (!d3lib) { d3lib = await import('d3'); }
  const d3 = d3lib;
  const w = container.value.clientWidth;
  const h = container.value.clientHeight;
  if (!w || !h) { return; }

  const svg = d3.select(svgEl.value);
  svg.selectAll('*').remove();
  svg.attr('width', w).attr('height', h);

  if (!circle) { circle = buildConnectionCircle(graph.value, 'sessions', circleColorDbField(props.widget.colorField), props.widget.maxNodes); }
  const { nodes, links, colorValues } = circle;

  const colors = d3.scaleOrdinal(colorRange(d3, props.colorScheme, colorValues.length)).domain(colorValues);
  legend.value = circleLegend(circle, colors);

  // labels only when the box leaves room for them and the ring isn't crowded
  const labelW = 70;
  const half = Math.min(w, h) / 2;
  let r = half - labelW - 6;
  const labeled = r >= 40 && (2 * Math.PI * r) / nodes.length >= 9;
  if (!labeled) { r = Math.max(10, half - 10); }

  const maxSessions = Math.max(1, ...nodes.map(n => n.data.node.sessions || 0));
  const maxValue = Math.max(1, ...links.map(l => l.value || 0));
  const radius = (n) => 2.5 + 4 * Math.sqrt((n.data.node.sessions || 0) / maxSessions);
  const linkWidth = (l) => 1 + 3 * Math.sqrt((l.value || 0) / maxValue);

  const g = svg.append('g').attr('transform', `translate(${w / 2},${h / 2})`);

  const link = g.append('g')
    .selectAll('path')
    .data(links)
    .join('path')
    .attr('class', 'circle-link')
    .attr('stroke', d => (d.colorValue ? colors(d.colorValue) : OTHER_COLOR))
    .attr('stroke-width', d => linkWidth(d))
    .attr('d', d => circleLinkPath(d, r))
    .on('mouseover', (e, d) => onArcHover(e, d));

  linkSel = link;

  const node = g.append('g')
    .selectAll('circle')
    .data(nodes)
    .join('circle')
    .attr('cx', d => circlePoint(d.angle, r)[0])
    .attr('cy', d => circlePoint(d.angle, r)[1])
    .attr('r', d => radius(d))
    .attr('class', d => `circle-node circle-node--${d.data.node.type === 1 ? 'src' : (d.data.node.type === 2 ? 'dst' : 'both')}`)
    .style('cursor', 'pointer');

  let label;
  if (labeled) {
    label = g.append('g')
      .selectAll('text')
      .data(nodes)
      .join('text')
      .attr('transform', d => circleLabel(d.angle, r + 8).transform)
      .attr('text-anchor', d => circleLabel(d.angle, r + 8).anchor)
      .attr('dy', '0.32em')
      .attr('class', 'circle-label')
      .style('font-size', '9px')
      .text(d => (d.data.id.length > 16 ? d.data.id.slice(0, 15) + '…' : d.data.id));
  }

  node.on('mouseover', (e, d) => {
    const peers = circlePeers(links, d);
    link.classed('circle-link--on', l => circleTouches(l, d))
      .classed('circle-link--off', l => !circleTouches(l, d));
    node.style('opacity', o => (peers.has(o) ? 1 : 0.15));
    label?.style('opacity', o => (peers.has(o) ? 1 : 0.15));
    onHover(e, d);
  }).on('mouseout', () => {
    link.classed('circle-link--on', false).classed('circle-link--off', false);
    node.style('opacity', 1);
    label?.style('opacity', 1);
  });
};

// (re)attach the resize observer once the container is in the DOM, then render
const setupObserver = () => {
  nextTick(() => {
    if (!container.value) { return; }
    if (ro) { ro.disconnect(); }
    if (typeof ResizeObserver !== 'undefined') {
      ro = new ResizeObserver(() => render());
      ro.observe(container.value);
    }
    render();
  });
};

watch(() => props.colorScheme, () => render());
watch(() => props.widget.maxNodes, () => { circle = undefined; render(); });

onBeforeUnmount(() => { if (ro) { ro.disconnect(); } });
</script>

<style scoped>
.circle-container {
  position: relative;
  width: 100%;
  height: 100%;
  min-height: 0;
  overflow: hidden;
}
/* node colors match the SPI Graph connections page (src/dst/both legend) */
.circle-container :deep(.circle-node) {
  stroke: rgb(var(--v-theme-background));
  stroke-width: 1;
}
.circle-container :deep(.circle-node--src) { fill: rgb(var(--v-theme-primary)); }
.circle-container :deep(.circle-node--dst) { fill: rgb(var(--v-theme-quaternary)); }
.circle-container :deep(.circle-node--both) { fill: rgb(var(--v-theme-tertiary)); }
.circle-container :deep(.circle-link) {
  fill: none;
  stroke-opacity: 0.45;
}
.circle-legend {
  position: absolute;
  top: 0;
  left: 4px;
  font-size: 10px;
  line-height: 1.3;
  cursor: default;
}
.circle-swatch {
  display: inline-block;
  width: 8px;
  height: 8px;
  border-radius: 2px;
}
.circle-container :deep(.circle-link--on) { stroke-opacity: 0.9; }
.circle-container :deep(.circle-link--off) { stroke-opacity: 0.04; }
.circle-container :deep(.circle-label) {
  fill: rgb(var(--v-theme-foreground));
  pointer-events: none;
}
</style>
