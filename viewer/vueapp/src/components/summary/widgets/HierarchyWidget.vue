<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0

Nested field widget for 1-3 fields, self-fetched from /api/spigraphhierarchy
(hierarchicalResults). Renders a d3 partition **sunburst** when viewMode is 'pie',
a nested d3 **treemap** when viewMode is 'treemap', or a d3-sankey **flow
diagram** when viewMode is 'sankey'. Count-based (the hierarchy endpoint carries
no metric). Colored by the dashboard palette; hover uses the shared chart popover.
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
      class="hierarchy-container">
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
import { colorRange } from './chartColors';
import { useSpigraphWidget } from './useSpigraphWidget';
import { fetchHierarchy, widgetFields } from './widgetData';

const props = defineProps({
  widget: { type: Object, required: true },
  reloadNonce: { type: Number, default: 0 },
  colorScheme: { type: String, default: 'rainbow' },
  infoItems: { type: Array, default: () => [] }
});

const emit = defineEmits(['edit', 'remove', 'show-tooltip']);

const container = ref(null);
const svgEl = ref(null);
const hierarchy = ref(null);
let d3lib;
let d3sankeyLib;
let ro;

const { loading, error, fetchData } = useSpigraphWidget(
  () => props.widget,
  () => props.reloadNonce,
  (res) => { hierarchy.value = res.hierarchicalResults || null; setupObserver(); },
  fetchHierarchy
);

const fieldExps = computed(() => widgetFields(props.widget));
const fieldObjs = computed(() => fieldExps.value.map(exp => FieldService.getField(exp, true)));
const title = computed(() => props.widget.title ||
  fieldObjs.value.map((f, i) => f?.friendlyName || fieldExps.value[i]).join(' / '));
const hasData = computed(() => !!hierarchy.value?.children?.length);

// hover → shared popover; resolve the field config by the node's depth
const onHover = (e, d) => {
  const fieldObj = fieldObjs.value[d.depth - 1] || fieldObjs.value[0];
  emit('show-tooltip', {
    data: {
      item: d.data.name,
      sessions: d.data.sizeValue ?? d.data.size ?? d.value,
      value: d.data.sizeValue ?? d.data.size ?? d.value
    },
    position: { x: e.clientX + 1, y: e.clientY + 1 },
    fieldConfig: fieldObj,
    metricType: 'sessions'
  });
};

// hover for sankey nodes/links (flat nodes with fieldIdx, not a d3.hierarchy)
const onSankeyHover = (e, node) => {
  emit('show-tooltip', {
    data: { item: node.name, sessions: node.value, value: node.value },
    position: { x: e.clientX + 1, y: e.clientY + 1 },
    fieldConfig: fieldObjs.value[node.fieldIdx] || fieldObjs.value[0],
    metricType: 'sessions'
  });
};

/**
 * Flatten the hierarchy endpoint's nested results into d3-sankey {nodes, links}.
 * Node values are cumulative (a parent counts at least the sum of its children,
 * see sizeValue vs size in the endpoint), node ids are name+depth so the same
 * value under two parents becomes one node with two inbound links. With a single
 * field there are no flows between fields, so the root is kept as the source
 * column (root → each value), matching the spigraph page's sankey.
 */
const hierarchyToSankey = (root) => {
  const nodes = [];
  const links = [];
  const nodeMap = new Map();
  const cumulative = (n) => {
    if (!n.children?.length) { return n.size || 0; }
    const sum = n.children.reduce((s, c) => s + cumulative(c), 0);
    return Math.max(n.sizeValue || 0, sum);
  };
  const multiLevel = (root.children || []).some(c => c.children?.length);
  const traverse = (n, depth, parentId) => {
    const id = `${n.name}_${depth}`;
    if (!nodeMap.has(id)) {
      // fieldIdx resolves the hover tooltip's field; the kept root has none
      const fieldIdx = multiLevel ? depth : depth - 1;
      nodeMap.set(id, { id, name: n.name, value: cumulative(n), fieldIdx: Math.max(0, fieldIdx) });
      nodes.push(nodeMap.get(id));
    }
    if (parentId && parentId !== id) {
      links.push({ source: parentId, target: id, value: cumulative(n) });
    }
    for (const c of (n.children || [])) { traverse(c, depth + 1, id); }
  };
  if (multiLevel) {
    for (const c of (root.children || [])) { traverse(c, 0, null); }
  } else {
    traverse(root, 0, null);
  }
  return { nodes, links };
};

