<!--
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0

Connection tree widget (prototype): the src → dst sample from /api/connections
drawn as a rooted tree (built by connectionTree.js, shared with the SPI Graph
tree type). The root is the widget's treeRoot value, or the source with the
most sessions when none is set; its branches are the hosts it talked to,
theirs the hosts they talked to, and so on up to treeDepth hops. Clicking
a node re-roots the tree there for a look around (clicking the root again goes
back to the configured one); the hover popover shows a branch's session count.
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
      class="tree-container">
      <div
        v-if="hasData && !tree"
        class="text-medium-emphasis tree-none">
        {{ $t('sessions.summary.widget.treeRootMissing', { root: rootId }) }}
      </div>
      <svg
        v-show="tree"
        ref="svgEl"
        @mouseleave="emit('hide-tooltip')" />
    </div>
  </WidgetCard>
</template>

<script setup>
import { ref, computed, watch, onBeforeUnmount, nextTick } from 'vue';
import WidgetCard from './WidgetCard.vue';
import FieldService from '../../search/FieldService';
import { useSpigraphWidget } from './useSpigraphWidget';
import { fetchConnections, widgetFields } from './widgetData';
import { DEFAULT_TREE_DEPTH } from './viewModes';
import { findTreeRoot, buildConnectionTree } from './connectionTree';

const props = defineProps({
  widget: { type: Object, required: true },
  reloadNonce: { type: Number, default: 0 },
  infoItems: { type: Array, default: () => [] },
  link: { type: Object, default: null },
  // the widget as saved-graph data (null: it can't be saved as one)
  graphDef: { type: Object, default: null }
});

const emit = defineEmits(['edit', 'remove', 'show-tooltip', 'hide-tooltip']);

const container = ref(null);
const svgEl = ref(null);
const graph = ref(null); // { nodes, links } from /api/connections
const rootOverride = ref(''); // a node the user clicked to look from
let d3lib;
let ro;

const { loading, error, fetchData } = useSpigraphWidget(
  () => props.widget,
  () => props.reloadNonce,
  (res) => { graph.value = res?.nodes ? res : null; rootOverride.value = ''; setupObserver(); },
  fetchConnections
);

// a new configured root is looked from, not the host clicked before it
watch(() => props.widget.treeRoot, () => { rootOverride.value = ''; });

const fieldExps = computed(() => widgetFields(props.widget));
const fieldObjs = computed(() => fieldExps.value.map(exp => FieldService.getField(exp, true)));
const title = computed(() => props.widget.title ||
  fieldObjs.value.map((f, i) => f?.friendlyName || fieldExps.value[i]).join(' → '));
const hasData = computed(() => !!graph.value?.nodes?.length);
const depth = computed(() => props.widget.treeDepth || DEFAULT_TREE_DEPTH);

// the root asked for: a clicked host, else the configured one, else the
// source with the most sessions
const rootId = computed(() => rootOverride.value ||
  (typeof props.widget.treeRoot === 'string' ? props.widget.treeRoot.trim() : '') ||
  findTreeRoot(graph.value?.nodes, '')?.id || '');

// the sample as a tree from the root ({ id, node, link, children }), null when
// the root isn't in the sample
const tree = computed(() => {
  if (!hasData.value || !rootId.value) { return null; }
  const rootNode = findTreeRoot(graph.value.nodes, rootId.value);
  return rootNode ? buildConnectionTree(graph.value, rootNode, depth.value) : null;
});

// hover → the shared popover: a branch shows the pair's own session count
const onHover = (e, d) => {
  // the root shows its own session count, a branch the pair's
  const isRoot = !d.parent;
  const sessions = isRoot ? (d.data.node.sessions || 0) : (d.data.link?.value || 0);
  emit('show-tooltip', {
    data: { item: d.data.id, sessions, value: sessions },
    position: { x: e.clientX + 1, y: e.clientY + 1 },
    fieldConfig: (isRoot ? fieldObjs.value[0] : fieldObjs.value[1]) || fieldObjs.value[0],
    metricType: 'sessions'
  });
};

