<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0

Nested field widget for 2-3 fields, self-fetched from /api/spigraphhierarchy
(hierarchicalResults). Renders a d3 partition **sunburst** when viewMode is 'pie'
or a nested d3 **treemap** when viewMode is 'treemap'. Count-based (the hierarchy
endpoint carries no metric). Colored by the dashboard palette; hover uses the
shared chart popover.
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
</style>
