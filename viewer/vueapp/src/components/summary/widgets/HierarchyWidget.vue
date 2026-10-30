<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0

Nested field widget, self-fetched from /api/spigraphhierarchy
(hierarchicalResults). Renders a d3 partition **sunburst** when viewMode is 'pie'
(one small sunburst per split value when the pie has a split field), a nested d3
**treemap** when viewMode is 'treemap', or a d3-sankey **flow diagram** when
viewMode is 'sankey'. Count-based (the hierarchy endpoint carries no metric).
Colored by the dashboard palette, with a pie's "Other" in gray; hover uses the
shared chart popover.
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
      class="hierarchy-container">
      <svg
        v-show="hasData"
        ref="svgEl" />
    </div>
  </WidgetCard>
</template>

<script setup>
import { ref, computed, watch, onBeforeUnmount, nextTick } from 'vue';
import { useI18n } from 'vue-i18n';
import WidgetCard from './WidgetCard.vue';
import FieldService from '../../search/FieldService';
import { colorRange, OTHER_COLOR } from './chartColors';
import { useSpigraphWidget } from './useSpigraphWidget';
import { fetchHierarchy, hierarchyFields } from './widgetData';
import { hierarchyToSankey } from '../../utils/sankeyData';
import { isUnderOther, smallMultiplesGrid, fitLabel } from '../../utils/hierarchyLayout';

const { t, locale } = useI18n();

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

// one per nesting level, outermost first (a split pie's split field leads)
const fieldExps = computed(() => hierarchyFields(props.widget));
const isSplitPie = computed(() => props.widget.viewMode === 'pie' && !!props.widget.splitField);
const fieldObjs = computed(() => fieldExps.value.map(exp => FieldService.getField(exp, true)));
const title = computed(() => props.widget.title ||
  fieldObjs.value.map((f, i) => f?.friendlyName || fieldExps.value[i]).join(' / '));
const hasData = computed(() => !!hierarchy.value?.children?.length);

// a node's label: "Other" (sessions outside a level's top values) is translated
const nodeName = (d) => (d.data.other ? t('sessions.summary.other') : d.data.name);

// hover → shared popover; resolve the field config by the node's depth (plus
// the levels above it when it was drawn in a split pie's subtree)
const onHover = (e, d, depthOffset = 0) => {
  const fieldObj = fieldObjs.value[d.depth + depthOffset - 1] || fieldObjs.value[0];
  emit('show-tooltip', {
    data: {
      item: nodeName(d),
      other: !!d.data.other,
      sessions: d.data.sizeValue ?? d.data.size ?? d.value,
      value: d.data.sizeValue ?? d.data.size ?? d.value
    },
    position: { x: e.clientX + 1, y: e.clientY + 1 },
    fieldConfig: fieldObj,
    metricType: 'sessions'
  });
};

// hover for sankey nodes/links (flat nodes carrying a depth, not a d3.hierarchy).
// The kept root of a single-field sankey stands for the whole result set rather
// than a value of any field, so it gets no field dropdown.
const onSankeyHover = (e, node, rootKept) => {
  const fieldIdx = rootKept ? node.depth - 1 : node.depth;
  if (fieldIdx < 0) { return; }
  emit('show-tooltip', {
    data: { item: node.name, sessions: node.value, value: node.value },
    position: { x: e.clientX + 1, y: e.clientY + 1 },
    fieldConfig: fieldObjs.value[fieldIdx] || fieldObjs.value[0],
    metricType: 'sessions'
  });
};

// color by the top-level ancestor so each first-level slice keeps one hue
const topName = (d) => { let n = d; while (n.depth > 1) { n = n.parent; } return n.data.name; };

const SUNBURST_LABEL_PX = 6; // ~width of a 10px label character

/**
 * Draw one sunburst of `node`'s subtree into `g` (centered at its origin):
 * a ring per level, colored by the first ring's value, labelled where a label
 * fits along its arc. depthOffset is how many levels sit above `node` (a split
 * pie's own value), so hover can name the right field. d3 is lazy-loaded by
 * render, which hands it in.
 */