// color by the top-level ancestor so each first-level slice keeps one hue
const topName = (d) => { let n = d; while (n.depth > 1) { n = n.parent; } return n.data.name; };

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

  if (props.widget.viewMode === 'sankey') {
    if (!d3sankeyLib) { d3sankeyLib = await import('d3-sankey'); }
    const { nodes, links } = hierarchyToSankey(hierarchy.value);
    if (!nodes.length) { return; }
    const sankeyColors = d3.scaleOrdinal(colorRange(d3, props.colorScheme, nodes.length));
    // labels sit outside the nodes, so keep a side margin for them
    const margin = { top: 2, right: 80, bottom: 2, left: 4 };
    const layout = d3sankeyLib.sankey()
      .nodeId(d => d.id)
      .nodeWidth(10)
      .nodePadding(8)
      .extent([[margin.left, margin.top], [w - margin.right, h - margin.bottom]]);
    const graph = layout({ nodes, links }); // mutates the fresh nodes/links in place
    const g = svg.append('g');
    g.append('g')
      .attr('fill', 'none')
      .selectAll('path')
      .data(graph.links)
      .join('path')
      .attr('d', d3sankeyLib.sankeyLinkHorizontal())
      .attr('stroke', d => sankeyColors(d.source.name))
      .attr('stroke-width', d => Math.max(1, d.width))
      .attr('opacity', 0.5)
      .style('cursor', 'pointer')
      .on('mouseover', (e, d) => onSankeyHover(e, d.source));
    g.append('g')
      .selectAll('rect')
      .data(graph.nodes)
      .join('rect')
      .attr('x', d => d.x0)
      .attr('y', d => d.y0)
      .attr('width', d => Math.max(1, d.x1 - d.x0))
      .attr('height', d => Math.max(1, d.y1 - d.y0))
      .attr('fill', d => sankeyColors(d.name))
      .attr('class', 'hierarchy-sep')
      .style('cursor', 'pointer')
      .on('mouseover', onSankeyHover);
    g.append('g')
      .selectAll('text')
      .data(graph.nodes.filter(d => (d.y1 - d.y0) > 8))
      .join('text')
      .attr('x', d => d.x0 < w / 2 ? d.x1 + 4 : d.x0 - 4)
      .attr('y', d => (d.y1 + d.y0) / 2)
      .attr('dy', '0.35em')
      .attr('text-anchor', d => d.x0 < w / 2 ? 'start' : 'end')
      .attr('class', 'sankey-label')
      .text(d => d.name);
    return;
  }

  const root = d3.hierarchy(hierarchy.value)
    .sum(d => d.size || 0)
    .sort((a, b) => b.value - a.value);

  const colors = d3.scaleOrdinal(colorRange(d3, props.colorScheme, (hierarchy.value.children || []).length));

  if (props.widget.viewMode === 'treemap') {
    d3.treemap().size([w, h]).paddingTop(d => (d.depth === 0 ? 0 : 14)).paddingInner(2).round(true)(root);
    const g = svg.append('g');
    g.selectAll('rect')
      .data(root.descendants().filter(d => d.depth))
      .join('rect')
      .attr('x', d => d.x0)
      .attr('y', d => d.y0)
      .attr('width', d => Math.max(0, d.x1 - d.x0))
      .attr('height', d => Math.max(0, d.y1 - d.y0))
      .attr('fill', d => colors(topName(d)))
      .attr('fill-opacity', d => 0.45 + 0.45 * (d.depth / (root.height || 1)))
      .attr('class', 'hierarchy-sep')
      .style('cursor', 'pointer')
      .on('mouseover', onHover);
    // parent (outer field) labels sit in the reserved top band of each group
    g.selectAll('text.hierarchy-parent-label')
      .data(root.descendants().filter(d => d.depth >= 1 && d.children && (d.x1 - d.x0) > 44))
      .join('text')
      .attr('x', d => d.x0 + 4)
      .attr('y', d => d.y0 + 11)
      .attr('class', 'hierarchy-parent-label')
      .text(d => d.data.name);
    // leaf (innermost) labels (only where they fit)
    g.selectAll('text.hierarchy-label')
      .data(root.leaves().filter(d => (d.x1 - d.x0) > 44 && (d.y1 - d.y0) > 18))
      .join('text')
      .attr('x', d => d.x0 + 4)
      .attr('y', d => d.y0 + 14)
      .attr('class', 'hierarchy-label')
      .text(d => d.data.name);
  } else { // pie → sunburst
    const radius = Math.min(w, h) / 2;
    d3.partition().size([2 * Math.PI, radius])(root);
    const arc = d3.arc()
      .startAngle(d => d.x0)
      .endAngle(d => d.x1)
      .padAngle(0.004)
      .innerRadius(d => d.y0)
      .outerRadius(d => d.y1 - 1)
      .cornerRadius(2);
    const g = svg.append('g').attr('transform', `translate(${w / 2},${h / 2})`);
    g.selectAll('path')
      .data(root.descendants().filter(d => d.depth))
      .join('path')
      .attr('d', arc)
      .attr('fill', d => colors(topName(d)))
      .attr('fill-opacity', d => 0.55 + 0.45 * (d.depth / (root.height || 1)))
      .attr('class', 'hierarchy-sep')
      .style('cursor', 'pointer')
      .on('mouseover', onHover);
  }
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

watch(() => props.colorScheme, render);
watch(() => props.widget.viewMode, render);

onBeforeUnmount(() => { if (ro) { ro.disconnect(); } });
</script>

<style scoped>
.hierarchy-container {
  position: relative;
  width: 100%;
  height: 100%;
  min-height: 0;
  overflow: hidden;
}
.hierarchy-container :deep(.hierarchy-label),
.hierarchy-container :deep(.hierarchy-parent-label) {
  fill: #fff;
  font-size: 10px;
  paint-order: stroke;
  stroke: rgba(0, 0, 0, 0.55);
  stroke-width: 2px;
  stroke-linejoin: round;
  pointer-events: none;
}
.hierarchy-container :deep(.hierarchy-label) { font-weight: 600; }
/* outer-field labels read a touch bolder so the nesting is legible */
.hierarchy-container :deep(.hierarchy-parent-label) { font-weight: 700; }
/* separators via a class so the theme var() resolves (an SVG stroke attr would not) */
.hierarchy-container :deep(.hierarchy-sep) {
  stroke: rgb(var(--v-theme-background));
  stroke-width: 1;
}
/* sankey node labels read in the page foreground, outside the colored nodes */
.hierarchy-container :deep(.sankey-label) {
  fill: rgb(var(--v-theme-foreground));
  font-size: 10px;
  pointer-events: none;
}
</style>
