<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0

Connections widget: a display-only force-directed graph of a src → dst field
pair, self-fetched from /api/connections. Unlike the SPI Graph connections view
this is just the picture + hover popover — no dragging, zooming, node locking,
popup action menus or baseline. The layout is settled synchronously (static
ticks) and fit to the widget box, so nothing jiggles. Node colors mirror the
SPI Graph page: src = primary, dst = quaternary, both = tertiary theme colors.
-->
<template>
  <WidgetCard
    :title="title"
    :loading="loading"
    :error="error"
    :has-data="hasData"
    :info-items="infoItems"
    @edit="$emit('edit')"
    @remove="$emit('remove')"
    @retry="fetchData">
    <div
      ref="container"
      class="connections-container">
      <svg
        v-show="hasData"
        ref="svgEl" />
    </div>
  </WidgetCard>
</template>

<script setup>
import { ref, computed, watch, onBeforeUnmount, nextTick } from 'vue';
import WidgetCard from './WidgetCard.vue';
import FieldService from '../../search/FieldService';
import { useSpigraphWidget } from './useSpigraphWidget';
import { fetchConnections, widgetFields } from './widgetData';

const props = defineProps({
  widget: { type: Object, required: true },
  reloadNonce: { type: Number, default: 0 },
  infoItems: { type: Array, default: () => [] }
});

const emit = defineEmits(['edit', 'remove', 'show-tooltip']);

const container = ref(null);
const svgEl = ref(null);
const graph = ref(null); // { nodes, links } from /api/connections
let d3lib;
let ro;

const { loading, error, fetchData } = useSpigraphWidget(
  () => props.widget,
  () => props.reloadNonce,
  (res) => { graph.value = res?.nodes ? res : null; setupObserver(); },
  fetchConnections
);

const fieldExps = computed(() => widgetFields(props.widget));
const fieldObjs = computed(() => fieldExps.value.map(exp => FieldService.getField(exp, true)));
const title = computed(() => props.widget.title ||
  fieldObjs.value.map((f, i) => f?.friendlyName || fieldExps.value[i]).join(' → '));
const hasData = computed(() => !!graph.value?.nodes?.length);

// hover → shared popover; src nodes resolve to the first field, dst to the
// second (a "both" node's value exists in either — use the src field)
const onHover = (e, node) => {
  emit('show-tooltip', {
    data: { item: node.id, sessions: node.sessions, value: node.sessions },
    position: { x: e.clientX + 1, y: e.clientY + 1 },
    fieldConfig: (node.type === 2 ? fieldObjs.value[1] : fieldObjs.value[0]) || fieldObjs.value[0],
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

  // clone: the force simulation mutates nodes (x/y) and links (index → ref)
  const nodes = graph.value.nodes.map(n => ({ ...n }));
  const links = graph.value.links.map(l => ({ ...l }));

  const maxSessions = Math.max(1, ...nodes.map(n => n.sessions || 0));
  const maxValue = Math.max(1, ...links.map(l => l.value || 0));
  const radius = (n) => 3 + 9 * Math.sqrt((n.sessions || 0) / maxSessions);
  const linkWidth = (l) => 1 + 3 * Math.sqrt((l.value || 0) / maxValue);

  // settle the layout synchronously — a static picture, no animation
  const sim = d3.forceSimulation(nodes)
    .force('link', d3.forceLink(links).distance(50).strength(0.4))
    .force('charge', d3.forceManyBody().strength(-100))
    .force('center', d3.forceCenter(0, 0))
    .force('collide', d3.forceCollide(d => radius(d) + 3))
    .stop();
  sim.tick(200);

  // map the settled layout into the widget box (px space, so radii, strokes
  // and label sizes stay constant regardless of graph spread)
  const pad = 14;
  const minX = Math.min(...nodes.map(n => n.x));
  const maxX = Math.max(...nodes.map(n => n.x));
  const minY = Math.min(...nodes.map(n => n.y));
  const maxY = Math.max(...nodes.map(n => n.y));
  for (const n of nodes) {
    n.x = pad + ((n.x - minX) / Math.max(1, maxX - minX)) * (w - 2 * pad);
    n.y = pad + ((n.y - minY) / Math.max(1, maxY - minY)) * (h - 2 * pad);
  }
  const g = svg.append('g');

  g.append('g')
    .selectAll('line')
    .data(links)
    .join('line')
    .attr('x1', d => d.source.x)
    .attr('y1', d => d.source.y)
    .attr('x2', d => d.target.x)
    .attr('y2', d => d.target.y)
    .attr('class', 'conn-link')
    .attr('stroke-width', d => linkWidth(d))
    .on('mouseover', (e, d) => onHover(e, d.source));

  g.append('g')
    .selectAll('circle')
    .data(nodes)
    .join('circle')
    .attr('cx', d => d.x)
    .attr('cy', d => d.y)
    .attr('r', d => radius(d))
    .attr('class', d => `conn-node conn-node--${d.type === 1 ? 'src' : (d.type === 2 ? 'dst' : 'both')}`)
    .style('cursor', 'pointer')
    .on('mouseover', onHover);

  // label the biggest talkers (keeps dense graphs legible)
  const labeled = [...nodes].sort((a, b) => (b.sessions || 0) - (a.sessions || 0)).slice(0, 30);
  g.append('g')
    .selectAll('text')
    .data(labeled)
    .join('text')
    .attr('x', d => d.x + radius(d) + 2)
    .attr('y', d => d.y)
    .attr('dy', '0.35em')
    .attr('class', 'conn-label')
    .style('font-size', '9px')
    .text(d => d.id);
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

watch(() => props.widget.viewMode, render);

onBeforeUnmount(() => { if (ro) { ro.disconnect(); } });
</script>

<style scoped>
.connections-container {
  position: relative;
  width: 100%;
  height: 100%;
  min-height: 0;
  overflow: hidden;
}
/* node colors match the SPI Graph connections page (src/dst/both legend) */
.connections-container :deep(.conn-node) {
  stroke: rgb(var(--v-theme-background));
  stroke-width: 1;
}
.connections-container :deep(.conn-node--src) { fill: rgb(var(--v-theme-primary)); }
.connections-container :deep(.conn-node--dst) { fill: rgb(var(--v-theme-quaternary)); }
.connections-container :deep(.conn-node--both) { fill: rgb(var(--v-theme-tertiary)); }
.connections-container :deep(.conn-link) {
  stroke: rgb(var(--v-theme-foreground));
  stroke-opacity: 0.25;
}
.connections-container :deep(.conn-label) {
  fill: rgb(var(--v-theme-foreground));
  pointer-events: none;
}
</style>