const drawSunburst = (d3, g, node, radius, colors, depthOffset = 0) => {
  d3.partition().size([2 * Math.PI, radius])(node);
  const arc = d3.arc()
    .startAngle(d => d.x0)
    .endAngle(d => d.x1)
    .padAngle(0.004)
    .innerRadius(d => d.y0)
    .outerRadius(d => d.y1 - 1)
    .cornerRadius(2);
  const arcs = node.descendants().filter(d => d.depth);
  g.selectAll('path')
    .data(arcs)
    .join('path')
    .attr('d', arc)
    .attr('fill', d => (isUnderOther(d) ? OTHER_COLOR : colors(topName(d)))) // Other is gray, not a palette color
    .attr('fill-opacity', d => 0.55 + 0.45 * (d.depth / (node.height || 1)))
    .attr('class', 'hierarchy-sep')
    .style('cursor', 'pointer')
    .on('mouseover', (e, d) => onHover(e, d, depthOffset));
  // a label sits flat at the arc's middle, cut to what fits along the arc
  const labelled = arcs
    .map(d => {
      const mid = (d.y0 + d.y1) / 2;
      return { d, text: fitLabel(nodeName(d), (d.x1 - d.x0) * mid, SUNBURST_LABEL_PX) };
    })
    .filter(l => l.text && (l.d.y1 - l.d.y0) >= 12);
  g.selectAll('text')
    .data(labelled)
    .join('text')
    .attr('transform', l => `translate(${arc.centroid(l.d)})`)
    .attr('text-anchor', 'middle')
    .attr('dy', '0.35em')
    .attr('class', 'hierarchy-label')
    .text(l => l.text);
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

  if (props.widget.viewMode === 'sankey') {
    if (!d3sankeyLib) { d3sankeyLib = await import('d3-sankey'); }
    const { nodes, links, rootKept } = hierarchyToSankey(hierarchy.value);
    if (!nodes.length) { return; }
    // a kept root is the endpoint's hard-coded, untranslated "Top Talkers"; it
    // stands for the whole result set, so label it in the user's language
    if (rootKept && nodes[0]) { nodes[0].name = t('sessions.summary.sessions'); }
    const sankeyColors = d3.scaleOrdinal(colorRange(d3, props.colorScheme, nodes.length));
    // every label is drawn to the right of its node, so the last column needs a
    // strip outside the diagram to put them in — size it to the widest value
    // (~5.5px per character at 10px) so long ones aren't cut off at the edge,
    // but never let it eat more than a quarter of the widget
    const widest = Math.max(0, ...nodes.map(n => (n.name || '').length));
    const margin = { top: 2, right: Math.min(w * 0.25, Math.max(40, widest * 5.5)), bottom: 2, left: 4 };
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
      .on('mouseover', (e, d) => onSankeyHover(e, d.source, rootKept));
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
      .on('mouseover', (e, d) => onSankeyHover(e, d, rootKept));
    g.append('g')
      .selectAll('text')
      .data(graph.nodes.filter(d => (d.y1 - d.y0) > 8))
      .join('text')
      // always to the right of the node: putting the right half's labels to the
      // left drew them back over the incoming ribbons while leaving the margin
      // reserved for them empty
      .attr('x', d => d.x1 + 4)
      .attr('y', d => (d.y1 + d.y0) / 2)
      .attr('dy', '0.35em')
      .attr('text-anchor', 'start')
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
  } else if (isSplitPie.value) { // split pie → one small sunburst per split value
    // the split level's own "Other" has no pie of its own to draw
    const pies = (root.children || []).filter(c => !c.data.other);
    if (!pies.length) { return; }
    // one color per slice value across every pie, so a value reads the same in
    // each; Other is gray, so it takes no palette color
    const sliceNames = new Set(pies.flatMap(p => (p.children || []).filter(c => !c.data.other).map(c => c.data.name)));
    const sliceColors = d3.scaleOrdinal(colorRange(d3, props.colorScheme, sliceNames.size));
    const TITLE_H = 14;
    const { cols, radius } = smallMultiplesGrid(pies.length, w, h, TITLE_H);
    const cellW = w / cols;
    const cellH = h / Math.ceil(pies.length / cols);
    pies.forEach((pie, i) => {
      const cx = (i % cols) * cellW + cellW / 2;
      const cy = Math.floor(i / cols) * cellH + TITLE_H + (cellH - TITLE_H) / 2;
      const cell = svg.append('g');
      cell.append('text') // the split value: cut to its cell (~6px a character at 11px), whole in the tooltip
        .attr('x', cx)
        .attr('y', cy - (cellH - TITLE_H) / 2 - 3)
        .attr('text-anchor', 'middle')
        .attr('class', 'hierarchy-pie-title')
        .text(fitLabel(nodeName(pie), cellW - 8, 6))
        .append('title').text(nodeName(pie));
      // the pie's subtree re-rooted (and re-summed) so it fills its own circle
      const node = pie.copy().sum(d => d.size || 0).sort((a, b) => b.value - a.value);
      drawSunburst(d3, cell.append('g').attr('transform', `translate(${cx},${cy})`), node, Math.max(radius, 1), sliceColors, 1);
    });
  } else { // pie → sunburst
    drawSunburst(d3, svg.append('g').attr('transform', `translate(${w / 2},${h / 2})`), root, Math.min(w, h) / 2, colors);
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
watch(locale, render); // "Other" labels are drawn translated

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
/* a split pie's value, above its circle */
.hierarchy-container :deep(.hierarchy-pie-title) {
  fill: rgb(var(--v-theme-foreground));
  font-size: 11px;
  font-weight: 600;
}
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