// click: look from that host; clicking the current root goes back
const onClick = (_e, d) => {
  rootOverride.value = (d.data.id === rootId.value) ? '' : d.data.id;
};

const render = async () => {
  if (!container.value || !svgEl.value) { return; }
  if (!tree.value) { return; }
  if (!d3lib) { d3lib = await import('d3'); }
  const d3 = d3lib;
  const w = container.value.clientWidth;
  const h = container.value.clientHeight;
  if (!w || !h) { return; }

  const svg = d3.select(svgEl.value);
  svg.selectAll('*').remove();

  const root = d3.hierarchy(tree.value);
  const leaves = Math.max(1, root.leaves().length);
  const levels = root.height + 1;
  // one row per leaf, so a bushy tree scrolls rather than overlapping its labels
  const rowH = 16;
  const innerH = Math.max(h - 20, leaves * rowH);
  // room for the root's label on the left and the leaves' on the right
  const labelW = 110;
  const innerW = Math.max(w - 2 * labelW - 12, (levels - 1) * 120);
  svg.attr('width', innerW + 2 * labelW + 12).attr('height', innerH + 20);

  d3.tree().size([innerH, innerW])(root);
  const g = svg.append('g').attr('transform', `translate(${labelW}, 10)`);

  const maxValue = Math.max(1, ...root.descendants().slice(1).map(d => d.data.link?.value || 0));
  const linkWidth = (d) => 1 + 3 * Math.sqrt((d.data.link?.value || 0) / maxValue);

  g.append('g')
    .selectAll('path')
    .data(root.links())
    .join('path')
    .attr('class', 'tree-link')
    .attr('stroke-width', d => linkWidth(d.target))
    .attr('d', d3.linkHorizontal().x(d => d.y).y(d => d.x))
    .on('mouseover', (e, d) => onHover(e, d.target));

  const node = g.append('g')
    .selectAll('g')
    .data(root.descendants())
    .join('g')
    .attr('transform', d => `translate(${d.y},${d.x})`)
    .style('cursor', 'pointer')
    .on('mouseover', onHover)
    .on('click', onClick);

  node.append('circle')
    .attr('r', d => (d.depth === 0 ? 6 : 4))
    .attr('class', d => `tree-node ${d.depth === 0 ? 'tree-node--root' : (d.children ? 'tree-node--mid' : 'tree-node--leaf')}`);

  node.append('text')
    .attr('dy', '0.32em')
    .attr('x', d => (d.children ? -8 : 8))
    .attr('text-anchor', d => (d.children ? 'end' : 'start'))
    .attr('class', 'tree-label')
    .style('font-size', '10px')
    .text(d => d.data.id);
};

watch(tree, () => nextTick(render));

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

onBeforeUnmount(() => { if (ro) { ro.disconnect(); } });
</script>

<style scoped>
.tree-container {
  position: relative;
  width: 100%;
  height: 100%;
  min-height: 0;
  overflow: auto;
}
.tree-none {
  padding: 12px;
  font-size: 0.9em;
}
.tree-container :deep(.tree-node) {
  stroke: rgb(var(--v-theme-background));
  stroke-width: 1;
}
.tree-container :deep(.tree-node--root) { fill: rgb(var(--v-theme-primary)); }
.tree-container :deep(.tree-node--mid) { fill: rgb(var(--v-theme-tertiary)); }
.tree-container :deep(.tree-node--leaf) { fill: rgb(var(--v-theme-quaternary)); }
.tree-container :deep(.tree-link) {
  fill: none;
  stroke: rgb(var(--v-theme-foreground));
  stroke-opacity: 0.3;
}
.tree-container :deep(.tree-label) {
  fill: rgb(var(--v-theme-foreground));
  pointer-events: none;
}
</style>
